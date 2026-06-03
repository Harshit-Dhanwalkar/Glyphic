/**
 * @file renderer.c
 *
 * Newlines: '\n' in the text buffer forces a line break in the layout.
 *   It occupies a line_map slot (pointing to the line it *ends*) but
 *   produces no rasterised glyph : it just advances current_line.
 */

#include "renderer.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* Layout constants */
#define MINIMAP_CONTENT_ROWS 6

static int minimap_start(int th) { return th - MINIMAP_CONTENT_ROWS - 2; }
static int viewport_rows(int th) { return minimap_start(th) - 1; }

/* Gradient ramp */
static const char SHADE_RAMP[] = " .,:;-~=+?!*#$@";
#define SHADE_RAMP_LEN ((int)(sizeof(SHADE_RAMP) - 1))

static char intensity_to_shade(u8 v) {
  if (v == 0)
    return ' ';
  int idx = (int)v * (SHADE_RAMP_LEN - 1) / 255;
  if (idx < 0)
    idx = 0;
  if (idx >= SHADE_RAMP_LEN)
    idx = SHADE_RAMP_LEN - 1;
  return SHADE_RAMP[idx];
}

/*
 * Minimum normalised intensity (0-255) at which we stamp the actual character
 * instead of a shade ramp character.  Lowered from 200 so thin-stroke glyphs
 * (punctuation, symbols) get their own char rendered.
 */
#define MIN_FILL_INTENSITY 80

/* Terminal queries */
int get_terminal_width(void) {
  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  return w.ws_col > 0 ? w.ws_col : 80;
}

int get_terminal_height(void) {
  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  return w.ws_row > 0 ? w.ws_row : 24;
}

/* Layout + render */
VirtualCanvas layout_and_render_string(string8 file, font_info *info,
                                       const char *text, f32 scale,
                                       int term_width) {
  VirtualCanvas vc = {0};
  u32 len = (u32)strlen(text);
  u32 mc = len + 1;

  font_glyph *glyphs = malloc(mc * sizeof *glyphs);
  f32 *adv = malloc(mc * sizeof *adv);

  /* Fetch glyph metrics */
  for (u32 i = 0; i < len; i++) {
    u8 ch = (u8)text[i];
    if (ch == '\n') {
      /* Newline: no glyph needed, zero advance */
      memset(&glyphs[i], 0, sizeof glyphs[i]);
      adv[i] = 0.0f;
      continue;
    }
    u32 gid = font_glyph_index(file, info, ch);
    font_load_glyph(file, info, &glyphs[i], gid);
    u16 a = (gid < info->num_hmetrics)
                ? info->advance_widths[gid]
                : info->advance_widths[info->num_hmetrics - 1];
    adv[i] = (f32)a * scale;
  }
  /* Cursor sentinel : space width */
  u32 sp_gid = font_glyph_index(file, info, ' ');
  u16 sp_adv =
      (sp_gid < info->num_hmetrics) ? info->advance_widths[sp_gid] : 500;
  adv[len] = (f32)sp_adv * scale;
  memset(&glyphs[len], 0, sizeof glyphs[len]);

  /* Vertical metrics */
  f32 asc = (f32)info->ascent * scale;
  f32 desc = (f32)info->descent * scale;
  f32 lh = (asc - desc + (f32)info->lineGap * scale) * 1.1f;
  if (lh < 1.0f)
    lh = 10.0f;

  /* Reserve 1 col for main scrollbar */
  int cw = term_width - 5;
  if (cw < 40)
    cw = 40;

  vc.line_map = malloc(mc * sizeof *vc.line_map);
  f32 *gx = malloc(mc * sizeof *gx);
  int cur_line = 0;
  f32 cursor_x = 0.0f;

  /* Line-wrap + newline layout */
  for (u32 i = 0; i <= len; i++) {
    if (i < len && text[i] == '\n') {
      /* newline */
      vc.line_map[i] = cur_line;
      gx[i] = cursor_x;
      cur_line++;
      cursor_x = 0.0f;
      continue;
    }
    /* Soft wrap */
    if (cursor_x + adv[i] > (f32)cw && cursor_x > 0.0f) {
      cur_line++;
      cursor_x = 0.0f;
    }
    vc.line_map[i] = cur_line;
    gx[i] = cursor_x;
    if (i < len)
      cursor_x += adv[i];
  }

  int ch = (int)((cur_line + 1) * lh);
  if (ch < (int)lh)
    ch = (int)lh;

  vc.width = cw;
  vc.height = ch;
  vc.grid = malloc((size_t)cw * ch);
  memset(vc.grid, ' ', (size_t)cw * ch);

  /* Rasterize and blit */
  for (u32 i = 0; i < len; i++) {
    u8 ch_byte = (u8)text[i];
    if (ch_byte == '\n')
      continue; /* no glyph for newline */
    if (glyphs[i].num_points == 0) {
      font_free_glyph(&glyphs[i]);
      continue;
    }

    bitmap_r8 gb = font_raster_glyph(&glyphs[i], scale, 0);
    if (!gb.data || gb.width == 0 || gb.height == 0) {
      font_free_glyph(&glyphs[i]);
      continue;
    }

    /* Per-glyph normalisation */
    u8 peak = 0;
    for (u32 p = 0; p < gb.width * gb.height; p++)
      if (gb.data[p] > peak)
        peak = gb.data[p];
    if (peak == 0) {
      free(gb.data);
      font_free_glyph(&glyphs[i]);
      continue;
    }

    int line_idx = vc.line_map[i];
    f32 base_y = (line_idx * lh) + asc;
    int sx = (int)gx[i];
    int sy = (int)(base_y - (f32)glyphs[i].y_max * scale);

    for (u32 y = 0; y < gb.height; y++) {
      int ty = sy + (int)y;
      if (ty < 0 || ty >= vc.height)
        continue;
      for (u32 x = 0; x < gb.width; x++) {
        int tx = sx + (int)x;
        if (tx < 0 || tx >= vc.width)
          continue;

        u8 raw = gb.data[y * gb.width + x];
        u8 norm = (u8)((u32)raw * 255u / peak);
        int gi = ty * vc.width + tx;

        if (norm >= MIN_FILL_INTENSITY) {
          vc.grid[gi] = (char)ch_byte;
        } else if (norm > 0 && vc.grid[gi] == ' ') {
          /* Low coverage: shade ramp fill (only into empty cells) */
          vc.grid[gi] = intensity_to_shade(norm);
        }
      }
    }
    free(gb.data);
    font_free_glyph(&glyphs[i]);
  }

  free(glyphs);
  free(adv);
  free(gx);
  return vc;
}

/* Scroll helpers */
void scroll_to_cursor(VirtualCanvas *vc, const char *text, int cursor_idx,
                      int terminal_height, ScrollState *scroll) {
  if (!vc || !vc->line_map)
    return;

  int vp = viewport_rows(terminal_height);
  if (vp < 1)
    vp = 1;

  int text_len = (int)strlen(text);
  if (text_len == 0)
    return;

  /* Count total logical lines */
  int total_log = vc->line_map[text_len - 1] + 1;
  if (total_log < 1)
    total_log = 1;

  int px_per_line = vc->height / total_log;
  if (px_per_line < 1)
    px_per_line = 1;

  int safe = (cursor_idx < text_len) ? cursor_idx : text_len - 1;
  int cur_log = vc->line_map[safe];
  int cur_px_top = cur_log * px_per_line;
  int cur_px_bot = cur_px_top + px_per_line - 1;

  if (cur_px_top < scroll->offset_rows)
    scroll->offset_rows = cur_px_top;
  else if (cur_px_bot >= scroll->offset_rows + vp)
    scroll->offset_rows = cur_px_bot - vp + 1;

  int max_off = vc->height - vp;
  if (max_off < 0)
    max_off = 0;
  if (scroll->offset_rows > max_off)
    scroll->offset_rows = max_off;
  if (scroll->offset_rows < 0)
    scroll->offset_rows = 0;
}

/* Scrollbar helper */

/**
 * Draw a vertical scrollbar in terminal column @col, covering terminal rows
 * [@top_row .. @bot_row] (1-indexed).
 * @total   total scrollable units
 * @visible units currently visible
 * @offset  current scroll offset (units from top)
 */
static void draw_scrollbar(int col, int top_row, int bot_row, int total,
                           int visible, int offset) {
  int track = bot_row - top_row + 1;
  if (track < 1)
    return;

  int denom = total > 0 ? total : 1;
  int thumb = (track * visible) / denom;
  if (thumb < 1)
    thumb = 1;
  if (thumb > track)
    thumb = track;

  int thumb0 = (int)((long)(track - thumb) * offset / denom);
  int thumb_t = top_row + thumb0;
  int thumb_b = thumb_t + thumb - 1;

  for (int r = top_row; r <= bot_row; r++) {
    printf("\033[%d;%dH", r, col);
    if (r >= thumb_t && r <= thumb_b)
      printf("\033[7m \033[0m");
    else
      printf("\033[90m│\033[0m");
  }
  printf("\033[%d;%dH▲", top_row, col);
  printf("\033[%d;%dH▼", bot_row, col);
}

/* Display */
void display_workspace(VirtualCanvas *vc, const char *text, int cursor_idx,
                       int terminal_height, int terminal_width,
                       ScrollState *scroll) {
  int ms = minimap_start(terminal_height); /* row of minimap top border */
  int vp = viewport_rows(terminal_height); /* canvas viewport row count */

  /* Clamp canvas scroll */
  {
    int max_off = vc->height - vp;
    if (max_off < 0)
      max_off = 0;
    if (scroll->offset_rows > max_off)
      scroll->offset_rows = max_off;
    if (scroll->offset_rows < 0)
      scroll->offset_rows = 0;
  }

  int view_start = scroll->offset_rows;
  int view_end = view_start + vp;
  if (view_end > vc->height)
    view_end = vc->height;

  /* Count total logical lines (for minimap scrollbar) */
  int text_len = (int)strlen(text);
  int total_lines = 0;
  {
    int prev = -1;
    for (int i = 0; i <= text_len; i++) {
      int li = (i < text_len) ? vc->line_map[i]
                              : vc->line_map[text_len > 0 ? text_len - 1 : 0];
      if (li != prev) {
        total_lines++;
        prev = li;
      }
    }
  }
  if (total_lines < 1)
    total_lines = 1;

  /* Clamp minimap scroll */
  {
    int max_mm = total_lines - MINIMAP_CONTENT_ROWS;
    if (max_mm < 0)
      max_mm = 0;
    if (scroll->minimap_line_off > max_mm)
      scroll->minimap_line_off = max_mm;
    if (scroll->minimap_line_off < 0)
      scroll->minimap_line_off = 0;
  }

  /* Clear terminal */
  printf("\033[H");
  for (int y = 0; y < terminal_height; y++)
    printf("\033[%d;1H\033[2K", y + 1);
  printf("\033[H");

  /* Canvas (rows 1..vp, columns 1..term_width-1) */
  int cw = terminal_width - 1;
  for (int y = view_start; y < view_end; y++) {
    printf("\033[%d;1H", y - view_start + 1);
    for (int x = 0; x < vc->width && x < cw; x++)
      putchar(vc->grid[y * vc->width + x]);
  }

  /* Canvas scrollbar (rightmost col, rows 1..vp) */
  draw_scrollbar(terminal_width, 1, vp, vc->height, vp, view_start);

  /* Minimap top border */
  printf("\033[%d;1H+", ms);
  {
    /* Reserve 1 col on right for minimap scrollbar */
    const char *title = " Minimap ";
    int tlen = 9;
    int inner = terminal_width - 3; /* -1 left '+', -1 right '+', -1 sb */
    int left_d = (inner - tlen) / 2;
    int right_d = inner - tlen - left_d;
    for (int i = 0; i < left_d; i++)
      putchar('-');
    printf("%s", title);
    for (int i = 0; i < right_d; i++)
      putchar('-');
    putchar('+');
  }

  /* Minimap content */
  /*
   * Build a list of (logical_line_index, start_char_index) pairs so we can
   * window into them via minimap_line_off.
   */
  /* Max possible logical lines = text_len + 1 */
  int *line_starts = malloc((text_len + 2) * sizeof *line_starts);
  int n_lines = 0;
  {
    int prev_line = -1;
    for (int i = 0; i <= text_len; i++) {
      int li = (i < text_len) ? vc->line_map[i]
                              : (text_len > 0 ? vc->line_map[text_len - 1] : 0);
      if (li != prev_line) {
        line_starts[n_lines++] = i;
        prev_line = li;
      }
    }
  }

  int mm_usable = terminal_width - 4; /* "| " prefix + " |" suffix = 4 chars */
  int mm_off = scroll->minimap_line_off;
  int rows_drawn = 0;

  for (int li = mm_off; li < n_lines && rows_drawn < MINIMAP_CONTENT_ROWS;
       li++) {
    rows_drawn++;
    printf("\033[%d;1H| ", ms + rows_drawn);
    int col_off = 0;

    /* Determine char range for this logical line */
    int start_ci = line_starts[li];
    int end_ci = (li + 1 < n_lines) ? line_starts[li + 1] : text_len;

    for (int ci = start_ci; ci < end_ci && col_off < mm_usable; ci++) {
      char ch = text[ci];
      if (ch == '\n') {
        /* Show return symbol for hard newlines */
        printf("\xe2\x86\xb5"); /* ↵ UTF-8 */
        col_off++;
        continue;
      }
      if (ci == cursor_idx)
        printf("\033[7m%c\033[0m", ch == ' ' ? '_' : ch);
      else
        putchar(ch);
      col_off++;
    }

    /* If cursor is at end of this line */
    if (cursor_idx == text_len && li == n_lines - 1 && col_off < mm_usable) {
      printf("\033[7m \033[0m");
      col_off++;
    }

    /* Pad with dots */
    printf("\033[90m");
    while (col_off < mm_usable) {
      printf("·");
      col_off++;
    }
    printf("\033[0m |");
  }

  /* Fill any unused minimap rows */
  while (rows_drawn < MINIMAP_CONTENT_ROWS) {
    rows_drawn++;
    printf("\033[%d;1H| ", ms + rows_drawn);
    printf("\033[90m");
    for (int i = 0; i < mm_usable; i++)
      printf("·");
    printf("\033[0m |");
  }

  /* Minimap scrollbar (col = terminal_width, rows ms+1 * ms+MINIMAP_CONTENT_ROWS) */
  draw_scrollbar(terminal_width, ms + 1, ms + MINIMAP_CONTENT_ROWS, total_lines,
                 MINIMAP_CONTENT_ROWS, mm_off);

  /* Minimap bottom border */
  printf("\033[%d;1H+", ms + MINIMAP_CONTENT_ROWS + 1);
  for (int i = 0; i < terminal_width - 2; i++)
    putchar('-');
  putchar('+');

  free(line_starts);
}

#include "renderer.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

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

VirtualCanvas layout_and_render_string(string8 file, font_info *info,
                                       const char *text, f32 scale,
                                       int term_width) {
  VirtualCanvas vc = {0};
  u32 len = strlen(text);
  u32 metric_count = len + 1;
  font_glyph *glyphs = malloc(metric_count * sizeof(font_glyph));
  f32 *glyph_widths = malloc(metric_count * sizeof(f32));

  for (u32 i = 0; i < len; i++) {
    u32 g_id = font_glyph_index(file, info, (u8)text[i]);
    font_load_glyph(file, info, &glyphs[i], g_id);
    u16 adv = (g_id < info->num_hmetrics)
                  ? info->advance_widths[g_id]
                  : info->advance_widths[info->num_hmetrics - 1];
    glyph_widths[i] = (f32)adv * scale;
  }
  u32 space_g_id = font_glyph_index(file, info, ' ');
  u16 default_adv = (space_g_id < info->num_hmetrics)
                        ? info->advance_widths[space_g_id]
                        : 500;
  glyph_widths[len] = (f32)default_adv * scale;
  memset(&glyphs[len], 0, sizeof(font_glyph));

  f32 scaled_ascent = (f32)info->ascent * scale;
  f32 scaled_descent = (f32)info->descent * scale;
  f32 line_height =
      (scaled_ascent - scaled_descent + (f32)info->lineGap * scale) * 1.1f;
  if (line_height < 1.0f)
    line_height = 10.0f;

  int max_needed_width = term_width - 4;
  if (max_needed_width < 40)
    max_needed_width = 40;

  int current_line = 0;
  f32 cursor_x = 0;
  vc.line_map = malloc(metric_count * sizeof(int));
  f32 *glyph_x_positions = malloc(metric_count * sizeof(f32));

  for (u32 i = 0; i <= len; i++) {
    if (cursor_x + glyph_widths[i] > max_needed_width && cursor_x > 0) {
      current_line++;
      cursor_x = 0;
    }
    vc.line_map[i] = current_line;
    glyph_x_positions[i] = cursor_x;
    if (i < len)
      cursor_x += glyph_widths[i];
  }

  int total_lines_count = current_line + 1;
  int canvas_w = max_needed_width;
  int canvas_h = (int)(total_lines_count * line_height);
  if (canvas_h < (int)line_height)
    canvas_h = (int)line_height;

  vc.width = canvas_w;
  vc.height = canvas_h;
  vc.grid = malloc(canvas_w * canvas_h);
  memset(vc.grid, ' ', canvas_w * canvas_h);

  for (u32 i = 0; i < len; i++) {
    if (glyphs[i].num_points == 0) {
      font_free_glyph(&glyphs[i]);
      continue;
    }
    bitmap_r8 gb = font_raster_glyph(&glyphs[i], scale, 0);
    if (gb.width == 0 || gb.height == 0 || !gb.data) {
      font_free_glyph(&glyphs[i]);
      continue;
    }
    u8 local_max = 0;
    for (u32 idx = 0; idx < gb.width * gb.height; idx++) {
      if (gb.data[idx] > local_max)
        local_max = gb.data[idx];
    }
    int base_line_idx = vc.line_map[i];
    f32 baseline_y = (base_line_idx * line_height) + scaled_ascent;
    int start_x = (int)glyph_x_positions[i];
    int start_y = (int)(baseline_y - ((f32)glyphs[i].y_max * scale));

    for (u32 y = 0; y < gb.height; y++) {
      int target_y = start_y + y;
      if (target_y < 0 || target_y >= canvas_h)
        continue;
      for (u32 x = 0; x < gb.width; x++) {
        int target_x = start_x + x;
        if (target_x < 0 || target_x >= canvas_w)
          continue;
        u8 intensity = gb.data[y * gb.width + x];
        if (local_max > 0)
          intensity = (intensity * 255) / local_max;
        if (intensity > 120)
          vc.grid[target_y * canvas_w + target_x] = text[i];
      }
    }
    free(gb.data);
    font_free_glyph(&glyphs[i]);
  }
  free(glyphs);
  free(glyph_widths);
  free(glyph_x_positions);
  return vc;
}

void display_workspace(VirtualCanvas *vc, const char *text, int cursor_idx,
                       int terminal_height, int terminal_width) {
  printf("\033[H");
  int minimap_reserve_rows = 6;
  int max_allowed_lines = terminal_height - minimap_reserve_rows - 4;
  int display_h = vc->height;
  if (display_h > max_allowed_lines)
    display_h = max_allowed_lines;

  for (int y = 0; y < terminal_height; y++) {
    printf("\033[%d;1H\033[2K", y + 1);
  }
  printf("\033[H");

  for (int y = 0; y < display_h; y++) {
    printf("\033[%d;1H", y + 1);
    for (int x = 0; x < vc->width && x < terminal_width; x++) {
      putchar(vc->grid[y * vc->width + x]);
    }
  }

  int minimap_start_row = terminal_height - minimap_reserve_rows - 3;
  printf("\033[%d;1H+", minimap_start_row);
  int title_len = 11;
  int left_dashes = (terminal_width - 2 - title_len) / 2;
  int right_dashes = terminal_width - 2 - title_len - left_dashes;
  for (int i = 0; i < left_dashes; i++)
    putchar('-');
  printf(" Minimap ");
  for (int i = 0; i < right_dashes; i++)
    putchar('-');
  putchar('+');

  int text_len = strlen(text);
  int map_row_count = 0;
  int current_map_line = -1;
  int current_col_offset = 0;
  int usable_width = terminal_width - 4;

  for (int i = 0; i <= text_len; i++) {
    int target_line = (i <= text_len) ? vc->line_map[i] : current_map_line;
    if (target_line != current_map_line) {
      if (current_map_line != -1) {
        printf("\033[90m");
        while (current_col_offset < usable_width) {
          printf("·");
          current_col_offset++;
        }
        printf("\033[0m |");
      }
      current_map_line = target_line;
      map_row_count++;
      if (map_row_count > minimap_reserve_rows)
        break;
      printf("\033[%d;1H| ", minimap_start_row + map_row_count);
      current_col_offset = 0;
    }
    if (i == text_len)
      break;
    if (current_col_offset >= usable_width)
      continue;

    if (i == cursor_idx) {
      printf("\033[7m%c\033[0m", text[i] == ' ' ? '_' : text[i]);
    } else {
      putchar(text[i]);
    }
    current_col_offset++;
  }

  if (map_row_count > 0 && map_row_count <= minimap_reserve_rows) {
    if (cursor_idx == text_len && current_col_offset < usable_width) {
      printf("\033[7m \033[0m");
      current_col_offset++;
    }
    printf("\033[90m");
    while (current_col_offset < usable_width) {
      printf("·");
      current_col_offset++;
    }
    printf("\033[0m |");
  }

  printf("\033[%d;1H+", minimap_start_row + map_row_count + 1);
  for (int i = 0; i < terminal_width - 2; i++)
    putchar('-');
  putchar('+');
}

#include "font.h"
#include "renderer.h"
#include "snapshot.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/* Raw mode */
static void set_raw_mode(struct termios *orig) {
  struct termios raw;
  tcgetattr(STDIN_FILENO, orig);
  raw = *orig;
  raw.c_lflag &= ~(ICANON | ECHO);
  raw.c_iflag &= ~(IXON);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void enable_mouse(void) {
  printf("\033[?1000h\033[?1006h");
  fflush(stdout);
}

static void disable_mouse(void) {
  printf("\033[?1000l\033[?1006l");
  fflush(stdout);
}

static int read_byte(void) {
  unsigned char c;
  return (read(STDIN_FILENO, &c, 1) == 1) ? (int)c : -1;
}

static void drain_seq(void) {
  for (int i = 0; i < 32; i++) {
    int c = read_byte();
    if (c < 0 || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '~')
      break;
  }
}

#define MINIMAP_CONTENT_ROWS 6
static int vp_rows(int th) { return th - MINIMAP_CONTENT_ROWS - 3; }

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <font.ttf> [em_size]\n", argv[0]);
    return 1;
  }

  string8 file = read_file(argv[1]);
  if (!file.str) {
    fprintf(stderr, "Error: cannot open '%s'\n", argv[1]);
    return 1;
  }

  font_info font = {0};
  font_init(file, &font);

  /* Sentence buffer : stores printable ASCII + '\n' */
  char sentence[4096] = "";
  int slen = 0;
  int cursor = 0;
  f32 em = (argc >= 3) ? (f32)atof(argv[2]) : 48.0f;

  ScrollState scroll = {0};
  int cursor_moved = 0;

  struct termios orig;
  set_raw_mode(&orig);
  enable_mouse();
  printf("\033[2J\033[H");
  fflush(stdout);

  int running = 1;
  while (running) {
    int tw = get_terminal_width();
    int th = get_terminal_height();
    f32 sc = font_scale_for_em(file, &font, em);

    VirtualCanvas vc = layout_and_render_string(file, &font, sentence, sc, tw);

    if (vc.grid) {
      if (cursor_moved) {
        scroll_to_cursor(&vc, sentence, cursor, th, &scroll);
        cursor_moved = 0;
      } else {
        /* resize clamp only */
        int vp = vp_rows(th);
        if (vp < 1)
          vp = 1;
        int mo = vc.height - vp;
        if (mo < 0)
          mo = 0;
        if (scroll.offset_rows > mo)
          scroll.offset_rows = mo;
        if (scroll.offset_rows < 0)
          scroll.offset_rows = 0;
      }
      display_workspace(&vc, sentence, cursor, th, tw, &scroll);
    }

    /* Status bar */
    printf("\033[%d;1H\033[2K"
           "Size:%.1fpx | +/-:Zoom | \xe2\x86\x90\xe2\x86\x92:Nav"
           " | \xe2\x86\x91\xe2\x86\x93:Lines | PgUp/Dn:Scroll | "
           "Enter:\xe2\x86\xb5"
           " | Ctrl+S:Save | q:Quit",
           th, em);
    fflush(stdout);

    int c = read_byte();
    if (c < 0)
      continue;

    /* Escape sequences */
    if (c == '\033') {
      int s1 = read_byte();
      if (s1 == '[') {
        int s2 = read_byte();

        if (s2 == 'D') {
          if (cursor > 0)
            cursor--;
          cursor_moved = 1;
        } else if (s2 == 'C') {
          if (cursor < slen)
            cursor++;
          cursor_moved = 1;
        } else if (s2 == 'A') { /* ↑ : move cursor to previous logical line */
          if (vc.grid && vc.line_map && cursor > 0) {
            int cur_line = vc.line_map[cursor < slen ? cursor : slen - 1];
            int col_off = 0;
            for (int i = cursor - 1; i >= 0; i--) {
              if (vc.line_map[i] < cur_line)
                break;
              col_off++;
            }
            if (cur_line > 0) {
              int prev_line = cur_line - 1;
              int line_start = 0;
              for (int i = cursor - 1; i >= 0; i--) {
                if (vc.line_map[i] <= prev_line) {
                  line_start = i;
                  break;
                }
              }
              while (line_start > 0 && vc.line_map[line_start - 1] == prev_line)
                line_start--;
              int prev_len = 0;
              for (int i = line_start; i < slen && vc.line_map[i] == prev_line;
                   i++)
                prev_len++;
              int target_col = col_off < prev_len ? col_off : prev_len;
              cursor = line_start + target_col;
            }
            cursor_moved = 1;
          }
        } else if (s2 == 'B') { /* ↓ : move cursor to next logical line */
          if (vc.grid && vc.line_map && cursor < slen) {
            int cur_line = vc.line_map[cursor];
            /* Column offset within current line */
            int line_start = cursor;
            while (line_start > 0 && vc.line_map[line_start - 1] == cur_line)
              line_start--;
            int col_off = cursor - line_start;
            /* Find start of next line */
            int next_line = cur_line + 1;
            int next_start = -1;
            for (int i = cursor; i <= slen; i++) {
              int li = (i < slen) ? vc.line_map[i]
                                  : (vc.line_map[slen > 0 ? slen - 1 : 0] + 1);
              if (li >= next_line) {
                next_start = i;
                break;
              }
            }
            if (next_start >= 0 && next_start <= slen) {
              int next_len = 0;
              for (int i = next_start; i < slen && vc.line_map[i] == next_line;
                   i++)
                next_len++;
              int target_col = col_off < next_len ? col_off : next_len;
              cursor = next_start + target_col;
              if (cursor > slen)
                cursor = slen;
            }
            cursor_moved = 1;
          }
        } else if (s2 == '5') { /* PgUp : scroll canvas up */
          read_byte();          /* consume '~' */
          int vp = vp_rows(th);
          if (vp < 1)
            vp = 1;
          scroll.offset_rows -= vp;
          if (scroll.offset_rows < 0)
            scroll.offset_rows = 0;
        } else if (s2 == '6') { /* PgDn : scroll canvas down */
          read_byte();          /* consume '~' */
          int vp = vp_rows(th);
          if (vp < 1)
            vp = 1;
          scroll.offset_rows += vp;
        } else if (s2 == 'M') { /* X10 mouse */
          int btn = read_byte();
          int mx = read_byte();
          int my = read_byte();
          int ms = th - MINIMAP_CONTENT_ROWS - 2; /* minimap_start row */
          int in_minimap = (my >= ms + 1 && my <= ms + MINIMAP_CONTENT_ROWS);
          if (btn == 64) { /* scroll up */
            if (in_minimap) {
              scroll.minimap_line_off -= 2;
              if (scroll.minimap_line_off < 0)
                scroll.minimap_line_off = 0;
            } else {
              scroll.offset_rows -= 3;
              if (scroll.offset_rows < 0)
                scroll.offset_rows = 0;
            }
          } else if (btn == 65) { /* scroll down */
            if (in_minimap)
              scroll.minimap_line_off += 2;
            else
              scroll.offset_rows += 3;
          }
          (void)mx;
        } else if (s2 == '<') { /* SGR mouse */
          char sgr[64];
          int slen2 = 0;
          sgr[slen2++] = '<';
          for (int i = 0; i < 60; i++) {
            int sc2 = read_byte();
            if (sc2 < 0)
              break;
            sgr[slen2++] = (char)sc2;
            if (sc2 == 'M' || sc2 == 'm')
              break;
          }
          sgr[slen2] = '\0';
          int pb = 0, px = 0, py = 0;
          sscanf(sgr + 1, "%d;%d;%d", &pb, &px, &py);
          int ms2 = th - MINIMAP_CONTENT_ROWS - 2;
          int in_mm = (py >= ms2 + 1 && py <= ms2 + MINIMAP_CONTENT_ROWS);
          if (pb == 64) {
            if (in_mm) {
              scroll.minimap_line_off -= 2;
              if (scroll.minimap_line_off < 0)
                scroll.minimap_line_off = 0;
            } else {
              scroll.offset_rows -= 3;
              if (scroll.offset_rows < 0)
                scroll.offset_rows = 0;
            }
          } else if (pb == 65) {
            if (in_mm)
              scroll.minimap_line_off += 2;
            else
              scroll.offset_rows += 3;
          }
        } else
          drain_seq();
      }
      if (vc.grid) {
        free(vc.grid);
        free(vc.line_map);
      }
      continue;
    }

    /* Regular keys */
    switch (c) {
    case '\r': /* Enter insert newline */
    case '\n':
      if (slen < (int)sizeof(sentence) - 1) {
        for (int i = slen; i >= cursor; i--)
          sentence[i + 1] = sentence[i];
        sentence[cursor] = '\n';
        slen++;
        cursor++;
        cursor_moved = 1;
      }
      break;

    case 127:
    case '\b':
      if (cursor > 0 && slen > 0) {
        for (int i = cursor - 1; i < slen; i++)
          sentence[i] = sentence[i + 1];
        slen--;
        cursor--;
        cursor_moved = 1;
      }
      break;

    case '+':
      em = fminf(300.0f, em * 1.1f);
      scroll.offset_rows = 0;
      cursor_moved = 1;
      printf("\033[2J\033[H");
      break;

    case '-':
      em = fmaxf(12.0f, em / 1.1f);
      scroll.offset_rows = 0;
      cursor_moved = 1;
      printf("\033[2J\033[H");
      break;

    case 19: /* Ctrl+S */
      if (vc.grid)
        save_canvas_as_png(&vc, sentence);
      break;

    case 'q':
      running = 0;
      break;

    default:
      if (c >= 32 && c <= 126 && slen < (int)sizeof(sentence) - 1) {
        for (int i = slen; i >= cursor; i--)
          sentence[i + 1] = sentence[i];
        sentence[cursor] = (char)c;
        slen++;
        cursor++;
        cursor_moved = 1;
      }
      break;
    }

    if (vc.grid) {
      free(vc.grid);
      free(vc.line_map);
    }
  }

  disable_mouse();
  printf("\033[2J\033[H");
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
  free(font.advance_widths);
  free(font.left_side_bearings);
  free(file.str);
  return 0;
}

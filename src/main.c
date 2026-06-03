#include "font.h"
#include "renderer.h"
#include "snapshot.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

void set_raw_mode(struct termios *orig) {
  struct termios raw;
  tcgetattr(STDIN_FILENO, orig);
  raw = *orig;
  raw.c_lflag &= ~(ICANON | ECHO);
  raw.c_iflag &= ~(IXON);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <font_file.ttf> [initial_em_size]\n", argv[0]);
    return 1;
  }
  const char *font_path = argv[1];
  float initial_em = (argc >= 3) ? atof(argv[2]) : 48.0f;
  string8 file = read_file(font_path);
  if (!file.str) {
    fprintf(stderr, "Error: Cannot open font file '%s'\n", font_path);
    return 1;
  }
  font_info font = {0};
  font_init(file, &font);

  char sentence[2048] = "";
  int sentence_len = 0;
  int cursor_index = 0;
  f32 em_size = initial_em;

  struct termios orig;
  set_raw_mode(&orig);

  printf("\033[2J\033[H");
  fflush(stdout);

  int running = 1;
  while (running) {
    int term_width = get_terminal_width();
    int term_height = get_terminal_height();
    f32 scale = font_scale_for_em(file, &font, em_size);

    VirtualCanvas vc =
        layout_and_render_string(file, &font, sentence, scale, term_width);
    if (vc.grid) {
      display_workspace(&vc, sentence, cursor_index, term_height, term_width);
    }

    int menu_row = term_height - 1;
    printf("\033[%d;1H\033[2KSize: %.1f px/em | +/-: Zoom | Arrow Keys: "
           "Navigation | Ctrl+S: Save | 'q': Quit",
           menu_row, em_size);
    fflush(stdout);

    char c = getchar();
    if (c == '\033') {
      char seq1 = getchar();
      if (seq1 == '[') {
        char seq2 = getchar();
        if (seq2 == 'D') {
          if (cursor_index > 0)
            cursor_index--;
        } else if (seq2 == 'C') {
          if (cursor_index < sentence_len)
            cursor_index++;
        }
      }
      if (vc.grid) {
        free(vc.grid);
        free(vc.line_map);
      }
      continue;
    }

    switch (c) {
    case '+':
      em_size = fminf(300.0f, em_size * 1.1f);
      printf("\033[2J\033[H");
      break;
    case '-':
      em_size = fmaxf(12.0f, em_size / 1.1f);
      printf("\033[2J\033[H");
      break;
    case 127:
    case '\b':
      if (cursor_index > 0 && sentence_len > 0) {
        for (int i = cursor_index - 1; i < sentence_len; i++) {
          sentence[i] = sentence[i + 1];
        }
        sentence_len--;
        cursor_index--;
      }
      break;
    case 19: // Ctrl+S
      if (vc.grid) {
        save_canvas_as_png(&vc, sentence);
      }
      break;
    case 'q':
      running = 0;
      break;
    default:
      if (c >= 32 && c <= 126 && sentence_len < (int)sizeof(sentence) - 1) {
        for (int i = sentence_len; i >= cursor_index; i--) {
          sentence[i + 1] = sentence[i];
        }
        sentence[cursor_index] = c;
        sentence_len++;
        cursor_index++;
      }
      break;
    }

    if (vc.grid) {
      free(vc.grid);
      free(vc.line_map);
    }
  }

  printf("\033[2J\033[H");
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
  free(font.advance_widths);
  free(font.left_side_bearings);
  free(file.str);
  return 0;
}

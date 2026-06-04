#include "listfonts.h"
#include <fontconfig/fontconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

// query system fonts via fontconfig
int get_system_ttf_fonts(SystemFont *font_list, int max_count) {
  FcConfig *config = FcInitLoadConfigAndFonts();
  FcPattern *pat = FcPatternCreate();
  FcObjectSet *os = FcObjectSetBuild(FC_FAMILY, FC_FILE, (char *)0);
  FcFontSet *fs = FcFontList(config, pat, os);

  int count = 0;
  if (fs) {
    for (int i = 0; i < fs->nfont && count < max_count; i++) {
      FcPattern *font = fs->fonts[i];
      FcChar8 *file = NULL;
      FcChar8 *family = NULL;

      if (FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch &&
          FcPatternGetString(font, FC_FAMILY, 0, &family) == FcResultMatch) {

        // Filter out non-TrueType files
        char *ext = strrchr((char *)file, '.');
        if (ext && (strcasecmp(ext, ".ttf") == 0)) {
          strncpy(font_list[count].path, (char *)file,
                  sizeof(font_list[count].path) - 1);
          strncpy(font_list[count].family, (char *)family,
                  sizeof(font_list[count].family) - 1);
          count++;
        }
      }
    }
    FcFontSetDestroy(fs);
  }
  FcObjectSetDestroy(os);
  FcPatternDestroy(pat);
  FcConfigDestroy(config);
  return count;
}

// interactive menu terminal engine
int select_font_menu(SystemFont *fonts, int font_count) {
  struct termios orig_termios;
  tcgetattr(STDIN_FILENO, &orig_termios);

  struct termios raw = orig_termios;
  raw.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

  int selected = 0;
  int scroll_offset = 0;
  int max_visible_rows = 15;
  int running = 1;

  // Clear Screen completely
  printf("\033[2J\033[H");

  while (running) {
    printf("\033[H"); // Reset cursor position
    printf("--- Select a system font (use up/down arrow keys, enter to select) "
           "---\n\n");

    for (int i = 0; i < max_visible_rows; i++) {
      int current_idx = scroll_offset + i;
      if (current_idx >= font_count)
        break;

      if (current_idx == selected) {
        // Highlight choice using ANSI Inverse Video Escapes
        printf("\033[7m  -> %-30s [%s] \033[0m\n", fonts[current_idx].family,
               fonts[current_idx].path);
      } else {
        printf("     %-30s [%s]\n", fonts[current_idx].family,
               fonts[current_idx].path);
      }
    }

    // Catch Terminal Inputs
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
      if (c == '\n' || c == '\r') {
        running = 0;
      } else if (c == '\033') { // Escape Sequence for Arrow Keys
        unsigned char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) == 1 &&
            read(STDIN_FILENO, &seq[1], 1) == 1) {
          if (seq[0] == '[') {
            if (seq[1] == 'A' && selected > 0) { // Up Arrow
              selected--;
              if (selected < scroll_offset)
                scroll_offset--;
            } else if (seq[1] == 'B' &&
                       selected < font_count - 1) { // Down Arrow
              selected++;
              if (selected >= scroll_offset + max_visible_rows)
                scroll_offset++;
            }
          }
        }
      } else if (c == 'q') {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        exit(0);
      }
    }
  }

  // Restore original terminal attributes immediately
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
  printf("\033[2J\033[H"); // Wipe menu out
  return selected;
}

/**
 * @file snapshot.c
 * @brief Implements high-resolution snapshot generation using stb_image_write.
 */

#include "snapshot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Configure header-only export symbols */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

void save_canvas_as_png(VirtualCanvas *vc, const char *text) {
  int text_len = strlen(text);
  if (text_len == 0 || !vc->line_map)
    return;

  // Resolve maximum wrapping lines to calculate target resolution boundaries
  int max_line_idx = 0;
  for (int i = 0; i < text_len; i++) {
    if (vc->line_map[i] > max_line_idx) {
      max_line_idx = vc->line_map[i];
    }
  }
  int total_lines = max_line_idx + 1;

  int max_chars_per_line = vc->width;
  int out_width = max_chars_per_line * 8;
  int out_height = total_lines * 8;

  u8 *img_data = calloc(out_width * out_height, 1);
  if (!img_data)
    return;

  int current_col = 0;
  int last_line_idx = 0;

  // Blit 8x8 font elements onto the output image array
  for (int i = 0; i < text_len; i++) {
    char c = text[i];
    int line_idx = vc->line_map[i];

    // Reset column counter at a line wrap boundary
    if (line_idx != last_line_idx) {
      current_col = 0;
      last_line_idx = line_idx;
    }

    if (current_col >= max_chars_per_line)
      continue;

    int font_idx = c - 32;
    if (font_idx < 0 || font_idx >= 95)
      font_idx = 31; // Fallback to Space for illegal characters

    for (int gy = 0; gy < 8; gy++) {
      u8 row_bits = font8x8_basic[font_idx][gy];
      int py = line_idx * 8 + gy;
      for (int gx = 0; gx < 8; gx++) {
        if (row_bits & (0x80 >> gx)) {
          int px = current_col * 8 + gx;
          if (px < out_width && py < out_height) {
            img_data[py * out_width + px] = 255;
          }
        }
      }
    }
    current_col++;
  }

  // Generate unique file identifiers using local timestamps
  char final_name[256];
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  strftime(final_name, sizeof(final_name), "text_canvas_%Y%m%d_%H%M%S.png", tm);

  // Write out raw pixel maps down to disk file streams
  if (stbi_write_png(final_name, out_width, out_height, 1, img_data,
                     out_width)) {
    printf("\033[%d;1H\033[2KSaved canvas snapshot to %s\n",
           get_terminal_height() - 1, final_name);
    fflush(stdout);
    sleep(1);
  }
  free(img_data);
}

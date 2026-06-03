/**
 * @file snapshot.c
 * @brief Implements high-resolution snapshot that renders the ASCII grid output
 *        (each character drawn with the 8x8 bitmap font), not the raw raster.
 *
 * BUG FIX: Previously the snapshot wrote a raw glyph bitmap, ignoring the
 * ASCII art grid produced by the renderer.  Now it iterates over every cell
 * in VirtualCanvas::grid, looks up the character in font8x8_basic, and blits
 * its 8x8 pixel pattern into the output image – giving you a pixel-exact
 * reproduction of what you see in the terminal.
 */

#include "snapshot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

void save_canvas_as_png(VirtualCanvas *vc, const char *text) {
  (void)text;

  if (!vc || !vc->grid || vc->width <= 0 || vc->height <= 0)
    return;

  /* Each grid cell becomes an 8x8 block of pixels. */
  int out_width = vc->width * 8;
  int out_height = vc->height * 8;

  /* Grayscale image – 1 byte per pixel, initialised to black (0). */
  u8 *img = calloc((size_t)out_width * out_height, 1);
  if (!img)
    return;

  for (int row = 0; row < vc->height; row++) {
    for (int col = 0; col < vc->width; col++) {
      char c = vc->grid[row * vc->width + col];

      /* Map ASCII to font8x8_basic index. */
      int font_idx = (int)c - 32;
      if (font_idx < 0 || font_idx >= 95)
        font_idx = 0; /* fallback to space */

      int px_base_x = col * 8;
      int px_base_y = row * 8;

      for (int gy = 0; gy < 8; gy++) {
        u8 row_bits = font8x8_basic[font_idx][gy];
        for (int gx = 0; gx < 8; gx++) {
          if (row_bits & (0x80 >> gx)) {
            int px = px_base_x + gx;
            int py = px_base_y + gy;
            if (px < out_width && py < out_height)
              img[py * out_width + px] = 255;
          }
        }
      }
    }
  }

  /* Generate timestamped filename */
  char filename[256];
  time_t t = time(NULL);
  struct tm *tm_info = localtime(&t);
  strftime(filename, sizeof(filename), "saved_text_%Y%m%d_%H%M%S.png",
           tm_info);

  if (stbi_write_png(filename, out_width, out_height, 1, img, out_width)) {
    printf("\033[%d;1H\033[2KSaved ASCII snapshot to %s  (%dx%d px)\n",
           get_terminal_height() - 1, filename, out_width, out_height);
    fflush(stdout);
    sleep(1);
  }

  free(img);
}

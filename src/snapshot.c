#include "snapshot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

void save_canvas_as_png(VirtualCanvas *vc, const char *text) {
  if (!vc->grid || vc->width <= 0 || vc->height <= 0)
    return;

  int out_width = vc->width;
  int out_height = vc->height;

  u8 *img_data = calloc(out_width * out_height, 1);
  if (!img_data)
    return;

  for (int i = 0; i < out_width * out_height; i++)
    img_data[i] = (vc->grid[i] != ' ') ? 255 : 0;

  char final_name[256];
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  strftime(final_name, sizeof(final_name), "text_canvas_%Y%m%d_%H%M%S.png", tm);

  if (stbi_write_png(final_name, out_width, out_height, 1, img_data,
                     out_width)) {
    printf("\033[%d;1H\033[2KSaved canvas snapshot to %s\n",
           get_terminal_height() - 1, final_name);
    fflush(stdout);
    sleep(1);
  }
}

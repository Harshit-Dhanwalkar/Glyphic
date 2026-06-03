#ifndef RENDERER_H
#define RENDERER_H

#include "font.h"

typedef struct {
  int width;
  int height;
  char *grid;
  int *line_map;
} VirtualCanvas;

int get_terminal_width(void);
int get_terminal_height(void);
VirtualCanvas layout_and_render_string(string8 file, font_info *info,
                                       const char *text, f32 scale,
                                       int term_width);
void display_workspace(VirtualCanvas *vc, const char *text, int cursor_idx,
                       int terminal_height, int terminal_width);

#endif

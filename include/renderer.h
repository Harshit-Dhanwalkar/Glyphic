/**
 * @file renderer.h
 * @brief Text layout manager and console workspace repainting pipeline.
 */

#ifndef RENDERER_H
#define RENDERER_H

#include "font.h"

/**
 * @struct VirtualCanvas
 * @brief Text-mode display backing buffer.
 */
typedef struct {
  int   width;     /**< Canvas character columns */
  int   height;    /**< Canvas pixel rows (rasterizer units) */
  char *grid;      /**< 1D character array [height * width] */
  int  *line_map;  /**< Maps each text char index → logical line number */
} VirtualCanvas;

/**
 * @struct ScrollState
 * @brief Tracks scroll offsets for both the main canvas and the minimap.
 */
typedef struct {
  int offset_rows;        /**< Canvas: top visible pixel-row  */
  int minimap_line_off;   /**< Minimap: index of first visible logical line */
} ScrollState;

int get_terminal_width(void);
int get_terminal_height(void);

VirtualCanvas layout_and_render_string(string8 file, font_info *info,
                                       const char *text, f32 scale,
                                       int term_width);

void display_workspace(VirtualCanvas *vc, const char *text, int cursor_idx,
                       int terminal_height, int terminal_width,
                       ScrollState *scroll);

void scroll_to_cursor(VirtualCanvas *vc, const char *text, int cursor_idx,
                      int terminal_height, ScrollState *scroll);

#endif

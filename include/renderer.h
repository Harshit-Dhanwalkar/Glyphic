/**
 * @file renderer.h
 * @brief Text layout manager and console workspace repainting pipeline.
 *
 * This system performs horizontal text layout calculations (tracking advances
 * and wraps), rasters text elements onto a text grid canvas using text
 * characters, and coordinates viewport presentation metrics alongside an
 * interactive workspace minimap.
 */

#ifndef RENDERER_H
#define RENDERER_H

#include "font.h"

/**
 * @struct VirtualCanvas
 * @brief Acts as a text-mode display backing buffer tracking the visual layout.
 */
typedef struct {
  int width;  /**< Absolute character horizontal stride width of layout
                 constraints */
  int height; /**< Absolute character row count allocation footprint constraint
               */
  char *grid; /**< 1D visual frame character array representing spatial pixel
                 maps */
  int *line_map; /**< Dynamic array mapping each input text character index to
                    its wrapped line row index */
} VirtualCanvas;

/**
 * @brief Queries hardware ioctl structures to verify active terminal console
 * character column limits.
 * @return int Target columns width count boundary. Defaults to standard 80
 * columns if query stream fails.
 */
int get_terminal_width(void);

/**
 * @brief Queries hardware ioctl structures to verify active terminal console
 * row line capacity bounds.
 * @return int Target console row height count capacity. Defaults to standard 24
 * rows if query stream fails.
 */
int get_terminal_height(void);

/**
 * @brief Computes font metrics and advances to compose text into an aligned
 * character grid.
 * @param file Mapped font binary content buffer.
 * @param info Mapped active font context metrics.
 * @param text Absolute raw input string value processing data string loops.
 * @param scale Geometric transformation multiplier factor applied during
 * processing routines.
 * @param term_width Layout line wrap boundary limit constraint value.
 * @return VirtualCanvas Computed layout backing array framework configuration.
 */
VirtualCanvas layout_and_render_string(string8 file, font_info *info,
                                       const char *text, f32 scale,
                                       int term_width);

/**
 * @brief Controls explicit ANSI command streams to redraw the workspace and
 * interactive minimap.
 * @param vc Pre-computed spatial character canvas mapping.
 * @param text Active text string storage.
 * @param cursor_idx Tracking context index position of the insertion caret.
 * @param terminal_height Total available console row limit constraints bounds.
 * @param terminal_width Total available console column limit constraints
 * bounds.
 */
void display_workspace(VirtualCanvas *vc, const char *text, int cursor_idx,
                       int terminal_height, int terminal_width);

#endif

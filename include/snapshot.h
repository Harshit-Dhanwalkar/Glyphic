/**
 * @file snapshot.h
 * @brief Snapshot utility for exporting text grids to high-resolution images.
 *
 * This file declares the interface used to blit the text-rendered character
 * layouts into high-resolution, pixel-exact PNG files using an 8x8 font pattern
 * tracker array.
 */

#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include "renderer.h"

/**
 * @brief Processes canvas layout positions and writes a high-resolution
 * snapshot out to a PNG image file.
 *
 * Uses an 8x8 monochrome typography matrix lookup to recreate character grids
 * precisely down to a grayscale image layout framework block.
 *
 * @param vc Canvas layout metadata context tracking wrapped boundaries.
 * @param text Raw source string data context parsing arrays.
 */
void save_canvas_as_png(VirtualCanvas *vc, const char *text);

#endif

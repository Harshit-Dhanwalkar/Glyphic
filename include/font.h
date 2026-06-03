/**
 * @file font.h
 * @brief Font parsing, glyph manipulation, and scanline rasterization
 * subsystem.
 *
 * This file contains data structures, macros, and function declarations for
 * processing TrueType Font (.ttf) files, indexing glyphs via cmap tables,
 * decomposing glyph outlines (including quadratic Bézier curves), and executing
 * a custom scanline rasterizer.
 */

#ifndef FONT_H
#define FONT_H

#include <stdint.h>

/* Explicit fixed-width types for readability and memory safety */
typedef int8_t i8;    /**< 8-bit signed integer */
typedef int16_t i16;  /**< 16-bit signed integer */
typedef int32_t i32;  /**< 32-bit signed integer */
typedef int64_t i64;  /**< 64-bit signed integer */
typedef uint8_t u8;   /**< 8-bit unsigned integer */
typedef uint16_t u16; /**< 16-bit unsigned integer */
typedef uint32_t u32; /**< 32-bit unsigned integer */
typedef uint64_t u64; /**< 64-bit unsigned integer */
typedef i8 b8;        /**< 8-bit boolean flag */
typedef i32 b32;      /**< 32-bit boolean flag */
typedef float f32;    /**< 32-bit floating point number */

/** @brief Computes the minimum of two values. */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
/** @brief Computes the maximum of two values. */
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/**
 * @brief Decodes a 16-bit unsigned integer from Big-Endian memory
 * representation.
 * @param m Pointer to the Big-Endian byte sequence.
 */
#define READ_BE16(m)                                                           \
  (u16)(((u16)(((u8 *)(m))[0]) << 8) | ((u16)(((u8 *)(m))[1]) << 0))

/**
 * @brief Decodes a 32-bit unsigned integer from Big-Endian memory
 * representation.
 * @param m Pointer to the Big-Endian byte sequence.
 */
#define READ_BE32(m)                                                           \
  (u32)(((u32)(((u8 *)(m))[0]) << 24) | ((u32)(((u8 *)(m))[1]) << 16) |        \
        ((u32)(((u8 *)(m))[2]) << 8) | ((u32)(((u8 *)(m))[3]) << 0))

/**
 * @brief Converts a 4-character string literal into a 32-bit integer tag used
 * by TTF tables.
 * @param m 4-character string representation.
 */
#define TAG(m) READ_BE32(m)

/**
 * @struct string8
 * @brief An immutable buffer wrapping a file's raw contents in memory.
 */
typedef struct {
  u8 *str;  /**< Pointer to the continuous block of heap-allocated memory */
  u64 size; /**< Absolute size of the buffer in bytes */
} string8;

/**
 * @struct v2_i16
 * @brief 2D mathematical vector structure utilizing 16-bit integers.
 */
typedef struct {
  i16 x, y; /**< Coordinate positions along the X and Y axes */
} v2_i16;

/**
 * @struct v2_f32
 * @brief 2D mathematical vector structure utilizing 32-bit floating-point
 * numbers.
 */
typedef struct {
  f32 x, y; /**< Coordinate positions along the X and Y axes */
} v2_f32;

/**
 * @struct font_info
 * @brief Internal metadata registry for tracking mapped TrueType Font binary
 * tables.
 */
typedef struct {
  u32 glyf; /**< Absolute file offset to the 'glyf' outline table */
  u32 loca; /**< Absolute file offset to the 'loca' index-to-location table */
  u32 head; /**< Absolute file offset to the 'head' global header table */
  u32 cmap_subtable; /**< Absolute file offset to the Segment Mapping format-12
                        cmap subtable */
  i16 loca_format;   /**< Form indexing indicator (0 for short 16-bit offsets, 1
                        for long 32-bit offsets) */
  u32 hmtx; /**< Absolute file offset to the 'hmtx' horizontal metrics table */
  u32 kern; /**< Absolute file offset to the 'kern' kerning data table */
  u32 hhea; /**< Absolute file offset to the 'hhea' horizontal header table */
  u16 num_hmetrics;    /**< Number of horizontal layout metric items declared in
                          the font */
  u16 *advance_widths; /**< Dynamically mapped array containing character
                          advance metrics */
  i16 *left_side_bearings; /**< Dynamically mapped array containing character
                              left side bounds */
  i16 ascent; /**< Maximum typographic distance above the coordinate baseline */
  i16 descent; /**< Maximum typographic distance below the coordinate baseline
                */
  i16 lineGap; /**< Typographic recommended spacing buffer applied between text
                  rows */
} font_info;

/**
 * @enum font_point_flag_enum
 * @brief Bitmask definitions specifying properties of outline points parsed
 * from 'glyf' records.
 */
typedef enum {
  FONT_POINT_FLAG_NONE = 0, /**< Implicit curve point descriptor */
  FONT_POINT_FLAG_ON_CURVE =
      1, /**< Explicit anchor point residing exactly on the contour line */
  FONT_POINT_FLAG_CONTOUR_END =
      2, /**< Termination boundary marker for a contour group sequence */
  FONT_POINT_FLAG_GENERATED = 4 /**< Midpoint calculated dynamically between
                                   adjacent off-curve points */
} font_point_flag_enum;
typedef u8 font_point_flag; /**< Bitmask storage abstraction type */

/**
 * @struct font_glyph
 * @brief Contains the geometric vector loops and bounding box parameters for an
 * individual glyph.
 */
typedef struct {
  i16 x_min; /**< Minimum bounding point along the design coordinate X axis */
  i16 y_min; /**< Minimum bounding point along the design coordinate Y axis */
  i16 x_max; /**< Maximum bounding point along the design coordinate X axis */
  i16 y_max; /**< Maximum bounding point along the design coordinate Y axis */
  u32 num_segments; /**< Evaluated total count of clean drawable segments (lines
                       or quadratic curves) */
  u32 num_points;   /**< Total number of geometric points evaluated inside this
                       record */
  u32 capacity; /**< Current capacity configuration of the tracking arrays (for
                   resizing) */
  font_point_flag *flags; /**< Dynamic array mapping specific structural flags
                             across outline vertices */
  v2_i16 *points; /**< Dynamic array storing coordinate nodes of vectors across
                     design coordinates */
} font_glyph;

/**
 * @struct bitmap_r8
 * @brief A grayscale 8-bit coverage mask generated by the glyph rasterizer.
 */
typedef struct {
  u32 width;  /**< Total horizontal resolution footprint of the coverage grid in
                 pixels */
  u32 height; /**< Total vertical resolution footprint of the coverage grid in
                 pixels */
  u8 *data;   /**< 1D grayscale pixel canvas (0 = transparent/empty, 255 = fully
                 opaque) */
} bitmap_r8;

/** * @brief Grayscale 8x8 bitmap block metrics representing legible characters.
 * * Sourced from the font8x8 open-source utility repository, this is used for
 * text snapshots.
 */
extern const u8 font8x8_basic[95][8];

/**
 * @brief Loads a target file directly into an allocated system memory
 * structure.
 * @param file_name System file path location.
 * @return string8 Encapsulated buffer mapping. Returns NULL values if tracking
 * streams fail.
 */
string8 read_file(const char *file_name);

/**
 * @brief Decodes root tables and metric registries from a standard TrueType
 * file header.
 * @param file Mapped font binary content buffer.
 * @param info Uninitialized font memory tracker map to fill.
 */
void font_init(string8 file, font_info *info);

/**
 * @brief Queries the active Segment format-12 cmap subtable to resolve an
 * internal glyph identification index.
 * @param file Mapped font binary content buffer.
 * @param info Mapped active font context metrics.
 * @param codepoint Target Unicode character point value.
 * @return u32 Target identifier code index. Returns index 0 (the fallback
 * missing glyph) if resolving fails.
 */
u32 font_glyph_index(string8 file, font_info *info, u32 codepoint);

/**
 * @brief Extracts layout contours, flags, and coordinate steps for a target
 * glyph index from the 'glyf' table.
 * @param file Mapped font binary content buffer.
 * @param info Mapped active font context metrics.
 * @param glyph Target uninitialized structure to allocate and fill.
 * @param glyph_index ID index returned by an earlier cmap evaluation query.
 */
void font_load_glyph(string8 file, font_info *info, font_glyph *glyph,
                     u32 glyph_index);

/**
 * @brief Safely releases heap structures allocated across an active font glyph
 * outline configuration block.
 * @param glyph Active structure context reference to purge.
 */
void font_free_glyph(font_glyph *glyph);

/**
 * @brief Resolves a mathematical scale multiplier for transforming internal
 * design space units into standard pixel metrics.
 * @param file Mapped font binary content buffer.
 * @param info Mapped active font context metrics.
 * @param pixels_per_em Desired point size scaling configuration (pixels per
 * EM).
 * @return f32 The calculated scale conversion factor.
 */
f32 font_scale_for_em(string8 file, font_info *info, f32 pixels_per_em);

/**
 * @brief Executes scanline coverage evaluations to convert mathematical vector
 * vectors into an 8-bit alpha bitmap.
 * @param glyph Mapped vector structural outlines coordinate points data.
 * @param scale Unit scale transformation factor.
 * @param padding Pixels count boundary buffer added around layout frames.
 * @return bitmap_r8 Generated alpha grayscale mask structure.
 */
bitmap_r8 font_raster_glyph(font_glyph *glyph, f32 scale, u32 padding);

#endif

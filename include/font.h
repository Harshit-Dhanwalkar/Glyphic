#ifndef FONT_H
#define FONT_H

#include <stdbool.h>
#include <stdint.h>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef i8 b8;
typedef i32 b32;
typedef float f32;

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define READ_BE16(m)                                                           \
  (u16)(((u16)(((u8 *)(m))[0]) << 8) | ((u16)(((u8 *)(m))[1]) << 0))

#define READ_BE32(m)                                                           \
  (u32)(((u32)(((u8 *)(m))[0]) << 24) | ((u32)(((u8 *)(m))[1]) << 16) |        \
        ((u32)(((u8 *)(m))[2]) << 8) | ((u32)(((u8 *)(m))[3]) << 0))

#define TAG(m) READ_BE32(m)

typedef struct {
  u8 *str;
  u64 size;
} string8;

typedef struct {
  i16 x, y;
} v2_i16;

typedef struct {
  f32 x, y;
} v2_f32;

typedef struct {
  u32 glyf, loca, head;
  u32 cmap_subtable;
  i16 loca_format;
  u32 hmtx, kern, hhea;
  u16 num_hmetrics;
  u16 *advance_widths;
  i16 *left_side_bearings;
  i16 ascent;
  i16 descent;
  i16 lineGap;
} font_info;

typedef enum {
  FONT_POINT_FLAG_NONE = 0,
  FONT_POINT_FLAG_ON_CURVE = 1,
  FONT_POINT_FLAG_CONTOUR_END = 2,
  FONT_POINT_FLAG_GENERATED = 4
} font_point_flag_enum;
typedef u8 font_point_flag;

typedef struct {
  i16 x_min;
  i16 y_min;
  i16 x_max;
  i16 y_max;
  u32 num_segments;
  u32 num_points;
  u32 capacity;
  font_point_flag *flags;
  v2_i16 *points;
} font_glyph;

typedef struct {
  u32 width, height;
  u8 *data;
} bitmap_r8;

extern const u8 font8x8_basic[95][8];

string8 read_file(const char *file_name);
void font_init(string8 file, font_info *info);
u32 font_glyph_index(string8 file, font_info *info, u32 codepoint);
void font_load_glyph(string8 file, font_info *info, font_glyph *glyph,
                     u32 glyph_index);
void font_free_glyph(font_glyph *glyph);
f32 font_scale_for_em(string8 file, font_info *info, f32 pixels_per_em);
bitmap_r8 font_raster_glyph(font_glyph *glyph, f32 scale, u32 padding);

#endif

#include <dirent.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

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

static const u8 font8x8_basic[95][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x18, 0x3c, 0x3c, 0x18, 0x18, 0x00, 0x18, 0x00},
    {0x6c, 0x6c, 0x6c, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x36, 0x36, 0x7f, 0x36, 0x7f, 0x36, 0x36, 0x00},
    {0x0c, 0x3e, 0x03, 0x1e, 0x30, 0x1f, 0x0c, 0x00},
    {0x00, 0x66, 0x66, 0x0c, 0x18, 0x33, 0x33, 0x00},
    {0x1c, 0x36, 0x1c, 0x3a, 0x6e, 0x36, 0x1d, 0x00},
    {0x18, 0x18, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x0c, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0c, 0x00},
    {0x30, 0x18, 0x0c, 0x0c, 0x0c, 0x18, 0x30, 0x00},
    {0x00, 0x66, 0x3c, 0xff, 0x3c, 0x66, 0x00, 0x00},
    {0x00, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x08},
    {0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00},
    {0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x00},
    {0x3e, 0x63, 0x67, 0x6f, 0x7b, 0x63, 0x3e, 0x00},
    {0x0c, 0x1c, 0x0c, 0x0c, 0x0c, 0x0c, 0x3f, 0x00},
    {0x3e, 0x63, 0x03, 0x1e, 0x30, 0x61, 0x7f, 0x00},
    {0x3e, 0x63, 0x03, 0x3e, 0x03, 0x63, 0x3e, 0x00},
    {0x06, 0x0e, 0x1e, 0x36, 0x7f, 0x06, 0x06, 0x00},
    {0x7f, 0x60, 0x7e, 0x03, 0x03, 0x63, 0x3e, 0x00},
    {0x1c, 0x30, 0x60, 0x7e, 0x63, 0x63, 0x3e, 0x00},
    {0x7f, 0x63, 0x03, 0x06, 0x0c, 0x18, 0x18, 0x00},
    {0x3e, 0x63, 0x63, 0x3e, 0x63, 0x63, 0x3e, 0x00},
    {0x3e, 0x63, 0x63, 0x3f, 0x03, 0x06, 0x3c, 0x00},
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00},
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x08, 0x00},
    {0x0c, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0c, 0x00},
    {0x00, 0x7e, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00},
    {0x30, 0x18, 0x0c, 0x06, 0x0c, 0x18, 0x30, 0x00},
    {0x3e, 0x63, 0x03, 0x06, 0x0c, 0x00, 0x0c, 0x00},
    {0x3e, 0x63, 0x6f, 0x6b, 0x6f, 0x60, 0x3e, 0x00},
    {0x18, 0x3c, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x00},
    {0x7e, 0x33, 0x33, 0x3e, 0x33, 0x33, 0x7e, 0x00},
    {0x1e, 0x33, 0x60, 0x60, 0x60, 0x33, 0x1e, 0x00},
    {0x7c, 0x36, 0x33, 0x33, 0x33, 0x36, 0x7c, 0x00},
    {0x7f, 0x33, 0x30, 0x3c, 0x30, 0x33, 0x7f, 0x00},
    {0x7f, 0x33, 0x30, 0x3c, 0x30, 0x30, 0x30, 0x00},
    {0x1e, 0x33, 0x60, 0x6c, 0x63, 0x33, 0x1f, 0x00},
    {0x66, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x66, 0x00},
    {0x3c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00},
    {0x1e, 0x0c, 0x0c, 0x0c, 0x0c, 0xcc, 0x78, 0x00},
    {0x66, 0x6c, 0x78, 0x70, 0x78, 0x6c, 0x66, 0x00},
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x33, 0x7f, 0x00},
    {0x63, 0x77, 0x7f, 0x6b, 0x63, 0x63, 0x63, 0x00},
    {0x63, 0x67, 0x6f, 0x7b, 0x73, 0x63, 0x63, 0x00},
    {0x3e, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00},
    {0x7e, 0x33, 0x33, 0x3e, 0x30, 0x30, 0x30, 0x00},
    {0x3e, 0x63, 0x63, 0x63, 0x6b, 0x67, 0x3e, 0x03},
    {0x7e, 0x33, 0x33, 0x3e, 0x36, 0x33, 0x63, 0x00},
    {0x3e, 0x63, 0x38, 0x0e, 0x03, 0x63, 0x3e, 0x00},
    {0x7f, 0x5d, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
    {0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00},
    {0x63, 0x63, 0x63, 0x63, 0x32, 0x1c, 0x08, 0x00},
    {0x63, 0x63, 0x63, 0x6b, 0x7f, 0x77, 0x63, 0x00},
    {0x63, 0x63, 0x36, 0x1c, 0x1c, 0x36, 0x63, 0x00},
    {0x00, 0x00, 0x63, 0x63, 0x63, 0x3f, 0x03, 0x3e},
    {0x00, 0x00, 0x7f, 0x46, 0x1c, 0x30, 0x7f, 0x00},
    {0x0c, 0x18, 0x18, 0x30, 0x18, 0x18, 0x0c, 0x00},
    {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00},
    {0x30, 0x18, 0x18, 0x0c, 0x18, 0x18, 0x30, 0x00},
    {0x00, 0x00, 0x3b, 0x6e, 0x00, 0x00, 0x00, 0x00},
};

char *expand_home(const char *path);
void font_init(string8 file, font_info *info);
u32 font_glyph_index(string8 file, font_info *info, u32 codepoint);
void font_load_glyph(string8 file, font_info *info, font_glyph *glyph,
                     u32 glyph_index);
void font_free_glyph(font_glyph *glyph);
f32 font_scale_for_em(string8 file, font_info *info, f32 pixels_per_em);
bitmap_r8 font_raster_glyph(font_glyph *glyph, f32 scale, u32 padding);
string8 read_file(const char *file_name);

int get_terminal_width() {
  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  return w.ws_col > 0 ? w.ws_col : 80;
}

int get_terminal_height() {
  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  return w.ws_row > 0 ? w.ws_row : 24;
}

void set_raw_mode(struct termios *orig) {
  struct termios raw;
  tcgetattr(STDIN_FILENO, orig);
  raw = *orig;
  raw.c_lflag &= ~(ICANON | ECHO);
  raw.c_iflag &= ~(IXON);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void font_init(string8 file, font_info *info) {
  memset(info, 0, sizeof(font_info));
  u32 cmap = 0;
  u32 num_tables = READ_BE16(file.str + 4);
  for (u32 i = 0; i < num_tables; i++) {
    u32 mem_offset = 12 + 16 * i;
    u32 tag = READ_BE32(file.str + mem_offset + 0);
    u32 offset = READ_BE32(file.str + mem_offset + 8);
    if (tag == TAG("cmap"))
      cmap = offset;
    else if (tag == TAG("glyf"))
      info->glyf = offset;
    else if (tag == TAG("loca"))
      info->loca = offset;
    else if (tag == TAG("head"))
      info->head = offset;
    else if (tag == TAG("hmtx"))
      info->hmtx = offset;
    else if (tag == TAG("kern"))
      info->kern = offset;
    else if (tag == TAG("hhea"))
      info->hhea = offset;
  }
  u32 cmap_num_subtables = READ_BE16(file.str + cmap + 2);
  for (u32 i = 0; i < cmap_num_subtables; i++) {
    u32 mem_offset = cmap + 4 + 8 * i;
    u32 offset = READ_BE32(file.str + mem_offset + 4);
    u16 format = READ_BE16(file.str + cmap + offset);
    if (format == 12) {
      info->cmap_subtable = cmap + offset;
      break;
    }
  }
  info->loca_format = (i16)READ_BE16(file.str + info->head + 50);
  if (info->hhea) {
    info->ascent = (i16)READ_BE16(file.str + info->hhea + 4);
    info->descent = (i16)READ_BE16(file.str + info->hhea + 6);
    info->lineGap = (i16)READ_BE16(file.str + info->hhea + 8);
    info->num_hmetrics = READ_BE16(file.str + info->hhea + 34);
    info->advance_widths = malloc(info->num_hmetrics * sizeof(u16));
    info->left_side_bearings = malloc(info->num_hmetrics * sizeof(i16));
    for (u32 i = 0; i < info->num_hmetrics; i++) {
      info->advance_widths[i] = READ_BE16(file.str + info->hmtx + i * 4);
      info->left_side_bearings[i] =
          (i16)READ_BE16(file.str + info->hmtx + i * 4 + 2);
    }
  }
}

u32 font_glyph_index(string8 file, font_info *info, u32 codepoint) {
  if (!info->cmap_subtable)
    return 0;
  u8 *subtable = file.str + info->cmap_subtable;
  u32 num_groups = READ_BE32(subtable + 12);
  for (u32 i = 0; i < num_groups; i++) {
    u32 group_offset = 16 + i * 12;
    u32 start_code = READ_BE32(subtable + group_offset + 0);
    u32 end_code = READ_BE32(subtable + group_offset + 4);
    u32 index_offset = READ_BE32(subtable + group_offset + 8);
    if (start_code <= codepoint && codepoint <= end_code) {
      return codepoint - start_code + index_offset;
    }
  }
  return 0;
}

void _font_glyph_init(font_glyph *glyph) {
  memset(glyph, 0, sizeof(font_glyph));
  glyph->capacity = 8;
  glyph->flags = malloc(sizeof(font_point_flag) * glyph->capacity);
  glyph->points = malloc(sizeof(v2_i16) * glyph->capacity);
}

void _font_glyph_append(font_glyph *glyph, font_point_flag flag, v2_i16 point) {
  if (glyph->num_points >= glyph->capacity) {
    glyph->capacity *= 2;
    glyph->flags =
        realloc(glyph->flags, sizeof(font_point_flag) * glyph->capacity);
    glyph->points = realloc(glyph->points, sizeof(v2_i16) * glyph->capacity);
  }
  glyph->num_points++;
  glyph->flags[glyph->num_points - 1] = flag;
  glyph->points[glyph->num_points - 1] = point;
}

void font_load_glyph(string8 file, font_info *info, font_glyph *glyph,
                     u32 glyph_index) {
  _font_glyph_init(glyph);
  u32 offset = info->loca_format == 0
                   ? (u32)READ_BE16(file.str + info->loca + glyph_index * 2) * 2
                   : READ_BE32(file.str + info->loca + glyph_index * 4);
  u8 *glyf = file.str + info->glyf + offset;
  i16 num_contours = READ_BE16(glyf);
  if (num_contours <= 0)
    return;
  glyph->x_min = READ_BE16(glyf + 2);
  glyph->y_min = READ_BE16(glyf + 4);
  glyph->x_max = READ_BE16(glyf + 6);
  glyph->y_max = READ_BE16(glyf + 8);
  u32 num_raw_points = READ_BE16(glyf + 10 + 2 * (num_contours - 1)) + 1;
  u8 *flags_raw = malloc(num_raw_points);
  v2_i16 *points_raw = malloc(sizeof(v2_i16) * num_raw_points);
  u16 num_instructions = READ_BE16(glyf + 10 + 2 * num_contours);
  u8 *point_data = glyf + 12 + 2 * num_contours + num_instructions;
  u32 num_flags = 0;
  while (num_flags < num_raw_points) {
    u8 flag = *(point_data++);
    flags_raw[num_flags++] = flag;
    if (flag & 0x08) {
      u8 count = *(point_data++);
      while (count--)
        flags_raw[num_flags++] = flag;
    }
  }
  i16 x = 0;
  for (u32 i = 0; i < num_raw_points; i++) {
    if (flags_raw[i] & 0x02) {
      u8 diff = *(point_data++);
      x += (flags_raw[i] & 0x10) ? diff : -(i16)diff;
    } else if ((flags_raw[i] & 0x10) != 0x10) {
      i16 diff = (i16)READ_BE16(point_data);
      point_data += 2;
      x += diff;
    }
    points_raw[i].x = x;
  }
  i16 y = 0;
  for (u32 i = 0; i < num_raw_points; i++) {
    if (flags_raw[i] & 0x04) {
      u8 diff = *(point_data++);
      y += (flags_raw[i] & 0x20) ? diff : -(i16)diff;
    } else if ((flags_raw[i] & 0x20) != 0x20) {
      i16 diff = (i16)READ_BE16(point_data);
      point_data += 2;
      y += diff;
    }
    points_raw[i].y = y;
  }
  for (i32 c = 0; c < num_contours; c++) {
    u32 start_point = c == 0 ? 0 : READ_BE16(glyf + 10 + 2 * (c - 1)) + 1;
    u32 end_point = READ_BE16(glyf + 10 + 2 * c);
    i32 num_points = (i32)end_point - start_point + 1;
    i32 point_offset = 0;
    for (i32 i = 0; i < num_points; i++) {
      u32 p0_i = ((i + point_offset + 0) % num_points) + start_point;
      u32 p1_i = ((i + point_offset + 1) % num_points) + start_point;
      u32 p2_i = ((i + point_offset + 2) % num_points) + start_point;
      u32 on_curve =
          (((flags_raw[p0_i] & 0x1) << 2) | ((flags_raw[p1_i] & 0x1) << 1) |
           ((flags_raw[p2_i] & 0x1) << 0));
      v2_i16 p0 = points_raw[p0_i];
      v2_i16 p1 = points_raw[p1_i];
      v2_i16 p2 = points_raw[p2_i];
      font_point_flag f0 = FONT_POINT_FLAG_ON_CURVE;
      font_point_flag f1 = 0;
      font_point_flag f2 = FONT_POINT_FLAG_ON_CURVE;
      b32 bezier = true;
      switch (on_curve) {
      case 0b110:
      case 0b111:
        p2 = p1;
        bezier = false;
        break;
      case 0b101:
        i++;
        break;
      case 0b100:
        p2 = (v2_i16){(p1.x + p2.x) / 2, (p1.y + p2.y) / 2};
        f2 |= FONT_POINT_FLAG_GENERATED;
        break;
      case 0b001:
        p0 = (v2_i16){(p0.x + p1.x) / 2, (p0.y + p1.y) / 2};
        f0 |= FONT_POINT_FLAG_GENERATED;
        i++;
        break;
      case 0b000:
        p0 = (v2_i16){(p0.x + p1.x) / 2, (p0.y + p1.y) / 2};
        p2 = (v2_i16){(p1.x + p2.x) / 2, (p1.y + p2.y) / 2};
        f0 |= FONT_POINT_FLAG_GENERATED;
        f2 |= FONT_POINT_FLAG_GENERATED;
        break;
      default:
        point_offset++;
        i--;
        break;
      }
      glyph->num_segments++;
      _font_glyph_append(glyph, f0, p0);
      if (bezier)
        _font_glyph_append(glyph, f1, p1);
      if (i == num_points - 1) {
        f2 |= FONT_POINT_FLAG_CONTOUR_END;
        _font_glyph_append(glyph, f2, p2);
      }
    }
  }
  free(points_raw);
  free(flags_raw);
}

void font_free_glyph(font_glyph *glyph) {
  free(glyph->flags);
  free(glyph->points);
  memset(glyph, 0, sizeof(font_glyph));
}

f32 font_scale_for_em(string8 file, font_info *info, f32 pixels_per_em) {
  f32 units_per_em = READ_BE16(file.str + info->head + 18);
  return pixels_per_em / units_per_em;
}

bitmap_r8 font_raster_glyph(font_glyph *glyph, f32 scale, u32 padding) {
  f32 x_min_scaled = (f32)glyph->x_min * scale;
  f32 y_min_scaled = (f32)glyph->y_min * scale;
  f32 x_max_scaled = (f32)glyph->x_max * scale;
  f32 y_max_scaled = (f32)glyph->y_max * scale;
  bitmap_r8 bitmap = {
      .width = (u32)ceilf(x_max_scaled - x_min_scaled) + padding * 2,
      .height = (u32)ceilf(y_max_scaled - y_min_scaled) + padding * 2,
  };
  if (bitmap.width == 0 || bitmap.height == 0) {
    bitmap.width = (bitmap.width == 0) ? 1 : bitmap.width;
    bitmap.height = (bitmap.height == 0) ? 1 : bitmap.height;
  }
  bitmap.data = calloc(bitmap.width * bitmap.height, 1);
  v2_f32 *points = malloc(sizeof(v2_f32) * glyph->num_points);
  for (u32 i = 0; i < glyph->num_points; i++) {
    points[i] = (v2_f32){
        (f32)(glyph->points[i].x - glyph->x_min) * scale + padding,
        (f32)(glyph->points[i].y - glyph->y_max) * (-scale) + padding,
    };
  }
  f32 *intersects = malloc(sizeof(f32) * glyph->num_segments * 2);
  u32 y_start = (u32)fmaxf(0, floorf(y_min_scaled + padding));
  u32 y_end = (u32)fminf(bitmap.height, ceilf(y_max_scaled + padding));
  for (u32 y_i = y_start; y_i < y_end; y_i++) {
    f32 y = (f32)y_i + 0.5f;
    u32 num_intersects = 0;
    u32 idx = 0;
    for (u32 seg = 0; seg < glyph->num_segments; seg++) {
      v2_f32 p0 = points[idx++];
      v2_f32 p1 = points[idx];
      b32 is_line = (glyph->flags[idx] & FONT_POINT_FLAG_ON_CURVE) != 0;
      if (is_line) {
        if (p0.y != p1.y) {
          f32 min_y = MIN(p0.y, p1.y);
          f32 max_y = MAX(p0.y, p1.y);
          if (min_y <= y && y <= max_y) {
            f32 t = (y - p0.y) / (p1.y - p0.y);
            if (t >= 0 && t <= 1)
              intersects[num_intersects++] = p0.x + t * (p1.x - p0.x);
          }
        }
      } else {
        v2_f32 p2 = points[++idx];
        f32 min_y = MIN(p0.y, MIN(p1.y, p2.y));
        f32 max_y = MAX(p0.y, MAX(p1.y, p2.y));
        if (min_y <= y && y <= max_y) {
          f32 a_y = p2.y - 2 * p1.y + p0.y;
          f32 b_y = 2 * (p1.y - p0.y);
          f32 c_y = p0.y - y;
          f32 a_x = p2.x - 2 * p1.x + p0.x;
          f32 b_x = 2 * (p1.x - p0.x);
          f32 c_x = p0.x;
          if (fabsf(a_y) < 1e-6f) {
            f32 t = -c_y / b_y;
            if (t >= 0 && t <= 1)
              intersects[num_intersects++] = t * (t * a_x + b_x) + c_x;
          } else {
            f32 disc = b_y * b_y - 4 * a_y * c_y;
            if (disc >= 0) {
              f32 sqrt_disc = sqrtf(disc);
              f32 t0 = (-b_y + sqrt_disc) / (2 * a_y);
              f32 t1 = (-b_y - sqrt_disc) / (2 * a_y);
              if (t0 >= 0 && t0 <= 1)
                intersects[num_intersects++] = t0 * (t0 * a_x + b_x) + c_x;
              if (t1 >= 0 && t1 <= 1)
                intersects[num_intersects++] = t1 * (t1 * a_x + b_x) + c_x;
            }
          }
        }
      }
      if (glyph->flags[idx] & FONT_POINT_FLAG_CONTOUR_END)
        idx++;
    }
    for (u32 i = 1; i < num_intersects; i++) {
      f32 key = intersects[i];
      i32 j = i - 1;
      while (j >= 0 && intersects[j] > key) {
        intersects[j + 1] = intersects[j];
        j--;
      }
      intersects[j + 1] = key;
    }
    u8 *row = bitmap.data + y_i * bitmap.width;
    for (u32 i = 0; i + 1 < num_intersects; i += 2) {
      f32 x_start = intersects[i];
      f32 x_end = intersects[i + 1];
      u32 xs = (u32)x_start;
      u32 xe = (u32)x_end;
      if (xs >= bitmap.width)
        continue;
      if (xe >= bitmap.width)
        xe = bitmap.width - 1;
      memset(row + xs, 255, xe - xs + 1);
      row[xs] = (u8)(255 * (1 - (x_start - xs)));
      row[xe] = (u8)(255 * (x_end - xe));
    }
  }
  free(points);
  free(intersects);
  return bitmap;
}

string8 read_file(const char *file_name) {
  FILE *f = fopen(file_name, "rb");
  if (!f)
    return (string8){.str = NULL, .size = 0};
  fseek(f, 0, SEEK_END);
  u64 size = ftell(f);
  fseek(f, 0, SEEK_SET);
  u8 *data = malloc(size);
  if (!data) {
    fclose(f);
    return (string8){.str = NULL, .size = 0};
  }
  fread(data, 1, size, f);
  fclose(f);
  return (string8){.str = data, .size = size};
}

typedef struct {
  int width;
  int height;
  char *grid;
  int *line_map;
} VirtualCanvas;

VirtualCanvas layout_and_render_string(string8 file, font_info *info,
                                       const char *text, f32 scale,
                                       int term_width) {
  VirtualCanvas vc = {0};
  u32 len = strlen(text);

  u32 metric_count = len + 1;
  font_glyph *glyphs = malloc(metric_count * sizeof(font_glyph));
  f32 *glyph_widths = malloc(metric_count * sizeof(f32));

  for (u32 i = 0; i < len; i++) {
    u32 g_id = font_glyph_index(file, info, (u8)text[i]);
    font_load_glyph(file, info, &glyphs[i], g_id);
    u16 adv = (g_id < info->num_hmetrics)
                  ? info->advance_widths[g_id]
                  : info->advance_widths[info->num_hmetrics - 1];
    glyph_widths[i] = (f32)adv * scale;
  }
  u32 space_g_id = font_glyph_index(file, info, ' ');
  u16 default_adv = (space_g_id < info->num_hmetrics)
                        ? info->advance_widths[space_g_id]
                        : 500;
  glyph_widths[len] = (f32)default_adv * scale;
  memset(&glyphs[len], 0, sizeof(font_glyph));

  f32 scaled_ascent = (f32)info->ascent * scale;
  f32 scaled_descent = (f32)info->descent * scale;
  f32 line_height =
      (scaled_ascent - scaled_descent + (f32)info->lineGap * scale) * 1.1f;
  if (line_height < 1.0f)
    line_height = 10.0f;

  int max_needed_width = term_width - 4;
  if (max_needed_width < 40)
    max_needed_width = 40;

  int current_line = 0;
  f32 cursor_x = 0;

  vc.line_map = malloc(metric_count * sizeof(int));
  f32 *glyph_x_positions = malloc(metric_count * sizeof(f32));

  for (u32 i = 0; i <= len; i++) {
    if (cursor_x + glyph_widths[i] > max_needed_width && cursor_x > 0) {
      current_line++;
      cursor_x = 0;
    }
    vc.line_map[i] = current_line;
    glyph_x_positions[i] = cursor_x;

    if (i < len) {
      cursor_x += glyph_widths[i];
    }
  }

  int total_lines_count = current_line + 1;
  int canvas_w = max_needed_width;
  int canvas_h = (int)(total_lines_count * line_height);
  if (canvas_h < (int)line_height)
    canvas_h = (int)line_height;

  vc.width = canvas_w;
  vc.height = canvas_h;
  vc.grid = malloc(canvas_w * canvas_h);
  memset(vc.grid, ' ', canvas_w * canvas_h);

  for (u32 i = 0; i < len; i++) {
    if (glyphs[i].num_points == 0) {
      font_free_glyph(&glyphs[i]);
      continue;
    }

    bitmap_r8 gb = font_raster_glyph(&glyphs[i], scale, 0);
    if (gb.width == 0 || gb.height == 0 || !gb.data) {
      font_free_glyph(&glyphs[i]);
      continue;
    }

    u8 local_max = 0;
    for (u32 idx = 0; idx < gb.width * gb.height; idx++) {
      if (gb.data[idx] > local_max)
        local_max = gb.data[idx];
    }

    int base_line_idx = vc.line_map[i];
    f32 baseline_y = (base_line_idx * line_height) + scaled_ascent;
    int start_x = (int)glyph_x_positions[i];
    int start_y = (int)(baseline_y - ((f32)glyphs[i].y_max * scale));

    for (u32 y = 0; y < gb.height; y++) {
      int target_y = start_y + y;
      if (target_y < 0 || target_y >= canvas_h)
        continue;

      for (u32 x = 0; x < gb.width; x++) {
        int target_x = start_x + x;
        if (target_x < 0 || target_x >= canvas_w)
          continue;

        u8 intensity = gb.data[y * gb.width + x];
        if (local_max > 0) {
          intensity = (intensity * 255) / local_max;
        }

        if (intensity > 120) {
          vc.grid[target_y * canvas_w + target_x] = text[i];
        }
      }
    }

    free(gb.data);
    font_free_glyph(&glyphs[i]);
  }

  free(glyphs);
  free(glyph_widths);
  free(glyph_x_positions);

  return vc;
}

void display_workspace(VirtualCanvas *vc, const char *text, int cursor_idx,
                       int terminal_height, int terminal_width) {
  printf("\033[H");

  int minimap_reserve_rows = 6;
  int max_allowed_lines = terminal_height - minimap_reserve_rows - 4;
  int display_h = vc->height;
  if (display_h > max_allowed_lines)
    display_h = max_allowed_lines;

  // Clear workspace bounds safely
  for (int y = 0; y < terminal_height; y++) {
    printf("\033[%d;1H\033[2K", y + 1);
  }
  printf("\033[H");

  // Render high-res structural text canvas
  for (int y = 0; y < display_h; y++) {
    printf("\033[%d;1H", y + 1);
    for (int x = 0; x < vc->width && x < terminal_width; x++) {
      putchar(vc->grid[y * vc->width + x]);
    }
  }

  int minimap_start_row = terminal_height - minimap_reserve_rows - 3;

  // 1. DYNAMIC TOP BORDER
  printf("\033[%d;1H+", minimap_start_row);
  int title_len = 11; // Length of " Minimap "
  int left_dashes = (terminal_width - 2 - title_len) / 2;
  int right_dashes = terminal_width - 2 - title_len - left_dashes;
  for (int i = 0; i < left_dashes; i++)
    putchar('-');
  printf(" Minimap ");
  for (int i = 0; i < right_dashes; i++)
    putchar('-');
  putchar('+');

  int text_len = strlen(text);
  int map_row_count = 0;
  int current_map_line = -1;
  int current_col_offset = 0;
  int usable_width = terminal_width - 4; // "| " and " |" take 4 cells

  // 2. WRAPPED CHAR OVERVIEW SCAN
  for (int i = 0; i <= text_len; i++) {
    int target_line = (i <= text_len) ? vc->line_map[i] : current_map_line;

    if (target_line != current_map_line) {
      if (current_map_line != -1) {
        // Safe line right-padding boundary
        printf("\033[90m"); // Start Gray
        while (current_col_offset < usable_width) {
          printf("·");
          current_col_offset++;
        }
        printf("\033[0m |"); // End Gray + Border End
      }
      current_map_line = target_line;
      map_row_count++;
      if (map_row_count > minimap_reserve_rows)
        break;

      printf("\033[%d;1H| ", minimap_start_row + map_row_count);
      current_col_offset = 0;
    }

    if (i == text_len)
      break;

    // Hard cutoff to prevent blowing past the dynamic right border
    if (current_col_offset >= usable_width)
      continue;

    if (i == cursor_idx) {
      printf("\033[7m%c\033[0m", text[i] == ' ' ? '_' : text[i]);
    } else {
      putchar(text[i]);
    }
    current_col_offset++;
  }

  // Handle trailing line fill properties safely
  if (map_row_count > 0 && map_row_count <= minimap_reserve_rows) {
    if (cursor_idx == text_len && current_col_offset < usable_width) {
      printf("\033[7m \033[0m");
      current_col_offset++;
    }
    printf("\033[90m"); // Start dim gray dots
    while (current_col_offset < usable_width) {
      printf("·");
      current_col_offset++;
    }
    printf("\033[0m |");
  }

  // 3. DYNAMIC BOTTOM BORDER
  printf("\033[%d;1H+", minimap_start_row + map_row_count + 1);
  for (int i = 0; i < terminal_width - 2; i++)
    putchar('-');
  putchar('+');
}

void save_canvas_as_png(VirtualCanvas *vc, const char *text) {
  if (!vc->grid || vc->width <= 0 || vc->height <= 0)
    return;

  // Convert it directly to a grayscale bitmap: non-space = white, space = black.
  int out_width = vc->width;
  int out_height = vc->height;

  u8 *img_data = calloc(out_width * out_height, 1);
  if (!img_data)
    return;

  for (int i = 0; i < out_width * out_height; i++) {
    img_data[i] = (vc->grid[i] != ' ') ? 255 : 0;
  }

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
  free(img_data);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <font_file.ttf> [initial_em_size]\n", argv[0]);
    return 1;
  }
  const char *font_path = argv[1];
  float initial_em = (argc >= 3) ? atof(argv[2]) : 48.0f;
  string8 file = read_file(font_path);
  if (!file.str) {
    fprintf(stderr, "Error: Cannot open font file '%s'\n", font_path);
    return 1;
  }
  font_info font = {0};
  font_init(file, &font);

  char sentence[2048] = "";
  int sentence_len = 0;
  int cursor_index = 0;
  f32 em_size = initial_em;

  struct termios orig;
  set_raw_mode(&orig);

  printf("\033[2J\033[H");
  fflush(stdout);

  int running = 1;
  while (running) {
    int term_width = get_terminal_width();
    int term_height = get_terminal_height();
    f32 scale = font_scale_for_em(file, &font, em_size);

    VirtualCanvas vc =
        layout_and_render_string(file, &font, sentence, scale, term_width);

    if (vc.grid) {
      display_workspace(&vc, sentence, cursor_index, term_height, term_width);
    }

    int menu_row = term_height - 1;
    printf("\033[%d;1H\033[2KSize: %.1f px/em | +/-: Zoom | Arrow Keys: "
           "Navigation | Ctrl+S: Save | 'q': Quit",
           menu_row, em_size);
    fflush(stdout);

    char c = getchar();
    if (c == '\033') {
      char seq1 = getchar();
      if (seq1 == '[') {
        char seq2 = getchar();
        if (seq2 == 'D') { // Left arrow key
          if (cursor_index > 0)
            cursor_index--;
        } else if (seq2 == 'C') { // Right arrow key
          if (cursor_index < sentence_len)
            cursor_index++;
        }
      }
      if (vc.grid) {
        free(vc.grid);
        free(vc.line_map);
      }
      continue;
    }

    switch (c) {
    case '+':
      em_size = fminf(300.0f, em_size * 1.1f);
      printf("\033[2J\033[H");
      break;
    case '-':
      em_size = fmaxf(12.0f, em_size / 1.1f);
      printf("\033[2J\033[H");
      break;
    case 127:
    case '\b':
      if (cursor_index > 0 && sentence_len > 0) {
        for (int i = cursor_index - 1; i < sentence_len; i++) {
          sentence[i] = sentence[i + 1];
        }
        sentence_len--;
        cursor_index--;
      }
      break;
    case 19: // Ctrl+S
      if (vc.grid) {
        save_canvas_as_png(&vc, sentence);
      }
      break;
    case 'q':
      running = 0;
      break;
    default:
      if (c >= 32 && c <= 126 && sentence_len < (int)sizeof(sentence) - 1) {
        for (int i = sentence_len; i >= cursor_index; i--) {
          sentence[i + 1] = sentence[i];
        }
        sentence[cursor_index] = c;
        sentence_len++;
        cursor_index++;
      }
      break;
    }

    if (vc.grid) {
      free(vc.grid);
      free(vc.line_map);
    }
  }

  printf("\033[2J\033[H");
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
  free(font.advance_widths);
  free(font.left_side_bearings);
  free(file.str);
  return 0;
}

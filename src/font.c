/**
 * @file font.c
 * @brief Implements core TrueType binary structure parsing and scanline glyph
 * rasterization.
 */

#include "font.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/**
 * @brief Hardcoded font tracking array mapping index 0-94 to printable
 * character matrices (ASCII 32-126).
 *
 * Extracted from standard console diagnostic utilities, each entry maps 8
 * sequential bytes where each byte acts as a horizontal bit field sequence.
 */
const u8 font8x8_basic[95][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // [Space]
    {0x18, 0x3c, 0x3c, 0x18, 0x18, 0x00, 0x18, 0x00}, // !
    {0x6c, 0x6c, 0x6c, 0x00, 0x00, 0x00, 0x00, 0x00}, // "
    {0x36, 0x36, 0x7f, 0x36, 0x7f, 0x36, 0x36, 0x00}, // #
    {0x0c, 0x3e, 0x03, 0x1e, 0x30, 0x1f, 0x0c, 0x00}, // $
    {0x00, 0x66, 0x66, 0x0c, 0x18, 0x33, 0x33, 0x00}, // %
    {0x1c, 0x36, 0x1c, 0x3a, 0x6e, 0x36, 0x1d, 0x00}, // &
    {0x18, 0x18, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00}, // '
    {0x0c, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0c, 0x00}, // (
    {0x30, 0x18, 0x0c, 0x0c, 0x0c, 0x18, 0x30, 0x00}, // )
    {0x00, 0x66, 0x3c, 0xff, 0x3c, 0x66, 0x00, 0x00}, // *
    {0x00, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x00}, // +
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x08}, // ,
    {0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00}, // -
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00}, // .
    {0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x00}, // /
    {0x3e, 0x63, 0x67, 0x6f, 0x7b, 0x63, 0x3e, 0x00}, // 0
    {0x0c, 0x1c, 0x0c, 0x0c, 0x0c, 0x0c, 0x3f, 0x00}, // 1
    {0x3e, 0x63, 0x03, 0x1e, 0x30, 0x61, 0x7f, 0x00}, // 2
    {0x3e, 0x63, 0x03, 0x3e, 0x03, 0x63, 0x3e, 0x00}, // 3
    {0x06, 0x0e, 0x1e, 0x36, 0x7f, 0x06, 0x06, 0x00}, // 4
    {0x7f, 0x60, 0x7e, 0x03, 0x03, 0x63, 0x3e, 0x00}, // 5
    {0x1c, 0x30, 0x60, 0x7e, 0x63, 0x63, 0x3e, 0x00}, // 6
    {0x7f, 0x63, 0x03, 0x06, 0x0c, 0x18, 0x18, 0x00}, // 7
    {0x3e, 0x63, 0x63, 0x3e, 0x63, 0x63, 0x3e, 0x00}, // 8
    {0x3e, 0x63, 0x63, 0x3f, 0x03, 0x06, 0x3c, 0x00}, // 9
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00}, // :
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x08, 0x00}, // ;
    {0x0c, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0c, 0x00}, // <
    {0x00, 0x7e, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00}, // =
    {0x30, 0x18, 0x0c, 0x06, 0x0c, 0x18, 0x30, 0x00}, // >
    {0x3e, 0x63, 0x03, 0x06, 0x0c, 0x00, 0x0c, 0x00}, // ?
    {0x3e, 0x63, 0x6f, 0x6b, 0x6f, 0x60, 0x3e, 0x00}, // @
    {0x18, 0x3c, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x00}, // A
    {0x7e, 0x33, 0x33, 0x3e, 0x33, 0x33, 0x7e, 0x00}, // B
    {0x1e, 0x33, 0x60, 0x60, 0x60, 0x33, 0x1e, 0x00}, // C
    {0x7c, 0x36, 0x33, 0x33, 0x33, 0x36, 0x7c, 0x00}, // D
    {0x7f, 0x33, 0x30, 0x3c, 0x30, 0x33, 0x7f, 0x00}, // E
    {0x7f, 0x33, 0x30, 0x3c, 0x30, 0x30, 0x30, 0x00}, // F
    {0x1e, 0x33, 0x60, 0x6c, 0x63, 0x33, 0x1f, 0x00}, // G
    {0x66, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x66, 0x00}, // H
    {0x3c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00}, // I
    {0x1e, 0x0c, 0x0c, 0x0c, 0x0c, 0xcc, 0x78, 0x00}, // J
    {0x66, 0x6c, 0x78, 0x70, 0x78, 0x6c, 0x66, 0x00}, // K
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x33, 0x7f, 0x00}, // L
    {0x63, 0x77, 0x7f, 0x6b, 0x63, 0x63, 0x63, 0x00}, // M
    {0x63, 0x67, 0x6f, 0x7b, 0x73, 0x63, 0x63, 0x00}, // N
    {0x3e, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00}, // O
    {0x7e, 0x33, 0x33, 0x3e, 0x30, 0x30, 0x30, 0x00}, // P
    {0x3e, 0x63, 0x63, 0x63, 0x6b, 0x67, 0x3e, 0x03}, // Q
    {0x7e, 0x33, 0x33, 0x3e, 0x36, 0x33, 0x63, 0x00}, // R
    {0x3e, 0x63, 0x38, 0x0e, 0x03, 0x63, 0x3e, 0x00}, // S
    {0x7f, 0x5d, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00}, // T
    {0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3e, 0x00}, // U
    {0x63, 0x63, 0x63, 0x63, 0x32, 0x1c, 0x08, 0x00}, // V
    {0x63, 0x63, 0x63, 0x6b, 0x7f, 0x77, 0x63, 0x00}, // W
    {0x63, 0x63, 0x34, 0x1c, 0x2c, 0x63, 0x63, 0x00}, // X
    {0x63, 0x63, 0x63, 0x3f, 0x03, 0x1f, 0x0e, 0x00}, // Y
    {0x7f, 0x46, 0x1c, 0x30, 0x61, 0x43, 0x7f, 0x00}, // Z
    {0x3c, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3c, 0x00}, // [
    {0xc0, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x03, 0x00}, // \ //
    {0x3c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x3c, 0x00}, // ]
    {0x18, 0x3c, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00}, // ^
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff}, // _
    {0x1c, 0x1c, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00}, // `
    {0x00, 0x00, 0x3e, 0x03, 0x3f, 0x63, 0x3f, 0x00}, // a
    {0x60, 0x60, 0x7e, 0x63, 0x63, 0x63, 0x7e, 0x00}, // b
    {0x00, 0x00, 0x3e, 0x60, 0x60, 0x63, 0x3e, 0x00}, // c
    {0x03, 0x03, 0x3f, 0x63, 0x63, 0x63, 0x3f, 0x00}, // d
    {0x00, 0x00, 0x3e, 0x63, 0x7f, 0x60, 0x3e, 0x00}, // e
    {0x1c, 0x36, 0x30, 0x7c, 0x30, 0x30, 0x30, 0x00}, // f
    {0x00, 0x00, 0x3f, 0x63, 0x63, 0x3f, 0x03, 0x3e}, // g
    {0x60, 0x60, 0x7e, 0x63, 0x63, 0x63, 0x63, 0x00}, // h
    {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3c, 0x00}, // i
    {0x06, 0x00, 0x0e, 0x06, 0x06, 0x06, 0x06, 0x3c}, // j
    {0x60, 0x60, 0x66, 0x6c, 0x78, 0x6c, 0x66, 0x00}, // k
    {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00}, // l
    {0x00, 0x00, 0x66, 0x7f, 0x6b, 0x63, 0x63, 0x00}, // m
    {0x00, 0x00, 0x7c, 0x62, 0x62, 0x62, 0x62, 0x00}, // n
    {0x00, 0x00, 0x3e, 0x63, 0x63, 0x63, 0x3e, 0x00}, // o
    {0x00, 0x00, 0x7e, 0x63, 0x63, 0x7e, 0x60, 0x60}, // p
    {0x00, 0x00, 0x3f, 0x63, 0x63, 0x3f, 0x03, 0x03}, // q
    {0x00, 0x00, 0x6e, 0x30, 0x30, 0x30, 0x30, 0x00}, // r
    {0x00, 0x00, 0x3e, 0x60, 0x3e, 0x03, 0x3e, 0x00}, // s
    {0x18, 0x18, 0x7e, 0x18, 0x18, 0x18, 0x0d, 0x00}, // t
    {0x00, 0x00, 0x63, 0x63, 0x63, 0x63, 0x3f, 0x00}, // u
    {0x00, 0x00, 0x63, 0x63, 0x63, 0x32, 0x1c, 0x08}, // v
    {0x00, 0x00, 0x63, 0x63, 0x6b, 0x7f, 0x36, 0x00}, // w
    {0x00, 0x00, 0x63, 0x34, 0x1c, 0x2c, 0x63, 0x00}, // x
    {0x00, 0x00, 0x63, 0x63, 0x63, 0x3f, 0x03, 0x3e}, // y
    {0x00, 0x00, 0x7f, 0x46, 0x1c, 0x30, 0x7f, 0x00}, // z
    {0x0c, 0x18, 0x18, 0x30, 0x18, 0x18, 0x0c, 0x00}, // {
    {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00}, // |
    {0x30, 0x18, 0x18, 0x0c, 0x18, 0x18, 0x30, 0x00}, // }
    {0x00, 0x00, 0x3b, 0x6e, 0x00, 0x00, 0x00, 0x00}, // ~
};

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
  size_t bytes_read = fread(data, 1, size, f);
  if (bytes_read < size) {
    free(data);
    fclose(f);
    return (string8){.str = NULL, .size = 0};
  }
  fclose(f);
  return (string8){.str = data, .size = size};
}

void font_init(string8 file, font_info *info) {
  memset(info, 0, sizeof(font_info));
  u32 cmap = 0;
  u32 num_tables = READ_BE16(file.str + 4);

  // Enumerate binary table directories
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

  // Resolve Segment Mapping format-12 entry point
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

  // Parse horizontal header typographic layout rules
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

/**
 * @brief Allocates dynamic trackers inside an active glyph working data block.
 * @param glyph Target reference to append capacity limits.
 */
static void _font_glyph_init(font_glyph *glyph) {
  memset(glyph, 0, sizeof(font_glyph));
  glyph->capacity = 8;
  glyph->flags = malloc(sizeof(font_point_flag) * glyph->capacity);
  glyph->points = malloc(sizeof(v2_i16) * glyph->capacity);
}

/**
 * @brief Safely pushes an outline vertex coordinate and flag state onto a glyph
 * structure.
 * @param glyph Active structure context reference.
 * @param flag Geometry tracking property bitmask state.
 * @param point Coordinates mapping design positions.
 */
static void _font_glyph_append(font_glyph *glyph, font_point_flag flag,
                               v2_i16 point) {
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
    return; // Ignores empty or composite layout variants safely

  glyph->x_min = READ_BE16(glyf + 2);
  glyph->y_min = READ_BE16(glyf + 4);
  glyph->x_max = READ_BE16(glyf + 6);
  glyph->y_max = READ_BE16(glyf + 8);

  u32 num_raw_points = READ_BE16(glyf + 10 + 2 * (num_contours - 1)) + 1;
  u8 *flags_raw = malloc(num_raw_points);
  v2_i16 *points_raw = malloc(sizeof(v2_i16) * num_raw_points);
  u16 num_instructions = READ_BE16(glyf + 10 + 2 * num_contours);
  u8 *point_data = glyf + 12 + 2 * num_contours + num_instructions;

  // Unpack compressed TTF flag layout vectors
  u32 num_flags = 0;
  while (num_flags < num_raw_points) {
    u8 flag = *(point_data++);
    flags_raw[num_flags++] = flag;
    if (flag & 0x08) { // Handles run-length compression repeats
      u8 count = *(point_data++);
      while (count--)
        flags_raw[num_flags++] = flag;
    }
  }

  // Unpack Relative X coordinate updates
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

  // Unpack Relative Y coordinate updates
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

  // Convert implicit vertex coordinates and implied midpoints into explicit
  // segments
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

  // Track across lines vertically to evaluate vector scanline crossings
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
      } else { // Evaluate quadratic Bézier scan intersections
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

    // Sort active scan intersections horizontally (Insertion Sort)
    for (u32 i = 1; i < num_intersects; i++) {
      f32 key = intersects[i];
      i32 j = i - 1;
      while (j >= 0 && intersects[j] > key) {
        intersects[j + 1] = intersects[j];
        j--;
      }
      intersects[j + 1] = key;
    }

    // Perform standard even-odd winding rules to fill pixels
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
      // Basic horizontal anti-aliasing edge blend estimation
      row[xs] = (u8)(255 * (1 - (x_start - xs)));
      row[xe] = (u8)(255 * (x_end - xe));
    }
  }
  free(points);
  free(intersects);
  return bitmap;
}

// Copyright 2014 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Library for converting WOFF2 format font files to their TTF versions.

#include "./woff2_dec.h"

#include <stdlib.h>
#include <algorithm>
#include <complex>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
#include <map>
#include <memory>

#include "./decode.h"
#include "./buffer.h"
#include "./port.h"
#include "./round.h"
#include "./store_bytes.h"
#include "./table_tags.h"
#include "./variable_length.h"
#include "./woff2_common.h"

namespace woff2 {

namespace {

using std::string;
using std::vector;


// simple glyph flags
const int kGlyfOnCurve = 1 << 0;
const int kGlyfXShort = 1 << 1;
const int kGlyfYShort = 1 << 2;
const int kGlyfRepeat = 1 << 3;
const int kGlyfThisXIsSame = 1 << 4;
const int kGlyfThisYIsSame = 1 << 5;

// composite glyph flags
// See CompositeGlyph.java in sfntly for full definitions
const int FLAG_ARG_1_AND_2_ARE_WORDS = 1 << 0;
const int FLAG_WE_HAVE_A_SCALE = 1 << 3;
const int FLAG_MORE_COMPONENTS = 1 << 5;
const int FLAG_WE_HAVE_AN_X_AND_Y_SCALE = 1 << 6;
const int FLAG_WE_HAVE_A_TWO_BY_TWO = 1 << 7;
const int FLAG_WE_HAVE_INSTRUCTIONS = 1 << 8;

const size_t kCheckSumAdjustmentOffset = 8;

const size_t kEndPtsOfContoursOffset = 10;
const size_t kCompositeGlyphBegin = 10;

// metadata for a TTC font entry
struct TtcFont {
  uint32_t flavor;
  uint32_t dst_offset;
  std::vector<uint16_t> table_indices;
};

int WithSign(int flag, int baseval) {
  // Precondition: 0 <= baseval < 65536 (to avoid integer overflow)
  return (flag & 1) ? baseval : -baseval;
}

bool TripletDecode(const uint8_t* flags_in, const uint8_t* in, size_t in_size,
    unsigned int n_points, Point* result, size_t* in_bytes_consumed) {
  int x = 0;
  int y = 0;

  if (PREDICT_FALSE(n_points > in_size)) {
    return FONT_COMPRESSION_FAILURE();
  }
  unsigned int triplet_index = 0;

  for (unsigned int i = 0; i < n_points; ++i) {
    uint8_t flag = flags_in[i];
    bool on_curve = !(flag >> 7);
    flag &= 0x7f;
    unsigned int n_data_bytes;
    if (flag < 84) {
      n_data_bytes = 1;
    } else if (flag < 120) {
      n_data_bytes = 2;
    } else if (flag < 124) {
      n_data_bytes = 3;
    } else {
      n_data_bytes = 4;
    }
    if (PREDICT_FALSE(triplet_index + n_data_bytes > in_size ||
        triplet_index + n_data_bytes < triplet_index)) {
      return FONT_COMPRESSION_FAILURE();
    }
    int dx, dy;
    if (flag < 10) {
      dx = 0;
      dy = WithSign(flag, ((flag & 14) << 7) + in[triplet_index]);
    } else if (flag < 20) {
      dx = WithSign(flag, (((flag - 10) & 14) << 7) + in[triplet_index]);
      dy = 0;
    } else if (flag < 84) {
      int b0 = flag - 20;
      int b1 = in[triplet_index];
      dx = WithSign(flag, 1 + (b0 & 0x30) + (b1 >> 4));
      dy = WithSign(flag >> 1, 1 + ((b0 & 0x0c) << 2) + (b1 & 0x0f));
    } else if (flag < 120) {
      int b0 = flag - 84;
      dx = WithSign(flag, 1 + ((b0 / 12) << 8) + in[triplet_index]);
      dy = WithSign(flag >> 1,
                    1 + (((b0 % 12) >> 2) << 8) + in[triplet_index + 1]);
    } else if (flag < 124) {
      int b2 = in[triplet_index + 1];
      dx = WithSign(flag, (in[triplet_index] << 4) + (b2 >> 4));
      dy = WithSign(flag >> 1, ((b2 & 0x0f) << 8) + in[triplet_index + 2]);
    } else {
      dx = WithSign(flag, (in[triplet_index] << 8) + in[triplet_index + 1]);
      dy = WithSign(flag >> 1,
          (in[triplet_index + 2] << 8) + in[triplet_index + 3]);
    }
    triplet_index += n_data_bytes;
    // Possible overflow but coordinate values are not security sensitive
    x += dx;
    y += dy;
    *result++ = {x, y, on_curve};
  }
  *in_bytes_consumed = triplet_index;
  return true;
}

// This function stores just the point data. On entry, dst points to the
// beginning of a simple glyph. Returns true on success.
bool StorePoints(unsigned int n_points, const Point* points,
    unsigned int n_contours, unsigned int instruction_length,
    uint8_t* dst, size_t dst_size, size_t* glyph_size) {
  // I believe that n_contours < 65536, in which case this is safe. However, a
  // comment and/or an assert would be good.
  unsigned int flag_offset = kEndPtsOfContoursOffset + 2 * n_contours + 2 +
    instruction_length;
  int last_flag = -1;
  int repeat_count = 0;
  int last_x = 0;
  int last_y = 0;
  unsigned int x_bytes = 0;
  unsigned int y_bytes = 0;

  for (unsigned int i = 0; i < n_points; ++i) {
    const Point& point = points[i];
    int flag = point.on_curve ? kGlyfOnCurve : 0;
    int dx = point.x - last_x;
    int dy = point.y - last_y;
    if (dx == 0) {
      flag |= kGlyfThisXIsSame;
    } else if (dx > -256 && dx < 256) {
      flag |= kGlyfXShort | (dx > 0 ? kGlyfThisXIsSame : 0);
      x_bytes += 1;
    } else {
      x_bytes += 2;
    }
    if (dy == 0) {
      flag |= kGlyfThisYIsSame;
    } else if (dy > -256 && dy < 256) {
      flag |= kGlyfYShort | (dy > 0 ? kGlyfThisYIsSame : 0);
      y_bytes += 1;
    } else {
      y_bytes += 2;
    }

    if (flag == last_flag && repeat_count != 255) {
      dst[flag_offset - 1] |= kGlyfRepeat;
      repeat_count++;
    } else {
      if (repeat_count != 0) {
        if (PREDICT_FALSE(flag_offset >= dst_size)) {
          return FONT_COMPRESSION_FAILURE();
        }
        dst[flag_offset++] = repeat_count;
      }
      if (PREDICT_FALSE(flag_offset >= dst_size)) {
        return FONT_COMPRESSION_FAILURE();
      }
      dst[flag_offset++] = flag;
      repeat_count = 0;
    }
    last_x = point.x;
    last_y = point.y;
    last_flag = flag;
  }

  if (repeat_count != 0) {
    if (PREDICT_FALSE(flag_offset >= dst_size)) {
      return FONT_COMPRESSION_FAILURE();
    }
    dst[flag_offset++] = repeat_count;
  }
  unsigned int xy_bytes = x_bytes + y_bytes;
  if (PREDICT_FALSE(xy_bytes < x_bytes ||
      flag_offset + xy_bytes < flag_offset ||
      flag_offset + xy_bytes > dst_size)) {
    return FONT_COMPRESSION_FAILURE();
  }

  int x_offset = flag_offset;
  int y_offset = flag_offset + x_bytes;
  last_x = 0;
  last_y = 0;
  for (unsigned int i = 0; i < n_points; ++i) {
    int dx = points[i].x - last_x;
    if (dx == 0) {
      // pass
    } else if (dx > -256 && dx < 256) {
      dst[x_offset++] = std::abs(dx);
    } else {
      // will always fit for valid input, but overflow is harmless
      x_offset = Store16(dst, x_offset, dx);
    }
    last_x += dx;
    int dy = points[i].y - last_y;
    if (dy == 0) {
      // pass
    } else if (dy > -256 && dy < 256) {
      dst[y_offset++] = std::abs(dy);
    } else {
      y_offset = Store16(dst, y_offset, dy);
    }
    last_y += dy;
  }
  *glyph_size = y_offset;
  return true;
}

// Compute the bounding box of the coordinates, and store into a glyf buffer.
// A precondition is that there are at least 10 bytes available.
void ComputeBbox(unsigned int n_points, const Point* points, uint8_t* dst) {
  int x_min = 0;
  int y_min = 0;
  int x_max = 0;
  int y_max = 0;

  if (n_points > 0) {
    x_min = points[0].x;
    x_max = points[0].x;
    y_min = points[0].y;
    y_max = points[0].y;
  }
  for (unsigned int i = 1; i < n_points; ++i) {
    int x = points[i].x;
    int y = points[i].y;
    x_min = std::min(x, x_min);
    x_max = std::max(x, x_max);
    y_min = std::min(y, y_min);
    y_max = std::max(y, y_max);
  }
  size_t offset = 2;
  offset = Store16(dst, offset, x_min);
  offset = Store16(dst, offset, y_min);
  offset = Store16(dst, offset, x_max);
  offset = Store16(dst, offset, y_max);
}

bool ProcessComposite(Buffer* composite_stream, uint8_t* dst,
    size_t dst_size, size_t* glyph_size, bool* have_instructions) {
  size_t start_offset = composite_stream->offset();
  bool we_have_instructions = false;

  uint16_t flags = FLAG_MORE_COMPONENTS;
  while (flags & FLAG_MORE_COMPONENTS) {
    if (PREDICT_FALSE(!composite_stream->ReadU16(&flags))) {
      return FONT_COMPRESSION_FAILURE();
    }
    we_have_instructions |= (flags & FLAG_WE_HAVE_INSTRUCTIONS) != 0;
    size_t arg_size = 2;  // glyph index
    if (flags & FLAG_ARG_1_AND_2_ARE_WORDS) {
      arg_size += 4;
    } else {
      arg_size += 2;
    }
    if (flags & FLAG_WE_HAVE_A_SCALE) {
      arg_size += 2;
    } else if (flags & FLAG_WE_HAVE_AN_X_AND_Y_SCALE) {
      arg_size += 4;
    } else if (flags & FLAG_WE_HAVE_A_TWO_BY_TWO) {
      arg_size += 8;
    }
    if (PREDICT_FALSE(!composite_stream->Skip(arg_size))) {
      return FONT_COMPRESSION_FAILURE();
    }
  }
  size_t composite_glyph_size = composite_stream->offset() - start_offset;
  if (PREDICT_FALSE(composite_glyph_size + kCompositeGlyphBegin > dst_size)) {
    return FONT_COMPRESSION_FAILURE();
  }
  Store16(dst, 0, 0xffff);  // nContours = -1 for composite glyph
  std::memcpy(dst + kCompositeGlyphBegin,
      composite_stream->buffer() + start_offset,
      composite_glyph_size);
  *glyph_size = kCompositeGlyphBegin + composite_glyph_size;
  *have_instructions = we_have_instructions;
  return true;
}

// Build TrueType loca table
bool StoreLoca(const std::vector<uint32_t>& loca_values, int index_format,
               uint8_t* dst, size_t dst_size) {
  const uint64_t loca_size = loca_values.size();
  const uint64_t offset_size = index_format ? 4 : 2;
  if (PREDICT_FALSE((loca_size << 2) >> 2 != loca_size)) {
    return FONT_COMPRESSION_FAILURE();
  }
  if (PREDICT_FALSE(offset_size * loca_size > dst_size)) {
    return FONT_COMPRESSION_FAILURE();
  }
  size_t offset = 0;
  for (size_t i = 0; i < loca_values.size(); ++i) {
    uint32_t value = loca_values[i];
    if (index_format) {
      offset = StoreU32(dst, offset, value);
    } else {
      offset = Store16(dst, offset, value >> 1);
    }
  }
  return true;
}

// Reconstruct entire glyf table based on transformed original
bool ReconstructGlyf(const uint8_t* data, size_t data_size,
                     uint8_t* dst, size_t dst_size,
                     uint8_t* loca_buf, size_t loca_size) {
  static const int kNumSubStreams = 7;
  Buffer file(data, data_size);
  uint32_t version;
  std::vector<std::pair<const uint8_t*, size_t> > substreams(kNumSubStreams);

  if (PREDICT_FALSE(!file.ReadU32(&version))) {
    return FONT_COMPRESSION_FAILURE();
  }
  uint16_t num_glyphs;
  uint16_t index_format;
  if (PREDICT_FALSE(!file.ReadU16(&num_glyphs) ||
      !file.ReadU16(&index_format))) {
    return FONT_COMPRESSION_FAILURE();
  }

  unsigned int offset = (2 + kNumSubStreams) * 4;
  if (PREDICT_FALSE(offset > data_size)) {
    return FONT_COMPRESSION_FAILURE();
  }
  // Invariant from here on: data_size >= offset
  for (int i = 0; i < kNumSubStreams; ++i) {
    uint32_t substream_size;
    if (PREDICT_FALSE(!file.ReadU32(&substream_size))) {
      return FONT_COMPRESSION_FAILURE();
    }
    if (PREDICT_FALSE(substream_size > data_size - offset)) {
      return FONT_COMPRESSION_FAILURE();
    }
    substreams[i] = std::make_pair(data + offset, substream_size);
    offset += substream_size;
  }
  Buffer n_contour_stream(substreams[0].first, substreams[0].second);
  Buffer n_points_stream(substreams[1].first, substreams[1].second);
  Buffer flag_stream(substreams[2].first, substreams[2].second);
  Buffer glyph_stream(substreams[3].first, substreams[3].second);
  Buffer composite_stream(substreams[4].first, substreams[4].second);
  Buffer bbox_stream(substreams[5].first, substreams[5].second);
  Buffer instruction_stream(substreams[6].first, substreams[6].second);

  std::vector<uint32_t> loca_values(num_glyphs + 1);
  std::vector<unsigned int> n_points_vec;
  std::unique_ptr<Point[]> points;
  size_t points_size = 0;
  uint32_t loca_offset = 0;
  const uint8_t* bbox_bitmap = bbox_stream.buffer();
  // Safe because num_glyphs is bounded
  unsigned int bitmap_length = ((num_glyphs + 31) >> 5) << 2;
  if (!bbox_stream.Skip(bitmap_length)) {
    return FONT_COMPRESSION_FAILURE();
  }
  for (unsigned int i = 0; i < num_glyphs; ++i) {
    size_t glyph_size = 0;
    uint16_t n_contours = 0;
    bool have_bbox = false;
    if (bbox_bitmap[i >> 3] & (0x80 >> (i & 7))) {
      have_bbox = true;
    }
    if (PREDICT_FALSE(!n_contour_stream.ReadU16(&n_contours))) {
      return FONT_COMPRESSION_FAILURE();
    }
    uint8_t* glyf_dst = dst + loca_offset;
    size_t glyf_dst_size = dst_size - loca_offset;

    if (n_contours == 0xffff) {
      // composite glyph
      bool have_instructions = false;
      unsigned int instruction_size = 0;
      if (PREDICT_FALSE(!have_bbox)) {
        // composite glyphs must have an explicit bbox
        return FONT_COMPRESSION_FAILURE();
      }
      if (PREDICT_FALSE(!ProcessComposite(&composite_stream, glyf_dst,
            glyf_dst_size, &glyph_size, &have_instructions))) {
        return FONT_COMPRESSION_FAILURE();
      }
      if (PREDICT_FALSE(!bbox_stream.Read(glyf_dst + 2, 8))) {
        return FONT_COMPRESSION_FAILURE();
      }
      if (have_instructions) {
        if (PREDICT_FALSE(!Read255UShort(&glyph_stream, &instruction_size))) {
          return FONT_COMPRESSION_FAILURE();
        }
        if (PREDICT_FALSE(instruction_size + 2 > glyf_dst_size - glyph_size)) {
          return FONT_COMPRESSION_FAILURE();
        }
        Store16(glyf_dst, glyph_size, instruction_size);
        if (PREDICT_FALSE(!instruction_stream.Read(glyf_dst + glyph_size + 2,
              instruction_size))) {
          return FONT_COMPRESSION_FAILURE();
        }
        glyph_size += instruction_size + 2;
      }
    } else if (n_contours > 0) {
      // simple glyph
      n_points_vec.clear();
      unsigned int total_n_points = 0;
      unsigned int n_points_contour;
      for (unsigned int j = 0; j < n_contours; ++j) {
        if (PREDICT_FALSE(
            !Read255UShort(&n_points_stream, &n_points_contour))) {
          return FONT_COMPRESSION_FAILURE();
        }
        n_points_vec.push_back(n_points_contour);
        if (PREDICT_FALSE(total_n_points + n_points_contour < total_n_points)) {
          return FONT_COMPRESSION_FAILURE();
        }
        total_n_points += n_points_contour;
      }
      unsigned int flag_size = total_n_points;
      if (PREDICT_FALSE(
          flag_size > flag_stream.length() - flag_stream.offset())) {
        return FONT_COMPRESSION_FAILURE();
      }
      const uint8_t* flags_buf = flag_stream.buffer() + flag_stream.offset();
      const uint8_t* triplet_buf = glyph_stream.buffer() +
        glyph_stream.offset();
      size_t triplet_size = glyph_stream.length() - glyph_stream.offset();
      size_t triplet_bytes_consumed = 0;
      if (points_size < total_n_points) {
        points_size = total_n_points;
        points.reset(new Point[points_size]);
      }
      if (PREDICT_FALSE(!TripletDecode(flags_buf, triplet_buf, triplet_size,
          total_n_points, points.get(), &triplet_bytes_consumed))) {
        return FONT_COMPRESSION_FAILURE();
      }
      const uint32_t header_and_endpts_contours_size =
          kEndPtsOfContoursOffset + 2 * n_contours;
      if (PREDICT_FALSE(glyf_dst_size < header_and_endpts_contours_size)) {
        return FONT_COMPRESSION_FAILURE();
      }
      Store16(glyf_dst, 0, n_contours);
      if (have_bbox) {
        if (PREDICT_FALSE(!bbox_stream.Read(glyf_dst + 2, 8))) {
          return FONT_COMPRESSION_FAILURE();
        }
      } else {
        ComputeBbox(total_n_points, points.get(), glyf_dst);
      }
      size_t offset = kEndPtsOfContoursOffset;
      int end_point = -1;
      for (unsigned int contour_ix = 0; contour_ix < n_contours; ++contour_ix) {
        end_point += n_points_vec[contour_ix];
        if (PREDICT_FALSE(end_point >= 65536)) {
          return FONT_COMPRESSION_FAILURE();
        }
        offset = Store16(glyf_dst, offset, end_point);
      }
      if (PREDICT_FALSE(!flag_stream.Skip(flag_size))) {
        return FONT_COMPRESSION_FAILURE();
      }
      if (PREDICT_FALSE(!glyph_stream.Skip(triplet_bytes_consumed))) {
        return FONT_COMPRESSION_FAILURE();
      }
      unsigned int instruction_size;
      if (PREDICT_FALSE(!Read255UShort(&glyph_stream, &instruction_size))) {
        return FONT_COMPRESSION_FAILURE();
      }
      if (PREDICT_FALSE(glyf_dst_size - header_and_endpts_contours_size <
          instruction_size + 2)) {
        return FONT_COMPRESSION_FAILURE();
      }
      uint8_t* instruction_dst = glyf_dst + header_and_endpts_contours_size;
      Store16(instruction_dst, 0, instruction_size);
      if (PREDICT_FALSE(
          !instruction_stream.Read(instruction_dst + 2, instruction_size))) {
        return FONT_COMPRESSION_FAILURE();
      }
      if (PREDICT_FALSE(!StorePoints(total_n_points, points.get(), n_contours,
            instruction_size, glyf_dst, glyf_dst_size, &glyph_size))) {
        return FONT_COMPRESSION_FAILURE();
      }
    } else {
      glyph_size = 0;
    }
    loca_values[i] = loca_offset;
    if (PREDICT_FALSE(glyph_size + 3 < glyph_size)) {
      return FONT_COMPRESSION_FAILURE();
    }
    glyph_size = Round4(glyph_size);
    if (PREDICT_FALSE(glyph_size > dst_size - loca_offset)) {
      // This shouldn't happen, but this test defensively maintains the
      // invariant that loca_offset <= dst_size.
      return FONT_COMPRESSION_FAILURE();
    }
    loca_offset += glyph_size;
  }
  loca_values[num_glyphs] = loca_offset;
  return StoreLoca(loca_values, index_format, loca_buf, loca_size);
}

// This is linear search, but could be changed to binary because we
// do have a guarantee that the tables are sorted by tag. But the total
// cpu time is expected to be very small in any case.
const Table* FindTable(const std::vector<Table>& tables, uint32_t tag) {
  size_t n_tables = tables.size();
  for (size_t i = 0; i < n_tables; ++i) {
    if (tables[i].tag == tag) {
      return &tables[i];
    }
  }
  return NULL;
}

// https://www.microsoft.com/typography/otspec/maxp.htm
bool ReadNumGlyphs(const Table* maxp_table,
                   uint8_t* dst, size_t dst_length, uint16_t* num_glyphs) {
  if (PREDICT_FALSE(static_cast<uint64_t>(maxp_table->dst_offset +
      maxp_table->dst_length) > dst_length)) {
    return FONT_COMPRESSION_FAILURE();
  }
  Buffer buffer(dst + maxp_table->dst_offset, maxp_table->dst_length);
  // Skip 4 to reach 'maxp' numGlyphs
  if (PREDICT_FALSE(!buffer.Skip(4) || !buffer.ReadU16(num_glyphs))) {
    return FONT_COMPRESSION_FAILURE();
  }
  return true;
}

// Get numberOfHMetrics, https://www.microsoft.com/typography/otspec/hhea.htm
bool ReadNumHMetrics(const Table* hhea_table,
                     uint8_t* dst, size_t dst_length, uint16_t* num_hmetrics) {
  if (PREDICT_FALSE(static_cast<uint64_t>(hhea_table->dst_offset +
      hhea_table->dst_length) > dst_length)) {
    return FONT_COMPRESSION_FAILURE();
  }
  // Skip 34 to reach 'hhea' numberOfHMetrics
  Buffer buffer(dst + hhea_table->dst_offset, hhea_table->dst_length);
  if (PREDICT_FALSE(!buffer.Skip(34) || !buffer.ReadU16(num_hmetrics))) {
    return FONT_COMPRESSION_FAILURE();
  }
  return true;
}

// x_min for glyph; https://www.microsoft.com/typography/otspec/glyf.htm
bool ReadGlyphXMin(Buffer* glyf_buff, Buffer* loca_buff, int16_t loca_format,
                   uint16_t index, int16_t* x_min) {
  uint32_t offset1, offset2;
  loca_buff->set_offset((loca_format == 0 ? 2 : 4) * index);
  if (loca_format == 0) {
    uint16_t tmp1, tmp2;
    if (PREDICT_FALSE(!loca_buff->ReadU16(&tmp1) ||
                      !loca_buff->ReadU16(&tmp2))) {
      return FONT_COMPRESSION_FAILURE();
    }
    // https://www.microsoft.com/typography/otspec/loca.htm
    // "The actual local offset divided by 2 is stored."
    offset1 = tmp1 * 2;
    offset2 = tmp2 * 2;
  } else if (PREDICT_FALSE(!loca_buff->ReadU32(&offset1) ||
                           !loca_buff->ReadU32(&offset2))) {
    return FONT_COMPRESSION_FAILURE();
  }

  if (offset1 != offset2) {
    glyf_buff->set_offset(offset1 + 2);
    if (!glyf_buff->ReadS16(x_min)) {
      return FONT_COMPRESSION_FAILURE();
    }
  } else {
    *x_min = 0;
  }
  return true;
}

// http://dev.w3.org/webfonts/WOFF2/spec/Overview.html#hmtx_table_format
bool ReconstructTransformedHmtx(const uint8_t* transformed_buf,
                                size_t transformed_size,
                                const Table* glyf_table,
                                const Table* hhea_table,
                                const Table* hmtx_table,
                                const Table* loca_table,
                                const Table* maxp_table,
                                uint8_t* dst, size_t dst_length) {
  if (PREDICT_FALSE(!glyf_table)) {
    return FONT_COMPRESSION_FAILURE();
  }
  if (PREDICT_FALSE(!hhea_table)) {
    return FONT_COMPRESSION_FAILURE();
  }
  if (PREDICT_FALSE(!hmtx_table)) {
    return FONT_COMPRESSION_FAILURE();
  }
  if (PREDICT_FALSE(!loca_table)) {
    return FONT_COMPRESSION_FAILURE();
  }
  if (PREDICT_FALSE(!maxp_table)) {
    return FONT_COMPRESSION_FAILURE();
  }

  uint16_t num_glyphs, num_hmetrics;
  if (!ReadNumGlyphs(maxp_table, dst, dst_length, &num_glyphs)) {
    return FONT_COMPRESSION_FAILURE();
  }
  if (!ReadNumHMetrics(hhea_table, dst, dst_length, &num_hmetrics)) {
    return FONT_COMPRESSION_FAILURE();
  }

  if (PREDICT_FALSE(static_cast<uint64_t>(hmtx_table->dst_offset +
      hmtx_table->dst_length) > dst_length)) {
    return FONT_COMPRESSION_FAILURE();
  }
  Buffer hmtx_buff_in(transformed_buf, transformed_size);

  uint8_t hmtx_flags;
  if (PREDICT_FALSE(!hmtx_buff_in.ReadU8(&hmtx_flags))) {
    return FONT_COMPRESSION_FAILURE();
  }

  std::vector<uint16_t> advance_widths;
  std::vector<int16_t> lsbs;
  bool has_proportional_lsbs = (hmtx_flags & 1) == 0;
  bool has_monospace_lsbs = (hmtx_flags & 2) == 0;

  // you say you transformed but there is little evidence of it
  if (has_proportional_lsbs && has_monospace_lsbs) {
    return FONT_COMPRESSION_FAILURE();
  }

  // glyf/loca are done already so we can use them to recover x_min's
  if (PREDICT_FALSE(static_cast<uint64_t>(glyf_table->dst_offset +
      glyf_table->dst_length) > dst_length)) {
    return FONT_COMPRESSION_FAILURE();
  }
  if (PREDICT_FALSE(static_cast<uint64_t>(loca_table->dst_offset +
      loca_table->dst_length) > dst_length)) {
    return FONT_COMPRESSION_FAILURE();
  }
  Buffer glyf_buff(dst + glyf_table->dst_offset, glyf_table->dst_length);
  Buffer loca_buff(dst + loca_table->dst_offset, loca_table->dst_length);
  int16_t loca_format;
  if (loca_table->dst_length == 2 * (num_glyphs + 1)) {
    loca_format = 0;
  } else if (loca_table->dst_length == 4 * (num_glyphs + 1)) {
    loca_format = 1;
  } else {
    return FONT_COMPRESSION_FAILURE();
  }

  for (uint16_t i = 0; i < num_hmetrics; i++) {
    uint16_t advance_width;
    if (PREDICT_FALSE(!hmtx_buff_in.ReadU16(&advance_width))) {
      return FONT_COMPRESSION_FAILURE();
    }
    advance_widths.push_back(advance_width);
  }

  for (uint16_t i = 0; i < num_hmetrics; i++) {
    int16_t lsb;
    if (has_proportional_lsbs) {
      if (PREDICT_FALSE(!hmtx_buff_in.ReadS16(&lsb))) {
        return FONT_COMPRESSION_FAILURE();
      }
    } else {
      if (PREDICT_FALSE(!ReadGlyphXMin(&glyf_buff, &loca_buff, loca_format, i,
          &lsb))) {
        return FONT_COMPRESSION_FAILURE();
      }
    }
    lsbs.push_back(lsb);
  }

  for (uint16_t i = num_hmetrics; i < num_glyphs; i++) {
    int16_t lsb;
    if (has_monospace_lsbs) {
      if (PREDICT_FALSE(!hmtx_buff_in.ReadS16(&lsb))) {
        return FONT_COMPRESSION_FAILURE();
      }
    } else {
      if (PREDICT_FALSE(!ReadGlyphXMin(&glyf_buff, &loca_buff, loca_format, i,
          &lsb))) {
        return FONT_COMPRESSION_FAILURE();
      }
    }
    lsbs.push_back(lsb);
  }

  // bake me a shiny new hmtx table
  uint32_t hmtx_output_size = 2 * num_glyphs + 2 * num_hmetrics;
  if (hmtx_output_size > hmtx_table->dst_length) {
    return FONT_COMPRESSION_FAILURE();
  }
  if (PREDICT_FALSE(static_cast<uint64_t>(hmtx_table->dst_offset +
      hmtx_table->dst_length) > dst_length)) {
    return FONT_COMPRESSION_FAILURE();
  }

  size_t dst_offset = hmtx_table->dst_offset;
  for (uint32_t i = 0; i < num_glyphs; i++) {
    if (i < num_hmetrics) {
      Store16(advance_widths[i], &dst_offset, dst);
    }
    Store16(lsbs[i], &dst_offset, dst);
  }

  return true;
}

bool ReconstructTransformedGlyf(const uint8_t* transformed_buf,
    size_t transformed_size, const Table* glyf_table, const Table* loca_table,
    uint8_t* dst, size_t dst_length) {
  if (PREDICT_FALSE(glyf_table == NULL || loca_table == NULL)) {
    return FONT_COMPRESSION_FAILURE();
  }
  if (PREDICT_FALSE(static_cast<uint64_t>(glyf_table->dst_offset +
      glyf_table->dst_length) > dst_length)) {
    return FONT_COMPRESSION_FAILURE();
  }
  if (PREDICT_FALSE(static_cast<uint64_t>(loca_table->dst_offset +
     loca_table->dst_length) > dst_length)) {
    return FONT_COMPRESSION_FAILURE();
  }
  return ReconstructGlyf(transformed_buf, transformed_size,
                         dst + glyf_table->dst_offset, glyf_table->dst_length,
                         dst + loca_table->dst_offset, loca_table->dst_length);
}

// Reconstructs a single font. tables must not contain duplicates.
bool ReconstructTransformed(const std::vector<Table>& tables, uint32_t tag,
    const uint8_t* transformed_buf, size_t transformed_size,
    uint8_t* dst, size_t dst_length) {

  if (tag == kGlyfTableTag) {
    const Table* glyf_table = FindTable(tables, tag);
    const Table* loca_table = FindTable(tables, kLocaTableTag);
    return ReconstructTransformedGlyf(transformed_buf, transformed_size,
                                      glyf_table, loca_table, dst, dst_length);
  } else if (tag == kLocaTableTag) {
    // processing was already done by glyf table, but validate
    if (PREDICT_FALSE(!FindTable(tables, kGlyfTableTag))) {
      return FONT_COMPRESSION_FAILURE();
    }
  } else if (tag == kHmtxTableTag) {
    // Tables are sorted for a non-collection.
    // glyf is before hmtx and includes loca so all our accesses are safe.
    const Table* glyf_table = FindTable(tables, kGlyfTableTag);
    const Table* hhea_table = FindTable(tables, kHheaTableTag);
    const Table* hmtx_table = FindTable(tables, kHmtxTableTag);
    const Table* loca_table = FindTable(tables, kLocaTableTag);
    const Table* maxp_table = FindTable(tables, kMaxpTableTag);

    if (PREDICT_FALSE(!ReconstructTransformedHmtx(
        transformed_buf, transformed_size, glyf_table, hhea_table,
        hmtx_table, loca_table, maxp_table, dst, dst_length))) {
      return FONT_COMPRESSION_FAILURE();
    }
  } else {
    // transform for the tag is not known
    return FONT_COMPRESSION_FAILURE();
  }
  return true;
}

uint32_t ComputeChecksum(const Table* table, const uint8_t* dst) {
  return ComputeULongSum(dst + table->dst_offset, table->dst_length);
}

const Table* FindTable(TtcFont ttc_font, const std::vector<Table>& tables,
  uint32_t tag) {
  for (const auto i : ttc_font.table_indices) {
    if (tables[i].tag == tag) return &tables[i];
  }
  return NULL;
}

bool FixCollectionChecksums(size_t header_version,
  const std::vector<Table>& tables, const std::vector<TtcFont>& ttc_fonts,
  uint8_t* dst) {
  size_t offset = CollectionHeaderSize(header_version, ttc_fonts.size());

  for (const auto& ttc_font : ttc_fonts) {
    offset += 12;  // move to start of Offset Table
    const std::vector<uint16_t>& table_indices = ttc_font.table_indices;

    const Table* head_table = FindTable(ttc_font, tables, kHeadTableTag);
    if (PREDICT_FALSE(head_table == NULL ||
        head_table->dst_length < kCheckSumAdjustmentOffset + 4)) {
      return FONT_COMPRESSION_FAILURE();
    }

    size_t first_table_offset = std::numeric_limits<size_t>::max();
    for (const auto index : table_indices) {
      const auto& table = tables[index];
      if (table.dst_offset < first_table_offset) {
        first_table_offset = table.dst_offset;
      }
    }

    size_t adjustment_offset = head_table->dst_offset
      + kCheckSumAdjustmentOffset;
    StoreU32(dst, adjustment_offset, 0);

    uint32_t file_checksum = 0;
    // compute each tables checksum
    for (size_t i = 0; i < table_indices.size(); i++) {
      const Table& table = tables[table_indices[i]];
      uint32_t table_checksum = ComputeChecksum(&table, dst);
      size_t checksum_offset = offset + 4;  // skip past tag to checkSum

      // write the checksum for the Table Record
      StoreU32(dst, checksum_offset, table_checksum);
      file_checksum += table_checksum;
      // next Table Record
      offset += 16;
    }

    size_t header_size = kSfntHeaderSize +
      kSfntEntrySize * table_indices.size();
    uint32_t header_checksum = ComputeULongSum(dst + ttc_font.dst_offset,
                                               header_size);

    file_checksum += header_checksum;
    uint32_t checksum_adjustment = 0xb1b0afba - file_checksum;
    StoreU32(dst, adjustment_offset, checksum_adjustment);
  }

  return true;
}

bool FixChecksums(const std::vector<Table>& tables, uint8_t* dst) {
  const Table* head_table = FindTable(tables, kHeadTableTag);
  if (PREDICT_FALSE(head_table == NULL ||
      head_table->dst_length < kCheckSumAdjustmentOffset + 4)) {
    return FONT_COMPRESSION_FAILURE();
  }
  size_t adjustment_offset = head_table->dst_offset + kCheckSumAdjustmentOffset;
  StoreU32(dst, adjustment_offset, 0);
  size_t n_tables = tables.size();
  uint32_t file_checksum = 0;
  for (size_t i = 0; i < n_tables; ++i) {
    uint32_t checksum = ComputeChecksum(&tables[i], dst);
    StoreU32(dst, kSfntHeaderSize + i * kSfntEntrySize + 4, checksum);
    file_checksum += checksum;
  }
  file_checksum += ComputeULongSum(dst,
                                   kSfntHeaderSize + kSfntEntrySize * n_tables);
  uint32_t checksum_adjustment = 0xb1b0afba - file_checksum;
  StoreU32(dst, adjustment_offset, checksum_adjustment);
  return true;
}

bool Woff2Uncompress(uint8_t* dst_buf, size_t dst_size,
  const uint8_t* src_buf, size_t src_size) {
  size_t uncompressed_size = dst_size;
  int ok = BrotliDecompressBuffer(src_size, src_buf,
                                  &uncompressed_size, dst_buf);
  if (PREDICT_FALSE(!ok || uncompressed_size != dst_size)) {
    return FONT_COMPRESSION_FAILURE();
  }
  return true;
}

bool ReadTableDirectory(Buffer* file, std::vector<Table>* tables,
    size_t num_tables) {
  for (size_t i = 0; i < num_tables; ++i) {
    Table* table = &(*tables)[i];
    uint8_t flag_byte;
    if (PREDICT_FALSE(!file->ReadU8(&flag_byte))) {
      return FONT_COMPRESSION_FAILURE();
    }
    uint32_t tag;
    if ((flag_byte & 0x3f) == 0x3f) {
      if (PREDICT_FALSE(!file->ReadU32(&tag))) {
        return FONT_COMPRESSION_FAILURE();
      }
    } else {
      tag = kKnownTags[flag_byte & 0x3f];
    }
    uint32_t flags = 0;
    uint8_t xform_version = (flag_byte >> 6) & 0x03;

    // 0 means xform for glyph/loca, non-0 for others
    if (tag == kGlyfTableTag || tag == kLocaTableTag) {
      if (xform_version == 0) {
        flags |= kWoff2FlagsTransform;
      }
    } else if (xform_version != 0) {
      flags |= kWoff2FlagsTransform;
    }
    flags |= xform_version;

    uint32_t dst_length;
    if (PREDICT_FALSE(!ReadBase128(file, &dst_length))) {
      return FONT_COMPRESSION_FAILURE();
    }
    uint32_t transform_length = dst_length;
    if ((flags & kWoff2FlagsTransform) != 0) {
      if (PREDICT_FALSE(!ReadBase128(file, &transform_length))) {
        return FONT_COMPRESSION_FAILURE();
      }
      if (PREDICT_FALSE(tag == kLocaTableTag && transform_length)) {
        return FONT_COMPRESSION_FAILURE();
      }
    }
    table->tag = tag;
    table->flags = flags;
    table->transform_length = transform_length;
    table->dst_length = dst_length;
  }
  return true;
}

}  // namespace

size_t ComputeWOFF2FinalSize(const uint8_t* data, size_t length) {
  Buffer file(data, length);
  uint32_t total_length;

  if (!file.Skip(16) ||
      !file.ReadU32(&total_length)) {
    return 0;
  }
  return total_length;
}

// Writes a single Offset Table entry
size_t StoreOffsetTable(uint8_t* result, size_t offset, uint32_t flavor,
                        uint16_t num_tables) {
  offset = StoreU32(result, offset, flavor);  // sfnt version
  offset = Store16(result, offset, num_tables);  // num_tables
  unsigned max_pow2 = 0;
  while (1u << (max_pow2 + 1) <= num_tables) {
    max_pow2++;
  }
  const uint16_t output_search_range = (1u << max_pow2) << 4;
  offset = Store16(result, offset, output_search_range);  // searchRange
  offset = Store16(result, offset, max_pow2);  // entrySelector
  // rangeShift
  offset = Store16(result, offset, (num_tables << 4) - output_search_range);
  return offset;
}

size_t StoreTableEntry(uint8_t* result, const Table& table, size_t offset) {
  offset = StoreU32(result, offset, table.tag);
  offset = StoreU32(result, offset, 0);  // checksum, to fill in later
  offset = StoreU32(result, offset, table.dst_offset);
  offset = StoreU32(result, offset, table.dst_length);
  return offset;
}

// First table goes after all the headers, table directory, etc
uint64_t ComputeOffsetToFirstTable(const uint32_t header_version,
                                   const uint16_t num_tables,
                                   const std::vector<TtcFont>& ttc_fonts) {
  uint64_t offset = kSfntHeaderSize +
    kSfntEntrySize * static_cast<uint64_t>(num_tables);
  if (header_version) {
    offset = CollectionHeaderSize(header_version, ttc_fonts.size())
      + kSfntHeaderSize * ttc_fonts.size();
    for (const auto& ttc_font : ttc_fonts) {
      offset +=
        kSfntEntrySize * ttc_font.table_indices.size();
    }
  }
  return offset;
}

bool ReconstructTransformedFont(const std::vector<Table>& tables,
                                std::map<uint32_t, const uint8_t*>* src_by_dest,
                                uint8_t* result, size_t result_length) {
  for (const auto& table : tables) {
    if ((table.flags & kWoff2FlagsTransform) != kWoff2FlagsTransform) {
      continue;
    }
    const auto it = src_by_dest->find(table.dst_offset);
    if (it == src_by_dest->end()) {
      continue;
    }
    const uint8_t* transform_buf = (*it).second;
    src_by_dest->erase(table.dst_offset);

    size_t transform_length = table.transform_length;
    if (PREDICT_FALSE(!ReconstructTransformed(tables, table.tag,
            transform_buf, transform_length, result, result_length))) {
      return FONT_COMPRESSION_FAILURE();
    }
  }
  return true;
}

bool ConvertWOFF2ToTTF(uint8_t* result, size_t result_length,
                       const uint8_t* data, size_t length) {
  Buffer file(data, length);

  uint32_t signature;
  uint32_t flavor;
  if (PREDICT_FALSE(!file.ReadU32(&signature) || signature != kWoff2Signature ||
      !file.ReadU32(&flavor))) {
    return FONT_COMPRESSION_FAILURE();
  }

  // TODO(user): Should call IsValidVersionTag() here.

  uint32_t reported_length;
  if (PREDICT_FALSE(
      !file.ReadU32(&reported_length) || length != reported_length)) {
    return FONT_COMPRESSION_FAILURE();
  }
  uint16_t num_tables;
  if (PREDICT_FALSE(!file.ReadU16(&num_tables) || !num_tables)) {
    return FONT_COMPRESSION_FAILURE();
  }
  // We don't care about these fields of the header:
  //   uint16_t reserved
  //   uint32_t total_sfnt_size, the caller already passes it as result_length
  if (PREDICT_FALSE(!file.Skip(6))) {
    return FONT_COMPRESSION_FAILURE();
  }
  uint32_t compressed_length;
  if (PREDICT_FALSE(!file.ReadU32(&compressed_length))) {
    return FONT_COMPRESSION_FAILURE();
  }
  // We don't care about these fields of the header:
  //   uint16_t major_version, minor_version
  if (PREDICT_FALSE(!file.Skip(2 * 2))) {
    return FONT_COMPRESSION_FAILURE();
  }
  uint32_t meta_offset;
  uint32_t meta_length;
  uint32_t meta_length_orig;
  if (PREDICT_FALSE(!file.ReadU32(&meta_offset) ||
      !file.ReadU32(&meta_length) ||
      !file.ReadU32(&meta_length_orig))) {
    return FONT_COMPRESSION_FAILURE();
  }
  if (meta_offset) {
    if (PREDICT_FALSE(
        meta_offset >= length || length - meta_offset < meta_length)) {
      return FONT_COMPRESSION_FAILURE();
    }
  }
  uint32_t priv_offset;
  uint32_t priv_length;
  if (PREDICT_FALSE(!file.ReadU32(&priv_offset) ||
      !file.ReadU32(&priv_length))) {
    return FONT_COMPRESSION_FAILURE();
  }
  if (priv_offset) {
    if (PREDICT_FALSE(
        priv_offset >= length || length - priv_offset < priv_length)) {
      return FONT_COMPRESSION_FAILURE();
    }
  }
  std::vector<Table> tables(num_tables);
  if (PREDICT_FALSE(!ReadTableDirectory(&file, &tables, num_tables))) {
    return FONT_COMPRESSION_FAILURE();
  }

  uint32_t header_version = 0;
  // for each font in a ttc, metadata to use when rebuilding
  std::vector<TtcFont> ttc_fonts;
  std::map<const Table*, const Table*> loca_by_glyf;

  if (flavor == kTtcFontFlavor) {
    if (PREDICT_FALSE(!file.ReadU32(&header_version))) {
      return FONT_COMPRESSION_FAILURE();
    }
    uint32_t num_fonts;
    if (PREDICT_FALSE(!Read255UShort(&file, &num_fonts) || !num_fonts)) {
      return FONT_COMPRESSION_FAILURE();
    }
    ttc_fonts.resize(num_fonts);

    for (uint32_t i = 0; i < num_fonts; i++) {
      TtcFont& ttc_font = ttc_fonts[i];
      uint32_t num_tables;
      if (PREDICT_FALSE(!Read255UShort(&file, &num_tables) || !num_tables)) {
        return FONT_COMPRESSION_FAILURE();
      }
      if (PREDICT_FALSE(!file.ReadU32(&ttc_font.flavor))) {
        return FONT_COMPRESSION_FAILURE();
      }

      ttc_font.table_indices.resize(num_tables);

      const Table* glyf_table = NULL;
      const Table* loca_table = NULL;
      uint16_t glyf_idx;
      uint16_t loca_idx;

      for (uint32_t j = 0; j < num_tables; j++) {
        unsigned int table_idx;
        if (PREDICT_FALSE(!Read255UShort(&file, &table_idx)) ||
            table_idx >= tables.size()) {
          return FONT_COMPRESSION_FAILURE();
        }
        ttc_font.table_indices[j] = table_idx;

        const Table& table = tables[table_idx];
        if (table.tag == kLocaTableTag) {
          loca_table = &table;
          loca_idx = table_idx;
        }
        if (table.tag == kGlyfTableTag) {
          glyf_table = &table;
          glyf_idx = table_idx;
        }

      }

      if (PREDICT_FALSE((glyf_table == NULL) != (loca_table == NULL))) {
#ifdef FONT_COMPRESSION_BIN
        fprintf(stderr, "Cannot have just one of glyf/loca\n");
#endif
        return FONT_COMPRESSION_FAILURE();
      }

      if (glyf_table != NULL && loca_table != NULL) {
        loca_by_glyf[glyf_table] = loca_table;
      }
    }
  }

  const uint64_t first_table_offset =
    ComputeOffsetToFirstTable(header_version, num_tables, ttc_fonts);

  if (PREDICT_FALSE(first_table_offset > result_length)) {
    return FONT_COMPRESSION_FAILURE();
  }

  uint64_t compressed_offset = file.offset();
  if (PREDICT_FALSE(compressed_offset > std::numeric_limits<uint32_t>::max())) {
    return FONT_COMPRESSION_FAILURE();
  }
  uint64_t src_offset = Round4(compressed_offset + compressed_length);
  uint64_t dst_offset = first_table_offset;


  for (uint16_t i = 0; i < num_tables; ++i) {
    Table* table = &tables[i];
    table->dst_offset = dst_offset;
    dst_offset += table->dst_length;
    if (PREDICT_FALSE(dst_offset > std::numeric_limits<uint32_t>::max())) {
      return FONT_COMPRESSION_FAILURE();
    }
    dst_offset = Round4(dst_offset);
  }
  if (PREDICT_FALSE(src_offset > length || dst_offset != result_length)) {
#ifdef FONT_COMPRESSION_BIN
    fprintf(stderr, "offset fail; src_offset %" PRIu64 " length %lu "
      "dst_offset %" PRIu64 " result_length %lu\n",
      src_offset, length, dst_offset, result_length);
#endif
    return FONT_COMPRESSION_FAILURE();
  }

  // Re-order tables in output (OTSpec) order
  std::vector<Table> sorted_tables(tables);
  if (header_version) {
    // collection; we have to sort the table offset vector in each font
    for (auto& ttc_font : ttc_fonts) {
      std::map<uint32_t, uint16_t> sorted_index_by_tag;
      for (auto table_index : ttc_font.table_indices) {
        sorted_index_by_tag[tables[table_index].tag] = table_index;
      }
      uint16_t index = 0;
      for (auto& i : sorted_index_by_tag) {
        ttc_font.table_indices[index++] = i.second;
      }
    }
  } else {
    // non-collection; we can just sort the tables
    std::sort(sorted_tables.begin(), sorted_tables.end());
  }

  if (meta_offset) {
    if (PREDICT_FALSE(src_offset != meta_offset)) {
      return FONT_COMPRESSION_FAILURE();
    }
    src_offset = Round4(meta_offset + meta_length);
    if (PREDICT_FALSE(src_offset > std::numeric_limits<uint32_t>::max())) {
      return FONT_COMPRESSION_FAILURE();
    }
  }

  if (priv_offset) {
    if (PREDICT_FALSE(src_offset != priv_offset)) {
      return FONT_COMPRESSION_FAILURE();
    }
    src_offset = Round4(priv_offset + priv_length);
    if (PREDICT_FALSE(src_offset > std::numeric_limits<uint32_t>::max())) {
      return FONT_COMPRESSION_FAILURE();
    }
  }

  if (PREDICT_FALSE(src_offset != Round4(length))) {
    return FONT_COMPRESSION_FAILURE();
  }

  // Start building the font
  size_t offset = 0;
  size_t offset_table = 0;
  if (header_version) {
    // TTC header
    offset = StoreU32(result, offset, flavor);  // TAG TTCTag
    offset = StoreU32(result, offset, header_version);  // FIXED Version
    offset = StoreU32(result, offset, ttc_fonts.size());  // ULONG numFonts
    // Space for ULONG OffsetTable[numFonts] (zeroed initially)
    offset_table = offset;  // keep start of offset table for later
    for (size_t i = 0; i < ttc_fonts.size(); i++) {
      offset = StoreU32(result, offset, 0);  // will fill real values in later
    }
    // space for DSIG fields for header v2
    if (header_version == 0x00020000) {
      offset = StoreU32(result, offset, 0);  // ULONG ulDsigTag
      offset = StoreU32(result, offset, 0);  // ULONG ulDsigLength
      offset = StoreU32(result, offset, 0);  // ULONG ulDsigOffset
    }

    // write Offset Tables and store the location of each in TTC Header
    for (auto& ttc_font : ttc_fonts) {
      // write Offset Table location into TTC Header
      offset_table = StoreU32(result, offset_table, offset);

      // write the actual offset table so our header doesn't lie
      ttc_font.dst_offset = offset;
      offset = StoreOffsetTable(result, offset, ttc_font.flavor,
                                ttc_font.table_indices.size());

      // write table entries
      for (const auto table_index : ttc_font.table_indices) {
        offset = StoreTableEntry(result, tables[table_index], offset);
      }
    }
  } else {
    offset = StoreOffsetTable(result, offset, flavor, num_tables);
    for (uint16_t i = 0; i < num_tables; ++i) {
      offset = StoreTableEntry(result, sorted_tables[i], offset);
    }
  }

  std::vector<uint8_t> uncompressed_buf;
  const uint8_t* transform_buf = NULL;
  uint64_t total_size = 0;
  for (uint16_t i = 0; i < num_tables; ++i) {
    total_size += tables[i].transform_length;
    if (PREDICT_FALSE(total_size > std::numeric_limits<uint32_t>::max())) {
      return FONT_COMPRESSION_FAILURE();
    }
  }
  uncompressed_buf.resize(total_size);
  const uint8_t* src_buf = data + compressed_offset;
  if (PREDICT_FALSE(!Woff2Uncompress(&uncompressed_buf[0], total_size,
                       src_buf, compressed_length))) {
    return FONT_COMPRESSION_FAILURE();
  }

  // round 1, copy across all the unmodified tables
  transform_buf = &uncompressed_buf[0];
  const uint8_t* const transform_buf_end = &uncompressed_buf[0]
                                           + uncompressed_buf.size();

  // We wipe out the values as they get written into the final result to dedup
  std::map<uint32_t, const uint8_t*> src_by_dest;
  for (uint16_t i = 0; i < num_tables; ++i) {
    const Table* table = &tables[i];
    size_t transform_length = table->transform_length;

    src_by_dest[table->dst_offset] = transform_buf;

    if (PREDICT_FALSE(transform_buf + transform_length > transform_buf_end)) {
      return FONT_COMPRESSION_FAILURE();
    }
    transform_buf += transform_length;
  }

  for (uint16_t i = 0; i < num_tables; ++i) {
    const Table* table = &tables[i];
    const size_t transform_length = table->transform_length;

    if ((table->flags & kWoff2FlagsTransform) != kWoff2FlagsTransform) {
      if (PREDICT_FALSE(transform_length != table->dst_length)) {
        return FONT_COMPRESSION_FAILURE();
      }
      if (PREDICT_FALSE(static_cast<uint64_t>(table->dst_offset +
          transform_length) > result_length)) {
        return FONT_COMPRESSION_FAILURE();
      }
      transform_buf = src_by_dest[table->dst_offset];
      src_by_dest.erase(table->dst_offset);
      std::memcpy(result + table->dst_offset, transform_buf, transform_length);
    }
  }

  // round 2, reconstruct transformed tables
  if (PREDICT_FALSE(header_version)) {
    // Rebuild collection font by font.
    std::vector<Table> font_tables;
    for (const auto& ttc_font : ttc_fonts) {
      font_tables.resize(ttc_font.table_indices.size());
      for (auto i = 0; i < ttc_font.table_indices.size(); i++) {
        font_tables[i] = tables[ttc_font.table_indices[i]];
      }
      if (PREDICT_FALSE(!ReconstructTransformedFont(font_tables, &src_by_dest,
                                                    result, result_length))) {
        return FONT_COMPRESSION_FAILURE();
      }
    }
  } else {
    if (PREDICT_FALSE(!ReconstructTransformedFont(tables, &src_by_dest, result,
                                                  result_length))) {
      return FONT_COMPRESSION_FAILURE();
    }
  }

  if (header_version) {
    if (PREDICT_FALSE(
        !FixCollectionChecksums(header_version, tables, ttc_fonts, result))) {
      return FONT_COMPRESSION_FAILURE();
    }
  } else {
    if (PREDICT_FALSE(!FixChecksums(sorted_tables, result))) {
      return FONT_COMPRESSION_FAILURE();
    }
  }

  return true;
}

} // namespace woff2

/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <stdbool.h>

#include "config/av1_rtcd.h"

#include "av1/encoder/block.h"
#include "av1/encoder/hash.h"
#include "av1/encoder/hash_motion.h"

#define kSrcBits 16
// kMaxAddr is the number of hash table buckets in p_hash_table->p_lookup_table.
// p_hash_table->p_lookup_table consists of 6 hash tables of 1 << kSrcBits
// buckets each. Each of the 6 supported block sizes (4, 8, 16, 32, 64, 128) has
// its own hash table, indexed by the return value of
// hash_block_size_to_index().
#define kMaxAddr (6 << kSrcBits)
#define kMaxCandidatesPerHashBucket 256

static void get_pixels_in_1D_char_array_by_block_2x2(const uint8_t *y_src,
                                                     int stride,
                                                     uint8_t *p_pixels_in1D) {
  const uint8_t *p_pel = y_src;
  int index = 0;
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      p_pixels_in1D[index++] = p_pel[j];
    }
    p_pel += stride;
  }
}

static void get_pixels_in_1D_short_array_by_block_2x2(const uint16_t *y_src,
                                                      int stride,
                                                      uint16_t *p_pixels_in1D) {
  const uint16_t *p_pel = y_src;
  int index = 0;
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      p_pixels_in1D[index++] = p_pel[j];
    }
    p_pel += stride;
  }
}

// the hash value (hash_value1) consists of two parts, the first 3 bits relate
// to the block size and the remaining 16 bits are the crc values. This
// function is used to get the first 3 bits.
static int hash_block_size_to_index(int block_size) {
  switch (block_size) {
    case 4: return 0;
    case 8: return 1;
    case 16: return 2;
    case 32: return 3;
    case 64: return 4;
    case 128: return 5;
    default: return -1;
  }
}

static uint32_t get_identity_hash_value(const uint8_t a, const uint8_t b,
                                        const uint8_t c, const uint8_t d) {
  // The four input values add up to 32 bits, which is the size of the output.
  // Just pack those values as is.
  return ((uint32_t)a << 24) + ((uint32_t)b << 16) + ((uint32_t)c << 8) +
         ((uint32_t)d);
}

static uint32_t get_xor_hash_value_hbd(const uint16_t a, const uint16_t b,
                                       const uint16_t c, const uint16_t d) {
  uint32_t result;
  // Pack the lower 8 bits of each input value to the 32 bit output, then xor
  // with the upper 8 bits of each input value.
  result = ((uint32_t)(a & 0x00ff) << 24) + ((uint32_t)(b & 0x00ff) << 16) +
           ((uint32_t)(c & 0x00ff) << 8) + ((uint32_t)(d & 0x00ff));
  result ^= ((uint32_t)(a & 0xff00) << 16) + ((uint32_t)(b & 0xff00) << 8) +
            ((uint32_t)(c & 0xff00)) + ((uint32_t)(d & 0xff00) >> 8);
  return result;
}

void av1_hash_table_init(IntraBCHashInfo *intrabc_hash_info) {
  if (!intrabc_hash_info->crc_initialized) {
    av1_crc32c_calculator_init(&intrabc_hash_info->crc_calculator);
    intrabc_hash_info->crc_initialized = 1;
  }
  intrabc_hash_info->intrabc_hash_table.p_lookup_table = NULL;
}

static void clear_all(hash_table *p_hash_table) {
  if (p_hash_table->p_lookup_table == NULL) {
    return;
  }
  for (int i = 0; i < kMaxAddr; i++) {
    if (p_hash_table->p_lookup_table[i] != NULL) {
      aom_vector_destroy(p_hash_table->p_lookup_table[i]);
      aom_free(p_hash_table->p_lookup_table[i]);
      p_hash_table->p_lookup_table[i] = NULL;
    }
  }
}

void av1_hash_table_destroy(hash_table *p_hash_table) {
  clear_all(p_hash_table);
  aom_free(p_hash_table->p_lookup_table);
  p_hash_table->p_lookup_table = NULL;
}

bool av1_hash_table_create(hash_table *p_hash_table) {
  if (p_hash_table->p_lookup_table != NULL) {
    clear_all(p_hash_table);
    return true;
  }
  p_hash_table->p_lookup_table =
      (Vector **)aom_calloc(kMaxAddr, sizeof(p_hash_table->p_lookup_table[0]));
  if (!p_hash_table->p_lookup_table) return false;
  return true;
}

static bool hash_table_add_to_table(hash_table *p_hash_table,
                                    uint32_t hash_value,
                                    const block_hash *curr_block_hash) {
  if (p_hash_table->p_lookup_table[hash_value] == NULL) {
    p_hash_table->p_lookup_table[hash_value] =
        aom_malloc(sizeof(*p_hash_table->p_lookup_table[hash_value]));
    if (p_hash_table->p_lookup_table[hash_value] == NULL) {
      return false;
    }
    if (aom_vector_setup(p_hash_table->p_lookup_table[hash_value], 10,
                         sizeof(*curr_block_hash)) == VECTOR_ERROR)
      return false;
  }
  // Place an upper bound each hash table bucket to up to 256 intrabc
  // block candidates, and ignore subsequent ones. Considering more can
  // unnecessarily slow down encoding for virtually no efficiency gain.
  if (aom_vector_byte_size(p_hash_table->p_lookup_table[hash_value]) <
      kMaxCandidatesPerHashBucket * sizeof(*curr_block_hash)) {
    if (aom_vector_push_back(p_hash_table->p_lookup_table[hash_value],
                             (void *)curr_block_hash) == VECTOR_ERROR)
      return false;
  }
  return true;
}

int32_t av1_hash_table_count(const hash_table *p_hash_table,
                             uint32_t hash_value) {
  if (p_hash_table->p_lookup_table[hash_value] == NULL) {
    return 0;
  } else {
    return (int32_t)(p_hash_table->p_lookup_table[hash_value]->size);
  }
}

Iterator av1_hash_get_first_iterator(hash_table *p_hash_table,
                                     uint32_t hash_value) {
  assert(av1_hash_table_count(p_hash_table, hash_value) > 0);
  return aom_vector_begin(p_hash_table->p_lookup_table[hash_value]);
}

void av1_generate_block_2x2_hash_value(const YV12_BUFFER_CONFIG *picture,
                                       uint32_t *pic_block_hash) {
  const int width = 2;
  const int height = 2;
  const int x_end = picture->y_crop_width - width + 1;
  const int y_end = picture->y_crop_height - height + 1;

  if (picture->flags & YV12_FLAG_HIGHBITDEPTH) {
    uint16_t p[4];
    int pos = 0;
    for (int y_pos = 0; y_pos < y_end; y_pos++) {
      for (int x_pos = 0; x_pos < x_end; x_pos++) {
        get_pixels_in_1D_short_array_by_block_2x2(
            CONVERT_TO_SHORTPTR(picture->y_buffer) + y_pos * picture->y_stride +
                x_pos,
            picture->y_stride, p);
        // For HBD, we either have 40 or 48 bits of input data that the xor hash
        // reduce to 32 bits. We intentionally don't want to "discard" bits to
        // avoid any kind of biasing.
        pic_block_hash[pos] = get_xor_hash_value_hbd(p[0], p[1], p[2], p[3]);
        pos++;
      }
      pos += width - 1;
    }
  } else {
    uint8_t p[4];
    int pos = 0;
    for (int y_pos = 0; y_pos < y_end; y_pos++) {
      for (int x_pos = 0; x_pos < x_end; x_pos++) {
        get_pixels_in_1D_char_array_by_block_2x2(
            picture->y_buffer + y_pos * picture->y_stride + x_pos,
            picture->y_stride, p);
        // This 2x2 hash isn't used directly as a "key" for the hash table, so
        // we can afford to just copy the 4 8-bit pixel values as a single
        // 32-bit value directly. (i.e. there are no concerns of a lack of
        // uniform distribution)
        pic_block_hash[pos] = get_identity_hash_value(p[0], p[1], p[2], p[3]);
        pos++;
      }
      pos += width - 1;
    }
  }
}

void av1_generate_block_hash_value(IntraBCHashInfo *intrabc_hash_info,
                                   const YV12_BUFFER_CONFIG *picture,
                                   int block_size,
                                   const uint32_t *src_pic_block_hash,
                                   uint32_t *dst_pic_block_hash) {
  CRC32C *calc = &intrabc_hash_info->crc_calculator;

  const int pic_width = picture->y_crop_width;
  const int x_end = picture->y_crop_width - block_size + 1;
  const int y_end = picture->y_crop_height - block_size + 1;
  const int src_size = block_size >> 1;

  uint32_t p[4];
  const int length = sizeof(p);

  int pos = 0;
  for (int y_pos = 0; y_pos < y_end; y_pos++) {
    for (int x_pos = 0; x_pos < x_end; x_pos++) {
      // Build up a bigger block from 4 smaller, non-overlapping source block
      // hashes, and compute its hash. Note: source blocks at the right and
      // bottom borders cannot be part of larger blocks, therefore they won't be
      // considered into the block hash value generation process.
      p[0] = src_pic_block_hash[pos];
      p[1] = src_pic_block_hash[pos + src_size];
      p[2] = src_pic_block_hash[pos + src_size * pic_width];
      p[3] = src_pic_block_hash[pos + src_size * pic_width + src_size];
      // TODO: bug aomedia:433531610 - serialize input values in a way that's
      // independent of the computer architecture's endianness
      dst_pic_block_hash[pos] =
          av1_get_crc32c_value(calc, (uint8_t *)p, length);
      pos++;
    }
    pos += block_size - 1;
  }
}

bool av1_add_to_hash_map_by_row_with_precal_data(hash_table *p_hash_table,
                                                 const uint32_t *pic_hash,
                                                 int pic_width, int pic_height,
                                                 int block_size) {
  const int x_end = pic_width - block_size + 1;
  const int y_end = pic_height - block_size + 1;

  int add_value = hash_block_size_to_index(block_size);
  assert(add_value >= 0);
  add_value <<= kSrcBits;
  const int crc_mask = (1 << kSrcBits) - 1;
  int step = block_size;
  int x_offset = 0;
  int y_offset = 0;

  // Explore the entire frame hierarchically to add intrabc candidate blocks to
  // the hash table, by starting with coarser steps (the block size), towards
  // finer-grained steps until every candidate block has been considered.
  // The nested for loop goes through the pic_hash array column by column.

  // Doing a hierarchical block exploration helps maximize spatial dispersion
  // of the first and foremost candidate blocks while minimizing overlap between
  // them. This is helpful because we only keep up to 256 entries of the
  // same candidate block (located in different places), so we want those
  // entries to cover the biggest area of the image to encode to maximize coding
  // efficiency.

  // This is the coordinate exploration order example for an 8x8 region, with
  // block_size = 4. The top-left corner (x, y) coordinates of each candidate
  // block are shown below. There are 5 * 5 (25) candidate blocks.
  //    x  0  1  2  3  4  5  6  7
  //  y +------------------------
  //  0 |  1 10  5 13  3
  //  1 | 16 22 18 24 20
  //  2 |  7 11  9 14  8
  //  3 | 17 23 19 25 21
  //  4 |  2 12  6 15  4--------+
  //  5 |              | 4 x 4  |
  //  6 |              | block  |
  //  7 |              +--------+

  // Please note that due to the way block exploration works, the smallest step
  // used is 2 (i.e. no two adjacent blocks will be explored consecutively).
  // Also, the exploration is designed to visit each block candidate only once.
  while (step > 1) {
    for (int x_pos = x_offset; x_pos < x_end; x_pos += step) {
      for (int y_pos = y_offset; y_pos < y_end; y_pos += step) {
        const int pos = y_pos * pic_width + x_pos;
        block_hash curr_block_hash;

        curr_block_hash.x = x_pos;
        curr_block_hash.y = y_pos;

        const uint32_t hash_value1 = (pic_hash[pos] & crc_mask) + add_value;
        curr_block_hash.hash_value2 = pic_hash[pos];

        if (!hash_table_add_to_table(p_hash_table, hash_value1,
                                     &curr_block_hash)) {
          return false;
        }
      }
    }

    // Adjust offsets and step sizes with this state machine.
    // State 0 is needed because no blocks in pic_hash have been explored,
    // so exploration requires a way to account for blocks with both zero
    // x_offset and zero y_offset.
    // State 0 is always meant to be executed first, but the relative order of
    // states 1, 2 and 3 can be arbitrary, as long as no two adjacent blocks
    // are explored consecutively.
    if (x_offset == 0 && y_offset == 0) {
      // State 0 -> State 1: special case
      // This state transition will only execute when step == block_size
      x_offset = step / 2;
    } else if (x_offset == step / 2 && y_offset == 0) {
      // State 1 -> State 2
      x_offset = 0;
      y_offset = step / 2;
    } else if (x_offset == 0 && y_offset == step / 2) {
      // State 2 -> State 3
      x_offset = step / 2;
    } else {
      assert(x_offset == step / 2 && y_offset == step / 2);
      // State 3 -> State 1: We've fully explored all the coordinates for the
      // current step size, continue by halving the step size
      step /= 2;
      x_offset = step / 2;
      y_offset = 0;
    }
  }

  return true;
}

int av1_hash_is_horizontal_perfect(const YV12_BUFFER_CONFIG *picture,
                                   int block_size, int x_start, int y_start) {
  const int stride = picture->y_stride;
  const uint8_t *p = picture->y_buffer + y_start * stride + x_start;

  if (picture->flags & YV12_FLAG_HIGHBITDEPTH) {
    const uint16_t *p16 = CONVERT_TO_SHORTPTR(p);
    for (int i = 0; i < block_size; i++) {
      for (int j = 1; j < block_size; j++) {
        if (p16[j] != p16[0]) {
          return 0;
        }
      }
      p16 += stride;
    }
  } else {
    for (int i = 0; i < block_size; i++) {
      for (int j = 1; j < block_size; j++) {
        if (p[j] != p[0]) {
          return 0;
        }
      }
      p += stride;
    }
  }

  return 1;
}

int av1_hash_is_vertical_perfect(const YV12_BUFFER_CONFIG *picture,
                                 int block_size, int x_start, int y_start) {
  const int stride = picture->y_stride;
  const uint8_t *p = picture->y_buffer + y_start * stride + x_start;

  if (picture->flags & YV12_FLAG_HIGHBITDEPTH) {
    const uint16_t *p16 = CONVERT_TO_SHORTPTR(p);
    for (int i = 0; i < block_size; i++) {
      for (int j = 1; j < block_size; j++) {
        if (p16[j * stride + i] != p16[i]) {
          return 0;
        }
      }
    }
  } else {
    for (int i = 0; i < block_size; i++) {
      for (int j = 1; j < block_size; j++) {
        if (p[j * stride + i] != p[i]) {
          return 0;
        }
      }
    }
  }
  return 1;
}

void av1_get_block_hash_value(IntraBCHashInfo *intra_bc_hash_info,
                              const uint8_t *y_src, int stride, int block_size,
                              uint32_t *hash_value1, uint32_t *hash_value2,
                              int use_highbitdepth) {
  int add_value = hash_block_size_to_index(block_size);
  assert(add_value >= 0);
  add_value <<= kSrcBits;
  const int crc_mask = (1 << kSrcBits) - 1;

  CRC32C *calc = &intra_bc_hash_info->crc_calculator;
  uint32_t **buf = intra_bc_hash_info->hash_value_buffer;

  // 2x2 subblock hash values in current CU
  int sub_block_in_width = (block_size >> 1);
  if (use_highbitdepth) {
    uint16_t pixel_to_hash[4];
    uint16_t *y16_src = CONVERT_TO_SHORTPTR(y_src);
    for (int y_pos = 0; y_pos < block_size; y_pos += 2) {
      for (int x_pos = 0; x_pos < block_size; x_pos += 2) {
        int pos = (y_pos >> 1) * sub_block_in_width + (x_pos >> 1);
        get_pixels_in_1D_short_array_by_block_2x2(
            y16_src + y_pos * stride + x_pos, stride, pixel_to_hash);
        assert(pos < AOM_BUFFER_SIZE_FOR_BLOCK_HASH);
        // For HBD, we either have 40 or 48 bits of input data that the xor hash
        // reduce to 32 bits. We intentionally don't want to "discard" bits to
        // avoid any kind of biasing.
        buf[0][pos] =
            get_xor_hash_value_hbd(pixel_to_hash[0], pixel_to_hash[1],
                                   pixel_to_hash[2], pixel_to_hash[3]);
      }
    }
  } else {
    uint8_t pixel_to_hash[4];
    for (int y_pos = 0; y_pos < block_size; y_pos += 2) {
      for (int x_pos = 0; x_pos < block_size; x_pos += 2) {
        int pos = (y_pos >> 1) * sub_block_in_width + (x_pos >> 1);
        get_pixels_in_1D_char_array_by_block_2x2(y_src + y_pos * stride + x_pos,
                                                 stride, pixel_to_hash);
        assert(pos < AOM_BUFFER_SIZE_FOR_BLOCK_HASH);
        // This 2x2 hash isn't used directly as a "key" for the hash table, so
        // we can afford to just copy the 4 8-bit pixel values as a single
        // 32-bit value directly. (i.e. there are no concerns of a lack of
        // uniform distribution)
        buf[0][pos] =
            get_identity_hash_value(pixel_to_hash[0], pixel_to_hash[1],
                                    pixel_to_hash[2], pixel_to_hash[3]);
      }
    }
  }

  int src_sub_block_in_width = sub_block_in_width;
  sub_block_in_width >>= 1;

  int src_idx = 0;
  int dst_idx = !src_idx;

  // 4x4 subblock hash values to current block hash values
  uint32_t to_hash[4];
  for (int sub_width = 4; sub_width <= block_size;
       sub_width *= 2, src_idx = !src_idx) {
    dst_idx = !src_idx;

    int dst_pos = 0;
    for (int y_pos = 0; y_pos < sub_block_in_width; y_pos++) {
      for (int x_pos = 0; x_pos < sub_block_in_width; x_pos++) {
        int srcPos = (y_pos << 1) * src_sub_block_in_width + (x_pos << 1);

        assert(srcPos + 1 < AOM_BUFFER_SIZE_FOR_BLOCK_HASH);
        assert(srcPos + src_sub_block_in_width + 1 <
               AOM_BUFFER_SIZE_FOR_BLOCK_HASH);
        assert(dst_pos < AOM_BUFFER_SIZE_FOR_BLOCK_HASH);

        to_hash[0] = buf[src_idx][srcPos];
        to_hash[1] = buf[src_idx][srcPos + 1];
        to_hash[2] = buf[src_idx][srcPos + src_sub_block_in_width];
        to_hash[3] = buf[src_idx][srcPos + src_sub_block_in_width + 1];

        // TODO: bug aomedia:433531610 - serialize input values in a way that's
        // independent of the computer architecture's endianness
        buf[dst_idx][dst_pos] =
            av1_get_crc32c_value(calc, (uint8_t *)to_hash, sizeof(to_hash));
        dst_pos++;
      }
    }

    src_sub_block_in_width = sub_block_in_width;
    sub_block_in_width >>= 1;
  }

  *hash_value1 = (buf[dst_idx][0] & crc_mask) + add_value;
  *hash_value2 = buf[dst_idx][0];
}

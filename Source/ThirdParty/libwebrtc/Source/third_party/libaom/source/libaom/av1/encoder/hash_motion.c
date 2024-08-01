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
#define kBlockSizeBits 3
#define kMaxAddr (1 << (kSrcBits + kBlockSizeBits))

// TODO(youzhou@microsoft.com): is higher than 8 bits screen content supported?
// If yes, fix this function
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

static int is_block_2x2_row_same_value(const uint8_t *p) {
  if (p[0] != p[1] || p[2] != p[3]) {
    return 0;
  }
  return 1;
}

static int is_block16_2x2_row_same_value(const uint16_t *p) {
  if (p[0] != p[1] || p[2] != p[3]) {
    return 0;
  }
  return 1;
}

static int is_block_2x2_col_same_value(const uint8_t *p) {
  if ((p[0] != p[2]) || (p[1] != p[3])) {
    return 0;
  }
  return 1;
}

static int is_block16_2x2_col_same_value(const uint16_t *p) {
  if ((p[0] != p[2]) || (p[1] != p[3])) {
    return 0;
  }
  return 1;
}

// the hash value (hash_value1 consists two parts, the first 3 bits relate to
// the block size and the remaining 16 bits are the crc values. This fuction
// is used to get the first 3 bits.
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

void av1_hash_table_init(IntraBCHashInfo *intrabc_hash_info) {
  if (!intrabc_hash_info->g_crc_initialized) {
    av1_crc_calculator_init(&intrabc_hash_info->crc_calculator1, 24, 0x5D6DCB);
    av1_crc_calculator_init(&intrabc_hash_info->crc_calculator2, 24, 0x864CFB);
    intrabc_hash_info->g_crc_initialized = 1;
  }
  intrabc_hash_info->intrabc_hash_table.p_lookup_table = NULL;
}

void av1_hash_table_clear_all(hash_table *p_hash_table) {
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
  av1_hash_table_clear_all(p_hash_table);
  aom_free(p_hash_table->p_lookup_table);
  p_hash_table->p_lookup_table = NULL;
}

bool av1_hash_table_create(hash_table *p_hash_table) {
  if (p_hash_table->p_lookup_table != NULL) {
    av1_hash_table_clear_all(p_hash_table);
    return true;
  }
  p_hash_table->p_lookup_table =
      (Vector **)aom_calloc(kMaxAddr, sizeof(p_hash_table->p_lookup_table[0]));
  if (!p_hash_table->p_lookup_table) return false;
  return true;
}

static bool hash_table_add_to_table(hash_table *p_hash_table,
                                    uint32_t hash_value,
                                    block_hash *curr_block_hash) {
  if (p_hash_table->p_lookup_table[hash_value] == NULL) {
    p_hash_table->p_lookup_table[hash_value] =
        aom_malloc(sizeof(p_hash_table->p_lookup_table[0][0]));
    if (p_hash_table->p_lookup_table[hash_value] == NULL) {
      return false;
    }
    if (aom_vector_setup(p_hash_table->p_lookup_table[hash_value], 10,
                         sizeof(curr_block_hash[0])) == VECTOR_ERROR)
      return false;
    if (aom_vector_push_back(p_hash_table->p_lookup_table[hash_value],
                             curr_block_hash) == VECTOR_ERROR)
      return false;
  } else {
    if (aom_vector_push_back(p_hash_table->p_lookup_table[hash_value],
                             curr_block_hash) == VECTOR_ERROR)
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

int32_t av1_has_exact_match(hash_table *p_hash_table, uint32_t hash_value1,
                            uint32_t hash_value2) {
  if (p_hash_table->p_lookup_table[hash_value1] == NULL) {
    return 0;
  }
  Iterator iterator =
      aom_vector_begin(p_hash_table->p_lookup_table[hash_value1]);
  Iterator last = aom_vector_end(p_hash_table->p_lookup_table[hash_value1]);
  for (; !aom_iterator_equals(&iterator, &last);
       aom_iterator_increment(&iterator)) {
    if ((*(block_hash *)aom_iterator_get(&iterator)).hash_value2 ==
        hash_value2) {
      return 1;
    }
  }
  return 0;
}

void av1_generate_block_2x2_hash_value(IntraBCHashInfo *intrabc_hash_info,
                                       const YV12_BUFFER_CONFIG *picture,
                                       uint32_t *pic_block_hash[2],
                                       int8_t *pic_block_same_info[3]) {
  const int width = 2;
  const int height = 2;
  const int x_end = picture->y_crop_width - width + 1;
  const int y_end = picture->y_crop_height - height + 1;
  CRC_CALCULATOR *calc_1 = &intrabc_hash_info->crc_calculator1;
  CRC_CALCULATOR *calc_2 = &intrabc_hash_info->crc_calculator2;

  const int length = width * 2;
  if (picture->flags & YV12_FLAG_HIGHBITDEPTH) {
    uint16_t p[4];
    int pos = 0;
    for (int y_pos = 0; y_pos < y_end; y_pos++) {
      for (int x_pos = 0; x_pos < x_end; x_pos++) {
        get_pixels_in_1D_short_array_by_block_2x2(
            CONVERT_TO_SHORTPTR(picture->y_buffer) + y_pos * picture->y_stride +
                x_pos,
            picture->y_stride, p);
        pic_block_same_info[0][pos] = is_block16_2x2_row_same_value(p);
        pic_block_same_info[1][pos] = is_block16_2x2_col_same_value(p);

        pic_block_hash[0][pos] =
            av1_get_crc_value(calc_1, (uint8_t *)p, length * sizeof(p[0]));
        pic_block_hash[1][pos] =
            av1_get_crc_value(calc_2, (uint8_t *)p, length * sizeof(p[0]));
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
        pic_block_same_info[0][pos] = is_block_2x2_row_same_value(p);
        pic_block_same_info[1][pos] = is_block_2x2_col_same_value(p);

        pic_block_hash[0][pos] =
            av1_get_crc_value(calc_1, p, length * sizeof(p[0]));
        pic_block_hash[1][pos] =
            av1_get_crc_value(calc_2, p, length * sizeof(p[0]));
        pos++;
      }
      pos += width - 1;
    }
  }
}

void av1_generate_block_hash_value(IntraBCHashInfo *intrabc_hash_info,
                                   const YV12_BUFFER_CONFIG *picture,
                                   int block_size,
                                   uint32_t *src_pic_block_hash[2],
                                   uint32_t *dst_pic_block_hash[2],
                                   int8_t *src_pic_block_same_info[3],
                                   int8_t *dst_pic_block_same_info[3]) {
  CRC_CALCULATOR *calc_1 = &intrabc_hash_info->crc_calculator1;
  CRC_CALCULATOR *calc_2 = &intrabc_hash_info->crc_calculator2;

  const int pic_width = picture->y_crop_width;
  const int x_end = picture->y_crop_width - block_size + 1;
  const int y_end = picture->y_crop_height - block_size + 1;

  const int src_size = block_size >> 1;
  const int quad_size = block_size >> 2;

  uint32_t p[4];
  const int length = sizeof(p);

  int pos = 0;
  for (int y_pos = 0; y_pos < y_end; y_pos++) {
    for (int x_pos = 0; x_pos < x_end; x_pos++) {
      p[0] = src_pic_block_hash[0][pos];
      p[1] = src_pic_block_hash[0][pos + src_size];
      p[2] = src_pic_block_hash[0][pos + src_size * pic_width];
      p[3] = src_pic_block_hash[0][pos + src_size * pic_width + src_size];
      dst_pic_block_hash[0][pos] =
          av1_get_crc_value(calc_1, (uint8_t *)p, length);

      p[0] = src_pic_block_hash[1][pos];
      p[1] = src_pic_block_hash[1][pos + src_size];
      p[2] = src_pic_block_hash[1][pos + src_size * pic_width];
      p[3] = src_pic_block_hash[1][pos + src_size * pic_width + src_size];
      dst_pic_block_hash[1][pos] =
          av1_get_crc_value(calc_2, (uint8_t *)p, length);

      dst_pic_block_same_info[0][pos] =
          src_pic_block_same_info[0][pos] &&
          src_pic_block_same_info[0][pos + quad_size] &&
          src_pic_block_same_info[0][pos + src_size] &&
          src_pic_block_same_info[0][pos + src_size * pic_width] &&
          src_pic_block_same_info[0][pos + src_size * pic_width + quad_size] &&
          src_pic_block_same_info[0][pos + src_size * pic_width + src_size];

      dst_pic_block_same_info[1][pos] =
          src_pic_block_same_info[1][pos] &&
          src_pic_block_same_info[1][pos + src_size] &&
          src_pic_block_same_info[1][pos + quad_size * pic_width] &&
          src_pic_block_same_info[1][pos + quad_size * pic_width + src_size] &&
          src_pic_block_same_info[1][pos + src_size * pic_width] &&
          src_pic_block_same_info[1][pos + src_size * pic_width + src_size];
      pos++;
    }
    pos += block_size - 1;
  }

  if (block_size >= 4) {
    const int size_minus_1 = block_size - 1;
    pos = 0;
    for (int y_pos = 0; y_pos < y_end; y_pos++) {
      for (int x_pos = 0; x_pos < x_end; x_pos++) {
        dst_pic_block_same_info[2][pos] =
            (!dst_pic_block_same_info[0][pos] &&
             !dst_pic_block_same_info[1][pos]) ||
            (((x_pos & size_minus_1) == 0) && ((y_pos & size_minus_1) == 0));
        pos++;
      }
      pos += block_size - 1;
    }
  }
}

bool av1_add_to_hash_map_by_row_with_precal_data(hash_table *p_hash_table,
                                                 uint32_t *pic_hash[2],
                                                 int8_t *pic_is_same,
                                                 int pic_width, int pic_height,
                                                 int block_size) {
  const int x_end = pic_width - block_size + 1;
  const int y_end = pic_height - block_size + 1;

  const int8_t *src_is_added = pic_is_same;
  const uint32_t *src_hash[2] = { pic_hash[0], pic_hash[1] };

  int add_value = hash_block_size_to_index(block_size);
  assert(add_value >= 0);
  add_value <<= kSrcBits;
  const int crc_mask = (1 << kSrcBits) - 1;

  for (int x_pos = 0; x_pos < x_end; x_pos++) {
    for (int y_pos = 0; y_pos < y_end; y_pos++) {
      const int pos = y_pos * pic_width + x_pos;
      // valid data
      if (src_is_added[pos]) {
        block_hash curr_block_hash;
        curr_block_hash.x = x_pos;
        curr_block_hash.y = y_pos;

        const uint32_t hash_value1 = (src_hash[0][pos] & crc_mask) + add_value;
        curr_block_hash.hash_value2 = src_hash[1][pos];

        if (!hash_table_add_to_table(p_hash_table, hash_value1,
                                     &curr_block_hash)) {
          return false;
        }
      }
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

void av1_get_block_hash_value(IntraBCHashInfo *intrabc_hash_info,
                              const uint8_t *y_src, int stride, int block_size,
                              uint32_t *hash_value1, uint32_t *hash_value2,
                              int use_highbitdepth) {
  int add_value = hash_block_size_to_index(block_size);
  assert(add_value >= 0);
  add_value <<= kSrcBits;
  const int crc_mask = (1 << kSrcBits) - 1;

  CRC_CALCULATOR *calc_1 = &intrabc_hash_info->crc_calculator1;
  CRC_CALCULATOR *calc_2 = &intrabc_hash_info->crc_calculator2;
  uint32_t **buf_1 = intrabc_hash_info->hash_value_buffer[0];
  uint32_t **buf_2 = intrabc_hash_info->hash_value_buffer[1];

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
        buf_1[0][pos] = av1_get_crc_value(calc_1, (uint8_t *)pixel_to_hash,
                                          sizeof(pixel_to_hash));
        buf_2[0][pos] = av1_get_crc_value(calc_2, (uint8_t *)pixel_to_hash,
                                          sizeof(pixel_to_hash));
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
        buf_1[0][pos] =
            av1_get_crc_value(calc_1, pixel_to_hash, sizeof(pixel_to_hash));
        buf_2[0][pos] =
            av1_get_crc_value(calc_2, pixel_to_hash, sizeof(pixel_to_hash));
      }
    }
  }

  int src_sub_block_in_width = sub_block_in_width;
  sub_block_in_width >>= 1;

  int src_idx = 1;
  int dst_idx = 0;

  // 4x4 subblock hash values to current block hash values
  uint32_t to_hash[4];
  for (int sub_width = 4; sub_width <= block_size; sub_width *= 2) {
    src_idx = 1 - src_idx;
    dst_idx = 1 - dst_idx;

    int dst_pos = 0;
    for (int y_pos = 0; y_pos < sub_block_in_width; y_pos++) {
      for (int x_pos = 0; x_pos < sub_block_in_width; x_pos++) {
        int srcPos = (y_pos << 1) * src_sub_block_in_width + (x_pos << 1);

        assert(srcPos + 1 < AOM_BUFFER_SIZE_FOR_BLOCK_HASH);
        assert(srcPos + src_sub_block_in_width + 1 <
               AOM_BUFFER_SIZE_FOR_BLOCK_HASH);
        assert(dst_pos < AOM_BUFFER_SIZE_FOR_BLOCK_HASH);
        to_hash[0] = buf_1[src_idx][srcPos];
        to_hash[1] = buf_1[src_idx][srcPos + 1];
        to_hash[2] = buf_1[src_idx][srcPos + src_sub_block_in_width];
        to_hash[3] = buf_1[src_idx][srcPos + src_sub_block_in_width + 1];

        buf_1[dst_idx][dst_pos] =
            av1_get_crc_value(calc_1, (uint8_t *)to_hash, sizeof(to_hash));

        to_hash[0] = buf_2[src_idx][srcPos];
        to_hash[1] = buf_2[src_idx][srcPos + 1];
        to_hash[2] = buf_2[src_idx][srcPos + src_sub_block_in_width];
        to_hash[3] = buf_2[src_idx][srcPos + src_sub_block_in_width + 1];
        buf_2[dst_idx][dst_pos] =
            av1_get_crc_value(calc_2, (uint8_t *)to_hash, sizeof(to_hash));
        dst_pos++;
      }
    }

    src_sub_block_in_width = sub_block_in_width;
    sub_block_in_width >>= 1;
  }

  *hash_value1 = (buf_1[dst_idx][0] & crc_mask) + add_value;
  *hash_value2 = buf_2[dst_idx][0];
}

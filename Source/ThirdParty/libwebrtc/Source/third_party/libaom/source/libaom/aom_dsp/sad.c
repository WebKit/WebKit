/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <stdlib.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_ports/mem.h"
#include "aom_dsp/blend.h"

/* Sum the difference between every corresponding element of the buffers. */
static INLINE unsigned int sad(const uint8_t *a, int a_stride, const uint8_t *b,
                               int b_stride, int width, int height) {
  int y, x;
  unsigned int sad = 0;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      sad += abs(a[x] - b[x]);
    }

    a += a_stride;
    b += b_stride;
  }
  return sad;
}

#define SADMXN(m, n)                                                          \
  unsigned int aom_sad##m##x##n##_c(const uint8_t *src, int src_stride,       \
                                    const uint8_t *ref, int ref_stride) {     \
    return sad(src, src_stride, ref, ref_stride, m, n);                       \
  }                                                                           \
  unsigned int aom_sad##m##x##n##_avg_c(const uint8_t *src, int src_stride,   \
                                        const uint8_t *ref, int ref_stride,   \
                                        const uint8_t *second_pred) {         \
    uint8_t comp_pred[m * n];                                                 \
    aom_comp_avg_pred(comp_pred, second_pred, m, n, ref, ref_stride);         \
    return sad(src, src_stride, comp_pred, m, m, n);                          \
  }                                                                           \
  unsigned int aom_dist_wtd_sad##m##x##n##_avg_c(                             \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, \
      const uint8_t *second_pred, const DIST_WTD_COMP_PARAMS *jcp_param) {    \
    uint8_t comp_pred[m * n];                                                 \
    aom_dist_wtd_comp_avg_pred_c(comp_pred, second_pred, m, n, ref,           \
                                 ref_stride, jcp_param);                      \
    return sad(src, src_stride, comp_pred, m, m, n);                          \
  }                                                                           \
  unsigned int aom_sad_skip_##m##x##n##_c(const uint8_t *src, int src_stride, \
                                          const uint8_t *ref,                 \
                                          int ref_stride) {                   \
    return 2 * sad(src, 2 * src_stride, ref, 2 * ref_stride, (m), (n / 2));   \
  }

// Calculate sad against 4 reference locations and store each in sad_array
#define SAD_MXNX4D(m, n)                                                      \
  void aom_sad##m##x##n##x4d_c(const uint8_t *src, int src_stride,            \
                               const uint8_t *const ref_array[4],             \
                               int ref_stride, uint32_t sad_array[4]) {       \
    int i;                                                                    \
    for (i = 0; i < 4; ++i) {                                                 \
      sad_array[i] =                                                          \
          aom_sad##m##x##n##_c(src, src_stride, ref_array[i], ref_stride);    \
    }                                                                         \
  }                                                                           \
  void aom_sad_skip_##m##x##n##x4d_c(const uint8_t *src, int src_stride,      \
                                     const uint8_t *const ref_array[4],       \
                                     int ref_stride, uint32_t sad_array[4]) { \
    int i;                                                                    \
    for (i = 0; i < 4; ++i) {                                                 \
      sad_array[i] = 2 * sad(src, 2 * src_stride, ref_array[i],               \
                             2 * ref_stride, (m), (n / 2));                   \
    }                                                                         \
  }
// Call SIMD version of aom_sad_mxnx4d if the 3d version is unavailable.
#define SAD_MXNX3D(m, n)                                                      \
  void aom_sad##m##x##n##x3d_c(const uint8_t *src, int src_stride,            \
                               const uint8_t *const ref_array[4],             \
                               int ref_stride, uint32_t sad_array[4]) {       \
    aom_sad##m##x##n##x4d(src, src_stride, ref_array, ref_stride, sad_array); \
  }

// 128x128
SADMXN(128, 128)
SAD_MXNX4D(128, 128)
SAD_MXNX3D(128, 128)

// 128x64
SADMXN(128, 64)
SAD_MXNX4D(128, 64)
SAD_MXNX3D(128, 64)

// 64x128
SADMXN(64, 128)
SAD_MXNX4D(64, 128)
SAD_MXNX3D(64, 128)

// 64x64
SADMXN(64, 64)
SAD_MXNX4D(64, 64)
SAD_MXNX3D(64, 64)

// 64x32
SADMXN(64, 32)
SAD_MXNX4D(64, 32)
SAD_MXNX3D(64, 32)

// 32x64
SADMXN(32, 64)
SAD_MXNX4D(32, 64)
SAD_MXNX3D(32, 64)

// 32x32
SADMXN(32, 32)
SAD_MXNX4D(32, 32)
SAD_MXNX3D(32, 32)

// 32x16
SADMXN(32, 16)
SAD_MXNX4D(32, 16)
SAD_MXNX3D(32, 16)

// 16x32
SADMXN(16, 32)
SAD_MXNX4D(16, 32)
SAD_MXNX3D(16, 32)

// 16x16
SADMXN(16, 16)
SAD_MXNX4D(16, 16)
SAD_MXNX3D(16, 16)

// 16x8
SADMXN(16, 8)
SAD_MXNX4D(16, 8)
SAD_MXNX3D(16, 8)

// 8x16
SADMXN(8, 16)
SAD_MXNX4D(8, 16)
SAD_MXNX3D(8, 16)

// 8x8
SADMXN(8, 8)
SAD_MXNX4D(8, 8)
SAD_MXNX3D(8, 8)

// 8x4
SADMXN(8, 4)
SAD_MXNX4D(8, 4)
SAD_MXNX3D(8, 4)

// 4x8
SADMXN(4, 8)
SAD_MXNX4D(4, 8)
SAD_MXNX3D(4, 8)

// 4x4
SADMXN(4, 4)
SAD_MXNX4D(4, 4)
SAD_MXNX3D(4, 4)

#if !CONFIG_REALTIME_ONLY
SADMXN(4, 16)
SAD_MXNX4D(4, 16)
SADMXN(16, 4)
SAD_MXNX4D(16, 4)
SADMXN(8, 32)
SAD_MXNX4D(8, 32)
SADMXN(32, 8)
SAD_MXNX4D(32, 8)
SADMXN(16, 64)
SAD_MXNX4D(16, 64)
SADMXN(64, 16)
SAD_MXNX4D(64, 16)
SAD_MXNX3D(4, 16)
SAD_MXNX3D(16, 4)
SAD_MXNX3D(8, 32)
SAD_MXNX3D(32, 8)
SAD_MXNX3D(16, 64)
SAD_MXNX3D(64, 16)
#endif  // !CONFIG_REALTIME_ONLY

#if CONFIG_AV1_HIGHBITDEPTH
static INLINE unsigned int highbd_sad(const uint8_t *a8, int a_stride,
                                      const uint8_t *b8, int b_stride,
                                      int width, int height) {
  int y, x;
  unsigned int sad = 0;
  const uint16_t *a = CONVERT_TO_SHORTPTR(a8);
  const uint16_t *b = CONVERT_TO_SHORTPTR(b8);
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      sad += abs(a[x] - b[x]);
    }

    a += a_stride;
    b += b_stride;
  }
  return sad;
}

static INLINE unsigned int highbd_sadb(const uint8_t *a8, int a_stride,
                                       const uint8_t *b8, int b_stride,
                                       int width, int height) {
  int y, x;
  unsigned int sad = 0;
  const uint16_t *a = CONVERT_TO_SHORTPTR(a8);
  const uint16_t *b = CONVERT_TO_SHORTPTR(b8);
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      sad += abs(a[x] - b[x]);
    }

    a += a_stride;
    b += b_stride;
  }
  return sad;
}

#define HIGHBD_SADMXN(m, n)                                                    \
  unsigned int aom_highbd_sad##m##x##n##_c(const uint8_t *src, int src_stride, \
                                           const uint8_t *ref,                 \
                                           int ref_stride) {                   \
    return highbd_sad(src, src_stride, ref, ref_stride, m, n);                 \
  }                                                                            \
  unsigned int aom_highbd_sad##m##x##n##_avg_c(                                \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride,  \
      const uint8_t *second_pred) {                                            \
    uint16_t comp_pred[m * n];                                                 \
    uint8_t *const comp_pred8 = CONVERT_TO_BYTEPTR(comp_pred);                 \
    aom_highbd_comp_avg_pred(comp_pred8, second_pred, m, n, ref, ref_stride);  \
    return highbd_sadb(src, src_stride, comp_pred8, m, m, n);                  \
  }                                                                            \
  unsigned int aom_highbd_dist_wtd_sad##m##x##n##_avg_c(                       \
      const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride,  \
      const uint8_t *second_pred, const DIST_WTD_COMP_PARAMS *jcp_param) {     \
    uint16_t comp_pred[m * n];                                                 \
    uint8_t *const comp_pred8 = CONVERT_TO_BYTEPTR(comp_pred);                 \
    aom_highbd_dist_wtd_comp_avg_pred(comp_pred8, second_pred, m, n, ref,      \
                                      ref_stride, jcp_param);                  \
    return highbd_sadb(src, src_stride, comp_pred8, m, m, n);                  \
  }                                                                            \
  unsigned int aom_highbd_sad_skip_##m##x##n##_c(                              \
      const uint8_t *src, int src_stride, const uint8_t *ref,                  \
      int ref_stride) {                                                        \
    return 2 *                                                                 \
           highbd_sad(src, 2 * src_stride, ref, 2 * ref_stride, (m), (n / 2)); \
  }

#define HIGHBD_SAD_MXNX4D(m, n)                                              \
  void aom_highbd_sad##m##x##n##x4d_c(const uint8_t *src, int src_stride,    \
                                      const uint8_t *const ref_array[],      \
                                      int ref_stride, uint32_t *sad_array) { \
    int i;                                                                   \
    for (i = 0; i < 4; ++i) {                                                \
      sad_array[i] = aom_highbd_sad##m##x##n##_c(src, src_stride,            \
                                                 ref_array[i], ref_stride);  \
    }                                                                        \
  }                                                                          \
  void aom_highbd_sad_skip_##m##x##n##x4d_c(                                 \
      const uint8_t *src, int src_stride, const uint8_t *const ref_array[],  \
      int ref_stride, uint32_t *sad_array) {                                 \
    int i;                                                                   \
    for (i = 0; i < 4; ++i) {                                                \
      sad_array[i] = 2 * highbd_sad(src, 2 * src_stride, ref_array[i],       \
                                    2 * ref_stride, (m), (n / 2));           \
    }                                                                        \
  }
// Call SIMD version of aom_highbd_sad_mxnx4d if the 3d version is unavailable.
#define HIGHBD_SAD_MXNX3D(m, n)                                              \
  void aom_highbd_sad##m##x##n##x3d_c(const uint8_t *src, int src_stride,    \
                                      const uint8_t *const ref_array[],      \
                                      int ref_stride, uint32_t *sad_array) { \
    aom_highbd_sad##m##x##n##x4d(src, src_stride, ref_array, ref_stride,     \
                                 sad_array);                                 \
  }

// 128x128
HIGHBD_SADMXN(128, 128)
HIGHBD_SAD_MXNX4D(128, 128)
HIGHBD_SAD_MXNX3D(128, 128)

// 128x64
HIGHBD_SADMXN(128, 64)
HIGHBD_SAD_MXNX4D(128, 64)
HIGHBD_SAD_MXNX3D(128, 64)

// 64x128
HIGHBD_SADMXN(64, 128)
HIGHBD_SAD_MXNX4D(64, 128)
HIGHBD_SAD_MXNX3D(64, 128)

// 64x64
HIGHBD_SADMXN(64, 64)
HIGHBD_SAD_MXNX4D(64, 64)
HIGHBD_SAD_MXNX3D(64, 64)

// 64x32
HIGHBD_SADMXN(64, 32)
HIGHBD_SAD_MXNX4D(64, 32)
HIGHBD_SAD_MXNX3D(64, 32)

// 32x64
HIGHBD_SADMXN(32, 64)
HIGHBD_SAD_MXNX4D(32, 64)
HIGHBD_SAD_MXNX3D(32, 64)

// 32x32
HIGHBD_SADMXN(32, 32)
HIGHBD_SAD_MXNX4D(32, 32)
HIGHBD_SAD_MXNX3D(32, 32)

// 32x16
HIGHBD_SADMXN(32, 16)
HIGHBD_SAD_MXNX4D(32, 16)
HIGHBD_SAD_MXNX3D(32, 16)

// 16x32
HIGHBD_SADMXN(16, 32)
HIGHBD_SAD_MXNX4D(16, 32)
HIGHBD_SAD_MXNX3D(16, 32)

// 16x16
HIGHBD_SADMXN(16, 16)
HIGHBD_SAD_MXNX4D(16, 16)
HIGHBD_SAD_MXNX3D(16, 16)

// 16x8
HIGHBD_SADMXN(16, 8)
HIGHBD_SAD_MXNX4D(16, 8)
HIGHBD_SAD_MXNX3D(16, 8)

// 8x16
HIGHBD_SADMXN(8, 16)
HIGHBD_SAD_MXNX4D(8, 16)
HIGHBD_SAD_MXNX3D(8, 16)

// 8x8
HIGHBD_SADMXN(8, 8)
HIGHBD_SAD_MXNX4D(8, 8)
HIGHBD_SAD_MXNX3D(8, 8)

// 8x4
HIGHBD_SADMXN(8, 4)
HIGHBD_SAD_MXNX4D(8, 4)
HIGHBD_SAD_MXNX3D(8, 4)

// 4x8
HIGHBD_SADMXN(4, 8)
HIGHBD_SAD_MXNX4D(4, 8)
HIGHBD_SAD_MXNX3D(4, 8)

// 4x4
HIGHBD_SADMXN(4, 4)
HIGHBD_SAD_MXNX4D(4, 4)
HIGHBD_SAD_MXNX3D(4, 4)

HIGHBD_SADMXN(4, 16)
HIGHBD_SAD_MXNX4D(4, 16)
HIGHBD_SADMXN(16, 4)
HIGHBD_SAD_MXNX4D(16, 4)
HIGHBD_SADMXN(8, 32)
HIGHBD_SAD_MXNX4D(8, 32)
HIGHBD_SADMXN(32, 8)
HIGHBD_SAD_MXNX4D(32, 8)
HIGHBD_SADMXN(16, 64)
HIGHBD_SAD_MXNX4D(16, 64)
HIGHBD_SADMXN(64, 16)
HIGHBD_SAD_MXNX4D(64, 16)

#if !CONFIG_REALTIME_ONLY
HIGHBD_SAD_MXNX3D(4, 16)
HIGHBD_SAD_MXNX3D(16, 4)
HIGHBD_SAD_MXNX3D(8, 32)
HIGHBD_SAD_MXNX3D(32, 8)
HIGHBD_SAD_MXNX3D(16, 64)
HIGHBD_SAD_MXNX3D(64, 16)
#endif  // !CONFIG_REALTIME_ONLY
#endif  // CONFIG_AV1_HIGHBITDEPTH

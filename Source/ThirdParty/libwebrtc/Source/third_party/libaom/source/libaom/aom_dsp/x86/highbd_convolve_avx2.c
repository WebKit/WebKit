/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include <immintrin.h>
#include <string.h>

#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/x86/convolve.h"
#include "aom_dsp/x86/convolve_avx2.h"
#include "aom_dsp/x86/synonyms.h"

// -----------------------------------------------------------------------------
// Copy and average

static const uint8_t ip_shuffle_f2f3[32] = { 0, 1, 2, 3, 2, 3, 4, 5, 4, 5, 6,
                                             7, 6, 7, 8, 9, 0, 1, 2, 3, 2, 3,
                                             4, 5, 4, 5, 6, 7, 6, 7, 8, 9 };
static const uint8_t ip_shuffle_f4f5[32] = { 4, 5, 6,  7,  6,  7,  8,  9,
                                             8, 9, 10, 11, 10, 11, 12, 13,
                                             4, 5, 6,  7,  6,  7,  8,  9,
                                             8, 9, 10, 11, 10, 11, 12, 13 };

void av1_highbd_convolve_x_sr_ssse3(const uint16_t *src, int src_stride,
                                    uint16_t *dst, int dst_stride, int w, int h,
                                    const InterpFilterParams *filter_params_x,
                                    const int subpel_x_qn,
                                    ConvolveParams *conv_params, int bd);
void av1_highbd_convolve_y_sr_ssse3(const uint16_t *src, int src_stride,
                                    uint16_t *dst, int dst_stride, int w, int h,
                                    const InterpFilterParams *filter_params_y,
                                    const int subpel_y_qn, int bd);

void av1_highbd_convolve_y_sr_avx2(const uint16_t *src, int src_stride,
                                   uint16_t *dst, int dst_stride, int w, int h,
                                   const InterpFilterParams *filter_params_y,
                                   const int subpel_y_qn, int bd) {
  if (filter_params_y->taps == 12) {
    av1_highbd_convolve_y_sr_ssse3(src, src_stride, dst, dst_stride, w, h,
                                   filter_params_y, subpel_y_qn, bd);
    return;
  }
  int i, j;
  const int fo_vert = filter_params_y->taps / 2 - 1;
  const uint16_t *const src_ptr = src - fo_vert * src_stride;

  __m256i s[8], coeffs_y[4];

  const int bits = FILTER_BITS;

  const __m128i round_shift_bits = _mm_cvtsi32_si128(bits);
  const __m256i round_const_bits = _mm256_set1_epi32((1 << bits) >> 1);
  const __m256i clip_pixel =
      _mm256_set1_epi16(bd == 10 ? 1023 : (bd == 12 ? 4095 : 255));
  const __m256i zero = _mm256_setzero_si256();

  prepare_coeffs(filter_params_y, subpel_y_qn, coeffs_y);

  for (j = 0; j < w; j += 8) {
    const uint16_t *data = &src_ptr[j];
    /* Vertical filter */
    {
      __m256i src6;
      __m256i s01 = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(
              _mm_loadu_si128((__m128i *)(data + 0 * src_stride))),
          _mm256_castsi128_si256(
              _mm_loadu_si128((__m128i *)(data + 1 * src_stride))),
          0x20);
      __m256i s12 = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(
              _mm_loadu_si128((__m128i *)(data + 1 * src_stride))),
          _mm256_castsi128_si256(
              _mm_loadu_si128((__m128i *)(data + 2 * src_stride))),
          0x20);
      __m256i s23 = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(
              _mm_loadu_si128((__m128i *)(data + 2 * src_stride))),
          _mm256_castsi128_si256(
              _mm_loadu_si128((__m128i *)(data + 3 * src_stride))),
          0x20);
      __m256i s34 = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(
              _mm_loadu_si128((__m128i *)(data + 3 * src_stride))),
          _mm256_castsi128_si256(
              _mm_loadu_si128((__m128i *)(data + 4 * src_stride))),
          0x20);
      __m256i s45 = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(
              _mm_loadu_si128((__m128i *)(data + 4 * src_stride))),
          _mm256_castsi128_si256(
              _mm_loadu_si128((__m128i *)(data + 5 * src_stride))),
          0x20);
      src6 = _mm256_castsi128_si256(
          _mm_loadu_si128((__m128i *)(data + 6 * src_stride)));
      __m256i s56 = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(
              _mm_loadu_si128((__m128i *)(data + 5 * src_stride))),
          src6, 0x20);

      s[0] = _mm256_unpacklo_epi16(s01, s12);
      s[1] = _mm256_unpacklo_epi16(s23, s34);
      s[2] = _mm256_unpacklo_epi16(s45, s56);

      s[4] = _mm256_unpackhi_epi16(s01, s12);
      s[5] = _mm256_unpackhi_epi16(s23, s34);
      s[6] = _mm256_unpackhi_epi16(s45, s56);

      for (i = 0; i < h; i += 2) {
        data = &src_ptr[i * src_stride + j];

        const __m256i s67 = _mm256_permute2x128_si256(
            src6,
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(data + 7 * src_stride))),
            0x20);

        src6 = _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 8 * src_stride)));

        const __m256i s78 = _mm256_permute2x128_si256(
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(data + 7 * src_stride))),
            src6, 0x20);

        s[3] = _mm256_unpacklo_epi16(s67, s78);
        s[7] = _mm256_unpackhi_epi16(s67, s78);

        const __m256i res_a = convolve(s, coeffs_y);

        __m256i res_a_round = _mm256_sra_epi32(
            _mm256_add_epi32(res_a, round_const_bits), round_shift_bits);

        if (w - j > 4) {
          const __m256i res_b = convolve(s + 4, coeffs_y);
          __m256i res_b_round = _mm256_sra_epi32(
              _mm256_add_epi32(res_b, round_const_bits), round_shift_bits);

          __m256i res_16bit = _mm256_packs_epi32(res_a_round, res_b_round);
          res_16bit = _mm256_min_epi16(res_16bit, clip_pixel);
          res_16bit = _mm256_max_epi16(res_16bit, zero);

          _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j],
                           _mm256_castsi256_si128(res_16bit));
          _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j + dst_stride],
                           _mm256_extracti128_si256(res_16bit, 1));
        } else if (w == 4) {
          res_a_round = _mm256_packs_epi32(res_a_round, res_a_round);
          res_a_round = _mm256_min_epi16(res_a_round, clip_pixel);
          res_a_round = _mm256_max_epi16(res_a_round, zero);

          _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j],
                           _mm256_castsi256_si128(res_a_round));
          _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j + dst_stride],
                           _mm256_extracti128_si256(res_a_round, 1));
        } else {
          res_a_round = _mm256_packs_epi32(res_a_round, res_a_round);
          res_a_round = _mm256_min_epi16(res_a_round, clip_pixel);
          res_a_round = _mm256_max_epi16(res_a_round, zero);

          xx_storel_32((__m128i *)&dst[i * dst_stride + j],
                       _mm256_castsi256_si128(res_a_round));
          xx_storel_32((__m128i *)&dst[i * dst_stride + j + dst_stride],
                       _mm256_extracti128_si256(res_a_round, 1));
        }

        s[0] = s[1];
        s[1] = s[2];
        s[2] = s[3];

        s[4] = s[5];
        s[5] = s[6];
        s[6] = s[7];
      }
    }
  }
}

void av1_highbd_convolve_x_sr_avx2(const uint16_t *src, int src_stride,
                                   uint16_t *dst, int dst_stride, int w, int h,
                                   const InterpFilterParams *filter_params_x,
                                   const int subpel_x_qn,
                                   ConvolveParams *conv_params, int bd) {
  if (filter_params_x->taps == 12) {
    av1_highbd_convolve_x_sr_ssse3(src, src_stride, dst, dst_stride, w, h,
                                   filter_params_x, subpel_x_qn, conv_params,
                                   bd);
    return;
  }
  int i, j;
  const int fo_horiz = filter_params_x->taps / 2 - 1;
  const uint16_t *const src_ptr = src - fo_horiz;

  // Check that, even with 12-bit input, the intermediate values will fit
  // into an unsigned 16-bit intermediate array.
  assert(bd + FILTER_BITS + 2 - conv_params->round_0 <= 16);

  __m256i s[4], coeffs_x[4];

  const __m256i round_const_x =
      _mm256_set1_epi32(((1 << conv_params->round_0) >> 1));
  const __m128i round_shift_x = _mm_cvtsi32_si128(conv_params->round_0);

  const int bits = FILTER_BITS - conv_params->round_0;
  const __m128i round_shift_bits = _mm_cvtsi32_si128(bits);
  const __m256i round_const_bits = _mm256_set1_epi32((1 << bits) >> 1);
  const __m256i clip_pixel =
      _mm256_set1_epi16(bd == 10 ? 1023 : (bd == 12 ? 4095 : 255));
  const __m256i zero = _mm256_setzero_si256();

  assert(bits >= 0);
  assert((FILTER_BITS - conv_params->round_1) >= 0 ||
         ((conv_params->round_0 + conv_params->round_1) == 2 * FILTER_BITS));

  prepare_coeffs(filter_params_x, subpel_x_qn, coeffs_x);

  for (j = 0; j < w; j += 8) {
    /* Horizontal filter */
    for (i = 0; i < h; i += 2) {
      const __m256i row0 =
          _mm256_loadu_si256((__m256i *)&src_ptr[i * src_stride + j]);
      __m256i row1 =
          _mm256_loadu_si256((__m256i *)&src_ptr[(i + 1) * src_stride + j]);

      const __m256i r0 = _mm256_permute2x128_si256(row0, row1, 0x20);
      const __m256i r1 = _mm256_permute2x128_si256(row0, row1, 0x31);

      // even pixels
      s[0] = _mm256_alignr_epi8(r1, r0, 0);
      s[1] = _mm256_alignr_epi8(r1, r0, 4);
      s[2] = _mm256_alignr_epi8(r1, r0, 8);
      s[3] = _mm256_alignr_epi8(r1, r0, 12);

      __m256i res_even = convolve(s, coeffs_x);
      res_even = _mm256_sra_epi32(_mm256_add_epi32(res_even, round_const_x),
                                  round_shift_x);

      // odd pixels
      s[0] = _mm256_alignr_epi8(r1, r0, 2);
      s[1] = _mm256_alignr_epi8(r1, r0, 6);
      s[2] = _mm256_alignr_epi8(r1, r0, 10);
      s[3] = _mm256_alignr_epi8(r1, r0, 14);

      __m256i res_odd = convolve(s, coeffs_x);
      res_odd = _mm256_sra_epi32(_mm256_add_epi32(res_odd, round_const_x),
                                 round_shift_x);

      res_even = _mm256_sra_epi32(_mm256_add_epi32(res_even, round_const_bits),
                                  round_shift_bits);
      res_odd = _mm256_sra_epi32(_mm256_add_epi32(res_odd, round_const_bits),
                                 round_shift_bits);

      __m256i res_even1 = _mm256_packs_epi32(res_even, res_even);
      __m256i res_odd1 = _mm256_packs_epi32(res_odd, res_odd);

      __m256i res = _mm256_unpacklo_epi16(res_even1, res_odd1);
      res = _mm256_min_epi16(res, clip_pixel);
      res = _mm256_max_epi16(res, zero);

      if (w - j > 4) {
        _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j],
                         _mm256_castsi256_si128(res));
        _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j + dst_stride],
                         _mm256_extracti128_si256(res, 1));
      } else if (w == 4) {
        _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j],
                         _mm256_castsi256_si128(res));
        _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j + dst_stride],
                         _mm256_extracti128_si256(res, 1));
      } else {
        xx_storel_32((__m128i *)&dst[i * dst_stride + j],
                     _mm256_castsi256_si128(res));
        xx_storel_32((__m128i *)&dst[i * dst_stride + j + dst_stride],
                     _mm256_extracti128_si256(res, 1));
      }
    }
  }
}

#define CONV8_ROUNDING_BITS (7)

// -----------------------------------------------------------------------------
// Horizontal and vertical filtering

static const uint8_t signal_pattern_0[32] = { 0, 1, 2, 3, 2, 3, 4, 5, 4, 5, 6,
                                              7, 6, 7, 8, 9, 0, 1, 2, 3, 2, 3,
                                              4, 5, 4, 5, 6, 7, 6, 7, 8, 9 };

static const uint8_t signal_pattern_1[32] = { 4, 5, 6,  7,  6,  7,  8,  9,
                                              8, 9, 10, 11, 10, 11, 12, 13,
                                              4, 5, 6,  7,  6,  7,  8,  9,
                                              8, 9, 10, 11, 10, 11, 12, 13 };

static const uint8_t signal_pattern_2[32] = { 6,  7,  8,  9,  8,  9,  10, 11,
                                              10, 11, 12, 13, 12, 13, 14, 15,
                                              6,  7,  8,  9,  8,  9,  10, 11,
                                              10, 11, 12, 13, 12, 13, 14, 15 };

static const uint32_t signal_index[8] = { 2, 3, 4, 5, 2, 3, 4, 5 };

// -----------------------------------------------------------------------------
// Horizontal Filtering

static INLINE void pack_pixels(const __m256i *s, __m256i *p /*p[4]*/) {
  const __m256i idx = _mm256_loadu_si256((const __m256i *)signal_index);
  const __m256i sf0 = _mm256_loadu_si256((const __m256i *)signal_pattern_0);
  const __m256i sf1 = _mm256_loadu_si256((const __m256i *)signal_pattern_1);
  const __m256i c = _mm256_permutevar8x32_epi32(*s, idx);

  p[0] = _mm256_shuffle_epi8(*s, sf0);  // x0x6
  p[1] = _mm256_shuffle_epi8(*s, sf1);  // x1x7
  p[2] = _mm256_shuffle_epi8(c, sf0);   // x2x4
  p[3] = _mm256_shuffle_epi8(c, sf1);   // x3x5
}

// Note:
//  Shared by 8x2 and 16x1 block
static INLINE void pack_16_pixels(const __m256i *s0, const __m256i *s1,
                                  __m256i *x /*x[8]*/) {
  __m256i pp[8];
  pack_pixels(s0, pp);
  pack_pixels(s1, &pp[4]);
  x[0] = _mm256_permute2x128_si256(pp[0], pp[4], 0x20);
  x[1] = _mm256_permute2x128_si256(pp[1], pp[5], 0x20);
  x[2] = _mm256_permute2x128_si256(pp[2], pp[6], 0x20);
  x[3] = _mm256_permute2x128_si256(pp[3], pp[7], 0x20);
  x[4] = x[2];
  x[5] = x[3];
  x[6] = _mm256_permute2x128_si256(pp[0], pp[4], 0x31);
  x[7] = _mm256_permute2x128_si256(pp[1], pp[5], 0x31);
}

static INLINE void pack_8x1_pixels(const uint16_t *src, __m256i *x) {
  __m256i pp[8];
  __m256i s0;
  s0 = _mm256_loadu_si256((const __m256i *)src);
  pack_pixels(&s0, pp);
  x[0] = _mm256_permute2x128_si256(pp[0], pp[2], 0x30);
  x[1] = _mm256_permute2x128_si256(pp[1], pp[3], 0x30);
  x[2] = _mm256_permute2x128_si256(pp[2], pp[0], 0x30);
  x[3] = _mm256_permute2x128_si256(pp[3], pp[1], 0x30);
}

static INLINE void pack_8x2_pixels(const uint16_t *src, ptrdiff_t stride,
                                   __m256i *x) {
  __m256i s0, s1;
  s0 = _mm256_loadu_si256((const __m256i *)src);
  s1 = _mm256_loadu_si256((const __m256i *)(src + stride));
  pack_16_pixels(&s0, &s1, x);
}

static INLINE void pack_16x1_pixels(const uint16_t *src, __m256i *x) {
  __m256i s0, s1;
  s0 = _mm256_loadu_si256((const __m256i *)src);
  s1 = _mm256_loadu_si256((const __m256i *)(src + 8));
  pack_16_pixels(&s0, &s1, x);
}

// Note:
//  Shared by horizontal and vertical filtering
static INLINE void pack_filters(const int16_t *filter, __m256i *f /*f[4]*/) {
  const __m128i h = _mm_loadu_si128((const __m128i *)filter);
  const __m256i hh = _mm256_insertf128_si256(_mm256_castsi128_si256(h), h, 1);
  const __m256i p0 = _mm256_set1_epi32(0x03020100);
  const __m256i p1 = _mm256_set1_epi32(0x07060504);
  const __m256i p2 = _mm256_set1_epi32(0x0b0a0908);
  const __m256i p3 = _mm256_set1_epi32(0x0f0e0d0c);
  f[0] = _mm256_shuffle_epi8(hh, p0);
  f[1] = _mm256_shuffle_epi8(hh, p1);
  f[2] = _mm256_shuffle_epi8(hh, p2);
  f[3] = _mm256_shuffle_epi8(hh, p3);
}

static INLINE void pack_filters_4tap(const int16_t *filter,
                                     __m256i *f /*f[4]*/) {
  const __m128i h = _mm_loadu_si128((const __m128i *)filter);
  const __m256i coeff = _mm256_broadcastsi128_si256(h);

  // coeffs 2 3 2 3 2 3 2 3
  f[0] = _mm256_shuffle_epi32(coeff, 0x55);
  // coeffs 4 5 4 5 4 5 4 5
  f[1] = _mm256_shuffle_epi32(coeff, 0xaa);
}

static INLINE void filter_8x1_pixels(const __m256i *sig /*sig[4]*/,
                                     const __m256i *fil /*fil[4]*/,
                                     __m256i *y) {
  __m256i a, a0, a1;

  a0 = _mm256_madd_epi16(fil[0], sig[0]);
  a1 = _mm256_madd_epi16(fil[3], sig[3]);
  a = _mm256_add_epi32(a0, a1);

  a0 = _mm256_madd_epi16(fil[1], sig[1]);
  a1 = _mm256_madd_epi16(fil[2], sig[2]);

  {
    const __m256i min = _mm256_min_epi32(a0, a1);
    a = _mm256_add_epi32(a, min);
  }
  {
    const __m256i max = _mm256_max_epi32(a0, a1);
    a = _mm256_add_epi32(a, max);
  }
  {
    const __m256i rounding = _mm256_set1_epi32(1 << (CONV8_ROUNDING_BITS - 1));
    a = _mm256_add_epi32(a, rounding);
    *y = _mm256_srai_epi32(a, CONV8_ROUNDING_BITS);
  }
}

static INLINE void store_8x1_pixels(const __m256i *y, const __m256i *mask,
                                    uint16_t *dst) {
  const __m128i a0 = _mm256_castsi256_si128(*y);
  const __m128i a1 = _mm256_extractf128_si256(*y, 1);
  __m128i res = _mm_packus_epi32(a0, a1);
  res = _mm_min_epi16(res, _mm256_castsi256_si128(*mask));
  _mm_storeu_si128((__m128i *)dst, res);
}

static INLINE void store_8x2_pixels(const __m256i *y0, const __m256i *y1,
                                    const __m256i *mask, uint16_t *dst,
                                    ptrdiff_t pitch) {
  __m256i a = _mm256_packus_epi32(*y0, *y1);
  a = _mm256_min_epi16(a, *mask);
  _mm_storeu_si128((__m128i *)dst, _mm256_castsi256_si128(a));
  _mm_storeu_si128((__m128i *)(dst + pitch), _mm256_extractf128_si256(a, 1));
}

static INLINE void store_16x1_pixels(const __m256i *y0, const __m256i *y1,
                                     const __m256i *mask, uint16_t *dst) {
  __m256i a = _mm256_packus_epi32(*y0, *y1);
  a = _mm256_min_epi16(a, *mask);
  _mm256_storeu_si256((__m256i *)dst, a);
}

static void aom_highbd_filter_block1d8_h8_avx2(
    const uint16_t *src_ptr, ptrdiff_t src_pitch, uint16_t *dst_ptr,
    ptrdiff_t dst_pitch, uint32_t height, const int16_t *filter, int bd) {
  __m256i signal[8], res0, res1;
  const __m256i max = _mm256_set1_epi16((1 << bd) - 1);

  __m256i ff[4];
  pack_filters(filter, ff);

  src_ptr -= 3;
  do {
    pack_8x2_pixels(src_ptr, src_pitch, signal);
    filter_8x1_pixels(signal, ff, &res0);
    filter_8x1_pixels(&signal[4], ff, &res1);
    store_8x2_pixels(&res0, &res1, &max, dst_ptr, dst_pitch);
    height -= 2;
    src_ptr += src_pitch << 1;
    dst_ptr += dst_pitch << 1;
  } while (height > 1);

  if (height > 0) {
    pack_8x1_pixels(src_ptr, signal);
    filter_8x1_pixels(signal, ff, &res0);
    store_8x1_pixels(&res0, &max, dst_ptr);
  }
}

static void aom_highbd_filter_block1d16_h8_avx2(
    const uint16_t *src_ptr, ptrdiff_t src_pitch, uint16_t *dst_ptr,
    ptrdiff_t dst_pitch, uint32_t height, const int16_t *filter, int bd) {
  __m256i signal[8], res0, res1;
  const __m256i max = _mm256_set1_epi16((1 << bd) - 1);

  __m256i ff[4];
  pack_filters(filter, ff);

  src_ptr -= 3;
  do {
    pack_16x1_pixels(src_ptr, signal);
    filter_8x1_pixels(signal, ff, &res0);
    filter_8x1_pixels(&signal[4], ff, &res1);
    store_16x1_pixels(&res0, &res1, &max, dst_ptr);
    height -= 1;
    src_ptr += src_pitch;
    dst_ptr += dst_pitch;
  } while (height > 0);
}

static void aom_highbd_filter_block1d4_h4_avx2(
    const uint16_t *src_ptr, ptrdiff_t src_pitch, uint16_t *dst_ptr,
    ptrdiff_t dst_pitch, uint32_t height, const int16_t *filter, int bd) {
  const __m256i rounding = _mm256_set1_epi32(1 << (CONV8_ROUNDING_BITS - 1));
  __m256i ff[2], s[2];
  uint32_t i;
  const __m256i clip_pixel =
      _mm256_set1_epi16(bd == 10 ? 1023 : (bd == 12 ? 4095 : 255));
  const __m256i zero = _mm256_setzero_si256();

  static const uint8_t shuffle_mask[32] = { 0, 1, 2, 3, 2, 3, 4, 5, 4, 5, 6,
                                            7, 6, 7, 8, 9, 0, 1, 2, 3, 2, 3,
                                            4, 5, 4, 5, 6, 7, 6, 7, 8, 9 };

  __m256i mask = _mm256_loadu_si256((__m256i *)shuffle_mask);
  __m256i ip_mask_f2f3 = _mm256_loadu_si256((__m256i *)ip_shuffle_f2f3);
  __m256i ip_mask_f4f5 = _mm256_loadu_si256((__m256i *)ip_shuffle_f4f5);

  pack_filters_4tap(filter, ff);
  src_ptr -= 3;
  for (i = 0; i <= (height - 2); i += 2) {
    __m256i row0 = _mm256_castsi128_si256(
        _mm_loadu_si128((__m128i *)&src_ptr[i * src_pitch + 2]));
    __m256i row1 = _mm256_castsi128_si256(
        _mm_loadu_si128((__m128i *)&src_ptr[(i + 1) * src_pitch + 2]));

    s[0] = _mm256_inserti128_si256(row0, _mm256_castsi256_si128(row1), 1);
    s[1] = _mm256_alignr_epi8(s[0], s[0], 4);

    s[0] = _mm256_shuffle_epi8(s[0], mask);
    s[1] = _mm256_shuffle_epi8(s[1], mask);

    __m256i res = convolve_4tap(s, ff);
    res =
        _mm256_srai_epi32(_mm256_add_epi32(res, rounding), CONV8_ROUNDING_BITS);

    res = _mm256_packs_epi32(res, res);
    res = _mm256_min_epi16(res, clip_pixel);
    res = _mm256_max_epi16(res, zero);

    _mm_storel_epi64((__m128i *)&dst_ptr[i * dst_pitch],
                     _mm256_castsi256_si128(res));
    _mm_storel_epi64((__m128i *)&dst_ptr[(i + 1) * dst_pitch],
                     _mm256_extracti128_si256(res, 1));
  }
  if (height % 2 != 0) {
    i = height - 1;
    const __m256i row0_0 = _mm256_castsi128_si256(
        _mm_loadu_si128((__m128i *)&src_ptr[i * src_pitch + 2]));
    const __m256i row0_1 = _mm256_castsi128_si256(
        _mm_loadu_si128((__m128i *)&src_ptr[i * src_pitch + 6]));

    const __m256i r0 =
        _mm256_inserti128_si256(row0_0, _mm256_castsi256_si128(row0_1), 1);

    s[0] = _mm256_shuffle_epi8(r0, ip_mask_f2f3);
    s[1] = _mm256_shuffle_epi8(r0, ip_mask_f4f5);

    __m256i res = convolve_4tap(s, ff);
    res =
        _mm256_srai_epi32(_mm256_add_epi32(res, rounding), CONV8_ROUNDING_BITS);

    res = _mm256_packs_epi32(res, res);
    res = _mm256_min_epi16(res, clip_pixel);
    res = _mm256_max_epi16(res, zero);

    _mm_storel_epi64((__m128i *)&dst_ptr[i * dst_pitch],
                     _mm256_castsi256_si128(res));
  }
}

static void aom_highbd_filter_block1d8_h4_avx2(
    const uint16_t *src_ptr, ptrdiff_t src_pitch, uint16_t *dst_ptr,
    ptrdiff_t dst_pitch, uint32_t height, const int16_t *filter, int bd) {
  const __m256i rounding = _mm256_set1_epi32(1 << (CONV8_ROUNDING_BITS - 1));
  __m256i ff[2], s[2];
  uint32_t i = 0;
  const __m256i clip_pixel =
      _mm256_set1_epi16(bd == 10 ? 1023 : (bd == 12 ? 4095 : 255));
  const __m256i zero = _mm256_setzero_si256();

  static const uint8_t shuffle_mask[32] = { 0, 1, 8,  9,  2, 3, 10, 11,
                                            4, 5, 12, 13, 6, 7, 14, 15,
                                            0, 1, 8,  9,  2, 3, 10, 11,
                                            4, 5, 12, 13, 6, 7, 14, 15 };

  __m256i mask = _mm256_loadu_si256((__m256i *)shuffle_mask);
  __m256i ip_mask_f2f3 = _mm256_loadu_si256((__m256i *)ip_shuffle_f2f3);
  __m256i ip_mask_f4f5 = _mm256_loadu_si256((__m256i *)ip_shuffle_f4f5);

  pack_filters_4tap(filter, ff);
  src_ptr -= 3;

  /* Horizontal filter */

  for (i = 0; i <= (height - 2); i += 2) {
    const __m256i row0 =
        _mm256_loadu_si256((__m256i *)&src_ptr[i * src_pitch + 2]);
    __m256i row1 =
        _mm256_loadu_si256((__m256i *)&src_ptr[(i + 1) * src_pitch + 2]);

    const __m256i r0 =
        _mm256_inserti128_si256(row0, _mm256_castsi256_si128(row1), 1);
    const __m256i r1 = _mm256_permute2x128_si256(row0, row1, 0x31);

    // even pixels
    s[0] = r0;
    s[1] = _mm256_alignr_epi8(r1, r0, 4);

    __m256i res_even = convolve_4tap(s, ff);
    res_even = _mm256_srai_epi32(_mm256_add_epi32(res_even, rounding),
                                 CONV8_ROUNDING_BITS);

    // odd pixels
    s[0] = _mm256_alignr_epi8(r1, r0, 2);
    s[1] = _mm256_alignr_epi8(r1, r0, 6);

    __m256i res_odd = convolve_4tap(s, ff);
    res_odd = _mm256_srai_epi32(_mm256_add_epi32(res_odd, rounding),
                                CONV8_ROUNDING_BITS);

    __m256i res = _mm256_packs_epi32(res_even, res_odd);
    res = _mm256_shuffle_epi8(res, mask);

    res = _mm256_min_epi16(res, clip_pixel);
    res = _mm256_max_epi16(res, zero);

    _mm_storeu_si128((__m128i *)&dst_ptr[i * dst_pitch],
                     _mm256_castsi256_si128(res));
    _mm_storeu_si128((__m128i *)&dst_ptr[i * dst_pitch + dst_pitch],
                     _mm256_extracti128_si256(res, 1));
  }

  if (height % 2 != 0) {
    i = height - 1;
    const __m256i row0_0 =
        _mm256_loadu_si256((__m256i *)&src_ptr[i * src_pitch + 2]);
    const __m256i row0_1 =
        _mm256_loadu_si256((__m256i *)&src_ptr[i * src_pitch + 6]);

    const __m256i r0 =
        _mm256_inserti128_si256(row0_0, _mm256_castsi256_si128(row0_1), 1);

    s[0] = _mm256_shuffle_epi8(r0, ip_mask_f2f3);
    s[1] = _mm256_shuffle_epi8(r0, ip_mask_f4f5);

    __m256i res = convolve_4tap(s, ff);
    res =
        _mm256_srai_epi32(_mm256_add_epi32(res, rounding), CONV8_ROUNDING_BITS);

    res = _mm256_packs_epi32(res, res);
    res = _mm256_min_epi16(res, clip_pixel);
    res = _mm256_max_epi16(res, zero);

    _mm_storel_epi64((__m128i *)&dst_ptr[i * dst_pitch],
                     _mm256_castsi256_si128(res));
    _mm_storel_epi64((__m128i *)&dst_ptr[i * dst_pitch + 4],
                     _mm256_extracti128_si256(res, 1));
  }
}

static void aom_highbd_filter_block1d16_h4_avx2(
    const uint16_t *src_ptr, ptrdiff_t src_pitch, uint16_t *dst_ptr,
    ptrdiff_t dst_pitch, uint32_t height, const int16_t *filter, int bd) {
  aom_highbd_filter_block1d8_h4_avx2(src_ptr, src_pitch, dst_ptr, dst_pitch,
                                     height, filter, bd);
  aom_highbd_filter_block1d8_h4_avx2(src_ptr + 8, src_pitch, dst_ptr + 8,
                                     dst_pitch, height, filter, bd);
}

// -----------------------------------------------------------------------------
// 2-tap horizontal filtering

static INLINE void pack_2t_filter(const int16_t *filter, __m256i *f) {
  const __m128i h = _mm_loadu_si128((const __m128i *)filter);
  const __m256i hh = _mm256_insertf128_si256(_mm256_castsi128_si256(h), h, 1);
  const __m256i p = _mm256_set1_epi32(0x09080706);
  f[0] = _mm256_shuffle_epi8(hh, p);
}

// can be used by pack_8x2_2t_pixels() and pack_16x1_2t_pixels()
// the difference is s0/s1 specifies first and second rows or,
// first 16 samples and 8-sample shifted 16 samples
static INLINE void pack_16_2t_pixels(const __m256i *s0, const __m256i *s1,
                                     __m256i *sig) {
  const __m256i idx = _mm256_loadu_si256((const __m256i *)signal_index);
  const __m256i sf2 = _mm256_loadu_si256((const __m256i *)signal_pattern_2);
  __m256i x0 = _mm256_shuffle_epi8(*s0, sf2);
  __m256i x1 = _mm256_shuffle_epi8(*s1, sf2);
  __m256i r0 = _mm256_permutevar8x32_epi32(*s0, idx);
  __m256i r1 = _mm256_permutevar8x32_epi32(*s1, idx);
  r0 = _mm256_shuffle_epi8(r0, sf2);
  r1 = _mm256_shuffle_epi8(r1, sf2);
  sig[0] = _mm256_permute2x128_si256(x0, x1, 0x20);
  sig[1] = _mm256_permute2x128_si256(r0, r1, 0x20);
}

static INLINE void pack_8x2_2t_pixels(const uint16_t *src,
                                      const ptrdiff_t pitch, __m256i *sig) {
  const __m256i r0 = _mm256_loadu_si256((const __m256i *)src);
  const __m256i r1 = _mm256_loadu_si256((const __m256i *)(src + pitch));
  pack_16_2t_pixels(&r0, &r1, sig);
}

static INLINE void pack_16x1_2t_pixels(const uint16_t *src,
                                       __m256i *sig /*sig[2]*/) {
  const __m256i r0 = _mm256_loadu_si256((const __m256i *)src);
  const __m256i r1 = _mm256_loadu_si256((const __m256i *)(src + 8));
  pack_16_2t_pixels(&r0, &r1, sig);
}

static INLINE void pack_8x1_2t_pixels(const uint16_t *src,
                                      __m256i *sig /*sig[2]*/) {
  const __m256i idx = _mm256_loadu_si256((const __m256i *)signal_index);
  const __m256i sf2 = _mm256_loadu_si256((const __m256i *)signal_pattern_2);
  __m256i r0 = _mm256_loadu_si256((const __m256i *)src);
  __m256i x0 = _mm256_shuffle_epi8(r0, sf2);
  r0 = _mm256_permutevar8x32_epi32(r0, idx);
  r0 = _mm256_shuffle_epi8(r0, sf2);
  sig[0] = _mm256_permute2x128_si256(x0, r0, 0x20);
}

// can be used by filter_8x2_2t_pixels() and filter_16x1_2t_pixels()
static INLINE void filter_16_2t_pixels(const __m256i *sig, const __m256i *f,
                                       __m256i *y0, __m256i *y1) {
  const __m256i rounding = _mm256_set1_epi32(1 << (CONV8_ROUNDING_BITS - 1));
  __m256i x0 = _mm256_madd_epi16(sig[0], *f);
  __m256i x1 = _mm256_madd_epi16(sig[1], *f);
  x0 = _mm256_add_epi32(x0, rounding);
  x1 = _mm256_add_epi32(x1, rounding);
  *y0 = _mm256_srai_epi32(x0, CONV8_ROUNDING_BITS);
  *y1 = _mm256_srai_epi32(x1, CONV8_ROUNDING_BITS);
}

static INLINE void filter_8x1_2t_pixels(const __m256i *sig, const __m256i *f,
                                        __m256i *y0) {
  const __m256i rounding = _mm256_set1_epi32(1 << (CONV8_ROUNDING_BITS - 1));
  __m256i x0 = _mm256_madd_epi16(sig[0], *f);
  x0 = _mm256_add_epi32(x0, rounding);
  *y0 = _mm256_srai_epi32(x0, CONV8_ROUNDING_BITS);
}

static void aom_highbd_filter_block1d8_h2_avx2(
    const uint16_t *src_ptr, ptrdiff_t src_pitch, uint16_t *dst_ptr,
    ptrdiff_t dst_pitch, uint32_t height, const int16_t *filter, int bd) {
  __m256i signal[2], res0, res1;
  const __m256i max = _mm256_set1_epi16((1 << bd) - 1);

  __m256i ff;
  pack_2t_filter(filter, &ff);

  src_ptr -= 3;
  do {
    pack_8x2_2t_pixels(src_ptr, src_pitch, signal);
    filter_16_2t_pixels(signal, &ff, &res0, &res1);
    store_8x2_pixels(&res0, &res1, &max, dst_ptr, dst_pitch);
    height -= 2;
    src_ptr += src_pitch << 1;
    dst_ptr += dst_pitch << 1;
  } while (height > 1);

  if (height > 0) {
    pack_8x1_2t_pixels(src_ptr, signal);
    filter_8x1_2t_pixels(signal, &ff, &res0);
    store_8x1_pixels(&res0, &max, dst_ptr);
  }
}

static void aom_highbd_filter_block1d16_h2_avx2(
    const uint16_t *src_ptr, ptrdiff_t src_pitch, uint16_t *dst_ptr,
    ptrdiff_t dst_pitch, uint32_t height, const int16_t *filter, int bd) {
  __m256i signal[2], res0, res1;
  const __m256i max = _mm256_set1_epi16((1 << bd) - 1);

  __m256i ff;
  pack_2t_filter(filter, &ff);

  src_ptr -= 3;
  do {
    pack_16x1_2t_pixels(src_ptr, signal);
    filter_16_2t_pixels(signal, &ff, &res0, &res1);
    store_16x1_pixels(&res0, &res1, &max, dst_ptr);
    height -= 1;
    src_ptr += src_pitch;
    dst_ptr += dst_pitch;
  } while (height > 0);
}

// -----------------------------------------------------------------------------
// Vertical Filtering

static void pack_8x9_init(const uint16_t *src, ptrdiff_t pitch, __m256i *sig) {
  __m256i s0 = _mm256_castsi128_si256(_mm_loadu_si128((const __m128i *)src));
  __m256i s1 =
      _mm256_castsi128_si256(_mm_loadu_si128((const __m128i *)(src + pitch)));
  __m256i s2 = _mm256_castsi128_si256(
      _mm_loadu_si128((const __m128i *)(src + 2 * pitch)));
  __m256i s3 = _mm256_castsi128_si256(
      _mm_loadu_si128((const __m128i *)(src + 3 * pitch)));
  __m256i s4 = _mm256_castsi128_si256(
      _mm_loadu_si128((const __m128i *)(src + 4 * pitch)));
  __m256i s5 = _mm256_castsi128_si256(
      _mm_loadu_si128((const __m128i *)(src + 5 * pitch)));
  __m256i s6 = _mm256_castsi128_si256(
      _mm_loadu_si128((const __m128i *)(src + 6 * pitch)));

  s0 = _mm256_inserti128_si256(s0, _mm256_castsi256_si128(s1), 1);
  s1 = _mm256_inserti128_si256(s1, _mm256_castsi256_si128(s2), 1);
  s2 = _mm256_inserti128_si256(s2, _mm256_castsi256_si128(s3), 1);
  s3 = _mm256_inserti128_si256(s3, _mm256_castsi256_si128(s4), 1);
  s4 = _mm256_inserti128_si256(s4, _mm256_castsi256_si128(s5), 1);
  s5 = _mm256_inserti128_si256(s5, _mm256_castsi256_si128(s6), 1);

  sig[0] = _mm256_unpacklo_epi16(s0, s1);
  sig[4] = _mm256_unpackhi_epi16(s0, s1);
  sig[1] = _mm256_unpacklo_epi16(s2, s3);
  sig[5] = _mm256_unpackhi_epi16(s2, s3);
  sig[2] = _mm256_unpacklo_epi16(s4, s5);
  sig[6] = _mm256_unpackhi_epi16(s4, s5);
  sig[8] = s6;
}

static INLINE void pack_8x9_pixels(const uint16_t *src, ptrdiff_t pitch,
                                   __m256i *sig) {
  // base + 7th row
  __m256i s0 = _mm256_castsi128_si256(
      _mm_loadu_si128((const __m128i *)(src + 7 * pitch)));
  // base + 8th row
  __m256i s1 = _mm256_castsi128_si256(
      _mm_loadu_si128((const __m128i *)(src + 8 * pitch)));
  __m256i s2 = _mm256_inserti128_si256(sig[8], _mm256_castsi256_si128(s0), 1);
  __m256i s3 = _mm256_inserti128_si256(s0, _mm256_castsi256_si128(s1), 1);
  sig[3] = _mm256_unpacklo_epi16(s2, s3);
  sig[7] = _mm256_unpackhi_epi16(s2, s3);
  sig[8] = s1;
}

static INLINE void filter_8x9_pixels(const __m256i *sig, const __m256i *f,
                                     __m256i *y0, __m256i *y1) {
  filter_8x1_pixels(sig, f, y0);
  filter_8x1_pixels(&sig[4], f, y1);
}

static INLINE void update_pixels(__m256i *sig) {
  int i;
  for (i = 0; i < 3; ++i) {
    sig[i] = sig[i + 1];
    sig[i + 4] = sig[i + 5];
  }
}

static void aom_highbd_filter_block1d8_v8_avx2(
    const uint16_t *src_ptr, ptrdiff_t src_pitch, uint16_t *dst_ptr,
    ptrdiff_t dst_pitch, uint32_t height, const int16_t *filter, int bd) {
  __m256i signal[9], res0, res1;
  const __m256i max = _mm256_set1_epi16((1 << bd) - 1);

  __m256i ff[4];
  pack_filters(filter, ff);

  pack_8x9_init(src_ptr, src_pitch, signal);

  do {
    pack_8x9_pixels(src_ptr, src_pitch, signal);

    filter_8x9_pixels(signal, ff, &res0, &res1);
    store_8x2_pixels(&res0, &res1, &max, dst_ptr, dst_pitch);
    update_pixels(signal);

    src_ptr += src_pitch << 1;
    dst_ptr += dst_pitch << 1;
    height -= 2;
  } while (height > 0);
}

static void pack_16x9_init(const uint16_t *src, ptrdiff_t pitch, __m256i *sig) {
  __m256i u0, u1, u2, u3;
  // load 0-6 rows
  const __m256i s0 = _mm256_loadu_si256((const __m256i *)src);
  const __m256i s1 = _mm256_loadu_si256((const __m256i *)(src + pitch));
  const __m256i s2 = _mm256_loadu_si256((const __m256i *)(src + 2 * pitch));
  const __m256i s3 = _mm256_loadu_si256((const __m256i *)(src + 3 * pitch));
  const __m256i s4 = _mm256_loadu_si256((const __m256i *)(src + 4 * pitch));
  const __m256i s5 = _mm256_loadu_si256((const __m256i *)(src + 5 * pitch));
  const __m256i s6 = _mm256_loadu_si256((const __m256i *)(src + 6 * pitch));

  u0 = _mm256_permute2x128_si256(s0, s1, 0x20);  // 0, 1 low
  u1 = _mm256_permute2x128_si256(s0, s1, 0x31);  // 0, 1 high

  u2 = _mm256_permute2x128_si256(s1, s2, 0x20);  // 1, 2 low
  u3 = _mm256_permute2x128_si256(s1, s2, 0x31);  // 1, 2 high

  sig[0] = _mm256_unpacklo_epi16(u0, u2);
  sig[4] = _mm256_unpackhi_epi16(u0, u2);

  sig[8] = _mm256_unpacklo_epi16(u1, u3);
  sig[12] = _mm256_unpackhi_epi16(u1, u3);

  u0 = _mm256_permute2x128_si256(s2, s3, 0x20);
  u1 = _mm256_permute2x128_si256(s2, s3, 0x31);

  u2 = _mm256_permute2x128_si256(s3, s4, 0x20);
  u3 = _mm256_permute2x128_si256(s3, s4, 0x31);

  sig[1] = _mm256_unpacklo_epi16(u0, u2);
  sig[5] = _mm256_unpackhi_epi16(u0, u2);

  sig[9] = _mm256_unpacklo_epi16(u1, u3);
  sig[13] = _mm256_unpackhi_epi16(u1, u3);

  u0 = _mm256_permute2x128_si256(s4, s5, 0x20);
  u1 = _mm256_permute2x128_si256(s4, s5, 0x31);

  u2 = _mm256_permute2x128_si256(s5, s6, 0x20);
  u3 = _mm256_permute2x128_si256(s5, s6, 0x31);

  sig[2] = _mm256_unpacklo_epi16(u0, u2);
  sig[6] = _mm256_unpackhi_epi16(u0, u2);

  sig[10] = _mm256_unpacklo_epi16(u1, u3);
  sig[14] = _mm256_unpackhi_epi16(u1, u3);

  sig[16] = s6;
}

static void pack_16x9_pixels(const uint16_t *src, ptrdiff_t pitch,
                             __m256i *sig) {
  // base + 7th row
  const __m256i s7 = _mm256_loadu_si256((const __m256i *)(src + 7 * pitch));
  // base + 8th row
  const __m256i s8 = _mm256_loadu_si256((const __m256i *)(src + 8 * pitch));

  __m256i u0, u1, u2, u3;
  u0 = _mm256_permute2x128_si256(sig[16], s7, 0x20);
  u1 = _mm256_permute2x128_si256(sig[16], s7, 0x31);

  u2 = _mm256_permute2x128_si256(s7, s8, 0x20);
  u3 = _mm256_permute2x128_si256(s7, s8, 0x31);

  sig[3] = _mm256_unpacklo_epi16(u0, u2);
  sig[7] = _mm256_unpackhi_epi16(u0, u2);

  sig[11] = _mm256_unpacklo_epi16(u1, u3);
  sig[15] = _mm256_unpackhi_epi16(u1, u3);

  sig[16] = s8;
}

static INLINE void filter_16x9_pixels(const __m256i *sig, const __m256i *f,
                                      __m256i *y0, __m256i *y1) {
  __m256i res[4];
  int i;
  for (i = 0; i < 4; ++i) {
    filter_8x1_pixels(&sig[i << 2], f, &res[i]);
  }

  {
    const __m256i l0l1 = _mm256_packus_epi32(res[0], res[1]);
    const __m256i h0h1 = _mm256_packus_epi32(res[2], res[3]);
    *y0 = _mm256_permute2x128_si256(l0l1, h0h1, 0x20);
    *y1 = _mm256_permute2x128_si256(l0l1, h0h1, 0x31);
  }
}

static INLINE void store_16x2_pixels(const __m256i *y0, const __m256i *y1,
                                     const __m256i *mask, uint16_t *dst,
                                     ptrdiff_t pitch) {
  __m256i p = _mm256_min_epi16(*y0, *mask);
  _mm256_storeu_si256((__m256i *)dst, p);
  p = _mm256_min_epi16(*y1, *mask);
  _mm256_storeu_si256((__m256i *)(dst + pitch), p);
}

static void update_16x9_pixels(__m256i *sig) {
  update_pixels(&sig[0]);
  update_pixels(&sig[8]);
}

static void aom_highbd_filter_block1d16_v8_avx2(
    const uint16_t *src_ptr, ptrdiff_t src_pitch, uint16_t *dst_ptr,
    ptrdiff_t dst_pitch, uint32_t height, const int16_t *filter, int bd) {
  __m256i signal[17], res0, res1;
  const __m256i max = _mm256_set1_epi16((1 << bd) - 1);

  __m256i ff[4];
  pack_filters(filter, ff);

  pack_16x9_init(src_ptr, src_pitch, signal);

  do {
    pack_16x9_pixels(src_ptr, src_pitch, signal);
    filter_16x9_pixels(signal, ff, &res0, &res1);
    store_16x2_pixels(&res0, &res1, &max, dst_ptr, dst_pitch);
    update_16x9_pixels(signal);

    src_ptr += src_pitch << 1;
    dst_ptr += dst_pitch << 1;
    height -= 2;
  } while (height > 0);
}

static void aom_highbd_filter_block1d4_v4_avx2(
    const uint16_t *src_ptr, ptrdiff_t src_pitch, uint16_t *dst_ptr,
    ptrdiff_t dst_pitch, uint32_t height, const int16_t *filter, int bd) {
  const int bits = FILTER_BITS;

  const __m128i round_shift_bits = _mm_cvtsi32_si128(bits);
  const __m256i round_const_bits = _mm256_set1_epi32((1 << bits) >> 1);
  const __m256i clip_pixel =
      _mm256_set1_epi32(bd == 10 ? 1023 : (bd == 12 ? 4095 : 255));
  const __m256i zero = _mm256_setzero_si256();
  uint32_t i;
  __m256i s[2], ff[2];

  pack_filters_4tap(filter, ff);

  const uint16_t *data = src_ptr;
  /* Vertical filter */
  {
    __m128i s2 = _mm_loadl_epi64((__m128i *)(data + 2 * src_pitch));
    __m128i s3 = _mm_loadl_epi64((__m128i *)(data + 3 * src_pitch));

    __m256i s23 = _mm256_inserti128_si256(_mm256_castsi128_si256(s2), s3, 1);

    __m128i s4 = _mm_loadl_epi64((__m128i *)(data + 4 * src_pitch));

    __m256i s34 = _mm256_inserti128_si256(_mm256_castsi128_si256(s3), s4, 1);

    s[0] = _mm256_unpacklo_epi16(s23, s34);

    for (i = 0; i < height; i += 2) {
      data = &src_ptr[i * src_pitch];

      __m128i s5 = _mm_loadl_epi64((__m128i *)(data + 5 * src_pitch));
      __m128i s6 = _mm_loadl_epi64((__m128i *)(data + 6 * src_pitch));

      __m256i s45 = _mm256_inserti128_si256(_mm256_castsi128_si256(s4), s5, 1);
      __m256i s56 = _mm256_inserti128_si256(_mm256_castsi128_si256(s5), s6, 1);

      s[1] = _mm256_unpacklo_epi16(s45, s56);

      const __m256i res_a = convolve_4tap(s, ff);

      __m256i res_a_round = _mm256_sra_epi32(
          _mm256_add_epi32(res_a, round_const_bits), round_shift_bits);

      __m256i res_16bit = _mm256_min_epi32(res_a_round, clip_pixel);
      res_16bit = _mm256_max_epi32(res_16bit, zero);
      res_16bit = _mm256_packs_epi32(res_16bit, res_16bit);

      _mm_storel_epi64((__m128i *)&dst_ptr[i * dst_pitch],
                       _mm256_castsi256_si128(res_16bit));
      _mm_storel_epi64((__m128i *)&dst_ptr[i * dst_pitch + dst_pitch],
                       _mm256_extracti128_si256(res_16bit, 1));

      s[0] = s[1];
      s4 = s6;
    }
  }
}

static void aom_highbd_filter_block1d8_v4_avx2(
    const uint16_t *src_ptr, ptrdiff_t src_pitch, uint16_t *dst_ptr,
    ptrdiff_t dst_pitch, uint32_t height, const int16_t *filter, int bd) {
  const int bits = FILTER_BITS;

  const __m128i round_shift_bits = _mm_cvtsi32_si128(bits);
  const __m256i round_const_bits = _mm256_set1_epi32((1 << bits) >> 1);
  const __m256i clip_pixel =
      _mm256_set1_epi16(bd == 10 ? 1023 : (bd == 12 ? 4095 : 255));
  const __m256i zero = _mm256_setzero_si256();
  __m256i s[4], ff[2];
  uint32_t i;
  pack_filters_4tap(filter, ff);

  const uint16_t *data = src_ptr;
  /* Vertical filter */
  {
    __m128i s2 = _mm_loadu_si128((__m128i *)(data + 2 * src_pitch));
    __m128i s3 = _mm_loadu_si128((__m128i *)(data + 3 * src_pitch));

    __m256i s23 = _mm256_inserti128_si256(_mm256_castsi128_si256(s2), s3, 1);

    __m128i s4 = _mm_loadu_si128((__m128i *)(data + 4 * src_pitch));

    __m256i s34 = _mm256_inserti128_si256(_mm256_castsi128_si256(s3), s4, 1);

    s[0] = _mm256_unpacklo_epi16(s23, s34);
    s[2] = _mm256_unpackhi_epi16(s23, s34);

    for (i = 0; i < height; i += 2) {
      data = &src_ptr[i * src_pitch];

      __m128i s5 = _mm_loadu_si128((__m128i *)(data + 5 * src_pitch));
      __m128i s6 = _mm_loadu_si128((__m128i *)(data + 6 * src_pitch));

      __m256i s45 = _mm256_inserti128_si256(_mm256_castsi128_si256(s4), s5, 1);
      __m256i s56 = _mm256_inserti128_si256(_mm256_castsi128_si256(s5), s6, 1);

      s[1] = _mm256_unpacklo_epi16(s45, s56);
      s[3] = _mm256_unpackhi_epi16(s45, s56);

      const __m256i res_a = convolve_4tap(s, ff);

      __m256i res_a_round = _mm256_sra_epi32(
          _mm256_add_epi32(res_a, round_const_bits), round_shift_bits);

      const __m256i res_b = convolve_4tap(s + 2, ff);
      __m256i res_b_round = _mm256_sra_epi32(
          _mm256_add_epi32(res_b, round_const_bits), round_shift_bits);

      __m256i res_16bit = _mm256_packs_epi32(res_a_round, res_b_round);
      res_16bit = _mm256_min_epi16(res_16bit, clip_pixel);
      res_16bit = _mm256_max_epi16(res_16bit, zero);

      _mm_storeu_si128((__m128i *)&dst_ptr[i * dst_pitch],
                       _mm256_castsi256_si128(res_16bit));
      _mm_storeu_si128((__m128i *)&dst_ptr[i * dst_pitch + dst_pitch],
                       _mm256_extracti128_si256(res_16bit, 1));

      s[0] = s[1];
      s[2] = s[3];
      s4 = s6;
    }
  }
}

static void aom_highbd_filter_block1d16_v4_avx2(
    const uint16_t *src_ptr, ptrdiff_t src_pitch, uint16_t *dst_ptr,
    ptrdiff_t dst_pitch, uint32_t height, const int16_t *filter, int bd) {
  aom_highbd_filter_block1d8_v4_avx2(src_ptr, src_pitch, dst_ptr, dst_pitch,
                                     height, filter, bd);

  aom_highbd_filter_block1d8_v4_avx2(src_ptr + 8, src_pitch, dst_ptr + 8,
                                     dst_pitch, height, filter, bd);
}

// -----------------------------------------------------------------------------
// 2-tap vertical filtering

static void pack_16x2_init(const uint16_t *src, __m256i *sig) {
  sig[2] = _mm256_loadu_si256((const __m256i *)src);
}

static INLINE void pack_16x2_2t_pixels(const uint16_t *src, ptrdiff_t pitch,
                                       __m256i *sig) {
  // load the next row
  const __m256i u = _mm256_loadu_si256((const __m256i *)(src + pitch));
  sig[0] = _mm256_unpacklo_epi16(sig[2], u);
  sig[1] = _mm256_unpackhi_epi16(sig[2], u);
  sig[2] = u;
}

static INLINE void filter_16x2_2t_pixels(const __m256i *sig, const __m256i *f,
                                         __m256i *y0, __m256i *y1) {
  filter_16_2t_pixels(sig, f, y0, y1);
}

static void aom_highbd_filter_block1d16_v2_avx2(
    const uint16_t *src_ptr, ptrdiff_t src_pitch, uint16_t *dst_ptr,
    ptrdiff_t dst_pitch, uint32_t height, const int16_t *filter, int bd) {
  __m256i signal[3], res0, res1;
  const __m256i max = _mm256_set1_epi16((1 << bd) - 1);
  __m256i ff;

  pack_2t_filter(filter, &ff);
  pack_16x2_init(src_ptr, signal);

  do {
    pack_16x2_2t_pixels(src_ptr, src_pitch, signal);
    filter_16x2_2t_pixels(signal, &ff, &res0, &res1);
    store_16x1_pixels(&res0, &res1, &max, dst_ptr);

    src_ptr += src_pitch;
    dst_ptr += dst_pitch;
    height -= 1;
  } while (height > 0);
}

static INLINE void pack_8x1_2t_filter(const int16_t *filter, __m128i *f) {
  const __m128i h = _mm_loadu_si128((const __m128i *)filter);
  const __m128i p = _mm_set1_epi32(0x09080706);
  f[0] = _mm_shuffle_epi8(h, p);
}

static void pack_8x2_init(const uint16_t *src, __m128i *sig) {
  sig[2] = _mm_loadu_si128((const __m128i *)src);
}

static INLINE void pack_8x2_2t_pixels_ver(const uint16_t *src, ptrdiff_t pitch,
                                          __m128i *sig) {
  // load the next row
  const __m128i u = _mm_loadu_si128((const __m128i *)(src + pitch));
  sig[0] = _mm_unpacklo_epi16(sig[2], u);
  sig[1] = _mm_unpackhi_epi16(sig[2], u);
  sig[2] = u;
}

static INLINE void filter_8_2t_pixels(const __m128i *sig, const __m128i *f,
                                      __m128i *y0, __m128i *y1) {
  const __m128i rounding = _mm_set1_epi32(1 << (CONV8_ROUNDING_BITS - 1));
  __m128i x0 = _mm_madd_epi16(sig[0], *f);
  __m128i x1 = _mm_madd_epi16(sig[1], *f);
  x0 = _mm_add_epi32(x0, rounding);
  x1 = _mm_add_epi32(x1, rounding);
  *y0 = _mm_srai_epi32(x0, CONV8_ROUNDING_BITS);
  *y1 = _mm_srai_epi32(x1, CONV8_ROUNDING_BITS);
}

static INLINE void store_8x1_2t_pixels_ver(const __m128i *y0, const __m128i *y1,
                                           const __m128i *mask, uint16_t *dst) {
  __m128i res = _mm_packus_epi32(*y0, *y1);
  res = _mm_min_epi16(res, *mask);
  _mm_storeu_si128((__m128i *)dst, res);
}

static void aom_highbd_filter_block1d8_v2_avx2(
    const uint16_t *src_ptr, ptrdiff_t src_pitch, uint16_t *dst_ptr,
    ptrdiff_t dst_pitch, uint32_t height, const int16_t *filter, int bd) {
  __m128i signal[3], res0, res1;
  const __m128i max = _mm_set1_epi16((1 << bd) - 1);
  __m128i ff;

  pack_8x1_2t_filter(filter, &ff);
  pack_8x2_init(src_ptr, signal);

  do {
    pack_8x2_2t_pixels_ver(src_ptr, src_pitch, signal);
    filter_8_2t_pixels(signal, &ff, &res0, &res1);
    store_8x1_2t_pixels_ver(&res0, &res1, &max, dst_ptr);

    src_ptr += src_pitch;
    dst_ptr += dst_pitch;
    height -= 1;
  } while (height > 0);
}

void aom_highbd_filter_block1d4_h8_sse2(const uint16_t *, ptrdiff_t, uint16_t *,
                                        ptrdiff_t, uint32_t, const int16_t *,
                                        int);
void aom_highbd_filter_block1d4_h2_sse2(const uint16_t *, ptrdiff_t, uint16_t *,
                                        ptrdiff_t, uint32_t, const int16_t *,
                                        int);
void aom_highbd_filter_block1d4_v8_sse2(const uint16_t *, ptrdiff_t, uint16_t *,
                                        ptrdiff_t, uint32_t, const int16_t *,
                                        int);
void aom_highbd_filter_block1d4_v2_sse2(const uint16_t *, ptrdiff_t, uint16_t *,
                                        ptrdiff_t, uint32_t, const int16_t *,
                                        int);
#define aom_highbd_filter_block1d4_h8_avx2 aom_highbd_filter_block1d4_h8_sse2
#define aom_highbd_filter_block1d4_h2_avx2 aom_highbd_filter_block1d4_h2_sse2
#define aom_highbd_filter_block1d4_v8_avx2 aom_highbd_filter_block1d4_v8_sse2
#define aom_highbd_filter_block1d4_v2_avx2 aom_highbd_filter_block1d4_v2_sse2

HIGH_FUN_CONV_1D(horiz, x_step_q4, filter_x, h, src, , avx2)
HIGH_FUN_CONV_1D(vert, y_step_q4, filter_y, v, src - src_stride * 3, , avx2)

#undef HIGHBD_FUNC

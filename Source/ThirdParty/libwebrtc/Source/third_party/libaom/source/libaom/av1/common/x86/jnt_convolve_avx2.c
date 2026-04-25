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

#include <emmintrin.h>
#include <immintrin.h>

#include "config/av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/aom_filter.h"
#include "aom_dsp/x86/convolve_avx2.h"
#include "aom_dsp/x86/convolve_common_intrin.h"
#include "aom_dsp/x86/convolve_sse4_1.h"
#include "aom_dsp/x86/mem_sse2.h"
#include "aom_dsp/x86/synonyms_avx2.h"

#include "av1/common/convolve.h"

static inline __m256i unpack_weights_avx2(ConvolveParams *conv_params) {
  const int w0 = conv_params->fwd_offset;
  const int w1 = conv_params->bck_offset;
  const __m256i wt0 = _mm256_set1_epi16((int16_t)w0);
  const __m256i wt1 = _mm256_set1_epi16((int16_t)w1);
  const __m256i wt = _mm256_unpacklo_epi16(wt0, wt1);
  return wt;
}

static inline __m256i load_line2_avx2(const void *a, const void *b) {
  return _mm256_permute2x128_si256(
      _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)a)),
      _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)b)), 0x20);
}

void av1_dist_wtd_convolve_x_avx2(const uint8_t *src, int src_stride,
                                  uint8_t *dst0, int dst_stride0, int w, int h,
                                  const InterpFilterParams *filter_params_x,
                                  const int subpel_x_qn,
                                  ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int bd = 8;
  int i, j, is_horiz_4tap = 0;
  const int bits = FILTER_BITS - conv_params->round_1;
  const __m256i wt = unpack_weights_avx2(conv_params);
  const int do_average = conv_params->do_average;
  const int use_dist_wtd_comp_avg = conv_params->use_dist_wtd_comp_avg;
  const int offset_0 =
      bd + 2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const int offset = (1 << offset_0) + (1 << (offset_0 - 1));
  const __m256i offset_const = _mm256_set1_epi16(offset);
  const int rounding_shift =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const __m256i rounding_const = _mm256_set1_epi16((1 << rounding_shift) >> 1);

  assert(bits >= 0);
  assert(conv_params->round_0 > 0);

  const __m256i round_const =
      _mm256_set1_epi16((1 << (conv_params->round_0 - 1)) >> 1);
  const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_0 - 1);

  __m256i filt[4], coeffs[4];

  filt[0] = _mm256_load_si256((__m256i const *)filt_global_avx2);
  filt[1] = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32));

  prepare_coeffs_lowbd(filter_params_x, subpel_x_qn, coeffs);

  // Condition for checking valid horz_filt taps
  if (!(_mm256_extract_epi32(_mm256_or_si256(coeffs[0], coeffs[3]), 0)))
    is_horiz_4tap = 1;

  // horz_filt as 4 tap
  if (is_horiz_4tap) {
    const int fo_horiz = 1;
    const uint8_t *const src_ptr = src - fo_horiz;
    for (i = 0; i < h; i += 2) {
      const uint8_t *src_data = src_ptr + i * src_stride;
      CONV_BUF_TYPE *dst_data = dst + i * dst_stride;
      for (j = 0; j < w; j += 8) {
        const __m256i data =
            load_line2_avx2(&src_data[j], &src_data[j + src_stride]);

        __m256i res = convolve_lowbd_x_4tap(data, coeffs + 1, filt);
        res = _mm256_sra_epi16(_mm256_add_epi16(res, round_const), round_shift);
        res = _mm256_slli_epi16(res, bits);

        const __m256i res_unsigned = _mm256_add_epi16(res, offset_const);

        // Accumulate values into the destination buffer
        if (do_average) {
          const __m256i data_ref_0 =
              load_line2_avx2(&dst_data[j], &dst_data[j + dst_stride]);
          const __m256i comp_avg_res =
              comp_avg(&data_ref_0, &res_unsigned, &wt, use_dist_wtd_comp_avg);

          const __m256i round_result = convolve_rounding(
              &comp_avg_res, &offset_const, &rounding_const, rounding_shift);

          const __m256i res_8 = _mm256_packus_epi16(round_result, round_result);
          const __m128i res_0 = _mm256_castsi256_si128(res_8);
          const __m128i res_1 = _mm256_extracti128_si256(res_8, 1);

          if (w > 4) {
            _mm_storel_epi64((__m128i *)(&dst0[i * dst_stride0 + j]), res_0);
            _mm_storel_epi64(
                (__m128i *)((&dst0[i * dst_stride0 + j + dst_stride0])), res_1);
          } else {
            *(int *)(&dst0[i * dst_stride0 + j]) = _mm_cvtsi128_si32(res_0);
            *(int *)(&dst0[i * dst_stride0 + j + dst_stride0]) =
                _mm_cvtsi128_si32(res_1);
          }
        } else {
          const __m128i res_0 = _mm256_castsi256_si128(res_unsigned);
          _mm_store_si128((__m128i *)(&dst[i * dst_stride + j]), res_0);

          const __m128i res_1 = _mm256_extracti128_si256(res_unsigned, 1);
          _mm_store_si128((__m128i *)(&dst[i * dst_stride + j + dst_stride]),
                          res_1);
        }
      }
    }
  } else {
    const int fo_horiz = filter_params_x->taps / 2 - 1;
    const uint8_t *const src_ptr = src - fo_horiz;

    filt[2] = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32 * 2));
    filt[3] = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32 * 3));
    for (i = 0; i < h; i += 2) {
      const uint8_t *src_data = src_ptr + i * src_stride;
      CONV_BUF_TYPE *dst_data = dst + i * dst_stride;
      for (j = 0; j < w; j += 8) {
        const __m256i data =
            load_line2_avx2(&src_data[j], &src_data[j + src_stride]);

        __m256i res = convolve_lowbd_x(data, coeffs, filt);

        res = _mm256_sra_epi16(_mm256_add_epi16(res, round_const), round_shift);

        res = _mm256_slli_epi16(res, bits);

        const __m256i res_unsigned = _mm256_add_epi16(res, offset_const);

        // Accumulate values into the destination buffer
        if (do_average) {
          const __m256i data_ref_0 =
              load_line2_avx2(&dst_data[j], &dst_data[j + dst_stride]);
          const __m256i comp_avg_res =
              comp_avg(&data_ref_0, &res_unsigned, &wt, use_dist_wtd_comp_avg);

          const __m256i round_result = convolve_rounding(
              &comp_avg_res, &offset_const, &rounding_const, rounding_shift);

          const __m256i res_8 = _mm256_packus_epi16(round_result, round_result);
          const __m128i res_0 = _mm256_castsi256_si128(res_8);
          const __m128i res_1 = _mm256_extracti128_si256(res_8, 1);

          if (w > 4) {
            _mm_storel_epi64((__m128i *)(&dst0[i * dst_stride0 + j]), res_0);
            _mm_storel_epi64(
                (__m128i *)((&dst0[i * dst_stride0 + j + dst_stride0])), res_1);
          } else {
            *(int *)(&dst0[i * dst_stride0 + j]) = _mm_cvtsi128_si32(res_0);
            *(int *)(&dst0[i * dst_stride0 + j + dst_stride0]) =
                _mm_cvtsi128_si32(res_1);
          }
        } else {
          const __m128i res_0 = _mm256_castsi256_si128(res_unsigned);
          _mm_store_si128((__m128i *)(&dst[i * dst_stride + j]), res_0);

          const __m128i res_1 = _mm256_extracti128_si256(res_unsigned, 1);
          _mm_store_si128((__m128i *)(&dst[i * dst_stride + j + dst_stride]),
                          res_1);
        }
      }
    }
  }
}

void av1_dist_wtd_convolve_y_avx2(const uint8_t *src, int src_stride,
                                  uint8_t *dst0, int dst_stride0, int w, int h,
                                  const InterpFilterParams *filter_params_y,
                                  const int subpel_y_qn,
                                  ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int bd = 8;
  int i, j, is_vert_4tap = 0;
  // +1 to compensate for dividing the filter coeffs by 2
  const int left_shift = FILTER_BITS - conv_params->round_0 + 1;
  const __m256i round_const =
      _mm256_set1_epi32((1 << conv_params->round_1) >> 1);
  const __m128i round_shift = _mm_cvtsi32_si128(conv_params->round_1);
  const __m256i wt = unpack_weights_avx2(conv_params);
  const int do_average = conv_params->do_average;
  const int use_dist_wtd_comp_avg = conv_params->use_dist_wtd_comp_avg;
  const int offset_0 =
      bd + 2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const int offset = (1 << offset_0) + (1 << (offset_0 - 1));
  const __m256i offset_const = _mm256_set1_epi16(offset);
  const int offset_1 = (1 << (bd + FILTER_BITS - 2));
  const __m256i offset_const_1 = _mm256_set1_epi16(offset_1);
  const __m256i offset_const_2 = _mm256_set1_epi16((1 << offset_0));
  const int rounding_shift =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const __m256i rounding_const = _mm256_set1_epi16((1 << rounding_shift) >> 1);
  const __m256i zero = _mm256_setzero_si256();
  __m256i coeffs[4], s[8];

  assert((FILTER_BITS - conv_params->round_0) >= 0);

  prepare_coeffs_lowbd(filter_params_y, subpel_y_qn, coeffs);

  // Condition for checking valid vert_filt taps
  if (!(_mm256_extract_epi32(_mm256_or_si256(coeffs[0], coeffs[3]), 0)))
    is_vert_4tap = 1;

  if (is_vert_4tap) {
    const int fo_vert = 1;
    const uint8_t *const src_ptr = src - fo_vert * src_stride;
    for (j = 0; j < w; j += 16) {
      const uint8_t *data = &src_ptr[j];
      __m256i src4;
      // Load lines a and b. Line a to lower 128, line b to upper 128
      {
        __m256i src_ab[4];
        __m256i src_a[5];
        src_a[0] = _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)data));
        for (int kk = 0; kk < 4; ++kk) {
          data += src_stride;
          src_a[kk + 1] =
              _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)data));
          src_ab[kk] =
              _mm256_permute2x128_si256(src_a[kk], src_a[kk + 1], 0x20);
        }
        src4 = src_a[4];
        s[0] = _mm256_unpacklo_epi8(src_ab[0], src_ab[1]);
        s[1] = _mm256_unpacklo_epi8(src_ab[2], src_ab[3]);

        s[3] = _mm256_unpackhi_epi8(src_ab[0], src_ab[1]);
        s[4] = _mm256_unpackhi_epi8(src_ab[2], src_ab[3]);
      }

      for (i = 0; i < h; i += 2) {
        data = &src_ptr[(i + 5) * src_stride + j];
        const __m256i src5 =
            _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)data));
        const __m256i src_45a = _mm256_permute2x128_si256(src4, src5, 0x20);

        src4 = _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + src_stride)));
        const __m256i src_56a = _mm256_permute2x128_si256(src5, src4, 0x20);

        s[2] = _mm256_unpacklo_epi8(src_45a, src_56a);
        s[5] = _mm256_unpackhi_epi8(src_45a, src_56a);

        __m256i res_lo = convolve_lowbd_4tap(s, coeffs + 1);

        res_lo = _mm256_add_epi16(res_lo, offset_const_1);

        const __m256i res_lo_0_32b = _mm256_unpacklo_epi16(res_lo, zero);
        const __m256i res_lo_0_shift =
            _mm256_slli_epi32(res_lo_0_32b, left_shift);
        const __m256i res_lo_0_round = _mm256_sra_epi32(
            _mm256_add_epi32(res_lo_0_shift, round_const), round_shift);

        const __m256i res_lo_1_32b = _mm256_unpackhi_epi16(res_lo, zero);
        const __m256i res_lo_1_shift =
            _mm256_slli_epi32(res_lo_1_32b, left_shift);
        const __m256i res_lo_1_round = _mm256_sra_epi32(
            _mm256_add_epi32(res_lo_1_shift, round_const), round_shift);

        const __m256i res_lo_round =
            _mm256_packs_epi32(res_lo_0_round, res_lo_1_round);

        const __m256i res_lo_unsigned =
            _mm256_add_epi16(res_lo_round, offset_const_2);

        if (w - j < 16) {
          if (do_average) {
            const __m256i data_ref_0 =
                load_line2_avx2(&dst[i * dst_stride + j],
                                &dst[i * dst_stride + j + dst_stride]);
            const __m256i comp_avg_res = comp_avg(&data_ref_0, &res_lo_unsigned,
                                                  &wt, use_dist_wtd_comp_avg);

            const __m256i round_result = convolve_rounding(
                &comp_avg_res, &offset_const, &rounding_const, rounding_shift);

            const __m256i res_8 =
                _mm256_packus_epi16(round_result, round_result);
            const __m128i res_0 = _mm256_castsi256_si128(res_8);
            const __m128i res_1 = _mm256_extracti128_si256(res_8, 1);

            if (w - j > 4) {
              _mm_storel_epi64((__m128i *)(&dst0[i * dst_stride0 + j]), res_0);
              _mm_storel_epi64(
                  (__m128i *)((&dst0[i * dst_stride0 + j + dst_stride0])),
                  res_1);
            } else {
              *(int *)(&dst0[i * dst_stride0 + j]) = _mm_cvtsi128_si32(res_0);
              *(int *)(&dst0[i * dst_stride0 + j + dst_stride0]) =
                  _mm_cvtsi128_si32(res_1);
            }
          } else {
            const __m128i res_0 = _mm256_castsi256_si128(res_lo_unsigned);
            _mm_store_si128((__m128i *)(&dst[i * dst_stride + j]), res_0);

            const __m128i res_1 = _mm256_extracti128_si256(res_lo_unsigned, 1);
            _mm_store_si128((__m128i *)(&dst[i * dst_stride + j + dst_stride]),
                            res_1);
          }
        } else {
          __m256i res_hi = convolve_lowbd_4tap(s + 3, coeffs + 1);

          res_hi = _mm256_add_epi16(res_hi, offset_const_1);

          const __m256i res_hi_0_32b = _mm256_unpacklo_epi16(res_hi, zero);
          const __m256i res_hi_0_shift =
              _mm256_slli_epi32(res_hi_0_32b, left_shift);
          const __m256i res_hi_0_round = _mm256_sra_epi32(
              _mm256_add_epi32(res_hi_0_shift, round_const), round_shift);

          const __m256i res_hi_1_32b = _mm256_unpackhi_epi16(res_hi, zero);
          const __m256i res_hi_1_shift =
              _mm256_slli_epi32(res_hi_1_32b, left_shift);
          const __m256i res_hi_1_round = _mm256_sra_epi32(
              _mm256_add_epi32(res_hi_1_shift, round_const), round_shift);

          const __m256i res_hi_round =
              _mm256_packs_epi32(res_hi_0_round, res_hi_1_round);

          const __m256i res_hi_unsigned =
              _mm256_add_epi16(res_hi_round, offset_const_2);

          if (do_average) {
            const __m256i data_ref_0_lo =
                load_line2_avx2(&dst[i * dst_stride + j],
                                &dst[i * dst_stride + j + dst_stride]);

            const __m256i data_ref_0_hi =
                load_line2_avx2(&dst[i * dst_stride + j + 8],
                                &dst[i * dst_stride + j + 8 + dst_stride]);

            const __m256i comp_avg_res_lo = comp_avg(
                &data_ref_0_lo, &res_lo_unsigned, &wt, use_dist_wtd_comp_avg);

            const __m256i comp_avg_res_hi = comp_avg(
                &data_ref_0_hi, &res_hi_unsigned, &wt, use_dist_wtd_comp_avg);

            const __m256i round_result_lo =
                convolve_rounding(&comp_avg_res_lo, &offset_const,
                                  &rounding_const, rounding_shift);

            const __m256i round_result_hi =
                convolve_rounding(&comp_avg_res_hi, &offset_const,
                                  &rounding_const, rounding_shift);

            const __m256i res_8 =
                _mm256_packus_epi16(round_result_lo, round_result_hi);
            const __m128i res_0 = _mm256_castsi256_si128(res_8);
            const __m128i res_1 = _mm256_extracti128_si256(res_8, 1);

            _mm_store_si128((__m128i *)(&dst0[i * dst_stride0 + j]), res_0);
            _mm_store_si128(
                (__m128i *)((&dst0[i * dst_stride0 + j + dst_stride0])), res_1);

          } else {
            const __m128i res_lo_0 = _mm256_castsi256_si128(res_lo_unsigned);
            _mm_store_si128((__m128i *)(&dst[i * dst_stride + j]), res_lo_0);

            const __m128i res_lo_1 =
                _mm256_extracti128_si256(res_lo_unsigned, 1);
            _mm_store_si128((__m128i *)(&dst[i * dst_stride + j + dst_stride]),
                            res_lo_1);

            const __m128i res_hi_0 = _mm256_castsi256_si128(res_hi_unsigned);
            _mm_store_si128((__m128i *)(&dst[i * dst_stride + j + 8]),
                            res_hi_0);

            const __m128i res_hi_1 =
                _mm256_extracti128_si256(res_hi_unsigned, 1);
            _mm_store_si128(
                (__m128i *)(&dst[i * dst_stride + j + 8 + dst_stride]),
                res_hi_1);
          }
        }
        s[0] = s[1];
        s[1] = s[2];

        s[3] = s[4];
        s[4] = s[5];
      }
    }
  } else {
    const int fo_vert = filter_params_y->taps / 2 - 1;
    const uint8_t *const src_ptr = src - fo_vert * src_stride;
    for (j = 0; j < w; j += 16) {
      const uint8_t *data = &src_ptr[j];
      __m256i src6;
      // Load lines a and b. Line a to lower 128, line b to upper 128
      {
        __m256i src_ab[7];
        __m256i src_a[7];
        src_a[0] = _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)data));
        for (int kk = 0; kk < 6; ++kk) {
          data += src_stride;
          src_a[kk + 1] =
              _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)data));
          src_ab[kk] =
              _mm256_permute2x128_si256(src_a[kk], src_a[kk + 1], 0x20);
        }
        src6 = src_a[6];
        s[0] = _mm256_unpacklo_epi8(src_ab[0], src_ab[1]);
        s[1] = _mm256_unpacklo_epi8(src_ab[2], src_ab[3]);
        s[2] = _mm256_unpacklo_epi8(src_ab[4], src_ab[5]);
        s[4] = _mm256_unpackhi_epi8(src_ab[0], src_ab[1]);
        s[5] = _mm256_unpackhi_epi8(src_ab[2], src_ab[3]);
        s[6] = _mm256_unpackhi_epi8(src_ab[4], src_ab[5]);
      }

      for (i = 0; i < h; i += 2) {
        data = &src_ptr[(i + 7) * src_stride + j];
        const __m256i src7 =
            _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)data));
        const __m256i src_67a = _mm256_permute2x128_si256(src6, src7, 0x20);

        src6 = _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + src_stride)));
        const __m256i src_78a = _mm256_permute2x128_si256(src7, src6, 0x20);

        s[3] = _mm256_unpacklo_epi8(src_67a, src_78a);
        s[7] = _mm256_unpackhi_epi8(src_67a, src_78a);

        __m256i res_lo = convolve_lowbd(s, coeffs);

        res_lo = _mm256_add_epi16(res_lo, offset_const_1);

        const __m256i res_lo_0_32b = _mm256_unpacklo_epi16(res_lo, zero);
        const __m256i res_lo_0_shift =
            _mm256_slli_epi32(res_lo_0_32b, left_shift);
        const __m256i res_lo_0_round = _mm256_sra_epi32(
            _mm256_add_epi32(res_lo_0_shift, round_const), round_shift);

        const __m256i res_lo_1_32b = _mm256_unpackhi_epi16(res_lo, zero);
        const __m256i res_lo_1_shift =
            _mm256_slli_epi32(res_lo_1_32b, left_shift);
        const __m256i res_lo_1_round = _mm256_sra_epi32(
            _mm256_add_epi32(res_lo_1_shift, round_const), round_shift);

        const __m256i res_lo_round =
            _mm256_packs_epi32(res_lo_0_round, res_lo_1_round);

        const __m256i res_lo_unsigned =
            _mm256_add_epi16(res_lo_round, offset_const_2);

        if (w - j < 16) {
          if (do_average) {
            const __m256i data_ref_0 =
                load_line2_avx2(&dst[i * dst_stride + j],
                                &dst[i * dst_stride + j + dst_stride]);
            const __m256i comp_avg_res = comp_avg(&data_ref_0, &res_lo_unsigned,
                                                  &wt, use_dist_wtd_comp_avg);

            const __m256i round_result = convolve_rounding(
                &comp_avg_res, &offset_const, &rounding_const, rounding_shift);

            const __m256i res_8 =
                _mm256_packus_epi16(round_result, round_result);
            const __m128i res_0 = _mm256_castsi256_si128(res_8);
            const __m128i res_1 = _mm256_extracti128_si256(res_8, 1);

            if (w - j > 4) {
              _mm_storel_epi64((__m128i *)(&dst0[i * dst_stride0 + j]), res_0);
              _mm_storel_epi64(
                  (__m128i *)((&dst0[i * dst_stride0 + j + dst_stride0])),
                  res_1);
            } else {
              *(int *)(&dst0[i * dst_stride0 + j]) = _mm_cvtsi128_si32(res_0);
              *(int *)(&dst0[i * dst_stride0 + j + dst_stride0]) =
                  _mm_cvtsi128_si32(res_1);
            }
          } else {
            const __m128i res_0 = _mm256_castsi256_si128(res_lo_unsigned);
            _mm_store_si128((__m128i *)(&dst[i * dst_stride + j]), res_0);

            const __m128i res_1 = _mm256_extracti128_si256(res_lo_unsigned, 1);
            _mm_store_si128((__m128i *)(&dst[i * dst_stride + j + dst_stride]),
                            res_1);
          }
        } else {
          __m256i res_hi = convolve_lowbd(s + 4, coeffs);

          res_hi = _mm256_add_epi16(res_hi, offset_const_1);

          const __m256i res_hi_0_32b = _mm256_unpacklo_epi16(res_hi, zero);
          const __m256i res_hi_0_shift =
              _mm256_slli_epi32(res_hi_0_32b, left_shift);
          const __m256i res_hi_0_round = _mm256_sra_epi32(
              _mm256_add_epi32(res_hi_0_shift, round_const), round_shift);

          const __m256i res_hi_1_32b = _mm256_unpackhi_epi16(res_hi, zero);
          const __m256i res_hi_1_shift =
              _mm256_slli_epi32(res_hi_1_32b, left_shift);
          const __m256i res_hi_1_round = _mm256_sra_epi32(
              _mm256_add_epi32(res_hi_1_shift, round_const), round_shift);

          const __m256i res_hi_round =
              _mm256_packs_epi32(res_hi_0_round, res_hi_1_round);

          const __m256i res_hi_unsigned =
              _mm256_add_epi16(res_hi_round, offset_const_2);

          if (do_average) {
            const __m256i data_ref_0_lo =
                load_line2_avx2(&dst[i * dst_stride + j],
                                &dst[i * dst_stride + j + dst_stride]);

            const __m256i data_ref_0_hi =
                load_line2_avx2(&dst[i * dst_stride + j + 8],
                                &dst[i * dst_stride + j + 8 + dst_stride]);

            const __m256i comp_avg_res_lo = comp_avg(
                &data_ref_0_lo, &res_lo_unsigned, &wt, use_dist_wtd_comp_avg);

            const __m256i comp_avg_res_hi = comp_avg(
                &data_ref_0_hi, &res_hi_unsigned, &wt, use_dist_wtd_comp_avg);

            const __m256i round_result_lo =
                convolve_rounding(&comp_avg_res_lo, &offset_const,
                                  &rounding_const, rounding_shift);

            const __m256i round_result_hi =
                convolve_rounding(&comp_avg_res_hi, &offset_const,
                                  &rounding_const, rounding_shift);

            const __m256i res_8 =
                _mm256_packus_epi16(round_result_lo, round_result_hi);
            const __m128i res_0 = _mm256_castsi256_si128(res_8);
            const __m128i res_1 = _mm256_extracti128_si256(res_8, 1);

            _mm_store_si128((__m128i *)(&dst0[i * dst_stride0 + j]), res_0);
            _mm_store_si128(
                (__m128i *)((&dst0[i * dst_stride0 + j + dst_stride0])), res_1);

          } else {
            const __m128i res_lo_0 = _mm256_castsi256_si128(res_lo_unsigned);
            _mm_store_si128((__m128i *)(&dst[i * dst_stride + j]), res_lo_0);

            const __m128i res_lo_1 =
                _mm256_extracti128_si256(res_lo_unsigned, 1);
            _mm_store_si128((__m128i *)(&dst[i * dst_stride + j + dst_stride]),
                            res_lo_1);

            const __m128i res_hi_0 = _mm256_castsi256_si128(res_hi_unsigned);
            _mm_store_si128((__m128i *)(&dst[i * dst_stride + j + 8]),
                            res_hi_0);

            const __m128i res_hi_1 =
                _mm256_extracti128_si256(res_hi_unsigned, 1);
            _mm_store_si128(
                (__m128i *)(&dst[i * dst_stride + j + 8 + dst_stride]),
                res_hi_1);
          }
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

void av1_dist_wtd_convolve_2d_avx2(const uint8_t *src, int src_stride,
                                   uint8_t *dst0, int dst_stride0, int w, int h,
                                   const InterpFilterParams *filter_params_x,
                                   const InterpFilterParams *filter_params_y,
                                   const int subpel_x_qn, const int subpel_y_qn,
                                   ConvolveParams *conv_params) {
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  const int bd = 8;

  DECLARE_ALIGNED(32, int16_t, im_block[(MAX_SB_SIZE + MAX_FILTER_TAP) * 8]);

  int im_stride = 8;
  int i, is_horiz_4tap = 0, is_vert_4tap = 0;
  const __m256i wt = unpack_weights_avx2(conv_params);
  const int do_average = conv_params->do_average;
  const int use_dist_wtd_comp_avg = conv_params->use_dist_wtd_comp_avg;
  const int offset_0 =
      bd + 2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const int offset = (1 << offset_0) + (1 << (offset_0 - 1));
  const __m256i offset_const = _mm256_set1_epi16(offset);
  const int rounding_shift =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const __m256i rounding_const = _mm256_set1_epi16((1 << rounding_shift) >> 1);

  assert(conv_params->round_0 > 0);

  const __m256i round_const_h = _mm256_set1_epi16(
      ((1 << (conv_params->round_0 - 1)) >> 1) + (1 << (bd + FILTER_BITS - 2)));
  const __m128i round_shift_h = _mm_cvtsi32_si128(conv_params->round_0 - 1);

  const __m256i round_const_v = _mm256_set1_epi32(
      ((1 << conv_params->round_1) >> 1) -
      (1 << (bd + 2 * FILTER_BITS - conv_params->round_0 - 1)));
  const __m128i round_shift_v = _mm_cvtsi32_si128(conv_params->round_1);

  __m256i filt[4], coeffs_x[4], coeffs_y[4];

  filt[0] = _mm256_load_si256((__m256i const *)filt_global_avx2);
  filt[1] = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32));

  prepare_coeffs_lowbd(filter_params_x, subpel_x_qn, coeffs_x);
  prepare_coeffs(filter_params_y, subpel_y_qn, coeffs_y);

  // Condition for checking valid horz_filt taps
  if (!(_mm256_extract_epi32(_mm256_or_si256(coeffs_x[0], coeffs_x[3]), 0)))
    is_horiz_4tap = 1;

  // Condition for checking valid vert_filt taps
  if (!(_mm256_extract_epi32(_mm256_or_si256(coeffs_y[0], coeffs_y[3]), 0)))
    is_vert_4tap = 1;

  if (is_horiz_4tap) {
    int im_h = h + filter_params_y->taps - 1;
    const int fo_vert = filter_params_y->taps / 2 - 1;
    const int fo_horiz = 1;
    const uint8_t *const src_ptr = src - fo_vert * src_stride - fo_horiz;
    for (int j = 0; j < w; j += 8) {
      /* Horizontal filter */
      const uint8_t *src_h = src_ptr + j;
      for (i = 0; i < im_h; i += 2) {
        __m256i data =
            _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)src_h));
        if (i + 1 < im_h)
          data = _mm256_inserti128_si256(
              data, _mm_loadu_si128((__m128i *)(src_h + src_stride)), 1);
        src_h += (src_stride << 1);
        __m256i res = convolve_lowbd_x_4tap(data, coeffs_x + 1, filt);

        res = _mm256_sra_epi16(_mm256_add_epi16(res, round_const_h),
                               round_shift_h);

        _mm256_store_si256((__m256i *)&im_block[i * im_stride], res);
      }
      DIST_WTD_CONVOLVE_VERTICAL_FILTER_8TAP;
    }
  } else if (is_vert_4tap) {
    int im_h = h + 3;
    const int fo_vert = 1;
    const int fo_horiz = filter_params_x->taps / 2 - 1;
    const uint8_t *const src_ptr = src - fo_vert * src_stride - fo_horiz;

    filt[2] = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32 * 2));
    filt[3] = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32 * 3));

    for (int j = 0; j < w; j += 8) {
      /* Horizontal filter */
      const uint8_t *src_h = src_ptr + j;
      DIST_WTD_CONVOLVE_HORIZONTAL_FILTER_8TAP;

      /* Vertical filter */
      __m256i s[6];
      __m256i s0 = _mm256_loadu_si256((__m256i *)(im_block + 0 * im_stride));
      __m256i s1 = _mm256_loadu_si256((__m256i *)(im_block + 1 * im_stride));
      __m256i s2 = _mm256_loadu_si256((__m256i *)(im_block + 2 * im_stride));
      __m256i s3 = _mm256_loadu_si256((__m256i *)(im_block + 3 * im_stride));

      s[0] = _mm256_unpacklo_epi16(s0, s1);
      s[1] = _mm256_unpacklo_epi16(s2, s3);

      s[3] = _mm256_unpackhi_epi16(s0, s1);
      s[4] = _mm256_unpackhi_epi16(s2, s3);

      for (i = 0; i < h; i += 2) {
        const int16_t *data = &im_block[i * im_stride];

        const __m256i s4 =
            _mm256_loadu_si256((__m256i *)(data + 4 * im_stride));
        const __m256i s5 =
            _mm256_loadu_si256((__m256i *)(data + 5 * im_stride));

        s[2] = _mm256_unpacklo_epi16(s4, s5);
        s[5] = _mm256_unpackhi_epi16(s4, s5);

        const __m256i res_a = convolve_4tap(s, coeffs_y + 1);
        const __m256i res_a_round = _mm256_sra_epi32(
            _mm256_add_epi32(res_a, round_const_v), round_shift_v);

        if (w - j > 4) {
          const __m256i res_b = convolve_4tap(s + 3, coeffs_y + 1);
          const __m256i res_b_round = _mm256_sra_epi32(
              _mm256_add_epi32(res_b, round_const_v), round_shift_v);
          const __m256i res_16b = _mm256_packs_epi32(res_a_round, res_b_round);
          const __m256i res_unsigned = _mm256_add_epi16(res_16b, offset_const);

          if (do_average) {
            const __m256i data_ref_0 =
                load_line2_avx2(&dst[i * dst_stride + j],
                                &dst[i * dst_stride + j + dst_stride]);
            const __m256i comp_avg_res = comp_avg(&data_ref_0, &res_unsigned,
                                                  &wt, use_dist_wtd_comp_avg);

            const __m256i round_result = convolve_rounding(
                &comp_avg_res, &offset_const, &rounding_const, rounding_shift);

            const __m256i res_8 =
                _mm256_packus_epi16(round_result, round_result);
            const __m128i res_0 = _mm256_castsi256_si128(res_8);
            const __m128i res_1 = _mm256_extracti128_si256(res_8, 1);

            _mm_storel_epi64((__m128i *)(&dst0[i * dst_stride0 + j]), res_0);
            _mm_storel_epi64(
                (__m128i *)((&dst0[i * dst_stride0 + j + dst_stride0])), res_1);
          } else {
            const __m128i res_0 = _mm256_castsi256_si128(res_unsigned);
            _mm_store_si128((__m128i *)(&dst[i * dst_stride + j]), res_0);

            const __m128i res_1 = _mm256_extracti128_si256(res_unsigned, 1);
            _mm_store_si128((__m128i *)(&dst[i * dst_stride + j + dst_stride]),
                            res_1);
          }
        } else {
          const __m256i res_16b = _mm256_packs_epi32(res_a_round, res_a_round);
          const __m256i res_unsigned = _mm256_add_epi16(res_16b, offset_const);

          if (do_average) {
            const __m256i data_ref_0 =
                load_line2_avx2(&dst[i * dst_stride + j],
                                &dst[i * dst_stride + j + dst_stride]);

            const __m256i comp_avg_res = comp_avg(&data_ref_0, &res_unsigned,
                                                  &wt, use_dist_wtd_comp_avg);

            const __m256i round_result = convolve_rounding(
                &comp_avg_res, &offset_const, &rounding_const, rounding_shift);

            const __m256i res_8 =
                _mm256_packus_epi16(round_result, round_result);
            const __m128i res_0 = _mm256_castsi256_si128(res_8);
            const __m128i res_1 = _mm256_extracti128_si256(res_8, 1);

            *(int *)(&dst0[i * dst_stride0 + j]) = _mm_cvtsi128_si32(res_0);
            *(int *)(&dst0[i * dst_stride0 + j + dst_stride0]) =
                _mm_cvtsi128_si32(res_1);

          } else {
            const __m128i res_0 = _mm256_castsi256_si128(res_unsigned);
            _mm_store_si128((__m128i *)(&dst[i * dst_stride + j]), res_0);

            const __m128i res_1 = _mm256_extracti128_si256(res_unsigned, 1);
            _mm_store_si128((__m128i *)(&dst[i * dst_stride + j + dst_stride]),
                            res_1);
          }
        }
        s[0] = s[1];
        s[1] = s[2];
        s[3] = s[4];
        s[4] = s[5];
      }
    }
  } else {
    int im_h = h + filter_params_y->taps - 1;
    const int fo_vert = filter_params_y->taps / 2 - 1;
    const int fo_horiz = filter_params_x->taps / 2 - 1;
    const uint8_t *const src_ptr = src - fo_vert * src_stride - fo_horiz;

    filt[2] = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32 * 2));
    filt[3] = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32 * 3));

    for (int j = 0; j < w; j += 8) {
      /* Horizontal filter */
      const uint8_t *src_h = src_ptr + j;
      DIST_WTD_CONVOLVE_HORIZONTAL_FILTER_8TAP;

      DIST_WTD_CONVOLVE_VERTICAL_FILTER_8TAP;
    }
  }
}

#define DO_NO_AVG_2D_COPY_4X16(r0, c0, r1, c1, r2, c2, r3, c3)          \
  do {                                                                  \
    src_0 = _mm256_cvtepu8_epi16(                                       \
        _mm_loadu_si128((__m128i *)(&src[r0 * src_stride + c0])));      \
    src_1 = _mm256_cvtepu8_epi16(                                       \
        _mm_loadu_si128((__m128i *)(&src[r1 * src_stride + c1])));      \
    src_2 = _mm256_cvtepu8_epi16(                                       \
        _mm_loadu_si128((__m128i *)(&src[r2 * src_stride + c2])));      \
    src_3 = _mm256_cvtepu8_epi16(                                       \
        _mm_loadu_si128((__m128i *)(&src[r3 * src_stride + c3])));      \
                                                                        \
    src_0 = _mm256_slli_epi16(src_0, LEFT_SHIFT);                       \
    src_1 = _mm256_slli_epi16(src_1, LEFT_SHIFT);                       \
    src_2 = _mm256_slli_epi16(src_2, LEFT_SHIFT);                       \
    src_3 = _mm256_slli_epi16(src_3, LEFT_SHIFT);                       \
                                                                        \
    src_0 = _mm256_add_epi16(src_0, offset_const);                      \
    src_1 = _mm256_add_epi16(src_1, offset_const);                      \
    src_2 = _mm256_add_epi16(src_2, offset_const);                      \
    src_3 = _mm256_add_epi16(src_3, offset_const);                      \
                                                                        \
    _mm256_store_si256((__m256i *)(&dst[r0 * dst_stride + c0]), src_0); \
    _mm256_store_si256((__m256i *)(&dst[r1 * dst_stride + c1]), src_1); \
    _mm256_store_si256((__m256i *)(&dst[r2 * dst_stride + c2]), src_2); \
    _mm256_store_si256((__m256i *)(&dst[r3 * dst_stride + c3]), src_3); \
  } while (0)

#define LEFT_SHIFT (2 * FILTER_BITS - 3 - 7)
static inline void av1_dist_wtd_convolve_2d_no_avg_copy_avx2(
    const uint8_t *src, int src_stride, CONV_BUF_TYPE *dst, int dst_stride,
    int w, int h, const __m256i offset_const) {
  int i = h;
  if (w >= 16) {
    __m256i src_0, src_1, src_2, src_3;
    if (w == 128) {
      do {
        DO_NO_AVG_2D_COPY_4X16(0, 0, 0, 16, 0, 32, 0, 48);
        DO_NO_AVG_2D_COPY_4X16(0, 64, 0, 80, 0, 96, 0, 112);
        src += 1 * src_stride;
        dst += 1 * dst_stride;
        i -= 1;
      } while (i);
    } else if (w == 64) {
      do {
        DO_NO_AVG_2D_COPY_4X16(0, 0, 0, 16, 0, 32, 0, 48);
        src += 1 * src_stride;
        dst += 1 * dst_stride;
        i -= 1;
      } while (i);
    } else if (w == 32) {
      do {
        DO_NO_AVG_2D_COPY_4X16(0, 0, 1, 0, 0, 16, 1, 16);
        src += 2 * src_stride;
        dst += 2 * dst_stride;
        i -= 2;
      } while (i);
    } else if (w == 16) {
      do {
        DO_NO_AVG_2D_COPY_4X16(0, 0, 1, 0, 2, 0, 3, 0);
        src += 4 * src_stride;
        dst += 4 * dst_stride;
        i -= 4;
      } while (i);
    }
  } else {
    const __m256i zero = _mm256_setzero_si256();
    do {
      const __m128i src_row_0 =
          _mm_loadl_epi64((__m128i *)(&src[0 * src_stride]));
      const __m128i src_row_1 =
          _mm_loadl_epi64((__m128i *)(&src[1 * src_stride]));
      const __m128i src_row_2 =
          _mm_loadl_epi64((__m128i *)(&src[2 * src_stride]));
      const __m128i src_row_3 =
          _mm_loadl_epi64((__m128i *)(&src[3 * src_stride]));

      __m256i src_10 = _mm256_insertf128_si256(
          _mm256_castsi128_si256(src_row_0), src_row_1, 1);
      __m256i src_32 = _mm256_insertf128_si256(
          _mm256_castsi128_si256(src_row_2), src_row_3, 1);

      src_10 = _mm256_unpacklo_epi8(src_10, zero);
      src_32 = _mm256_unpacklo_epi8(src_32, zero);

      src_10 = _mm256_slli_epi16(src_10, LEFT_SHIFT);
      src_32 = _mm256_slli_epi16(src_32, LEFT_SHIFT);

      src_10 = _mm256_add_epi16(src_10, offset_const);
      src_32 = _mm256_add_epi16(src_32, offset_const);

      // Accumulate values into the destination buffer
      _mm_store_si128((__m128i *)(&dst[0 * dst_stride]),
                      _mm256_castsi256_si128(src_10));
      _mm_store_si128((__m128i *)(&dst[1 * dst_stride]),
                      _mm256_extracti128_si256(src_10, 1));
      _mm_store_si128((__m128i *)(&dst[2 * dst_stride]),
                      _mm256_castsi256_si128(src_32));
      _mm_store_si128((__m128i *)(&dst[3 * dst_stride]),
                      _mm256_extracti128_si256(src_32, 1));

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      i -= 4;
    } while (i);
  }
}

#define DO_AVG_2D_COPY_4X16(USE_DIST_WEIGHTED, r0, c0, r1, c1, r2, c2, r3, c3) \
  do {                                                                         \
    src_0 = _mm256_cvtepu8_epi16(                                              \
        _mm_loadu_si128((__m128i *)(&src[r0 * src_stride + c0])));             \
    src_1 = _mm256_cvtepu8_epi16(                                              \
        _mm_loadu_si128((__m128i *)(&src[r1 * src_stride + c1])));             \
    src_2 = _mm256_cvtepu8_epi16(                                              \
        _mm_loadu_si128((__m128i *)(&src[r2 * src_stride + c2])));             \
    src_3 = _mm256_cvtepu8_epi16(                                              \
        _mm_loadu_si128((__m128i *)(&src[r3 * src_stride + c3])));             \
                                                                               \
    src_0 = _mm256_slli_epi16(src_0, LEFT_SHIFT);                              \
    src_1 = _mm256_slli_epi16(src_1, LEFT_SHIFT);                              \
    src_2 = _mm256_slli_epi16(src_2, LEFT_SHIFT);                              \
    src_3 = _mm256_slli_epi16(src_3, LEFT_SHIFT);                              \
    src_0 = _mm256_add_epi16(src_0, offset_const);                             \
    src_1 = _mm256_add_epi16(src_1, offset_const);                             \
    src_2 = _mm256_add_epi16(src_2, offset_const);                             \
    src_3 = _mm256_add_epi16(src_3, offset_const);                             \
                                                                               \
    ref_0 = _mm256_loadu_si256((__m256i *)(&dst[r0 * dst_stride + c0]));       \
    ref_1 = _mm256_loadu_si256((__m256i *)(&dst[r1 * dst_stride + c1]));       \
    ref_2 = _mm256_loadu_si256((__m256i *)(&dst[r2 * dst_stride + c2]));       \
    ref_3 = _mm256_loadu_si256((__m256i *)(&dst[r3 * dst_stride + c3]));       \
                                                                               \
    res_0 = comp_avg(&ref_0, &src_0, &wt, USE_DIST_WEIGHTED);                  \
    res_1 = comp_avg(&ref_1, &src_1, &wt, USE_DIST_WEIGHTED);                  \
    res_2 = comp_avg(&ref_2, &src_2, &wt, USE_DIST_WEIGHTED);                  \
    res_3 = comp_avg(&ref_3, &src_3, &wt, USE_DIST_WEIGHTED);                  \
                                                                               \
    res_0 = convolve_rounding(&res_0, &offset_const, &rounding_const,          \
                              rounding_shift);                                 \
    res_1 = convolve_rounding(&res_1, &offset_const, &rounding_const,          \
                              rounding_shift);                                 \
    res_2 = convolve_rounding(&res_2, &offset_const, &rounding_const,          \
                              rounding_shift);                                 \
    res_3 = convolve_rounding(&res_3, &offset_const, &rounding_const,          \
                              rounding_shift);                                 \
                                                                               \
    res_10 = _mm256_packus_epi16(res_0, res_1);                                \
    res_32 = _mm256_packus_epi16(res_2, res_3);                                \
    res_10 = _mm256_permute4x64_epi64(res_10, 0xD8);                           \
    res_32 = _mm256_permute4x64_epi64(res_32, 0xD8);                           \
                                                                               \
    _mm_store_si128((__m128i *)(&dst0[r0 * dst_stride0 + c0]),                 \
                    _mm256_castsi256_si128(res_10));                           \
    _mm_store_si128((__m128i *)(&dst0[r1 * dst_stride0 + c1]),                 \
                    _mm256_extracti128_si256(res_10, 1));                      \
    _mm_store_si128((__m128i *)(&dst0[r2 * dst_stride0 + c2]),                 \
                    _mm256_castsi256_si128(res_32));                           \
    _mm_store_si128((__m128i *)(&dst0[r3 * dst_stride0 + c3]),                 \
                    _mm256_extracti128_si256(res_32, 1));                      \
  } while (0)

#define DO_AVG_2D_COPY(USE_DIST_WEIGHTED)                                     \
  int i = h;                                                                  \
  if (w >= 16) {                                                              \
    __m256i src_0, src_1, src_2, src_3;                                       \
    __m256i ref_0, ref_1, ref_2, ref_3;                                       \
    __m256i res_0, res_1, res_2, res_3;                                       \
    __m256i res_10, res_32;                                                   \
    if (w == 128) {                                                           \
      do {                                                                    \
        DO_AVG_2D_COPY_4X16(USE_DIST_WEIGHTED, 0, 0, 0, 16, 0, 32, 0, 48);    \
        DO_AVG_2D_COPY_4X16(USE_DIST_WEIGHTED, 0, 64, 0, 80, 0, 96, 0, 112);  \
        i -= 1;                                                               \
        src += 1 * src_stride;                                                \
        dst += 1 * dst_stride;                                                \
        dst0 += 1 * dst_stride0;                                              \
      } while (i);                                                            \
    } else if (w == 64) {                                                     \
      do {                                                                    \
        DO_AVG_2D_COPY_4X16(USE_DIST_WEIGHTED, 0, 0, 0, 16, 0, 32, 0, 48);    \
                                                                              \
        i -= 1;                                                               \
        src += 1 * src_stride;                                                \
        dst += 1 * dst_stride;                                                \
        dst0 += 1 * dst_stride0;                                              \
      } while (i);                                                            \
    } else if (w == 32) {                                                     \
      do {                                                                    \
        DO_AVG_2D_COPY_4X16(USE_DIST_WEIGHTED, 0, 0, 1, 0, 0, 16, 1, 16);     \
                                                                              \
        i -= 2;                                                               \
        src += 2 * src_stride;                                                \
        dst += 2 * dst_stride;                                                \
        dst0 += 2 * dst_stride0;                                              \
      } while (i);                                                            \
    } else {                                                                  \
      assert(w == 16);                                                        \
      do {                                                                    \
        DO_AVG_2D_COPY_4X16(USE_DIST_WEIGHTED, 0, 0, 1, 0, 2, 0, 3, 0);       \
                                                                              \
        i -= 4;                                                               \
        src += 4 * src_stride;                                                \
        dst += 4 * dst_stride;                                                \
        dst0 += 4 * dst_stride0;                                              \
      } while (i);                                                            \
    }                                                                         \
  } else if (w == 8) {                                                        \
    do {                                                                      \
      const __m128i src_0 =                                                   \
          _mm_loadl_epi64((__m128i *)(&src[0 * src_stride]));                 \
      const __m128i src_1 =                                                   \
          _mm_loadl_epi64((__m128i *)(&src[1 * src_stride]));                 \
      const __m128i src_2 =                                                   \
          _mm_loadl_epi64((__m128i *)(&src[2 * src_stride]));                 \
      const __m128i src_3 =                                                   \
          _mm_loadl_epi64((__m128i *)(&src[3 * src_stride]));                 \
      __m256i src_10 =                                                        \
          _mm256_insertf128_si256(_mm256_castsi128_si256(src_0), src_1, 1);   \
      __m256i src_32 =                                                        \
          _mm256_insertf128_si256(_mm256_castsi128_si256(src_2), src_3, 1);   \
                                                                              \
      src_10 = _mm256_unpacklo_epi8(src_10, zero);                            \
      src_32 = _mm256_unpacklo_epi8(src_32, zero);                            \
                                                                              \
      src_10 = _mm256_slli_epi16(src_10, LEFT_SHIFT);                         \
      src_32 = _mm256_slli_epi16(src_32, LEFT_SHIFT);                         \
                                                                              \
      src_10 = _mm256_add_epi16(src_10, offset_const);                        \
      src_32 = _mm256_add_epi16(src_32, offset_const);                        \
                                                                              \
      const __m256i ref_10 =                                                  \
          load_line2_avx2(&dst[0 * dst_stride], &dst[1 * dst_stride]);        \
      const __m256i ref_32 =                                                  \
          load_line2_avx2(&dst[2 * dst_stride], &dst[3 * dst_stride]);        \
      __m256i res_10 = comp_avg(&ref_10, &src_10, &wt, USE_DIST_WEIGHTED);    \
      __m256i res_32 = comp_avg(&ref_32, &src_32, &wt, USE_DIST_WEIGHTED);    \
                                                                              \
      res_10 = convolve_rounding(&res_10, &offset_const, &rounding_const,     \
                                 rounding_shift);                             \
      res_32 = convolve_rounding(&res_32, &offset_const, &rounding_const,     \
                                 rounding_shift);                             \
                                                                              \
      __m256i res = _mm256_packus_epi16(res_10, res_32);                      \
      const __m128i res_20 = _mm256_castsi256_si128(res);                     \
      const __m128i res_31 = _mm256_extracti128_si256(res, 1);                \
                                                                              \
      _mm_storel_epi64((__m128i *)(&dst0[0 * dst_stride0]), res_20);          \
      _mm_storel_epi64((__m128i *)((&dst0[1 * dst_stride0])), res_31);        \
      _mm_storeh_epi64((__m128i *)(&dst0[2 * dst_stride0]), res_20);          \
      _mm_storeh_epi64((__m128i *)((&dst0[3 * dst_stride0])), res_31);        \
      i -= 4;                                                                 \
      src += 4 * src_stride;                                                  \
      dst += 4 * dst_stride;                                                  \
      dst0 += 4 * dst_stride0;                                                \
    } while (i);                                                              \
  } else {                                                                    \
    assert(w == 4);                                                           \
    do {                                                                      \
      __m256i src_3210_8bit =                                                 \
          _mm256_setr_epi32(loadu_int32(src + 0 * src_stride),                \
                            loadu_int32(src + 1 * src_stride), 0, 0,          \
                            loadu_int32(src + 2 * src_stride),                \
                            loadu_int32(src + 3 * src_stride), 0, 0);         \
                                                                              \
      __m256i src_3210 = _mm256_unpacklo_epi8(src_3210_8bit, zero);           \
      src_3210 = _mm256_slli_epi16(src_3210, LEFT_SHIFT);                     \
      src_3210 = _mm256_add_epi16(src_3210, offset_const);                    \
                                                                              \
      __m256i ref_3210 =                                                      \
          _mm256_setr_epi64x(*(int64_t *)(dst + 0 * dst_stride),              \
                             *(int64_t *)(dst + 1 * dst_stride),              \
                             *(int64_t *)(dst + 2 * dst_stride),              \
                             *(int64_t *)(dst + 3 * dst_stride));             \
      __m256i res_3210 =                                                      \
          comp_avg(&ref_3210, &src_3210, &wt, USE_DIST_WEIGHTED);             \
                                                                              \
      res_3210 = convolve_rounding(&res_3210, &offset_const, &rounding_const, \
                                   rounding_shift);                           \
                                                                              \
      res_3210 = _mm256_packus_epi16(res_3210, res_3210);                     \
      const __m128i res_10 = _mm256_castsi256_si128(res_3210);                \
      const __m128i res_32 = _mm256_extracti128_si256(res_3210, 1);           \
                                                                              \
      *(int *)(&dst0[0 * dst_stride0]) = _mm_cvtsi128_si32(res_10);           \
      *(int *)(&dst0[2 * dst_stride0]) = _mm_cvtsi128_si32(res_32);           \
      *(int *)(&dst0[1 * dst_stride0]) = _mm_extract_epi32(res_10, 1);        \
      *(int *)(&dst0[3 * dst_stride0]) = _mm_extract_epi32(res_32, 1);        \
      i -= 4;                                                                 \
      src += 4 * src_stride;                                                  \
      dst += 4 * dst_stride;                                                  \
      dst0 += 4 * dst_stride0;                                                \
    } while (i);                                                              \
  }

void av1_dist_wtd_convolve_2d_copy_avx2(const uint8_t *src, int src_stride,
                                        uint8_t *dst0, int dst_stride0, int w,
                                        int h, ConvolveParams *conv_params) {
  const int bd = 8;
  CONV_BUF_TYPE *dst = conv_params->dst;
  int dst_stride = conv_params->dst_stride;
  assert(conv_params->round_0 == 3);
  assert(conv_params->round_1 == 7);
  assert(w % 4 == 0);
  assert(h % 4 == 0);

  const int do_average = conv_params->do_average;
  const int use_dist_wtd_comp_avg = conv_params->use_dist_wtd_comp_avg;
  const __m256i wt = unpack_weights_avx2(conv_params);
  const __m256i zero = _mm256_setzero_si256();

  const int offset_0 =
      bd + 2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const int offset = (1 << offset_0) + (1 << (offset_0 - 1));
  const __m256i offset_const = _mm256_set1_epi16(offset);
  const int rounding_shift =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const __m256i rounding_const = _mm256_set1_epi16((1 << rounding_shift) >> 1);

  if (do_average) {
    if (use_dist_wtd_comp_avg) {
      DO_AVG_2D_COPY(1)
    } else {
      DO_AVG_2D_COPY(0)
    }
  } else {
    av1_dist_wtd_convolve_2d_no_avg_copy_avx2(src, src_stride, dst, dst_stride,
                                              w, h, offset_const);
  }
}
#undef LEFT_SHIFT

/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved.
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <immintrin.h>

#include "config/av1_rtcd.h"

#include "third_party/SVT-AV1/convolve_avx2.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/x86/convolve_avx2.h"
#include "aom_dsp/x86/convolve_common_intrin.h"
#include "aom_dsp/x86/synonyms.h"

static AOM_INLINE void av1_convolve_y_sr_general_avx2(
    const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_y, const int subpel_y_qn) {
  // right shift is F-1 because we are already dividing
  // filter co-efficients by 2
  const int right_shift_bits = (FILTER_BITS - 1);
  __m128i right_shift = _mm_cvtsi32_si128(right_shift_bits);
  __m256i right_shift_const = _mm256_set1_epi16((1 << right_shift_bits) >> 1);

  __m256i coeffs[6], s[12];
  __m128i d[10];

  int i, vert_tap = get_filter_tap(filter_params_y, subpel_y_qn);

  if (vert_tap == 6)
    prepare_coeffs_6t_lowbd(filter_params_y, subpel_y_qn, coeffs);
  else if (vert_tap == 12) {
    prepare_coeffs_12taps(filter_params_y, subpel_y_qn, coeffs);
  } else {
    prepare_coeffs_lowbd(filter_params_y, subpel_y_qn, coeffs);
  }

  // vert_filt as 4 tap
  if (vert_tap == 4) {
    const int fo_vert = 1;
    const uint8_t *const src_ptr = src - fo_vert * src_stride;
    for (int j = 0; j < w; j += 16) {
      const uint8_t *data = &src_ptr[j];
      d[0] = _mm_loadu_si128((__m128i *)(data + 0 * src_stride));
      d[1] = _mm_loadu_si128((__m128i *)(data + 1 * src_stride));
      d[2] = _mm_loadu_si128((__m128i *)(data + 2 * src_stride));
      d[3] = _mm_loadu_si128((__m128i *)(data + 3 * src_stride));
      d[4] = _mm_loadu_si128((__m128i *)(data + 4 * src_stride));

      // Load lines a and b. Line a to lower 128, line b to upper 128
      const __m256i src_01a = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(d[0]), _mm256_castsi128_si256(d[1]), 0x20);

      const __m256i src_12a = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(d[1]), _mm256_castsi128_si256(d[2]), 0x20);

      const __m256i src_23a = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(d[2]), _mm256_castsi128_si256(d[3]), 0x20);

      const __m256i src_34a = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(d[3]), _mm256_castsi128_si256(d[4]), 0x20);

      s[0] = _mm256_unpacklo_epi8(src_01a, src_12a);
      s[1] = _mm256_unpacklo_epi8(src_23a, src_34a);

      s[3] = _mm256_unpackhi_epi8(src_01a, src_12a);
      s[4] = _mm256_unpackhi_epi8(src_23a, src_34a);

      for (i = 0; i < h; i += 2) {
        data = &src_ptr[i * src_stride + j];
        d[5] = _mm_loadu_si128((__m128i *)(data + 5 * src_stride));
        const __m256i src_45a = _mm256_permute2x128_si256(
            _mm256_castsi128_si256(d[4]), _mm256_castsi128_si256(d[5]), 0x20);

        d[4] = _mm_loadu_si128((__m128i *)(data + 6 * src_stride));
        const __m256i src_56a = _mm256_permute2x128_si256(
            _mm256_castsi128_si256(d[5]), _mm256_castsi128_si256(d[4]), 0x20);

        s[2] = _mm256_unpacklo_epi8(src_45a, src_56a);
        s[5] = _mm256_unpackhi_epi8(src_45a, src_56a);

        const __m256i res_lo = convolve_lowbd_4tap(s, coeffs + 1);
        /* rounding code */
        // shift by F - 1
        const __m256i res_16b_lo = _mm256_sra_epi16(
            _mm256_add_epi16(res_lo, right_shift_const), right_shift);
        // 8 bit conversion and saturation to uint8
        __m256i res_8b_lo = _mm256_packus_epi16(res_16b_lo, res_16b_lo);

        if (w - j > 8) {
          const __m256i res_hi = convolve_lowbd_4tap(s + 3, coeffs + 1);

          /* rounding code */
          // shift by F - 1
          const __m256i res_16b_hi = _mm256_sra_epi16(
              _mm256_add_epi16(res_hi, right_shift_const), right_shift);
          // 8 bit conversion and saturation to uint8
          __m256i res_8b_hi = _mm256_packus_epi16(res_16b_hi, res_16b_hi);

          __m256i res_a = _mm256_unpacklo_epi64(res_8b_lo, res_8b_hi);

          const __m128i res_0 = _mm256_castsi256_si128(res_a);
          const __m128i res_1 = _mm256_extracti128_si256(res_a, 1);

          _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j], res_0);
          _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j + dst_stride],
                           res_1);
        } else {
          const __m128i res_0 = _mm256_castsi256_si128(res_8b_lo);
          const __m128i res_1 = _mm256_extracti128_si256(res_8b_lo, 1);
          if (w - j > 4) {
            _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j], res_0);
            _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j + dst_stride],
                             res_1);
          } else if (w - j > 2) {
            xx_storel_32(&dst[i * dst_stride + j], res_0);
            xx_storel_32(&dst[i * dst_stride + j + dst_stride], res_1);
          } else {
            __m128i *const p_0 = (__m128i *)&dst[i * dst_stride + j];
            __m128i *const p_1 =
                (__m128i *)&dst[i * dst_stride + j + dst_stride];
            *(uint16_t *)p_0 = (uint16_t)_mm_cvtsi128_si32(res_0);
            *(uint16_t *)p_1 = (uint16_t)_mm_cvtsi128_si32(res_1);
          }
        }
        s[0] = s[1];
        s[1] = s[2];

        s[3] = s[4];
        s[4] = s[5];
      }
    }
  } else if (vert_tap == 6) {
    const int fo_vert = vert_tap / 2 - 1;
    const uint8_t *const src_ptr = src - fo_vert * src_stride;

    for (int j = 0; j < w; j += 16) {
      const uint8_t *data = &src_ptr[j];
      __m256i src6;

      d[0] = _mm_loadu_si128((__m128i *)(data + 0 * src_stride));
      d[1] = _mm_loadu_si128((__m128i *)(data + 1 * src_stride));
      d[2] = _mm_loadu_si128((__m128i *)(data + 2 * src_stride));
      d[3] = _mm_loadu_si128((__m128i *)(data + 3 * src_stride));
      // Load lines a and b. Line a to lower 128, line b to upper 128
      const __m256i src_01a = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(d[0]), _mm256_castsi128_si256(d[1]), 0x20);

      const __m256i src_12a = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(d[1]), _mm256_castsi128_si256(d[2]), 0x20);

      const __m256i src_23a = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(d[2]), _mm256_castsi128_si256(d[3]), 0x20);

      src6 = _mm256_castsi128_si256(
          _mm_loadu_si128((__m128i *)(data + 4 * src_stride)));
      const __m256i src_34a =
          _mm256_permute2x128_si256(_mm256_castsi128_si256(d[3]), src6, 0x20);

      s[0] = _mm256_unpacklo_epi8(src_01a, src_12a);
      s[1] = _mm256_unpacklo_epi8(src_23a, src_34a);

      s[3] = _mm256_unpackhi_epi8(src_01a, src_12a);
      s[4] = _mm256_unpackhi_epi8(src_23a, src_34a);

      for (i = 0; i < h; i += 2) {
        data = &src_ptr[i * src_stride + j];
        const __m256i src_45a = _mm256_permute2x128_si256(
            src6,
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(data + 5 * src_stride))),
            0x20);

        src6 = _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 6 * src_stride)));
        const __m256i src_56a = _mm256_permute2x128_si256(
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(data + 5 * src_stride))),
            src6, 0x20);

        s[2] = _mm256_unpacklo_epi8(src_45a, src_56a);
        s[5] = _mm256_unpackhi_epi8(src_45a, src_56a);

        const __m256i res_lo = convolve_lowbd_6tap(s, coeffs);

        /* rounding code */
        // shift by F - 1
        const __m256i res_16b_lo = _mm256_sra_epi16(
            _mm256_add_epi16(res_lo, right_shift_const), right_shift);
        // 8 bit conversion and saturation to uint8
        __m256i res_8b_lo = _mm256_packus_epi16(res_16b_lo, res_16b_lo);

        if (w - j > 8) {
          const __m256i res_hi = convolve_lowbd_6tap(s + 3, coeffs);

          /* rounding code */
          // shift by F - 1
          const __m256i res_16b_hi = _mm256_sra_epi16(
              _mm256_add_epi16(res_hi, right_shift_const), right_shift);
          // 8 bit conversion and saturation to uint8
          __m256i res_8b_hi = _mm256_packus_epi16(res_16b_hi, res_16b_hi);

          __m256i res_a = _mm256_unpacklo_epi64(res_8b_lo, res_8b_hi);

          const __m128i res_0 = _mm256_castsi256_si128(res_a);
          const __m128i res_1 = _mm256_extracti128_si256(res_a, 1);

          _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j], res_0);
          _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j + dst_stride],
                           res_1);
        } else {
          const __m128i res_0 = _mm256_castsi256_si128(res_8b_lo);
          const __m128i res_1 = _mm256_extracti128_si256(res_8b_lo, 1);
          if (w - j > 4) {
            _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j], res_0);
            _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j + dst_stride],
                             res_1);
          } else if (w - j > 2) {
            xx_storel_32(&dst[i * dst_stride + j], res_0);
            xx_storel_32(&dst[i * dst_stride + j + dst_stride], res_1);
          } else {
            __m128i *const p_0 = (__m128i *)&dst[i * dst_stride + j];
            __m128i *const p_1 =
                (__m128i *)&dst[i * dst_stride + j + dst_stride];
            *(uint16_t *)p_0 = (uint16_t)_mm_cvtsi128_si32(res_0);
            *(uint16_t *)p_1 = (uint16_t)_mm_cvtsi128_si32(res_1);
          }
        }
        s[0] = s[1];
        s[1] = s[2];
        s[3] = s[4];
        s[4] = s[5];
      }
    }
  } else if (vert_tap == 12) {  // vert_tap == 12
    const int fo_vert = filter_params_y->taps / 2 - 1;
    const uint8_t *const src_ptr = src - fo_vert * src_stride;
    const __m256i v_zero = _mm256_setzero_si256();
    right_shift = _mm_cvtsi32_si128(FILTER_BITS);
    right_shift_const = _mm256_set1_epi32((1 << FILTER_BITS) >> 1);

    for (int j = 0; j < w; j += 8) {
      const uint8_t *data = &src_ptr[j];
      __m256i src10;

      d[0] = _mm_loadl_epi64((__m128i *)(data + 0 * src_stride));
      d[1] = _mm_loadl_epi64((__m128i *)(data + 1 * src_stride));
      d[2] = _mm_loadl_epi64((__m128i *)(data + 2 * src_stride));
      d[3] = _mm_loadl_epi64((__m128i *)(data + 3 * src_stride));
      d[4] = _mm_loadl_epi64((__m128i *)(data + 4 * src_stride));
      d[5] = _mm_loadl_epi64((__m128i *)(data + 5 * src_stride));
      d[6] = _mm_loadl_epi64((__m128i *)(data + 6 * src_stride));
      d[7] = _mm_loadl_epi64((__m128i *)(data + 7 * src_stride));
      d[8] = _mm_loadl_epi64((__m128i *)(data + 8 * src_stride));
      d[9] = _mm_loadl_epi64((__m128i *)(data + 9 * src_stride));
      // Load lines a and b. Line a to lower 128, line b to upper 128
      const __m256i src_01a = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(d[0]), _mm256_castsi128_si256(d[1]), 0x20);

      const __m256i src_12a = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(d[1]), _mm256_castsi128_si256(d[2]), 0x20);

      const __m256i src_23a = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(d[2]), _mm256_castsi128_si256(d[3]), 0x20);

      const __m256i src_34a = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(d[3]), _mm256_castsi128_si256(d[4]), 0x20);

      const __m256i src_45a = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(d[4]), _mm256_castsi128_si256(d[5]), 0x20);

      const __m256i src_56a = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(d[5]), _mm256_castsi128_si256(d[6]), 0x20);

      const __m256i src_67a = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(d[6]), _mm256_castsi128_si256(d[7]), 0x20);

      const __m256i src_78a = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(d[7]), _mm256_castsi128_si256(d[8]), 0x20);

      const __m256i src_89a = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(d[8]), _mm256_castsi128_si256(d[9]), 0x20);

      src10 = _mm256_castsi128_si256(
          _mm_loadl_epi64((__m128i *)(data + 10 * src_stride)));
      const __m256i src_910a =
          _mm256_permute2x128_si256(_mm256_castsi128_si256(d[9]), src10, 0x20);

      const __m256i src_01 = _mm256_unpacklo_epi8(src_01a, v_zero);
      const __m256i src_12 = _mm256_unpacklo_epi8(src_12a, v_zero);
      const __m256i src_23 = _mm256_unpacklo_epi8(src_23a, v_zero);
      const __m256i src_34 = _mm256_unpacklo_epi8(src_34a, v_zero);
      const __m256i src_45 = _mm256_unpacklo_epi8(src_45a, v_zero);
      const __m256i src_56 = _mm256_unpacklo_epi8(src_56a, v_zero);
      const __m256i src_67 = _mm256_unpacklo_epi8(src_67a, v_zero);
      const __m256i src_78 = _mm256_unpacklo_epi8(src_78a, v_zero);
      const __m256i src_89 = _mm256_unpacklo_epi8(src_89a, v_zero);
      const __m256i src_910 = _mm256_unpacklo_epi8(src_910a, v_zero);

      s[0] = _mm256_unpacklo_epi16(src_01, src_12);
      s[1] = _mm256_unpacklo_epi16(src_23, src_34);
      s[2] = _mm256_unpacklo_epi16(src_45, src_56);
      s[3] = _mm256_unpacklo_epi16(src_67, src_78);
      s[4] = _mm256_unpacklo_epi16(src_89, src_910);

      s[6] = _mm256_unpackhi_epi16(src_01, src_12);
      s[7] = _mm256_unpackhi_epi16(src_23, src_34);
      s[8] = _mm256_unpackhi_epi16(src_45, src_56);
      s[9] = _mm256_unpackhi_epi16(src_67, src_78);
      s[10] = _mm256_unpackhi_epi16(src_89, src_910);

      for (i = 0; i < h; i += 2) {
        data = &src_ptr[i * src_stride + j];
        const __m256i src_1011a = _mm256_permute2x128_si256(
            src10,
            _mm256_castsi128_si256(
                _mm_loadl_epi64((__m128i *)(data + 11 * src_stride))),
            0x20);

        src10 = _mm256_castsi128_si256(
            _mm_loadl_epi64((__m128i *)(data + 12 * src_stride)));

        const __m256i src_1112a = _mm256_permute2x128_si256(
            _mm256_castsi128_si256(
                _mm_loadl_epi64((__m128i *)(data + 11 * src_stride))),
            src10, 0x20);

        const __m256i src_1011 = _mm256_unpacklo_epi8(src_1011a, v_zero);
        const __m256i src_1112 = _mm256_unpacklo_epi8(src_1112a, v_zero);

        s[5] = _mm256_unpacklo_epi16(src_1011, src_1112);
        s[11] = _mm256_unpackhi_epi16(src_1011, src_1112);

        const __m256i res_lo = convolve_12taps(s, coeffs);

        const __m256i res_32b_lo = _mm256_sra_epi32(
            _mm256_add_epi32(res_lo, right_shift_const), right_shift);
        // 8 bit conversion and saturation to uint8
        __m256i res_16b_lo = _mm256_packs_epi32(res_32b_lo, res_32b_lo);
        __m256i res_8b_lo = _mm256_packus_epi16(res_16b_lo, res_16b_lo);

        if (w - j > 4) {
          const __m256i res_hi = convolve_12taps(s + 6, coeffs);

          const __m256i res_32b_hi = _mm256_sra_epi32(
              _mm256_add_epi32(res_hi, right_shift_const), right_shift);
          __m256i res_16b_hi = _mm256_packs_epi32(res_32b_hi, res_32b_hi);
          // 8 bit conversion and saturation to uint8
          __m256i res_8b_hi = _mm256_packus_epi16(res_16b_hi, res_16b_hi);

          __m256i res_a = _mm256_unpacklo_epi32(res_8b_lo, res_8b_hi);

          const __m128i res_0 = _mm256_extracti128_si256(res_a, 0);
          const __m128i res_1 = _mm256_extracti128_si256(res_a, 1);

          _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j], res_0);
          _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j + dst_stride],
                           res_1);
        } else {
          const __m128i res_0 = _mm256_extracti128_si256(res_8b_lo, 0);
          const __m128i res_1 = _mm256_extracti128_si256(res_8b_lo, 1);
          if (w - j > 2) {
            *(int *)&dst[i * dst_stride + j] = _mm_cvtsi128_si32(res_0);
            *(int *)&dst[i * dst_stride + j + dst_stride] =
                _mm_cvtsi128_si32(res_1);
          } else {
            *(uint16_t *)&dst[i * dst_stride + j] =
                (uint16_t)_mm_cvtsi128_si32(res_0);
            *(uint16_t *)&dst[i * dst_stride + j + dst_stride] =
                (uint16_t)_mm_cvtsi128_si32(res_1);
          }
        }
        s[0] = s[1];
        s[1] = s[2];
        s[2] = s[3];
        s[3] = s[4];
        s[4] = s[5];

        s[6] = s[7];
        s[7] = s[8];
        s[8] = s[9];
        s[9] = s[10];
        s[10] = s[11];
      }
    }
  } else {
    const int fo_vert = filter_params_y->taps / 2 - 1;
    const uint8_t *const src_ptr = src - fo_vert * src_stride;

    for (int j = 0; j < w; j += 16) {
      const uint8_t *data = &src_ptr[j];
      __m256i src6;

      d[0] = _mm_loadu_si128((__m128i *)(data + 0 * src_stride));
      d[1] = _mm_loadu_si128((__m128i *)(data + 1 * src_stride));
      d[2] = _mm_loadu_si128((__m128i *)(data + 2 * src_stride));
      d[3] = _mm_loadu_si128((__m128i *)(data + 3 * src_stride));
      d[4] = _mm_loadu_si128((__m128i *)(data + 4 * src_stride));
      d[5] = _mm_loadu_si128((__m128i *)(data + 5 * src_stride));
      // Load lines a and b. Line a to lower 128, line b to upper 128
      const __m256i src_01a = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(d[0]), _mm256_castsi128_si256(d[1]), 0x20);

      const __m256i src_12a = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(d[1]), _mm256_castsi128_si256(d[2]), 0x20);

      const __m256i src_23a = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(d[2]), _mm256_castsi128_si256(d[3]), 0x20);

      const __m256i src_34a = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(d[3]), _mm256_castsi128_si256(d[4]), 0x20);

      const __m256i src_45a = _mm256_permute2x128_si256(
          _mm256_castsi128_si256(d[4]), _mm256_castsi128_si256(d[5]), 0x20);

      src6 = _mm256_castsi128_si256(
          _mm_loadu_si128((__m128i *)(data + 6 * src_stride)));
      const __m256i src_56a =
          _mm256_permute2x128_si256(_mm256_castsi128_si256(d[5]), src6, 0x20);

      s[0] = _mm256_unpacklo_epi8(src_01a, src_12a);
      s[1] = _mm256_unpacklo_epi8(src_23a, src_34a);
      s[2] = _mm256_unpacklo_epi8(src_45a, src_56a);

      s[4] = _mm256_unpackhi_epi8(src_01a, src_12a);
      s[5] = _mm256_unpackhi_epi8(src_23a, src_34a);
      s[6] = _mm256_unpackhi_epi8(src_45a, src_56a);

      for (i = 0; i < h; i += 2) {
        data = &src_ptr[i * src_stride + j];
        const __m256i src_67a = _mm256_permute2x128_si256(
            src6,
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(data + 7 * src_stride))),
            0x20);

        src6 = _mm256_castsi128_si256(
            _mm_loadu_si128((__m128i *)(data + 8 * src_stride)));
        const __m256i src_78a = _mm256_permute2x128_si256(
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(data + 7 * src_stride))),
            src6, 0x20);

        s[3] = _mm256_unpacklo_epi8(src_67a, src_78a);
        s[7] = _mm256_unpackhi_epi8(src_67a, src_78a);

        const __m256i res_lo = convolve_lowbd(s, coeffs);

        /* rounding code */
        // shift by F - 1
        const __m256i res_16b_lo = _mm256_sra_epi16(
            _mm256_add_epi16(res_lo, right_shift_const), right_shift);
        // 8 bit conversion and saturation to uint8
        __m256i res_8b_lo = _mm256_packus_epi16(res_16b_lo, res_16b_lo);

        if (w - j > 8) {
          const __m256i res_hi = convolve_lowbd(s + 4, coeffs);

          /* rounding code */
          // shift by F - 1
          const __m256i res_16b_hi = _mm256_sra_epi16(
              _mm256_add_epi16(res_hi, right_shift_const), right_shift);
          // 8 bit conversion and saturation to uint8
          __m256i res_8b_hi = _mm256_packus_epi16(res_16b_hi, res_16b_hi);

          __m256i res_a = _mm256_unpacklo_epi64(res_8b_lo, res_8b_hi);

          const __m128i res_0 = _mm256_castsi256_si128(res_a);
          const __m128i res_1 = _mm256_extracti128_si256(res_a, 1);

          _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j], res_0);
          _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j + dst_stride],
                           res_1);
        } else {
          const __m128i res_0 = _mm256_castsi256_si128(res_8b_lo);
          const __m128i res_1 = _mm256_extracti128_si256(res_8b_lo, 1);
          if (w - j > 4) {
            _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j], res_0);
            _mm_storel_epi64((__m128i *)&dst[i * dst_stride + j + dst_stride],
                             res_1);
          } else if (w - j > 2) {
            xx_storel_32(&dst[i * dst_stride + j], res_0);
            xx_storel_32(&dst[i * dst_stride + j + dst_stride], res_1);
          } else {
            __m128i *const p_0 = (__m128i *)&dst[i * dst_stride + j];
            __m128i *const p_1 =
                (__m128i *)&dst[i * dst_stride + j + dst_stride];
            *(uint16_t *)p_0 = (uint16_t)_mm_cvtsi128_si32(res_0);
            *(uint16_t *)p_1 = (uint16_t)_mm_cvtsi128_si32(res_1);
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

void av1_convolve_y_sr_avx2(const uint8_t *src, int32_t src_stride,
                            uint8_t *dst, int32_t dst_stride, int32_t w,
                            int32_t h,
                            const InterpFilterParams *filter_params_y,
                            const int32_t subpel_y_q4) {
  const int vert_tap = get_filter_tap(filter_params_y, subpel_y_q4);

  if (vert_tap == 12) {
    av1_convolve_y_sr_general_avx2(src, src_stride, dst, dst_stride, w, h,
                                   filter_params_y, subpel_y_q4);
  } else {
    av1_convolve_y_sr_specialized_avx2(src, src_stride, dst, dst_stride, w, h,
                                       filter_params_y, subpel_y_q4);
  }
}

static AOM_INLINE void av1_convolve_x_sr_general_avx2(
    const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w,
    int h, const InterpFilterParams *filter_params_x, const int subpel_x_qn,
    ConvolveParams *conv_params) {
  const int bits = FILTER_BITS - conv_params->round_0;
  const __m128i round_shift = _mm_cvtsi32_si128(bits);
  __m256i round_0_const =
      _mm256_set1_epi16((1 << (conv_params->round_0 - 1)) >> 1);
  __m128i round_0_shift = _mm_cvtsi32_si128(conv_params->round_0 - 1);
  __m256i round_const = _mm256_set1_epi16((1 << bits) >> 1);
  int i, horiz_tap = get_filter_tap(filter_params_x, subpel_x_qn);

  assert(bits >= 0);
  assert((FILTER_BITS - conv_params->round_1) >= 0 ||
         ((conv_params->round_0 + conv_params->round_1) == 2 * FILTER_BITS));
  assert(conv_params->round_0 > 0);

  __m256i coeffs[6], filt[4];
  filt[0] = _mm256_load_si256((__m256i const *)(filt_global_avx2));
  filt[1] = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32));

  if (horiz_tap == 6)
    prepare_coeffs_6t_lowbd(filter_params_x, subpel_x_qn, coeffs);
  else if (horiz_tap == 12) {
    prepare_coeffs_12taps(filter_params_x, subpel_x_qn, coeffs);
  } else {
    prepare_coeffs_lowbd(filter_params_x, subpel_x_qn, coeffs);
  }

  // horz_filt as 4 tap
  if (horiz_tap == 4) {
    const int fo_horiz = 1;
    const uint8_t *const src_ptr = src - fo_horiz;
    if (w <= 8) {
      for (i = 0; i < h; i += 2) {
        const __m256i data = _mm256_permute2x128_si256(
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(&src_ptr[i * src_stride]))),
            _mm256_castsi128_si256(_mm_loadu_si128(
                (__m128i *)(&src_ptr[i * src_stride + src_stride]))),
            0x20);

        __m256i res_16b = convolve_lowbd_x_4tap(data, coeffs + 1, filt);

        res_16b = _mm256_sra_epi16(_mm256_add_epi16(res_16b, round_0_const),
                                   round_0_shift);

        res_16b = _mm256_sra_epi16(_mm256_add_epi16(res_16b, round_const),
                                   round_shift);

        /* rounding code */
        // 8 bit conversion and saturation to uint8
        __m256i res_8b = _mm256_packus_epi16(res_16b, res_16b);

        const __m128i res_0 = _mm256_castsi256_si128(res_8b);
        const __m128i res_1 = _mm256_extracti128_si256(res_8b, 1);

        if (w > 4) {
          _mm_storel_epi64((__m128i *)&dst[i * dst_stride], res_0);
          _mm_storel_epi64((__m128i *)&dst[i * dst_stride + dst_stride], res_1);
        } else if (w > 2) {
          xx_storel_32(&dst[i * dst_stride], res_0);
          xx_storel_32(&dst[i * dst_stride + dst_stride], res_1);
        } else {
          __m128i *const p_0 = (__m128i *)&dst[i * dst_stride];
          __m128i *const p_1 = (__m128i *)&dst[i * dst_stride + dst_stride];
          *(uint16_t *)p_0 = (uint16_t)_mm_cvtsi128_si32(res_0);
          *(uint16_t *)p_1 = (uint16_t)_mm_cvtsi128_si32(res_1);
        }
      }
    } else {
      for (i = 0; i < h; ++i) {
        for (int j = 0; j < w; j += 16) {
          // 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 8 9 10 11 12 13 14 15 16 17
          // 18 19 20 21 22 23
          const __m256i data = _mm256_inserti128_si256(
              _mm256_loadu_si256((__m256i *)&src_ptr[(i * src_stride) + j]),
              _mm_loadu_si128((__m128i *)&src_ptr[(i * src_stride) + (j + 8)]),
              1);

          __m256i res_16b = convolve_lowbd_x_4tap(data, coeffs + 1, filt);

          res_16b = _mm256_sra_epi16(_mm256_add_epi16(res_16b, round_0_const),
                                     round_0_shift);

          res_16b = _mm256_sra_epi16(_mm256_add_epi16(res_16b, round_const),
                                     round_shift);

          /* rounding code */
          // 8 bit conversion and saturation to uint8
          __m256i res_8b = _mm256_packus_epi16(res_16b, res_16b);

          // Store values into the destination buffer
          // 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
          res_8b = _mm256_permute4x64_epi64(res_8b, 216);
          __m128i res = _mm256_castsi256_si128(res_8b);
          _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j], res);
        }
      }
    }
  } else if (horiz_tap == 6) {
    const int fo_horiz = horiz_tap / 2 - 1;
    const uint8_t *const src_ptr = src - fo_horiz;
    filt[2] = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32 * 2));
    filt[3] = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32 * 3));

    if (w <= 8) {
      for (i = 0; i < h; i += 2) {
        const __m256i data = _mm256_permute2x128_si256(
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(&src_ptr[i * src_stride]))),
            _mm256_castsi128_si256(_mm_loadu_si128(
                (__m128i *)(&src_ptr[i * src_stride + src_stride]))),
            0x20);

        __m256i res_16b = convolve_lowbd_x_6tap(data, coeffs, filt);

        res_16b = _mm256_sra_epi16(_mm256_add_epi16(res_16b, round_0_const),
                                   round_0_shift);

        res_16b = _mm256_sra_epi16(_mm256_add_epi16(res_16b, round_const),
                                   round_shift);

        /* rounding code */
        // 8 bit conversion and saturation to uint8
        __m256i res_8b = _mm256_packus_epi16(res_16b, res_16b);

        const __m128i res_0 = _mm256_castsi256_si128(res_8b);
        const __m128i res_1 = _mm256_extracti128_si256(res_8b, 1);
        if (w > 4) {
          _mm_storel_epi64((__m128i *)&dst[i * dst_stride], res_0);
          _mm_storel_epi64((__m128i *)&dst[i * dst_stride + dst_stride], res_1);
        } else if (w > 2) {
          xx_storel_32(&dst[i * dst_stride], res_0);
          xx_storel_32(&dst[i * dst_stride + dst_stride], res_1);
        } else {
          __m128i *const p_0 = (__m128i *)&dst[i * dst_stride];
          __m128i *const p_1 = (__m128i *)&dst[i * dst_stride + dst_stride];
          *(uint16_t *)p_0 = _mm_cvtsi128_si32(res_0);
          *(uint16_t *)p_1 = _mm_cvtsi128_si32(res_1);
        }
      }
    } else {
      for (i = 0; i < h; ++i) {
        for (int j = 0; j < w; j += 16) {
          // 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 8 9 10 11 12 13 14 15 16 17
          // 18 19 20 21 22 23
          const __m256i data = _mm256_inserti128_si256(
              _mm256_loadu_si256((__m256i *)&src_ptr[(i * src_stride) + j]),
              _mm_loadu_si128((__m128i *)&src_ptr[(i * src_stride) + (j + 8)]),
              1);

          __m256i res_16b = convolve_lowbd_x_6tap(data, coeffs, filt);

          res_16b = _mm256_sra_epi16(_mm256_add_epi16(res_16b, round_0_const),
                                     round_0_shift);

          res_16b = _mm256_sra_epi16(_mm256_add_epi16(res_16b, round_const),
                                     round_shift);

          /* rounding code */
          // 8 bit conversion and saturation to uint8
          __m256i res_8b = _mm256_packus_epi16(res_16b, res_16b);

          // Store values into the destination buffer
          // 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
          res_8b = _mm256_permute4x64_epi64(res_8b, 216);
          __m128i res = _mm256_castsi256_si128(res_8b);
          _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j], res);
        }
      }
    }
  } else if (horiz_tap == 12) {  // horiz_tap == 12
    const int fo_horiz = filter_params_x->taps / 2 - 1;
    const uint8_t *const src_ptr = src - fo_horiz;
    const __m256i v_zero = _mm256_setzero_si256();
    round_0_const = _mm256_set1_epi32((1 << (conv_params->round_0)) >> 1);
    round_const = _mm256_set1_epi32((1 << bits) >> 1);
    round_0_shift = _mm_cvtsi32_si128(conv_params->round_0);
    __m256i s[6];

    if (w <= 4) {
      for (i = 0; i < h; i += 2) {
        const __m256i data = _mm256_permute2x128_si256(
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(&src_ptr[i * src_stride]))),
            _mm256_castsi128_si256(_mm_loadu_si128(
                (__m128i *)(&src_ptr[i * src_stride + src_stride]))),
            0x20);
        // row0 0..7 row1 0..7
        const __m256i s_16lo = _mm256_unpacklo_epi8(data, v_zero);
        // row0 8..F row1 8..F
        const __m256i s_16hi = _mm256_unpackhi_epi8(data, v_zero);

        // row0 00 00 01 01 .. 03 03 row1 00 00 01 01 .. 03 03
        const __m256i s_lolo = _mm256_unpacklo_epi16(s_16lo, s_16lo);
        // row0 04 04 .. 07 07 row1 04 04 .. 07 07
        const __m256i s_lohi = _mm256_unpackhi_epi16(s_16lo, s_16lo);

        // row0 08 08 09 09 .. 0B 0B row1 08 08 09 09 .. 0B 0B
        const __m256i s_hilo = _mm256_unpacklo_epi16(s_16hi, s_16hi);
        // row0 0C 0C .. 0F 0F row1 0C 0C .. 0F 0F
        const __m256i s_hihi = _mm256_unpackhi_epi16(s_16hi, s_16hi);

        // 00 01 01 02 02 03 03 04 10 11 11 12 12 13 13 14
        s[0] = _mm256_alignr_epi8(s_lohi, s_lolo, 2);
        // 02 03 03 04 04 05 05 06 12 13 13 14 14 15 15 16
        s[1] = _mm256_alignr_epi8(s_lohi, s_lolo, 10);
        // 04 05 05 06 06 07 07 08 14 15 15 16 16 17 17 18
        s[2] = _mm256_alignr_epi8(s_hilo, s_lohi, 2);
        // 06 07 07 08 08 09 09 0A 16 17 17 18 18 19 19 1A
        s[3] = _mm256_alignr_epi8(s_hilo, s_lohi, 10);
        // 08 09 09 0A 0A 0B 0B 0C 18 19 19 1A 1A 1B 1B 1C
        s[4] = _mm256_alignr_epi8(s_hihi, s_hilo, 2);
        // 0A 0B 0B 0C 0C 0D 0D 0E 1A 1B 1B 1C 1C 1D 1D 1E
        s[5] = _mm256_alignr_epi8(s_hihi, s_hilo, 10);

        const __m256i res_lo = convolve_12taps(s, coeffs);

        __m256i res_32b_lo = _mm256_sra_epi32(
            _mm256_add_epi32(res_lo, round_0_const), round_0_shift);

        // 00 01 02 03 10 12 13 14
        res_32b_lo = _mm256_sra_epi32(_mm256_add_epi32(res_32b_lo, round_const),
                                      round_shift);
        // 8 bit conversion and saturation to uint8
        // 00 01 02 03 00 01 02 03 10 11 12 13 10 11 12 13
        __m256i res_16b_lo = _mm256_packs_epi32(res_32b_lo, res_32b_lo);
        // 00 01 02 03 00 01 02 03 00 01 02 03 00 01 02 03
        // 10 11 12 13 10 11 12 13 10 11 12 13 10 11 12 13
        __m256i res_8b_lo = _mm256_packus_epi16(res_16b_lo, res_16b_lo);

        // 00 01 02 03 00 01 02 03 00 01 02 03 00 01 02 03
        const __m128i res_0 = _mm256_extracti128_si256(res_8b_lo, 0);
        // 10 11 12 13 10 11 12 13 10 11 12 13 10 11 12 13
        const __m128i res_1 = _mm256_extracti128_si256(res_8b_lo, 1);
        if (w > 2) {
          // 00 01 02 03
          *(int *)&dst[i * dst_stride] = _mm_cvtsi128_si32(res_0);
          // 10 11 12 13
          *(int *)&dst[i * dst_stride + dst_stride] = _mm_cvtsi128_si32(res_1);
        } else {
          // 00 01
          *(uint16_t *)&dst[i * dst_stride] =
              (uint16_t)_mm_cvtsi128_si32(res_0);
          // 10 11
          *(uint16_t *)&dst[i * dst_stride + dst_stride] =
              (uint16_t)_mm_cvtsi128_si32(res_1);
        }
      }
    } else {
      for (i = 0; i < h; i++) {
        for (int j = 0; j < w; j += 8) {
          const __m256i data = _mm256_permute2x128_si256(
              _mm256_castsi128_si256(
                  _mm_loadu_si128((__m128i *)(&src_ptr[i * src_stride + j]))),
              _mm256_castsi128_si256(_mm_loadu_si128(
                  (__m128i *)(&src_ptr[i * src_stride + j + 4]))),
              0x20);
          // row0 0..7 4..B
          const __m256i s_16lo = _mm256_unpacklo_epi8(data, v_zero);
          // row0 8..F C..13
          const __m256i s_16hi = _mm256_unpackhi_epi8(data, v_zero);

          // row0 00 00 01 01 .. 03 03 04 04 05 05 .. 07 07
          const __m256i s_lolo = _mm256_unpacklo_epi16(s_16lo, s_16lo);
          // row0 04 04 .. 07 07 08 08 .. 0B 0B
          const __m256i s_lohi = _mm256_unpackhi_epi16(s_16lo, s_16lo);

          // row0 08 08 09 09 .. 0B 0B 0C 0C 0D 0D .. 0F 0F
          const __m256i s_hilo = _mm256_unpacklo_epi16(s_16hi, s_16hi);
          // row0 0C 0C 0D 0D .. 0F 0F 10 10 11 11 .. 13 13
          const __m256i s_hihi = _mm256_unpackhi_epi16(s_16hi, s_16hi);

          s[0] = _mm256_alignr_epi8(s_lohi, s_lolo, 2);
          s[1] = _mm256_alignr_epi8(s_lohi, s_lolo, 10);
          s[2] = _mm256_alignr_epi8(s_hilo, s_lohi, 2);
          s[3] = _mm256_alignr_epi8(s_hilo, s_lohi, 10);
          s[4] = _mm256_alignr_epi8(s_hihi, s_hilo, 2);
          s[5] = _mm256_alignr_epi8(s_hihi, s_hilo, 10);

          const __m256i res_lo = convolve_12taps(s, coeffs);

          __m256i res_32b_lo = _mm256_sra_epi32(
              _mm256_add_epi32(res_lo, round_0_const), round_0_shift);

          res_32b_lo = _mm256_sra_epi32(
              _mm256_add_epi32(res_32b_lo, round_const), round_shift);
          // 8 bit conversion and saturation to uint8
          __m256i res_16b_lo = _mm256_packs_epi32(res_32b_lo, res_32b_lo);
          __m256i res_8b_lo = _mm256_packus_epi16(res_16b_lo, res_16b_lo);
          const __m128i res_0 = _mm256_extracti128_si256(res_8b_lo, 0);
          const __m128i res_1 = _mm256_extracti128_si256(res_8b_lo, 1);
          *(int *)&dst[i * dst_stride + j] = _mm_cvtsi128_si32(res_0);
          *(int *)&dst[i * dst_stride + j + 4] = _mm_cvtsi128_si32(res_1);
        }
      }
    }
  } else {
    const int fo_horiz = filter_params_x->taps / 2 - 1;
    const uint8_t *const src_ptr = src - fo_horiz;
    filt[2] = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32 * 2));
    filt[3] = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32 * 3));

    if (w <= 8) {
      for (i = 0; i < h; i += 2) {
        const __m256i data = _mm256_permute2x128_si256(
            _mm256_castsi128_si256(
                _mm_loadu_si128((__m128i *)(&src_ptr[i * src_stride]))),
            _mm256_castsi128_si256(_mm_loadu_si128(
                (__m128i *)(&src_ptr[i * src_stride + src_stride]))),
            0x20);

        __m256i res_16b = convolve_lowbd_x(data, coeffs, filt);

        res_16b = _mm256_sra_epi16(_mm256_add_epi16(res_16b, round_0_const),
                                   round_0_shift);

        res_16b = _mm256_sra_epi16(_mm256_add_epi16(res_16b, round_const),
                                   round_shift);

        /* rounding code */
        // 8 bit conversion and saturation to uint8
        __m256i res_8b = _mm256_packus_epi16(res_16b, res_16b);

        const __m128i res_0 = _mm256_castsi256_si128(res_8b);
        const __m128i res_1 = _mm256_extracti128_si256(res_8b, 1);
        if (w > 4) {
          _mm_storel_epi64((__m128i *)&dst[i * dst_stride], res_0);
          _mm_storel_epi64((__m128i *)&dst[i * dst_stride + dst_stride], res_1);
        } else if (w > 2) {
          xx_storel_32(&dst[i * dst_stride], res_0);
          xx_storel_32(&dst[i * dst_stride + dst_stride], res_1);
        } else {
          __m128i *const p_0 = (__m128i *)&dst[i * dst_stride];
          __m128i *const p_1 = (__m128i *)&dst[i * dst_stride + dst_stride];
          *(uint16_t *)p_0 = (uint16_t)_mm_cvtsi128_si32(res_0);
          *(uint16_t *)p_1 = (uint16_t)_mm_cvtsi128_si32(res_1);
        }
      }
    } else {
      for (i = 0; i < h; ++i) {
        for (int j = 0; j < w; j += 16) {
          // 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 8 9 10 11 12 13 14 15 16 17
          // 18 19 20 21 22 23
          const __m256i data = _mm256_inserti128_si256(
              _mm256_loadu_si256((__m256i *)&src_ptr[(i * src_stride) + j]),
              _mm_loadu_si128((__m128i *)&src_ptr[(i * src_stride) + (j + 8)]),
              1);

          __m256i res_16b = convolve_lowbd_x(data, coeffs, filt);

          res_16b = _mm256_sra_epi16(_mm256_add_epi16(res_16b, round_0_const),
                                     round_0_shift);

          res_16b = _mm256_sra_epi16(_mm256_add_epi16(res_16b, round_const),
                                     round_shift);

          /* rounding code */
          // 8 bit conversion and saturation to uint8
          __m256i res_8b = _mm256_packus_epi16(res_16b, res_16b);

          // Store values into the destination buffer
          // 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
          res_8b = _mm256_permute4x64_epi64(res_8b, 216);
          __m128i res = _mm256_castsi256_si128(res_8b);
          _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j], res);
        }
      }
    }
  }
}

void av1_convolve_x_sr_avx2(const uint8_t *src, int32_t src_stride,
                            uint8_t *dst, int32_t dst_stride, int32_t w,
                            int32_t h,
                            const InterpFilterParams *filter_params_x,
                            const int32_t subpel_x_q4,
                            ConvolveParams *conv_params) {
  const int horz_tap = get_filter_tap(filter_params_x, subpel_x_q4);

  if (horz_tap == 12) {
    av1_convolve_x_sr_general_avx2(src, src_stride, dst, dst_stride, w, h,
                                   filter_params_x, subpel_x_q4, conv_params);
  } else {
    av1_convolve_x_sr_specialized_avx2(src, src_stride, dst, dst_stride, w, h,
                                       filter_params_x, subpel_x_q4,
                                       conv_params);
  }
}

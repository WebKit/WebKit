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

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/x86/convolve_avx2.h"
#include "aom_dsp/x86/convolve_common_intrin.h"
#include "aom_dsp/x86/synonyms.h"

void av1_convolve_y_sr_avx2(const uint8_t *src, int32_t src_stride,
                            uint8_t *dst, int32_t dst_stride, int32_t w,
                            int32_t h,
                            const InterpFilterParams *filter_params_y,
                            const int32_t subpel_y_qn) {
  __m128i coeffs_128[4];
  __m256i coeffs[6];
  int x = 0, y = h;

  int i, vert_tap = get_filter_tap(filter_params_y, subpel_y_qn);
  assert(vert_tap == 2 || vert_tap == 4 || vert_tap == 6 || vert_tap == 8 ||
         vert_tap == 12);
  assert(!(w % 2));
  assert(!(h % 2));

  const int fo_vert = vert_tap / 2 - 1;
  const uint8_t *const src_ptr = src - fo_vert * src_stride;
  const uint8_t *data = src_ptr;
  uint8_t *dst_ptr = dst;

  if (vert_tap == 2) {
    if (subpel_y_qn != 8) {
      if (w <= 4) {
        prepare_coeffs_2t_ssse3(filter_params_y, subpel_y_qn, coeffs_128);
        __m128i d[2], res;
        if (w == 2) {
          d[0] = _mm_cvtsi32_si128(loadu_int16(data));

          do {
            convolve_y_2tap_2x2_ssse3(data, src_stride, coeffs_128, d, &res);
            res = round_sr_y_ssse3(res);
            pack_store_u8_2x2_sse2(res, dst_ptr, dst_stride);

            dst_ptr += 2 * dst_stride;
            data += 2 * src_stride;
            y -= 2;
          } while (y > 0);
        } else {
          assert(w == 4);
          d[0] = _mm_cvtsi32_si128(loadu_int32(data));

          do {
            convolve_y_2tap_4x2_ssse3(data, src_stride, coeffs_128, d, &res);
            res = round_sr_y_ssse3(res);
            pack_store_u8_4x2_sse2(res, dst_ptr, dst_stride);

            dst_ptr += 2 * dst_stride;
            data += 2 * src_stride;
            y -= 2;
          } while (y > 0);
        }
      } else {
        prepare_coeffs_2t_lowbd(filter_params_y, subpel_y_qn, coeffs);

        if (w == 8) {
          __m128i d[2];
          d[0] = _mm_loadl_epi64((__m128i *)data);

          do {
            __m256i res;
            convolve_y_2tap_8x2_avx2(data, src_stride, coeffs, d, &res);
            round_pack_store_y_8x2_avx2(res, dst_ptr, dst_stride);

            dst_ptr += 2 * dst_stride;
            data += 2 * src_stride;
            y -= 2;

          } while (y > 0);

        } else if (w == 16) {
          __m128i d[2];
          d[0] = _mm_loadu_si128((__m128i *)data);

          do {
            __m256i res[2];
            convolve_y_2tap_16x2_avx2(data, src_stride, coeffs, d, res);
            round_pack_store_y_16x2_avx2(res, dst_ptr, dst_stride);

            dst_ptr += 2 * dst_stride;
            data += 2 * src_stride;
            y -= 2;
          } while (y > 0);

        } else {
          assert(!(w % 32));

          __m256i d[2];
          do {
            data = src_ptr + x;
            dst_ptr = dst + x;
            y = h;

            d[0] = _mm256_loadu_si256((__m256i *)data);

            do {
              __m256i res[4];
              convolve_y_2tap_32x2_avx2(data, src_stride, coeffs, d, res);
              round_pack_store_y_32x2_avx2(res, dst_ptr, dst_stride);

              dst_ptr += 2 * dst_stride;
              data += 2 * src_stride;
              y -= 2;
            } while (y > 0);

            x += 32;
          } while (x < w);
        }
      }
    } else {
      if (w <= 16) {
        __m128i s[2], res;

        if (w == 2) {
          s[0] = _mm_cvtsi32_si128(loadu_int16(data));

          do {
            s[1] = _mm_cvtsi32_si128(loadu_int16(data + src_stride));
            res = _mm_avg_epu8(s[0], s[1]);
            xx_storel_16(dst_ptr, res);
            s[0] = _mm_cvtsi32_si128(loadu_int16(data + 2 * src_stride));
            res = _mm_avg_epu8(s[1], s[0]);
            xx_storel_16(dst_ptr + dst_stride, res);

            data += 2 * src_stride;
            dst_ptr += 2 * dst_stride;
            y -= 2;
          } while (y > 0);
        } else if (w == 4) {
          s[0] = _mm_cvtsi32_si128(loadu_int32(data));

          do {
            s[1] = _mm_cvtsi32_si128(loadu_int32(data + src_stride));
            res = _mm_avg_epu8(s[0], s[1]);
            xx_storel_32(dst_ptr, res);
            s[0] = _mm_cvtsi32_si128(loadu_int32(data + 2 * src_stride));
            res = _mm_avg_epu8(s[1], s[0]);
            xx_storel_32(dst_ptr + dst_stride, res);

            data += 2 * src_stride;
            dst_ptr += 2 * dst_stride;
            y -= 2;
          } while (y > 0);
        } else if (w == 8) {
          s[0] = _mm_loadl_epi64((__m128i *)data);

          do {
            s[1] = _mm_loadl_epi64((__m128i *)(data + src_stride));
            res = _mm_avg_epu8(s[0], s[1]);
            _mm_storel_epi64((__m128i *)dst_ptr, res);
            s[0] = _mm_loadl_epi64((__m128i *)(data + 2 * src_stride));
            res = _mm_avg_epu8(s[1], s[0]);
            _mm_storel_epi64((__m128i *)(dst_ptr + dst_stride), res);

            data += 2 * src_stride;
            dst_ptr += 2 * dst_stride;
            y -= 2;
          } while (y > 0);
        } else {
          assert(w == 16);

          s[0] = _mm_loadu_si128((__m128i *)data);

          do {
            s[1] = _mm_loadu_si128((__m128i *)(data + src_stride));
            res = _mm_avg_epu8(s[0], s[1]);
            _mm_storeu_si128((__m128i *)dst_ptr, res);
            s[0] = _mm_loadu_si128((__m128i *)(data + 2 * src_stride));
            res = _mm_avg_epu8(s[1], s[0]);
            _mm_storeu_si128((__m128i *)(dst_ptr + dst_stride), res);

            data += 2 * src_stride;
            dst_ptr += 2 * dst_stride;
            y -= 2;
          } while (y > 0);
        }
      } else {
        assert(!(w % 32));

        __m256i s[2], res;
        do {
          data = src_ptr + x;
          dst_ptr = dst + x;
          y = h;

          s[0] = _mm256_loadu_si256((__m256i *)data);

          do {
            s[1] = _mm256_loadu_si256((__m256i *)(data + src_stride));
            res = _mm256_avg_epu8(s[0], s[1]);
            _mm256_storeu_si256((__m256i *)dst_ptr, res);
            s[0] = _mm256_loadu_si256((__m256i *)(data + 2 * src_stride));
            res = _mm256_avg_epu8(s[1], s[0]);
            _mm256_storeu_si256((__m256i *)(dst_ptr + dst_stride), res);

            data += 2 * src_stride;
            dst_ptr += 2 * dst_stride;
            y -= 2;
          } while (y > 0);

          x += 32;
        } while (x < w);
      }
    }
  } else if (vert_tap == 4) {
    if (w <= 4) {
      prepare_coeffs_4t_ssse3(filter_params_y, subpel_y_qn, coeffs_128);
      __m128i d[4], s[2];

      if (w == 2) {
        d[0] = _mm_cvtsi32_si128(loadu_int16(data + 0 * src_stride));
        d[1] = _mm_cvtsi32_si128(loadu_int16(data + 1 * src_stride));
        d[2] = _mm_cvtsi32_si128(loadu_int16(data + 2 * src_stride));

        const __m128i src_01a = _mm_unpacklo_epi16(d[0], d[1]);
        const __m128i src_12a = _mm_unpacklo_epi16(d[1], d[2]);

        s[0] = _mm_unpacklo_epi8(src_01a, src_12a);
        do {
          __m128i res;
          convolve_y_4tap_2x2_ssse3(data, src_stride, coeffs_128, d, s, &res);
          res = round_sr_y_ssse3(res);
          pack_store_u8_2x2_sse2(res, dst_ptr, dst_stride);

          dst_ptr += 2 * dst_stride;
          data += 2 * src_stride;
          y -= 2;

          s[0] = s[1];
        } while (y > 0);

      } else {
        assert(w == 4);

        d[0] = _mm_cvtsi32_si128(loadu_int32(data + 0 * src_stride));
        d[1] = _mm_cvtsi32_si128(loadu_int32(data + 1 * src_stride));
        d[2] = _mm_cvtsi32_si128(loadu_int32(data + 2 * src_stride));

        const __m128i src_01a = _mm_unpacklo_epi32(d[0], d[1]);
        const __m128i src_12a = _mm_unpacklo_epi32(d[1], d[2]);

        s[0] = _mm_unpacklo_epi8(src_01a, src_12a);
        do {
          __m128i res;
          convolve_y_4tap_4x2_ssse3(data, src_stride, coeffs_128, d, s, &res);
          res = round_sr_y_ssse3(res);
          pack_store_u8_4x2_sse2(res, dst_ptr, dst_stride);

          dst_ptr += 2 * dst_stride;
          data += 2 * src_stride;
          y -= 2;

          s[0] = s[1];
        } while (y > 0);
      }
    } else {
      prepare_coeffs_4t_lowbd(filter_params_y, subpel_y_qn, coeffs);

      if (w == 8) {
        __m128i d[4];
        __m256i s[2];

        d[0] = _mm_loadl_epi64((__m128i *)(data + 0 * src_stride));
        d[1] = _mm_loadl_epi64((__m128i *)(data + 1 * src_stride));
        d[2] = _mm_loadl_epi64((__m128i *)(data + 2 * src_stride));

        const __m256i src_01a = _mm256_setr_m128i(d[0], d[1]);
        const __m256i src_12a = _mm256_setr_m128i(d[1], d[2]);

        s[0] = _mm256_unpacklo_epi8(src_01a, src_12a);
        do {
          __m256i res;
          convolve_y_4tap_8x2_avx2(data, src_stride, coeffs, d, s, &res);
          round_pack_store_y_8x2_avx2(res, dst_ptr, dst_stride);

          dst_ptr += 2 * dst_stride;
          data += 2 * src_stride;
          y -= 2;

          s[0] = s[1];
        } while (y > 0);
      } else if (w == 16) {
        __m128i d[4];
        __m256i s[4];

        d[0] = _mm_loadu_si128((__m128i *)(data + 0 * src_stride));
        d[1] = _mm_loadu_si128((__m128i *)(data + 1 * src_stride));
        d[2] = _mm_loadu_si128((__m128i *)(data + 2 * src_stride));

        const __m256i src_01a = _mm256_setr_m128i(d[0], d[1]);
        const __m256i src_12a = _mm256_setr_m128i(d[1], d[2]);

        s[0] = _mm256_unpacklo_epi8(src_01a, src_12a);
        s[2] = _mm256_unpackhi_epi8(src_01a, src_12a);

        do {
          __m256i res[2];
          convolve_y_4tap_16x2_avx2(data, src_stride, coeffs, d, s, res);
          round_pack_store_y_16x2_avx2(res, dst_ptr, dst_stride);

          dst_ptr += 2 * dst_stride;
          data += 2 * src_stride;
          y -= 2;

          s[0] = s[1];
          s[2] = s[3];
        } while (y > 0);
      } else {
        assert(!(w % 32));

        __m256i d[4], s1[4], s2[4];
        do {
          data = src_ptr + x;
          dst_ptr = dst + x;
          y = h;

          d[0] = _mm256_loadu_si256((__m256i *)(data + 0 * src_stride));
          d[1] = _mm256_loadu_si256((__m256i *)(data + 1 * src_stride));
          d[2] = _mm256_loadu_si256((__m256i *)(data + 2 * src_stride));

          s1[0] = _mm256_unpacklo_epi8(d[0], d[1]);
          s1[2] = _mm256_unpackhi_epi8(d[0], d[1]);

          s2[0] = _mm256_unpacklo_epi8(d[1], d[2]);
          s2[2] = _mm256_unpackhi_epi8(d[1], d[2]);

          do {
            __m256i res[4];
            convolve_y_4tap_32x2_avx2(data, src_stride, coeffs, d, s1, s2, res);
            round_pack_store_y_32x2_avx2(res, dst_ptr, dst_stride);

            dst_ptr += 2 * dst_stride;
            data += 2 * src_stride;
            y -= 2;

            s1[0] = s1[1];
            s1[2] = s1[3];

            s2[0] = s2[1];
            s2[2] = s2[3];
          } while (y > 0);

          x += 32;
        } while (x < w);
      }
    }
  } else if (vert_tap == 6) {
    if (w <= 4) {
      prepare_coeffs_6t_ssse3(filter_params_y, subpel_y_qn, coeffs_128);

      __m128i d[6], s[3];
      if (w == 2) {
        d[0] = _mm_cvtsi32_si128(loadu_int16(data + 0 * src_stride));
        d[1] = _mm_cvtsi32_si128(loadu_int16(data + 1 * src_stride));
        d[2] = _mm_cvtsi32_si128(loadu_int16(data + 2 * src_stride));
        d[3] = _mm_cvtsi32_si128(loadu_int16(data + 3 * src_stride));
        d[4] = _mm_cvtsi32_si128(loadu_int16(data + 4 * src_stride));

        const __m128i src_01a = _mm_unpacklo_epi16(d[0], d[1]);
        const __m128i src_12a = _mm_unpacklo_epi16(d[1], d[2]);
        const __m128i src_23a = _mm_unpacklo_epi16(d[2], d[3]);
        const __m128i src_34a = _mm_unpacklo_epi16(d[3], d[4]);

        s[0] = _mm_unpacklo_epi8(src_01a, src_12a);
        s[1] = _mm_unpacklo_epi8(src_23a, src_34a);

        do {
          __m128i res;
          convolve_y_6tap_2x2_ssse3(data, src_stride, coeffs_128, d, s, &res);
          res = round_sr_y_ssse3(res);
          pack_store_u8_2x2_sse2(res, dst_ptr, dst_stride);

          dst_ptr += 2 * dst_stride;
          data += 2 * src_stride;
          y -= 2;

          s[0] = s[1];
          s[1] = s[2];
        } while (y > 0);

      } else {
        assert(w == 4);
        d[0] = _mm_cvtsi32_si128(loadu_int32(data + 0 * src_stride));
        d[1] = _mm_cvtsi32_si128(loadu_int32(data + 1 * src_stride));
        d[2] = _mm_cvtsi32_si128(loadu_int32(data + 2 * src_stride));
        d[3] = _mm_cvtsi32_si128(loadu_int32(data + 3 * src_stride));
        d[4] = _mm_cvtsi32_si128(loadu_int32(data + 4 * src_stride));

        const __m128i src_01a = _mm_unpacklo_epi32(d[0], d[1]);
        const __m128i src_12a = _mm_unpacklo_epi32(d[1], d[2]);
        const __m128i src_23a = _mm_unpacklo_epi32(d[2], d[3]);
        const __m128i src_34a = _mm_unpacklo_epi32(d[3], d[4]);

        s[0] = _mm_unpacklo_epi8(src_01a, src_12a);
        s[1] = _mm_unpacklo_epi8(src_23a, src_34a);

        do {
          __m128i res;
          convolve_y_6tap_4x2_ssse3(data, src_stride, coeffs_128, d, s, &res);
          res = round_sr_y_ssse3(res);
          pack_store_u8_4x2_sse2(res, dst_ptr, dst_stride);

          dst_ptr += 2 * dst_stride;
          data += 2 * src_stride;
          y -= 2;

          s[0] = s[1];
          s[1] = s[2];
        } while (y > 0);
      }
    } else {
      prepare_coeffs_6t_lowbd(filter_params_y, subpel_y_qn, coeffs);

      if (w == 8) {
        __m128i d[6];
        __m256i s[3];

        d[0] = _mm_loadl_epi64((__m128i *)(data + 0 * src_stride));
        d[1] = _mm_loadl_epi64((__m128i *)(data + 1 * src_stride));
        d[2] = _mm_loadl_epi64((__m128i *)(data + 2 * src_stride));
        d[3] = _mm_loadl_epi64((__m128i *)(data + 3 * src_stride));
        d[4] = _mm_loadl_epi64((__m128i *)(data + 4 * src_stride));

        const __m256i src_01a = _mm256_setr_m128i(d[0], d[1]);
        const __m256i src_12a = _mm256_setr_m128i(d[1], d[2]);
        const __m256i src_23a = _mm256_setr_m128i(d[2], d[3]);
        const __m256i src_34a = _mm256_setr_m128i(d[3], d[4]);

        s[0] = _mm256_unpacklo_epi8(src_01a, src_12a);
        s[1] = _mm256_unpacklo_epi8(src_23a, src_34a);

        do {
          __m256i res;
          convolve_y_6tap_8x2_avx2(data, src_stride, coeffs, d, s, &res);
          round_pack_store_y_8x2_avx2(res, dst_ptr, dst_stride);

          dst_ptr += 2 * dst_stride;
          data += 2 * src_stride;
          y -= 2;

          s[0] = s[1];
          s[1] = s[2];
        } while (y > 0);

      } else {
        assert(!(w % 16));

        __m128i d[6];
        __m256i s[6];
        do {
          data = src_ptr + x;
          dst_ptr = dst + x;
          y = h;

          d[0] = _mm_loadu_si128((__m128i *)(data + 0 * src_stride));
          d[1] = _mm_loadu_si128((__m128i *)(data + 1 * src_stride));
          d[2] = _mm_loadu_si128((__m128i *)(data + 2 * src_stride));
          d[3] = _mm_loadu_si128((__m128i *)(data + 3 * src_stride));
          d[4] = _mm_loadu_si128((__m128i *)(data + 4 * src_stride));

          const __m256i src_01a = _mm256_setr_m128i(d[0], d[1]);
          const __m256i src_12a = _mm256_setr_m128i(d[1], d[2]);
          const __m256i src_23a = _mm256_setr_m128i(d[2], d[3]);
          const __m256i src_34a = _mm256_setr_m128i(d[3], d[4]);

          s[0] = _mm256_unpacklo_epi8(src_01a, src_12a);
          s[1] = _mm256_unpacklo_epi8(src_23a, src_34a);

          s[3] = _mm256_unpackhi_epi8(src_01a, src_12a);
          s[4] = _mm256_unpackhi_epi8(src_23a, src_34a);

          do {
            __m256i res[2];
            convolve_y_6tap_16x2_avx2(data, src_stride, coeffs, d, s, res);
            round_pack_store_y_16x2_avx2(res, dst_ptr, dst_stride);

            dst_ptr += 2 * dst_stride;
            data += 2 * src_stride;
            y -= 2;

            s[0] = s[1];
            s[1] = s[2];

            s[3] = s[4];
            s[4] = s[5];
          } while (y > 0);

          x += 16;
        } while (x < w);
      }
    }
  } else if (vert_tap == 12) {  // vert_tap == 12
    __m128i d[12];
    __m256i s[12];
    prepare_coeffs_12taps(filter_params_y, subpel_y_qn, coeffs);
    const __m256i v_zero = _mm256_setzero_si256();
    __m128i right_shift = _mm_cvtsi32_si128(FILTER_BITS);
    __m256i right_shift_const = _mm256_set1_epi32((1 << FILTER_BITS) >> 1);

    for (int j = 0; j < w; j += 8) {
      data = &src_ptr[j];
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
            xx_storel_32(&dst[i * dst_stride + j], res_0);
            xx_storel_32(&dst[i * dst_stride + j + dst_stride], res_1);
          } else {
            xx_storel_16(&dst[i * dst_stride + j], res_0);
            xx_storel_16(&dst[i * dst_stride + j + dst_stride], res_1);
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
    assert(vert_tap == 8);

    if (w <= 4) {
      prepare_coeffs_ssse3(filter_params_y, subpel_y_qn, coeffs_128);

      __m128i d[8], s[4], res;
      if (w == 2) {
        d[0] = _mm_cvtsi32_si128(loadu_int16(data + 0 * src_stride));
        d[1] = _mm_cvtsi32_si128(loadu_int16(data + 1 * src_stride));
        d[2] = _mm_cvtsi32_si128(loadu_int16(data + 2 * src_stride));
        d[3] = _mm_cvtsi32_si128(loadu_int16(data + 3 * src_stride));
        d[4] = _mm_cvtsi32_si128(loadu_int16(data + 4 * src_stride));
        d[5] = _mm_cvtsi32_si128(loadu_int16(data + 5 * src_stride));
        d[6] = _mm_cvtsi32_si128(loadu_int16(data + 6 * src_stride));

        const __m128i src_01a = _mm_unpacklo_epi16(d[0], d[1]);
        const __m128i src_12a = _mm_unpacklo_epi16(d[1], d[2]);
        const __m128i src_23a = _mm_unpacklo_epi16(d[2], d[3]);
        const __m128i src_34a = _mm_unpacklo_epi16(d[3], d[4]);
        const __m128i src_45a = _mm_unpacklo_epi16(d[4], d[5]);
        const __m128i src_56a = _mm_unpacklo_epi16(d[5], d[6]);

        s[0] = _mm_unpacklo_epi8(src_01a, src_12a);
        s[1] = _mm_unpacklo_epi8(src_23a, src_34a);
        s[2] = _mm_unpacklo_epi8(src_45a, src_56a);

        do {
          convolve_y_8tap_2x2_ssse3(data, src_stride, coeffs_128, d, s, &res);
          res = round_sr_y_ssse3(res);
          pack_store_u8_2x2_sse2(res, dst_ptr, dst_stride);

          dst_ptr += 2 * dst_stride;
          data += 2 * src_stride;
          y -= 2;

          s[0] = s[1];
          s[1] = s[2];
          s[2] = s[3];
        } while (y > 0);

      } else {
        assert(w == 4);

        d[0] = _mm_cvtsi32_si128(loadu_int32(data + 0 * src_stride));
        d[1] = _mm_cvtsi32_si128(loadu_int32(data + 1 * src_stride));
        d[2] = _mm_cvtsi32_si128(loadu_int32(data + 2 * src_stride));
        d[3] = _mm_cvtsi32_si128(loadu_int32(data + 3 * src_stride));
        d[4] = _mm_cvtsi32_si128(loadu_int32(data + 4 * src_stride));
        d[5] = _mm_cvtsi32_si128(loadu_int32(data + 5 * src_stride));
        d[6] = _mm_cvtsi32_si128(loadu_int32(data + 6 * src_stride));

        const __m128i src_01a = _mm_unpacklo_epi32(d[0], d[1]);
        const __m128i src_12a = _mm_unpacklo_epi32(d[1], d[2]);
        const __m128i src_23a = _mm_unpacklo_epi32(d[2], d[3]);
        const __m128i src_34a = _mm_unpacklo_epi32(d[3], d[4]);
        const __m128i src_45a = _mm_unpacklo_epi32(d[4], d[5]);
        const __m128i src_56a = _mm_unpacklo_epi32(d[5], d[6]);

        s[0] = _mm_unpacklo_epi8(src_01a, src_12a);
        s[1] = _mm_unpacklo_epi8(src_23a, src_34a);
        s[2] = _mm_unpacklo_epi8(src_45a, src_56a);

        do {
          convolve_y_8tap_4x2_ssse3(data, src_stride, coeffs_128, d, s, &res);
          res = round_sr_y_ssse3(res);
          pack_store_u8_4x2_sse2(res, dst_ptr, dst_stride);

          dst_ptr += 2 * dst_stride;
          data += 2 * src_stride;
          y -= 2;

          s[0] = s[1];
          s[1] = s[2];
          s[2] = s[3];
        } while (y > 0);
      }
    } else {
      prepare_coeffs_lowbd(filter_params_y, subpel_y_qn, coeffs);

      if (w == 8) {
        __m128i d[8];
        __m256i s[4];

        d[0] = _mm_loadl_epi64((__m128i *)(data + 0 * src_stride));
        d[1] = _mm_loadl_epi64((__m128i *)(data + 1 * src_stride));
        d[2] = _mm_loadl_epi64((__m128i *)(data + 2 * src_stride));
        d[3] = _mm_loadl_epi64((__m128i *)(data + 3 * src_stride));
        d[4] = _mm_loadl_epi64((__m128i *)(data + 4 * src_stride));
        d[5] = _mm_loadl_epi64((__m128i *)(data + 5 * src_stride));
        d[6] = _mm_loadl_epi64((__m128i *)(data + 6 * src_stride));

        const __m256i src_01a = _mm256_setr_m128i(d[0], d[1]);
        const __m256i src_12a = _mm256_setr_m128i(d[1], d[2]);
        const __m256i src_23a = _mm256_setr_m128i(d[2], d[3]);
        const __m256i src_34a = _mm256_setr_m128i(d[3], d[4]);
        const __m256i src_45a = _mm256_setr_m128i(d[4], d[5]);
        const __m256i src_56a = _mm256_setr_m128i(d[5], d[6]);

        s[0] = _mm256_unpacklo_epi8(src_01a, src_12a);
        s[1] = _mm256_unpacklo_epi8(src_23a, src_34a);
        s[2] = _mm256_unpacklo_epi8(src_45a, src_56a);

        do {
          __m256i res;
          convolve_y_8tap_8x2_avx2(data, src_stride, coeffs, d, s, &res);
          round_pack_store_y_8x2_avx2(res, dst_ptr, dst_stride);

          dst_ptr += 2 * dst_stride;
          data += 2 * src_stride;
          y -= 2;

          s[0] = s[1];
          s[1] = s[2];
          s[2] = s[3];
        } while (y > 0);

      } else {
        assert(!(w % 16));

        __m128i d[8];
        __m256i s[8];
        do {
          data = src_ptr + x;
          dst_ptr = dst + x;
          y = h;

          d[0] = _mm_loadu_si128((__m128i *)(data + 0 * src_stride));
          d[1] = _mm_loadu_si128((__m128i *)(data + 1 * src_stride));
          d[2] = _mm_loadu_si128((__m128i *)(data + 2 * src_stride));
          d[3] = _mm_loadu_si128((__m128i *)(data + 3 * src_stride));
          d[4] = _mm_loadu_si128((__m128i *)(data + 4 * src_stride));
          d[5] = _mm_loadu_si128((__m128i *)(data + 5 * src_stride));
          d[6] = _mm_loadu_si128((__m128i *)(data + 6 * src_stride));

          const __m256i src_01a = _mm256_setr_m128i(d[0], d[1]);
          const __m256i src_12a = _mm256_setr_m128i(d[1], d[2]);
          const __m256i src_23a = _mm256_setr_m128i(d[2], d[3]);
          const __m256i src_34a = _mm256_setr_m128i(d[3], d[4]);
          const __m256i src_45a = _mm256_setr_m128i(d[4], d[5]);
          const __m256i src_56a = _mm256_setr_m128i(d[5], d[6]);

          s[0] = _mm256_unpacklo_epi8(src_01a, src_12a);
          s[1] = _mm256_unpacklo_epi8(src_23a, src_34a);
          s[2] = _mm256_unpacklo_epi8(src_45a, src_56a);

          s[4] = _mm256_unpackhi_epi8(src_01a, src_12a);
          s[5] = _mm256_unpackhi_epi8(src_23a, src_34a);
          s[6] = _mm256_unpackhi_epi8(src_45a, src_56a);

          do {
            __m256i res[2];
            convolve_y_8tap_16x2_avx2(data, src_stride, coeffs, d, s, res);
            round_pack_store_y_16x2_avx2(res, dst_ptr, dst_stride);

            dst_ptr += 2 * dst_stride;
            data += 2 * src_stride;
            y -= 2;

            s[0] = s[1];
            s[1] = s[2];
            s[2] = s[3];

            s[4] = s[5];
            s[5] = s[6];
            s[6] = s[7];
          } while (y > 0);

          x += 16;
        } while (x < w);
      }
    }
  }
}

void av1_convolve_x_sr_avx2(const uint8_t *src, int32_t src_stride,
                            uint8_t *dst, int32_t dst_stride, int32_t w,
                            int32_t h,
                            const InterpFilterParams *filter_params_x,
                            const int32_t subpel_x_qn,
                            ConvolveParams *conv_params) {
  const int bits = FILTER_BITS - conv_params->round_0;
  int i, j, horiz_tap = get_filter_tap(filter_params_x, subpel_x_qn);

  assert(bits >= 0);
  assert((FILTER_BITS - conv_params->round_1) >= 0 ||
         ((conv_params->round_0 + conv_params->round_1) == 2 * FILTER_BITS));
  assert(conv_params->round_0 > 0);

  assert(horiz_tap == 2 || horiz_tap == 4 || horiz_tap == 6 || horiz_tap == 8 ||
         horiz_tap == 12);
  assert((!(w % 2)) || (w <= 128));
  assert((h % 2) == 0);

  __m256i coeffs[6] = { 0 }, filt[4] = { 0 };
  __m128i coeffs_128[4] = { 0 };

  i = 0;
  // horz_filt as 4 tap
  if (horiz_tap == 4) {
    // since fo_horiz = 1
    const uint8_t *src_ptr = src - 1;
    if (w == 2) {
      prepare_coeffs_4t_ssse3(filter_params_x, subpel_x_qn, coeffs_128);
      do {
        const __m128i res =
            convolve_x_4tap_2x2_ssse3(src_ptr, src_stride, coeffs_128);
        const __m128i reg = round_sr_x_ssse3(res);
        pack_store_u8_2x2_sse2(reg, dst, dst_stride);
        src_ptr += 2 * src_stride;
        dst += 2 * dst_stride;
        h -= 2;
      } while (h);
    } else if (w == 4) {
      prepare_coeffs_4t_ssse3(filter_params_x, subpel_x_qn, coeffs_128);
      do {
        const __m128i reg =
            convolve_x_4tap_4x2_ssse3(src_ptr, src_stride, coeffs_128);
        const __m128i res = round_sr_x_ssse3(reg);
        pack_store_u8_4x2_sse2(res, dst, dst_stride);
        src_ptr += 2 * src_stride;
        dst += 2 * dst_stride;
        h -= 2;
      } while (h);
    } else if (w == 8) {
      prepare_coeffs_lowbd(filter_params_x, subpel_x_qn, coeffs);
      filt[0] = _mm256_load_si256((__m256i const *)(filt_global_avx2));
      filt[1] = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32));
      do {
        const __m256i data = _mm256_setr_m128i(
            _mm_loadu_si128((__m128i *)(&src_ptr[i * src_stride])),
            _mm_loadu_si128(
                (__m128i *)(&src_ptr[i * src_stride + src_stride])));

        __m256i res_16b = convolve_lowbd_x_4tap(data, coeffs + 1, filt);

        res_16b = round_sr_x_avx2(res_16b);

        /* rounding code */
        // 8 bit conversion and saturation to uint8
        __m256i res_8b = _mm256_packus_epi16(res_16b, res_16b);

        const __m128i res_0 = _mm256_castsi256_si128(res_8b);
        const __m128i res_1 = _mm256_extracti128_si256(res_8b, 1);

        _mm_storel_epi64((__m128i *)&dst[i * dst_stride], res_0);
        _mm_storel_epi64((__m128i *)&dst[i * dst_stride + dst_stride], res_1);
        i += 2;
      } while (i < h);
    } else {
      assert(!(w % 16));
      prepare_coeffs_lowbd(filter_params_x, subpel_x_qn, coeffs);
      filt[0] = _mm256_load_si256((__m256i const *)(filt_global_avx2));
      filt[1] = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32));
      do {
        j = 0;
        do {
          // 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 8 9 10 11 12 13 14 15 16 17
          // 18 19 20 21 22 23
          const __m256i data = _mm256_inserti128_si256(
              _mm256_loadu_si256((__m256i *)&src_ptr[(i * src_stride) + j]),
              _mm_loadu_si128((__m128i *)&src_ptr[(i * src_stride) + (j + 8)]),
              1);

          __m256i res_16b = convolve_lowbd_x_4tap(data, coeffs + 1, filt);

          res_16b = round_sr_x_avx2(res_16b);

          /* rounding code */
          // 8 bit conversion and saturation to uint8
          __m256i res_8b = _mm256_packus_epi16(res_16b, res_16b);

          // Store values into the destination buffer
          // 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
          res_8b = _mm256_permute4x64_epi64(res_8b, 216);
          __m128i res = _mm256_castsi256_si128(res_8b);
          _mm_storeu_si128((__m128i *)&dst[i * dst_stride + j], res);
          j += 16;
        } while (j < w);
        i++;
      } while (i < h);
    }
  } else if (horiz_tap == 6) {
    // since (horiz_tap/2 - 1 == 2)
    const uint8_t *src_ptr = src - 2;
    prepare_coeffs_6t_lowbd(filter_params_x, subpel_x_qn, coeffs);
    filt[0] = _mm256_load_si256((__m256i const *)(filt_global_avx2));
    filt[1] = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32));
    filt[2] = _mm256_load_si256((__m256i const *)(filt_global_avx2 + 32 * 2));
    if (w == 8) {
      do {
        const __m256i data = _mm256_setr_m128i(
            _mm_loadu_si128((__m128i *)(&src_ptr[i * src_stride])),
            _mm_loadu_si128(
                (__m128i *)(&src_ptr[i * src_stride + src_stride])));

        __m256i res_16b = convolve_lowbd_x_6tap(data, coeffs, filt);

        res_16b = round_sr_x_avx2(res_16b);

        /* rounding code */
        // 8 bit conversion and saturation to uint8
        __m256i res_8b = _mm256_packus_epi16(res_16b, res_16b);

        const __m128i res_0 = _mm256_castsi256_si128(res_8b);
        const __m128i res_1 = _mm256_extracti128_si256(res_8b, 1);
        _mm_storel_epi64((__m128i *)&dst[i * dst_stride], res_0);
        _mm_storel_epi64((__m128i *)&dst[i * dst_stride + dst_stride], res_1);
        i += 2;
      } while (i < h);
    } else if (w == 16) {
      do {
        __m256i data[2] = { 0 };

        load_convolve_6tap_16x2_avx2(src_ptr, src_stride, coeffs, filt, data);
        round_pack_store_16x2_avx2(data, dst, dst_stride);
        src_ptr += 2 * src_stride;
        dst += 2 * dst_stride;
        h -= 2;
      } while (h);
    } else if (w == 32) {
      do {
        convolve_sr_store_6tap_32_avx2(src_ptr, coeffs, filt, dst);
        src_ptr += src_stride;
        dst += dst_stride;
      } while ((--h) > 0);
    } else if (w == 64) {
      do {
        convolve_sr_store_6tap_32_avx2(src_ptr, coeffs, filt, dst);
        convolve_sr_store_6tap_32_avx2(src_ptr + 32, coeffs, filt, dst + 32);
        src_ptr += src_stride;
        dst += dst_stride;
      } while ((--h) > 0);
    } else {
      assert(w == 128);

      do {
        convolve_sr_store_6tap_32_avx2(src_ptr, coeffs, filt, dst);
        convolve_sr_store_6tap_32_avx2(src_ptr + SECOND_32_BLK, coeffs, filt,
                                       dst + SECOND_32_BLK);
        convolve_sr_store_6tap_32_avx2(src_ptr + THIRD_32_BLK, coeffs, filt,
                                       dst + THIRD_32_BLK);
        convolve_sr_store_6tap_32_avx2(src_ptr + FOURTH_32_BLK, coeffs, filt,
                                       dst + FOURTH_32_BLK);
        src_ptr += src_stride;
        dst += dst_stride;
      } while ((--h) > 0);
    }
  } else if (horiz_tap == 8) {
    // since (horiz_tap / 2 - 1) == 3
    const uint8_t *src_ptr = src - 3;
    prepare_coeffs_lowbd(filter_params_x, subpel_x_qn, coeffs);
    filt[0] = _mm256_load_si256((__m256i const *)(filt_global_avx2));
    filt[1] =
        _mm256_load_si256((__m256i const *)(filt_global_avx2 + SECOND_32_BLK));
    filt[2] =
        _mm256_load_si256((__m256i const *)(filt_global_avx2 + THIRD_32_BLK));
    filt[3] =
        _mm256_load_si256((__m256i const *)(filt_global_avx2 + FOURTH_32_BLK));

    if (w == 8) {
      do {
        const __m256i data = _mm256_setr_m128i(
            _mm_loadu_si128((__m128i *)(&src_ptr[i * src_stride])),
            _mm_loadu_si128(
                (__m128i *)(&src_ptr[i * src_stride + src_stride])));

        __m256i res_16b = convolve_lowbd_x(data, coeffs, filt);

        res_16b = round_sr_x_avx2(res_16b);

        /* rounding code */
        // 8 bit conversion and saturation to uint8
        __m256i res_8b = _mm256_packus_epi16(res_16b, res_16b);

        const __m128i res_0 = _mm256_castsi256_si128(res_8b);
        const __m128i res_1 = _mm256_extracti128_si256(res_8b, 1);
        _mm_storel_epi64((__m128i *)&dst[i * dst_stride], res_0);
        _mm_storel_epi64((__m128i *)&dst[i * dst_stride + dst_stride], res_1);
        i += 2;
      } while (i < h);
    } else if (w == 16) {
      do {
        __m256i data[2] = { 0 };

        load_convolve_8tap_16x2_avx2(src_ptr, src_stride, coeffs, filt, data);
        round_pack_store_16x2_avx2(data, dst, dst_stride);
        src_ptr += 2 * src_stride;
        dst += 2 * dst_stride;
        h -= 2;
      } while (h);
    } else if (w == 32) {
      do {
        load_convolve_round_8tap_32_avx2(src_ptr, coeffs, filt, dst);
        src_ptr += src_stride;
        dst += dst_stride;
      } while ((--h) > 0);
    } else if (w == 64) {
      do {
        load_convolve_round_8tap_32_avx2(src_ptr, coeffs, filt, dst);
        load_convolve_round_8tap_32_avx2(src_ptr + 32, coeffs, filt, dst + 32);
        src_ptr += src_stride;
        dst += dst_stride;
      } while ((--h) > 0);
    } else {
      assert(w == 128);
      do {
        load_convolve_round_8tap_32_avx2(src_ptr, coeffs, filt, dst);
        load_convolve_round_8tap_32_avx2(src_ptr + SECOND_32_BLK, coeffs, filt,
                                         dst + SECOND_32_BLK);
        load_convolve_round_8tap_32_avx2(src_ptr + THIRD_32_BLK, coeffs, filt,
                                         dst + THIRD_32_BLK);
        load_convolve_round_8tap_32_avx2(src_ptr + FOURTH_32_BLK, coeffs, filt,
                                         dst + FOURTH_32_BLK);
        src_ptr += src_stride;
        dst += dst_stride;
      } while ((--h) > 0);
    }
  } else if (horiz_tap == 12) {  // horiz_tap == 12
    const int fo_horiz = filter_params_x->taps / 2 - 1;
    prepare_coeffs_12taps(filter_params_x, subpel_x_qn, coeffs);
    const __m128i round_shift = _mm_cvtsi32_si128(bits);
    const uint8_t *const src_ptr = src - fo_horiz;
    const __m256i v_zero = _mm256_setzero_si256();
    __m256i round_0_const =
        _mm256_set1_epi32((1 << (conv_params->round_0)) >> 1);
    __m256i round_const = _mm256_set1_epi32((1 << bits) >> 1);
    __m128i round_0_shift = _mm_cvtsi32_si128(conv_params->round_0);
    __m256i s[6] = { 0 };

    if (w <= 4) {
      do {
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
          xx_storel_32(&dst[i * dst_stride], res_0);
          // 10 11 12 13
          xx_storel_32(&dst[i * dst_stride + dst_stride], res_1);
        } else {
          // 00 01
          xx_storel_16(&dst[i * dst_stride], res_0);
          // 10 11
          xx_storel_16(&dst[i * dst_stride + dst_stride], res_1);
        }
        i += 2;
      } while (i < h);
    } else {
      assert(!(w % 8));
      do {
        j = 0;
        do {
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
          xx_storel_32(&dst[i * dst_stride + j], res_0);
          xx_storel_32(&dst[i * dst_stride + j + 4], res_1);

          j += 8;
        } while (j < w);
        i++;
      } while (i < h);
    }
  } else {
    assert(horiz_tap == 2);
    // since (filter_params_x->taps / 2 - 1) == 0
    const uint8_t *src_ptr = src;
    if (subpel_x_qn != 8) {
      if (w <= 8) {
        prepare_coeffs_2t_ssse3(filter_params_x, subpel_x_qn, coeffs_128);

        if (w == 2) {
          do {
            const __m128i data =
                convolve_x_2tap_2x2_ssse3(src_ptr, src_stride, coeffs_128);
            const __m128i reg = round_sr_x_ssse3(data);
            pack_store_u8_2x2_sse2(reg, dst, dst_stride);
            src_ptr += 2 * src_stride;
            dst += 2 * dst_stride;
            h -= 2;
          } while (h);
        } else if (w == 4) {
          do {
            const __m128i data =
                convolve_x_2tap_4x2_ssse3(src_ptr, src_stride, coeffs_128);
            const __m128i reg = round_sr_x_ssse3(data);
            pack_store_u8_4x2_sse2(reg, dst, dst_stride);
            src_ptr += 2 * src_stride;
            dst += 2 * dst_stride;
            h -= 2;
          } while (h);
        } else {
          assert(w == 8);

          do {
            __m128i data[2] = { 0 };

            convolve_x_2tap_8x2_ssse3(src_ptr, src_stride, coeffs_128, data);
            data[0] = round_sr_x_ssse3(data[0]);
            data[1] = round_sr_x_ssse3(data[1]);
            const __m128i reg = _mm_packus_epi16(data[0], data[1]);
            _mm_storel_epi64((__m128i *)dst, reg);
            _mm_storeh_epi64((__m128i *)(dst + dst_stride), reg);

            src_ptr += 2 * src_stride;
            dst += 2 * dst_stride;
            h -= 2;
          } while (h);
        }
      } else {
        prepare_coeffs_2t_lowbd(filter_params_x, subpel_x_qn, coeffs);

        if (w == 16) {
          do {
            __m256i data[2] = { 0 };

            convolve_x_2tap_16x2_avx2(src_ptr, src_stride, coeffs, data);
            round_pack_store_16x2_avx2(data, dst, dst_stride);
            src_ptr += 2 * src_stride;
            dst += 2 * dst_stride;
            h -= 2;
          } while (h);
        } else if (w == 32) {
          do {
            convolve_round_2tap_32_avx2(src_ptr, coeffs, dst);
            src_ptr += src_stride;
            dst += dst_stride;
          } while ((--h) > 0);
        } else if (w == 64) {
          do {
            convolve_round_2tap_32_avx2(src_ptr, coeffs, dst);
            convolve_round_2tap_32_avx2(src_ptr + SECOND_32_BLK, coeffs,
                                        dst + SECOND_32_BLK);
            src_ptr += src_stride;
            dst += dst_stride;
          } while ((--h) > 0);
        } else {
          assert(w == 128);

          do {
            convolve_round_2tap_32_avx2(src_ptr, coeffs, dst);
            convolve_round_2tap_32_avx2(src_ptr + (SECOND_32_BLK), coeffs,
                                        dst + (SECOND_32_BLK));
            convolve_round_2tap_32_avx2(src_ptr + (THIRD_32_BLK), coeffs,
                                        dst + (THIRD_32_BLK));
            convolve_round_2tap_32_avx2(src_ptr + (FOURTH_32_BLK), coeffs,
                                        dst + (FOURTH_32_BLK));
            src_ptr += src_stride;
            dst += dst_stride;
          } while ((--h) > 0);
        }
      }
    } else {
      if (w == 2) {
        do {
          __m128i data = load_x_u8_4x2_sse4(src_ptr, src_stride);
          const __m128i reg1 = _mm_srli_si128(data, 1);
          const __m128i reg2 = _mm_avg_epu8(data, reg1);
          xx_storel_16(dst, reg2);
          {
            uint16_t val = (uint16_t)_mm_extract_epi16(reg2, 2);
            memcpy(dst + dst_stride, &val, sizeof(val));
          }
          src_ptr += 2 * src_stride;
          dst += 2 * dst_stride;
          h -= 2;
        } while (h);
      } else if (w == 4) {
        do {
          __m128i data = load_8bit_8x2_to_1_reg_sse2(
              src_ptr, (int)(sizeof(*src_ptr) * src_stride));
          const __m128i reg1 = _mm_srli_si128(data, 1);
          const __m128i reg2 = _mm_avg_epu8(data, reg1);
          xx_storel_32(dst, reg2);
          {
            int32_t val = _mm_extract_epi32(reg2, 2);
            memcpy(dst + dst_stride, &val, sizeof(val));
          }

          src_ptr += 2 * src_stride;
          dst += 2 * dst_stride;
          h -= 2;
        } while (h);
      } else if (w == 8) {
        do {
          const __m128i data00 = _mm_loadu_si128((__m128i *)src_ptr);
          const __m128i data10 =
              _mm_loadu_si128((__m128i *)(src_ptr + src_stride));
          const __m128i data01 = _mm_srli_si128(data00, 1);
          const __m128i data11 = _mm_srli_si128(data10, 1);
          const __m128i reg0 = _mm_avg_epu8(data00, data01);
          const __m128i reg1 = _mm_avg_epu8(data10, data11);
          _mm_storel_epi64((__m128i *)dst, reg0);
          _mm_storel_epi64((__m128i *)(dst + dst_stride), reg1);

          src_ptr += 2 * src_stride;
          dst += 2 * dst_stride;
          h -= 2;
        } while (h);
      } else if (w == 16) {
        do {
          const __m128i data00 = _mm_loadu_si128((__m128i *)src_ptr);
          const __m128i data01 = _mm_loadu_si128((__m128i *)(src_ptr + 1));
          const __m128i data10 =
              _mm_loadu_si128((__m128i *)(src_ptr + src_stride));
          const __m128i data11 =
              _mm_loadu_si128((__m128i *)(src_ptr + src_stride + 1));
          const __m128i reg0 = _mm_avg_epu8(data00, data01);
          const __m128i reg1 = _mm_avg_epu8(data10, data11);
          _mm_storeu_si128((__m128i *)dst, reg0);
          _mm_storeu_si128((__m128i *)(dst + dst_stride), reg1);

          src_ptr += 2 * src_stride;
          dst += 2 * dst_stride;
          h -= 2;
        } while (h);
      } else if (w == 32) {
        do {
          load_avg_store_2tap_32_avx2(src_ptr, dst);
          src_ptr += src_stride;
          dst += dst_stride;
        } while ((--h) > 0);
      } else if (w == 64) {
        do {
          load_avg_store_2tap_32_avx2(src_ptr, dst);
          load_avg_store_2tap_32_avx2(src_ptr + (SECOND_32_BLK),
                                      dst + (SECOND_32_BLK));
          src_ptr += src_stride;
          dst += dst_stride;
        } while ((--h) > 0);
      } else {
        assert(w == 128);

        do {
          load_avg_store_2tap_32_avx2(src_ptr, dst);
          load_avg_store_2tap_32_avx2(src_ptr + (SECOND_32_BLK),
                                      dst + (SECOND_32_BLK));
          load_avg_store_2tap_32_avx2(src_ptr + (THIRD_32_BLK),
                                      dst + (THIRD_32_BLK));
          load_avg_store_2tap_32_avx2(src_ptr + (FOURTH_32_BLK),
                                      dst + (FOURTH_32_BLK));
          src_ptr += src_stride;
          dst += dst_stride;
        } while ((--h) > 0);
      }
    }
  }
}

/*
 *  Copyright (c) 2018 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <emmintrin.h>
#include <stdlib.h>

#include "./vpx_dsp_rtcd.h"
#include "vpx/vpx_integer.h"
#include "vpx_dsp/x86/mem_sse2.h"

extern const int16_t vpx_rv[];

static INLINE __m128i filter_16_sse2(__m128i v, __m128i p_above2,
                                     __m128i p_above1, __m128i p_below1,
                                     __m128i p_below2, __m128i flimit,
                                     __m128i zero) {
  const __m128i k1 = _mm_avg_epu8(p_above2, p_above1);
  const __m128i k2 = _mm_avg_epu8(p_below2, p_below1);
  const __m128i k3 = _mm_avg_epu8(k1, k2);
  const __m128i filtered = _mm_avg_epu8(k3, v);

  const __m128i diff_b1 =
      _mm_or_si128(_mm_subs_epu8(v, p_below1), _mm_subs_epu8(p_below1, v));
  const __m128i diff_b2 =
      _mm_or_si128(_mm_subs_epu8(v, p_below2), _mm_subs_epu8(p_below2, v));
  const __m128i diff_a1 =
      _mm_or_si128(_mm_subs_epu8(v, p_above1), _mm_subs_epu8(p_above1, v));
  const __m128i diff_a2 =
      _mm_or_si128(_mm_subs_epu8(v, p_above2), _mm_subs_epu8(p_above2, v));

  const __m128i mask_b1 = _mm_cmpeq_epi8(_mm_subs_epu8(flimit, diff_b1), zero);
  const __m128i mask_b2 = _mm_cmpeq_epi8(_mm_subs_epu8(flimit, diff_b2), zero);
  const __m128i mask_a1 = _mm_cmpeq_epi8(_mm_subs_epu8(flimit, diff_a1), zero);
  const __m128i mask_a2 = _mm_cmpeq_epi8(_mm_subs_epu8(flimit, diff_a2), zero);

  const __m128i fail_mask = _mm_or_si128(_mm_or_si128(mask_b1, mask_b2),
                                         _mm_or_si128(mask_a1, mask_a2));

  return _mm_or_si128(_mm_and_si128(fail_mask, v),
                      _mm_andnot_si128(fail_mask, filtered));
}

static INLINE unsigned char filter_scalar(
    unsigned char v, unsigned char p_above2, unsigned char p_above1,
    unsigned char p_below1, unsigned char p_below2, unsigned char flimit) {
  if ((abs(v - p_above2) < flimit) && (abs(v - p_above1) < flimit) &&
      (abs(v - p_below1) < flimit) && (abs(v - p_below2) < flimit)) {
    const unsigned char k1 = (p_above2 + p_above1 + 1) >> 1;
    const unsigned char k2 = (p_below2 + p_below1 + 1) >> 1;
    const unsigned char k3 = (k1 + k2 + 1) >> 1;
    return (k3 + v + 1) >> 1;
  }
  return v;
}

void vpx_post_proc_down_and_across_mb_row_sse2(unsigned char *src,
                                               unsigned char *dst,
                                               int src_pitch, int dst_pitch,
                                               int cols, unsigned char *flimits,
                                               int size) {
  int row, col;
  const __m128i zero = _mm_setzero_si128();

  for (row = 0; row < size; row++) {
    const int aligned_cols = cols & ~15;
    for (col = 0; col < aligned_cols; col += 16) {
      const __m128i v = _mm_loadu_si128((const __m128i *)(src + col));
      const __m128i p_below1 =
          _mm_loadu_si128((const __m128i *)(src + src_pitch + col));
      const __m128i p_below2 =
          _mm_loadu_si128((const __m128i *)(src + 2 * src_pitch + col));
      const __m128i p_above1 =
          _mm_loadu_si128((const __m128i *)(src - src_pitch + col));
      const __m128i p_above2 =
          _mm_loadu_si128((const __m128i *)(src - 2 * src_pitch + col));
      const __m128i f = _mm_loadu_si128((const __m128i *)(flimits + col));

      const __m128i out =
          filter_16_sse2(v, p_above2, p_above1, p_below1, p_below2, f, zero);
      _mm_storeu_si128((__m128i *)(dst + col), out);
    }

    for (; col < cols; col++) {
      const unsigned char p_above2 = src[col - 2 * src_pitch];
      const unsigned char p_above1 = src[col - src_pitch];
      const unsigned char p_below1 = src[col + src_pitch];
      const unsigned char p_below2 = src[col + 2 * src_pitch];
      const unsigned char v = src[col];

      dst[col] = filter_scalar(v, p_above2, p_above1, p_below1, p_below2,
                               flimits[col]);
    }

    dst[-2] = dst[-1] = dst[0];
    dst[cols] = dst[cols + 1] = dst[cols - 1];

    __m128i prev_out = _mm_setzero_si128();
    for (col = 0; col < aligned_cols; col += 16) {
      const __m128i v = _mm_loadu_si128((const __m128i *)(dst + col));
      const __m128i p_left2 = _mm_loadu_si128((const __m128i *)(dst + col - 2));
      const __m128i p_left1 = _mm_loadu_si128((const __m128i *)(dst + col - 1));
      const __m128i p_right1 =
          _mm_loadu_si128((const __m128i *)(dst + col + 1));
      const __m128i p_right2 =
          _mm_loadu_si128((const __m128i *)(dst + col + 2));
      const __m128i f = _mm_loadu_si128((const __m128i *)(flimits + col));

      if (col > 0) {
        _mm_storeu_si128((__m128i *)(dst + col - 16), prev_out);
      }
      prev_out =
          filter_16_sse2(v, p_left2, p_left1, p_right1, p_right2, f, zero);
    }

    const int last_sse_col = col;
    const unsigned char prev_p2 = dst[col - 2];
    const unsigned char prev_p1 = dst[col - 1];
    unsigned char d[4] = { 0 };

    if (col > 0) {
      _mm_storeu_si128((__m128i *)(dst + col - 16), prev_out);
    }

    for (; col < cols; col++) {
      const unsigned char p_left2 =
          (col == last_sse_col)
              ? prev_p2
              : ((col == last_sse_col + 1) ? prev_p1 : dst[col - 2]);
      const unsigned char p_left1 =
          (col == last_sse_col) ? prev_p1 : dst[col - 1];
      const unsigned char p_right1 = dst[col + 1];
      const unsigned char p_right2 = dst[col + 2];
      const unsigned char v = dst[col];

      d[col & 3] =
          filter_scalar(v, p_left2, p_left1, p_right1, p_right2, flimits[col]);

      if (col >= last_sse_col + 2) dst[col - 2] = d[(col - 2) & 3];
    }

    if (cols - last_sse_col >= 2) {
      dst[cols - 2] = d[(cols - 2) & 3];
      dst[cols - 1] = d[(cols - 1) & 3];
    } else if (cols - last_sse_col == 1) {
      dst[cols - 1] = d[(cols - 1) & 3];
    }

    src += src_pitch;
    dst += dst_pitch;
  }
}

static INLINE __m128i filter_4_across(const uint8_t *s_c, __m128i left4_u8,
                                      __m128i right4_u8, int *sum, int *sumsq,
                                      __m128i f_vec, __m128i eight,
                                      __m128i zero) {
  const __m128i left16 = _mm_unpacklo_epi8(left4_u8, zero);
  const __m128i right16 = _mm_unpacklo_epi8(right4_u8, zero);

  const __m128i x16 = _mm_sub_epi16(right16, left16);
  const __m128i y16 = _mm_add_epi16(right16, left16);

  const __m128i d_sum = _mm_srai_epi32(_mm_unpacklo_epi16(zero, x16), 16);

  const __m128i xy_lo = _mm_mullo_epi16(x16, y16);
  const __m128i xy_hi = _mm_mulhi_epi16(x16, y16);
  const __m128i d_sumsq = _mm_unpacklo_epi16(xy_lo, xy_hi);

  const __m128i t1 = _mm_slli_si128(d_sum, 4);
  const __m128i s1 = _mm_add_epi32(d_sum, t1);
  const __m128i t2 = _mm_slli_si128(s1, 8);
  const __m128i pref_sum = _mm_add_epi32(s1, t2);
  const __m128i sum_vec = _mm_add_epi32(pref_sum, _mm_set1_epi32(*sum));

  const __m128i u1 = _mm_slli_si128(d_sumsq, 4);
  const __m128i q1 = _mm_add_epi32(d_sumsq, u1);
  const __m128i u2 = _mm_slli_si128(q1, 8);
  const __m128i pref_sumsq = _mm_add_epi32(q1, u2);
  const __m128i sumsq_vec = _mm_add_epi32(pref_sumsq, _mm_set1_epi32(*sumsq));

  *sum = _mm_cvtsi128_si32(_mm_shuffle_epi32(sum_vec, 3));
  *sumsq = _mm_cvtsi128_si32(_mm_shuffle_epi32(sumsq_vec, 3));

  const __m128i sumsq15 =
      _mm_sub_epi32(_mm_slli_epi32(sumsq_vec, 4), sumsq_vec);
  const __m128i sum_sq = _mm_madd_epi16(sum_vec, sum_vec);
  const __m128i diff = _mm_sub_epi32(_mm_sub_epi32(sumsq15, sum_sq), f_vec);
  const __m128i mask32 = _mm_srai_epi32(diff, 31);

  const __m128i sc4 = load_unaligned_u32(s_c);
  const __m128i sc16 = _mm_unpacklo_epi8(sc4, zero);
  const __m128i sc32 = _mm_unpacklo_epi16(sc16, zero);

  const __m128i filt32 =
      _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(sum_vec, sc32), eight), 4);
  return _mm_or_si128(_mm_and_si128(mask32, filt32),
                      _mm_andnot_si128(mask32, sc32));
}

void vpx_mbpost_proc_across_ip_sse2(unsigned char *src, int pitch, int rows,
                                    int cols, int flimit) {
  int r, c, i;
  const __m128i zero = _mm_setzero_si128();
  const __m128i f_vec = _mm_set1_epi32(flimit);
  const __m128i eight = _mm_set1_epi32(8);

  if (cols < 8 || (cols & 7)) {
    vpx_mbpost_proc_across_ip_c(src, pitch, rows, cols, flimit);
    return;
  }

  for (r = 0; r < rows; r++) {
    unsigned char *s = src + r * pitch;
    int sumsq = 16;
    int sum = 0;

    for (i = -8; i < 0; i++) s[i] = s[0];
    for (i = 0; i < 17; i++) s[i + cols] = s[cols - 1];

    for (i = -8; i <= 6; i++) {
      sumsq += s[i] * s[i];
      sum += s[i];
    }

    __m128i prev_out = _mm_setzero_si128();
    for (c = 0; c < cols + 8; c += 8) {
      __m128i res8 = _mm_setzero_si128();
      if (c < cols) {
        const __m128i left_lo = load_unaligned_u32(s + c - 8);
        const __m128i right_lo = load_unaligned_u32(s + c + 7);
        const __m128i res32_lo = filter_4_across(s + c, left_lo, right_lo, &sum,
                                                 &sumsq, f_vec, eight, zero);

        const __m128i left_hi = load_unaligned_u32(s + c - 4);
        const __m128i right_hi = load_unaligned_u32(s + c + 11);
        const __m128i res32_hi = filter_4_across(
            s + c + 4, left_hi, right_hi, &sum, &sumsq, f_vec, eight, zero);

        const __m128i res16 = _mm_packs_epi32(res32_lo, res32_hi);
        res8 = _mm_packus_epi16(res16, zero);
      }

      if (c >= 8) {
        _mm_storel_epi64((__m128i *)(s + c - 8), prev_out);
      }
      prev_out = res8;
    }
  }
}

void vpx_mbpost_proc_down_sse2(unsigned char *dst, int pitch, int rows,
                               int cols, int flimit) {
  int col;
  const __m128i zero = _mm_setzero_si128();
  const __m128i f = _mm_set1_epi32(flimit);
  DECLARE_ALIGNED(16, int16_t, above_context[8 * 8]);

  // 8 columns are processed at a time.
  // If rows is less than 8 the bottom border extension fails.
  assert(cols % 8 == 0);
  assert(rows >= 8);

  for (col = 0; col < cols; col += 8) {
    int row, i;
    __m128i s = _mm_loadl_epi64((__m128i *)dst);
    __m128i sum, sumsq_0, sumsq_1;
    __m128i tmp_0, tmp_1;
    __m128i below_context = _mm_setzero_si128();

    s = _mm_unpacklo_epi8(s, zero);

    for (i = 0; i < 8; ++i) {
      _mm_store_si128((__m128i *)above_context + i, s);
    }

    // sum *= 9
    sum = _mm_slli_epi16(s, 3);
    sum = _mm_add_epi16(s, sum);

    // sum^2 * 9 == (sum * 9) * sum
    tmp_0 = _mm_mullo_epi16(sum, s);
    tmp_1 = _mm_mulhi_epi16(sum, s);

    sumsq_0 = _mm_unpacklo_epi16(tmp_0, tmp_1);
    sumsq_1 = _mm_unpackhi_epi16(tmp_0, tmp_1);

    // Prime sum/sumsq
    for (i = 1; i <= 6; ++i) {
      __m128i a = _mm_loadl_epi64((__m128i *)(dst + i * pitch));
      a = _mm_unpacklo_epi8(a, zero);
      sum = _mm_add_epi16(sum, a);
      a = _mm_mullo_epi16(a, a);
      sumsq_0 = _mm_add_epi32(sumsq_0, _mm_unpacklo_epi16(a, zero));
      sumsq_1 = _mm_add_epi32(sumsq_1, _mm_unpackhi_epi16(a, zero));
    }

    for (row = 0; row < rows + 8; row++) {
      const __m128i above =
          _mm_load_si128((__m128i *)above_context + (row & 7));
      __m128i this_row = _mm_loadl_epi64((__m128i *)(dst + row * pitch));
      __m128i above_sq, below_sq;
      __m128i mask_0, mask_1;
      __m128i multmp_0, multmp_1;
      __m128i rv;
      __m128i out;

      this_row = _mm_unpacklo_epi8(this_row, zero);

      if (row + 7 < rows) {
        // Instead of copying the end context we just stop loading when we get
        // to the last one.
        below_context = _mm_loadl_epi64((__m128i *)(dst + (row + 7) * pitch));
        below_context = _mm_unpacklo_epi8(below_context, zero);
      }

      sum = _mm_sub_epi16(sum, above);
      sum = _mm_add_epi16(sum, below_context);

      // context^2 fits in 16 bits. Don't need to mulhi and combine. Just zero
      // extend. Unfortunately we can't do below_sq - above_sq in 16 bits
      // because x86 does not have unpack with sign extension.
      above_sq = _mm_mullo_epi16(above, above);
      sumsq_0 = _mm_sub_epi32(sumsq_0, _mm_unpacklo_epi16(above_sq, zero));
      sumsq_1 = _mm_sub_epi32(sumsq_1, _mm_unpackhi_epi16(above_sq, zero));

      below_sq = _mm_mullo_epi16(below_context, below_context);
      sumsq_0 = _mm_add_epi32(sumsq_0, _mm_unpacklo_epi16(below_sq, zero));
      sumsq_1 = _mm_add_epi32(sumsq_1, _mm_unpackhi_epi16(below_sq, zero));

      // sumsq * 16 - sumsq == sumsq * 15
      mask_0 = _mm_slli_epi32(sumsq_0, 4);
      mask_0 = _mm_sub_epi32(mask_0, sumsq_0);
      mask_1 = _mm_slli_epi32(sumsq_1, 4);
      mask_1 = _mm_sub_epi32(mask_1, sumsq_1);

      multmp_0 = _mm_mullo_epi16(sum, sum);
      multmp_1 = _mm_mulhi_epi16(sum, sum);

      mask_0 = _mm_sub_epi32(mask_0, _mm_unpacklo_epi16(multmp_0, multmp_1));
      mask_1 = _mm_sub_epi32(mask_1, _mm_unpackhi_epi16(multmp_0, multmp_1));

      // mask - f gives a negative value when mask < f
      mask_0 = _mm_sub_epi32(mask_0, f);
      mask_1 = _mm_sub_epi32(mask_1, f);

      // Shift the sign bit down to create a mask
      mask_0 = _mm_srai_epi32(mask_0, 31);
      mask_1 = _mm_srai_epi32(mask_1, 31);

      mask_0 = _mm_packs_epi32(mask_0, mask_1);

      rv = _mm_loadu_si128((__m128i const *)(vpx_rv + (row & 127)));

      mask_1 = _mm_add_epi16(rv, sum);
      mask_1 = _mm_add_epi16(mask_1, this_row);
      mask_1 = _mm_srai_epi16(mask_1, 4);

      mask_1 = _mm_and_si128(mask_0, mask_1);
      mask_0 = _mm_andnot_si128(mask_0, this_row);
      out = _mm_or_si128(mask_1, mask_0);

      _mm_storel_epi64((__m128i *)(dst + row * pitch),
                       _mm_packus_epi16(out, zero));

      _mm_store_si128((__m128i *)above_context + ((row + 8) & 7), this_row);
    }

    dst += 8;
  }
}

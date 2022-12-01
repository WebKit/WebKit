/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <immintrin.h> /* AVX2 */

#include "config/aom_dsp_rtcd.h"

DECLARE_ALIGNED(32, static const uint8_t, filt_loopfilter_avx2[32]) = {
  0, 128, 1, 128, 2,  128, 3,  128, 4,  128, 5,  128, 6,  128, 7,  128,
  8, 128, 9, 128, 10, 128, 11, 128, 12, 128, 13, 128, 14, 128, 15, 128
};

void aom_lpf_horizontal_6_quad_avx2(unsigned char *s, int p,
                                    const unsigned char *_blimit0,
                                    const unsigned char *_limit0,
                                    const unsigned char *_thresh0) {
  __m256i p256_2, q256_2, p256_1, q256_1, p256_0, q256_0;
  __m128i p2, p1, p0, q0, q1, q2;
  __m128i mask, flat;

  const __m128i thresh_v =
      _mm_broadcastb_epi8(_mm_cvtsi32_si128((int)_thresh0[0]));
  const __m128i limit_v =
      _mm_broadcastb_epi8(_mm_cvtsi32_si128((int)_limit0[0]));
  const __m128i blimit_v =
      _mm_broadcastb_epi8(_mm_cvtsi32_si128((int)_blimit0[0]));
  const __m128i zero = _mm_set1_epi16(0);
  const __m128i ff = _mm_cmpeq_epi8(zero, zero);

  p256_2 =
      _mm256_castpd_si256(_mm256_broadcast_pd((__m128d const *)(s - 3 * p)));
  p256_1 =
      _mm256_castpd_si256(_mm256_broadcast_pd((__m128d const *)(s - 2 * p)));
  p256_0 =
      _mm256_castpd_si256(_mm256_broadcast_pd((__m128d const *)(s - 1 * p)));
  q256_0 =
      _mm256_castpd_si256(_mm256_broadcast_pd((__m128d const *)(s - 0 * p)));
  q256_1 =
      _mm256_castpd_si256(_mm256_broadcast_pd((__m128d const *)(s + 1 * p)));
  q256_2 =
      _mm256_castpd_si256(_mm256_broadcast_pd((__m128d const *)(s + 2 * p)));

  p2 = _mm256_castsi256_si128(p256_2);
  p1 = _mm256_castsi256_si128(p256_1);
  p0 = _mm256_castsi256_si128(p256_0);
  q0 = _mm256_castsi256_si128(q256_0);
  q1 = _mm256_castsi256_si128(q256_1);
  q2 = _mm256_castsi256_si128(q256_2);

  {
    __m128i work;
    const __m128i fe = _mm_set1_epi8((int8_t)0xfe);
    const __m128i abs_p1p0 =
        _mm_or_si128(_mm_subs_epu8(p1, p0), _mm_subs_epu8(p0, p1));
    const __m128i abs_q1q0 =
        _mm_or_si128(_mm_subs_epu8(q1, q0), _mm_subs_epu8(q0, q1));
    __m128i abs_p0q0 =
        _mm_or_si128(_mm_subs_epu8(p0, q0), _mm_subs_epu8(q0, p0));
    __m128i abs_p1q1 =
        _mm_or_si128(_mm_subs_epu8(p1, q1), _mm_subs_epu8(q1, p1));

    flat = _mm_max_epu8(abs_p1p0, abs_q1q0);

    abs_p0q0 = _mm_adds_epu8(abs_p0q0, abs_p0q0);
    abs_p1q1 = _mm_srli_epi16(_mm_and_si128(abs_p1q1, fe), 1);
    mask = _mm_subs_epu8(_mm_adds_epu8(abs_p0q0, abs_p1q1), blimit_v);
    mask = _mm_xor_si128(_mm_cmpeq_epi8(mask, zero), ff);
    // mask |= (abs(p0 - q0) * 2 + abs(p1 - q1) / 2  > blimit) * -1;
    mask = _mm_max_epu8(flat, mask);
    // mask |= (abs(p1 - p0) > limit) * -1;
    // mask |= (abs(q1 - q0) > limit) * -1;
    work = _mm_max_epu8(
        _mm_or_si128(_mm_subs_epu8(p2, p1), _mm_subs_epu8(p1, p2)),
        _mm_or_si128(_mm_subs_epu8(q2, q1), _mm_subs_epu8(q1, q2)));
    mask = _mm_max_epu8(work, mask);
    mask = _mm_subs_epu8(mask, limit_v);
    mask = _mm_cmpeq_epi8(mask, zero);
  }

  if (0xffff == _mm_movemask_epi8(_mm_cmpeq_epi8(mask, zero))) return;

  // loop filter
  {
    const __m128i t4 = _mm_set1_epi8(4);
    const __m128i t3 = _mm_set1_epi8(3);
    const __m128i t80 = _mm_set1_epi8((int8_t)0x80);
    const __m128i te0 = _mm_set1_epi8((int8_t)0xe0);
    const __m128i t1f = _mm_set1_epi8(0x1f);
    const __m128i t1 = _mm_set1_epi8(0x1);
    const __m128i t7f = _mm_set1_epi8(0x7f);
    const __m128i one = _mm_set1_epi8(1);
    __m128i hev;

    hev = _mm_subs_epu8(flat, thresh_v);
    hev = _mm_xor_si128(_mm_cmpeq_epi8(hev, zero), ff);

    __m128i ps1 = _mm_xor_si128(p1, t80);
    __m128i ps0 = _mm_xor_si128(p0, t80);
    __m128i qs0 = _mm_xor_si128(q0, t80);
    __m128i qs1 = _mm_xor_si128(q1, t80);
    __m128i filt;
    __m128i work_a;
    __m128i filter1, filter2;
    __m128i flat_p1, flat_p0, flat_q0, flat_q1;

    filt = _mm_and_si128(_mm_subs_epi8(ps1, qs1), hev);
    work_a = _mm_subs_epi8(qs0, ps0);
    filt = _mm_adds_epi8(filt, work_a);
    filt = _mm_adds_epi8(filt, work_a);
    filt = _mm_adds_epi8(filt, work_a);
    filt = _mm_and_si128(filt, mask);

    filter1 = _mm_adds_epi8(filt, t4);
    filter2 = _mm_adds_epi8(filt, t3);

    work_a = _mm_cmpgt_epi8(zero, filter1);
    filter1 = _mm_srli_epi16(filter1, 3);
    work_a = _mm_and_si128(work_a, te0);
    filter1 = _mm_and_si128(filter1, t1f);
    filter1 = _mm_or_si128(filter1, work_a);
    qs0 = _mm_xor_si128(_mm_subs_epi8(qs0, filter1), t80);

    work_a = _mm_cmpgt_epi8(zero, filter2);
    filter2 = _mm_srli_epi16(filter2, 3);
    work_a = _mm_and_si128(work_a, te0);
    filter2 = _mm_and_si128(filter2, t1f);
    filter2 = _mm_or_si128(filter2, work_a);
    ps0 = _mm_xor_si128(_mm_adds_epi8(ps0, filter2), t80);

    filt = _mm_adds_epi8(filter1, t1);
    work_a = _mm_cmpgt_epi8(zero, filt);
    filt = _mm_srli_epi16(filt, 1);
    work_a = _mm_and_si128(work_a, t80);
    filt = _mm_and_si128(filt, t7f);
    filt = _mm_or_si128(filt, work_a);
    filt = _mm_andnot_si128(hev, filt);
    ps1 = _mm_xor_si128(_mm_adds_epi8(ps1, filt), t80);
    qs1 = _mm_xor_si128(_mm_subs_epi8(qs1, filt), t80);

    __m128i work;
    work = _mm_max_epu8(
        _mm_or_si128(_mm_subs_epu8(p2, p0), _mm_subs_epu8(p0, p2)),
        _mm_or_si128(_mm_subs_epu8(q2, q0), _mm_subs_epu8(q0, q2)));
    flat = _mm_max_epu8(work, flat);
    flat = _mm_subs_epu8(flat, one);
    flat = _mm_cmpeq_epi8(flat, zero);
    flat = _mm_and_si128(flat, mask);

    if (0xffff != _mm_movemask_epi8(_mm_cmpeq_epi8(flat, zero))) {
      const __m256i four = _mm256_set1_epi16(4);
      __m256i pixetFilter, add, res;

      const __m256i filter =
          _mm256_load_si256((__m256i const *)filt_loopfilter_avx2);

      p256_2 = _mm256_shuffle_epi8(p256_2, filter);
      p256_1 = _mm256_shuffle_epi8(p256_1, filter);
      p256_0 = _mm256_shuffle_epi8(p256_0, filter);
      q256_0 = _mm256_shuffle_epi8(q256_0, filter);
      q256_1 = _mm256_shuffle_epi8(q256_1, filter);
      q256_2 = _mm256_shuffle_epi8(q256_2, filter);

      pixetFilter = _mm256_slli_epi16(
          _mm256_add_epi16(p256_2, _mm256_add_epi16(p256_1, p256_0)), 1);
      pixetFilter =
          _mm256_add_epi16(pixetFilter, _mm256_add_epi16(p256_2, q256_0));
      pixetFilter = _mm256_add_epi16(four, pixetFilter);
      res = _mm256_srli_epi16(pixetFilter, 3);
      flat_p1 = _mm256_castsi256_si128(
          _mm256_permute4x64_epi64(_mm256_packus_epi16(res, res), 168));
      p1 = _mm_andnot_si128(flat, ps1);
      flat_p1 = _mm_and_si128(flat, flat_p1);
      p1 = _mm_or_si128(flat_p1, p1);

      add = _mm256_add_epi16(_mm256_sub_epi16(q256_1, p256_2),
                             _mm256_sub_epi16(q256_0, p256_2));
      pixetFilter = _mm256_add_epi16(pixetFilter, add);
      res = _mm256_srli_epi16(pixetFilter, 3);
      flat_p0 = _mm256_castsi256_si128(
          _mm256_permute4x64_epi64(_mm256_packus_epi16(res, res), 168));
      p0 = _mm_andnot_si128(flat, ps0);
      flat_p0 = _mm_and_si128(flat, flat_p0);
      p0 = _mm_or_si128(flat_p0, p0);

      add = _mm256_add_epi16(_mm256_sub_epi16(q256_2, p256_2),
                             _mm256_sub_epi16(q256_1, p256_1));
      pixetFilter = _mm256_add_epi16(pixetFilter, add);
      res = _mm256_srli_epi16(pixetFilter, 3);
      flat_q0 = _mm256_castsi256_si128(
          _mm256_permute4x64_epi64(_mm256_packus_epi16(res, res), 168));
      q0 = _mm_andnot_si128(flat, qs0);
      flat_q0 = _mm_and_si128(flat, flat_q0);
      q0 = _mm_or_si128(flat_q0, q0);

      add = _mm256_add_epi16(_mm256_sub_epi16(q256_2, p256_1),
                             _mm256_sub_epi16(q256_2, p256_0));
      pixetFilter = _mm256_add_epi16(pixetFilter, add);
      res = _mm256_srli_epi16(pixetFilter, 3);
      flat_q1 = _mm256_castsi256_si128(
          _mm256_permute4x64_epi64(_mm256_packus_epi16(res, res), 168));
      q1 = _mm_andnot_si128(flat, qs1);
      flat_q1 = _mm_and_si128(flat, flat_q1);
      q1 = _mm_or_si128(flat_q1, q1);

      _mm_storeu_si128((__m128i *)(s - 3 * p), p2);
      _mm_storeu_si128((__m128i *)(s - 2 * p), p1);
      _mm_storeu_si128((__m128i *)(s - 1 * p), p0);
      _mm_storeu_si128((__m128i *)(s - 0 * p), q0);
      _mm_storeu_si128((__m128i *)(s + 1 * p), q1);
      _mm_storeu_si128((__m128i *)(s + 2 * p), q2);
    } else {
      _mm_storeu_si128((__m128i *)(s - 2 * p), ps1);
      _mm_storeu_si128((__m128i *)(s - 1 * p), ps0);
      _mm_storeu_si128((__m128i *)(s - 0 * p), qs0);
      _mm_storeu_si128((__m128i *)(s + 1 * p), qs1);
    }
  }
}

void aom_lpf_horizontal_8_quad_avx2(unsigned char *s, int p,
                                    const unsigned char *_blimit0,
                                    const unsigned char *_limit0,
                                    const unsigned char *_thresh0) {
  __m256i p256_3, q256_3, p256_2, q256_2, p256_1, q256_1, p256_0, q256_0;
  __m128i p3, p2, p1, p0, q0, q1, q2, q3;
  __m128i mask, flat;

  const __m128i thresh_v =
      _mm_broadcastb_epi8(_mm_cvtsi32_si128((int)_thresh0[0]));
  const __m128i limit_v =
      _mm_broadcastb_epi8(_mm_cvtsi32_si128((int)_limit0[0]));
  const __m128i blimit_v =
      _mm_broadcastb_epi8(_mm_cvtsi32_si128((int)_blimit0[0]));
  const __m128i zero = _mm_set1_epi16(0);
  const __m128i ff = _mm_cmpeq_epi8(zero, zero);

  p256_3 =
      _mm256_castpd_si256(_mm256_broadcast_pd((__m128d const *)(s - 4 * p)));
  p256_2 =
      _mm256_castpd_si256(_mm256_broadcast_pd((__m128d const *)(s - 3 * p)));
  p256_1 =
      _mm256_castpd_si256(_mm256_broadcast_pd((__m128d const *)(s - 2 * p)));
  p256_0 =
      _mm256_castpd_si256(_mm256_broadcast_pd((__m128d const *)(s - 1 * p)));
  q256_0 =
      _mm256_castpd_si256(_mm256_broadcast_pd((__m128d const *)(s - 0 * p)));
  q256_1 =
      _mm256_castpd_si256(_mm256_broadcast_pd((__m128d const *)(s + 1 * p)));
  q256_2 =
      _mm256_castpd_si256(_mm256_broadcast_pd((__m128d const *)(s + 2 * p)));
  q256_3 =
      _mm256_castpd_si256(_mm256_broadcast_pd((__m128d const *)(s + 3 * p)));

  p3 = _mm256_castsi256_si128(p256_3);
  p2 = _mm256_castsi256_si128(p256_2);
  p1 = _mm256_castsi256_si128(p256_1);
  p0 = _mm256_castsi256_si128(p256_0);
  q0 = _mm256_castsi256_si128(q256_0);
  q1 = _mm256_castsi256_si128(q256_1);
  q2 = _mm256_castsi256_si128(q256_2);
  q3 = _mm256_castsi256_si128(q256_3);

  {
    __m128i work;
    const __m128i fe = _mm_set1_epi8((int8_t)0xfe);
    const __m128i abs_p1p0 =
        _mm_or_si128(_mm_subs_epu8(p1, p0), _mm_subs_epu8(p0, p1));
    const __m128i abs_q1q0 =
        _mm_or_si128(_mm_subs_epu8(q1, q0), _mm_subs_epu8(q0, q1));
    __m128i abs_p0q0 =
        _mm_or_si128(_mm_subs_epu8(p0, q0), _mm_subs_epu8(q0, p0));
    __m128i abs_p1q1 =
        _mm_or_si128(_mm_subs_epu8(p1, q1), _mm_subs_epu8(q1, p1));

    flat = _mm_max_epu8(abs_p1p0, abs_q1q0);

    abs_p0q0 = _mm_adds_epu8(abs_p0q0, abs_p0q0);
    abs_p1q1 = _mm_srli_epi16(_mm_and_si128(abs_p1q1, fe), 1);
    mask = _mm_subs_epu8(_mm_adds_epu8(abs_p0q0, abs_p1q1), blimit_v);
    mask = _mm_xor_si128(_mm_cmpeq_epi8(mask, zero), ff);
    // mask |= (abs(p0 - q0) * 2 + abs(p1 - q1) / 2  > blimit) * -1;
    mask = _mm_max_epu8(flat, mask);
    // mask |= (abs(p1 - p0) > limit) * -1;
    // mask |= (abs(q1 - q0) > limit) * -1;
    work = _mm_max_epu8(
        _mm_or_si128(_mm_subs_epu8(p2, p1), _mm_subs_epu8(p1, p2)),
        _mm_or_si128(_mm_subs_epu8(p3, p2), _mm_subs_epu8(p2, p3)));
    mask = _mm_max_epu8(work, mask);
    work = _mm_max_epu8(
        _mm_or_si128(_mm_subs_epu8(q2, q1), _mm_subs_epu8(q1, q2)),
        _mm_or_si128(_mm_subs_epu8(q3, q2), _mm_subs_epu8(q2, q3)));
    mask = _mm_max_epu8(work, mask);
    mask = _mm_subs_epu8(mask, limit_v);
    mask = _mm_cmpeq_epi8(mask, zero);
  }

  if (0xffff == _mm_movemask_epi8(_mm_cmpeq_epi8(mask, zero))) return;

  // loop filter
  {
    const __m128i t4 = _mm_set1_epi8(4);
    const __m128i t3 = _mm_set1_epi8(3);
    const __m128i t80 = _mm_set1_epi8((int8_t)0x80);
    const __m128i te0 = _mm_set1_epi8((int8_t)0xe0);
    const __m128i t1f = _mm_set1_epi8(0x1f);
    const __m128i t1 = _mm_set1_epi8(0x1);
    const __m128i t7f = _mm_set1_epi8(0x7f);
    const __m128i one = _mm_set1_epi8(1);
    __m128i hev;

    hev = _mm_subs_epu8(flat, thresh_v);
    hev = _mm_xor_si128(_mm_cmpeq_epi8(hev, zero), ff);

    __m128i ps1 = _mm_xor_si128(p1, t80);
    __m128i ps0 = _mm_xor_si128(p0, t80);
    __m128i qs0 = _mm_xor_si128(q0, t80);
    __m128i qs1 = _mm_xor_si128(q1, t80);
    __m128i filt;
    __m128i work_a;
    __m128i filter1, filter2;
    __m128i flat_p2, flat_p1, flat_p0, flat_q0, flat_q1, flat_q2;

    filt = _mm_and_si128(_mm_subs_epi8(ps1, qs1), hev);
    work_a = _mm_subs_epi8(qs0, ps0);
    filt = _mm_adds_epi8(filt, work_a);
    filt = _mm_adds_epi8(filt, work_a);
    filt = _mm_adds_epi8(filt, work_a);
    filt = _mm_and_si128(filt, mask);

    filter1 = _mm_adds_epi8(filt, t4);
    filter2 = _mm_adds_epi8(filt, t3);

    work_a = _mm_cmpgt_epi8(zero, filter1);
    filter1 = _mm_srli_epi16(filter1, 3);
    work_a = _mm_and_si128(work_a, te0);
    filter1 = _mm_and_si128(filter1, t1f);
    filter1 = _mm_or_si128(filter1, work_a);
    qs0 = _mm_xor_si128(_mm_subs_epi8(qs0, filter1), t80);

    work_a = _mm_cmpgt_epi8(zero, filter2);
    filter2 = _mm_srli_epi16(filter2, 3);
    work_a = _mm_and_si128(work_a, te0);
    filter2 = _mm_and_si128(filter2, t1f);
    filter2 = _mm_or_si128(filter2, work_a);
    ps0 = _mm_xor_si128(_mm_adds_epi8(ps0, filter2), t80);

    filt = _mm_adds_epi8(filter1, t1);
    work_a = _mm_cmpgt_epi8(zero, filt);
    filt = _mm_srli_epi16(filt, 1);
    work_a = _mm_and_si128(work_a, t80);
    filt = _mm_and_si128(filt, t7f);
    filt = _mm_or_si128(filt, work_a);
    filt = _mm_andnot_si128(hev, filt);
    ps1 = _mm_xor_si128(_mm_adds_epi8(ps1, filt), t80);
    qs1 = _mm_xor_si128(_mm_subs_epi8(qs1, filt), t80);

    __m128i work;
    work = _mm_max_epu8(
        _mm_or_si128(_mm_subs_epu8(p2, p0), _mm_subs_epu8(p0, p2)),
        _mm_or_si128(_mm_subs_epu8(q2, q0), _mm_subs_epu8(q0, q2)));
    flat = _mm_max_epu8(work, flat);
    work = _mm_max_epu8(
        _mm_or_si128(_mm_subs_epu8(p3, p0), _mm_subs_epu8(p0, p3)),
        _mm_or_si128(_mm_subs_epu8(q3, q0), _mm_subs_epu8(q0, q3)));
    flat = _mm_max_epu8(work, flat);
    flat = _mm_subs_epu8(flat, one);
    flat = _mm_cmpeq_epi8(flat, zero);
    flat = _mm_and_si128(flat, mask);

    if (0xffff != _mm_movemask_epi8(_mm_cmpeq_epi8(flat, zero))) {
      const __m256i four = _mm256_set1_epi16(4);
      __m256i pixetFilter_p2p1p0, p2p1p0, q2q1q0, pixetFilter_q2q1q0, sum_p,
          sum_q, res_p, res_q;

      const __m256i filter =
          _mm256_load_si256((__m256i const *)filt_loopfilter_avx2);

      p256_3 = _mm256_shuffle_epi8(p256_3, filter);
      p256_2 = _mm256_shuffle_epi8(p256_2, filter);
      p256_1 = _mm256_shuffle_epi8(p256_1, filter);
      p256_0 = _mm256_shuffle_epi8(p256_0, filter);
      q256_0 = _mm256_shuffle_epi8(q256_0, filter);
      q256_1 = _mm256_shuffle_epi8(q256_1, filter);
      q256_2 = _mm256_shuffle_epi8(q256_2, filter);
      q256_3 = _mm256_shuffle_epi8(q256_3, filter);

      p2p1p0 = _mm256_add_epi16(p256_0, _mm256_add_epi16(p256_2, p256_1));
      q2q1q0 = _mm256_add_epi16(q256_0, _mm256_add_epi16(q256_2, q256_1));

      pixetFilter_p2p1p0 =
          _mm256_add_epi16(four, _mm256_add_epi16(p2p1p0, q2q1q0));
      pixetFilter_q2q1q0 = pixetFilter_p2p1p0;

      pixetFilter_p2p1p0 = _mm256_add_epi16(pixetFilter_p2p1p0, p256_3);
      res_p =
          _mm256_srli_epi16(_mm256_add_epi16(pixetFilter_p2p1p0, p256_0), 3);
      flat_p0 = _mm256_castsi256_si128(
          _mm256_permute4x64_epi64(_mm256_packus_epi16(res_p, res_p), 168));
      p0 = _mm_andnot_si128(flat, ps0);
      flat_p0 = _mm_and_si128(flat, flat_p0);
      p0 = _mm_or_si128(flat_p0, p0);

      pixetFilter_q2q1q0 = _mm256_add_epi16(pixetFilter_q2q1q0, q256_3);
      res_q =
          _mm256_srli_epi16(_mm256_add_epi16(pixetFilter_q2q1q0, q256_0), 3);
      flat_q0 = _mm256_castsi256_si128(
          _mm256_permute4x64_epi64(_mm256_packus_epi16(res_q, res_q), 168));
      q0 = _mm_andnot_si128(flat, qs0);
      flat_q0 = _mm_and_si128(flat, flat_q0);
      q0 = _mm_or_si128(flat_q0, q0);

      sum_p = _mm256_sub_epi16(p256_3, q256_2);
      pixetFilter_p2p1p0 = _mm256_add_epi16(pixetFilter_p2p1p0, sum_p);
      res_p =
          _mm256_srli_epi16(_mm256_add_epi16(pixetFilter_p2p1p0, p256_1), 3);
      flat_p1 = _mm256_castsi256_si128(
          _mm256_permute4x64_epi64(_mm256_packus_epi16(res_p, res_p), 168));
      p1 = _mm_andnot_si128(flat, ps1);
      flat_p1 = _mm_and_si128(flat, flat_p1);
      p1 = _mm_or_si128(flat_p1, p1);

      sum_q = _mm256_sub_epi16(q256_3, p256_2);
      pixetFilter_q2q1q0 = _mm256_add_epi16(pixetFilter_q2q1q0, sum_q);
      res_q =
          _mm256_srli_epi16(_mm256_add_epi16(pixetFilter_q2q1q0, q256_1), 3);
      flat_q1 = _mm256_castsi256_si128(
          _mm256_permute4x64_epi64(_mm256_packus_epi16(res_q, res_q), 168));
      q1 = _mm_andnot_si128(flat, qs1);
      flat_q1 = _mm_and_si128(flat, flat_q1);
      q1 = _mm_or_si128(flat_q1, q1);

      sum_p = _mm256_sub_epi16(p256_3, q256_1);
      pixetFilter_p2p1p0 = _mm256_add_epi16(pixetFilter_p2p1p0, sum_p);
      res_p =
          _mm256_srli_epi16(_mm256_add_epi16(pixetFilter_p2p1p0, p256_2), 3);
      flat_p2 = _mm256_castsi256_si128(
          _mm256_permute4x64_epi64(_mm256_packus_epi16(res_p, res_p), 168));
      p2 = _mm_andnot_si128(flat, p2);
      flat_p2 = _mm_and_si128(flat, flat_p2);
      p2 = _mm_or_si128(flat_p2, p2);

      sum_q = _mm256_sub_epi16(q256_3, p256_1);
      pixetFilter_q2q1q0 = _mm256_add_epi16(pixetFilter_q2q1q0, sum_q);
      res_q =
          _mm256_srli_epi16(_mm256_add_epi16(pixetFilter_q2q1q0, q256_2), 3);
      flat_q2 = _mm256_castsi256_si128(
          _mm256_permute4x64_epi64(_mm256_packus_epi16(res_q, res_q), 168));
      q2 = _mm_andnot_si128(flat, q2);
      flat_q2 = _mm_and_si128(flat, flat_q2);
      q2 = _mm_or_si128(flat_q2, q2);

      _mm_storeu_si128((__m128i *)(s - 3 * p), p2);
      _mm_storeu_si128((__m128i *)(s - 2 * p), p1);
      _mm_storeu_si128((__m128i *)(s - 1 * p), p0);
      _mm_storeu_si128((__m128i *)(s - 0 * p), q0);
      _mm_storeu_si128((__m128i *)(s + 1 * p), q1);
      _mm_storeu_si128((__m128i *)(s + 2 * p), q2);
    } else {
      _mm_storeu_si128((__m128i *)(s - 2 * p), ps1);
      _mm_storeu_si128((__m128i *)(s - 1 * p), ps0);
      _mm_storeu_si128((__m128i *)(s - 0 * p), qs0);
      _mm_storeu_si128((__m128i *)(s + 1 * p), qs1);
    }
  }
}

void aom_lpf_horizontal_14_quad_avx2(unsigned char *s, int p,
                                     const unsigned char *_blimit0,
                                     const unsigned char *_limit0,
                                     const unsigned char *_thresh0) {
  __m256i p256_3, q256_3, p256_2, q256_2, p256_1, q256_1, p256_0, q256_0;
  __m128i p3, p2, p1, p0, q0, q1, q2, q3;
  __m128i mask, flat, flat2;

  const __m128i thresh_v =
      _mm_broadcastb_epi8(_mm_cvtsi32_si128((int)_thresh0[0]));
  const __m128i limit_v =
      _mm_broadcastb_epi8(_mm_cvtsi32_si128((int)_limit0[0]));
  const __m128i blimit_v =
      _mm_broadcastb_epi8(_mm_cvtsi32_si128((int)_blimit0[0]));
  const __m128i zero = _mm_set1_epi16(0);
  const __m128i ff = _mm_cmpeq_epi8(zero, zero);

  p256_3 =
      _mm256_castpd_si256(_mm256_broadcast_pd((__m128d const *)(s - 4 * p)));
  p256_2 =
      _mm256_castpd_si256(_mm256_broadcast_pd((__m128d const *)(s - 3 * p)));
  p256_1 =
      _mm256_castpd_si256(_mm256_broadcast_pd((__m128d const *)(s - 2 * p)));
  p256_0 =
      _mm256_castpd_si256(_mm256_broadcast_pd((__m128d const *)(s - 1 * p)));
  q256_0 =
      _mm256_castpd_si256(_mm256_broadcast_pd((__m128d const *)(s - 0 * p)));
  q256_1 =
      _mm256_castpd_si256(_mm256_broadcast_pd((__m128d const *)(s + 1 * p)));
  q256_2 =
      _mm256_castpd_si256(_mm256_broadcast_pd((__m128d const *)(s + 2 * p)));
  q256_3 =
      _mm256_castpd_si256(_mm256_broadcast_pd((__m128d const *)(s + 3 * p)));

  p3 = _mm256_castsi256_si128(p256_3);
  p2 = _mm256_castsi256_si128(p256_2);
  p1 = _mm256_castsi256_si128(p256_1);
  p0 = _mm256_castsi256_si128(p256_0);
  q0 = _mm256_castsi256_si128(q256_0);
  q1 = _mm256_castsi256_si128(q256_1);
  q2 = _mm256_castsi256_si128(q256_2);
  q3 = _mm256_castsi256_si128(q256_3);

  {
    __m128i work;
    const __m128i fe = _mm_set1_epi8((int8_t)0xfe);
    const __m128i abs_p1p0 =
        _mm_or_si128(_mm_subs_epu8(p1, p0), _mm_subs_epu8(p0, p1));
    const __m128i abs_q1q0 =
        _mm_or_si128(_mm_subs_epu8(q1, q0), _mm_subs_epu8(q0, q1));
    __m128i abs_p0q0 =
        _mm_or_si128(_mm_subs_epu8(p0, q0), _mm_subs_epu8(q0, p0));
    __m128i abs_p1q1 =
        _mm_or_si128(_mm_subs_epu8(p1, q1), _mm_subs_epu8(q1, p1));

    flat = _mm_max_epu8(abs_p1p0, abs_q1q0);

    abs_p0q0 = _mm_adds_epu8(abs_p0q0, abs_p0q0);
    abs_p1q1 = _mm_srli_epi16(_mm_and_si128(abs_p1q1, fe), 1);
    mask = _mm_subs_epu8(_mm_adds_epu8(abs_p0q0, abs_p1q1), blimit_v);
    mask = _mm_xor_si128(_mm_cmpeq_epi8(mask, zero), ff);
    // mask |= (abs(p0 - q0) * 2 + abs(p1 - q1) / 2  > blimit) * -1;
    mask = _mm_max_epu8(flat, mask);
    // mask |= (abs(p1 - p0) > limit) * -1;
    // mask |= (abs(q1 - q0) > limit) * -1;
    work = _mm_max_epu8(
        _mm_or_si128(_mm_subs_epu8(p2, p1), _mm_subs_epu8(p1, p2)),
        _mm_or_si128(_mm_subs_epu8(p3, p2), _mm_subs_epu8(p2, p3)));
    mask = _mm_max_epu8(work, mask);
    work = _mm_max_epu8(
        _mm_or_si128(_mm_subs_epu8(q2, q1), _mm_subs_epu8(q1, q2)),
        _mm_or_si128(_mm_subs_epu8(q3, q2), _mm_subs_epu8(q2, q3)));
    mask = _mm_max_epu8(work, mask);
    mask = _mm_subs_epu8(mask, limit_v);
    mask = _mm_cmpeq_epi8(mask, zero);
  }

  if (0xffff == _mm_movemask_epi8(_mm_cmpeq_epi8(mask, zero))) return;

  // loop filter
  {
    const __m128i t4 = _mm_set1_epi8(4);
    const __m128i t3 = _mm_set1_epi8(3);
    const __m128i t80 = _mm_set1_epi8((int8_t)0x80);
    const __m128i te0 = _mm_set1_epi8((int8_t)0xe0);
    const __m128i t1f = _mm_set1_epi8(0x1f);
    const __m128i t1 = _mm_set1_epi8(0x1);
    const __m128i t7f = _mm_set1_epi8(0x7f);
    const __m128i one = _mm_set1_epi8(1);
    __m128i hev;

    hev = _mm_subs_epu8(flat, thresh_v);
    hev = _mm_xor_si128(_mm_cmpeq_epi8(hev, zero), ff);

    __m128i ps1 = _mm_xor_si128(p1, t80);
    __m128i ps0 = _mm_xor_si128(p0, t80);
    __m128i qs0 = _mm_xor_si128(q0, t80);
    __m128i qs1 = _mm_xor_si128(q1, t80);
    __m128i filt;
    __m128i work_a;
    __m128i filter1, filter2;
    __m128i flat2_p5, flat2_p4, flat2_p3, flat2_p2, flat2_p1, flat2_p0,
        flat2_q0, flat2_q1, flat2_q2, flat2_q3, flat2_q4, flat2_q5, flat_p2,
        flat_p1, flat_p0, flat_q0, flat_q1, flat_q2;

    filt = _mm_and_si128(_mm_subs_epi8(ps1, qs1), hev);
    work_a = _mm_subs_epi8(qs0, ps0);
    filt = _mm_adds_epi8(filt, work_a);
    filt = _mm_adds_epi8(filt, work_a);
    filt = _mm_adds_epi8(filt, work_a);
    filt = _mm_and_si128(filt, mask);

    filter1 = _mm_adds_epi8(filt, t4);
    filter2 = _mm_adds_epi8(filt, t3);

    work_a = _mm_cmpgt_epi8(zero, filter1);
    filter1 = _mm_srli_epi16(filter1, 3);
    work_a = _mm_and_si128(work_a, te0);
    filter1 = _mm_and_si128(filter1, t1f);
    filter1 = _mm_or_si128(filter1, work_a);
    qs0 = _mm_xor_si128(_mm_subs_epi8(qs0, filter1), t80);

    work_a = _mm_cmpgt_epi8(zero, filter2);
    filter2 = _mm_srli_epi16(filter2, 3);
    work_a = _mm_and_si128(work_a, te0);
    filter2 = _mm_and_si128(filter2, t1f);
    filter2 = _mm_or_si128(filter2, work_a);
    ps0 = _mm_xor_si128(_mm_adds_epi8(ps0, filter2), t80);

    filt = _mm_adds_epi8(filter1, t1);
    work_a = _mm_cmpgt_epi8(zero, filt);
    filt = _mm_srli_epi16(filt, 1);
    work_a = _mm_and_si128(work_a, t80);
    filt = _mm_and_si128(filt, t7f);
    filt = _mm_or_si128(filt, work_a);
    filt = _mm_andnot_si128(hev, filt);
    ps1 = _mm_xor_si128(_mm_adds_epi8(ps1, filt), t80);
    qs1 = _mm_xor_si128(_mm_subs_epi8(qs1, filt), t80);

    __m128i work;
    work = _mm_max_epu8(
        _mm_or_si128(_mm_subs_epu8(p2, p0), _mm_subs_epu8(p0, p2)),
        _mm_or_si128(_mm_subs_epu8(q2, q0), _mm_subs_epu8(q0, q2)));
    flat = _mm_max_epu8(work, flat);
    work = _mm_max_epu8(
        _mm_or_si128(_mm_subs_epu8(p3, p0), _mm_subs_epu8(p0, p3)),
        _mm_or_si128(_mm_subs_epu8(q3, q0), _mm_subs_epu8(q0, q3)));
    flat = _mm_max_epu8(work, flat);
    flat = _mm_subs_epu8(flat, one);
    flat = _mm_cmpeq_epi8(flat, zero);
    flat = _mm_and_si128(flat, mask);

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // flat and wide flat calculations
    if (0xffff != _mm_movemask_epi8(_mm_cmpeq_epi8(flat, zero))) {
      const __m256i eight = _mm256_set1_epi16(8);
      const __m256i four = _mm256_set1_epi16(4);
      __m256i pixelFilter_p, pixelFilter_q, pixetFilter_p2p1p0, p2p1p0, q2q1q0,
          pixetFilter_q2q1q0, sum_p, sum_q, res_p, res_q;
      __m256i p256_4, q256_4, p256_5, q256_5, p256_6, q256_6;
      __m128i p4, q4, p5, q5, p6, q6;

      p256_4 = _mm256_castpd_si256(
          _mm256_broadcast_pd((__m128d const *)(s - 5 * p)));
      q256_4 = _mm256_castpd_si256(
          _mm256_broadcast_pd((__m128d const *)(s + 4 * p)));
      p4 = _mm256_castsi256_si128(p256_4);
      q4 = _mm256_castsi256_si128(q256_4);
      work = _mm_max_epu8(
          _mm_or_si128(_mm_subs_epu8(p4, p0), _mm_subs_epu8(p0, p4)),
          _mm_or_si128(_mm_subs_epu8(q4, q0), _mm_subs_epu8(q0, q4)));

      p256_5 = _mm256_castpd_si256(
          _mm256_broadcast_pd((__m128d const *)(s - 6 * p)));
      q256_5 = _mm256_castpd_si256(
          _mm256_broadcast_pd((__m128d const *)(s + 5 * p)));
      p5 = _mm256_castsi256_si128(p256_5);
      q5 = _mm256_castsi256_si128(q256_5);
      flat2 = _mm_max_epu8(
          _mm_or_si128(_mm_subs_epu8(p5, p0), _mm_subs_epu8(p0, p5)),
          _mm_or_si128(_mm_subs_epu8(q5, q0), _mm_subs_epu8(q0, q5)));

      flat2 = _mm_max_epu8(work, flat2);

      p256_6 = _mm256_castpd_si256(
          _mm256_broadcast_pd((__m128d const *)(s - 7 * p)));
      q256_6 = _mm256_castpd_si256(
          _mm256_broadcast_pd((__m128d const *)(s + 6 * p)));
      p6 = _mm256_castsi256_si128(p256_6);
      q6 = _mm256_castsi256_si128(q256_6);
      work = _mm_max_epu8(
          _mm_or_si128(_mm_subs_epu8(p6, p0), _mm_subs_epu8(p0, p6)),
          _mm_or_si128(_mm_subs_epu8(q6, q0), _mm_subs_epu8(q0, q6)));

      flat2 = _mm_max_epu8(work, flat2);

      flat2 = _mm_subs_epu8(flat2, one);
      flat2 = _mm_cmpeq_epi8(flat2, zero);
      flat2 = _mm_and_si128(flat2, flat);  // flat2 & flat & mask

      const __m256i filter =
          _mm256_load_si256((__m256i const *)filt_loopfilter_avx2);

      p256_3 = _mm256_shuffle_epi8(p256_3, filter);
      p256_2 = _mm256_shuffle_epi8(p256_2, filter);
      p256_1 = _mm256_shuffle_epi8(p256_1, filter);
      p256_0 = _mm256_shuffle_epi8(p256_0, filter);
      q256_0 = _mm256_shuffle_epi8(q256_0, filter);
      q256_1 = _mm256_shuffle_epi8(q256_1, filter);
      q256_2 = _mm256_shuffle_epi8(q256_2, filter);
      q256_3 = _mm256_shuffle_epi8(q256_3, filter);

      p2p1p0 = _mm256_add_epi16(p256_0, _mm256_add_epi16(p256_2, p256_1));
      q2q1q0 = _mm256_add_epi16(q256_0, _mm256_add_epi16(q256_2, q256_1));

      pixetFilter_p2p1p0 =
          _mm256_add_epi16(four, _mm256_add_epi16(p2p1p0, q2q1q0));
      pixetFilter_q2q1q0 = pixetFilter_p2p1p0;

      pixetFilter_p2p1p0 = _mm256_add_epi16(pixetFilter_p2p1p0, p256_3);
      res_p =
          _mm256_srli_epi16(_mm256_add_epi16(pixetFilter_p2p1p0, p256_0), 3);
      flat_p0 = _mm256_castsi256_si128(
          _mm256_permute4x64_epi64(_mm256_packus_epi16(res_p, res_p), 168));
      p0 = _mm_andnot_si128(flat, ps0);
      flat_p0 = _mm_and_si128(flat, flat_p0);
      p0 = _mm_or_si128(flat_p0, p0);

      pixetFilter_q2q1q0 = _mm256_add_epi16(pixetFilter_q2q1q0, q256_3);
      res_q =
          _mm256_srli_epi16(_mm256_add_epi16(pixetFilter_q2q1q0, q256_0), 3);
      flat_q0 = _mm256_castsi256_si128(
          _mm256_permute4x64_epi64(_mm256_packus_epi16(res_q, res_q), 168));
      q0 = _mm_andnot_si128(flat, qs0);
      flat_q0 = _mm_and_si128(flat, flat_q0);
      q0 = _mm_or_si128(flat_q0, q0);

      sum_p = _mm256_sub_epi16(p256_3, q256_2);
      pixetFilter_p2p1p0 = _mm256_add_epi16(pixetFilter_p2p1p0, sum_p);
      res_p =
          _mm256_srli_epi16(_mm256_add_epi16(pixetFilter_p2p1p0, p256_1), 3);
      flat_p1 = _mm256_castsi256_si128(
          _mm256_permute4x64_epi64(_mm256_packus_epi16(res_p, res_p), 168));
      p1 = _mm_andnot_si128(flat, ps1);
      flat_p1 = _mm_and_si128(flat, flat_p1);
      p1 = _mm_or_si128(flat_p1, p1);

      sum_q = _mm256_sub_epi16(q256_3, p256_2);
      pixetFilter_q2q1q0 = _mm256_add_epi16(pixetFilter_q2q1q0, sum_q);
      res_q =
          _mm256_srli_epi16(_mm256_add_epi16(pixetFilter_q2q1q0, q256_1), 3);
      flat_q1 = _mm256_castsi256_si128(
          _mm256_permute4x64_epi64(_mm256_packus_epi16(res_q, res_q), 168));
      q1 = _mm_andnot_si128(flat, qs1);
      flat_q1 = _mm_and_si128(flat, flat_q1);
      q1 = _mm_or_si128(flat_q1, q1);

      sum_p = _mm256_sub_epi16(p256_3, q256_1);
      pixetFilter_p2p1p0 = _mm256_add_epi16(pixetFilter_p2p1p0, sum_p);
      res_p =
          _mm256_srli_epi16(_mm256_add_epi16(pixetFilter_p2p1p0, p256_2), 3);
      flat_p2 = _mm256_castsi256_si128(
          _mm256_permute4x64_epi64(_mm256_packus_epi16(res_p, res_p), 168));
      p2 = _mm_andnot_si128(flat, p2);
      flat_p2 = _mm_and_si128(flat, flat_p2);
      p2 = _mm_or_si128(flat_p2, p2);

      sum_q = _mm256_sub_epi16(q256_3, p256_1);
      pixetFilter_q2q1q0 = _mm256_add_epi16(pixetFilter_q2q1q0, sum_q);
      res_q =
          _mm256_srli_epi16(_mm256_add_epi16(pixetFilter_q2q1q0, q256_2), 3);
      flat_q2 = _mm256_castsi256_si128(
          _mm256_permute4x64_epi64(_mm256_packus_epi16(res_q, res_q), 168));
      q2 = _mm_andnot_si128(flat, q2);
      flat_q2 = _mm_and_si128(flat, flat_q2);
      q2 = _mm_or_si128(flat_q2, q2);

      if (0xffff != _mm_movemask_epi8(_mm_cmpeq_epi8(flat2, zero))) {
        p256_6 = _mm256_shuffle_epi8(p256_6, filter);
        p256_5 = _mm256_shuffle_epi8(p256_5, filter);
        p256_4 = _mm256_shuffle_epi8(p256_4, filter);
        q256_4 = _mm256_shuffle_epi8(q256_4, filter);
        q256_5 = _mm256_shuffle_epi8(q256_5, filter);
        q256_6 = _mm256_shuffle_epi8(q256_6, filter);

        pixelFilter_p =
            _mm256_add_epi16(p256_5, _mm256_add_epi16(p256_4, p256_3));
        pixelFilter_q =
            _mm256_add_epi16(q256_5, _mm256_add_epi16(q256_4, q256_3));

        pixelFilter_p = _mm256_add_epi16(pixelFilter_p, p2p1p0);
        pixelFilter_q = _mm256_add_epi16(pixelFilter_q, q2q1q0);

        pixelFilter_p = _mm256_add_epi16(pixelFilter_p, p256_0);
        pixelFilter_q = _mm256_add_epi16(pixelFilter_q, q256_0);
        pixelFilter_p = _mm256_add_epi16(
            eight, _mm256_add_epi16(pixelFilter_p, pixelFilter_q));
        pixelFilter_q = pixelFilter_p;

        pixelFilter_p =
            _mm256_add_epi16(_mm256_add_epi16(p256_6, p256_1), pixelFilter_p);
        res_p = _mm256_srli_epi16(pixelFilter_p, 4);
        flat2_p0 = _mm256_castsi256_si128(
            _mm256_permute4x64_epi64(_mm256_packus_epi16(res_p, res_p), 168));
        p0 = _mm_andnot_si128(flat2, p0);
        flat2_p0 = _mm_and_si128(flat2, flat2_p0);
        p0 = _mm_or_si128(flat2_p0, p0);
        _mm_storeu_si128((__m128i *)(s - 1 * p), p0);

        pixelFilter_q =
            _mm256_add_epi16(_mm256_add_epi16(q256_6, q256_1), pixelFilter_q);
        res_q = _mm256_srli_epi16(pixelFilter_q, 4);
        flat2_q0 = _mm256_castsi256_si128(
            _mm256_permute4x64_epi64(_mm256_packus_epi16(res_q, res_q), 168));
        q0 = _mm_andnot_si128(flat2, q0);
        flat2_q0 = _mm_and_si128(flat2, flat2_q0);
        q0 = _mm_or_si128(flat2_q0, q0);
        _mm_storeu_si128((__m128i *)(s - 0 * p), q0);

        sum_p = _mm256_add_epi16(_mm256_sub_epi16(p256_6, q256_5),
                                 _mm256_sub_epi16(p256_2, q256_0));
        pixelFilter_p = _mm256_add_epi16(pixelFilter_p, sum_p);
        res_p = _mm256_srli_epi16(pixelFilter_p, 4);
        flat2_p1 = _mm256_castsi256_si128(
            _mm256_permute4x64_epi64(_mm256_packus_epi16(res_p, res_p), 168));
        p1 = _mm_andnot_si128(flat2, p1);
        flat2_p1 = _mm_and_si128(flat2, flat2_p1);
        p1 = _mm_or_si128(flat2_p1, p1);
        _mm_storeu_si128((__m128i *)(s - 2 * p), p1);

        sum_q = _mm256_add_epi16(_mm256_sub_epi16(q256_6, p256_5),
                                 _mm256_sub_epi16(q256_2, p256_0));
        pixelFilter_q = _mm256_add_epi16(pixelFilter_q, sum_q);
        res_q = _mm256_srli_epi16(pixelFilter_q, 4);
        flat2_q1 = _mm256_castsi256_si128(
            _mm256_permute4x64_epi64(_mm256_packus_epi16(res_q, res_q), 168));
        q1 = _mm_andnot_si128(flat2, q1);
        flat2_q1 = _mm_and_si128(flat2, flat2_q1);
        q1 = _mm_or_si128(flat2_q1, q1);
        _mm_storeu_si128((__m128i *)(s + 1 * p), q1);

        sum_p = _mm256_add_epi16(_mm256_sub_epi16(p256_6, q256_4),
                                 _mm256_sub_epi16(p256_3, p256_0));
        pixelFilter_p = _mm256_add_epi16(pixelFilter_p, sum_p);
        res_p = _mm256_srli_epi16(pixelFilter_p, 4);
        flat2_p2 = _mm256_castsi256_si128(
            _mm256_permute4x64_epi64(_mm256_packus_epi16(res_p, res_p), 168));
        p2 = _mm_andnot_si128(flat2, p2);
        flat2_p2 = _mm_and_si128(flat2, flat2_p2);
        p2 = _mm_or_si128(flat2_p2, p2);
        _mm_storeu_si128((__m128i *)(s - 3 * p), p2);

        sum_q = _mm256_add_epi16(_mm256_sub_epi16(q256_6, p256_4),
                                 _mm256_sub_epi16(q256_3, q256_0));
        pixelFilter_q = _mm256_add_epi16(pixelFilter_q, sum_q);
        res_q = _mm256_srli_epi16(pixelFilter_q, 4);
        flat2_q2 = _mm256_castsi256_si128(
            _mm256_permute4x64_epi64(_mm256_packus_epi16(res_q, res_q), 168));
        q2 = _mm_andnot_si128(flat2, q2);
        flat2_q2 = _mm_and_si128(flat2, flat2_q2);
        q2 = _mm_or_si128(flat2_q2, q2);
        _mm_storeu_si128((__m128i *)(s + 2 * p), q2);

        sum_p = _mm256_add_epi16(_mm256_sub_epi16(p256_6, q256_3),
                                 _mm256_sub_epi16(p256_4, p256_1));
        pixelFilter_p = _mm256_add_epi16(pixelFilter_p, sum_p);
        res_p = _mm256_srli_epi16(pixelFilter_p, 4);
        flat2_p3 = _mm256_castsi256_si128(
            _mm256_permute4x64_epi64(_mm256_packus_epi16(res_p, res_p), 168));
        p3 = _mm_andnot_si128(flat2, p3);
        flat2_p3 = _mm_and_si128(flat2, flat2_p3);
        p3 = _mm_or_si128(flat2_p3, p3);
        _mm_storeu_si128((__m128i *)(s - 4 * p), p3);

        sum_q = _mm256_add_epi16(_mm256_sub_epi16(q256_6, p256_3),
                                 _mm256_sub_epi16(q256_4, q256_1));
        pixelFilter_q = _mm256_add_epi16(pixelFilter_q, sum_q);
        res_q = _mm256_srli_epi16(pixelFilter_q, 4);
        flat2_q3 = _mm256_castsi256_si128(
            _mm256_permute4x64_epi64(_mm256_packus_epi16(res_q, res_q), 168));
        q3 = _mm_andnot_si128(flat2, q3);
        flat2_q3 = _mm_and_si128(flat2, flat2_q3);
        q3 = _mm_or_si128(flat2_q3, q3);
        _mm_storeu_si128((__m128i *)(s + 3 * p), q3);

        sum_p = _mm256_add_epi16(_mm256_sub_epi16(p256_6, q256_2),
                                 _mm256_sub_epi16(p256_5, p256_2));
        pixelFilter_p = _mm256_add_epi16(pixelFilter_p, sum_p);
        res_p = _mm256_srli_epi16(pixelFilter_p, 4);
        flat2_p4 = _mm256_castsi256_si128(
            _mm256_permute4x64_epi64(_mm256_packus_epi16(res_p, res_p), 168));
        p4 = _mm_andnot_si128(flat2, p4);
        flat2_p4 = _mm_and_si128(flat2, flat2_p4);
        p4 = _mm_or_si128(flat2_p4, p4);
        _mm_storeu_si128((__m128i *)(s - 5 * p), p4);

        sum_q = _mm256_add_epi16(_mm256_sub_epi16(q256_6, p256_2),
                                 _mm256_sub_epi16(q256_5, q256_2));
        pixelFilter_q = _mm256_add_epi16(pixelFilter_q, sum_q);
        res_q = _mm256_srli_epi16(pixelFilter_q, 4);
        flat2_q4 = _mm256_castsi256_si128(
            _mm256_permute4x64_epi64(_mm256_packus_epi16(res_q, res_q), 168));
        q4 = _mm_andnot_si128(flat2, q4);
        flat2_q4 = _mm_and_si128(flat2, flat2_q4);
        q4 = _mm_or_si128(flat2_q4, q4);
        _mm_storeu_si128((__m128i *)(s + 4 * p), q4);

        sum_p = _mm256_add_epi16(_mm256_sub_epi16(p256_6, q256_1),
                                 _mm256_sub_epi16(p256_6, p256_3));
        pixelFilter_p = _mm256_add_epi16(pixelFilter_p, sum_p);
        res_p = _mm256_srli_epi16(pixelFilter_p, 4);
        flat2_p5 = _mm256_castsi256_si128(
            _mm256_permute4x64_epi64(_mm256_packus_epi16(res_p, res_p), 168));
        p5 = _mm_andnot_si128(flat2, p5);
        flat2_p5 = _mm_and_si128(flat2, flat2_p5);
        p5 = _mm_or_si128(flat2_p5, p5);
        _mm_storeu_si128((__m128i *)(s - 6 * p), p5);

        sum_q = _mm256_add_epi16(_mm256_sub_epi16(q256_6, p256_1),
                                 _mm256_sub_epi16(q256_6, q256_3));
        pixelFilter_q = _mm256_add_epi16(pixelFilter_q, sum_q);
        res_q = _mm256_srli_epi16(pixelFilter_q, 4);
        flat2_q5 = _mm256_castsi256_si128(
            _mm256_permute4x64_epi64(_mm256_packus_epi16(res_q, res_q), 168));
        q5 = _mm_andnot_si128(flat2, q5);
        flat2_q5 = _mm_and_si128(flat2, flat2_q5);
        q5 = _mm_or_si128(flat2_q5, q5);
        _mm_storeu_si128((__m128i *)(s + 5 * p), q5);
      } else {
        _mm_storeu_si128((__m128i *)(s - 3 * p), p2);
        _mm_storeu_si128((__m128i *)(s - 2 * p), p1);
        _mm_storeu_si128((__m128i *)(s - 1 * p), p0);
        _mm_storeu_si128((__m128i *)(s - 0 * p), q0);
        _mm_storeu_si128((__m128i *)(s + 1 * p), q1);
        _mm_storeu_si128((__m128i *)(s + 2 * p), q2);
      }
    } else {
      _mm_storeu_si128((__m128i *)(s - 2 * p), ps1);
      _mm_storeu_si128((__m128i *)(s - 1 * p), ps0);
      _mm_storeu_si128((__m128i *)(s - 0 * p), qs0);
      _mm_storeu_si128((__m128i *)(s + 1 * p), qs1);
    }
  }
}

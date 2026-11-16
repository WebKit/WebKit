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

#include <assert.h>
#include <smmintrin.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

void av1_filter_intra_edge_sse4_1(uint8_t *p, int sz, int strength) {
  if (!strength) return;

  DECLARE_ALIGNED(16, static const int8_t, kern[3][16]) = {
    { 4, 8, 4, 0, 4, 8, 4, 0, 4, 8, 4, 0, 4, 8, 4, 0 },  // strength 1: 4,8,4
    { 5, 6, 5, 0, 5, 6, 5, 0, 5, 6, 5, 0, 5, 6, 5, 0 },  // strength 2: 5,6,5
    { 2, 4, 4, 4, 2, 0, 0, 0, 2, 4, 4, 4, 2, 0, 0, 0 }  // strength 3: 2,4,4,4,2
  };

  DECLARE_ALIGNED(16, static const int8_t, v_const[5][16]) = {
    { 0, 1, 2, 3, 1, 2, 3, 4, 2, 3, 4, 5, 3, 4, 5, 6 },
    { 4, 5, 6, 7, 5, 6, 7, 8, 6, 7, 8, 9, 7, 8, 9, 10 },
    { 0, 1, 2, 3, 4, 5, 6, 7, 1, 2, 3, 4, 5, 6, 7, 8 },
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
  };

  // Extend the first and last samples to simplify the loop for the 5-tap case
  p[-1] = p[0];
  __m128i last = _mm_set1_epi8((char)p[sz - 1]);
  _mm_storeu_si128((__m128i *)&p[sz], last);

  // Adjust input pointer for filter support area
  uint8_t *in = (strength == 3) ? p - 1 : p;

  // Avoid modifying first sample
  uint8_t *out = p + 1;
  int len = sz - 1;

  const int use_3tap_filter = (strength < 3);

  if (use_3tap_filter) {
    __m128i coef0 = _mm_lddqu_si128((__m128i const *)kern[strength - 1]);
    __m128i shuf0 = _mm_lddqu_si128((__m128i const *)v_const[0]);
    __m128i shuf1 = _mm_lddqu_si128((__m128i const *)v_const[1]);
    __m128i iden = _mm_lddqu_si128((__m128i *)v_const[3]);
    __m128i in0 = _mm_lddqu_si128((__m128i *)in);
    while (len > 0) {
      int n_out = (len < 8) ? len : 8;
      __m128i d0 = _mm_shuffle_epi8(in0, shuf0);
      __m128i d1 = _mm_shuffle_epi8(in0, shuf1);
      d0 = _mm_maddubs_epi16(d0, coef0);
      d1 = _mm_maddubs_epi16(d1, coef0);
      d0 = _mm_hadd_epi16(d0, d1);
      __m128i eight = _mm_set1_epi16(8);
      d0 = _mm_add_epi16(d0, eight);
      d0 = _mm_srai_epi16(d0, 4);
      d0 = _mm_packus_epi16(d0, d0);
      __m128i out0 = _mm_lddqu_si128((__m128i *)out);
      __m128i n0 = _mm_set1_epi8(n_out);
      __m128i mask = _mm_cmpgt_epi8(n0, iden);
      out0 = _mm_blendv_epi8(out0, d0, mask);
      _mm_storel_epi64((__m128i *)out, out0);
      __m128i in1 = _mm_lddqu_si128((__m128i *)(in + 16));
      in0 = _mm_alignr_epi8(in1, in0, 8);
      in += 8;
      out += 8;
      len -= n_out;
    }
  } else {  // 5-tap filter
    __m128i coef0 = _mm_lddqu_si128((__m128i const *)kern[strength - 1]);
    __m128i two = _mm_set1_epi8(2);
    __m128i shuf_a = _mm_lddqu_si128((__m128i const *)v_const[2]);
    __m128i shuf_b = _mm_add_epi8(shuf_a, two);
    __m128i shuf_c = _mm_add_epi8(shuf_b, two);
    __m128i shuf_d = _mm_add_epi8(shuf_c, two);
    __m128i iden = _mm_lddqu_si128((__m128i *)v_const[3]);
    __m128i in0 = _mm_lddqu_si128((__m128i *)in);
    while (len > 0) {
      int n_out = (len < 8) ? len : 8;
      __m128i d0 = _mm_shuffle_epi8(in0, shuf_a);
      __m128i d1 = _mm_shuffle_epi8(in0, shuf_b);
      __m128i d2 = _mm_shuffle_epi8(in0, shuf_c);
      __m128i d3 = _mm_shuffle_epi8(in0, shuf_d);
      d0 = _mm_maddubs_epi16(d0, coef0);
      d1 = _mm_maddubs_epi16(d1, coef0);
      d2 = _mm_maddubs_epi16(d2, coef0);
      d3 = _mm_maddubs_epi16(d3, coef0);
      d0 = _mm_hadd_epi16(d0, d1);
      d2 = _mm_hadd_epi16(d2, d3);
      d0 = _mm_hadd_epi16(d0, d2);
      __m128i eight = _mm_set1_epi16(8);
      d0 = _mm_add_epi16(d0, eight);
      d0 = _mm_srai_epi16(d0, 4);
      d0 = _mm_packus_epi16(d0, d0);
      __m128i out0 = _mm_lddqu_si128((__m128i *)out);
      __m128i n0 = _mm_set1_epi8(n_out);
      __m128i mask = _mm_cmpgt_epi8(n0, iden);
      out0 = _mm_blendv_epi8(out0, d0, mask);
      _mm_storel_epi64((__m128i *)out, out0);
      __m128i in1 = _mm_lddqu_si128((__m128i *)(in + 16));
      in0 = _mm_alignr_epi8(in1, in0, 8);
      in += 8;
      out += 8;
      len -= n_out;
    }
  }
}

void av1_upsample_intra_edge_sse4_1(uint8_t *p, int sz) {
  // interpolate half-sample positions
  assert(sz <= 24);

  DECLARE_ALIGNED(16, static const int8_t, kernel[1][16]) = {
    { -1, 9, 9, -1, -1, 9, 9, -1, -1, 9, 9, -1, -1, 9, 9, -1 }
  };

  DECLARE_ALIGNED(
      16, static const int8_t,
      v_const[2][16]) = { { 0, 1, 2, 3, 1, 2, 3, 4, 2, 3, 4, 5, 3, 4, 5, 6 },
                          { 4, 5, 6, 7, 5, 6, 7, 8, 6, 7, 8, 9, 7, 8, 9, 10 } };

  // Extend first/last samples (upper-left p[-1], last p[sz-1])
  // to support 4-tap filter
  p[-2] = p[-1];
  p[sz] = p[sz - 1];

  uint8_t *in = &p[-2];
  uint8_t *out = &p[-2];

  int n = sz + 1;  // Input length including upper-left sample

  __m128i in0 = _mm_lddqu_si128((__m128i *)&in[0]);
  __m128i in16 = _mm_lddqu_si128((__m128i *)&in[16]);

  __m128i coef0 = _mm_lddqu_si128((__m128i *)kernel[0]);
  __m128i shuf0 = _mm_lddqu_si128((__m128i *)v_const[0]);
  __m128i shuf1 = _mm_lddqu_si128((__m128i *)v_const[1]);

  while (n > 0) {
    __m128i in8 = _mm_alignr_epi8(in16, in0, 8);
    __m128i d0 = _mm_shuffle_epi8(in0, shuf0);
    __m128i d1 = _mm_shuffle_epi8(in0, shuf1);
    __m128i d2 = _mm_shuffle_epi8(in8, shuf0);
    __m128i d3 = _mm_shuffle_epi8(in8, shuf1);
    d0 = _mm_maddubs_epi16(d0, coef0);
    d1 = _mm_maddubs_epi16(d1, coef0);
    d2 = _mm_maddubs_epi16(d2, coef0);
    d3 = _mm_maddubs_epi16(d3, coef0);
    d0 = _mm_hadd_epi16(d0, d1);
    d2 = _mm_hadd_epi16(d2, d3);
    __m128i eight = _mm_set1_epi16(8);
    d0 = _mm_add_epi16(d0, eight);
    d2 = _mm_add_epi16(d2, eight);
    d0 = _mm_srai_epi16(d0, 4);
    d2 = _mm_srai_epi16(d2, 4);
    d0 = _mm_packus_epi16(d0, d2);
    __m128i in1 = _mm_alignr_epi8(in16, in0, 1);
    __m128i out0 = _mm_unpacklo_epi8(in1, d0);
    __m128i out1 = _mm_unpackhi_epi8(in1, d0);
    _mm_storeu_si128((__m128i *)&out[0], out0);
    _mm_storeu_si128((__m128i *)&out[16], out1);
    in0 = in16;
    in16 = _mm_setzero_si128();
    out += 32;
    n -= 16;
  }
}

#if CONFIG_AV1_HIGHBITDEPTH

void av1_highbd_filter_intra_edge_sse4_1(uint16_t *p, int sz, int strength) {
  if (!strength) return;

  DECLARE_ALIGNED(16, static const int16_t, kern[3][8]) = {
    { 4, 8, 4, 8, 4, 8, 4, 8 },  // strength 1: 4,8,4
    { 5, 6, 5, 6, 5, 6, 5, 6 },  // strength 2: 5,6,5
    { 2, 4, 2, 4, 2, 4, 2, 4 }   // strength 3: 2,4,4,4,2
  };

  DECLARE_ALIGNED(16, static const int16_t,
                  v_const[1][8]) = { { 0, 1, 2, 3, 4, 5, 6, 7 } };

  // Extend the first and last samples to simplify the loop for the 5-tap case
  p[-1] = p[0];
  __m128i last = _mm_set1_epi16(p[sz - 1]);
  _mm_storeu_si128((__m128i *)&p[sz], last);

  // Adjust input pointer for filter support area
  uint16_t *in = (strength == 3) ? p - 1 : p;

  // Avoid modifying first sample
  uint16_t *out = p + 1;
  int len = sz - 1;

  const int use_3tap_filter = (strength < 3);

  if (use_3tap_filter) {
    __m128i coef0 = _mm_lddqu_si128((__m128i const *)kern[strength - 1]);
    __m128i iden = _mm_lddqu_si128((__m128i *)v_const[0]);
    __m128i in0 = _mm_lddqu_si128((__m128i *)&in[0]);
    __m128i in8 = _mm_lddqu_si128((__m128i *)&in[8]);
    while (len > 0) {
      int n_out = (len < 8) ? len : 8;
      __m128i in1 = _mm_alignr_epi8(in8, in0, 2);
      __m128i in2 = _mm_alignr_epi8(in8, in0, 4);
      __m128i in02 = _mm_add_epi16(in0, in2);
      __m128i d0 = _mm_unpacklo_epi16(in02, in1);
      __m128i d1 = _mm_unpackhi_epi16(in02, in1);
      d0 = _mm_mullo_epi16(d0, coef0);
      d1 = _mm_mullo_epi16(d1, coef0);
      d0 = _mm_hadd_epi16(d0, d1);
      __m128i eight = _mm_set1_epi16(8);
      d0 = _mm_add_epi16(d0, eight);
      d0 = _mm_srli_epi16(d0, 4);
      __m128i out0 = _mm_lddqu_si128((__m128i *)out);
      __m128i n0 = _mm_set1_epi16(n_out);
      __m128i mask = _mm_cmpgt_epi16(n0, iden);
      out0 = _mm_blendv_epi8(out0, d0, mask);
      _mm_storeu_si128((__m128i *)out, out0);
      in += 8;
      in0 = in8;
      in8 = _mm_lddqu_si128((__m128i *)&in[8]);
      out += 8;
      len -= n_out;
    }
  } else {  // 5-tap filter
    __m128i coef0 = _mm_lddqu_si128((__m128i const *)kern[strength - 1]);
    __m128i iden = _mm_lddqu_si128((__m128i *)v_const[0]);
    __m128i in0 = _mm_lddqu_si128((__m128i *)&in[0]);
    __m128i in8 = _mm_lddqu_si128((__m128i *)&in[8]);
    while (len > 0) {
      int n_out = (len < 8) ? len : 8;
      __m128i in1 = _mm_alignr_epi8(in8, in0, 2);
      __m128i in2 = _mm_alignr_epi8(in8, in0, 4);
      __m128i in3 = _mm_alignr_epi8(in8, in0, 6);
      __m128i in4 = _mm_alignr_epi8(in8, in0, 8);
      __m128i in04 = _mm_add_epi16(in0, in4);
      __m128i in123 = _mm_add_epi16(in1, in2);
      in123 = _mm_add_epi16(in123, in3);
      __m128i d0 = _mm_unpacklo_epi16(in04, in123);
      __m128i d1 = _mm_unpackhi_epi16(in04, in123);
      d0 = _mm_mullo_epi16(d0, coef0);
      d1 = _mm_mullo_epi16(d1, coef0);
      d0 = _mm_hadd_epi16(d0, d1);
      __m128i eight = _mm_set1_epi16(8);
      d0 = _mm_add_epi16(d0, eight);
      d0 = _mm_srli_epi16(d0, 4);
      __m128i out0 = _mm_lddqu_si128((__m128i *)out);
      __m128i n0 = _mm_set1_epi16(n_out);
      __m128i mask = _mm_cmpgt_epi16(n0, iden);
      out0 = _mm_blendv_epi8(out0, d0, mask);
      _mm_storeu_si128((__m128i *)out, out0);
      in += 8;
      in0 = in8;
      in8 = _mm_lddqu_si128((__m128i *)&in[8]);
      out += 8;
      len -= n_out;
    }
  }
}

void av1_highbd_upsample_intra_edge_sse4_1(uint16_t *p, int sz, int bd) {
  // interpolate half-sample positions
  assert(sz <= 24);

  DECLARE_ALIGNED(16, static const int16_t,
                  kernel[1][8]) = { { -1, 9, -1, 9, -1, 9, -1, 9 } };

  // Extend first/last samples (upper-left p[-1], last p[sz-1])
  // to support 4-tap filter
  p[-2] = p[-1];
  p[sz] = p[sz - 1];

  uint16_t *in = &p[-2];
  uint16_t *out = in;
  int n = sz + 1;

  __m128i in0 = _mm_lddqu_si128((__m128i *)&in[0]);
  __m128i in8 = _mm_lddqu_si128((__m128i *)&in[8]);
  __m128i in16 = _mm_lddqu_si128((__m128i *)&in[16]);
  __m128i in24 = _mm_lddqu_si128((__m128i *)&in[24]);

  while (n > 0) {
    __m128i in1 = _mm_alignr_epi8(in8, in0, 2);
    __m128i in2 = _mm_alignr_epi8(in8, in0, 4);
    __m128i in3 = _mm_alignr_epi8(in8, in0, 6);
    __m128i sum0 = _mm_add_epi16(in0, in3);
    __m128i sum1 = _mm_add_epi16(in1, in2);
    __m128i d0 = _mm_unpacklo_epi16(sum0, sum1);
    __m128i d1 = _mm_unpackhi_epi16(sum0, sum1);
    __m128i coef0 = _mm_lddqu_si128((__m128i *)kernel[0]);
    d0 = _mm_madd_epi16(d0, coef0);
    d1 = _mm_madd_epi16(d1, coef0);
    __m128i eight = _mm_set1_epi32(8);
    d0 = _mm_add_epi32(d0, eight);
    d1 = _mm_add_epi32(d1, eight);
    d0 = _mm_srai_epi32(d0, 4);
    d1 = _mm_srai_epi32(d1, 4);
    d0 = _mm_packus_epi32(d0, d1);
    __m128i max0 = _mm_set1_epi16((1 << bd) - 1);
    d0 = _mm_min_epi16(d0, max0);
    __m128i out0 = _mm_unpacklo_epi16(in1, d0);
    __m128i out1 = _mm_unpackhi_epi16(in1, d0);
    _mm_storeu_si128((__m128i *)&out[0], out0);
    _mm_storeu_si128((__m128i *)&out[8], out1);
    in0 = in8;
    in8 = in16;
    in16 = in24;
    in24 = _mm_setzero_si128();
    out += 16;
    n -= 8;
  }
}

#endif  // CONFIG_AV1_HIGHBITDEPTH

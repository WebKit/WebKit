/*
 *  Copyright (c) 2020, Alliance for Open Media. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <immintrin.h>

#include "config/aom_dsp_rtcd.h"

static INLINE void copy_128(const uint8_t *src, uint8_t *dst) {
  __m128i s[8];
  s[0] = _mm_loadu_si128((__m128i *)(src + 0 * 16));
  s[1] = _mm_loadu_si128((__m128i *)(src + 1 * 16));
  s[2] = _mm_loadu_si128((__m128i *)(src + 2 * 16));
  s[3] = _mm_loadu_si128((__m128i *)(src + 3 * 16));
  s[4] = _mm_loadu_si128((__m128i *)(src + 4 * 16));
  s[5] = _mm_loadu_si128((__m128i *)(src + 5 * 16));
  s[6] = _mm_loadu_si128((__m128i *)(src + 6 * 16));
  s[7] = _mm_loadu_si128((__m128i *)(src + 7 * 16));
  _mm_store_si128((__m128i *)(dst + 0 * 16), s[0]);
  _mm_store_si128((__m128i *)(dst + 1 * 16), s[1]);
  _mm_store_si128((__m128i *)(dst + 2 * 16), s[2]);
  _mm_store_si128((__m128i *)(dst + 3 * 16), s[3]);
  _mm_store_si128((__m128i *)(dst + 4 * 16), s[4]);
  _mm_store_si128((__m128i *)(dst + 5 * 16), s[5]);
  _mm_store_si128((__m128i *)(dst + 6 * 16), s[6]);
  _mm_store_si128((__m128i *)(dst + 7 * 16), s[7]);
}

void aom_convolve_copy_sse2(const uint8_t *src, ptrdiff_t src_stride,
                            uint8_t *dst, ptrdiff_t dst_stride, int w, int h) {
  if (w >= 16) {
    assert(!((intptr_t)dst % 16));
    assert(!(dst_stride % 16));
  }

  if (w == 2) {
    do {
      memmove(dst, src, 2 * sizeof(*src));
      src += src_stride;
      dst += dst_stride;
      memmove(dst, src, 2 * sizeof(*src));
      src += src_stride;
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 4) {
    do {
      memmove(dst, src, 4 * sizeof(*src));
      src += src_stride;
      dst += dst_stride;
      memmove(dst, src, 4 * sizeof(*src));
      src += src_stride;
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 8) {
    do {
      __m128i s[2];
      s[0] = _mm_loadl_epi64((__m128i *)src);
      src += src_stride;
      s[1] = _mm_loadl_epi64((__m128i *)src);
      src += src_stride;
      _mm_storel_epi64((__m128i *)dst, s[0]);
      dst += dst_stride;
      _mm_storel_epi64((__m128i *)dst, s[1]);
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 16) {
    do {
      __m128i s[2];
      s[0] = _mm_loadu_si128((__m128i *)src);
      src += src_stride;
      s[1] = _mm_loadu_si128((__m128i *)src);
      src += src_stride;
      _mm_store_si128((__m128i *)dst, s[0]);
      dst += dst_stride;
      _mm_store_si128((__m128i *)dst, s[1]);
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 32) {
    do {
      __m128i s[4];
      s[0] = _mm_loadu_si128((__m128i *)(src + 0 * 16));
      s[1] = _mm_loadu_si128((__m128i *)(src + 1 * 16));
      src += src_stride;
      s[2] = _mm_loadu_si128((__m128i *)(src + 0 * 16));
      s[3] = _mm_loadu_si128((__m128i *)(src + 1 * 16));
      src += src_stride;
      _mm_store_si128((__m128i *)(dst + 0 * 16), s[0]);
      _mm_store_si128((__m128i *)(dst + 1 * 16), s[1]);
      dst += dst_stride;
      _mm_store_si128((__m128i *)(dst + 0 * 16), s[2]);
      _mm_store_si128((__m128i *)(dst + 1 * 16), s[3]);
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 64) {
    do {
      __m128i s[8];
      s[0] = _mm_loadu_si128((__m128i *)(src + 0 * 16));
      s[1] = _mm_loadu_si128((__m128i *)(src + 1 * 16));
      s[2] = _mm_loadu_si128((__m128i *)(src + 2 * 16));
      s[3] = _mm_loadu_si128((__m128i *)(src + 3 * 16));
      src += src_stride;
      s[4] = _mm_loadu_si128((__m128i *)(src + 0 * 16));
      s[5] = _mm_loadu_si128((__m128i *)(src + 1 * 16));
      s[6] = _mm_loadu_si128((__m128i *)(src + 2 * 16));
      s[7] = _mm_loadu_si128((__m128i *)(src + 3 * 16));
      src += src_stride;
      _mm_store_si128((__m128i *)(dst + 0 * 16), s[0]);
      _mm_store_si128((__m128i *)(dst + 1 * 16), s[1]);
      _mm_store_si128((__m128i *)(dst + 2 * 16), s[2]);
      _mm_store_si128((__m128i *)(dst + 3 * 16), s[3]);
      dst += dst_stride;
      _mm_store_si128((__m128i *)(dst + 0 * 16), s[4]);
      _mm_store_si128((__m128i *)(dst + 1 * 16), s[5]);
      _mm_store_si128((__m128i *)(dst + 2 * 16), s[6]);
      _mm_store_si128((__m128i *)(dst + 3 * 16), s[7]);
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else {
    do {
      copy_128(src, dst);
      src += src_stride;
      dst += dst_stride;
      copy_128(src, dst);
      src += src_stride;
      dst += dst_stride;
      h -= 2;
    } while (h);
  }
}

static INLINE void highbd_copy_64(const uint16_t *src, uint16_t *dst) {
  __m128i s[8];
  s[0] = _mm_loadu_si128((__m128i *)(src + 0 * 8));
  s[1] = _mm_loadu_si128((__m128i *)(src + 1 * 8));
  s[2] = _mm_loadu_si128((__m128i *)(src + 2 * 8));
  s[3] = _mm_loadu_si128((__m128i *)(src + 3 * 8));
  s[4] = _mm_loadu_si128((__m128i *)(src + 4 * 8));
  s[5] = _mm_loadu_si128((__m128i *)(src + 5 * 8));
  s[6] = _mm_loadu_si128((__m128i *)(src + 6 * 8));
  s[7] = _mm_loadu_si128((__m128i *)(src + 7 * 8));
  _mm_store_si128((__m128i *)(dst + 0 * 8), s[0]);
  _mm_store_si128((__m128i *)(dst + 1 * 8), s[1]);
  _mm_store_si128((__m128i *)(dst + 2 * 8), s[2]);
  _mm_store_si128((__m128i *)(dst + 3 * 8), s[3]);
  _mm_store_si128((__m128i *)(dst + 4 * 8), s[4]);
  _mm_store_si128((__m128i *)(dst + 5 * 8), s[5]);
  _mm_store_si128((__m128i *)(dst + 6 * 8), s[6]);
  _mm_store_si128((__m128i *)(dst + 7 * 8), s[7]);
}

static INLINE void highbd_copy_128(const uint16_t *src, uint16_t *dst) {
  __m128i s[16];
  s[0] = _mm_loadu_si128((__m128i *)(src + 0 * 8));
  s[1] = _mm_loadu_si128((__m128i *)(src + 1 * 8));
  s[2] = _mm_loadu_si128((__m128i *)(src + 2 * 8));
  s[3] = _mm_loadu_si128((__m128i *)(src + 3 * 8));
  s[4] = _mm_loadu_si128((__m128i *)(src + 4 * 8));
  s[5] = _mm_loadu_si128((__m128i *)(src + 5 * 8));
  s[6] = _mm_loadu_si128((__m128i *)(src + 6 * 8));
  s[7] = _mm_loadu_si128((__m128i *)(src + 7 * 8));
  s[8] = _mm_loadu_si128((__m128i *)(src + 8 * 8));
  s[9] = _mm_loadu_si128((__m128i *)(src + 9 * 8));
  s[10] = _mm_loadu_si128((__m128i *)(src + 10 * 8));
  s[11] = _mm_loadu_si128((__m128i *)(src + 11 * 8));
  s[12] = _mm_loadu_si128((__m128i *)(src + 12 * 8));
  s[13] = _mm_loadu_si128((__m128i *)(src + 13 * 8));
  s[14] = _mm_loadu_si128((__m128i *)(src + 14 * 8));
  s[15] = _mm_loadu_si128((__m128i *)(src + 15 * 8));
  _mm_store_si128((__m128i *)(dst + 0 * 8), s[0]);
  _mm_store_si128((__m128i *)(dst + 1 * 8), s[1]);
  _mm_store_si128((__m128i *)(dst + 2 * 8), s[2]);
  _mm_store_si128((__m128i *)(dst + 3 * 8), s[3]);
  _mm_store_si128((__m128i *)(dst + 4 * 8), s[4]);
  _mm_store_si128((__m128i *)(dst + 5 * 8), s[5]);
  _mm_store_si128((__m128i *)(dst + 6 * 8), s[6]);
  _mm_store_si128((__m128i *)(dst + 7 * 8), s[7]);
  _mm_store_si128((__m128i *)(dst + 8 * 8), s[8]);
  _mm_store_si128((__m128i *)(dst + 9 * 8), s[9]);
  _mm_store_si128((__m128i *)(dst + 10 * 8), s[10]);
  _mm_store_si128((__m128i *)(dst + 11 * 8), s[11]);
  _mm_store_si128((__m128i *)(dst + 12 * 8), s[12]);
  _mm_store_si128((__m128i *)(dst + 13 * 8), s[13]);
  _mm_store_si128((__m128i *)(dst + 14 * 8), s[14]);
  _mm_store_si128((__m128i *)(dst + 15 * 8), s[15]);
}

void aom_highbd_convolve_copy_sse2(const uint16_t *src, ptrdiff_t src_stride,
                                   uint16_t *dst, ptrdiff_t dst_stride, int w,
                                   int h) {
  if (w >= 16) {
    assert(!((intptr_t)dst % 16));
    assert(!(dst_stride % 16));
  }

  if (w == 2) {
    do {
      __m128i s = _mm_loadl_epi64((__m128i *)src);
      *(int *)dst = _mm_cvtsi128_si32(s);
      src += src_stride;
      dst += dst_stride;
      s = _mm_loadl_epi64((__m128i *)src);
      *(int *)dst = _mm_cvtsi128_si32(s);
      src += src_stride;
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 4) {
    do {
      __m128i s[2];
      s[0] = _mm_loadl_epi64((__m128i *)src);
      src += src_stride;
      s[1] = _mm_loadl_epi64((__m128i *)src);
      src += src_stride;
      _mm_storel_epi64((__m128i *)dst, s[0]);
      dst += dst_stride;
      _mm_storel_epi64((__m128i *)dst, s[1]);
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 8) {
    do {
      __m128i s[2];
      s[0] = _mm_loadu_si128((__m128i *)src);
      src += src_stride;
      s[1] = _mm_loadu_si128((__m128i *)src);
      src += src_stride;
      _mm_store_si128((__m128i *)dst, s[0]);
      dst += dst_stride;
      _mm_store_si128((__m128i *)dst, s[1]);
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 16) {
    do {
      __m128i s[4];
      s[0] = _mm_loadu_si128((__m128i *)(src + 0 * 8));
      s[1] = _mm_loadu_si128((__m128i *)(src + 1 * 8));
      src += src_stride;
      s[2] = _mm_loadu_si128((__m128i *)(src + 0 * 8));
      s[3] = _mm_loadu_si128((__m128i *)(src + 1 * 8));
      src += src_stride;
      _mm_store_si128((__m128i *)(dst + 0 * 8), s[0]);
      _mm_store_si128((__m128i *)(dst + 1 * 8), s[1]);
      dst += dst_stride;
      _mm_store_si128((__m128i *)(dst + 0 * 8), s[2]);
      _mm_store_si128((__m128i *)(dst + 1 * 8), s[3]);
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 32) {
    do {
      __m128i s[8];
      s[0] = _mm_loadu_si128((__m128i *)(src + 0 * 8));
      s[1] = _mm_loadu_si128((__m128i *)(src + 1 * 8));
      s[2] = _mm_loadu_si128((__m128i *)(src + 2 * 8));
      s[3] = _mm_loadu_si128((__m128i *)(src + 3 * 8));
      src += src_stride;
      s[4] = _mm_loadu_si128((__m128i *)(src + 0 * 8));
      s[5] = _mm_loadu_si128((__m128i *)(src + 1 * 8));
      s[6] = _mm_loadu_si128((__m128i *)(src + 2 * 8));
      s[7] = _mm_loadu_si128((__m128i *)(src + 3 * 8));
      src += src_stride;
      _mm_store_si128((__m128i *)(dst + 0 * 8), s[0]);
      _mm_store_si128((__m128i *)(dst + 1 * 8), s[1]);
      _mm_store_si128((__m128i *)(dst + 2 * 8), s[2]);
      _mm_store_si128((__m128i *)(dst + 3 * 8), s[3]);
      dst += dst_stride;
      _mm_store_si128((__m128i *)(dst + 0 * 8), s[4]);
      _mm_store_si128((__m128i *)(dst + 1 * 8), s[5]);
      _mm_store_si128((__m128i *)(dst + 2 * 8), s[6]);
      _mm_store_si128((__m128i *)(dst + 3 * 8), s[7]);
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else if (w == 64) {
    do {
      highbd_copy_64(src, dst);
      src += src_stride;
      dst += dst_stride;
      highbd_copy_64(src, dst);
      src += src_stride;
      dst += dst_stride;
      h -= 2;
    } while (h);
  } else {
    do {
      highbd_copy_128(src, dst);
      src += src_stride;
      dst += dst_stride;
      highbd_copy_128(src, dst);
      src += src_stride;
      dst += dst_stride;
      h -= 2;
    } while (h);
  }
}

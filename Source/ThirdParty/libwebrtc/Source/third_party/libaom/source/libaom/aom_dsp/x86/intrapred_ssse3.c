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

#include <tmmintrin.h>

#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/intrapred_common.h"

// -----------------------------------------------------------------------------
// PAETH_PRED

// Return 8 16-bit pixels in one row
static INLINE __m128i paeth_8x1_pred(const __m128i *left, const __m128i *top,
                                     const __m128i *topleft) {
  const __m128i base = _mm_sub_epi16(_mm_add_epi16(*top, *left), *topleft);

  __m128i pl = _mm_abs_epi16(_mm_sub_epi16(base, *left));
  __m128i pt = _mm_abs_epi16(_mm_sub_epi16(base, *top));
  __m128i ptl = _mm_abs_epi16(_mm_sub_epi16(base, *topleft));

  __m128i mask1 = _mm_cmpgt_epi16(pl, pt);
  mask1 = _mm_or_si128(mask1, _mm_cmpgt_epi16(pl, ptl));
  __m128i mask2 = _mm_cmpgt_epi16(pt, ptl);

  pl = _mm_andnot_si128(mask1, *left);

  ptl = _mm_and_si128(mask2, *topleft);
  pt = _mm_andnot_si128(mask2, *top);
  pt = _mm_or_si128(pt, ptl);
  pt = _mm_and_si128(mask1, pt);

  return _mm_or_si128(pl, pt);
}

void aom_paeth_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_loadl_epi64((const __m128i *)left);
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 4; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_8x1_pred(&l16, &t16, &tl16);

    *(int *)dst = _mm_cvtsi128_si32(_mm_packus_epi16(row, row));
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_4x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_loadl_epi64((const __m128i *)left);
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 8; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_8x1_pred(&l16, &t16, &tl16);

    *(int *)dst = _mm_cvtsi128_si32(_mm_packus_epi16(row, row));
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_4x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_load_si128((const __m128i *)left);
  const __m128i t = _mm_cvtsi32_si128(((const uint32_t *)above)[0]);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);

  for (int i = 0; i < 16; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_8x1_pred(&l16, &t16, &tl16);

    *(int *)dst = _mm_cvtsi128_si32(_mm_packus_epi16(row, row));
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_8x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_loadl_epi64((const __m128i *)left);
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 4; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_8x1_pred(&l16, &t16, &tl16);

    _mm_storel_epi64((__m128i *)dst, _mm_packus_epi16(row, row));
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_loadl_epi64((const __m128i *)left);
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 8; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_8x1_pred(&l16, &t16, &tl16);

    _mm_storel_epi64((__m128i *)dst, _mm_packus_epi16(row, row));
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_8x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_load_si128((const __m128i *)left);
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 16; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_8x1_pred(&l16, &t16, &tl16);

    _mm_storel_epi64((__m128i *)dst, _mm_packus_epi16(row, row));
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_8x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  const __m128i one = _mm_set1_epi16(1);

  for (int j = 0; j < 2; ++j) {
    const __m128i l = _mm_load_si128((const __m128i *)(left + j * 16));
    __m128i rep = _mm_set1_epi16((short)0x8000);
    for (int i = 0; i < 16; ++i) {
      const __m128i l16 = _mm_shuffle_epi8(l, rep);
      const __m128i row = paeth_8x1_pred(&l16, &t16, &tl16);

      _mm_storel_epi64((__m128i *)dst, _mm_packus_epi16(row, row));
      dst += stride;
      rep = _mm_add_epi16(rep, one);
    }
  }
}

// Return 16 8-bit pixels in one row
static INLINE __m128i paeth_16x1_pred(const __m128i *left, const __m128i *top0,
                                      const __m128i *top1,
                                      const __m128i *topleft) {
  const __m128i p0 = paeth_8x1_pred(left, top0, topleft);
  const __m128i p1 = paeth_8x1_pred(left, top1, topleft);
  return _mm_packus_epi16(p0, p1);
}

void aom_paeth_predictor_16x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_cvtsi32_si128(((const uint32_t *)left)[0]);
  const __m128i t = _mm_load_si128((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i top0 = _mm_unpacklo_epi8(t, zero);
  const __m128i top1 = _mm_unpackhi_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);

  for (int i = 0; i < 4; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_16x1_pred(&l16, &top0, &top1, &tl16);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_16x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_loadl_epi64((const __m128i *)left);
  const __m128i t = _mm_load_si128((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i top0 = _mm_unpacklo_epi8(t, zero);
  const __m128i top1 = _mm_unpackhi_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 8; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_16x1_pred(&l16, &top0, &top1, &tl16);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m128i l = _mm_load_si128((const __m128i *)left);
  const __m128i t = _mm_load_si128((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i top0 = _mm_unpacklo_epi8(t, zero);
  const __m128i top1 = _mm_unpackhi_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 16; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_16x1_pred(&l16, &top0, &top1, &tl16);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_16x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m128i l = _mm_load_si128((const __m128i *)left);
  const __m128i t = _mm_load_si128((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i top0 = _mm_unpacklo_epi8(t, zero);
  const __m128i top1 = _mm_unpackhi_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);
  __m128i l16;

  int i;
  for (i = 0; i < 16; ++i) {
    l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_16x1_pred(&l16, &top0, &top1, &tl16);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }

  l = _mm_load_si128((const __m128i *)(left + 16));
  rep = _mm_set1_epi16((short)0x8000);
  for (i = 0; i < 16; ++i) {
    l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_16x1_pred(&l16, &top0, &top1, &tl16);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_16x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i t = _mm_load_si128((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i top0 = _mm_unpacklo_epi8(t, zero);
  const __m128i top1 = _mm_unpackhi_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  const __m128i one = _mm_set1_epi16(1);

  for (int j = 0; j < 4; ++j) {
    const __m128i l = _mm_load_si128((const __m128i *)(left + j * 16));
    __m128i rep = _mm_set1_epi16((short)0x8000);
    for (int i = 0; i < 16; ++i) {
      const __m128i l16 = _mm_shuffle_epi8(l, rep);
      const __m128i row = paeth_16x1_pred(&l16, &top0, &top1, &tl16);
      _mm_store_si128((__m128i *)dst, row);
      dst += stride;
      rep = _mm_add_epi16(rep, one);
    }
  }
}

void aom_paeth_predictor_32x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  const __m128i a = _mm_load_si128((const __m128i *)above);
  const __m128i b = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i zero = _mm_setzero_si128();
  const __m128i al = _mm_unpacklo_epi8(a, zero);
  const __m128i ah = _mm_unpackhi_epi8(a, zero);
  const __m128i bl = _mm_unpacklo_epi8(b, zero);
  const __m128i bh = _mm_unpackhi_epi8(b, zero);

  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);
  const __m128i l = _mm_loadl_epi64((const __m128i *)left);
  __m128i l16;

  for (int i = 0; i < 8; ++i) {
    l16 = _mm_shuffle_epi8(l, rep);
    const __m128i r32l = paeth_16x1_pred(&l16, &al, &ah, &tl16);
    const __m128i r32h = paeth_16x1_pred(&l16, &bl, &bh, &tl16);

    _mm_store_si128((__m128i *)dst, r32l);
    _mm_store_si128((__m128i *)(dst + 16), r32h);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_32x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i a = _mm_load_si128((const __m128i *)above);
  const __m128i b = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i zero = _mm_setzero_si128();
  const __m128i al = _mm_unpacklo_epi8(a, zero);
  const __m128i ah = _mm_unpackhi_epi8(a, zero);
  const __m128i bl = _mm_unpacklo_epi8(b, zero);
  const __m128i bh = _mm_unpackhi_epi8(b, zero);

  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);
  __m128i l = _mm_load_si128((const __m128i *)left);
  __m128i l16;

  int i;
  for (i = 0; i < 16; ++i) {
    l16 = _mm_shuffle_epi8(l, rep);
    const __m128i r32l = paeth_16x1_pred(&l16, &al, &ah, &tl16);
    const __m128i r32h = paeth_16x1_pred(&l16, &bl, &bh, &tl16);

    _mm_store_si128((__m128i *)dst, r32l);
    _mm_store_si128((__m128i *)(dst + 16), r32h);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i a = _mm_load_si128((const __m128i *)above);
  const __m128i b = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i zero = _mm_setzero_si128();
  const __m128i al = _mm_unpacklo_epi8(a, zero);
  const __m128i ah = _mm_unpackhi_epi8(a, zero);
  const __m128i bl = _mm_unpacklo_epi8(b, zero);
  const __m128i bh = _mm_unpackhi_epi8(b, zero);

  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);
  __m128i l = _mm_load_si128((const __m128i *)left);
  __m128i l16;

  int i;
  for (i = 0; i < 16; ++i) {
    l16 = _mm_shuffle_epi8(l, rep);
    const __m128i r32l = paeth_16x1_pred(&l16, &al, &ah, &tl16);
    const __m128i r32h = paeth_16x1_pred(&l16, &bl, &bh, &tl16);

    _mm_store_si128((__m128i *)dst, r32l);
    _mm_store_si128((__m128i *)(dst + 16), r32h);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }

  rep = _mm_set1_epi16((short)0x8000);
  l = _mm_load_si128((const __m128i *)(left + 16));
  for (i = 0; i < 16; ++i) {
    l16 = _mm_shuffle_epi8(l, rep);
    const __m128i r32l = paeth_16x1_pred(&l16, &al, &ah, &tl16);
    const __m128i r32h = paeth_16x1_pred(&l16, &bl, &bh, &tl16);

    _mm_store_si128((__m128i *)dst, r32l);
    _mm_store_si128((__m128i *)(dst + 16), r32h);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_32x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i a = _mm_load_si128((const __m128i *)above);
  const __m128i b = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i zero = _mm_setzero_si128();
  const __m128i al = _mm_unpacklo_epi8(a, zero);
  const __m128i ah = _mm_unpackhi_epi8(a, zero);
  const __m128i bl = _mm_unpacklo_epi8(b, zero);
  const __m128i bh = _mm_unpackhi_epi8(b, zero);

  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  const __m128i one = _mm_set1_epi16(1);
  __m128i l16;

  int i, j;
  for (j = 0; j < 4; ++j) {
    const __m128i l = _mm_load_si128((const __m128i *)(left + j * 16));
    __m128i rep = _mm_set1_epi16((short)0x8000);
    for (i = 0; i < 16; ++i) {
      l16 = _mm_shuffle_epi8(l, rep);
      const __m128i r32l = paeth_16x1_pred(&l16, &al, &ah, &tl16);
      const __m128i r32h = paeth_16x1_pred(&l16, &bl, &bh, &tl16);

      _mm_store_si128((__m128i *)dst, r32l);
      _mm_store_si128((__m128i *)(dst + 16), r32h);
      dst += stride;
      rep = _mm_add_epi16(rep, one);
    }
  }
}

void aom_paeth_predictor_64x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i a = _mm_load_si128((const __m128i *)above);
  const __m128i b = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i c = _mm_load_si128((const __m128i *)(above + 32));
  const __m128i d = _mm_load_si128((const __m128i *)(above + 48));
  const __m128i zero = _mm_setzero_si128();
  const __m128i al = _mm_unpacklo_epi8(a, zero);
  const __m128i ah = _mm_unpackhi_epi8(a, zero);
  const __m128i bl = _mm_unpacklo_epi8(b, zero);
  const __m128i bh = _mm_unpackhi_epi8(b, zero);
  const __m128i cl = _mm_unpacklo_epi8(c, zero);
  const __m128i ch = _mm_unpackhi_epi8(c, zero);
  const __m128i dl = _mm_unpacklo_epi8(d, zero);
  const __m128i dh = _mm_unpackhi_epi8(d, zero);

  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  const __m128i one = _mm_set1_epi16(1);
  __m128i l16;

  int i, j;
  for (j = 0; j < 2; ++j) {
    const __m128i l = _mm_load_si128((const __m128i *)(left + j * 16));
    __m128i rep = _mm_set1_epi16((short)0x8000);
    for (i = 0; i < 16; ++i) {
      l16 = _mm_shuffle_epi8(l, rep);
      const __m128i r0 = paeth_16x1_pred(&l16, &al, &ah, &tl16);
      const __m128i r1 = paeth_16x1_pred(&l16, &bl, &bh, &tl16);
      const __m128i r2 = paeth_16x1_pred(&l16, &cl, &ch, &tl16);
      const __m128i r3 = paeth_16x1_pred(&l16, &dl, &dh, &tl16);

      _mm_store_si128((__m128i *)dst, r0);
      _mm_store_si128((__m128i *)(dst + 16), r1);
      _mm_store_si128((__m128i *)(dst + 32), r2);
      _mm_store_si128((__m128i *)(dst + 48), r3);
      dst += stride;
      rep = _mm_add_epi16(rep, one);
    }
  }
}

void aom_paeth_predictor_64x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i a = _mm_load_si128((const __m128i *)above);
  const __m128i b = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i c = _mm_load_si128((const __m128i *)(above + 32));
  const __m128i d = _mm_load_si128((const __m128i *)(above + 48));
  const __m128i zero = _mm_setzero_si128();
  const __m128i al = _mm_unpacklo_epi8(a, zero);
  const __m128i ah = _mm_unpackhi_epi8(a, zero);
  const __m128i bl = _mm_unpacklo_epi8(b, zero);
  const __m128i bh = _mm_unpackhi_epi8(b, zero);
  const __m128i cl = _mm_unpacklo_epi8(c, zero);
  const __m128i ch = _mm_unpackhi_epi8(c, zero);
  const __m128i dl = _mm_unpacklo_epi8(d, zero);
  const __m128i dh = _mm_unpackhi_epi8(d, zero);

  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  const __m128i one = _mm_set1_epi16(1);
  __m128i l16;

  int i, j;
  for (j = 0; j < 4; ++j) {
    const __m128i l = _mm_load_si128((const __m128i *)(left + j * 16));
    __m128i rep = _mm_set1_epi16((short)0x8000);
    for (i = 0; i < 16; ++i) {
      l16 = _mm_shuffle_epi8(l, rep);
      const __m128i r0 = paeth_16x1_pred(&l16, &al, &ah, &tl16);
      const __m128i r1 = paeth_16x1_pred(&l16, &bl, &bh, &tl16);
      const __m128i r2 = paeth_16x1_pred(&l16, &cl, &ch, &tl16);
      const __m128i r3 = paeth_16x1_pred(&l16, &dl, &dh, &tl16);

      _mm_store_si128((__m128i *)dst, r0);
      _mm_store_si128((__m128i *)(dst + 16), r1);
      _mm_store_si128((__m128i *)(dst + 32), r2);
      _mm_store_si128((__m128i *)(dst + 48), r3);
      dst += stride;
      rep = _mm_add_epi16(rep, one);
    }
  }
}

void aom_paeth_predictor_64x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i a = _mm_load_si128((const __m128i *)above);
  const __m128i b = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i c = _mm_load_si128((const __m128i *)(above + 32));
  const __m128i d = _mm_load_si128((const __m128i *)(above + 48));
  const __m128i zero = _mm_setzero_si128();
  const __m128i al = _mm_unpacklo_epi8(a, zero);
  const __m128i ah = _mm_unpackhi_epi8(a, zero);
  const __m128i bl = _mm_unpacklo_epi8(b, zero);
  const __m128i bh = _mm_unpackhi_epi8(b, zero);
  const __m128i cl = _mm_unpacklo_epi8(c, zero);
  const __m128i ch = _mm_unpackhi_epi8(c, zero);
  const __m128i dl = _mm_unpacklo_epi8(d, zero);
  const __m128i dh = _mm_unpackhi_epi8(d, zero);

  const __m128i tl16 = _mm_set1_epi16((uint16_t)above[-1]);
  const __m128i one = _mm_set1_epi16(1);
  __m128i l16;

  int i;
  const __m128i l = _mm_load_si128((const __m128i *)left);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  for (i = 0; i < 16; ++i) {
    l16 = _mm_shuffle_epi8(l, rep);
    const __m128i r0 = paeth_16x1_pred(&l16, &al, &ah, &tl16);
    const __m128i r1 = paeth_16x1_pred(&l16, &bl, &bh, &tl16);
    const __m128i r2 = paeth_16x1_pred(&l16, &cl, &ch, &tl16);
    const __m128i r3 = paeth_16x1_pred(&l16, &dl, &dh, &tl16);

    _mm_store_si128((__m128i *)dst, r0);
    _mm_store_si128((__m128i *)(dst + 16), r1);
    _mm_store_si128((__m128i *)(dst + 32), r2);
    _mm_store_si128((__m128i *)(dst + 48), r3);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

// -----------------------------------------------------------------------------
// SMOOTH_PRED

// pixels[0]: above and below_pred interleave vector
// pixels[1]: left vector
// pixels[2]: right_pred vector
static INLINE void load_pixel_w4(const uint8_t *above, const uint8_t *left,
                                 int height, __m128i *pixels) {
  __m128i d = _mm_cvtsi32_si128(((const uint32_t *)above)[0]);
  if (height == 4)
    pixels[1] = _mm_cvtsi32_si128(((const uint32_t *)left)[0]);
  else if (height == 8)
    pixels[1] = _mm_loadl_epi64(((const __m128i *)left));
  else
    pixels[1] = _mm_loadu_si128(((const __m128i *)left));

  pixels[2] = _mm_set1_epi16((uint16_t)above[3]);

  const __m128i bp = _mm_set1_epi16((uint16_t)left[height - 1]);
  const __m128i zero = _mm_setzero_si128();
  d = _mm_unpacklo_epi8(d, zero);
  pixels[0] = _mm_unpacklo_epi16(d, bp);
}

// weight_h[0]: weight_h vector
// weight_h[1]: scale - weight_h vector
// weight_h[2]: same as [0], second half for height = 16 only
// weight_h[3]: same as [1], second half for height = 16 only
// weight_w[0]: weights_w and scale - weights_w interleave vector
static INLINE void load_weight_w4(int height, __m128i *weight_h,
                                  __m128i *weight_w) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i d = _mm_set1_epi16((uint16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));
  const __m128i t = _mm_cvtsi32_si128(((const uint32_t *)smooth_weights)[0]);
  weight_h[0] = _mm_unpacklo_epi8(t, zero);
  weight_h[1] = _mm_sub_epi16(d, weight_h[0]);
  weight_w[0] = _mm_unpacklo_epi16(weight_h[0], weight_h[1]);

  if (height == 8) {
    const __m128i weight = _mm_loadl_epi64((const __m128i *)&smooth_weights[4]);
    weight_h[0] = _mm_unpacklo_epi8(weight, zero);
    weight_h[1] = _mm_sub_epi16(d, weight_h[0]);
  } else if (height == 16) {
    const __m128i weight =
        _mm_loadu_si128((const __m128i *)&smooth_weights[12]);
    weight_h[0] = _mm_unpacklo_epi8(weight, zero);
    weight_h[1] = _mm_sub_epi16(d, weight_h[0]);
    weight_h[2] = _mm_unpackhi_epi8(weight, zero);
    weight_h[3] = _mm_sub_epi16(d, weight_h[2]);
  }
}

static INLINE void smooth_pred_4xh(const __m128i *pixel, const __m128i *wh,
                                   const __m128i *ww, int h, uint8_t *dst,
                                   ptrdiff_t stride, int second_half) {
  const __m128i round = _mm_set1_epi32((1 << SMOOTH_WEIGHT_LOG2_SCALE));
  const __m128i one = _mm_set1_epi16(1);
  const __m128i inc = _mm_set1_epi16(0x202);
  const __m128i gat = _mm_set1_epi32(0xc080400);
  __m128i rep = second_half ? _mm_set1_epi16((short)0x8008)
                            : _mm_set1_epi16((short)0x8000);
  __m128i d = _mm_set1_epi16(0x100);

  for (int i = 0; i < h; ++i) {
    const __m128i wg_wg = _mm_shuffle_epi8(wh[0], d);
    const __m128i sc_sc = _mm_shuffle_epi8(wh[1], d);
    const __m128i wh_sc = _mm_unpacklo_epi16(wg_wg, sc_sc);
    __m128i s = _mm_madd_epi16(pixel[0], wh_sc);

    __m128i b = _mm_shuffle_epi8(pixel[1], rep);
    b = _mm_unpacklo_epi16(b, pixel[2]);
    __m128i sum = _mm_madd_epi16(b, ww[0]);

    sum = _mm_add_epi32(s, sum);
    sum = _mm_add_epi32(sum, round);
    sum = _mm_srai_epi32(sum, 1 + SMOOTH_WEIGHT_LOG2_SCALE);

    sum = _mm_shuffle_epi8(sum, gat);
    *(int *)dst = _mm_cvtsi128_si32(sum);
    dst += stride;

    rep = _mm_add_epi16(rep, one);
    d = _mm_add_epi16(d, inc);
  }
}

void aom_smooth_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i pixels[3];
  load_pixel_w4(above, left, 4, pixels);

  __m128i wh[4], ww[2];
  load_weight_w4(4, wh, ww);

  smooth_pred_4xh(pixels, wh, ww, 4, dst, stride, 0);
}

void aom_smooth_predictor_4x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i pixels[3];
  load_pixel_w4(above, left, 8, pixels);

  __m128i wh[4], ww[2];
  load_weight_w4(8, wh, ww);

  smooth_pred_4xh(pixels, wh, ww, 8, dst, stride, 0);
}

void aom_smooth_predictor_4x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m128i pixels[3];
  load_pixel_w4(above, left, 16, pixels);

  __m128i wh[4], ww[2];
  load_weight_w4(16, wh, ww);

  smooth_pred_4xh(pixels, wh, ww, 8, dst, stride, 0);
  dst += stride << 3;
  smooth_pred_4xh(pixels, &wh[2], ww, 8, dst, stride, 1);
}

// pixels[0]: above and below_pred interleave vector, first half
// pixels[1]: above and below_pred interleave vector, second half
// pixels[2]: left vector
// pixels[3]: right_pred vector
// pixels[4]: above and below_pred interleave vector, first half
// pixels[5]: above and below_pred interleave vector, second half
// pixels[6]: left vector + 16
// pixels[7]: right_pred vector
static INLINE void load_pixel_w8(const uint8_t *above, const uint8_t *left,
                                 int height, __m128i *pixels) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i bp = _mm_set1_epi16((uint16_t)left[height - 1]);
  __m128i d = _mm_loadl_epi64((const __m128i *)above);
  d = _mm_unpacklo_epi8(d, zero);
  pixels[0] = _mm_unpacklo_epi16(d, bp);
  pixels[1] = _mm_unpackhi_epi16(d, bp);

  pixels[3] = _mm_set1_epi16((uint16_t)above[7]);

  if (height == 4) {
    pixels[2] = _mm_cvtsi32_si128(((const uint32_t *)left)[0]);
  } else if (height == 8) {
    pixels[2] = _mm_loadl_epi64((const __m128i *)left);
  } else if (height == 16) {
    pixels[2] = _mm_load_si128((const __m128i *)left);
  } else {
    pixels[2] = _mm_load_si128((const __m128i *)left);
    pixels[4] = pixels[0];
    pixels[5] = pixels[1];
    pixels[6] = _mm_load_si128((const __m128i *)(left + 16));
    pixels[7] = pixels[3];
  }
}

// weight_h[0]: weight_h vector
// weight_h[1]: scale - weight_h vector
// weight_h[2]: same as [0], offset 8
// weight_h[3]: same as [1], offset 8
// weight_h[4]: same as [0], offset 16
// weight_h[5]: same as [1], offset 16
// weight_h[6]: same as [0], offset 24
// weight_h[7]: same as [1], offset 24
// weight_w[0]: weights_w and scale - weights_w interleave vector, first half
// weight_w[1]: weights_w and scale - weights_w interleave vector, second half
static INLINE void load_weight_w8(int height, __m128i *weight_h,
                                  __m128i *weight_w) {
  const __m128i zero = _mm_setzero_si128();
  const int we_offset = height < 8 ? 0 : 4;
  __m128i we = _mm_loadu_si128((const __m128i *)&smooth_weights[we_offset]);
  weight_h[0] = _mm_unpacklo_epi8(we, zero);
  const __m128i d = _mm_set1_epi16((uint16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));
  weight_h[1] = _mm_sub_epi16(d, weight_h[0]);

  if (height == 4) {
    we = _mm_srli_si128(we, 4);
    __m128i tmp1 = _mm_unpacklo_epi8(we, zero);
    __m128i tmp2 = _mm_sub_epi16(d, tmp1);
    weight_w[0] = _mm_unpacklo_epi16(tmp1, tmp2);
    weight_w[1] = _mm_unpackhi_epi16(tmp1, tmp2);
  } else {
    weight_w[0] = _mm_unpacklo_epi16(weight_h[0], weight_h[1]);
    weight_w[1] = _mm_unpackhi_epi16(weight_h[0], weight_h[1]);
  }

  if (height == 16) {
    we = _mm_loadu_si128((const __m128i *)&smooth_weights[12]);
    weight_h[0] = _mm_unpacklo_epi8(we, zero);
    weight_h[1] = _mm_sub_epi16(d, weight_h[0]);
    weight_h[2] = _mm_unpackhi_epi8(we, zero);
    weight_h[3] = _mm_sub_epi16(d, weight_h[2]);
  } else if (height == 32) {
    const __m128i weight_lo =
        _mm_loadu_si128((const __m128i *)&smooth_weights[28]);
    weight_h[0] = _mm_unpacklo_epi8(weight_lo, zero);
    weight_h[1] = _mm_sub_epi16(d, weight_h[0]);
    weight_h[2] = _mm_unpackhi_epi8(weight_lo, zero);
    weight_h[3] = _mm_sub_epi16(d, weight_h[2]);
    const __m128i weight_hi =
        _mm_loadu_si128((const __m128i *)&smooth_weights[28 + 16]);
    weight_h[4] = _mm_unpacklo_epi8(weight_hi, zero);
    weight_h[5] = _mm_sub_epi16(d, weight_h[4]);
    weight_h[6] = _mm_unpackhi_epi8(weight_hi, zero);
    weight_h[7] = _mm_sub_epi16(d, weight_h[6]);
  }
}

static INLINE void smooth_pred_8xh(const __m128i *pixels, const __m128i *wh,
                                   const __m128i *ww, int h, uint8_t *dst,
                                   ptrdiff_t stride, int second_half) {
  const __m128i round = _mm_set1_epi32((1 << SMOOTH_WEIGHT_LOG2_SCALE));
  const __m128i one = _mm_set1_epi16(1);
  const __m128i inc = _mm_set1_epi16(0x202);
  const __m128i gat = _mm_set_epi32(0, 0, 0xe0c0a08, 0x6040200);

  __m128i rep = second_half ? _mm_set1_epi16((short)0x8008)
                            : _mm_set1_epi16((short)0x8000);
  __m128i d = _mm_set1_epi16(0x100);

  int i;
  for (i = 0; i < h; ++i) {
    const __m128i wg_wg = _mm_shuffle_epi8(wh[0], d);
    const __m128i sc_sc = _mm_shuffle_epi8(wh[1], d);
    const __m128i wh_sc = _mm_unpacklo_epi16(wg_wg, sc_sc);
    __m128i s0 = _mm_madd_epi16(pixels[0], wh_sc);
    __m128i s1 = _mm_madd_epi16(pixels[1], wh_sc);

    __m128i b = _mm_shuffle_epi8(pixels[2], rep);
    b = _mm_unpacklo_epi16(b, pixels[3]);
    __m128i sum0 = _mm_madd_epi16(b, ww[0]);
    __m128i sum1 = _mm_madd_epi16(b, ww[1]);

    s0 = _mm_add_epi32(s0, sum0);
    s0 = _mm_add_epi32(s0, round);
    s0 = _mm_srai_epi32(s0, 1 + SMOOTH_WEIGHT_LOG2_SCALE);

    s1 = _mm_add_epi32(s1, sum1);
    s1 = _mm_add_epi32(s1, round);
    s1 = _mm_srai_epi32(s1, 1 + SMOOTH_WEIGHT_LOG2_SCALE);

    sum0 = _mm_packus_epi16(s0, s1);
    sum0 = _mm_shuffle_epi8(sum0, gat);
    _mm_storel_epi64((__m128i *)dst, sum0);
    dst += stride;

    rep = _mm_add_epi16(rep, one);
    d = _mm_add_epi16(d, inc);
  }
}

void aom_smooth_predictor_8x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i pixels[4];
  load_pixel_w8(above, left, 4, pixels);

  __m128i wh[4], ww[2];
  load_weight_w8(4, wh, ww);

  smooth_pred_8xh(pixels, wh, ww, 4, dst, stride, 0);
}

void aom_smooth_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i pixels[4];
  load_pixel_w8(above, left, 8, pixels);

  __m128i wh[4], ww[2];
  load_weight_w8(8, wh, ww);

  smooth_pred_8xh(pixels, wh, ww, 8, dst, stride, 0);
}

void aom_smooth_predictor_8x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m128i pixels[4];
  load_pixel_w8(above, left, 16, pixels);

  __m128i wh[4], ww[2];
  load_weight_w8(16, wh, ww);

  smooth_pred_8xh(pixels, wh, ww, 8, dst, stride, 0);
  dst += stride << 3;
  smooth_pred_8xh(pixels, &wh[2], ww, 8, dst, stride, 1);
}

void aom_smooth_predictor_8x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m128i pixels[8];
  load_pixel_w8(above, left, 32, pixels);

  __m128i wh[8], ww[2];
  load_weight_w8(32, wh, ww);

  smooth_pred_8xh(&pixels[0], wh, ww, 8, dst, stride, 0);
  dst += stride << 3;
  smooth_pred_8xh(&pixels[0], &wh[2], ww, 8, dst, stride, 1);
  dst += stride << 3;
  smooth_pred_8xh(&pixels[4], &wh[4], ww, 8, dst, stride, 0);
  dst += stride << 3;
  smooth_pred_8xh(&pixels[4], &wh[6], ww, 8, dst, stride, 1);
}

static INLINE void smooth_predictor_wxh(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left, uint32_t bw,
                                        uint32_t bh) {
  const uint8_t *const sm_weights_w = smooth_weights + bw - 4;
  const uint8_t *const sm_weights_h = smooth_weights + bh - 4;
  const __m128i zero = _mm_setzero_si128();
  const __m128i scale_value =
      _mm_set1_epi16((uint16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));
  const __m128i bottom_left = _mm_cvtsi32_si128((uint32_t)left[bh - 1]);
  const __m128i dup16 = _mm_set1_epi32(0x01000100);
  const __m128i top_right =
      _mm_shuffle_epi8(_mm_cvtsi32_si128((uint32_t)above[bw - 1]), dup16);
  const __m128i gat = _mm_set_epi32(0, 0, 0xe0c0a08, 0x6040200);
  const __m128i round =
      _mm_set1_epi32((uint16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));

  for (uint32_t y = 0; y < bh; ++y) {
    const __m128i weights_y = _mm_cvtsi32_si128((uint32_t)sm_weights_h[y]);
    const __m128i left_y = _mm_cvtsi32_si128((uint32_t)left[y]);
    const __m128i scale_m_weights_y = _mm_sub_epi16(scale_value, weights_y);
    __m128i pred_scaled_bl = _mm_mullo_epi16(scale_m_weights_y, bottom_left);
    const __m128i wl_y =
        _mm_shuffle_epi32(_mm_unpacklo_epi16(weights_y, left_y), 0);
    pred_scaled_bl = _mm_add_epi32(pred_scaled_bl, round);
    pred_scaled_bl = _mm_shuffle_epi32(pred_scaled_bl, 0);

    for (uint32_t x = 0; x < bw; x += 8) {
      const __m128i top_x = _mm_loadl_epi64((const __m128i *)(above + x));
      const __m128i weights_x =
          _mm_loadl_epi64((const __m128i *)(sm_weights_w + x));
      const __m128i tw_x = _mm_unpacklo_epi8(top_x, weights_x);
      const __m128i tw_x_lo = _mm_unpacklo_epi8(tw_x, zero);
      const __m128i tw_x_hi = _mm_unpackhi_epi8(tw_x, zero);

      __m128i pred_lo = _mm_madd_epi16(tw_x_lo, wl_y);
      __m128i pred_hi = _mm_madd_epi16(tw_x_hi, wl_y);

      const __m128i scale_m_weights_x =
          _mm_sub_epi16(scale_value, _mm_unpacklo_epi8(weights_x, zero));
      const __m128i swxtr = _mm_mullo_epi16(scale_m_weights_x, top_right);
      const __m128i swxtr_lo = _mm_unpacklo_epi16(swxtr, zero);
      const __m128i swxtr_hi = _mm_unpackhi_epi16(swxtr, zero);

      pred_lo = _mm_add_epi32(pred_lo, pred_scaled_bl);
      pred_hi = _mm_add_epi32(pred_hi, pred_scaled_bl);

      pred_lo = _mm_add_epi32(pred_lo, swxtr_lo);
      pred_hi = _mm_add_epi32(pred_hi, swxtr_hi);

      pred_lo = _mm_srai_epi32(pred_lo, (1 + SMOOTH_WEIGHT_LOG2_SCALE));
      pred_hi = _mm_srai_epi32(pred_hi, (1 + SMOOTH_WEIGHT_LOG2_SCALE));

      __m128i pred = _mm_packus_epi16(pred_lo, pred_hi);
      pred = _mm_shuffle_epi8(pred, gat);
      _mm_storel_epi64((__m128i *)(dst + x), pred);
    }
    dst += stride;
  }
}

void aom_smooth_predictor_16x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  smooth_predictor_wxh(dst, stride, above, left, 16, 4);
}

void aom_smooth_predictor_16x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  smooth_predictor_wxh(dst, stride, above, left, 16, 8);
}

void aom_smooth_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_wxh(dst, stride, above, left, 16, 16);
}

void aom_smooth_predictor_16x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_wxh(dst, stride, above, left, 16, 32);
}

void aom_smooth_predictor_32x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  smooth_predictor_wxh(dst, stride, above, left, 32, 8);
}

void aom_smooth_predictor_32x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_wxh(dst, stride, above, left, 32, 16);
}

void aom_smooth_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_wxh(dst, stride, above, left, 32, 32);
}

void aom_smooth_predictor_32x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_wxh(dst, stride, above, left, 32, 64);
}

void aom_smooth_predictor_64x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_wxh(dst, stride, above, left, 64, 64);
}

void aom_smooth_predictor_64x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_wxh(dst, stride, above, left, 64, 32);
}

void aom_smooth_predictor_64x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_wxh(dst, stride, above, left, 64, 16);
}

void aom_smooth_predictor_16x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_wxh(dst, stride, above, left, 16, 64);
}

// -----------------------------------------------------------------------------
// SMOOTH_V_PRED

// pixels[0]: above and below_pred interleave vector
static INLINE void load_pixel_v_w4(const uint8_t *above, const uint8_t *left,
                                   int height, __m128i *pixels) {
  const __m128i zero = _mm_setzero_si128();
  __m128i d = _mm_cvtsi32_si128(((const uint32_t *)above)[0]);
  const __m128i bp = _mm_set1_epi16((uint16_t)left[height - 1]);
  d = _mm_unpacklo_epi8(d, zero);
  pixels[0] = _mm_unpacklo_epi16(d, bp);
}

// weights[0]: weights_h vector
// weights[1]: scale - weights_h vector
static INLINE void load_weight_v_w4(int height, __m128i *weights) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i d = _mm_set1_epi16((uint16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));

  if (height == 4) {
    const __m128i weight =
        _mm_cvtsi32_si128(((const uint32_t *)smooth_weights)[0]);
    weights[0] = _mm_unpacklo_epi8(weight, zero);
    weights[1] = _mm_sub_epi16(d, weights[0]);
  } else if (height == 8) {
    const __m128i weight = _mm_loadl_epi64((const __m128i *)&smooth_weights[4]);
    weights[0] = _mm_unpacklo_epi8(weight, zero);
    weights[1] = _mm_sub_epi16(d, weights[0]);
  } else {
    const __m128i weight =
        _mm_loadu_si128((const __m128i *)&smooth_weights[12]);
    weights[0] = _mm_unpacklo_epi8(weight, zero);
    weights[1] = _mm_sub_epi16(d, weights[0]);
    weights[2] = _mm_unpackhi_epi8(weight, zero);
    weights[3] = _mm_sub_epi16(d, weights[2]);
  }
}

static INLINE void smooth_v_pred_4xh(const __m128i *pixel,
                                     const __m128i *weight, int h, uint8_t *dst,
                                     ptrdiff_t stride) {
  const __m128i pred_round =
      _mm_set1_epi32((1 << (SMOOTH_WEIGHT_LOG2_SCALE - 1)));
  const __m128i inc = _mm_set1_epi16(0x202);
  const __m128i gat = _mm_set1_epi32(0xc080400);
  __m128i d = _mm_set1_epi16(0x100);

  for (int i = 0; i < h; ++i) {
    const __m128i wg_wg = _mm_shuffle_epi8(weight[0], d);
    const __m128i sc_sc = _mm_shuffle_epi8(weight[1], d);
    const __m128i wh_sc = _mm_unpacklo_epi16(wg_wg, sc_sc);
    __m128i sum = _mm_madd_epi16(pixel[0], wh_sc);
    sum = _mm_add_epi32(sum, pred_round);
    sum = _mm_srai_epi32(sum, SMOOTH_WEIGHT_LOG2_SCALE);
    sum = _mm_shuffle_epi8(sum, gat);
    *(int *)dst = _mm_cvtsi128_si32(sum);
    dst += stride;
    d = _mm_add_epi16(d, inc);
  }
}

void aom_smooth_v_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i pixels;
  load_pixel_v_w4(above, left, 4, &pixels);

  __m128i weights[2];
  load_weight_v_w4(4, weights);

  smooth_v_pred_4xh(&pixels, weights, 4, dst, stride);
}

void aom_smooth_v_predictor_4x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i pixels;
  load_pixel_v_w4(above, left, 8, &pixels);

  __m128i weights[2];
  load_weight_v_w4(8, weights);

  smooth_v_pred_4xh(&pixels, weights, 8, dst, stride);
}

void aom_smooth_v_predictor_4x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above,
                                       const uint8_t *left) {
  __m128i pixels;
  load_pixel_v_w4(above, left, 16, &pixels);

  __m128i weights[4];
  load_weight_v_w4(16, weights);

  smooth_v_pred_4xh(&pixels, weights, 8, dst, stride);
  dst += stride << 3;
  smooth_v_pred_4xh(&pixels, &weights[2], 8, dst, stride);
}

// pixels[0]: above and below_pred interleave vector, first half
// pixels[1]: above and below_pred interleave vector, second half
static INLINE void load_pixel_v_w8(const uint8_t *above, const uint8_t *left,
                                   int height, __m128i *pixels) {
  const __m128i zero = _mm_setzero_si128();
  __m128i d = _mm_loadl_epi64((const __m128i *)above);
  const __m128i bp = _mm_set1_epi16((uint16_t)left[height - 1]);
  d = _mm_unpacklo_epi8(d, zero);
  pixels[0] = _mm_unpacklo_epi16(d, bp);
  pixels[1] = _mm_unpackhi_epi16(d, bp);
}

// weight_h[0]: weight_h vector
// weight_h[1]: scale - weight_h vector
// weight_h[2]: same as [0], offset 8
// weight_h[3]: same as [1], offset 8
// weight_h[4]: same as [0], offset 16
// weight_h[5]: same as [1], offset 16
// weight_h[6]: same as [0], offset 24
// weight_h[7]: same as [1], offset 24
static INLINE void load_weight_v_w8(int height, __m128i *weight_h) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i d = _mm_set1_epi16((uint16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));

  if (height < 16) {
    const int offset = height < 8 ? 0 : 4;
    const __m128i weight =
        _mm_loadu_si128((const __m128i *)&smooth_weights[offset]);
    weight_h[0] = _mm_unpacklo_epi8(weight, zero);
    weight_h[1] = _mm_sub_epi16(d, weight_h[0]);
  } else if (height == 16) {
    const __m128i weight =
        _mm_loadu_si128((const __m128i *)&smooth_weights[12]);
    weight_h[0] = _mm_unpacklo_epi8(weight, zero);
    weight_h[1] = _mm_sub_epi16(d, weight_h[0]);
    weight_h[2] = _mm_unpackhi_epi8(weight, zero);
    weight_h[3] = _mm_sub_epi16(d, weight_h[2]);
  } else {
    const __m128i weight_lo =
        _mm_loadu_si128((const __m128i *)&smooth_weights[28]);
    weight_h[0] = _mm_unpacklo_epi8(weight_lo, zero);
    weight_h[1] = _mm_sub_epi16(d, weight_h[0]);
    weight_h[2] = _mm_unpackhi_epi8(weight_lo, zero);
    weight_h[3] = _mm_sub_epi16(d, weight_h[2]);
    const __m128i weight_hi =
        _mm_loadu_si128((const __m128i *)&smooth_weights[28 + 16]);
    weight_h[4] = _mm_unpacklo_epi8(weight_hi, zero);
    weight_h[5] = _mm_sub_epi16(d, weight_h[4]);
    weight_h[6] = _mm_unpackhi_epi8(weight_hi, zero);
    weight_h[7] = _mm_sub_epi16(d, weight_h[6]);
  }
}

static INLINE void smooth_v_pred_8xh(const __m128i *pixels, const __m128i *wh,
                                     int h, uint8_t *dst, ptrdiff_t stride) {
  const __m128i pred_round =
      _mm_set1_epi32((1 << (SMOOTH_WEIGHT_LOG2_SCALE - 1)));
  const __m128i inc = _mm_set1_epi16(0x202);
  const __m128i gat = _mm_set_epi32(0, 0, 0xe0c0a08, 0x6040200);
  __m128i d = _mm_set1_epi16(0x100);

  for (int i = 0; i < h; ++i) {
    const __m128i wg_wg = _mm_shuffle_epi8(wh[0], d);
    const __m128i sc_sc = _mm_shuffle_epi8(wh[1], d);
    const __m128i wh_sc = _mm_unpacklo_epi16(wg_wg, sc_sc);
    __m128i s0 = _mm_madd_epi16(pixels[0], wh_sc);
    __m128i s1 = _mm_madd_epi16(pixels[1], wh_sc);

    s0 = _mm_add_epi32(s0, pred_round);
    s0 = _mm_srai_epi32(s0, SMOOTH_WEIGHT_LOG2_SCALE);

    s1 = _mm_add_epi32(s1, pred_round);
    s1 = _mm_srai_epi32(s1, SMOOTH_WEIGHT_LOG2_SCALE);

    __m128i sum01 = _mm_packus_epi16(s0, s1);
    sum01 = _mm_shuffle_epi8(sum01, gat);
    _mm_storel_epi64((__m128i *)dst, sum01);
    dst += stride;

    d = _mm_add_epi16(d, inc);
  }
}

void aom_smooth_v_predictor_8x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i pixels[2];
  load_pixel_v_w8(above, left, 4, pixels);

  __m128i wh[2];
  load_weight_v_w8(4, wh);

  smooth_v_pred_8xh(pixels, wh, 4, dst, stride);
}

void aom_smooth_v_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i pixels[2];
  load_pixel_v_w8(above, left, 8, pixels);

  __m128i wh[2];
  load_weight_v_w8(8, wh);

  smooth_v_pred_8xh(pixels, wh, 8, dst, stride);
}

void aom_smooth_v_predictor_8x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above,
                                       const uint8_t *left) {
  __m128i pixels[2];
  load_pixel_v_w8(above, left, 16, pixels);

  __m128i wh[4];
  load_weight_v_w8(16, wh);

  smooth_v_pred_8xh(pixels, wh, 8, dst, stride);
  dst += stride << 3;
  smooth_v_pred_8xh(pixels, &wh[2], 8, dst, stride);
}

void aom_smooth_v_predictor_8x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above,
                                       const uint8_t *left) {
  __m128i pixels[2];
  load_pixel_v_w8(above, left, 32, pixels);

  __m128i wh[8];
  load_weight_v_w8(32, wh);

  smooth_v_pred_8xh(pixels, &wh[0], 8, dst, stride);
  dst += stride << 3;
  smooth_v_pred_8xh(pixels, &wh[2], 8, dst, stride);
  dst += stride << 3;
  smooth_v_pred_8xh(pixels, &wh[4], 8, dst, stride);
  dst += stride << 3;
  smooth_v_pred_8xh(pixels, &wh[6], 8, dst, stride);
}

static INLINE void smooth_v_predictor_wxh(uint8_t *dst, ptrdiff_t stride,
                                          const uint8_t *above,
                                          const uint8_t *left, uint32_t bw,
                                          uint32_t bh) {
  const uint8_t *const sm_weights_h = smooth_weights + bh - 4;
  const __m128i zero = _mm_setzero_si128();
  const __m128i scale_value =
      _mm_set1_epi16((uint16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));
  const __m128i dup16 = _mm_set1_epi32(0x01000100);
  const __m128i bottom_left =
      _mm_shuffle_epi8(_mm_cvtsi32_si128((uint32_t)left[bh - 1]), dup16);
  const __m128i gat = _mm_set_epi32(0, 0, 0xe0c0a08, 0x6040200);
  const __m128i round =
      _mm_set1_epi32((uint16_t)(1 << (SMOOTH_WEIGHT_LOG2_SCALE - 1)));

  for (uint32_t y = 0; y < bh; ++y) {
    const __m128i weights_y = _mm_cvtsi32_si128((uint32_t)sm_weights_h[y]);
    const __m128i scale_m_weights_y =
        _mm_shuffle_epi8(_mm_sub_epi16(scale_value, weights_y), dup16);
    const __m128i wl_y =
        _mm_shuffle_epi32(_mm_unpacklo_epi16(weights_y, bottom_left), 0);

    for (uint32_t x = 0; x < bw; x += 8) {
      const __m128i top_x = _mm_loadl_epi64((const __m128i *)(above + x));
      // 8 -> 16
      const __m128i tw_x = _mm_unpacklo_epi8(top_x, zero);
      const __m128i tw_x_lo = _mm_unpacklo_epi16(tw_x, scale_m_weights_y);
      const __m128i tw_x_hi = _mm_unpackhi_epi16(tw_x, scale_m_weights_y);
      // top_x * weights_y + scale_m_weights_y * bottom_left
      __m128i pred_lo = _mm_madd_epi16(tw_x_lo, wl_y);
      __m128i pred_hi = _mm_madd_epi16(tw_x_hi, wl_y);

      pred_lo = _mm_add_epi32(pred_lo, round);
      pred_hi = _mm_add_epi32(pred_hi, round);
      pred_lo = _mm_srai_epi32(pred_lo, SMOOTH_WEIGHT_LOG2_SCALE);
      pred_hi = _mm_srai_epi32(pred_hi, SMOOTH_WEIGHT_LOG2_SCALE);

      __m128i pred = _mm_packus_epi16(pred_lo, pred_hi);
      pred = _mm_shuffle_epi8(pred, gat);
      _mm_storel_epi64((__m128i *)(dst + x), pred);
    }
    dst += stride;
  }
}

void aom_smooth_v_predictor_16x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above,
                                       const uint8_t *left) {
  smooth_v_predictor_wxh(dst, stride, above, left, 16, 4);
}

void aom_smooth_v_predictor_16x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above,
                                       const uint8_t *left) {
  smooth_v_predictor_wxh(dst, stride, above, left, 16, 8);
}

void aom_smooth_v_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_v_predictor_wxh(dst, stride, above, left, 16, 16);
}

void aom_smooth_v_predictor_16x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_v_predictor_wxh(dst, stride, above, left, 16, 32);
}

void aom_smooth_v_predictor_32x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above,
                                       const uint8_t *left) {
  smooth_v_predictor_wxh(dst, stride, above, left, 32, 8);
}

void aom_smooth_v_predictor_32x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_v_predictor_wxh(dst, stride, above, left, 32, 16);
}

void aom_smooth_v_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_v_predictor_wxh(dst, stride, above, left, 32, 32);
}

void aom_smooth_v_predictor_32x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_v_predictor_wxh(dst, stride, above, left, 32, 64);
}

void aom_smooth_v_predictor_64x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_v_predictor_wxh(dst, stride, above, left, 64, 64);
}

void aom_smooth_v_predictor_64x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_v_predictor_wxh(dst, stride, above, left, 64, 32);
}

void aom_smooth_v_predictor_64x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_v_predictor_wxh(dst, stride, above, left, 64, 16);
}

void aom_smooth_v_predictor_16x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_v_predictor_wxh(dst, stride, above, left, 16, 64);
}

// -----------------------------------------------------------------------------
// SMOOTH_H_PRED

// pixels[0]: left vector
// pixels[1]: right_pred vector
static INLINE void load_pixel_h_w4(const uint8_t *above, const uint8_t *left,
                                   int height, __m128i *pixels) {
  if (height == 4)
    pixels[0] = _mm_cvtsi32_si128(((const uint32_t *)left)[0]);
  else if (height == 8)
    pixels[0] = _mm_loadl_epi64(((const __m128i *)left));
  else
    pixels[0] = _mm_loadu_si128(((const __m128i *)left));
  pixels[1] = _mm_set1_epi16((uint16_t)above[3]);
}

// weights[0]: weights_w and scale - weights_w interleave vector
static INLINE void load_weight_h_w4(int height, __m128i *weights) {
  (void)height;
  const __m128i t = _mm_loadu_si128((const __m128i *)&smooth_weights[0]);
  const __m128i zero = _mm_setzero_si128();

  const __m128i weights_0 = _mm_unpacklo_epi8(t, zero);
  const __m128i d = _mm_set1_epi16((uint16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));
  const __m128i weights_1 = _mm_sub_epi16(d, weights_0);
  weights[0] = _mm_unpacklo_epi16(weights_0, weights_1);
}

static INLINE void smooth_h_pred_4xh(const __m128i *pixel,
                                     const __m128i *weight, int h, uint8_t *dst,
                                     ptrdiff_t stride) {
  const __m128i pred_round =
      _mm_set1_epi32((1 << (SMOOTH_WEIGHT_LOG2_SCALE - 1)));
  const __m128i one = _mm_set1_epi16(1);
  const __m128i gat = _mm_set1_epi32(0xc080400);
  __m128i rep = _mm_set1_epi16((short)0x8000);

  for (int i = 0; i < h; ++i) {
    __m128i b = _mm_shuffle_epi8(pixel[0], rep);
    b = _mm_unpacklo_epi16(b, pixel[1]);
    __m128i sum = _mm_madd_epi16(b, weight[0]);

    sum = _mm_add_epi32(sum, pred_round);
    sum = _mm_srai_epi32(sum, SMOOTH_WEIGHT_LOG2_SCALE);

    sum = _mm_shuffle_epi8(sum, gat);
    *(int *)dst = _mm_cvtsi128_si32(sum);
    dst += stride;

    rep = _mm_add_epi16(rep, one);
  }
}

void aom_smooth_h_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i pixels[2];
  load_pixel_h_w4(above, left, 4, pixels);

  __m128i weights;
  load_weight_h_w4(4, &weights);

  smooth_h_pred_4xh(pixels, &weights, 4, dst, stride);
}

void aom_smooth_h_predictor_4x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i pixels[2];
  load_pixel_h_w4(above, left, 8, pixels);

  __m128i weights;
  load_weight_h_w4(8, &weights);

  smooth_h_pred_4xh(pixels, &weights, 8, dst, stride);
}

void aom_smooth_h_predictor_4x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above,
                                       const uint8_t *left) {
  __m128i pixels[2];
  load_pixel_h_w4(above, left, 16, pixels);

  __m128i weights;
  load_weight_h_w4(8, &weights);

  smooth_h_pred_4xh(pixels, &weights, 8, dst, stride);
  dst += stride << 3;

  pixels[0] = _mm_srli_si128(pixels[0], 8);
  smooth_h_pred_4xh(pixels, &weights, 8, dst, stride);
}

// pixels[0]: left vector
// pixels[1]: right_pred vector
// pixels[2]: left vector + 16
// pixels[3]: right_pred vector
static INLINE void load_pixel_h_w8(const uint8_t *above, const uint8_t *left,
                                   int height, __m128i *pixels) {
  pixels[1] = _mm_set1_epi16((uint16_t)above[7]);

  if (height == 4) {
    pixels[0] = _mm_cvtsi32_si128(((const uint32_t *)left)[0]);
  } else if (height == 8) {
    pixels[0] = _mm_loadl_epi64((const __m128i *)left);
  } else if (height == 16) {
    pixels[0] = _mm_load_si128((const __m128i *)left);
  } else {
    pixels[0] = _mm_load_si128((const __m128i *)left);
    pixels[2] = _mm_load_si128((const __m128i *)(left + 16));
    pixels[3] = pixels[1];
  }
}

// weight_w[0]: weights_w and scale - weights_w interleave vector, first half
// weight_w[1]: weights_w and scale - weights_w interleave vector, second half
static INLINE void load_weight_h_w8(int height, __m128i *weight_w) {
  (void)height;
  const __m128i zero = _mm_setzero_si128();
  const __m128i d = _mm_set1_epi16((uint16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));
  const __m128i we = _mm_loadu_si128((const __m128i *)&smooth_weights[4]);
  const __m128i tmp1 = _mm_unpacklo_epi8(we, zero);
  const __m128i tmp2 = _mm_sub_epi16(d, tmp1);
  weight_w[0] = _mm_unpacklo_epi16(tmp1, tmp2);
  weight_w[1] = _mm_unpackhi_epi16(tmp1, tmp2);
}

static INLINE void smooth_h_pred_8xh(const __m128i *pixels, const __m128i *ww,
                                     int h, uint8_t *dst, ptrdiff_t stride,
                                     int second_half) {
  const __m128i pred_round =
      _mm_set1_epi32((1 << (SMOOTH_WEIGHT_LOG2_SCALE - 1)));
  const __m128i one = _mm_set1_epi16(1);
  const __m128i gat = _mm_set_epi32(0, 0, 0xe0c0a08, 0x6040200);
  __m128i rep = second_half ? _mm_set1_epi16((short)0x8008)
                            : _mm_set1_epi16((short)0x8000);

  for (int i = 0; i < h; ++i) {
    __m128i b = _mm_shuffle_epi8(pixels[0], rep);
    b = _mm_unpacklo_epi16(b, pixels[1]);
    __m128i sum0 = _mm_madd_epi16(b, ww[0]);
    __m128i sum1 = _mm_madd_epi16(b, ww[1]);

    sum0 = _mm_add_epi32(sum0, pred_round);
    sum0 = _mm_srai_epi32(sum0, SMOOTH_WEIGHT_LOG2_SCALE);

    sum1 = _mm_add_epi32(sum1, pred_round);
    sum1 = _mm_srai_epi32(sum1, SMOOTH_WEIGHT_LOG2_SCALE);

    sum0 = _mm_packus_epi16(sum0, sum1);
    sum0 = _mm_shuffle_epi8(sum0, gat);
    _mm_storel_epi64((__m128i *)dst, sum0);
    dst += stride;

    rep = _mm_add_epi16(rep, one);
  }
}

void aom_smooth_h_predictor_8x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i pixels[2];
  load_pixel_h_w8(above, left, 4, pixels);

  __m128i ww[2];
  load_weight_h_w8(4, ww);

  smooth_h_pred_8xh(pixels, ww, 4, dst, stride, 0);
}

void aom_smooth_h_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i pixels[2];
  load_pixel_h_w8(above, left, 8, pixels);

  __m128i ww[2];
  load_weight_h_w8(8, ww);

  smooth_h_pred_8xh(pixels, ww, 8, dst, stride, 0);
}

void aom_smooth_h_predictor_8x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above,
                                       const uint8_t *left) {
  __m128i pixels[2];
  load_pixel_h_w8(above, left, 16, pixels);

  __m128i ww[2];
  load_weight_h_w8(16, ww);

  smooth_h_pred_8xh(pixels, ww, 8, dst, stride, 0);
  dst += stride << 3;
  smooth_h_pred_8xh(pixels, ww, 8, dst, stride, 1);
}

void aom_smooth_h_predictor_8x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above,
                                       const uint8_t *left) {
  __m128i pixels[4];
  load_pixel_h_w8(above, left, 32, pixels);

  __m128i ww[2];
  load_weight_h_w8(32, ww);

  smooth_h_pred_8xh(&pixels[0], ww, 8, dst, stride, 0);
  dst += stride << 3;
  smooth_h_pred_8xh(&pixels[0], ww, 8, dst, stride, 1);
  dst += stride << 3;
  smooth_h_pred_8xh(&pixels[2], ww, 8, dst, stride, 0);
  dst += stride << 3;
  smooth_h_pred_8xh(&pixels[2], ww, 8, dst, stride, 1);
}

static INLINE void smooth_h_predictor_wxh(uint8_t *dst, ptrdiff_t stride,
                                          const uint8_t *above,
                                          const uint8_t *left, uint32_t bw,
                                          uint32_t bh) {
  const uint8_t *const sm_weights_w = smooth_weights + bw - 4;
  const __m128i zero = _mm_setzero_si128();
  const __m128i scale_value =
      _mm_set1_epi16((uint16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));
  const __m128i top_right = _mm_cvtsi32_si128((uint32_t)above[bw - 1]);
  const __m128i gat = _mm_set_epi32(0, 0, 0xe0c0a08, 0x6040200);
  const __m128i pred_round =
      _mm_set1_epi32((1 << (SMOOTH_WEIGHT_LOG2_SCALE - 1)));

  for (uint32_t y = 0; y < bh; ++y) {
    const __m128i left_y = _mm_cvtsi32_si128((uint32_t)left[y]);
    const __m128i tr_ly =
        _mm_shuffle_epi32(_mm_unpacklo_epi16(top_right, left_y), 0);

    for (uint32_t x = 0; x < bw; x += 8) {
      const __m128i weights_x =
          _mm_loadl_epi64((const __m128i *)(sm_weights_w + x));
      const __m128i weights_xw = _mm_unpacklo_epi8(weights_x, zero);
      const __m128i scale_m_weights_x = _mm_sub_epi16(scale_value, weights_xw);
      const __m128i wx_lo = _mm_unpacklo_epi16(scale_m_weights_x, weights_xw);
      const __m128i wx_hi = _mm_unpackhi_epi16(scale_m_weights_x, weights_xw);
      __m128i pred_lo = _mm_madd_epi16(wx_lo, tr_ly);
      __m128i pred_hi = _mm_madd_epi16(wx_hi, tr_ly);

      pred_lo = _mm_add_epi32(pred_lo, pred_round);
      pred_hi = _mm_add_epi32(pred_hi, pred_round);

      pred_lo = _mm_srai_epi32(pred_lo, SMOOTH_WEIGHT_LOG2_SCALE);
      pred_hi = _mm_srai_epi32(pred_hi, SMOOTH_WEIGHT_LOG2_SCALE);

      __m128i pred = _mm_packus_epi16(pred_lo, pred_hi);
      pred = _mm_shuffle_epi8(pred, gat);
      _mm_storel_epi64((__m128i *)(dst + x), pred);
    }
    dst += stride;
  }
}

void aom_smooth_h_predictor_16x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above,
                                       const uint8_t *left) {
  smooth_h_predictor_wxh(dst, stride, above, left, 16, 4);
}

void aom_smooth_h_predictor_16x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above,
                                       const uint8_t *left) {
  smooth_h_predictor_wxh(dst, stride, above, left, 16, 8);
}

void aom_smooth_h_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_h_predictor_wxh(dst, stride, above, left, 16, 16);
}

void aom_smooth_h_predictor_16x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_h_predictor_wxh(dst, stride, above, left, 16, 32);
}

void aom_smooth_h_predictor_16x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_h_predictor_wxh(dst, stride, above, left, 16, 64);
}

void aom_smooth_h_predictor_32x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above,
                                       const uint8_t *left) {
  smooth_h_predictor_wxh(dst, stride, above, left, 32, 8);
}

void aom_smooth_h_predictor_32x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_h_predictor_wxh(dst, stride, above, left, 32, 16);
}

void aom_smooth_h_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_h_predictor_wxh(dst, stride, above, left, 32, 32);
}

void aom_smooth_h_predictor_32x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_h_predictor_wxh(dst, stride, above, left, 32, 64);
}

void aom_smooth_h_predictor_64x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_h_predictor_wxh(dst, stride, above, left, 64, 64);
}

void aom_smooth_h_predictor_64x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_h_predictor_wxh(dst, stride, above, left, 64, 32);
}

void aom_smooth_h_predictor_64x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_h_predictor_wxh(dst, stride, above, left, 64, 16);
}

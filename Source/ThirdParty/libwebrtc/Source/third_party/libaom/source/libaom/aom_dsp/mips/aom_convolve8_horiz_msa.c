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

#include <assert.h>

#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/mips/aom_convolve_msa.h"

static void common_hz_8t_4x4_msa(const uint8_t *src, int32_t src_stride,
                                 uint8_t *dst, int32_t dst_stride,
                                 int8_t *filter) {
  v16u8 mask0, mask1, mask2, mask3, out;
  v16i8 src0, src1, src2, src3, filt0, filt1, filt2, filt3;
  v8i16 filt, out0, out1;

  mask0 = LD_UB(&mc_filt_mask_arr[16]);
  src -= 3;

  /* rearranging filter */
  filt = LD_SH(filter);
  SPLATI_H4_SB(filt, 0, 1, 2, 3, filt0, filt1, filt2, filt3);

  mask1 = mask0 + 2;
  mask2 = mask0 + 4;
  mask3 = mask0 + 6;

  LD_SB4(src, src_stride, src0, src1, src2, src3);
  XORI_B4_128_SB(src0, src1, src2, src3);
  HORIZ_8TAP_4WID_4VECS_FILT(src0, src1, src2, src3, mask0, mask1, mask2, mask3,
                             filt0, filt1, filt2, filt3, out0, out1);
  SRARI_H2_SH(out0, out1, FILTER_BITS);
  SAT_SH2_SH(out0, out1, 7);
  out = PCKEV_XORI128_UB(out0, out1);
  ST4x4_UB(out, out, 0, 1, 2, 3, dst, dst_stride);
}

static void common_hz_8t_4x8_msa(const uint8_t *src, int32_t src_stride,
                                 uint8_t *dst, int32_t dst_stride,
                                 int8_t *filter) {
  v16i8 filt0, filt1, filt2, filt3;
  v16i8 src0, src1, src2, src3;
  v16u8 mask0, mask1, mask2, mask3, out;
  v8i16 filt, out0, out1, out2, out3;

  mask0 = LD_UB(&mc_filt_mask_arr[16]);
  src -= 3;

  /* rearranging filter */
  filt = LD_SH(filter);
  SPLATI_H4_SB(filt, 0, 1, 2, 3, filt0, filt1, filt2, filt3);

  mask1 = mask0 + 2;
  mask2 = mask0 + 4;
  mask3 = mask0 + 6;

  LD_SB4(src, src_stride, src0, src1, src2, src3);
  XORI_B4_128_SB(src0, src1, src2, src3);
  src += (4 * src_stride);
  HORIZ_8TAP_4WID_4VECS_FILT(src0, src1, src2, src3, mask0, mask1, mask2, mask3,
                             filt0, filt1, filt2, filt3, out0, out1);
  LD_SB4(src, src_stride, src0, src1, src2, src3);
  XORI_B4_128_SB(src0, src1, src2, src3);
  HORIZ_8TAP_4WID_4VECS_FILT(src0, src1, src2, src3, mask0, mask1, mask2, mask3,
                             filt0, filt1, filt2, filt3, out2, out3);
  SRARI_H4_SH(out0, out1, out2, out3, FILTER_BITS);
  SAT_SH4_SH(out0, out1, out2, out3, 7);
  out = PCKEV_XORI128_UB(out0, out1);
  ST4x4_UB(out, out, 0, 1, 2, 3, dst, dst_stride);
  dst += (4 * dst_stride);
  out = PCKEV_XORI128_UB(out2, out3);
  ST4x4_UB(out, out, 0, 1, 2, 3, dst, dst_stride);
}

static void common_hz_8t_4w_msa(const uint8_t *src, int32_t src_stride,
                                uint8_t *dst, int32_t dst_stride,
                                int8_t *filter, int32_t height) {
  if (4 == height) {
    common_hz_8t_4x4_msa(src, src_stride, dst, dst_stride, filter);
  } else if (8 == height) {
    common_hz_8t_4x8_msa(src, src_stride, dst, dst_stride, filter);
  }
}

static void common_hz_8t_8x4_msa(const uint8_t *src, int32_t src_stride,
                                 uint8_t *dst, int32_t dst_stride,
                                 int8_t *filter) {
  v16i8 src0, src1, src2, src3, filt0, filt1, filt2, filt3;
  v16u8 mask0, mask1, mask2, mask3, tmp0, tmp1;
  v8i16 filt, out0, out1, out2, out3;

  mask0 = LD_UB(&mc_filt_mask_arr[0]);
  src -= 3;

  /* rearranging filter */
  filt = LD_SH(filter);
  SPLATI_H4_SB(filt, 0, 1, 2, 3, filt0, filt1, filt2, filt3);

  mask1 = mask0 + 2;
  mask2 = mask0 + 4;
  mask3 = mask0 + 6;

  LD_SB4(src, src_stride, src0, src1, src2, src3);
  XORI_B4_128_SB(src0, src1, src2, src3);
  HORIZ_8TAP_8WID_4VECS_FILT(src0, src1, src2, src3, mask0, mask1, mask2, mask3,
                             filt0, filt1, filt2, filt3, out0, out1, out2,
                             out3);
  SRARI_H4_SH(out0, out1, out2, out3, FILTER_BITS);
  SAT_SH4_SH(out0, out1, out2, out3, 7);
  tmp0 = PCKEV_XORI128_UB(out0, out1);
  tmp1 = PCKEV_XORI128_UB(out2, out3);
  ST8x4_UB(tmp0, tmp1, dst, dst_stride);
}

static void common_hz_8t_8x8mult_msa(const uint8_t *src, int32_t src_stride,
                                     uint8_t *dst, int32_t dst_stride,
                                     int8_t *filter, int32_t height) {
  uint32_t loop_cnt;
  v16i8 src0, src1, src2, src3, filt0, filt1, filt2, filt3;
  v16u8 mask0, mask1, mask2, mask3, tmp0, tmp1;
  v8i16 filt, out0, out1, out2, out3;

  mask0 = LD_UB(&mc_filt_mask_arr[0]);
  src -= 3;

  /* rearranging filter */
  filt = LD_SH(filter);
  SPLATI_H4_SB(filt, 0, 1, 2, 3, filt0, filt1, filt2, filt3);

  mask1 = mask0 + 2;
  mask2 = mask0 + 4;
  mask3 = mask0 + 6;

  for (loop_cnt = (height >> 2); loop_cnt--;) {
    LD_SB4(src, src_stride, src0, src1, src2, src3);
    XORI_B4_128_SB(src0, src1, src2, src3);
    src += (4 * src_stride);
    HORIZ_8TAP_8WID_4VECS_FILT(src0, src1, src2, src3, mask0, mask1, mask2,
                               mask3, filt0, filt1, filt2, filt3, out0, out1,
                               out2, out3);
    SRARI_H4_SH(out0, out1, out2, out3, FILTER_BITS);
    SAT_SH4_SH(out0, out1, out2, out3, 7);
    tmp0 = PCKEV_XORI128_UB(out0, out1);
    tmp1 = PCKEV_XORI128_UB(out2, out3);
    ST8x4_UB(tmp0, tmp1, dst, dst_stride);
    dst += (4 * dst_stride);
  }
}

static void common_hz_8t_8w_msa(const uint8_t *src, int32_t src_stride,
                                uint8_t *dst, int32_t dst_stride,
                                int8_t *filter, int32_t height) {
  if (4 == height) {
    common_hz_8t_8x4_msa(src, src_stride, dst, dst_stride, filter);
  } else {
    common_hz_8t_8x8mult_msa(src, src_stride, dst, dst_stride, filter, height);
  }
}

static void common_hz_8t_16w_msa(const uint8_t *src, int32_t src_stride,
                                 uint8_t *dst, int32_t dst_stride,
                                 int8_t *filter, int32_t height) {
  uint32_t loop_cnt;
  v16i8 src0, src1, src2, src3, filt0, filt1, filt2, filt3;
  v16u8 mask0, mask1, mask2, mask3, out;
  v8i16 filt, out0, out1, out2, out3;

  mask0 = LD_UB(&mc_filt_mask_arr[0]);
  src -= 3;

  /* rearranging filter */
  filt = LD_SH(filter);
  SPLATI_H4_SB(filt, 0, 1, 2, 3, filt0, filt1, filt2, filt3);

  mask1 = mask0 + 2;
  mask2 = mask0 + 4;
  mask3 = mask0 + 6;

  for (loop_cnt = (height >> 1); loop_cnt--;) {
    LD_SB2(src, src_stride, src0, src2);
    LD_SB2(src + 8, src_stride, src1, src3);
    XORI_B4_128_SB(src0, src1, src2, src3);
    src += (2 * src_stride);
    HORIZ_8TAP_8WID_4VECS_FILT(src0, src1, src2, src3, mask0, mask1, mask2,
                               mask3, filt0, filt1, filt2, filt3, out0, out1,
                               out2, out3);
    SRARI_H4_SH(out0, out1, out2, out3, FILTER_BITS);
    SAT_SH4_SH(out0, out1, out2, out3, 7);
    out = PCKEV_XORI128_UB(out0, out1);
    ST_UB(out, dst);
    dst += dst_stride;
    out = PCKEV_XORI128_UB(out2, out3);
    ST_UB(out, dst);
    dst += dst_stride;
  }
}

static void common_hz_8t_32w_msa(const uint8_t *src, int32_t src_stride,
                                 uint8_t *dst, int32_t dst_stride,
                                 int8_t *filter, int32_t height) {
  uint32_t loop_cnt;
  v16i8 src0, src1, src2, src3, filt0, filt1, filt2, filt3;
  v16u8 mask0, mask1, mask2, mask3, out;
  v8i16 filt, out0, out1, out2, out3;

  mask0 = LD_UB(&mc_filt_mask_arr[0]);
  src -= 3;

  /* rearranging filter */
  filt = LD_SH(filter);
  SPLATI_H4_SB(filt, 0, 1, 2, 3, filt0, filt1, filt2, filt3);

  mask1 = mask0 + 2;
  mask2 = mask0 + 4;
  mask3 = mask0 + 6;

  for (loop_cnt = (height >> 1); loop_cnt--;) {
    src0 = LD_SB(src);
    src2 = LD_SB(src + 16);
    src3 = LD_SB(src + 24);
    src1 = __msa_sldi_b(src2, src0, 8);
    src += src_stride;
    XORI_B4_128_SB(src0, src1, src2, src3);
    HORIZ_8TAP_8WID_4VECS_FILT(src0, src1, src2, src3, mask0, mask1, mask2,
                               mask3, filt0, filt1, filt2, filt3, out0, out1,
                               out2, out3);
    SRARI_H4_SH(out0, out1, out2, out3, FILTER_BITS);
    SAT_SH4_SH(out0, out1, out2, out3, 7);

    src0 = LD_SB(src);
    src2 = LD_SB(src + 16);
    src3 = LD_SB(src + 24);
    src1 = __msa_sldi_b(src2, src0, 8);
    src += src_stride;

    out = PCKEV_XORI128_UB(out0, out1);
    ST_UB(out, dst);
    out = PCKEV_XORI128_UB(out2, out3);
    ST_UB(out, dst + 16);
    dst += dst_stride;

    XORI_B4_128_SB(src0, src1, src2, src3);
    HORIZ_8TAP_8WID_4VECS_FILT(src0, src1, src2, src3, mask0, mask1, mask2,
                               mask3, filt0, filt1, filt2, filt3, out0, out1,
                               out2, out3);
    SRARI_H4_SH(out0, out1, out2, out3, FILTER_BITS);
    SAT_SH4_SH(out0, out1, out2, out3, 7);
    out = PCKEV_XORI128_UB(out0, out1);
    ST_UB(out, dst);
    out = PCKEV_XORI128_UB(out2, out3);
    ST_UB(out, dst + 16);
    dst += dst_stride;
  }
}

static void common_hz_8t_64w_msa(const uint8_t *src, int32_t src_stride,
                                 uint8_t *dst, int32_t dst_stride,
                                 int8_t *filter, int32_t height) {
  int32_t loop_cnt;
  v16i8 src0, src1, src2, src3, filt0, filt1, filt2, filt3;
  v16u8 mask0, mask1, mask2, mask3, out;
  v8i16 filt, out0, out1, out2, out3;

  mask0 = LD_UB(&mc_filt_mask_arr[0]);
  src -= 3;

  /* rearranging filter */
  filt = LD_SH(filter);
  SPLATI_H4_SB(filt, 0, 1, 2, 3, filt0, filt1, filt2, filt3);

  mask1 = mask0 + 2;
  mask2 = mask0 + 4;
  mask3 = mask0 + 6;

  for (loop_cnt = height; loop_cnt--;) {
    src0 = LD_SB(src);
    src2 = LD_SB(src + 16);
    src3 = LD_SB(src + 24);
    src1 = __msa_sldi_b(src2, src0, 8);

    XORI_B4_128_SB(src0, src1, src2, src3);
    HORIZ_8TAP_8WID_4VECS_FILT(src0, src1, src2, src3, mask0, mask1, mask2,
                               mask3, filt0, filt1, filt2, filt3, out0, out1,
                               out2, out3);
    SRARI_H4_SH(out0, out1, out2, out3, FILTER_BITS);
    SAT_SH4_SH(out0, out1, out2, out3, 7);
    out = PCKEV_XORI128_UB(out0, out1);
    ST_UB(out, dst);
    out = PCKEV_XORI128_UB(out2, out3);
    ST_UB(out, dst + 16);

    src0 = LD_SB(src + 32);
    src2 = LD_SB(src + 48);
    src3 = LD_SB(src + 56);
    src1 = __msa_sldi_b(src2, src0, 8);
    src += src_stride;

    XORI_B4_128_SB(src0, src1, src2, src3);
    HORIZ_8TAP_8WID_4VECS_FILT(src0, src1, src2, src3, mask0, mask1, mask2,
                               mask3, filt0, filt1, filt2, filt3, out0, out1,
                               out2, out3);
    SRARI_H4_SH(out0, out1, out2, out3, FILTER_BITS);
    SAT_SH4_SH(out0, out1, out2, out3, 7);
    out = PCKEV_XORI128_UB(out0, out1);
    ST_UB(out, dst + 32);
    out = PCKEV_XORI128_UB(out2, out3);
    ST_UB(out, dst + 48);
    dst += dst_stride;
  }
}

static void common_hz_2t_4x4_msa(const uint8_t *src, int32_t src_stride,
                                 uint8_t *dst, int32_t dst_stride,
                                 int8_t *filter) {
  v16i8 src0, src1, src2, src3, mask;
  v16u8 filt0, vec0, vec1, res0, res1;
  v8u16 vec2, vec3, filt;

  mask = LD_SB(&mc_filt_mask_arr[16]);

  /* rearranging filter */
  filt = LD_UH(filter);
  filt0 = (v16u8)__msa_splati_h((v8i16)filt, 0);

  LD_SB4(src, src_stride, src0, src1, src2, src3);
  VSHF_B2_UB(src0, src1, src2, src3, mask, mask, vec0, vec1);
  DOTP_UB2_UH(vec0, vec1, filt0, filt0, vec2, vec3);
  SRARI_H2_UH(vec2, vec3, FILTER_BITS);
  PCKEV_B2_UB(vec2, vec2, vec3, vec3, res0, res1);
  ST4x4_UB(res0, res1, 0, 1, 0, 1, dst, dst_stride);
}

static void common_hz_2t_4x8_msa(const uint8_t *src, int32_t src_stride,
                                 uint8_t *dst, int32_t dst_stride,
                                 int8_t *filter) {
  v16u8 vec0, vec1, vec2, vec3, filt0;
  v16i8 src0, src1, src2, src3, src4, src5, src6, src7, mask;
  v16i8 res0, res1, res2, res3;
  v8u16 vec4, vec5, vec6, vec7, filt;

  mask = LD_SB(&mc_filt_mask_arr[16]);

  /* rearranging filter */
  filt = LD_UH(filter);
  filt0 = (v16u8)__msa_splati_h((v8i16)filt, 0);

  LD_SB8(src, src_stride, src0, src1, src2, src3, src4, src5, src6, src7);
  VSHF_B2_UB(src0, src1, src2, src3, mask, mask, vec0, vec1);
  VSHF_B2_UB(src4, src5, src6, src7, mask, mask, vec2, vec3);
  DOTP_UB4_UH(vec0, vec1, vec2, vec3, filt0, filt0, filt0, filt0, vec4, vec5,
              vec6, vec7);
  SRARI_H4_UH(vec4, vec5, vec6, vec7, FILTER_BITS);
  PCKEV_B4_SB(vec4, vec4, vec5, vec5, vec6, vec6, vec7, vec7, res0, res1, res2,
              res3);
  ST4x4_UB(res0, res1, 0, 1, 0, 1, dst, dst_stride);
  dst += (4 * dst_stride);
  ST4x4_UB(res2, res3, 0, 1, 0, 1, dst, dst_stride);
}

static void common_hz_2t_4w_msa(const uint8_t *src, int32_t src_stride,
                                uint8_t *dst, int32_t dst_stride,
                                int8_t *filter, int32_t height) {
  if (4 == height) {
    common_hz_2t_4x4_msa(src, src_stride, dst, dst_stride, filter);
  } else if (8 == height) {
    common_hz_2t_4x8_msa(src, src_stride, dst, dst_stride, filter);
  }
}

static void common_hz_2t_8x4_msa(const uint8_t *src, int32_t src_stride,
                                 uint8_t *dst, int32_t dst_stride,
                                 int8_t *filter) {
  v16u8 filt0;
  v16i8 src0, src1, src2, src3, mask;
  v8u16 vec0, vec1, vec2, vec3, filt;

  mask = LD_SB(&mc_filt_mask_arr[0]);

  /* rearranging filter */
  filt = LD_UH(filter);
  filt0 = (v16u8)__msa_splati_h((v8i16)filt, 0);

  LD_SB4(src, src_stride, src0, src1, src2, src3);
  VSHF_B2_UH(src0, src0, src1, src1, mask, mask, vec0, vec1);
  VSHF_B2_UH(src2, src2, src3, src3, mask, mask, vec2, vec3);
  DOTP_UB4_UH(vec0, vec1, vec2, vec3, filt0, filt0, filt0, filt0, vec0, vec1,
              vec2, vec3);
  SRARI_H4_UH(vec0, vec1, vec2, vec3, FILTER_BITS);
  PCKEV_B2_SB(vec1, vec0, vec3, vec2, src0, src1);
  ST8x4_UB(src0, src1, dst, dst_stride);
}

static void common_hz_2t_8x8mult_msa(const uint8_t *src, int32_t src_stride,
                                     uint8_t *dst, int32_t dst_stride,
                                     int8_t *filter, int32_t height) {
  v16u8 filt0;
  v16i8 src0, src1, src2, src3, mask, out0, out1;
  v8u16 vec0, vec1, vec2, vec3, filt;

  mask = LD_SB(&mc_filt_mask_arr[0]);

  /* rearranging filter */
  filt = LD_UH(filter);
  filt0 = (v16u8)__msa_splati_h((v8i16)filt, 0);

  LD_SB4(src, src_stride, src0, src1, src2, src3);
  src += (4 * src_stride);

  VSHF_B2_UH(src0, src0, src1, src1, mask, mask, vec0, vec1);
  VSHF_B2_UH(src2, src2, src3, src3, mask, mask, vec2, vec3);
  DOTP_UB4_UH(vec0, vec1, vec2, vec3, filt0, filt0, filt0, filt0, vec0, vec1,
              vec2, vec3);
  SRARI_H4_UH(vec0, vec1, vec2, vec3, FILTER_BITS);

  LD_SB4(src, src_stride, src0, src1, src2, src3);
  src += (4 * src_stride);

  PCKEV_B2_SB(vec1, vec0, vec3, vec2, out0, out1);
  ST8x4_UB(out0, out1, dst, dst_stride);
  dst += (4 * dst_stride);

  VSHF_B2_UH(src0, src0, src1, src1, mask, mask, vec0, vec1);
  VSHF_B2_UH(src2, src2, src3, src3, mask, mask, vec2, vec3);
  DOTP_UB4_UH(vec0, vec1, vec2, vec3, filt0, filt0, filt0, filt0, vec0, vec1,
              vec2, vec3);
  SRARI_H4_UH(vec0, vec1, vec2, vec3, FILTER_BITS);
  PCKEV_B2_SB(vec1, vec0, vec3, vec2, out0, out1);
  ST8x4_UB(out0, out1, dst, dst_stride);
  dst += (4 * dst_stride);

  if (16 == height) {
    LD_SB4(src, src_stride, src0, src1, src2, src3);
    src += (4 * src_stride);

    VSHF_B2_UH(src0, src0, src1, src1, mask, mask, vec0, vec1);
    VSHF_B2_UH(src2, src2, src3, src3, mask, mask, vec2, vec3);
    DOTP_UB4_UH(vec0, vec1, vec2, vec3, filt0, filt0, filt0, filt0, vec0, vec1,
                vec2, vec3);
    SRARI_H4_UH(vec0, vec1, vec2, vec3, FILTER_BITS);
    LD_SB4(src, src_stride, src0, src1, src2, src3);

    PCKEV_B2_SB(vec1, vec0, vec3, vec2, out0, out1);
    ST8x4_UB(out0, out1, dst, dst_stride);

    VSHF_B2_UH(src0, src0, src1, src1, mask, mask, vec0, vec1);
    VSHF_B2_UH(src2, src2, src3, src3, mask, mask, vec2, vec3);
    DOTP_UB4_UH(vec0, vec1, vec2, vec3, filt0, filt0, filt0, filt0, vec0, vec1,
                vec2, vec3);
    SRARI_H4_UH(vec0, vec1, vec2, vec3, FILTER_BITS);
    PCKEV_B2_SB(vec1, vec0, vec3, vec2, out0, out1);
    ST8x4_UB(out0, out1, dst + 4 * dst_stride, dst_stride);
  }
}

static void common_hz_2t_8w_msa(const uint8_t *src, int32_t src_stride,
                                uint8_t *dst, int32_t dst_stride,
                                int8_t *filter, int32_t height) {
  if (4 == height) {
    common_hz_2t_8x4_msa(src, src_stride, dst, dst_stride, filter);
  } else {
    common_hz_2t_8x8mult_msa(src, src_stride, dst, dst_stride, filter, height);
  }
}

static void common_hz_2t_16w_msa(const uint8_t *src, int32_t src_stride,
                                 uint8_t *dst, int32_t dst_stride,
                                 int8_t *filter, int32_t height) {
  uint32_t loop_cnt;
  v16i8 src0, src1, src2, src3, src4, src5, src6, src7, mask;
  v16u8 filt0, vec0, vec1, vec2, vec3, vec4, vec5, vec6, vec7;
  v8u16 out0, out1, out2, out3, out4, out5, out6, out7, filt;

  mask = LD_SB(&mc_filt_mask_arr[0]);

  loop_cnt = (height >> 2) - 1;

  /* rearranging filter */
  filt = LD_UH(filter);
  filt0 = (v16u8)__msa_splati_h((v8i16)filt, 0);

  LD_SB4(src, src_stride, src0, src2, src4, src6);
  LD_SB4(src + 8, src_stride, src1, src3, src5, src7);
  src += (4 * src_stride);

  VSHF_B2_UB(src0, src0, src1, src1, mask, mask, vec0, vec1);
  VSHF_B2_UB(src2, src2, src3, src3, mask, mask, vec2, vec3);
  VSHF_B2_UB(src4, src4, src5, src5, mask, mask, vec4, vec5);
  VSHF_B2_UB(src6, src6, src7, src7, mask, mask, vec6, vec7);
  DOTP_UB4_UH(vec0, vec1, vec2, vec3, filt0, filt0, filt0, filt0, out0, out1,
              out2, out3);
  DOTP_UB4_UH(vec4, vec5, vec6, vec7, filt0, filt0, filt0, filt0, out4, out5,
              out6, out7);
  SRARI_H4_UH(out0, out1, out2, out3, FILTER_BITS);
  SRARI_H4_UH(out4, out5, out6, out7, FILTER_BITS);
  PCKEV_ST_SB(out0, out1, dst);
  dst += dst_stride;
  PCKEV_ST_SB(out2, out3, dst);
  dst += dst_stride;
  PCKEV_ST_SB(out4, out5, dst);
  dst += dst_stride;
  PCKEV_ST_SB(out6, out7, dst);
  dst += dst_stride;

  for (; loop_cnt--;) {
    LD_SB4(src, src_stride, src0, src2, src4, src6);
    LD_SB4(src + 8, src_stride, src1, src3, src5, src7);
    src += (4 * src_stride);

    VSHF_B2_UB(src0, src0, src1, src1, mask, mask, vec0, vec1);
    VSHF_B2_UB(src2, src2, src3, src3, mask, mask, vec2, vec3);
    VSHF_B2_UB(src4, src4, src5, src5, mask, mask, vec4, vec5);
    VSHF_B2_UB(src6, src6, src7, src7, mask, mask, vec6, vec7);
    DOTP_UB4_UH(vec0, vec1, vec2, vec3, filt0, filt0, filt0, filt0, out0, out1,
                out2, out3);
    DOTP_UB4_UH(vec4, vec5, vec6, vec7, filt0, filt0, filt0, filt0, out4, out5,
                out6, out7);
    SRARI_H4_UH(out0, out1, out2, out3, FILTER_BITS);
    SRARI_H4_UH(out4, out5, out6, out7, FILTER_BITS);
    PCKEV_ST_SB(out0, out1, dst);
    dst += dst_stride;
    PCKEV_ST_SB(out2, out3, dst);
    dst += dst_stride;
    PCKEV_ST_SB(out4, out5, dst);
    dst += dst_stride;
    PCKEV_ST_SB(out6, out7, dst);
    dst += dst_stride;
  }
}

static void common_hz_2t_32w_msa(const uint8_t *src, int32_t src_stride,
                                 uint8_t *dst, int32_t dst_stride,
                                 int8_t *filter, int32_t height) {
  uint32_t loop_cnt;
  v16i8 src0, src1, src2, src3, src4, src5, src6, src7, mask;
  v16u8 filt0, vec0, vec1, vec2, vec3, vec4, vec5, vec6, vec7;
  v8u16 out0, out1, out2, out3, out4, out5, out6, out7, filt;

  mask = LD_SB(&mc_filt_mask_arr[0]);

  /* rearranging filter */
  filt = LD_UH(filter);
  filt0 = (v16u8)__msa_splati_h((v8i16)filt, 0);

  for (loop_cnt = height >> 1; loop_cnt--;) {
    src0 = LD_SB(src);
    src2 = LD_SB(src + 16);
    src3 = LD_SB(src + 24);
    src1 = __msa_sldi_b(src2, src0, 8);
    src += src_stride;
    src4 = LD_SB(src);
    src6 = LD_SB(src + 16);
    src7 = LD_SB(src + 24);
    src5 = __msa_sldi_b(src6, src4, 8);
    src += src_stride;

    VSHF_B2_UB(src0, src0, src1, src1, mask, mask, vec0, vec1);
    VSHF_B2_UB(src2, src2, src3, src3, mask, mask, vec2, vec3);
    VSHF_B2_UB(src4, src4, src5, src5, mask, mask, vec4, vec5);
    VSHF_B2_UB(src6, src6, src7, src7, mask, mask, vec6, vec7);
    DOTP_UB4_UH(vec0, vec1, vec2, vec3, filt0, filt0, filt0, filt0, out0, out1,
                out2, out3);
    DOTP_UB4_UH(vec4, vec5, vec6, vec7, filt0, filt0, filt0, filt0, out4, out5,
                out6, out7);
    SRARI_H4_UH(out0, out1, out2, out3, FILTER_BITS);
    SRARI_H4_UH(out4, out5, out6, out7, FILTER_BITS);
    PCKEV_ST_SB(out0, out1, dst);
    PCKEV_ST_SB(out2, out3, dst + 16);
    dst += dst_stride;
    PCKEV_ST_SB(out4, out5, dst);
    PCKEV_ST_SB(out6, out7, dst + 16);
    dst += dst_stride;
  }
}

static void common_hz_2t_64w_msa(const uint8_t *src, int32_t src_stride,
                                 uint8_t *dst, int32_t dst_stride,
                                 int8_t *filter, int32_t height) {
  uint32_t loop_cnt;
  v16i8 src0, src1, src2, src3, src4, src5, src6, src7, mask;
  v16u8 filt0, vec0, vec1, vec2, vec3, vec4, vec5, vec6, vec7;
  v8u16 out0, out1, out2, out3, out4, out5, out6, out7, filt;

  mask = LD_SB(&mc_filt_mask_arr[0]);

  /* rearranging filter */
  filt = LD_UH(filter);
  filt0 = (v16u8)__msa_splati_h((v8i16)filt, 0);

  for (loop_cnt = height; loop_cnt--;) {
    src0 = LD_SB(src);
    src2 = LD_SB(src + 16);
    src4 = LD_SB(src + 32);
    src6 = LD_SB(src + 48);
    src7 = LD_SB(src + 56);
    SLDI_B3_SB(src2, src4, src6, src0, src2, src4, src1, src3, src5, 8);
    src += src_stride;

    VSHF_B2_UB(src0, src0, src1, src1, mask, mask, vec0, vec1);
    VSHF_B2_UB(src2, src2, src3, src3, mask, mask, vec2, vec3);
    VSHF_B2_UB(src4, src4, src5, src5, mask, mask, vec4, vec5);
    VSHF_B2_UB(src6, src6, src7, src7, mask, mask, vec6, vec7);
    DOTP_UB4_UH(vec0, vec1, vec2, vec3, filt0, filt0, filt0, filt0, out0, out1,
                out2, out3);
    DOTP_UB4_UH(vec4, vec5, vec6, vec7, filt0, filt0, filt0, filt0, out4, out5,
                out6, out7);
    SRARI_H4_UH(out0, out1, out2, out3, FILTER_BITS);
    SRARI_H4_UH(out4, out5, out6, out7, FILTER_BITS);
    PCKEV_ST_SB(out0, out1, dst);
    PCKEV_ST_SB(out2, out3, dst + 16);
    PCKEV_ST_SB(out4, out5, dst + 32);
    PCKEV_ST_SB(out6, out7, dst + 48);
    dst += dst_stride;
  }
}

void aom_convolve8_horiz_msa(const uint8_t *src, ptrdiff_t src_stride,
                             uint8_t *dst, ptrdiff_t dst_stride,
                             const int16_t *filter_x, int x_step_q4,
                             const int16_t *filter_y, int y_step_q4, int w,
                             int h) {
  int8_t cnt, filt_hor[8];

  assert(x_step_q4 == 16);
  assert(((const int32_t *)filter_x)[1] != 0x800000);

  for (cnt = 0; cnt < 8; ++cnt) {
    filt_hor[cnt] = filter_x[cnt];
  }

  if (((const int32_t *)filter_x)[0] == 0) {
    switch (w) {
      case 4:
        common_hz_2t_4w_msa(src, (int32_t)src_stride, dst, (int32_t)dst_stride,
                            &filt_hor[3], h);
        break;
      case 8:
        common_hz_2t_8w_msa(src, (int32_t)src_stride, dst, (int32_t)dst_stride,
                            &filt_hor[3], h);
        break;
      case 16:
        common_hz_2t_16w_msa(src, (int32_t)src_stride, dst, (int32_t)dst_stride,
                             &filt_hor[3], h);
        break;
      case 32:
        common_hz_2t_32w_msa(src, (int32_t)src_stride, dst, (int32_t)dst_stride,
                             &filt_hor[3], h);
        break;
      case 64:
        common_hz_2t_64w_msa(src, (int32_t)src_stride, dst, (int32_t)dst_stride,
                             &filt_hor[3], h);
        break;
      default:
        aom_convolve8_horiz_c(src, src_stride, dst, dst_stride, filter_x,
                              x_step_q4, filter_y, y_step_q4, w, h);
        break;
    }
  } else {
    switch (w) {
      case 4:
        common_hz_8t_4w_msa(src, (int32_t)src_stride, dst, (int32_t)dst_stride,
                            filt_hor, h);
        break;
      case 8:
        common_hz_8t_8w_msa(src, (int32_t)src_stride, dst, (int32_t)dst_stride,
                            filt_hor, h);
        break;
      case 16:
        common_hz_8t_16w_msa(src, (int32_t)src_stride, dst, (int32_t)dst_stride,
                             filt_hor, h);
        break;
      case 32:
        common_hz_8t_32w_msa(src, (int32_t)src_stride, dst, (int32_t)dst_stride,
                             filt_hor, h);
        break;
      case 64:
        common_hz_8t_64w_msa(src, (int32_t)src_stride, dst, (int32_t)dst_stride,
                             filt_hor, h);
        break;
      default:
        aom_convolve8_horiz_c(src, src_stride, dst, dst_stride, filter_x,
                              x_step_q4, filter_y, y_step_q4, w, h);
        break;
    }
  }
}

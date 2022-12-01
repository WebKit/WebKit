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

#include "config/av1_rtcd.h"

#include "aom_dsp/mips/macros_msa.h"

static void temporal_filter_apply_8size_msa(uint8_t *frm1_ptr, uint32_t stride,
                                            uint8_t *frm2_ptr, int32_t filt_sth,
                                            int32_t filt_wgt, uint32_t *acc,
                                            uint16_t *cnt) {
  uint32_t row;
  uint64_t f0, f1, f2, f3;
  v16i8 frm2, frm1 = { 0 };
  v16i8 frm4, frm3 = { 0 };
  v16u8 frm_r, frm_l;
  v8i16 frm2_r, frm2_l;
  v8i16 diff0, diff1, mod0_h, mod1_h;
  v4i32 cnst3, cnst16, filt_wt, strength;
  v4i32 mod0_w, mod1_w, mod2_w, mod3_w;
  v4i32 diff0_r, diff0_l, diff1_r, diff1_l;
  v4i32 frm2_rr, frm2_rl, frm2_lr, frm2_ll;
  v4i32 acc0, acc1, acc2, acc3;
  v8i16 cnt0, cnt1;

  filt_wt = __msa_fill_w(filt_wgt);
  strength = __msa_fill_w(filt_sth);
  cnst3 = __msa_ldi_w(3);
  cnst16 = __msa_ldi_w(16);

  for (row = 2; row--;) {
    LD4(frm1_ptr, stride, f0, f1, f2, f3);
    frm1_ptr += (4 * stride);

    LD_SB2(frm2_ptr, 16, frm2, frm4);
    frm2_ptr += 32;

    LD_SW2(acc, 4, acc0, acc1);
    LD_SW2(acc + 8, 4, acc2, acc3);
    LD_SH2(cnt, 8, cnt0, cnt1);

    INSERT_D2_SB(f0, f1, frm1);
    INSERT_D2_SB(f2, f3, frm3);
    ILVRL_B2_UB(frm1, frm2, frm_r, frm_l);
    HSUB_UB2_SH(frm_r, frm_l, diff0, diff1);
    UNPCK_SH_SW(diff0, diff0_r, diff0_l);
    UNPCK_SH_SW(diff1, diff1_r, diff1_l);
    MUL4(diff0_r, diff0_r, diff0_l, diff0_l, diff1_r, diff1_r, diff1_l, diff1_l,
         mod0_w, mod1_w, mod2_w, mod3_w);
    MUL4(mod0_w, cnst3, mod1_w, cnst3, mod2_w, cnst3, mod3_w, cnst3, mod0_w,
         mod1_w, mod2_w, mod3_w);
    SRAR_W4_SW(mod0_w, mod1_w, mod2_w, mod3_w, strength);

    diff0_r = (mod0_w < cnst16);
    diff0_l = (mod1_w < cnst16);
    diff1_r = (mod2_w < cnst16);
    diff1_l = (mod3_w < cnst16);

    SUB4(cnst16, mod0_w, cnst16, mod1_w, cnst16, mod2_w, cnst16, mod3_w, mod0_w,
         mod1_w, mod2_w, mod3_w);

    mod0_w = diff0_r & mod0_w;
    mod1_w = diff0_l & mod1_w;
    mod2_w = diff1_r & mod2_w;
    mod3_w = diff1_l & mod3_w;

    MUL4(mod0_w, filt_wt, mod1_w, filt_wt, mod2_w, filt_wt, mod3_w, filt_wt,
         mod0_w, mod1_w, mod2_w, mod3_w);
    PCKEV_H2_SH(mod1_w, mod0_w, mod3_w, mod2_w, mod0_h, mod1_h);
    ADD2(mod0_h, cnt0, mod1_h, cnt1, mod0_h, mod1_h);
    ST_SH2(mod0_h, mod1_h, cnt, 8);
    cnt += 16;

    UNPCK_UB_SH(frm2, frm2_r, frm2_l);
    UNPCK_SH_SW(frm2_r, frm2_rr, frm2_rl);
    UNPCK_SH_SW(frm2_l, frm2_lr, frm2_ll);
    MUL4(mod0_w, frm2_rr, mod1_w, frm2_rl, mod2_w, frm2_lr, mod3_w, frm2_ll,
         mod0_w, mod1_w, mod2_w, mod3_w);
    ADD4(mod0_w, acc0, mod1_w, acc1, mod2_w, acc2, mod3_w, acc3, mod0_w, mod1_w,
         mod2_w, mod3_w);

    ST_SW2(mod0_w, mod1_w, acc, 4);
    acc += 8;
    ST_SW2(mod2_w, mod3_w, acc, 4);
    acc += 8;

    LD_SW2(acc, 4, acc0, acc1);
    LD_SW2(acc + 8, 4, acc2, acc3);
    LD_SH2(cnt, 8, cnt0, cnt1);

    ILVRL_B2_UB(frm3, frm4, frm_r, frm_l);
    HSUB_UB2_SH(frm_r, frm_l, diff0, diff1);
    UNPCK_SH_SW(diff0, diff0_r, diff0_l);
    UNPCK_SH_SW(diff1, diff1_r, diff1_l);
    MUL4(diff0_r, diff0_r, diff0_l, diff0_l, diff1_r, diff1_r, diff1_l, diff1_l,
         mod0_w, mod1_w, mod2_w, mod3_w);
    MUL4(mod0_w, cnst3, mod1_w, cnst3, mod2_w, cnst3, mod3_w, cnst3, mod0_w,
         mod1_w, mod2_w, mod3_w);
    SRAR_W4_SW(mod0_w, mod1_w, mod2_w, mod3_w, strength);

    diff0_r = (mod0_w < cnst16);
    diff0_l = (mod1_w < cnst16);
    diff1_r = (mod2_w < cnst16);
    diff1_l = (mod3_w < cnst16);

    SUB4(cnst16, mod0_w, cnst16, mod1_w, cnst16, mod2_w, cnst16, mod3_w, mod0_w,
         mod1_w, mod2_w, mod3_w);

    mod0_w = diff0_r & mod0_w;
    mod1_w = diff0_l & mod1_w;
    mod2_w = diff1_r & mod2_w;
    mod3_w = diff1_l & mod3_w;

    MUL4(mod0_w, filt_wt, mod1_w, filt_wt, mod2_w, filt_wt, mod3_w, filt_wt,
         mod0_w, mod1_w, mod2_w, mod3_w);
    PCKEV_H2_SH(mod1_w, mod0_w, mod3_w, mod2_w, mod0_h, mod1_h);
    ADD2(mod0_h, cnt0, mod1_h, cnt1, mod0_h, mod1_h);
    ST_SH2(mod0_h, mod1_h, cnt, 8);
    cnt += 16;
    UNPCK_UB_SH(frm4, frm2_r, frm2_l);
    UNPCK_SH_SW(frm2_r, frm2_rr, frm2_rl);
    UNPCK_SH_SW(frm2_l, frm2_lr, frm2_ll);
    MUL4(mod0_w, frm2_rr, mod1_w, frm2_rl, mod2_w, frm2_lr, mod3_w, frm2_ll,
         mod0_w, mod1_w, mod2_w, mod3_w);
    ADD4(mod0_w, acc0, mod1_w, acc1, mod2_w, acc2, mod3_w, acc3, mod0_w, mod1_w,
         mod2_w, mod3_w);

    ST_SW2(mod0_w, mod1_w, acc, 4);
    acc += 8;
    ST_SW2(mod2_w, mod3_w, acc, 4);
    acc += 8;
  }
}

static void temporal_filter_apply_16size_msa(uint8_t *frm1_ptr, uint32_t stride,
                                             uint8_t *frm2_ptr,
                                             int32_t filt_sth, int32_t filt_wgt,
                                             uint32_t *acc, uint16_t *cnt) {
  uint32_t row;
  v16i8 frm1, frm2, frm3, frm4;
  v16u8 frm_r, frm_l;
  v16i8 zero = { 0 };
  v8u16 frm2_r, frm2_l;
  v8i16 diff0, diff1, mod0_h, mod1_h;
  v4i32 cnst3, cnst16, filt_wt, strength;
  v4i32 mod0_w, mod1_w, mod2_w, mod3_w;
  v4i32 diff0_r, diff0_l, diff1_r, diff1_l;
  v4i32 frm2_rr, frm2_rl, frm2_lr, frm2_ll;
  v4i32 acc0, acc1, acc2, acc3;
  v8i16 cnt0, cnt1;

  filt_wt = __msa_fill_w(filt_wgt);
  strength = __msa_fill_w(filt_sth);
  cnst3 = __msa_ldi_w(3);
  cnst16 = __msa_ldi_w(16);

  for (row = 8; row--;) {
    LD_SB2(frm1_ptr, stride, frm1, frm3);
    frm1_ptr += stride;

    LD_SB2(frm2_ptr, 16, frm2, frm4);
    frm2_ptr += 16;

    LD_SW2(acc, 4, acc0, acc1);
    LD_SW2(acc, 4, acc2, acc3);
    LD_SH2(cnt, 8, cnt0, cnt1);

    ILVRL_B2_UB(frm1, frm2, frm_r, frm_l);
    HSUB_UB2_SH(frm_r, frm_l, diff0, diff1);
    UNPCK_SH_SW(diff0, diff0_r, diff0_l);
    UNPCK_SH_SW(diff1, diff1_r, diff1_l);
    MUL4(diff0_r, diff0_r, diff0_l, diff0_l, diff1_r, diff1_r, diff1_l, diff1_l,
         mod0_w, mod1_w, mod2_w, mod3_w);
    MUL4(mod0_w, cnst3, mod1_w, cnst3, mod2_w, cnst3, mod3_w, cnst3, mod0_w,
         mod1_w, mod2_w, mod3_w);
    SRAR_W4_SW(mod0_w, mod1_w, mod2_w, mod3_w, strength);

    diff0_r = (mod0_w < cnst16);
    diff0_l = (mod1_w < cnst16);
    diff1_r = (mod2_w < cnst16);
    diff1_l = (mod3_w < cnst16);

    SUB4(cnst16, mod0_w, cnst16, mod1_w, cnst16, mod2_w, cnst16, mod3_w, mod0_w,
         mod1_w, mod2_w, mod3_w);

    mod0_w = diff0_r & mod0_w;
    mod1_w = diff0_l & mod1_w;
    mod2_w = diff1_r & mod2_w;
    mod3_w = diff1_l & mod3_w;

    MUL4(mod0_w, filt_wt, mod1_w, filt_wt, mod2_w, filt_wt, mod3_w, filt_wt,
         mod0_w, mod1_w, mod2_w, mod3_w);
    PCKEV_H2_SH(mod1_w, mod0_w, mod3_w, mod2_w, mod0_h, mod1_h);
    ADD2(mod0_h, cnt0, mod1_h, cnt1, mod0_h, mod1_h);
    ST_SH2(mod0_h, mod1_h, cnt, 8);
    cnt += 16;

    ILVRL_B2_UH(zero, frm2, frm2_r, frm2_l);
    UNPCK_SH_SW(frm2_r, frm2_rr, frm2_rl);
    UNPCK_SH_SW(frm2_l, frm2_lr, frm2_ll);
    MUL4(mod0_w, frm2_rr, mod1_w, frm2_rl, mod2_w, frm2_lr, mod3_w, frm2_ll,
         mod0_w, mod1_w, mod2_w, mod3_w);
    ADD4(mod0_w, acc0, mod1_w, acc1, mod2_w, acc2, mod3_w, acc3, mod0_w, mod1_w,
         mod2_w, mod3_w);

    ST_SW2(mod0_w, mod1_w, acc, 4);
    acc += 8;
    ST_SW2(mod2_w, mod3_w, acc, 4);
    acc += 8;

    LD_SW2(acc, 4, acc0, acc1);
    LD_SW2(acc + 8, 4, acc2, acc3);
    LD_SH2(cnt, 8, cnt0, cnt1);

    ILVRL_B2_UB(frm3, frm4, frm_r, frm_l);
    HSUB_UB2_SH(frm_r, frm_l, diff0, diff1);
    UNPCK_SH_SW(diff0, diff0_r, diff0_l);
    UNPCK_SH_SW(diff1, diff1_r, diff1_l);
    MUL4(diff0_r, diff0_r, diff0_l, diff0_l, diff1_r, diff1_r, diff1_l, diff1_l,
         mod0_w, mod1_w, mod2_w, mod3_w);
    MUL4(mod0_w, cnst3, mod1_w, cnst3, mod2_w, cnst3, mod3_w, cnst3, mod0_w,
         mod1_w, mod2_w, mod3_w);
    SRAR_W4_SW(mod0_w, mod1_w, mod2_w, mod3_w, strength);

    diff0_r = (mod0_w < cnst16);
    diff0_l = (mod1_w < cnst16);
    diff1_r = (mod2_w < cnst16);
    diff1_l = (mod3_w < cnst16);

    SUB4(cnst16, mod0_w, cnst16, mod1_w, cnst16, mod2_w, cnst16, mod3_w, mod0_w,
         mod1_w, mod2_w, mod3_w);

    mod0_w = diff0_r & mod0_w;
    mod1_w = diff0_l & mod1_w;
    mod2_w = diff1_r & mod2_w;
    mod3_w = diff1_l & mod3_w;

    MUL4(mod0_w, filt_wt, mod1_w, filt_wt, mod2_w, filt_wt, mod3_w, filt_wt,
         mod0_w, mod1_w, mod2_w, mod3_w);
    PCKEV_H2_SH(mod1_w, mod0_w, mod3_w, mod2_w, mod0_h, mod1_h);
    ADD2(mod0_h, cnt0, mod1_h, cnt1, mod0_h, mod1_h);
    ST_SH2(mod0_h, mod1_h, cnt, 8);
    cnt += 16;

    ILVRL_B2_UH(zero, frm4, frm2_r, frm2_l);
    UNPCK_SH_SW(frm2_r, frm2_rr, frm2_rl);
    UNPCK_SH_SW(frm2_l, frm2_lr, frm2_ll);
    MUL4(mod0_w, frm2_rr, mod1_w, frm2_rl, mod2_w, frm2_lr, mod3_w, frm2_ll,
         mod0_w, mod1_w, mod2_w, mod3_w);
    ADD4(mod0_w, acc0, mod1_w, acc1, mod2_w, acc2, mod3_w, acc3, mod0_w, mod1_w,
         mod2_w, mod3_w);
    ST_SW2(mod0_w, mod1_w, acc, 4);
    acc += 8;
    ST_SW2(mod2_w, mod3_w, acc, 4);
    acc += 8;

    frm1_ptr += stride;
    frm2_ptr += 16;
  }
}

// TODO(yunqing) The following optimization is not used since c code changes.
void av1_temporal_filter_apply_msa(uint8_t *frame1_ptr, uint32_t stride,
                                   uint8_t *frame2_ptr, uint32_t blk_w,
                                   uint32_t blk_h, int32_t strength,
                                   int32_t filt_wgt, uint32_t *accu,
                                   uint16_t *cnt) {
  if (8 == (blk_w * blk_h)) {
    temporal_filter_apply_8size_msa(frame1_ptr, stride, frame2_ptr, strength,
                                    filt_wgt, accu, cnt);
  } else if (16 == (blk_w * blk_h)) {
    temporal_filter_apply_16size_msa(frame1_ptr, stride, frame2_ptr, strength,
                                     filt_wgt, accu, cnt);
  } else {
    av1_temporal_filter_apply_c(frame1_ptr, stride, frame2_ptr, blk_w, blk_h,
                                strength, filt_wgt, accu, cnt);
  }
}

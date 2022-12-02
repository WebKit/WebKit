/*
 * Copyright (c) 2001-2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "aom_dsp/entcode.h"

/*Given the current total integer number of bits used and the current value of
   rng, computes the fraction number of bits used to OD_BITRES precision.
  This is used by od_ec_enc_tell_frac() and od_ec_dec_tell_frac().
  nbits_total: The number of whole bits currently used, i.e., the value
                returned by od_ec_enc_tell() or od_ec_dec_tell().
  rng: The current value of rng from either the encoder or decoder state.
  Return: The number of bits scaled by 2**OD_BITRES.
          This will always be slightly larger than the exact value (e.g., all
           rounding error is in the positive direction).*/
uint32_t od_ec_tell_frac(uint32_t nbits_total, uint32_t rng) {
  uint32_t nbits;
  int l;
  int i;
  /*To handle the non-integral number of bits still left in the encoder/decoder
     state, we compute the worst-case number of bits of val that must be
     encoded to ensure that the value is inside the range for any possible
     subsequent bits.
    The computation here is independent of val itself (the decoder does not
     even track that value), even though the real number of bits used after
     od_ec_enc_done() may be 1 smaller if rng is a power of two and the
     corresponding trailing bits of val are all zeros.
    If we did try to track that special case, then coding a value with a
     probability of 1/(1 << n) might sometimes appear to use more than n bits.
    This may help explain the surprising result that a newly initialized
     encoder or decoder claims to have used 1 bit.*/
  nbits = nbits_total << OD_BITRES;
  l = 0;
  for (i = OD_BITRES; i-- > 0;) {
    int b;
    rng = rng * rng >> 15;
    b = (int)(rng >> 16);
    l = l << 1 | b;
    rng >>= b;
  }
  return nbits - l;
}

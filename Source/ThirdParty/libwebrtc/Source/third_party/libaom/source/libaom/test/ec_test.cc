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

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include <cstdlib>
#include <memory>
#include <new>

#include "aom_dsp/entenc.h"
#include "aom_dsp/entdec.h"

TEST(EC_TEST, random_ec_test) {
  od_ec_enc enc;
  od_ec_dec dec;
  int sz;
  int i;
  int ret;
  unsigned int seed;
  unsigned char *ptr;
  uint32_t ptr_sz;
  char *seed_str;
  ret = 0;
  seed_str = getenv("EC_TEST_SEED");
  if (seed_str) {
    seed = atoi(seed_str);
  } else {
    seed = 0xdaa1a;
  }
  srand(seed);
  od_ec_enc_init(&enc, 1);
  /*Test compatibility between multiple different encode/decode routines.*/
  for (i = 0; i < 409600; i++) {
    int j;
    sz = rand() / ((RAND_MAX >> (rand() % 9U)) + 1U);
    std::unique_ptr<unsigned[]> fz(new (std::nothrow) unsigned[sz]);
    ASSERT_NE(fz, nullptr);
    std::unique_ptr<unsigned[]> fts(new (std::nothrow) unsigned[sz]);
    ASSERT_NE(fts, nullptr);
    std::unique_ptr<unsigned[]> data(new (std::nothrow) unsigned[sz]);
    ASSERT_NE(data, nullptr);
    std::unique_ptr<unsigned[]> tell(new (std::nothrow) unsigned[sz + 1]);
    ASSERT_NE(tell, nullptr);
    std::unique_ptr<unsigned[]> enc_method(new (std::nothrow) unsigned[sz]);
    ASSERT_NE(enc_method, nullptr);
    od_ec_enc_reset(&enc);
    tell[0] = od_ec_enc_tell_frac(&enc);
    for (j = 0; j < sz; j++) {
      data[j] = rand() / ((RAND_MAX >> 1) + 1);

      fts[j] = CDF_PROB_BITS;
      fz[j] = (rand() % (CDF_PROB_TOP - 2)) >> (CDF_PROB_BITS - fts[j]);
      fz[j] = OD_MAXI(fz[j], 1);
      enc_method[j] = 3 + (rand() & 1);
      switch (enc_method[j]) {
        case 3: {
          od_ec_encode_bool_q15(&enc, data[j],
                                OD_ICDF(fz[j] << (CDF_PROB_BITS - fts[j])));
          break;
        }
        case 4: {
          uint16_t cdf[2];
          cdf[0] = OD_ICDF(fz[j]);
          cdf[1] = OD_ICDF(1U << fts[j]);
          od_ec_encode_cdf_q15(&enc, data[j], cdf, 2);
          break;
        }
      }

      tell[j + 1] = od_ec_enc_tell_frac(&enc);
    }
    ptr = od_ec_enc_done(&enc, &ptr_sz);
    EXPECT_GE(((od_ec_enc_tell(&enc) + 7U) >> 3), ptr_sz)
        << "od_ec_enc_tell() lied: "
           "there's "
        << ptr_sz << " bytes instead of " << ((od_ec_enc_tell(&enc) + 7) >> 3)
        << " (Random seed: " << seed << ")\n";
    od_ec_dec_init(&dec, ptr, ptr_sz);
    EXPECT_EQ(od_ec_dec_tell_frac(&dec), tell[0])
        << "od_ec_dec_tell() mismatch between encoder and decoder "
           "at symbol 0: "
        << (unsigned)od_ec_dec_tell_frac(&dec) << " instead of " << tell[0]
        << " (Random seed: " << seed << ").\n";
    for (j = 0; j < sz; j++) {
      int dec_method;
      unsigned int sym = data[j] + 1;  // Initialize sym to an invalid value.

      if (CDF_SHIFT == 0) {
        dec_method = 3 + (rand() & 1);
      } else {
        dec_method = enc_method[j];
      }
      switch (dec_method) {
        case 3: {
          sym = od_ec_decode_bool_q15(
              &dec, OD_ICDF(fz[j] << (CDF_PROB_BITS - fts[j])));
          break;
        }
        case 4: {
          uint16_t cdf[2];
          cdf[0] = OD_ICDF(fz[j]);
          cdf[1] = OD_ICDF(1U << fts[j]);
          sym = od_ec_decode_cdf_q15(&dec, cdf, 2);
          break;
        }
      }

      EXPECT_EQ(sym, data[j])
          << "Decoded " << sym << " instead of " << data[j]
          << " with fz=" << fz[j] << " and ftb=" << fts[j] << "at position "
          << j << " of " << sz << " (Random seed: " << seed << ").\n"
          << "Encoding method: " << enc_method[j]
          << " decoding method: " << dec_method << "\n";
      EXPECT_EQ(od_ec_dec_tell_frac(&dec), tell[j + 1])
          << "od_ec_dec_tell() mismatch between encoder and "
             "decoder at symbol "
          << j + 1 << ": " << (unsigned)od_ec_dec_tell_frac(&dec)
          << " instead of " << tell[j + 1] << " (Random seed: " << seed
          << ").\n";
    }
  }
  od_ec_enc_reset(&enc);
  if (CDF_SHIFT == 0) {
    od_ec_encode_bool_q15(&enc, 0, OD_ICDF(16384));
    od_ec_encode_bool_q15(&enc, 0, OD_ICDF(16384));
    od_ec_encode_bool_q15(&enc, 0, OD_ICDF(16384));
    od_ec_encode_bool_q15(&enc, 0, OD_ICDF(16384));
    od_ec_encode_bool_q15(&enc, 0, OD_ICDF(24576));
    od_ec_enc_patch_initial_bits(&enc, 3, 2);
    EXPECT_FALSE(enc.error) << "od_ec_enc_patch_initial_bits() failed.\n";
    od_ec_enc_patch_initial_bits(&enc, 0, 5);
    EXPECT_TRUE(enc.error)
        << "od_ec_enc_patch_initial_bits() didn't fail when it should have.\n";
    od_ec_enc_reset(&enc);
    od_ec_encode_bool_q15(&enc, 0, OD_ICDF(16384));
    od_ec_encode_bool_q15(&enc, 0, OD_ICDF(16384));
    od_ec_encode_bool_q15(&enc, 1, OD_ICDF(32256));
    od_ec_encode_bool_q15(&enc, 0, OD_ICDF(24576));
    od_ec_enc_patch_initial_bits(&enc, 0, 2);
    EXPECT_FALSE(enc.error) << "od_ec_enc_patch_initial_bits() failed.\n";
    ptr = od_ec_enc_done(&enc, &ptr_sz);
    EXPECT_EQ(ptr_sz, 2u);
    EXPECT_EQ(ptr[0], 63)
        << "Got " << ptr[0]
        << " when expecting 63 for od_ec_enc_patch_initial_bits().\n";
  }
  od_ec_enc_clear(&enc);
  EXPECT_EQ(ret, 0);
}

/* Copyright (c) 2023, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include <gtest/gtest.h>
#include "../../internal.h"
#include "../../test/abi_test.h"

#if !defined(OPENSSL_NO_ASM) && defined(__GNUC__) && defined(__x86_64__) && \
    defined(SUPPORTS_ABI_TEST)
extern "C" {
#include "../../../third_party/fiat/p256_64.h"
}

TEST(P256Test, AdxMulABI) {
  static const uint64_t in1[4] = {0}, in2[4] = {0};
  uint64_t out[4];
  if (CRYPTO_is_BMI1_capable() && CRYPTO_is_BMI2_capable() &&
      CRYPTO_is_ADX_capable()) {
    CHECK_ABI(fiat_p256_adx_mul, out, in1, in2);
  } else {
    GTEST_SKIP() << "Can't test ABI of ADX code without ADX";
  }
}

#include <assert.h>
TEST(P256Test, AdxSquareABI) {
  static const uint64_t in[4] = {0};
  uint64_t out[4];
  if (CRYPTO_is_BMI1_capable() && CRYPTO_is_BMI2_capable() &&
      CRYPTO_is_ADX_capable()) {
    CHECK_ABI(fiat_p256_adx_sqr, out, in);
  } else {
    GTEST_SKIP() << "Can't test ABI of ADX code without ADX";
  }
}
#endif

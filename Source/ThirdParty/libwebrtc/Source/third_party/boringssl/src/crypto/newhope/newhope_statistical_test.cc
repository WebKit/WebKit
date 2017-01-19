/* Copyright (c) 2016, Google Inc.
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

#include <string>

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <openssl/crypto.h>
#include <openssl/newhope.h>
#include <openssl/rand.h>

#include "internal.h"


static const unsigned kNumTests = 1000;

static bool TestNoise(void) {
  printf("noise distribution:\n");

  size_t buckets[1 + 2 * PARAM_K];
  memset(buckets, 0, sizeof(buckets));
  for (size_t i = 0; i < kNumTests; i++) {
    NEWHOPE_POLY s;
    NEWHOPE_POLY_noise(&s);
    for (int j = 0; j < PARAM_N; j++) {
      uint16_t value = (s.coeffs[j] + PARAM_K) % PARAM_Q;
      buckets[value]++;
    }
  }

  int64_t sum = 0, square_sum = 0;
  for (int64_t i = 0; i < 1 + 2 * PARAM_K; i++) {
    sum += (i - PARAM_K) * (int64_t) buckets[i];
    square_sum += (i - PARAM_K) * (i - PARAM_K) * (int64_t) buckets[i];
  }
  double mean = double(sum) / double(PARAM_N * kNumTests);

  double expected_variance = 0.5 * 0.5 * double(PARAM_K * 2);
  double variance = double(square_sum) / double(PARAM_N * kNumTests) - mean * mean;

  for (size_t i = 0; i < 1 + 2 * PARAM_K; i++) {
    std::string dots;
    for (size_t j = 0; j < 79 * buckets[i] / buckets[PARAM_K]; j++) {
      dots += "+";
    }
    printf("%+zd\t%zd\t%s\n", i - PARAM_K, buckets[i], dots.c_str());
  }
  printf("mean: got %f, want %f\n", mean, 0.0);
  printf("variance: got %f, want %f\n", variance, expected_variance);
  printf("\n");

  if (mean < -0.5 || 0.5 < mean) {
    fprintf(stderr, "mean out of range: %f\n", mean);
    return false;
  }

  if (variance < expected_variance - 1.0 || expected_variance + 1.0 < variance) {
    fprintf(stderr, "variance out of range: got %f, want %f\n", variance,
            expected_variance);
    return false;
  }
  return true;
}

static int Hamming32(const uint8_t key[NEWHOPE_KEY_LENGTH]) {
  static int kHamming[256] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
  };

  int r = 0;
  for(int i = 0; i < NEWHOPE_KEY_LENGTH; i++) {
    r += kHamming[key[i]];
  }
  return r;
}

static bool TestKeys(void) {
  printf("keys (prior to whitening):\n");

  uint8_t key[NEWHOPE_KEY_LENGTH];
  uint8_t offermsg[NEWHOPE_OFFERMSG_LENGTH];

  bssl::UniquePtr<NEWHOPE_POLY> sk(NEWHOPE_POLY_new()), pk(NEWHOPE_POLY_new()),
      sp(NEWHOPE_POLY_new()), ep(NEWHOPE_POLY_new()), epp(NEWHOPE_POLY_new()),
      a(NEWHOPE_POLY_new()), bp(NEWHOPE_POLY_new()), rec(NEWHOPE_POLY_new());

  int ones = 0;
  for (size_t i = 0; i < kNumTests; i++) {
    NEWHOPE_offer(offermsg, sk.get());
    NEWHOPE_offer_frommsg(pk.get(), a.get(), offermsg);

    NEWHOPE_POLY_noise_ntt(sp.get());
    NEWHOPE_POLY_noise_ntt(ep.get());
    NEWHOPE_POLY_noise(epp.get());  /* intentionally not NTT */

    uint8_t rand[32];
    RAND_bytes(rand, 32);

    NEWHOPE_accept_computation(key, bp.get(), rec.get(),
                               sp.get(), ep.get(), epp.get(), rand,
                               pk.get(), a.get());
    ones += Hamming32(key);
  }

  int bits = NEWHOPE_KEY_LENGTH * 8 * kNumTests;
  int diff = bits - 2 * ones;
  double fraction = (double) abs(diff) / bits;
  printf("ones:   %d\n", ones);
  printf("zeroes: %d\n", (bits - ones));
  printf("diff:   got %d (%f), want 0\n", diff, fraction);
  printf("\n");

  if (fraction > 0.01) {
    fprintf(stderr, "key bias is too high (%f)\n", fraction);
    return false;
  }

  return true;
}

int main(void) {
  if (!TestKeys() ||
      !TestNoise()) {
    return 1;
  }
  printf("PASS\n");
  return 0;
}

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

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <openssl/crypto.h>
#include <openssl/rand.h>

#include "internal.h"


// Set to 10 for quick execution.  Tested up to 1,000,000.
static const int kNumTests = 10;

static bool TestKeys(void) {
  // Alice generates a public key.
  bssl::UniquePtr<NEWHOPE_POLY> sk(NEWHOPE_POLY_new());
  uint8_t offer_msg[NEWHOPE_OFFERMSG_LENGTH];
  NEWHOPE_offer(offer_msg, sk.get());

  // Bob derives a secret key and creates a response.
  uint8_t accept_msg[NEWHOPE_ACCEPTMSG_LENGTH];
  uint8_t accept_key[SHA256_DIGEST_LENGTH];
  if (!NEWHOPE_accept(accept_key, accept_msg, offer_msg, sizeof(offer_msg))) {
    fprintf(stderr, "ERROR accept key exchange failed\n");
    return false;
  }

  // Alice uses Bob's response to get her secret key.
  uint8_t offer_key[SHA256_DIGEST_LENGTH];
  if (!NEWHOPE_finish(offer_key, sk.get(), accept_msg, sizeof(accept_msg))) {
    fprintf(stderr, "ERROR finish key exchange failed\n");
    return false;
  }

  if (memcmp(offer_key, accept_key, SHA256_DIGEST_LENGTH) != 0) {
    fprintf(stderr, "ERROR keys did not agree\n");
    return false;
  }

  return true;
}

static bool TestInvalidSK(void) {
  // Alice generates a public key.
  uint8_t offer_msg[NEWHOPE_OFFERMSG_LENGTH];
  bssl::UniquePtr<NEWHOPE_POLY> sk(NEWHOPE_POLY_new());
  NEWHOPE_offer(offer_msg, sk.get());

  // Bob derives a secret key and creates a response.
  uint8_t accept_key[SHA256_DIGEST_LENGTH];
  uint8_t accept_msg[NEWHOPE_ACCEPTMSG_LENGTH];
  if (!NEWHOPE_accept(accept_key, accept_msg, offer_msg, sizeof(offer_msg))) {
    fprintf(stderr, "ERROR accept key exchange failed\n");
    return false;
  }

  // Corrupt the secret key.  It turns out that you need to corrupt a lot of
  // bits to ensure that the key exchange always fails!
  sk->coeffs[PARAM_N - 1] = 0;
  sk->coeffs[PARAM_N - 2] = 0;
  sk->coeffs[PARAM_N - 3] = 0;
  sk->coeffs[PARAM_N - 4] = 0;

  // Alice uses Bob's response to get her secret key.
  uint8_t offer_key[SHA256_DIGEST_LENGTH];
  if (!NEWHOPE_finish(offer_key, sk.get(), accept_msg, sizeof(accept_msg))) {
    fprintf(stderr, "ERROR finish key exchange failed\n");
    return false;
  }

  if (memcmp(offer_key, accept_key, SHA256_DIGEST_LENGTH) == 0) {
    fprintf(stderr, "ERROR keys agreed despite corrupt sk\n");
    return false;
  }

  return true;
}

static bool TestInvalidAcceptMsg(void) {
  // Alice generates a public key.
  bssl::UniquePtr<NEWHOPE_POLY> sk(NEWHOPE_POLY_new());
  uint8_t offer_msg[NEWHOPE_OFFERMSG_LENGTH];
  NEWHOPE_offer(offer_msg, sk.get());

  // Bob derives a secret key and creates a response.
  uint8_t accept_key[SHA256_DIGEST_LENGTH];
  uint8_t accept_msg[NEWHOPE_ACCEPTMSG_LENGTH];
  if (!NEWHOPE_accept(accept_key, accept_msg, offer_msg, sizeof(offer_msg))) {
    fprintf(stderr, "ERROR accept key exchange failed\n");
    return false;
  }

  // Corrupt the (polynomial part of the) accept message.  It turns out that
  // you need to corrupt a lot of bits to ensure that the key exchange always
  // fails!
  accept_msg[PARAM_N - 1] = 0;
  accept_msg[PARAM_N - 2] = 0;
  accept_msg[PARAM_N - 3] = 0;
  accept_msg[PARAM_N - 4] = 0;

  // Alice uses Bob's response to get her secret key.
  uint8_t offer_key[SHA256_DIGEST_LENGTH];
  if (!NEWHOPE_finish(offer_key, sk.get(), accept_msg, sizeof(accept_msg))) {
    fprintf(stderr, "ERROR finish key exchange failed\n");
    return false;
  }

  if (!memcmp(offer_key, accept_key, SHA256_DIGEST_LENGTH)) {
    fprintf(stderr, "ERROR keys agreed despite corrupt accept message\n");
    return false;
  }

  return true;
}

int main(void) {
  for (int i = 0; i < kNumTests; i++) {
    if (!TestKeys() ||
        !TestInvalidSK() ||
        !TestInvalidAcceptMsg()) {
      return 1;
    }
  }

  printf("PASS\n");
  return 0;
}

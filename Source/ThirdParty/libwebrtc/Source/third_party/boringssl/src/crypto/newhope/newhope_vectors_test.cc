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

#include "../test/file_test.h"
#include "internal.h"


static bool TestNewhope(FileTest *t, void *arg) {
  bssl::UniquePtr<NEWHOPE_POLY> a(NEWHOPE_POLY_new());
  bssl::UniquePtr<NEWHOPE_POLY> s(NEWHOPE_POLY_new()), sp(NEWHOPE_POLY_new());
  bssl::UniquePtr<NEWHOPE_POLY> e(NEWHOPE_POLY_new()), ep(NEWHOPE_POLY_new()),
      epp(NEWHOPE_POLY_new());
  bssl::UniquePtr<NEWHOPE_POLY> in_pk(NEWHOPE_POLY_new());
  bssl::UniquePtr<NEWHOPE_POLY> in_rec(NEWHOPE_POLY_new());

  if (t->GetType() == "InRandA") {
    std::vector<uint8_t> a_bytes, s_bytes, e_bytes, expected_pk;
    if (!t->GetBytes(&a_bytes, "InRandA") ||
        !t->GetBytes(&s_bytes, "InNoiseS") ||
        !t->GetBytes(&e_bytes, "InNoiseE") ||
        !t->GetBytes(&expected_pk, "OutPK")) {
      return false;
    }
    NEWHOPE_POLY_frombytes(a.get(), a_bytes.data());
    NEWHOPE_POLY_frombytes(s.get(), s_bytes.data());
    NEWHOPE_POLY_frombytes(e.get(), e_bytes.data());

    NEWHOPE_POLY pk;
    NEWHOPE_offer_computation(&pk, s.get(), e.get(), a.get());

    uint8_t pk_bytes[NEWHOPE_POLY_LENGTH];
    NEWHOPE_POLY_tobytes(pk_bytes, &pk);
    return t->ExpectBytesEqual(expected_pk.data(), expected_pk.size(),
                               pk_bytes, NEWHOPE_POLY_LENGTH);
  } else if (t->GetType() == "InPK") {
    std::vector<uint8_t> rand, in_pk_bytes, a_bytes, sp_bytes, ep_bytes,
        epp_bytes, expected_pk, expected_rec, expected_key;
    if (!t->GetBytes(&in_pk_bytes, "InPK") ||
        !t->GetBytes(&rand, "InRand") ||
        !t->GetBytes(&a_bytes, "InA") ||
        !t->GetBytes(&sp_bytes, "InNoiseSP") ||
        !t->GetBytes(&ep_bytes, "InNoiseEP") ||
        !t->GetBytes(&epp_bytes, "InNoiseEPP") ||
        !t->GetBytes(&expected_pk, "OutPK") ||
        !t->GetBytes(&expected_rec, "OutRec") ||
        !t->GetBytes(&expected_key, "Key")) {
      return false;
    }
    NEWHOPE_POLY_frombytes(in_pk.get(), in_pk_bytes.data());
    NEWHOPE_POLY_frombytes(a.get(), a_bytes.data());
    NEWHOPE_POLY_frombytes(sp.get(), sp_bytes.data());
    NEWHOPE_POLY_frombytes(ep.get(), ep_bytes.data());
    NEWHOPE_POLY_frombytes(epp.get(), epp_bytes.data());

    uint8_t key[NEWHOPE_KEY_LENGTH];
    NEWHOPE_POLY pk, rec;
    NEWHOPE_accept_computation(key, &pk, &rec,
                               sp.get(), ep.get(), epp.get(),
                               rand.data(), in_pk.get(), a.get());

    uint8_t pk_bytes[NEWHOPE_POLY_LENGTH], rec_bytes[NEWHOPE_POLY_LENGTH];
    NEWHOPE_POLY_tobytes(pk_bytes, &pk);
    NEWHOPE_POLY_tobytes(rec_bytes, &rec);
    return
        t->ExpectBytesEqual(expected_key.data(), expected_key.size(),
                            key, NEWHOPE_KEY_LENGTH) &&
        t->ExpectBytesEqual(expected_pk.data(), expected_pk.size(),
                            pk_bytes, NEWHOPE_POLY_LENGTH) &&
        t->ExpectBytesEqual(expected_rec.data(), expected_rec.size(),
                            rec_bytes, NEWHOPE_POLY_LENGTH);
  } else if (t->GetType() == "InNoiseS") {
    std::vector<uint8_t> s_bytes, in_pk_bytes, in_rec_bytes, expected_key;
    if (!t->GetBytes(&s_bytes, "InNoiseS") ||
        !t->GetBytes(&in_pk_bytes, "InPK") ||
        !t->GetBytes(&in_rec_bytes, "InRec") ||
        !t->GetBytes(&expected_key, "Key")) {
      return false;
    }
    NEWHOPE_POLY_frombytes(s.get(), s_bytes.data());
    NEWHOPE_POLY_frombytes(in_pk.get(), in_pk_bytes.data());
    NEWHOPE_POLY_frombytes(in_rec.get(), in_rec_bytes.data());

    uint8_t key[NEWHOPE_KEY_LENGTH];
    NEWHOPE_finish_computation(key, s.get(), in_pk.get(), in_rec.get());

    return t->ExpectBytesEqual(expected_key.data(), expected_key.size(), key,
                               NEWHOPE_KEY_LENGTH);
  } else {
    t->PrintLine("Unknown test '%s'", t->GetType().c_str());
    return false;
  }
}

int main(int argc, char **argv) {
  CRYPTO_library_init();

  if (argc != 2) {
    fprintf(stderr, "%s <test file>\n", argv[0]);
    return 1;
  }

  return FileTestMain(TestNewhope, nullptr, argv[1]);
}

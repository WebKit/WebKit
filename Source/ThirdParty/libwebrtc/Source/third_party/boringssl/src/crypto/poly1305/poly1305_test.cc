/* Copyright (c) 2015, Google Inc.
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

#include <stdio.h>
#include <string.h>

#include <vector>

#include <openssl/crypto.h>
#include <openssl/poly1305.h>

#include "../internal.h"
#include "../test/file_test.h"


static bool TestSIMD(FileTest *t, unsigned excess,
                     const std::vector<uint8_t> &key,
                     const std::vector<uint8_t> &in,
                     const std::vector<uint8_t> &mac) {
  poly1305_state state;
  CRYPTO_poly1305_init(&state, key.data());

  size_t done = 0;

  // Feed 16 bytes in. Some implementations begin in non-SIMD mode and upgrade
  // on-demand. Stress the upgrade path.
  size_t todo = 16;
  if (todo > in.size()) {
    todo = in.size();
  }
  CRYPTO_poly1305_update(&state, in.data(), todo);
  done += todo;

  for (;;) {
    // Feed 128 + |excess| bytes to test SIMD mode.
    if (done + 128 + excess > in.size()) {
      break;
    }
    CRYPTO_poly1305_update(&state, in.data() + done, 128 + excess);
    done += 128 + excess;

    // Feed |excess| bytes to ensure SIMD mode can handle short inputs.
    if (done + excess > in.size()) {
      break;
    }
    CRYPTO_poly1305_update(&state, in.data() + done, excess);
    done += excess;
  }

  // Consume the remainder and finish.
  CRYPTO_poly1305_update(&state, in.data() + done, in.size() - done);

  // |CRYPTO_poly1305_finish| requires a 16-byte-aligned output.
  alignas(16) uint8_t out[16];
  CRYPTO_poly1305_finish(&state, out);
  if (!t->ExpectBytesEqual(mac.data(), mac.size(), out, 16)) {
    t->PrintLine("SIMD pattern %u failed.", excess);
    return false;
  }

  return true;
}

static bool TestPoly1305(FileTest *t, void *arg) {
  std::vector<uint8_t> key, in, mac;
  if (!t->GetBytes(&key, "Key") ||
      !t->GetBytes(&in, "Input") ||
      !t->GetBytes(&mac, "MAC")) {
    return false;
  }
  if (key.size() != 32 || mac.size() != 16) {
    t->PrintLine("Invalid test");
    return false;
  }

  // Test single-shot operation.
  poly1305_state state;
  CRYPTO_poly1305_init(&state, key.data());
  CRYPTO_poly1305_update(&state, in.data(), in.size());
  // |CRYPTO_poly1305_finish| requires a 16-byte-aligned output.
  alignas(16) uint8_t out[16];
  CRYPTO_poly1305_finish(&state, out);
  if (!t->ExpectBytesEqual(out, 16, mac.data(), mac.size())) {
    t->PrintLine("Single-shot Poly1305 failed.");
    return false;
  }

  // Test streaming byte-by-byte.
  CRYPTO_poly1305_init(&state, key.data());
  for (size_t i = 0; i < in.size(); i++) {
    CRYPTO_poly1305_update(&state, &in[i], 1);
  }
  CRYPTO_poly1305_finish(&state, out);
  if (!t->ExpectBytesEqual(mac.data(), mac.size(), out, 16)) {
    t->PrintLine("Streaming Poly1305 failed.");
    return false;
  }

  // Test SIMD stress patterns. OpenSSL's AVX2 assembly needs a multiple of
  // four blocks, so test up to three blocks of excess.
  if (!TestSIMD(t, 0, key, in, mac) ||
      !TestSIMD(t, 16, key, in, mac) ||
      !TestSIMD(t, 32, key, in, mac) ||
      !TestSIMD(t, 48, key, in, mac)) {
    return false;
  }

  return true;
}

int main(int argc, char **argv) {
  CRYPTO_library_init();

  if (argc != 2) {
    fprintf(stderr, "%s <test file>\n", argv[0]);
    return 1;
  }

  return FileTestMain(TestPoly1305, nullptr, argv[1]);
}

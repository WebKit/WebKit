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

#include <stdio.h>
#include <string.h>

#include <openssl/pool.h>


static bool TestUnpooled() {
  static const uint8_t kData[4] = {1, 2, 3, 4};
  bssl::UniquePtr<CRYPTO_BUFFER> buf(
      CRYPTO_BUFFER_new(kData, sizeof(kData), nullptr));
  if (!buf) {
    return false;
  }

  if (CRYPTO_BUFFER_len(buf.get()) != sizeof(kData) ||
      memcmp(kData, CRYPTO_BUFFER_data(buf.get()), sizeof(kData)) != 0) {
    fprintf(stderr, "CRYPTO_BUFFER corrupted data.\n");
    return false;
  }

  CRYPTO_BUFFER_up_ref(buf.get());
  bssl::UniquePtr<CRYPTO_BUFFER> buf2(buf.get());

  return true;
}

static bool TestEmpty() {
  bssl::UniquePtr<CRYPTO_BUFFER> buf(CRYPTO_BUFFER_new(nullptr, 0, nullptr));
  if (!buf) {
    return false;
  }

  return true;
}

static bool TestPool() {
  bssl::UniquePtr<CRYPTO_BUFFER_POOL> pool(CRYPTO_BUFFER_POOL_new());
  if (!pool) {
    return false;
  }

  static const uint8_t kData[4] = {1, 2, 3, 4};
  bssl::UniquePtr<CRYPTO_BUFFER> buf(
      CRYPTO_BUFFER_new(kData, sizeof(kData), pool.get()));
  if (!buf) {
    return false;
  }

  bssl::UniquePtr<CRYPTO_BUFFER> buf2(
      CRYPTO_BUFFER_new(kData, sizeof(kData), pool.get()));
  if (!buf2) {
    return false;
  }

  if (buf.get() != buf2.get()) {
    fprintf(stderr, "CRYPTO_BUFFER_POOL did not dedup data.\n");
    return false;
  }

  return true;
}

int main(int argc, char **argv) {
  if (!TestUnpooled() ||
      !TestEmpty() ||
      !TestPool()) {
    return 1;
  }

  printf("PASS\n");
  return 0;
}

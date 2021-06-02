/* Copyright (c) 2020, Google Inc.
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

#include <openssl/base.h>
#include "../fipsmodule/rand/internal.h"

#if defined(BORINGSSL_FIPS)

// RAND_need_entropy is called by the FIPS module when it has blocked because of
// a lack of entropy. This signal is used as an indication to feed it more.
void RAND_need_entropy(size_t bytes_needed) {
  uint8_t buf[CTR_DRBG_ENTROPY_LEN * BORINGSSL_FIPS_OVERREAD];
  size_t todo = sizeof(buf);
  if (todo > bytes_needed) {
    todo = bytes_needed;
  }

  int used_cpu;
  CRYPTO_get_seed_entropy(buf, todo, &used_cpu);
  RAND_load_entropy(buf, todo, used_cpu);
}

#endif  // FIPS

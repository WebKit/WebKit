/* Copyright (c) 2014, Google Inc.
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

#include <openssl/rand.h>

#if defined(OPENSSL_WINDOWS) && !defined(BORINGSSL_UNSAFE_DETERMINISTIC_MODE)

#include <limits.h>
#include <stdlib.h>

OPENSSL_MSVC_PRAGMA(warning(push, 3))

#include <windows.h>

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP) && \
    !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#include <bcrypt.h>
OPENSSL_MSVC_PRAGMA(comment(lib, "bcrypt.lib"))
#else
// #define needed to link in RtlGenRandom(), a.k.a. SystemFunction036.  See the
// "Community Additions" comment on MSDN here:
// http://msdn.microsoft.com/en-us/library/windows/desktop/aa387694.aspx
#define SystemFunction036 NTAPI SystemFunction036
#include <ntsecapi.h>
#undef SystemFunction036
#endif  // WINAPI_PARTITION_APP && !WINAPI_PARTITION_DESKTOP

OPENSSL_MSVC_PRAGMA(warning(pop))

#include "../fipsmodule/rand/internal.h"


void CRYPTO_sysrand(uint8_t *out, size_t requested) {
  while (requested > 0) {
    ULONG output_bytes_this_pass = ULONG_MAX;
    if (requested < output_bytes_this_pass) {
      output_bytes_this_pass = (ULONG)requested;
    }
    // On non-UWP configurations, use RtlGenRandom instead of BCryptGenRandom
    // to avoid accessing resources that may be unavailable inside the
    // Chromium sandbox. See https://crbug.com/boringssl/307
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP) && \
    !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    if (!BCRYPT_SUCCESS(BCryptGenRandom(
            /*hAlgorithm=*/NULL, out, output_bytes_this_pass,
            BCRYPT_USE_SYSTEM_PREFERRED_RNG))) {
#else
    if (RtlGenRandom(out, output_bytes_this_pass) == FALSE) {
#endif  // WINAPI_PARTITION_APP && !WINAPI_PARTITION_DESKTOP
      abort();
    }
    requested -= output_bytes_this_pass;
    out += output_bytes_this_pass;
  }
  return;
}

#endif  // OPENSSL_WINDOWS && !BORINGSSL_UNSAFE_DETERMINISTIC_MODE

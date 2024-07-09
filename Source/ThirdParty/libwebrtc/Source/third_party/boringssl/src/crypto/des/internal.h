/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.] */

#ifndef OPENSSL_HEADER_DES_INTERNAL_H
#define OPENSSL_HEADER_DES_INTERNAL_H

#include <openssl/base.h>
#include <openssl/des.h>

#include "../internal.h"

#if defined(__cplusplus)
extern "C" {
#endif


// TODO(davidben): Ideally these macros would be replaced with
// |CRYPTO_load_u32_le| and |CRYPTO_store_u32_le|.

#define c2l(c, l)                         \
  do {                                    \
    (l) = ((uint32_t)(*((c)++)));         \
    (l) |= ((uint32_t)(*((c)++))) << 8L;  \
    (l) |= ((uint32_t)(*((c)++))) << 16L; \
    (l) |= ((uint32_t)(*((c)++))) << 24L; \
  } while (0)

#define l2c(l, c)                                    \
  do {                                               \
    *((c)++) = (unsigned char)(((l)) & 0xff);        \
    *((c)++) = (unsigned char)(((l) >> 8L) & 0xff);  \
    *((c)++) = (unsigned char)(((l) >> 16L) & 0xff); \
    *((c)++) = (unsigned char)(((l) >> 24L) & 0xff); \
  } while (0)

// NOTE - c is not incremented as per c2l
#define c2ln(c, l1, l2, n)                     \
  do {                                         \
    (c) += (n);                                \
    (l1) = (l2) = 0;                           \
    switch (n) {                               \
      case 8:                                  \
        (l2) = ((uint32_t)(*(--(c)))) << 24L;  \
        OPENSSL_FALLTHROUGH;                   \
      case 7:                                  \
        (l2) |= ((uint32_t)(*(--(c)))) << 16L; \
        OPENSSL_FALLTHROUGH;                   \
      case 6:                                  \
        (l2) |= ((uint32_t)(*(--(c)))) << 8L;  \
        OPENSSL_FALLTHROUGH;                   \
      case 5:                                  \
        (l2) |= ((uint32_t)(*(--(c))));        \
        OPENSSL_FALLTHROUGH;                   \
      case 4:                                  \
        (l1) = ((uint32_t)(*(--(c)))) << 24L;  \
        OPENSSL_FALLTHROUGH;                   \
      case 3:                                  \
        (l1) |= ((uint32_t)(*(--(c)))) << 16L; \
        OPENSSL_FALLTHROUGH;                   \
      case 2:                                  \
        (l1) |= ((uint32_t)(*(--(c)))) << 8L;  \
        OPENSSL_FALLTHROUGH;                   \
      case 1:                                  \
        (l1) |= ((uint32_t)(*(--(c))));        \
    }                                          \
  } while (0)

// NOTE - c is not incremented as per l2c
#define l2cn(l1, l2, c, n)                                \
  do {                                                    \
    (c) += (n);                                           \
    switch (n) {                                          \
      case 8:                                             \
        *(--(c)) = (unsigned char)(((l2) >> 24L) & 0xff); \
        OPENSSL_FALLTHROUGH;                              \
      case 7:                                             \
        *(--(c)) = (unsigned char)(((l2) >> 16L) & 0xff); \
        OPENSSL_FALLTHROUGH;                              \
      case 6:                                             \
        *(--(c)) = (unsigned char)(((l2) >> 8L) & 0xff);  \
        OPENSSL_FALLTHROUGH;                              \
      case 5:                                             \
        *(--(c)) = (unsigned char)(((l2)) & 0xff);        \
        OPENSSL_FALLTHROUGH;                              \
      case 4:                                             \
        *(--(c)) = (unsigned char)(((l1) >> 24L) & 0xff); \
        OPENSSL_FALLTHROUGH;                              \
      case 3:                                             \
        *(--(c)) = (unsigned char)(((l1) >> 16L) & 0xff); \
        OPENSSL_FALLTHROUGH;                              \
      case 2:                                             \
        *(--(c)) = (unsigned char)(((l1) >> 8L) & 0xff);  \
        OPENSSL_FALLTHROUGH;                              \
      case 1:                                             \
        *(--(c)) = (unsigned char)(((l1)) & 0xff);        \
    }                                                     \
  } while (0)


// Correctly-typed versions of DES functions.
//
// See https://crbug.com/boringssl/683.

void DES_set_key_ex(const uint8_t key[8], DES_key_schedule *schedule);
void DES_ecb_encrypt_ex(const uint8_t in[8], uint8_t out[8],
                        const DES_key_schedule *schedule, int is_encrypt);
void DES_ncbc_encrypt_ex(const uint8_t *in, uint8_t *out, size_t len,
                         const DES_key_schedule *schedule, uint8_t ivec[8],
                         int enc);
void DES_ecb3_encrypt_ex(const uint8_t input[8], uint8_t output[8],
                         const DES_key_schedule *ks1,
                         const DES_key_schedule *ks2,
                         const DES_key_schedule *ks3, int enc);
void DES_ede3_cbc_encrypt_ex(const uint8_t *in, uint8_t *out, size_t len,
                             const DES_key_schedule *ks1,
                             const DES_key_schedule *ks2,
                             const DES_key_schedule *ks3, uint8_t ivec[8],
                             int enc);


// Private functions.
//
// These functions are only exported for use in |decrepit|.

OPENSSL_EXPORT void DES_decrypt3(uint32_t data[2], const DES_key_schedule *ks1,
                                 const DES_key_schedule *ks2,
                                 const DES_key_schedule *ks3);

OPENSSL_EXPORT void DES_encrypt3(uint32_t data[2], const DES_key_schedule *ks1,
                                 const DES_key_schedule *ks2,
                                 const DES_key_schedule *ks3);


#if defined(__cplusplus)
}  // extern C
#endif

#endif  // OPENSSL_HEADER_DES_INTERNAL_H

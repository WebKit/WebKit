/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2005.
 */
/* ====================================================================
 * Copyright (c) 2005 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com). */

#include <openssl/rsa.h>

#include <assert.h>
#include <limits.h>
#include <string.h>

#include <openssl/bn.h>
#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "internal.h"
#include "../internal.h"

/* TODO(fork): don't the check functions have to be constant time? */

int RSA_padding_add_PKCS1_type_1(uint8_t *to, unsigned to_len,
                                 const uint8_t *from, unsigned from_len) {
  unsigned j;

  if (to_len < RSA_PKCS1_PADDING_SIZE) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_KEY_SIZE_TOO_SMALL);
    return 0;
  }

  if (from_len > to_len - RSA_PKCS1_PADDING_SIZE) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE);
    return 0;
  }

  uint8_t *p = to;

  *(p++) = 0;
  *(p++) = 1; /* Private Key BT (Block Type) */

  /* pad out with 0xff data */
  j = to_len - 3 - from_len;
  memset(p, 0xff, j);
  p += j;
  *(p++) = 0;
  memcpy(p, from, from_len);
  return 1;
}

int RSA_padding_check_PKCS1_type_1(uint8_t *to, unsigned to_len,
                                   const uint8_t *from, unsigned from_len) {
  unsigned i, j;
  const uint8_t *p;

  if (from_len < 2) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_DATA_TOO_SMALL);
    return -1;
  }

  p = from;
  if ((*(p++) != 0) || (*(p++) != 1)) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_BLOCK_TYPE_IS_NOT_01);
    return -1;
  }

  /* scan over padding data */
  j = from_len - 2; /* one for leading 00, one for type. */
  for (i = 0; i < j; i++) {
    /* should decrypt to 0xff */
    if (*p != 0xff) {
      if (*p == 0) {
        p++;
        break;
      } else {
        OPENSSL_PUT_ERROR(RSA, RSA_R_BAD_FIXED_HEADER_DECRYPT);
        return -1;
      }
    }
    p++;
  }

  if (i == j) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_NULL_BEFORE_BLOCK_MISSING);
    return -1;
  }

  if (i < 8) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_BAD_PAD_BYTE_COUNT);
    return -1;
  }
  i++; /* Skip over the '\0' */
  j -= i;
  if (j > to_len) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_DATA_TOO_LARGE);
    return -1;
  }
  memcpy(to, p, j);

  return j;
}

int RSA_padding_add_PKCS1_type_2(uint8_t *to, unsigned to_len,
                                 const uint8_t *from, unsigned from_len) {
  unsigned i, j;

  if (to_len < RSA_PKCS1_PADDING_SIZE) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_KEY_SIZE_TOO_SMALL);
    return 0;
  }

  if (from_len > to_len - RSA_PKCS1_PADDING_SIZE) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE);
    return 0;
  }

  uint8_t *p = to;

  *(p++) = 0;
  *(p++) = 2; /* Public Key BT (Block Type) */

  /* pad out with non-zero random data */
  j = to_len - 3 - from_len;

  if (!RAND_bytes(p, j)) {
    return 0;
  }

  for (i = 0; i < j; i++) {
    while (*p == 0) {
      if (!RAND_bytes(p, 1)) {
        return 0;
      }
    }
    p++;
  }

  *(p++) = 0;

  memcpy(p, from, from_len);
  return 1;
}

int RSA_padding_check_PKCS1_type_2(uint8_t *to, unsigned to_len,
                                   const uint8_t *from, unsigned from_len) {
  if (from_len == 0) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_EMPTY_PUBLIC_KEY);
    return -1;
  }

  /* PKCS#1 v1.5 decryption. See "PKCS #1 v2.2: RSA Cryptography
   * Standard", section 7.2.2. */
  if (from_len < RSA_PKCS1_PADDING_SIZE) {
    /* |from| is zero-padded to the size of the RSA modulus, a public value, so
     * this can be rejected in non-constant time. */
    OPENSSL_PUT_ERROR(RSA, RSA_R_KEY_SIZE_TOO_SMALL);
    return -1;
  }

  unsigned first_byte_is_zero = constant_time_eq(from[0], 0);
  unsigned second_byte_is_two = constant_time_eq(from[1], 2);

  unsigned i, zero_index = 0, looking_for_index = ~0u;
  for (i = 2; i < from_len; i++) {
    unsigned equals0 = constant_time_is_zero(from[i]);
    zero_index = constant_time_select(looking_for_index & equals0, (unsigned)i,
                                      zero_index);
    looking_for_index = constant_time_select(equals0, 0, looking_for_index);
  }

  /* The input must begin with 00 02. */
  unsigned valid_index = first_byte_is_zero;
  valid_index &= second_byte_is_two;

  /* We must have found the end of PS. */
  valid_index &= ~looking_for_index;

  /* PS must be at least 8 bytes long, and it starts two bytes into |from|. */
  valid_index &= constant_time_ge(zero_index, 2 + 8);

  /* Skip the zero byte. */
  zero_index++;

  /* NOTE: Although this logic attempts to be constant time, the API contracts
   * of this function and |RSA_decrypt| with |RSA_PKCS1_PADDING| make it
   * impossible to completely avoid Bleichenbacher's attack. Consumers should
   * use |RSA_unpad_key_pkcs1|. */
  if (!valid_index) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_PKCS_DECODING_ERROR);
    return -1;
  }

  const unsigned msg_len = from_len - zero_index;
  if (msg_len > to_len) {
    /* This shouldn't happen because this function is always called with
     * |to_len| as the key size and |from_len| is bounded by the key size. */
    OPENSSL_PUT_ERROR(RSA, RSA_R_PKCS_DECODING_ERROR);
    return -1;
  }

  if (msg_len > INT_MAX) {
    OPENSSL_PUT_ERROR(RSA, ERR_R_OVERFLOW);
    return -1;
  }

  memcpy(to, &from[zero_index], msg_len);
  return (int)msg_len;
}

int RSA_padding_add_none(uint8_t *to, unsigned to_len, const uint8_t *from,
                         unsigned from_len) {
  if (from_len > to_len) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE);
    return 0;
  }

  if (from_len < to_len) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_DATA_TOO_SMALL_FOR_KEY_SIZE);
    return 0;
  }

  memcpy(to, from, from_len);
  return 1;
}

static int PKCS1_MGF1(uint8_t *out, size_t len, const uint8_t *seed,
                      size_t seed_len, const EVP_MD *md) {
  int ret = 0;
  EVP_MD_CTX ctx;
  EVP_MD_CTX_init(&ctx);

  size_t md_len = EVP_MD_size(md);

  for (uint32_t i = 0; len > 0; i++) {
    uint8_t counter[4];
    counter[0] = (uint8_t)(i >> 24);
    counter[1] = (uint8_t)(i >> 16);
    counter[2] = (uint8_t)(i >> 8);
    counter[3] = (uint8_t)i;
    if (!EVP_DigestInit_ex(&ctx, md, NULL) ||
        !EVP_DigestUpdate(&ctx, seed, seed_len) ||
        !EVP_DigestUpdate(&ctx, counter, sizeof(counter))) {
      goto err;
    }

    if (md_len <= len) {
      if (!EVP_DigestFinal_ex(&ctx, out, NULL)) {
        goto err;
      }
      out += md_len;
      len -= md_len;
    } else {
      uint8_t digest[EVP_MAX_MD_SIZE];
      if (!EVP_DigestFinal_ex(&ctx, digest, NULL)) {
        goto err;
      }
      memcpy(out, digest, len);
      len = 0;
    }
  }

  ret = 1;

err:
  EVP_MD_CTX_cleanup(&ctx);
  return ret;
}

int RSA_padding_add_PKCS1_OAEP_mgf1(uint8_t *to, unsigned to_len,
                                    const uint8_t *from, unsigned from_len,
                                    const uint8_t *param, unsigned param_len,
                                    const EVP_MD *md, const EVP_MD *mgf1md) {
  unsigned i, emlen, mdlen;
  uint8_t *db, *seed;
  uint8_t *dbmask = NULL, seedmask[EVP_MAX_MD_SIZE];
  int ret = 0;

  if (md == NULL) {
    md = EVP_sha1();
  }
  if (mgf1md == NULL) {
    mgf1md = md;
  }

  mdlen = EVP_MD_size(md);

  if (to_len < 2 * mdlen + 2) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_KEY_SIZE_TOO_SMALL);
    return 0;
  }

  emlen = to_len - 1;
  if (from_len > emlen - 2 * mdlen - 1) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE);
    return 0;
  }

  if (emlen < 2 * mdlen + 1) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_KEY_SIZE_TOO_SMALL);
    return 0;
  }

  to[0] = 0;
  seed = to + 1;
  db = to + mdlen + 1;

  if (!EVP_Digest(param, param_len, db, NULL, md, NULL)) {
    return 0;
  }
  memset(db + mdlen, 0, emlen - from_len - 2 * mdlen - 1);
  db[emlen - from_len - mdlen - 1] = 0x01;
  memcpy(db + emlen - from_len - mdlen, from, from_len);
  if (!RAND_bytes(seed, mdlen)) {
    return 0;
  }

  dbmask = OPENSSL_malloc(emlen - mdlen);
  if (dbmask == NULL) {
    OPENSSL_PUT_ERROR(RSA, ERR_R_MALLOC_FAILURE);
    return 0;
  }

  if (!PKCS1_MGF1(dbmask, emlen - mdlen, seed, mdlen, mgf1md)) {
    goto out;
  }
  for (i = 0; i < emlen - mdlen; i++) {
    db[i] ^= dbmask[i];
  }

  if (!PKCS1_MGF1(seedmask, mdlen, db, emlen - mdlen, mgf1md)) {
    goto out;
  }
  for (i = 0; i < mdlen; i++) {
    seed[i] ^= seedmask[i];
  }
  ret = 1;

out:
  OPENSSL_free(dbmask);
  return ret;
}

int RSA_padding_check_PKCS1_OAEP_mgf1(uint8_t *to, unsigned to_len,
                                      const uint8_t *from, unsigned from_len,
                                      const uint8_t *param, unsigned param_len,
                                      const EVP_MD *md, const EVP_MD *mgf1md) {
  unsigned i, dblen, mlen = -1, mdlen, bad, looking_for_one_byte, one_index = 0;
  const uint8_t *maskeddb, *maskedseed;
  uint8_t *db = NULL, seed[EVP_MAX_MD_SIZE], phash[EVP_MAX_MD_SIZE];

  if (md == NULL) {
    md = EVP_sha1();
  }
  if (mgf1md == NULL) {
    mgf1md = md;
  }

  mdlen = EVP_MD_size(md);

  /* The encoded message is one byte smaller than the modulus to ensure that it
   * doesn't end up greater than the modulus. Thus there's an extra "+1" here
   * compared to https://tools.ietf.org/html/rfc2437#section-9.1.1.2. */
  if (from_len < 1 + 2*mdlen + 1) {
    /* 'from_len' is the length of the modulus, i.e. does not depend on the
     * particular ciphertext. */
    goto decoding_err;
  }

  dblen = from_len - mdlen - 1;
  db = OPENSSL_malloc(dblen);
  if (db == NULL) {
    OPENSSL_PUT_ERROR(RSA, ERR_R_MALLOC_FAILURE);
    goto err;
  }

  maskedseed = from + 1;
  maskeddb = from + 1 + mdlen;

  if (!PKCS1_MGF1(seed, mdlen, maskeddb, dblen, mgf1md)) {
    goto err;
  }
  for (i = 0; i < mdlen; i++) {
    seed[i] ^= maskedseed[i];
  }

  if (!PKCS1_MGF1(db, dblen, seed, mdlen, mgf1md)) {
    goto err;
  }
  for (i = 0; i < dblen; i++) {
    db[i] ^= maskeddb[i];
  }

  if (!EVP_Digest(param, param_len, phash, NULL, md, NULL)) {
    goto err;
  }

  bad = ~constant_time_is_zero(CRYPTO_memcmp(db, phash, mdlen));
  bad |= ~constant_time_is_zero(from[0]);

  looking_for_one_byte = ~0u;
  for (i = mdlen; i < dblen; i++) {
    unsigned equals1 = constant_time_eq(db[i], 1);
    unsigned equals0 = constant_time_eq(db[i], 0);
    one_index = constant_time_select(looking_for_one_byte & equals1, i,
                                     one_index);
    looking_for_one_byte =
        constant_time_select(equals1, 0, looking_for_one_byte);
    bad |= looking_for_one_byte & ~equals0;
  }

  bad |= looking_for_one_byte;

  if (bad) {
    goto decoding_err;
  }

  one_index++;
  mlen = dblen - one_index;
  if (to_len < mlen) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_DATA_TOO_LARGE);
    mlen = -1;
  } else {
    memcpy(to, db + one_index, mlen);
  }

  OPENSSL_free(db);
  return mlen;

decoding_err:
  /* to avoid chosen ciphertext attacks, the error message should not reveal
   * which kind of decoding error happened */
  OPENSSL_PUT_ERROR(RSA, RSA_R_OAEP_DECODING_ERROR);
 err:
  OPENSSL_free(db);
  return -1;
}

static const unsigned char zeroes[] = {0,0,0,0,0,0,0,0};

int RSA_verify_PKCS1_PSS_mgf1(RSA *rsa, const uint8_t *mHash,
                              const EVP_MD *Hash, const EVP_MD *mgf1Hash,
                              const uint8_t *EM, int sLen) {
  int i;
  int ret = 0;
  int maskedDBLen, MSBits, emLen;
  size_t hLen;
  const uint8_t *H;
  uint8_t *DB = NULL;
  EVP_MD_CTX ctx;
  uint8_t H_[EVP_MAX_MD_SIZE];
  EVP_MD_CTX_init(&ctx);

  if (mgf1Hash == NULL) {
    mgf1Hash = Hash;
  }

  hLen = EVP_MD_size(Hash);

  /* Negative sLen has special meanings:
   *	-1	sLen == hLen
   *	-2	salt length is autorecovered from signature
   *	-N	reserved */
  if (sLen == -1) {
    sLen = hLen;
  } else if (sLen == -2) {
    sLen = -2;
  } else if (sLen < -2) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_SLEN_CHECK_FAILED);
    goto err;
  }

  MSBits = (BN_num_bits(rsa->n) - 1) & 0x7;
  emLen = RSA_size(rsa);
  if (EM[0] & (0xFF << MSBits)) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_FIRST_OCTET_INVALID);
    goto err;
  }
  if (MSBits == 0) {
    EM++;
    emLen--;
  }
  if (emLen < ((int)hLen + sLen + 2)) {
    /* sLen can be small negative */
    OPENSSL_PUT_ERROR(RSA, RSA_R_DATA_TOO_LARGE);
    goto err;
  }
  if (EM[emLen - 1] != 0xbc) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_LAST_OCTET_INVALID);
    goto err;
  }
  maskedDBLen = emLen - hLen - 1;
  H = EM + maskedDBLen;
  DB = OPENSSL_malloc(maskedDBLen);
  if (!DB) {
    OPENSSL_PUT_ERROR(RSA, ERR_R_MALLOC_FAILURE);
    goto err;
  }
  if (!PKCS1_MGF1(DB, maskedDBLen, H, hLen, mgf1Hash)) {
    goto err;
  }
  for (i = 0; i < maskedDBLen; i++) {
    DB[i] ^= EM[i];
  }
  if (MSBits) {
    DB[0] &= 0xFF >> (8 - MSBits);
  }
  for (i = 0; DB[i] == 0 && i < (maskedDBLen - 1); i++) {
    ;
  }
  if (DB[i++] != 0x1) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_SLEN_RECOVERY_FAILED);
    goto err;
  }
  if (sLen >= 0 && (maskedDBLen - i) != sLen) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_SLEN_CHECK_FAILED);
    goto err;
  }
  if (!EVP_DigestInit_ex(&ctx, Hash, NULL) ||
      !EVP_DigestUpdate(&ctx, zeroes, sizeof zeroes) ||
      !EVP_DigestUpdate(&ctx, mHash, hLen)) {
    goto err;
  }
  if (maskedDBLen - i) {
    if (!EVP_DigestUpdate(&ctx, DB + i, maskedDBLen - i)) {
      goto err;
    }
  }
  if (!EVP_DigestFinal_ex(&ctx, H_, NULL)) {
    goto err;
  }
  if (memcmp(H_, H, hLen)) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_BAD_SIGNATURE);
    ret = 0;
  } else {
    ret = 1;
  }

err:
  OPENSSL_free(DB);
  EVP_MD_CTX_cleanup(&ctx);

  return ret;
}

int RSA_padding_add_PKCS1_PSS_mgf1(RSA *rsa, unsigned char *EM,
                                   const unsigned char *mHash,
                                   const EVP_MD *Hash, const EVP_MD *mgf1Hash,
                                   int sLenRequested) {
  int ret = 0;
  size_t maskedDBLen, MSBits, emLen;
  size_t hLen;
  unsigned char *H, *salt = NULL, *p;
  EVP_MD_CTX ctx;

  if (mgf1Hash == NULL) {
    mgf1Hash = Hash;
  }

  hLen = EVP_MD_size(Hash);

  if (BN_is_zero(rsa->n)) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_EMPTY_PUBLIC_KEY);
    goto err;
  }

  MSBits = (BN_num_bits(rsa->n) - 1) & 0x7;
  emLen = RSA_size(rsa);
  if (MSBits == 0) {
    assert(emLen >= 1);
    *EM++ = 0;
    emLen--;
  }

  if (emLen < hLen + 2) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE);
    goto err;
  }

  /* Negative sLenRequested has special meanings:
   *   -1  sLen == hLen
   *   -2  salt length is maximized
   *   -N  reserved */
  size_t sLen;
  if (sLenRequested == -1) {
    sLen = hLen;
  } else if (sLenRequested == -2) {
    sLen = emLen - hLen - 2;
  } else if (sLenRequested < 0) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_SLEN_CHECK_FAILED);
    goto err;
  } else {
    sLen = (size_t)sLenRequested;
  }

  if (emLen - hLen - 2 < sLen) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE);
    goto err;
  }

  if (sLen > 0) {
    salt = OPENSSL_malloc(sLen);
    if (!salt) {
      OPENSSL_PUT_ERROR(RSA, ERR_R_MALLOC_FAILURE);
      goto err;
    }
    if (!RAND_bytes(salt, sLen)) {
      goto err;
    }
  }
  maskedDBLen = emLen - hLen - 1;
  H = EM + maskedDBLen;
  EVP_MD_CTX_init(&ctx);
  if (!EVP_DigestInit_ex(&ctx, Hash, NULL) ||
      !EVP_DigestUpdate(&ctx, zeroes, sizeof zeroes) ||
      !EVP_DigestUpdate(&ctx, mHash, hLen)) {
    goto err;
  }
  if (sLen && !EVP_DigestUpdate(&ctx, salt, sLen)) {
    goto err;
  }
  if (!EVP_DigestFinal_ex(&ctx, H, NULL)) {
    goto err;
  }
  EVP_MD_CTX_cleanup(&ctx);

  /* Generate dbMask in place then perform XOR on it */
  if (!PKCS1_MGF1(EM, maskedDBLen, H, hLen, mgf1Hash)) {
    goto err;
  }

  p = EM;

  /* Initial PS XORs with all zeroes which is a NOP so just update
   * pointer. Note from a test above this value is guaranteed to
   * be non-negative. */
  p += emLen - sLen - hLen - 2;
  *p++ ^= 0x1;
  if (sLen > 0) {
    for (size_t i = 0; i < sLen; i++) {
      *p++ ^= salt[i];
    }
  }
  if (MSBits) {
    EM[0] &= 0xFF >> (8 - MSBits);
  }

  /* H is already in place so just set final 0xbc */

  EM[emLen - 1] = 0xbc;

  ret = 1;

err:
  OPENSSL_free(salt);

  return ret;
}

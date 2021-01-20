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

#include <assert.h>
#include <string.h>

#include <openssl/aead.h>
#include <openssl/bytestring.h>
#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hkdf.h>
#include <openssl/sha.h>

#include "../internal.h"
#include "internal.h"


// This file implements draft-irtf-cfrg-hpke-05.

#define KEM_CONTEXT_LEN (2 * X25519_PUBLIC_VALUE_LEN)

// HPKE KEM scheme IDs.
#define HPKE_DHKEM_X25519_HKDF_SHA256 0x0020

// This is strlen("HPKE") + 3 * sizeof(uint16_t).
#define HPKE_SUITE_ID_LEN 10

#define HPKE_MODE_BASE 0
#define HPKE_MODE_PSK 1

static const char kHpkeRfcId[] = "HPKE-05 ";

static int add_label_string(CBB *cbb, const char *label) {
  return CBB_add_bytes(cbb, (const uint8_t *)label, strlen(label));
}

// The suite_id for the KEM is defined as concat("KEM", I2OSP(kem_id, 2)). Note
// that the suite_id used outside of the KEM also includes the kdf_id and
// aead_id.
static const uint8_t kX25519SuiteID[] = {
    'K', 'E', 'M', HPKE_DHKEM_X25519_HKDF_SHA256 >> 8,
    HPKE_DHKEM_X25519_HKDF_SHA256 & 0x00ff};

// The suite_id for non-KEM pieces of HPKE is defined as concat("HPKE",
// I2OSP(kem_id, 2), I2OSP(kdf_id, 2), I2OSP(aead_id, 2)).
static int hpke_build_suite_id(uint8_t out[HPKE_SUITE_ID_LEN], uint16_t kdf_id,
                               uint16_t aead_id) {
  CBB cbb;
  int ret = CBB_init_fixed(&cbb, out, HPKE_SUITE_ID_LEN) &&
            add_label_string(&cbb, "HPKE") &&
            CBB_add_u16(&cbb, HPKE_DHKEM_X25519_HKDF_SHA256) &&
            CBB_add_u16(&cbb, kdf_id) &&
            CBB_add_u16(&cbb, aead_id);
  CBB_cleanup(&cbb);
  return ret;
}

static int hpke_labeled_extract(const EVP_MD *hkdf_md, uint8_t *out_key,
                                size_t *out_len, const uint8_t *salt,
                                size_t salt_len, const uint8_t *suite_id,
                                size_t suite_id_len, const char *label,
                                const uint8_t *ikm, size_t ikm_len) {
  // labeledIKM = concat("RFCXXXX ", suite_id, label, IKM)
  CBB labeled_ikm;
  int ok = CBB_init(&labeled_ikm, 0) &&
           add_label_string(&labeled_ikm, kHpkeRfcId) &&
           CBB_add_bytes(&labeled_ikm, suite_id, suite_id_len) &&
           add_label_string(&labeled_ikm, label) &&
           CBB_add_bytes(&labeled_ikm, ikm, ikm_len) &&
           HKDF_extract(out_key, out_len, hkdf_md, CBB_data(&labeled_ikm),
                        CBB_len(&labeled_ikm), salt, salt_len);
  CBB_cleanup(&labeled_ikm);
  return ok;
}

static int hpke_labeled_expand(const EVP_MD *hkdf_md, uint8_t *out_key,
                               size_t out_len, const uint8_t *prk,
                               size_t prk_len, const uint8_t *suite_id,
                               size_t suite_id_len, const char *label,
                               const uint8_t *info, size_t info_len) {
  // labeledInfo = concat(I2OSP(L, 2), "RFCXXXX ", suite_id, label, info)
  CBB labeled_info;
  int ok = CBB_init(&labeled_info, 0) &&
           CBB_add_u16(&labeled_info, out_len) &&
           add_label_string(&labeled_info, kHpkeRfcId) &&
           CBB_add_bytes(&labeled_info, suite_id, suite_id_len) &&
           add_label_string(&labeled_info, label) &&
           CBB_add_bytes(&labeled_info, info, info_len) &&
           HKDF_expand(out_key, out_len, hkdf_md, prk, prk_len,
                       CBB_data(&labeled_info), CBB_len(&labeled_info));
  CBB_cleanup(&labeled_info);
  return ok;
}

static int hpke_extract_and_expand(const EVP_MD *hkdf_md, uint8_t *out_key,
                                   size_t out_len,
                                   const uint8_t dh[X25519_PUBLIC_VALUE_LEN],
                                   const uint8_t kem_context[KEM_CONTEXT_LEN]) {
  uint8_t prk[EVP_MAX_MD_SIZE];
  size_t prk_len;
  static const char kEaePrkLabel[] = "eae_prk";
  if (!hpke_labeled_extract(hkdf_md, prk, &prk_len, NULL, 0, kX25519SuiteID,
                            sizeof(kX25519SuiteID), kEaePrkLabel, dh,
                            X25519_PUBLIC_VALUE_LEN)) {
    return 0;
  }
  static const char kPRKExpandLabel[] = "shared_secret";
  if (!hpke_labeled_expand(hkdf_md, out_key, out_len, prk, prk_len,
                           kX25519SuiteID, sizeof(kX25519SuiteID),
                           kPRKExpandLabel, kem_context, KEM_CONTEXT_LEN)) {
    return 0;
  }
  return 1;
}

static const EVP_AEAD *hpke_get_aead(uint16_t aead_id) {
  switch (aead_id) {
    case EVP_HPKE_AEAD_AES_GCM_128:
      return EVP_aead_aes_128_gcm();
    case EVP_HPKE_AEAD_AES_GCM_256:
      return EVP_aead_aes_256_gcm();
    case EVP_HPKE_AEAD_CHACHA20POLY1305:
      return EVP_aead_chacha20_poly1305();
  }
  OPENSSL_PUT_ERROR(EVP, ERR_R_INTERNAL_ERROR);
  return NULL;
}

static const EVP_MD *hpke_get_kdf(uint16_t kdf_id) {
  switch (kdf_id) {
    case EVP_HPKE_HKDF_SHA256:
      return EVP_sha256();
    case EVP_HPKE_HKDF_SHA384:
      return EVP_sha384();
    case EVP_HPKE_HKDF_SHA512:
      return EVP_sha512();
  }
  OPENSSL_PUT_ERROR(EVP, ERR_R_INTERNAL_ERROR);
  return NULL;
}

static int hpke_key_schedule(EVP_HPKE_CTX *hpke, uint8_t mode,
                             const uint8_t *shared_secret,
                             size_t shared_secret_len, const uint8_t *info,
                             size_t info_len, const uint8_t *psk,
                             size_t psk_len, const uint8_t *psk_id,
                             size_t psk_id_len) {
  // Verify the PSK inputs.
  switch (mode) {
    case HPKE_MODE_BASE:
      // This is an internal error, unreachable from the caller.
      assert(psk_len == 0 && psk_id_len == 0);
      break;
    case HPKE_MODE_PSK:
      if (psk_len == 0 || psk_id_len == 0) {
        OPENSSL_PUT_ERROR(EVP, EVP_R_EMPTY_PSK);
        return 0;
      }
      break;
    default:
      return 0;
  }

  // Attempt to get an EVP_AEAD*.
  const EVP_AEAD *aead = hpke_get_aead(hpke->aead_id);
  if (aead == NULL) {
    return 0;
  }

  uint8_t suite_id[HPKE_SUITE_ID_LEN];
  if (!hpke_build_suite_id(suite_id, hpke->kdf_id, hpke->aead_id)) {
    return 0;
  }

  // psk_id_hash = LabeledExtract("", "psk_id_hash", psk_id)
  static const char kPskIdHashLabel[] = "psk_id_hash";
  uint8_t psk_id_hash[EVP_MAX_MD_SIZE];
  size_t psk_id_hash_len;
  if (!hpke_labeled_extract(hpke->hkdf_md, psk_id_hash, &psk_id_hash_len, NULL,
                            0, suite_id, sizeof(suite_id), kPskIdHashLabel,
                            psk_id, psk_id_len)) {
    return 0;
  }

  // info_hash = LabeledExtract("", "info_hash", info)
  static const char kInfoHashLabel[] = "info_hash";
  uint8_t info_hash[EVP_MAX_MD_SIZE];
  size_t info_hash_len;
  if (!hpke_labeled_extract(hpke->hkdf_md, info_hash, &info_hash_len, NULL, 0,
                            suite_id, sizeof(suite_id), kInfoHashLabel, info,
                            info_len)) {
    return 0;
  }

  // key_schedule_context = concat(mode, psk_id_hash, info_hash)
  uint8_t context[sizeof(uint8_t) + 2 * EVP_MAX_MD_SIZE];
  size_t context_len;
  CBB context_cbb;
  if (!CBB_init_fixed(&context_cbb, context, sizeof(context)) ||
      !CBB_add_u8(&context_cbb, mode) ||
      !CBB_add_bytes(&context_cbb, psk_id_hash, psk_id_hash_len) ||
      !CBB_add_bytes(&context_cbb, info_hash, info_hash_len) ||
      !CBB_finish(&context_cbb, NULL, &context_len)) {
    return 0;
  }

  // psk_hash = LabeledExtract("", "psk_hash", psk)
  static const char kPskHashLabel[] = "psk_hash";
  uint8_t psk_hash[EVP_MAX_MD_SIZE];
  size_t psk_hash_len;
  if (!hpke_labeled_extract(hpke->hkdf_md, psk_hash, &psk_hash_len, NULL, 0,
                            suite_id, sizeof(suite_id), kPskHashLabel, psk,
                            psk_len)) {
    return 0;
  }

  // secret = LabeledExtract(psk_hash, "secret", shared_secret)
  static const char kSecretExtractLabel[] = "secret";
  uint8_t secret[EVP_MAX_MD_SIZE];
  size_t secret_len;
  if (!hpke_labeled_extract(hpke->hkdf_md, secret, &secret_len, psk_hash,
                            psk_hash_len, suite_id, sizeof(suite_id),
                            kSecretExtractLabel, shared_secret,
                            shared_secret_len)) {
    return 0;
  }

  // key = LabeledExpand(secret, "key", key_schedule_context, Nk)
  static const char kKeyExpandLabel[] = "key";
  uint8_t key[EVP_AEAD_MAX_KEY_LENGTH];
  const size_t kKeyLen = EVP_AEAD_key_length(aead);
  if (!hpke_labeled_expand(hpke->hkdf_md, key, kKeyLen, secret, secret_len,
                           suite_id, sizeof(suite_id), kKeyExpandLabel, context,
                           context_len)) {
    return 0;
  }

  // Initialize the HPKE context's AEAD context, storing a copy of |key|.
  if (!EVP_AEAD_CTX_init(&hpke->aead_ctx, aead, key, kKeyLen, 0, NULL)) {
    return 0;
  }

  // nonce = LabeledExpand(secret, "nonce", key_schedule_context, Nn)
  static const char kNonceExpandLabel[] = "nonce";
  if (!hpke_labeled_expand(hpke->hkdf_md, hpke->nonce,
                           EVP_AEAD_nonce_length(aead), secret, secret_len,
                           suite_id, sizeof(suite_id), kNonceExpandLabel,
                           context, context_len)) {
    return 0;
  }

  // exporter_secret = LabeledExpand(secret, "exp", key_schedule_context, Nh)
  static const char kExporterSecretExpandLabel[] = "exp";
  if (!hpke_labeled_expand(hpke->hkdf_md, hpke->exporter_secret,
                           EVP_MD_size(hpke->hkdf_md), secret, secret_len,
                           suite_id, sizeof(suite_id),
                           kExporterSecretExpandLabel, context, context_len)) {
    return 0;
  }

  return 1;
}

// The number of bytes written to |out_shared_secret| is the size of the KEM's
// KDF (currently we only support SHA256).
static int hpke_encap(EVP_HPKE_CTX *hpke,
                      uint8_t out_shared_secret[SHA256_DIGEST_LENGTH],
                      const uint8_t public_key_r[X25519_PUBLIC_VALUE_LEN],
                      const uint8_t ephemeral_private[X25519_PRIVATE_KEY_LEN],
                      const uint8_t ephemeral_public[X25519_PUBLIC_VALUE_LEN]) {
  uint8_t dh[X25519_PUBLIC_VALUE_LEN];
  if (!X25519(dh, ephemeral_private, public_key_r)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_PEER_KEY);
    return 0;
  }

  uint8_t kem_context[KEM_CONTEXT_LEN];
  OPENSSL_memcpy(kem_context, ephemeral_public, X25519_PUBLIC_VALUE_LEN);
  OPENSSL_memcpy(kem_context + X25519_PUBLIC_VALUE_LEN, public_key_r,
                 X25519_PUBLIC_VALUE_LEN);
  if (!hpke_extract_and_expand(EVP_sha256(), out_shared_secret,
                               SHA256_DIGEST_LENGTH, dh, kem_context)) {
    return 0;
  }
  return 1;
}

static int hpke_decap(const EVP_HPKE_CTX *hpke,
                      uint8_t out_shared_secret[SHA256_DIGEST_LENGTH],
                      const uint8_t enc[X25519_PUBLIC_VALUE_LEN],
                      const uint8_t public_key_r[X25519_PUBLIC_VALUE_LEN],
                      const uint8_t secret_key_r[X25519_PRIVATE_KEY_LEN]) {
  uint8_t dh[X25519_PUBLIC_VALUE_LEN];
  if (!X25519(dh, secret_key_r, enc)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_PEER_KEY);
    return 0;
  }
  uint8_t kem_context[KEM_CONTEXT_LEN];
  OPENSSL_memcpy(kem_context, enc, X25519_PUBLIC_VALUE_LEN);
  OPENSSL_memcpy(kem_context + X25519_PUBLIC_VALUE_LEN, public_key_r,
                 X25519_PUBLIC_VALUE_LEN);
  if (!hpke_extract_and_expand(EVP_sha256(), out_shared_secret,
                               SHA256_DIGEST_LENGTH, dh, kem_context)) {
    return 0;
  }
  return 1;
}

void EVP_HPKE_CTX_init(EVP_HPKE_CTX *ctx) {
  OPENSSL_memset(ctx, 0, sizeof(EVP_HPKE_CTX));
  EVP_AEAD_CTX_zero(&ctx->aead_ctx);
}

void EVP_HPKE_CTX_cleanup(EVP_HPKE_CTX *ctx) {
  EVP_AEAD_CTX_cleanup(&ctx->aead_ctx);
}

int EVP_HPKE_CTX_setup_base_s_x25519(
    EVP_HPKE_CTX *hpke, uint8_t out_enc[X25519_PUBLIC_VALUE_LEN],
    uint16_t kdf_id, uint16_t aead_id,
    const uint8_t peer_public_value[X25519_PUBLIC_VALUE_LEN],
    const uint8_t *info, size_t info_len) {
  // The GenerateKeyPair() step technically belongs in the KEM's Encap()
  // function, but we've moved it up a layer to make it easier for tests to
  // inject an ephemeral keypair.
  uint8_t ephemeral_private[X25519_PRIVATE_KEY_LEN];
  X25519_keypair(out_enc, ephemeral_private);
  return EVP_HPKE_CTX_setup_base_s_x25519_for_test(
      hpke, kdf_id, aead_id, peer_public_value, info, info_len,
      ephemeral_private, out_enc);
}

int EVP_HPKE_CTX_setup_base_s_x25519_for_test(
    EVP_HPKE_CTX *hpke, uint16_t kdf_id, uint16_t aead_id,
    const uint8_t peer_public_value[X25519_PUBLIC_VALUE_LEN],
    const uint8_t *info, size_t info_len,
    const uint8_t ephemeral_private[X25519_PRIVATE_KEY_LEN],
    const uint8_t ephemeral_public[X25519_PUBLIC_VALUE_LEN]) {
  hpke->is_sender = 1;
  hpke->kdf_id = kdf_id;
  hpke->aead_id = aead_id;
  hpke->hkdf_md = hpke_get_kdf(kdf_id);
  if (hpke->hkdf_md == NULL) {
    return 0;
  }
  uint8_t shared_secret[SHA256_DIGEST_LENGTH];
  if (!hpke_encap(hpke, shared_secret, peer_public_value, ephemeral_private,
                  ephemeral_public) ||
      !hpke_key_schedule(hpke, HPKE_MODE_BASE, shared_secret,
                         sizeof(shared_secret), info, info_len, NULL, 0, NULL,
                         0)) {
    return 0;
  }
  return 1;
}

int EVP_HPKE_CTX_setup_base_r_x25519(
    EVP_HPKE_CTX *hpke, uint16_t kdf_id, uint16_t aead_id,
    const uint8_t enc[X25519_PUBLIC_VALUE_LEN],
    const uint8_t public_key[X25519_PUBLIC_VALUE_LEN],
    const uint8_t private_key[X25519_PRIVATE_KEY_LEN], const uint8_t *info,
    size_t info_len) {
  hpke->is_sender = 0;
  hpke->kdf_id = kdf_id;
  hpke->aead_id = aead_id;
  hpke->hkdf_md = hpke_get_kdf(kdf_id);
  if (hpke->hkdf_md == NULL) {
    return 0;
  }
  uint8_t shared_secret[SHA256_DIGEST_LENGTH];
  if (!hpke_decap(hpke, shared_secret, enc, public_key, private_key) ||
      !hpke_key_schedule(hpke, HPKE_MODE_BASE, shared_secret,
                         sizeof(shared_secret), info, info_len, NULL, 0, NULL,
                         0)) {
    return 0;
  }
  return 1;
}

int EVP_HPKE_CTX_setup_psk_s_x25519(
    EVP_HPKE_CTX *hpke, uint8_t out_enc[X25519_PUBLIC_VALUE_LEN],
    uint16_t kdf_id, uint16_t aead_id,
    const uint8_t peer_public_value[X25519_PUBLIC_VALUE_LEN],
    const uint8_t *info, size_t info_len, const uint8_t *psk, size_t psk_len,
    const uint8_t *psk_id, size_t psk_id_len) {
  // The GenerateKeyPair() step technically belongs in the KEM's Encap()
  // function, but we've moved it up a layer to make it easier for tests to
  // inject an ephemeral keypair.
  uint8_t ephemeral_private[X25519_PRIVATE_KEY_LEN];
  X25519_keypair(out_enc, ephemeral_private);
  return EVP_HPKE_CTX_setup_psk_s_x25519_for_test(
      hpke, kdf_id, aead_id, peer_public_value, info, info_len, psk, psk_len,
      psk_id, psk_id_len, ephemeral_private, out_enc);
}

int EVP_HPKE_CTX_setup_psk_s_x25519_for_test(
    EVP_HPKE_CTX *hpke, uint16_t kdf_id, uint16_t aead_id,
    const uint8_t peer_public_value[X25519_PUBLIC_VALUE_LEN],
    const uint8_t *info, size_t info_len, const uint8_t *psk, size_t psk_len,
    const uint8_t *psk_id, size_t psk_id_len,
    const uint8_t ephemeral_private[X25519_PRIVATE_KEY_LEN],
    const uint8_t ephemeral_public[X25519_PUBLIC_VALUE_LEN]) {
  hpke->is_sender = 1;
  hpke->kdf_id = kdf_id;
  hpke->aead_id = aead_id;
  hpke->hkdf_md = hpke_get_kdf(kdf_id);
  if (hpke->hkdf_md == NULL) {
    return 0;
  }
  uint8_t shared_secret[SHA256_DIGEST_LENGTH];
  if (!hpke_encap(hpke, shared_secret, peer_public_value, ephemeral_private,
                  ephemeral_public) ||
      !hpke_key_schedule(hpke, HPKE_MODE_PSK, shared_secret,
                         sizeof(shared_secret), info, info_len, psk, psk_len,
                         psk_id, psk_id_len)) {
    return 0;
  }
  return 1;
}

int EVP_HPKE_CTX_setup_psk_r_x25519(
    EVP_HPKE_CTX *hpke, uint16_t kdf_id, uint16_t aead_id,
    const uint8_t enc[X25519_PUBLIC_VALUE_LEN],
    const uint8_t public_key[X25519_PUBLIC_VALUE_LEN],
    const uint8_t private_key[X25519_PRIVATE_KEY_LEN], const uint8_t *info,
    size_t info_len, const uint8_t *psk, size_t psk_len, const uint8_t *psk_id,
    size_t psk_id_len) {
  hpke->is_sender = 0;
  hpke->kdf_id = kdf_id;
  hpke->aead_id = aead_id;
  hpke->hkdf_md = hpke_get_kdf(kdf_id);
  if (hpke->hkdf_md == NULL) {
    return 0;
  }
  uint8_t shared_secret[SHA256_DIGEST_LENGTH];
  if (!hpke_decap(hpke, shared_secret, enc, public_key, private_key) ||
      !hpke_key_schedule(hpke, HPKE_MODE_PSK, shared_secret,
                         sizeof(shared_secret), info, info_len, psk, psk_len,
                         psk_id, psk_id_len)) {
    return 0;
  }
  return 1;
}

static void hpke_nonce(const EVP_HPKE_CTX *hpke, uint8_t *out_nonce,
                       size_t nonce_len) {
  assert(nonce_len >= 8);

  // Write padded big-endian bytes of |hpke->seq| to |out_nonce|.
  OPENSSL_memset(out_nonce, 0, nonce_len);
  uint64_t seq_copy = hpke->seq;
  for (size_t i = 0; i < 8; i++) {
    out_nonce[nonce_len - i - 1] = seq_copy & 0xff;
    seq_copy >>= 8;
  }

  // XOR the encoded sequence with the |hpke->nonce|.
  for (size_t i = 0; i < nonce_len; i++) {
    out_nonce[i] ^= hpke->nonce[i];
  }
}

size_t EVP_HPKE_CTX_max_overhead(const EVP_HPKE_CTX *hpke) {
  assert(hpke->is_sender);
  return EVP_AEAD_max_overhead(hpke->aead_ctx.aead);
}

int EVP_HPKE_CTX_open(EVP_HPKE_CTX *hpke, uint8_t *out, size_t *out_len,
                      size_t max_out_len, const uint8_t *in, size_t in_len,
                      const uint8_t *ad, size_t ad_len) {
  if (hpke->is_sender) {
    OPENSSL_PUT_ERROR(EVP, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }
  if (hpke->seq == UINT64_MAX) {
    OPENSSL_PUT_ERROR(EVP, ERR_R_OVERFLOW);
    return 0;
  }

  uint8_t nonce[EVP_AEAD_MAX_NONCE_LENGTH];
  const size_t nonce_len = EVP_AEAD_nonce_length(hpke->aead_ctx.aead);
  hpke_nonce(hpke, nonce, nonce_len);

  if (!EVP_AEAD_CTX_open(&hpke->aead_ctx, out, out_len, max_out_len, nonce,
                         nonce_len, in, in_len, ad, ad_len)) {
    return 0;
  }
  hpke->seq++;
  return 1;
}

int EVP_HPKE_CTX_seal(EVP_HPKE_CTX *hpke, uint8_t *out, size_t *out_len,
                      size_t max_out_len, const uint8_t *in, size_t in_len,
                      const uint8_t *ad, size_t ad_len) {
  if (!hpke->is_sender) {
    OPENSSL_PUT_ERROR(EVP, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }
  if (hpke->seq == UINT64_MAX) {
    OPENSSL_PUT_ERROR(EVP, ERR_R_OVERFLOW);
    return 0;
  }

  uint8_t nonce[EVP_AEAD_MAX_NONCE_LENGTH];
  const size_t nonce_len = EVP_AEAD_nonce_length(hpke->aead_ctx.aead);
  hpke_nonce(hpke, nonce, nonce_len);

  if (!EVP_AEAD_CTX_seal(&hpke->aead_ctx, out, out_len, max_out_len, nonce,
                         nonce_len, in, in_len, ad, ad_len)) {
    return 0;
  }
  hpke->seq++;
  return 1;
}

int EVP_HPKE_CTX_export(const EVP_HPKE_CTX *hpke, uint8_t *out,
                        size_t secret_len, const uint8_t *context,
                        size_t context_len) {
  uint8_t suite_id[HPKE_SUITE_ID_LEN];
  if (!hpke_build_suite_id(suite_id, hpke->kdf_id, hpke->aead_id)) {
    return 0;
  }
  static const char kExportExpandLabel[] = "sec";
  if (!hpke_labeled_expand(hpke->hkdf_md, out, secret_len,
                           hpke->exporter_secret, EVP_MD_size(hpke->hkdf_md),
                           suite_id, sizeof(suite_id), kExportExpandLabel,
                           context, context_len)) {
    return 0;
  }
  return 1;
}

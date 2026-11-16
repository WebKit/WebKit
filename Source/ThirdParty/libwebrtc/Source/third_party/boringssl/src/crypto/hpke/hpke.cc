// Copyright 2020 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <openssl/hpke.h>

#include <assert.h>
#include <string.h>

#include <array>
#include <optional>
#include <string_view>

#include <openssl/aead.h>
#include <openssl/bytestring.h>
#include <openssl/curve25519.h>
#include <openssl/digest.h>
#include <openssl/ec.h>
#include <openssl/ec_key.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/evp_errors.h>
#include <openssl/hkdf.h>
#include <openssl/mem.h>
#include <openssl/mlkem.h>
#include <openssl/sha2.h>
#include <openssl/span.h>
#include <openssl/xwing.h>

#include "../evp/internal.h"
#include "../fipsmodule/ec/internal.h"
#include "../fipsmodule/keccak/internal.h"
#include "../internal.h"
#include "../mem_internal.h"


using namespace bssl;

// This file implements RFC 9180.

#define MAX_SHARED_SECRET_LEN SHA256_DIGEST_LENGTH
#define MAX_DHKEM_DH_SHARED_KEY_LEN X25519_SHARED_KEY_LEN
#define MAX_DHKEM_PUBLIC_KEY_LEN 65  // kP256PublicValueLen

namespace {

// HpkeKeyMethod implements the key-type-specific operations that HPKE performs
// on keys of a particular `EVP_PKEY_ALG` type.
struct HpkeKeyMethod {
  // hpke_kem_id is the ID of the KEM in RFC 9180.
  uint16_t hpke_kem_id;

  // public_key_len corresponds to Npk in RFC 9180.
  size_t public_key_len;
  // private_key_len corresponds to Nsk in RFC 9180.
  size_t private_key_len;

  const EVP_PKEY_ALG *(*alg_func)();

  // deserialize_private_key and serialize_private_key implement the
  // DeserializePrivateKey and SerializePrivateKey operations for the key type
  // from section 4 of RFC 9180, using the same format accepted by
  // `EVP_HPKE_KEY_init`.
  EVP_PKEY *(*deserialize_private_key)(const EVP_PKEY_ALG *alg,
                                       const uint8_t *in, size_t len);
  int (*serialize_private_key)(const EVP_PKEY *pkey, uint8_t *out,
                               size_t *out_len);

  // deserialize_public_key and serialize_public_key implement the
  // DeserializePublicKey and SerializePublicKey operations from section 4 of
  // RFC 9180.
  EVP_PKEY *(*deserialize_public_key)(const EVP_PKEY_ALG *alg,
                                      const uint8_t *in, size_t len);
  int (*serialize_public_key)(const EVP_PKEY *pkey, uint8_t *out,
                              size_t *out_len);

  // derive_key_pair implements the DeriveKeyPair operation from section 4 of
  // RFC 9180.
  EVP_PKEY *(*derive_key_pair)(const EVP_PKEY_ALG *alg, uint16_t kem_suite_id,
                               Span<const uint8_t> ikm);
};

// EVP_KEM_DHKEM implements the DHKEM construction from RFC 9180 based on a
// DH group and an HKDF.
//
// In DHKEM, the encapsulated key is the serialized ephemeral public key, so
// Nenc (`ciphertext_len`) equals Npk (`public_key_len`).
struct EVP_KEM_DHKEM : public EVP_KEM {
  // dh_shared_key_len corresponds to Ndh in RFC 9180.
  size_t dh_shared_key_len;

  const HpkeKeyMethod *key_method;
  const EVP_MD *(*hkdf_digest_func)();

  // Generates the ephemeral encapsulation key from external entropy bytes
  // provided as a "seed" for testing. The seed's format is variant-specific.
  EVP_PKEY *(*key_from_encap_entropy)(const EVP_PKEY_ALG *alg,
                                      const uint8_t *in, size_t len);
};

}  // namespace

struct evp_hpke_kem_st {
  uint16_t id;

  const EVP_KEM *(*evp_kem_func)();
  const HpkeKeyMethod *key_method;

  int (*auth_encap)(const EVP_KEM *kem, Span<uint8_t> out_ciphertext,
                    Span<uint8_t> out_secret, const EVP_PKEY *peer_key,
                    const EVP_PKEY *auth_key);
  int (*auth_encap_external_entropy)(const EVP_KEM *kem,
                                     Span<uint8_t> out_ciphertext,
                                     Span<uint8_t> out_secret,
                                     const EVP_PKEY *peer_key,
                                     const EVP_PKEY *auth_key,
                                     Span<const uint8_t> entropy);
  int (*auth_decap)(const EVP_KEM *kem, Span<uint8_t> out_secret,
                    Span<const uint8_t> ciphertext, const EVP_PKEY *key,
                    const EVP_PKEY *auth_key);
};

struct evp_hpke_kdf_st {
  uint16_t id;
  // We only support HKDF-based KDFs.
  const EVP_MD *(*hkdf_md_func)();
};

struct evp_hpke_aead_st {
  uint16_t id;
  const EVP_AEAD *(*aead_func)();
};

namespace {

// Low-level labeled KDF functions.

const char kHpkeVersionId[] = "HPKE-v1";

int add_label_string(CBB *cbb, const char *label) {
  return CBB_add_bytes(cbb, (const uint8_t *)label, strlen(label));
}

int hpke_labeled_extract(const EVP_MD *hkdf_md, uint8_t *out_key,
                         size_t *out_len, const uint8_t *salt, size_t salt_len,
                         const uint8_t *suite_id, size_t suite_id_len,
                         const char *label, const uint8_t *ikm,
                         size_t ikm_len) {
  // labeledIKM = concat("HPKE-v1", suite_id, label, IKM)
  CBB labeled_ikm;
  int ok = CBB_init(&labeled_ikm, 0) &&
           add_label_string(&labeled_ikm, kHpkeVersionId) &&
           CBB_add_bytes(&labeled_ikm, suite_id, suite_id_len) &&
           add_label_string(&labeled_ikm, label) &&
           CBB_add_bytes(&labeled_ikm, ikm, ikm_len) &&
           HKDF_extract(out_key, out_len, hkdf_md, CBB_data(&labeled_ikm),
                        CBB_len(&labeled_ikm), salt, salt_len);
  CBB_cleanup(&labeled_ikm);
  return ok;
}

int hpke_labeled_expand(const EVP_MD *hkdf_md, uint8_t *out_key, size_t out_len,
                        const uint8_t *prk, size_t prk_len,
                        const uint8_t *suite_id, size_t suite_id_len,
                        const char *label, const uint8_t *info,
                        size_t info_len) {
  // labeledInfo = concat(I2OSP(L, 2), "HPKE-v1", suite_id, label, info)
  CBB labeled_info;
  int ok = CBB_init(&labeled_info, 0) &&  //
           CBB_add_u16(&labeled_info, out_len) &&
           add_label_string(&labeled_info, kHpkeVersionId) &&
           CBB_add_bytes(&labeled_info, suite_id, suite_id_len) &&
           add_label_string(&labeled_info, label) &&
           CBB_add_bytes(&labeled_info, info, info_len) &&
           HKDF_expand(out_key, out_len, hkdf_md, prk, prk_len,
                       CBB_data(&labeled_info), CBB_len(&labeled_info));
  CBB_cleanup(&labeled_info);
  return ok;
}

void absorb_u16(BORINGSSL_keccak_st *ctx, uint16_t n) {
  uint8_t bytes[2];
  CRYPTO_store_u16_be(bytes, n);
  BORINGSSL_keccak_absorb(ctx, bytes, sizeof(bytes));
}

void hpke_shake256_labeled_derive(Span<uint8_t> out, Span<const uint8_t> ikm,
                                  Span<const uint8_t> suite_id,
                                  std::string_view label,
                                  Span<const uint8_t> context) {
  // https://www.ietf.org/archive/id/draft-ietf-hpke-hpke-03.html#name-labeled-derivation-function
  // https://www.ietf.org/archive/id/draft-ietf-hpke-pq-04.html#name-single-stage-kdfs
  BORINGSSL_keccak_st ctx;
  BORINGSSL_keccak_init(&ctx, boringssl_shake256);
  BORINGSSL_keccak_absorb(&ctx, ikm.data(), ikm.size());
  auto hpke_version_id = StringAsBytes(kHpkeVersionId);
  BORINGSSL_keccak_absorb(&ctx, hpke_version_id.data(), hpke_version_id.size());
  BORINGSSL_keccak_absorb(&ctx, suite_id.data(), suite_id.size());
  auto label_bytes = StringAsBytes(label);
  assert(label_bytes.size() < 0xffff);
  absorb_u16(&ctx, static_cast<uint16_t>(label_bytes.size()));
  BORINGSSL_keccak_absorb(&ctx, label_bytes.data(), label_bytes.size());
  assert(out.size() <= 0xffff);
  absorb_u16(&ctx, static_cast<uint16_t>(out.size()));
  BORINGSSL_keccak_absorb(&ctx, context.data(), context.size());
  BORINGSSL_keccak_squeeze(&ctx, out.data(), out.size());
}


// KEM implementations.

std::array<uint8_t, 5> hpke_kem_suite_id(uint16_t kem_id) {
  // concat("KEM", I2OSP(kem_id, 2))
  return {'K', 'E', 'M', static_cast<uint8_t>(kem_id >> 8),
          static_cast<uint8_t>(kem_id)};
}

// dhkem_extract_and_expand implements the ExtractAndExpand operation in the
// DHKEM construction. See section 4.1 of RFC 9180.
int dhkem_extract_and_expand(uint16_t kem_id, const EVP_MD *hkdf_md,
                             uint8_t *out_key, size_t out_len,
                             const uint8_t *dh, size_t dh_len,
                             const uint8_t *kem_context,
                             size_t kem_context_len) {
  auto suite_id = hpke_kem_suite_id(kem_id);
  uint8_t prk[EVP_MAX_MD_SIZE];
  size_t prk_len;
  return hpke_labeled_extract(hkdf_md, prk, &prk_len, nullptr, 0,
                              suite_id.data(), suite_id.size(), "eae_prk", dh,
                              dh_len) &&
         hpke_labeled_expand(hkdf_md, out_key, out_len, prk, prk_len,
                             suite_id.data(), suite_id.size(), "shared_secret",
                             kem_context, kem_context_len);
}

bool dhkem_dh(const EVP_KEM_DHKEM *dhkem, Span<uint8_t> out,
              const EVP_PKEY *key, const EVP_PKEY *peer) {
  assert(out.size() == dhkem->dh_shared_key_len);
  UniquePtr<EVP_PKEY_CTX> pkctx(EVP_PKEY_CTX_new(key, nullptr));
  if (pkctx == nullptr) {
    return false;
  }
  size_t out_len = out.size();
  return EVP_PKEY_derive_init(pkctx.get()) &&
         EVP_PKEY_derive_set_peer(pkctx.get(), peer) &&
         EVP_PKEY_derive(pkctx.get(), out.data(), &out_len) &&
         out_len == dhkem->dh_shared_key_len;
}

bool dhkem_serialize_public_key(const EVP_KEM_DHKEM *dhkem, Span<uint8_t> out,
                                const EVP_PKEY *key) {
  if (out.size() != dhkem->key_method->public_key_len) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_BUFFER_SIZE);
    return false;
  }
  size_t serialized_len = out.size();
  if (!dhkem->key_method->serialize_public_key(key, out.data(),
                                               &serialized_len)) {
    return false;
  }
  BSSL_CHECK(serialized_len == out.size());
  return true;
}

bool dhkem_serialize_public_keys(const EVP_KEM_DHKEM *dhkem, Span<uint8_t> out,
                                 const EVP_PKEY *key1, const EVP_PKEY *key2) {
  const size_t public_len = dhkem->key_method->public_key_len;
  if (out.size() != 2 * dhkem->key_method->public_key_len) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_BUFFER_SIZE);
    return false;
  }
  return dhkem_serialize_public_key(dhkem, out.subspan(0, public_len), key1) &&
         dhkem_serialize_public_key(dhkem, out.subspan(public_len), key2);
}

bool dhkem_serialize_public_keys(const EVP_KEM_DHKEM *dhkem, Span<uint8_t> out,
                                 const EVP_PKEY *key1, const EVP_PKEY *key2,
                                 const EVP_PKEY *key3) {
  const size_t public_len = dhkem->key_method->public_key_len;
  if (out.size() != 3 * public_len) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_BUFFER_SIZE);
    return false;
  }
  return dhkem_serialize_public_key(dhkem, out.subspan(0, public_len), key1) &&
         dhkem_serialize_public_keys(dhkem, out.subspan(public_len), key2,
                                     key3);
}

int dhkem_encap_with_ephemeral_key(const EVP_KEM_DHKEM *dhkem,
                                   Span<uint8_t> out_ciphertext,
                                   Span<uint8_t> out_secret,
                                   const EVP_PKEY *peer_key,
                                   const EVP_PKEY *ephemeral_key) {
  const size_t public_len = dhkem->key_method->public_key_len;
  if (out_ciphertext.size() != public_len) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_CIPHERTEXT_LENGTH);
    return 0;
  }
  if (out_secret.size() != dhkem->secret_len) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_SECRET_LENGTH);
    return 0;
  }
  // dh = DH(skE, pkR)
  InplaceVector<uint8_t, MAX_DHKEM_DH_SHARED_KEY_LEN> dh;
  if (!dh.TryResizeForOverwrite(dhkem->dh_shared_key_len) ||
      !dhkem_dh(dhkem, Span(dh), ephemeral_key, peer_key)) {
    return 0;
  }
  // enc = SerializePublicKey(pkE)
  // pkRm = SerializePublicKey(pkR)
  // kem_context = concat(enc, pkRm)
  InplaceVector<uint8_t, 2 * MAX_DHKEM_PUBLIC_KEY_LEN> kem_context;
  if (!kem_context.TryResizeForOverwrite(2 * public_len) ||
      !dhkem_serialize_public_keys(dhkem, Span(kem_context), ephemeral_key,
                                   peer_key)) {
    return 0;
  }
  OPENSSL_memcpy(out_ciphertext.data(), kem_context.data(), public_len);
  // shared_secret = ExtractAndExpand(dh, kem_context)
  return dhkem_extract_and_expand(dhkem->key_method->hpke_kem_id,
                                  dhkem->hkdf_digest_func(), out_secret.data(),
                                  out_secret.size(), dh.data(), dh.size(),
                                  kem_context.data(), kem_context.size());
}

int dhkem_encap(const EVP_KEM *kem, Span<uint8_t> out_ciphertext,
                Span<uint8_t> out_secret, const EVP_PKEY *peer_key) {
  const auto *dhkem = static_cast<const EVP_KEM_DHKEM *>(kem);
  UniquePtr<EVP_PKEY> ephemeral_key(
      EVP_PKEY_generate_from_alg(dhkem->key_method->alg_func()));
  if (ephemeral_key == nullptr) {
    return 0;
  }
  return dhkem_encap_with_ephemeral_key(dhkem, out_ciphertext, out_secret,
                                        peer_key, ephemeral_key.get());
}

int dhkem_encap_external_entropy(const EVP_KEM *kem,
                                 Span<uint8_t> out_ciphertext,
                                 Span<uint8_t> out_secret,
                                 const EVP_PKEY *peer_key,
                                 Span<const uint8_t> entropy) {
  const auto *dhkem = static_cast<const EVP_KEM_DHKEM *>(kem);
  UniquePtr<EVP_PKEY> ephemeral_key(dhkem->key_from_encap_entropy(
      dhkem->key_method->alg_func(), entropy.data(), entropy.size()));
  if (ephemeral_key == nullptr) {
    return 0;
  }
  return dhkem_encap_with_ephemeral_key(dhkem, out_ciphertext, out_secret,
                                        peer_key, ephemeral_key.get());
}

int dhkem_decap(const EVP_KEM *kem, Span<uint8_t> out_secret,
                Span<const uint8_t> ciphertext, const EVP_PKEY *key) {
  const auto *dhkem = static_cast<const EVP_KEM_DHKEM *>(kem);
  const size_t public_len = dhkem->key_method->public_key_len;
  if (ciphertext.size() != public_len) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_CIPHERTEXT_LENGTH);
    return 0;
  }
  if (out_secret.size() != dhkem->secret_len) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_SECRET_LENGTH);
    return 0;
  }
  // pkE = DeserializePublicKey(enc)
  UniquePtr<EVP_PKEY> ephemeral_key(dhkem->key_method->deserialize_public_key(
      dhkem->key_method->alg_func(), ciphertext.data(), ciphertext.size()));
  if (ephemeral_key == nullptr) {
    return 0;
  }
  // dh = DH(skR, pkE)
  InplaceVector<uint8_t, MAX_DHKEM_DH_SHARED_KEY_LEN> dh;
  if (!dh.TryResizeForOverwrite(dhkem->dh_shared_key_len) ||
      !dhkem_dh(dhkem, Span(dh), key, ephemeral_key.get())) {
    return 0;
  }
  // pkRm = SerializePublicKey(pk(skR))
  // kem_context = concat(enc, pkRm)
  InplaceVector<uint8_t, 2 * MAX_DHKEM_PUBLIC_KEY_LEN> kem_context;
  if (!kem_context.TryResizeForOverwrite(2 * public_len) ||
      !dhkem_serialize_public_key(dhkem, Span(kem_context).subspan(public_len),
                                  key)) {
    return 0;
  }
  OPENSSL_memcpy(kem_context.data(), ciphertext.data(), ciphertext.size());
  // shared_secret = ExtractAndExpand(dh, kem_context)
  return dhkem_extract_and_expand(dhkem->key_method->hpke_kem_id,
                                  dhkem->hkdf_digest_func(), out_secret.data(),
                                  out_secret.size(), dh.data(), dh.size(),
                                  kem_context.data(), kem_context.size());
}

int dhkem_auth_encap_with_ephemeral_key(const EVP_KEM_DHKEM *dhkem,
                                        Span<uint8_t> out_ciphertext,
                                        Span<uint8_t> out_secret,
                                        const EVP_PKEY *peer_key,
                                        const EVP_PKEY *auth_key,
                                        const EVP_PKEY *ephemeral_key) {
  const size_t public_len = dhkem->key_method->public_key_len;
  if (out_ciphertext.size() != public_len) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_CIPHERTEXT_LENGTH);
    return 0;
  }
  if (out_secret.size() != dhkem->secret_len) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_SECRET_LENGTH);
    return 0;
  }
  // dh = concat(DH(skE, pkR), DH(skS, pkR))
  InplaceVector<uint8_t, 2 * MAX_DHKEM_DH_SHARED_KEY_LEN> dh;
  if (!dh.TryResizeForOverwrite(2 * dhkem->dh_shared_key_len) ||
      !dhkem_dh(dhkem, Span(dh).subspan(0, dhkem->dh_shared_key_len),
                ephemeral_key, peer_key) ||
      !dhkem_dh(dhkem, Span(dh).subspan(dhkem->dh_shared_key_len), auth_key,
                peer_key)) {
    return 0;
  }
  // enc = SerializePublicKey(pkE)
  // pkRm = SerializePublicKey(pkR)
  // pkSm = SerializePublicKey(pk(skS))
  // kem_context = concat(enc, pkRm, pkSm)
  InplaceVector<uint8_t, 3 * MAX_DHKEM_PUBLIC_KEY_LEN> kem_context;
  if (!kem_context.TryResizeForOverwrite(3 * public_len) ||
      !dhkem_serialize_public_keys(dhkem, Span(kem_context), ephemeral_key,
                                   peer_key, auth_key)) {
    return 0;
  }
  OPENSSL_memcpy(out_ciphertext.data(), kem_context.data(), public_len);
  // shared_secret = ExtractAndExpand(dh, kem_context)
  return dhkem_extract_and_expand(dhkem->key_method->hpke_kem_id,
                                  dhkem->hkdf_digest_func(), out_secret.data(),
                                  out_secret.size(), dh.data(), dh.size(),
                                  kem_context.data(), kem_context.size());
}

int dhkem_auth_encap(const EVP_KEM *kem, Span<uint8_t> out_ciphertext,
                     Span<uint8_t> out_secret, const EVP_PKEY *peer_key,
                     const EVP_PKEY *auth_key) {
  const auto *dhkem = static_cast<const EVP_KEM_DHKEM *>(kem);
  UniquePtr<EVP_PKEY> ephemeral_key(
      EVP_PKEY_generate_from_alg(dhkem->key_method->alg_func()));
  if (ephemeral_key == nullptr) {
    return 0;
  }
  return dhkem_auth_encap_with_ephemeral_key(dhkem, out_ciphertext, out_secret,
                                             peer_key, auth_key,
                                             ephemeral_key.get());
}

int dhkem_auth_encap_external_entropy(const EVP_KEM *kem,
                                      Span<uint8_t> out_ciphertext,
                                      Span<uint8_t> out_secret,
                                      const EVP_PKEY *peer_key,
                                      const EVP_PKEY *auth_key,
                                      Span<const uint8_t> entropy) {
  const auto *dhkem = static_cast<const EVP_KEM_DHKEM *>(kem);
  UniquePtr<EVP_PKEY> ephemeral_key(dhkem->key_from_encap_entropy(
      dhkem->key_method->alg_func(), entropy.data(), entropy.size()));
  if (ephemeral_key == nullptr) {
    return 0;
  }
  return dhkem_auth_encap_with_ephemeral_key(dhkem, out_ciphertext, out_secret,
                                             peer_key, auth_key,
                                             ephemeral_key.get());
}

int dhkem_auth_decap(const EVP_KEM *kem, Span<uint8_t> out_secret,
                     Span<const uint8_t> ciphertext, const EVP_PKEY *key,
                     const EVP_PKEY *auth_key) {
  const auto *dhkem = static_cast<const EVP_KEM_DHKEM *>(kem);
  const size_t public_len = dhkem->key_method->public_key_len;
  if (ciphertext.size() != public_len) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_CIPHERTEXT_LENGTH);
    return 0;
  }
  if (out_secret.size() != dhkem->secret_len) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_SECRET_LENGTH);
    return 0;
  }
  // pkE = DeserializePublicKey(enc)
  UniquePtr<EVP_PKEY> ephemeral_key(dhkem->key_method->deserialize_public_key(
      dhkem->key_method->alg_func(), ciphertext.data(), ciphertext.size()));
  if (ephemeral_key == nullptr) {
    return 0;
  }
  // dh = concat(DH(skR, pkE), DH(skR, pkS))
  InplaceVector<uint8_t, 2 * MAX_DHKEM_DH_SHARED_KEY_LEN> dh;
  if (!dh.TryResizeForOverwrite(2 * dhkem->dh_shared_key_len) ||
      !dhkem_dh(dhkem, Span(dh).subspan(0, dhkem->dh_shared_key_len), key,
                ephemeral_key.get()) ||
      !dhkem_dh(dhkem, Span(dh).subspan(dhkem->dh_shared_key_len), key,
                auth_key)) {
    return 0;
  }
  // pkRm = SerializePublicKey(pk(skR))
  // pkSm = SerializePublicKey(pkS)
  // kem_context = concat(enc, pkRm, pkSm)
  InplaceVector<uint8_t, 3 * MAX_DHKEM_PUBLIC_KEY_LEN> kem_context;
  if (!kem_context.TryResizeForOverwrite(3 * public_len) ||
      !dhkem_serialize_public_keys(dhkem, Span(kem_context).subspan(public_len),
                                   key, auth_key)) {
    return 0;
  }
  OPENSSL_memcpy(kem_context.data(), ciphertext.data(), ciphertext.size());
  // shared_secret = ExtractAndExpand(dh, kem_context)
  return dhkem_extract_and_expand(dhkem->key_method->hpke_kem_id,
                                  dhkem->hkdf_digest_func(), out_secret.data(),
                                  out_secret.size(), dh.data(), dh.size(),
                                  kem_context.data(), kem_context.size());
}

// RFC 9180: "For X25519 ..., the DeriveKeyPair() function applies a KDF to the
// input." Since HPKE uses X25519 only in the DHKEM(X25519, HKDF-SHA256)
// construction, this uses HKDF-SHA256 as the KDF.
EVP_PKEY *x25519_derive_key_pair(const EVP_PKEY_ALG *alg, uint16_t kem_suite_id,
                                 Span<const uint8_t> ikm) {
  // https://www.rfc-editor.org/rfc/rfc9180.html#name-derivekeypair
  auto suite_id = hpke_kem_suite_id(kem_suite_id);
  uint8_t dkp_prk[SHA256_DIGEST_LENGTH];
  size_t dkp_prk_len;
  uint8_t sk[X25519_PRIVATE_KEY_LEN];
  if (!hpke_labeled_extract(EVP_sha256(), dkp_prk, &dkp_prk_len, nullptr, 0,
                            suite_id.data(), suite_id.size(), "dkp_prk",
                            ikm.data(), ikm.size()) ||
      !hpke_labeled_expand(EVP_sha256(), sk, sizeof(sk), dkp_prk, dkp_prk_len,
                           suite_id.data(), suite_id.size(), "sk",
                           /*info=*/nullptr, /*info_len=*/0)) {
    return nullptr;
  }
  return EVP_PKEY_from_raw_private_key(alg, sk, sizeof(sk));
}

constexpr HpkeKeyMethod kX25519KeyMethod = {
    /*hpke_kem_id=*/EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
    /*public_key_len=*/X25519_PUBLIC_VALUE_LEN,
    /*private_key_len=*/X25519_PRIVATE_KEY_LEN,
    /*alg_func=*/&EVP_pkey_x25519,
    /*deserialize_private_key=*/&EVP_PKEY_from_raw_private_key,
    /*serialize_private_key=*/&EVP_PKEY_get_raw_private_key,
    /*deserialize_public_key=*/&EVP_PKEY_from_raw_public_key,
    /*serialize_public_key=*/&EVP_PKEY_get_raw_public_key,
    /*derive_key_pair=*/x25519_derive_key_pair,
};

}  // namespace

const EVP_KEM *EVP_kem_dhkem_x25519_hkdf_sha256() {
  static constexpr EVP_KEM_DHKEM kDhkemX25519 = {
      /*EVP_KEM=*/{
          /*pkey_id=*/EVP_PKEY_X25519,
          /*ciphertext_len=*/X25519_PUBLIC_VALUE_LEN,
          /*secret_len=*/SHA256_DIGEST_LENGTH,
          /*entropy_len=*/X25519_PRIVATE_KEY_LEN,
          /*check_key=*/nullptr,
          /*encap=*/&dhkem_encap,
          /*encap_external_entropy=*/&dhkem_encap_external_entropy,
          /*decap=*/&dhkem_decap,
      },
      /*dh_shared_key_len=*/X25519_SHARED_KEY_LEN,
      /*key_method=*/&kX25519KeyMethod,
      /*hkdf_digest_func=*/&EVP_sha256,
      // The X25519 encap entropy is the ephemeral private key itself.
      /*key_from_encap_entropy=*/&EVP_PKEY_from_raw_private_key,
  };
  return &kDhkemX25519;
}

const EVP_HPKE_KEM *EVP_hpke_x25519_hkdf_sha256() {
  static constexpr EVP_HPKE_KEM kKEM = {
      /*id=*/EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
      /*evp_kem_func=*/&EVP_kem_dhkem_x25519_hkdf_sha256,
      /*key_method=*/&kX25519KeyMethod,
      /*auth_encap=*/&dhkem_auth_encap,
      /*auth_encap_external_entropy=*/&dhkem_auth_encap_external_entropy,
      /*auth_decap=*/&dhkem_auth_decap,
  };
  return &kKEM;
}

namespace {

constexpr size_t kP256PrivateKeyLen = 32;
constexpr size_t kP256PublicValueLen = 65;
constexpr size_t kP256SeedLen = 32;
constexpr size_t kP256SharedKeyLen = 32;

// RFC 9180's SerializePublicKey format for DHKEM(P-256, HKDF-SHA256) is the
// uncompressed X9.62 point, and its private keys are the big-endian scalar.
int p256_serialize_public_key(const EVP_PKEY *pkey, uint8_t *out,
                              size_t *out_len) {
  CBB cbb;
  CBB_init_fixed(&cbb, out, *out_len);
  if (!EVP_PKEY_marshal_ec_uncompressed_point(&cbb, pkey)) {
    return 0;
  }
  *out_len = CBB_len(&cbb);
  return 1;
}

int p256_serialize_private_key(const EVP_PKEY *pkey, uint8_t *out,
                               size_t *out_len) {
  CBB cbb;
  CBB_init_fixed(&cbb, out, *out_len);
  if (!EVP_PKEY_marshal_ec_private_scalar(&cbb, pkey)) {
    return 0;
  }
  *out_len = CBB_len(&cbb);
  return 1;
}

EVP_PKEY *p256_derive_key_pair(const EVP_PKEY_ALG *alg, uint16_t kem_suite_id,
                               Span<const uint8_t> ikm) {
  // https://www.rfc-editor.org/rfc/rfc9180.html#name-derivekeypair
  auto suite_id = hpke_kem_suite_id(kem_suite_id);
  uint8_t dkp_prk[SHA256_DIGEST_LENGTH];
  size_t dkp_prk_len;
  if (!hpke_labeled_extract(EVP_sha256(), dkp_prk, &dkp_prk_len, nullptr, 0,
                            suite_id.data(), suite_id.size(), "dkp_prk",
                            ikm.data(), ikm.size())) {
    return nullptr;
  }
  assert(dkp_prk_len == sizeof(dkp_prk));
  const EC_GROUP *const group = EC_group_p256();

  // Rejection sampling.
  uint8_t priv[kP256PrivateKeyLen];
  for (size_t counter = 0; counter < 256; counter++) {
    const uint8_t counter_byte = counter & 0xff;
    if (!hpke_labeled_expand(EVP_sha256(), priv, sizeof(priv), dkp_prk,
                             sizeof(dkp_prk), suite_id.data(), suite_id.size(),
                             "candidate", &counter_byte,
                             sizeof(counter_byte))) {
      return nullptr;
    }
    EC_SCALAR private_scalar;
    // `ec_scalar_from_bytes` checks that the scalar is less than the order.
    if (ec_scalar_from_bytes(group, &private_scalar, priv, sizeof(priv)) &&
        !ec_scalar_is_zero(group, &private_scalar)) {
      return EVP_PKEY_from_ec_private_scalar(alg, priv, sizeof(priv));
    }
  }
  // This happens with probability of 2^-(32*256).
  OPENSSL_PUT_ERROR(EVP, ERR_R_INTERNAL_ERROR);
  return nullptr;
}

// The P-256 encap entropy is ikmE, the input to DeriveKeyPair.
EVP_PKEY *p256_key_from_encap_entropy(const EVP_PKEY_ALG *alg,
                                      const uint8_t *in, size_t len) {
  return p256_derive_key_pair(alg, EVP_HPKE_DHKEM_P256_HKDF_SHA256,
                              Span(in, len));
}

bool p256_check_key(const EVP_KEM *kem, const EVP_PKEY *key) {
  const EC_KEY *ec_key = EVP_PKEY_get0_EC_KEY(key);
  return ec_key != nullptr &&
         EC_GROUP_cmp(EC_KEY_get0_group(ec_key), EC_group_p256(),
                      /*ignored=*/nullptr) == 0;
}

constexpr HpkeKeyMethod kP256KeyMethod = {
    /*hpke_kem_id=*/EVP_HPKE_DHKEM_P256_HKDF_SHA256,
    /*public_key_len=*/kP256PublicValueLen,
    /*private_key_len=*/kP256PrivateKeyLen,
    /*alg_func=*/&EVP_pkey_ec_p256,
    /*deserialize_private_key=*/&EVP_PKEY_from_ec_private_scalar,
    /*serialize_private_key=*/&p256_serialize_private_key,
    /*deserialize_public_key=*/&EVP_PKEY_from_ec_uncompressed_point,
    /*serialize_public_key=*/&p256_serialize_public_key,
    /*derive_key_pair=*/&p256_derive_key_pair,
};

}  // namespace

const EVP_KEM *EVP_kem_dhkem_p256_hkdf_sha256() {
  static constexpr EVP_KEM_DHKEM kDhkemP256 = {
      /*EVP_KEM=*/{
          /*pkey_id=*/EVP_PKEY_EC,
          /*ciphertext_len=*/kP256PublicValueLen,
          /*secret_len=*/SHA256_DIGEST_LENGTH,
          /*entropy_len=*/kP256SeedLen,
          /*check_key=*/&p256_check_key,
          /*encap=*/&dhkem_encap,
          /*encap_external_entropy=*/&dhkem_encap_external_entropy,
          /*decap=*/&dhkem_decap,
      },
      /*dh_shared_key_len=*/kP256SharedKeyLen,
      /*key_method=*/&kP256KeyMethod,
      /*hkdf_digest_func=*/&EVP_sha256,
      /*key_from_encap_entropy=*/&p256_key_from_encap_entropy,
  };
  return &kDhkemP256;
}

const EVP_HPKE_KEM *EVP_hpke_p256_hkdf_sha256() {
  static constexpr EVP_HPKE_KEM kKEM = {
      /*id=*/EVP_HPKE_DHKEM_P256_HKDF_SHA256,
      /*evp_kem_func=*/&EVP_kem_dhkem_p256_hkdf_sha256,
      /*key_method=*/&kP256KeyMethod,
      /*auth_encap=*/&dhkem_auth_encap,
      /*auth_encap_external_entropy=*/&dhkem_auth_encap_external_entropy,
      /*auth_decap=*/&dhkem_auth_decap,
  };
  return &kKEM;
}

namespace {

template <size_t SEED_BYTES>
EVP_PKEY *shake256_derive_key_pair_from_private_seed(const EVP_PKEY_ALG *alg,
                                                     uint16_t kem_suite_id,
                                                     Span<const uint8_t> ikm) {
  uint8_t seed[SEED_BYTES];
  hpke_shake256_labeled_derive(seed, ikm, hpke_kem_suite_id(kem_suite_id),
                               "DeriveKeyPair", /*context=*/{});
  return EVP_PKEY_from_private_seed(alg, seed, sizeof(seed));
}

constexpr HpkeKeyMethod kXwingKeyMethod = {
    /*hpke_kem_id=*/EVP_HPKE_XWING,
    /*public_key_len=*/XWING_PUBLIC_KEY_BYTES,
    /*private_key_len=*/XWING_PRIVATE_KEY_BYTES,
    /*alg_func=*/&EVP_pkey_xwing,
    /*deserialize_private_key=*/&EVP_PKEY_from_private_seed,
    /*serialize_private_key=*/&EVP_PKEY_get_private_seed,
    /*deserialize_public_key=*/&EVP_PKEY_from_raw_public_key,
    /*serialize_public_key=*/&EVP_PKEY_get_raw_public_key,
    /*derive_key_pair=*/
    shake256_derive_key_pair_from_private_seed<XWING_PRIVATE_KEY_BYTES>,
};

}  // namespace

const EVP_HPKE_KEM *EVP_hpke_xwing() {
  static constexpr EVP_HPKE_KEM kKEM = {
      /*id=*/EVP_HPKE_XWING,
      /*evp_kem_func=*/&EVP_kem_xwing,
      /*key_method=*/&kXwingKeyMethod,
      // X-Wing doesn't support authenticated encapsulation/decapsulation:
      // https://datatracker.ietf.org/doc/html/draft-connolly-cfrg-xwing-kem-08#name-use-in-hpke
      /*auth_encap=*/nullptr,
      /*auth_encap_external_entropy=*/nullptr,
      /*auth_decap=*/nullptr,
  };
  return &kKEM;
}

namespace {

constexpr HpkeKeyMethod kMlkem768KeyMethod = {
    /*hpke_kem_id=*/EVP_HPKE_MLKEM768,
    /*public_key_len=*/MLKEM768_PUBLIC_KEY_BYTES,
    /*private_key_len=*/MLKEM_SEED_BYTES,
    /*alg_func=*/&EVP_pkey_ml_kem_768,
    /*deserialize_private_key=*/&EVP_PKEY_from_private_seed,
    /*serialize_private_key=*/&EVP_PKEY_get_private_seed,
    /*deserialize_public_key=*/&EVP_PKEY_from_raw_public_key,
    /*serialize_public_key=*/&EVP_PKEY_get_raw_public_key,
    /*derive_key_pair=*/
    shake256_derive_key_pair_from_private_seed<MLKEM_SEED_BYTES>,
};

constexpr HpkeKeyMethod kMlkem1024KeyMethod = {
    /*hpke_kem_id=*/EVP_HPKE_MLKEM1024,
    /*public_key_len=*/MLKEM1024_PUBLIC_KEY_BYTES,
    /*private_key_len=*/MLKEM_SEED_BYTES,
    /*alg_func=*/&EVP_pkey_ml_kem_1024,
    /*deserialize_private_key=*/&EVP_PKEY_from_private_seed,
    /*serialize_private_key=*/&EVP_PKEY_get_private_seed,
    /*deserialize_public_key=*/&EVP_PKEY_from_raw_public_key,
    /*serialize_public_key=*/&EVP_PKEY_get_raw_public_key,
    /*derive_key_pair=*/
    shake256_derive_key_pair_from_private_seed<MLKEM_SEED_BYTES>,
};

}  // namespace

const EVP_HPKE_KEM *EVP_hpke_mlkem768() {
  static constexpr EVP_HPKE_KEM kKEM = {
      /*id=*/EVP_HPKE_MLKEM768,
      /*evp_kem_func=*/&EVP_kem_ml_kem_768,
      /*key_method=*/&kMlkem768KeyMethod,
      // ML-KEM doesn't support authenticated encapsulation/decapsulation:
      // https://datatracker.ietf.org/doc/draft-ietf-hpke-pq/01/
      /*auth_encap=*/nullptr,
      /*auth_encap_external_entropy=*/nullptr,
      /*auth_decap=*/nullptr,
  };
  return &kKEM;
}

const EVP_HPKE_KEM *EVP_hpke_mlkem1024() {
  static constexpr EVP_HPKE_KEM kKEM = {
      /*id=*/EVP_HPKE_MLKEM1024,
      /*evp_kem_func=*/&EVP_kem_ml_kem_1024,
      /*key_method=*/&kMlkem1024KeyMethod,
      // ML-KEM doesn't support authenticated encapsulation/decapsulation:
      // https://datatracker.ietf.org/doc/draft-ietf-hpke-pq/01/
      /*auth_encap=*/nullptr,
      /*auth_encap_external_entropy=*/nullptr,
      /*auth_decap=*/nullptr,
  };
  return &kKEM;
}

uint16_t EVP_HPKE_KEM_id(const EVP_HPKE_KEM *kem) { return kem->id; }

size_t EVP_HPKE_KEM_public_key_len(const EVP_HPKE_KEM *kem) {
  return kem->key_method->public_key_len;
}

size_t EVP_HPKE_KEM_private_key_len(const EVP_HPKE_KEM *kem) {
  return kem->key_method->private_key_len;
}

size_t EVP_HPKE_KEM_enc_len(const EVP_HPKE_KEM *kem) {
  return EVP_KEM_ciphertext_len(kem->evp_kem_func());
}

size_t EVP_HPKE_KEM_shared_secret_len(const EVP_HPKE_KEM *kem) {
  return EVP_KEM_secret_len(kem->evp_kem_func());
}

void EVP_HPKE_KEY_zero(EVP_HPKE_KEY *key) {
  OPENSSL_memset(key, 0, sizeof(EVP_HPKE_KEY));
}

void EVP_HPKE_KEY_cleanup(EVP_HPKE_KEY *key) {
  EVP_PKEY_free(key->pkey);
  key->pkey = nullptr;
}

EVP_HPKE_KEY *EVP_HPKE_KEY_new() {
  EVP_HPKE_KEY *key = New<EVP_HPKE_KEY>();
  if (key == nullptr) {
    return nullptr;
  }
  EVP_HPKE_KEY_zero(key);
  return key;
}

void EVP_HPKE_KEY_free(EVP_HPKE_KEY *key) {
  if (key != nullptr) {
    EVP_HPKE_KEY_cleanup(key);
    Delete(key);
  }
}

int EVP_HPKE_KEY_copy(EVP_HPKE_KEY *dst, const EVP_HPKE_KEY *src) {
  dst->kem = src->kem;
  OPENSSL_memcpy(dst->private_key, src->private_key, sizeof(src->private_key));
  OPENSSL_memcpy(dst->public_key, src->public_key, sizeof(src->public_key));
  dst->pkey = EVP_PKEY_dup_ref(src->pkey);
  return 1;
}

void EVP_HPKE_KEY_move(EVP_HPKE_KEY *out, EVP_HPKE_KEY *in) {
  EVP_HPKE_KEY_cleanup(out);
  // For now, `EVP_HPKE_KEY` is trivially movable.
  // Note that Rust may move this structure. See
  // bssl-crypto/src/scoped.rs:EvpHpkeKey.
  *out = *in;
  EVP_HPKE_KEY_zero(in);
}

int EVP_HPKE_KEY_init(EVP_HPKE_KEY *key, const EVP_HPKE_KEM *kem,
                      const uint8_t *priv_key, size_t priv_key_len) {
  EVP_HPKE_KEY_zero(key);
  key->kem = kem;
  UniquePtr<EVP_PKEY> pkey(kem->key_method->deserialize_private_key(
      kem->key_method->alg_func(), priv_key, priv_key_len));
  if (pkey == nullptr) {
    key->kem = nullptr;
    return 0;
  }
  key->pkey = pkey.release();
  return 1;
}

int EVP_HPKE_KEY_generate(EVP_HPKE_KEY *key, const EVP_HPKE_KEM *kem) {
  EVP_HPKE_KEY_zero(key);
  key->kem = kem;
  UniquePtr<EVP_PKEY> pkey(
      EVP_PKEY_generate_from_alg(kem->key_method->alg_func()));
  if (pkey == nullptr) {
    key->kem = nullptr;
    return 0;
  }
  key->pkey = pkey.release();
  return 1;
}

int EVP_HPKE_KEY_derive(EVP_HPKE_KEY *key, const EVP_HPKE_KEM *kem,
                        const uint8_t *ikm, size_t ikm_len) {
  EVP_HPKE_KEY_zero(key);
  key->kem = kem;
  UniquePtr<EVP_PKEY> pkey(kem->key_method->derive_key_pair(
      kem->key_method->alg_func(), kem->id, Span(ikm, ikm_len)));
  if (pkey == nullptr) {
    key->kem = nullptr;
    return 0;
  }
  key->pkey = pkey.release();
  return 1;
}

const EVP_HPKE_KEM *EVP_HPKE_KEY_kem(const EVP_HPKE_KEY *key) {
  return key->kem;
}

int EVP_HPKE_KEY_public_key(const EVP_HPKE_KEY *key, uint8_t *out,
                            size_t *out_len, size_t max_out) {
  if (max_out < EVP_HPKE_KEM_public_key_len(key->kem)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_BUFFER_SIZE);
    return 0;
  }
  *out_len = max_out;
  return key->kem->key_method->serialize_public_key(key->pkey, out, out_len);
}

int EVP_HPKE_KEY_private_key(const EVP_HPKE_KEY *key, uint8_t *out,
                             size_t *out_len, size_t max_out) {
  if (max_out < EVP_HPKE_KEM_private_key_len(key->kem)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_BUFFER_SIZE);
    return 0;
  }
  *out_len = max_out;
  return key->kem->key_method->serialize_private_key(key->pkey, out, out_len);
}


// Supported KDFs and AEADs.

const EVP_HPKE_KDF *EVP_hpke_hkdf_sha256() {
  static const EVP_HPKE_KDF kKDF = {EVP_HPKE_HKDF_SHA256, &EVP_sha256};
  return &kKDF;
}

const EVP_HPKE_KDF *EVP_hpke_hkdf_sha384() {
  static const EVP_HPKE_KDF kKDF = {EVP_HPKE_HKDF_SHA384, &EVP_sha384};
  return &kKDF;
}

uint16_t EVP_HPKE_KDF_id(const EVP_HPKE_KDF *kdf) { return kdf->id; }

const EVP_MD *EVP_HPKE_KDF_hkdf_md(const EVP_HPKE_KDF *kdf) {
  return kdf->hkdf_md_func();
}

const EVP_HPKE_AEAD *EVP_hpke_aes_128_gcm() {
  static const EVP_HPKE_AEAD kAEAD = {EVP_HPKE_AES_128_GCM,
                                      &EVP_aead_aes_128_gcm};
  return &kAEAD;
}

const EVP_HPKE_AEAD *EVP_hpke_aes_256_gcm() {
  static const EVP_HPKE_AEAD kAEAD = {EVP_HPKE_AES_256_GCM,
                                      &EVP_aead_aes_256_gcm};
  return &kAEAD;
}

const EVP_HPKE_AEAD *EVP_hpke_chacha20_poly1305() {
  static const EVP_HPKE_AEAD kAEAD = {EVP_HPKE_CHACHA20_POLY1305,
                                      &EVP_aead_chacha20_poly1305};
  return &kAEAD;
}

uint16_t EVP_HPKE_AEAD_id(const EVP_HPKE_AEAD *aead) { return aead->id; }

const EVP_AEAD *EVP_HPKE_AEAD_aead(const EVP_HPKE_AEAD *aead) {
  return aead->aead_func();
}


// HPKE implementation.

#define HPKE_MODE_BASE 0
#define HPKE_MODE_AUTH 2

namespace {

std::array<uint8_t, 10> hpke_full_suite_id(const EVP_HPKE_CTX *ctx) {
  return {
      'H',
      'P',
      'K',
      'E',
      static_cast<uint8_t>(ctx->kem->id >> 8),
      static_cast<uint8_t>(ctx->kem->id),
      static_cast<uint8_t>(ctx->kdf->id >> 8),
      static_cast<uint8_t>(ctx->kdf->id),
      static_cast<uint8_t>(ctx->aead->id >> 8),
      static_cast<uint8_t>(ctx->aead->id),
  };
}
int hpke_key_schedule(EVP_HPKE_CTX *ctx, uint8_t mode,
                      const uint8_t *shared_secret, size_t shared_secret_len,
                      const uint8_t *info, size_t info_len) {
  auto suite_id = hpke_full_suite_id(ctx);

  // psk_id_hash = LabeledExtract("", "psk_id_hash", psk_id)
  // TODO(davidben): Precompute this value and store it with the EVP_HPKE_KDF.
  const EVP_MD *hkdf_md = ctx->kdf->hkdf_md_func();
  uint8_t psk_id_hash[EVP_MAX_MD_SIZE];
  size_t psk_id_hash_len;
  if (!hpke_labeled_extract(hkdf_md, psk_id_hash, &psk_id_hash_len, nullptr, 0,
                            suite_id.data(), suite_id.size(), "psk_id_hash",
                            nullptr, 0)) {
    return 0;
  }

  // info_hash = LabeledExtract("", "info_hash", info)
  uint8_t info_hash[EVP_MAX_MD_SIZE];
  size_t info_hash_len;
  if (!hpke_labeled_extract(hkdf_md, info_hash, &info_hash_len, nullptr, 0,
                            suite_id.data(), suite_id.size(), "info_hash", info,
                            info_len)) {
    return 0;
  }

  // key_schedule_context = concat(mode, psk_id_hash, info_hash)
  uint8_t context[sizeof(uint8_t) + 2 * EVP_MAX_MD_SIZE];
  size_t context_len;
  CBB context_cbb;
  CBB_init_fixed(&context_cbb, context, sizeof(context));
  if (!CBB_add_u8(&context_cbb, mode) ||
      !CBB_add_bytes(&context_cbb, psk_id_hash, psk_id_hash_len) ||
      !CBB_add_bytes(&context_cbb, info_hash, info_hash_len) ||
      !CBB_finish(&context_cbb, nullptr, &context_len)) {
    return 0;
  }

  // secret = LabeledExtract(shared_secret, "secret", psk)
  uint8_t secret[EVP_MAX_MD_SIZE];
  size_t secret_len;
  if (!hpke_labeled_extract(hkdf_md, secret, &secret_len, shared_secret,
                            shared_secret_len, suite_id.data(), suite_id.size(),
                            "secret", nullptr, 0)) {
    return 0;
  }

  // key = LabeledExpand(secret, "key", key_schedule_context, Nk)
  const EVP_AEAD *aead = EVP_HPKE_AEAD_aead(ctx->aead);
  uint8_t key[EVP_AEAD_MAX_KEY_LENGTH];
  const size_t kKeyLen = EVP_AEAD_key_length(aead);
  if (!hpke_labeled_expand(hkdf_md, key, kKeyLen, secret, secret_len,
                           suite_id.data(), suite_id.size(), "key", context,
                           context_len) ||
      !EVP_AEAD_CTX_init(&ctx->aead_ctx, aead, key, kKeyLen,
                         EVP_AEAD_DEFAULT_TAG_LENGTH, nullptr)) {
    return 0;
  }

  // base_nonce = LabeledExpand(secret, "base_nonce", key_schedule_context, Nn)
  if (!hpke_labeled_expand(hkdf_md, ctx->base_nonce,
                           EVP_AEAD_nonce_length(aead), secret, secret_len,
                           suite_id.data(), suite_id.size(), "base_nonce",
                           context, context_len)) {
    return 0;
  }

  // exporter_secret = LabeledExpand(secret, "exp", key_schedule_context, Nh)
  if (!hpke_labeled_expand(hkdf_md, ctx->exporter_secret, EVP_MD_size(hkdf_md),
                           secret, secret_len, suite_id.data(), suite_id.size(),
                           "exp", context, context_len)) {
    return 0;
  }

  return 1;
}

int hpke_setup_sender_impl(EVP_HPKE_CTX *ctx, uint8_t *out_enc,
                           size_t *out_enc_len, size_t max_enc,
                           const EVP_HPKE_KEM *kem, const EVP_HPKE_KDF *kdf,
                           const EVP_HPKE_AEAD *aead,
                           Span<const uint8_t> peer_public_key,
                           Span<const uint8_t> info,
                           std::optional<Span<const uint8_t>> entropy) {
  EVP_HPKE_CTX_zero(ctx);
  ctx->is_sender = 1;
  ctx->kem = kem;
  ctx->kdf = kdf;
  ctx->aead = aead;

  const EVP_KEM *evp_kem = kem->evp_kem_func();
  if (max_enc < EVP_KEM_ciphertext_len(evp_kem)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_BUFFER_SIZE);
    return 0;
  }
  *out_enc_len = EVP_KEM_ciphertext_len(evp_kem);

  UniquePtr<EVP_PKEY> peer_pubkey(kem->key_method->deserialize_public_key(
      kem->key_method->alg_func(), peer_public_key.data(),
      peer_public_key.size()));
  if (peer_pubkey == nullptr) {
    return 0;
  }
  InplaceVector<uint8_t, MAX_SHARED_SECRET_LEN> shared_secret;
  if (!shared_secret.TryResizeForOverwrite(evp_kem->secret_len)) {
    OPENSSL_PUT_ERROR(EVP, ERR_R_INTERNAL_ERROR);
    return 0;
  }
  int encap_ok =
      entropy.has_value()
          ? EVP_KEM_encap_external_entropy_for_testing(
                evp_kem, out_enc, *out_enc_len, shared_secret.data(),
                shared_secret.size(), peer_pubkey.get(), entropy->data(),
                entropy->size())
          : EVP_KEM_encap(evp_kem, out_enc, *out_enc_len, shared_secret.data(),
                          shared_secret.size(), peer_pubkey.get());
  if (!encap_ok ||
      !hpke_key_schedule(ctx, HPKE_MODE_BASE, shared_secret.data(),
                         shared_secret.size(), info.data(), info.size())) {
    EVP_HPKE_CTX_cleanup(ctx);
    return 0;
  }
  return 1;
}

int hpke_setup_auth_sender_impl(
    EVP_HPKE_CTX *ctx, uint8_t *out_enc, size_t *out_enc_len, size_t max_enc,
    const EVP_HPKE_KEY *key, const EVP_HPKE_KDF *kdf, const EVP_HPKE_AEAD *aead,
    Span<const uint8_t> peer_public_key, Span<const uint8_t> info,
    std::optional<Span<const uint8_t>> entropy) {
  if (entropy.has_value() ? key->kem->auth_encap_external_entropy == nullptr
                          : key->kem->auth_encap == nullptr) {
    // Not all HPKE KEMs support AuthEncap.
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }

  const EVP_KEM *kem = key->kem->evp_kem_func();
  EVP_HPKE_CTX_zero(ctx);
  ctx->is_sender = 1;
  ctx->kem = key->kem;
  ctx->kdf = kdf;
  ctx->aead = aead;
  if (max_enc < kem->ciphertext_len) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_BUFFER_SIZE);
    return 0;
  }
  UniquePtr<EVP_PKEY> peer_key(key->kem->key_method->deserialize_public_key(
      key->kem->key_method->alg_func(), peer_public_key.data(),
      peer_public_key.size()));
  if (peer_key == nullptr) {
    return 0;
  }

  InplaceVector<uint8_t, MAX_SHARED_SECRET_LEN> shared_secret;
  if (!shared_secret.TryResizeForOverwrite(kem->secret_len)) {
    OPENSSL_PUT_ERROR(EVP, ERR_R_INTERNAL_ERROR);
    return 0;
  }
  Span<uint8_t> enc_span(out_enc, kem->ciphertext_len);
  Span<uint8_t> secret_span(shared_secret);
  int auth_encap_ok =
      entropy.has_value()
          ? key->kem->auth_encap_external_entropy(
                kem, enc_span, secret_span, peer_key.get(), key->pkey, *entropy)
          : key->kem->auth_encap(kem, enc_span, secret_span, peer_key.get(),
                                 key->pkey);
  if (!auth_encap_ok ||
      !hpke_key_schedule(ctx, HPKE_MODE_AUTH, secret_span.data(),
                         secret_span.size(), info.data(), info.size())) {
    EVP_HPKE_CTX_cleanup(ctx);
    return 0;
  }
  *out_enc_len = kem->ciphertext_len;
  return 1;
}

void hpke_nonce(const EVP_HPKE_CTX *ctx, uint8_t *out_nonce, size_t nonce_len) {
  assert(nonce_len >= 8);

  // Write padded big-endian bytes of `ctx->seq` to `out_nonce`.
  OPENSSL_memset(out_nonce, 0, nonce_len);
  uint64_t seq_copy = ctx->seq;
  for (size_t i = 0; i < 8; i++) {
    out_nonce[nonce_len - i - 1] = seq_copy & 0xff;
    seq_copy >>= 8;
  }

  // XOR the encoded sequence with the `ctx->base_nonce`.
  for (size_t i = 0; i < nonce_len; i++) {
    out_nonce[i] ^= ctx->base_nonce[i];
  }
}

}  // namespace

void EVP_HPKE_CTX_zero(EVP_HPKE_CTX *ctx) {
  OPENSSL_memset(ctx, 0, sizeof(EVP_HPKE_CTX));
  EVP_AEAD_CTX_zero(&ctx->aead_ctx);
}

void EVP_HPKE_CTX_cleanup(EVP_HPKE_CTX *ctx) {
  EVP_AEAD_CTX_cleanup(&ctx->aead_ctx);
}

EVP_HPKE_CTX *EVP_HPKE_CTX_new() {
  EVP_HPKE_CTX *ctx = New<EVP_HPKE_CTX>();
  if (ctx == nullptr) {
    return nullptr;
  }
  EVP_HPKE_CTX_zero(ctx);
  return ctx;
}

void EVP_HPKE_CTX_free(EVP_HPKE_CTX *ctx) {
  if (ctx != nullptr) {
    EVP_HPKE_CTX_cleanup(ctx);
    Delete(ctx);
  }
}


int EVP_HPKE_CTX_setup_sender(EVP_HPKE_CTX *ctx, uint8_t *out_enc,
                              size_t *out_enc_len, size_t max_enc,
                              const EVP_HPKE_KEM *kem, const EVP_HPKE_KDF *kdf,
                              const EVP_HPKE_AEAD *aead,
                              const uint8_t *peer_public_key,
                              size_t peer_public_key_len, const uint8_t *info,
                              size_t info_len) {
  return hpke_setup_sender_impl(ctx, out_enc, out_enc_len, max_enc, kem, kdf,
                                aead,
                                Span(peer_public_key, peer_public_key_len),
                                Span(info, info_len), /*entropy=*/std::nullopt);
}

int EVP_HPKE_CTX_setup_sender_with_seed_for_testing(
    EVP_HPKE_CTX *ctx, uint8_t *out_enc, size_t *out_enc_len, size_t max_enc,
    const EVP_HPKE_KEM *kem, const EVP_HPKE_KDF *kdf, const EVP_HPKE_AEAD *aead,
    const uint8_t *peer_public_key, size_t peer_public_key_len,
    const uint8_t *info, size_t info_len, const uint8_t *seed,
    size_t seed_len) {
  return hpke_setup_sender_impl(ctx, out_enc, out_enc_len, max_enc, kem, kdf,
                                aead,
                                Span(peer_public_key, peer_public_key_len),
                                Span(info, info_len), Span(seed, seed_len));
}

int EVP_HPKE_CTX_setup_recipient_with_shared_secret(
    EVP_HPKE_CTX *ctx, const EVP_HPKE_KEM *kem, const EVP_HPKE_KDF *kdf,
    const EVP_HPKE_AEAD *aead, const uint8_t *shared_secret,
    size_t shared_secret_len, const uint8_t *info, size_t info_len) {
  EVP_HPKE_CTX_zero(ctx);

  if (shared_secret_len != EVP_HPKE_KEM_shared_secret_len(kem)) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_BUFFER_SIZE);
    return 0;
  }

  ctx->is_sender = 0;
  ctx->kem = kem;
  ctx->kdf = kdf;
  ctx->aead = aead;
  if (!hpke_key_schedule(ctx, HPKE_MODE_BASE, shared_secret, shared_secret_len,
                         info, info_len)) {
    EVP_HPKE_CTX_cleanup(ctx);
    return 0;
  }
  return 1;
}

int EVP_HPKE_CTX_setup_recipient(EVP_HPKE_CTX *ctx, const EVP_HPKE_KEY *key,
                                 const EVP_HPKE_KDF *kdf,
                                 const EVP_HPKE_AEAD *aead, const uint8_t *enc,
                                 size_t enc_len, const uint8_t *info,
                                 size_t info_len) {
  EVP_HPKE_CTX_zero(ctx);

  const EVP_KEM *evp_kem = key->kem->evp_kem_func();
  const EVP_PKEY *decap_key = key->pkey;

  InplaceVector<uint8_t, MAX_SHARED_SECRET_LEN> shared_secret;
  if (!shared_secret.TryResizeForOverwrite(evp_kem->secret_len)) {
    OPENSSL_PUT_ERROR(EVP, ERR_R_INTERNAL_ERROR);
    return 0;
  }
  return EVP_KEM_decap(evp_kem, shared_secret.data(), shared_secret.size(), enc,
                       enc_len, decap_key) &&
         EVP_HPKE_CTX_setup_recipient_with_shared_secret(
             ctx, key->kem, kdf, aead, shared_secret.data(),
             shared_secret.size(), info, info_len);
}

int EVP_HPKE_CTX_setup_auth_sender(
    EVP_HPKE_CTX *ctx, uint8_t *out_enc, size_t *out_enc_len, size_t max_enc,
    const EVP_HPKE_KEY *key, const EVP_HPKE_KDF *kdf, const EVP_HPKE_AEAD *aead,
    const uint8_t *peer_public_key, size_t peer_public_key_len,
    const uint8_t *info, size_t info_len) {
  return hpke_setup_auth_sender_impl(
      ctx, out_enc, out_enc_len, max_enc, key, kdf, aead,
      Span(peer_public_key, peer_public_key_len), Span(info, info_len),
      /*entropy=*/std::nullopt);
}

int EVP_HPKE_CTX_setup_auth_sender_with_seed_for_testing(
    EVP_HPKE_CTX *ctx, uint8_t *out_enc, size_t *out_enc_len, size_t max_enc,
    const EVP_HPKE_KEY *key, const EVP_HPKE_KDF *kdf, const EVP_HPKE_AEAD *aead,
    const uint8_t *peer_public_key, size_t peer_public_key_len,
    const uint8_t *info, size_t info_len, const uint8_t *seed,
    size_t seed_len) {
  return hpke_setup_auth_sender_impl(
      ctx, out_enc, out_enc_len, max_enc, key, kdf, aead,
      Span(peer_public_key, peer_public_key_len), Span(info, info_len),
      Span(seed, seed_len));
}

int EVP_HPKE_CTX_setup_auth_recipient(
    EVP_HPKE_CTX *ctx, const EVP_HPKE_KEY *key, const EVP_HPKE_KDF *kdf,
    const EVP_HPKE_AEAD *aead, const uint8_t *enc, size_t enc_len,
    const uint8_t *info, size_t info_len, const uint8_t *peer_public_key,
    size_t peer_public_key_len) {
  if (key->kem->auth_decap == nullptr) {
    // Not all HPKE KEMs support AuthDecap.
    OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE);
    return 0;
  }

  EVP_HPKE_CTX_zero(ctx);
  ctx->is_sender = 0;
  ctx->kem = key->kem;
  ctx->kdf = kdf;
  ctx->aead = aead;
  const EVP_KEM *kem = key->kem->evp_kem_func();
  InplaceVector<uint8_t, MAX_SHARED_SECRET_LEN> shared_secret;
  if (!shared_secret.TryResizeForOverwrite(kem->secret_len)) {
    OPENSSL_PUT_ERROR(EVP, ERR_R_INTERNAL_ERROR);
    return 0;
  }
  UniquePtr<EVP_PKEY> peer_key(key->kem->key_method->deserialize_public_key(
      key->kem->key_method->alg_func(), peer_public_key, peer_public_key_len));
  if (peer_key == nullptr) {
    return 0;
  }
  if (!key->kem->auth_decap(kem, Span(shared_secret), Span(enc, enc_len),
                            key->pkey, peer_key.get()) ||
      !hpke_key_schedule(ctx, HPKE_MODE_AUTH, shared_secret.data(),
                         shared_secret.size(), info, info_len)) {
    EVP_HPKE_CTX_cleanup(ctx);
    return 0;
  }
  return 1;
}

int EVP_HPKE_CTX_open(EVP_HPKE_CTX *ctx, uint8_t *out, size_t *out_len,
                      size_t max_out_len, const uint8_t *in, size_t in_len,
                      const uint8_t *ad, size_t ad_len) {
  if (ctx->is_sender) {
    OPENSSL_PUT_ERROR(EVP, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }
  if (ctx->seq == UINT64_MAX) {
    OPENSSL_PUT_ERROR(EVP, ERR_R_OVERFLOW);
    return 0;
  }

  uint8_t nonce[EVP_AEAD_MAX_NONCE_LENGTH];
  const size_t nonce_len = EVP_AEAD_nonce_length(ctx->aead_ctx.aead);
  hpke_nonce(ctx, nonce, nonce_len);

  if (!EVP_AEAD_CTX_open(&ctx->aead_ctx, out, out_len, max_out_len, nonce,
                         nonce_len, in, in_len, ad, ad_len)) {
    return 0;
  }
  ctx->seq++;
  return 1;
}

int EVP_HPKE_CTX_seal(EVP_HPKE_CTX *ctx, uint8_t *out, size_t *out_len,
                      size_t max_out_len, const uint8_t *in, size_t in_len,
                      const uint8_t *ad, size_t ad_len) {
  if (!ctx->is_sender) {
    OPENSSL_PUT_ERROR(EVP, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }
  if (ctx->seq == UINT64_MAX) {
    OPENSSL_PUT_ERROR(EVP, ERR_R_OVERFLOW);
    return 0;
  }

  uint8_t nonce[EVP_AEAD_MAX_NONCE_LENGTH];
  const size_t nonce_len = EVP_AEAD_nonce_length(ctx->aead_ctx.aead);
  hpke_nonce(ctx, nonce, nonce_len);

  if (!EVP_AEAD_CTX_seal(&ctx->aead_ctx, out, out_len, max_out_len, nonce,
                         nonce_len, in, in_len, ad, ad_len)) {
    return 0;
  }
  ctx->seq++;
  return 1;
}

int EVP_HPKE_CTX_export(const EVP_HPKE_CTX *ctx, uint8_t *out,
                        size_t secret_len, const uint8_t *context,
                        size_t context_len) {
  auto suite_id = hpke_full_suite_id(ctx);
  const EVP_MD *hkdf_md = ctx->kdf->hkdf_md_func();
  if (!hpke_labeled_expand(hkdf_md, out, secret_len, ctx->exporter_secret,
                           EVP_MD_size(hkdf_md), suite_id.data(),
                           suite_id.size(), "sec", context, context_len)) {
    return 0;
  }
  return 1;
}

size_t EVP_HPKE_CTX_max_overhead(const EVP_HPKE_CTX *ctx) {
  assert(ctx->is_sender);
  return EVP_AEAD_max_overhead(EVP_AEAD_CTX_aead(&ctx->aead_ctx));
}

const EVP_HPKE_KEM *EVP_HPKE_CTX_kem(const EVP_HPKE_CTX *ctx) {
  return ctx->kem;
}

const EVP_HPKE_AEAD *EVP_HPKE_CTX_aead(const EVP_HPKE_CTX *ctx) {
  return ctx->aead;
}

const EVP_HPKE_KDF *EVP_HPKE_CTX_kdf(const EVP_HPKE_CTX *ctx) {
  return ctx->kdf;
}

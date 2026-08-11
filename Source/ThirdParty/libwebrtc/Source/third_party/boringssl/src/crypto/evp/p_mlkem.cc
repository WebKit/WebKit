// Copyright 2026 The BoringSSL Authors
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

#include <openssl/evp.h>

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <type_traits>

#include <openssl/bytestring.h>
#include <openssl/err.h>
#include <openssl/evp_errors.h>
#include <openssl/mlkem.h>
#include <openssl/nid.h>
#include <openssl/span.h>

#include "../fipsmodule/bcm_interface.h"
#include "../internal.h"
#include "../mem_internal.h"
#include "internal.h"

using namespace bssl;

namespace {

constexpr CBS_ASN1_TAG kSeedTag = CBS_ASN1_CONTEXT_SPECIFIC | 0;

constexpr uint8_t kMLKEM768OID[] = {OBJ_ENC_ML_KEM_768};
constexpr uint8_t kMLKEM1024OID[] = {OBJ_ENC_ML_KEM_1024};

// Generate EVP bindings for multiple ML-KEM algorithms.
#define MAKE_MLKEM_TRAITS(x)                                                   \
  struct MLKEM##x##Traits {                                                    \
    using PublicKey = MLKEM##x##_public_key;                                   \
    using PrivateKey = MLKEM##x##_private_key;                                 \
    static constexpr size_t kPublicKeyBytes = MLKEM##x##_PUBLIC_KEY_BYTES;     \
    static constexpr size_t kCiphertextBytes = MLKEM##x##_CIPHERTEXT_BYTES;    \
    static constexpr int kType = EVP_PKEY_ML_KEM_##x;                          \
    static constexpr Span<const uint8_t> kOID = kMLKEM##x##OID;                \
    static constexpr auto GenerateKey = &MLKEM##x##_generate_key;              \
    static constexpr auto PrivateKeyFromSeed =                                 \
        &MLKEM##x##_private_key_from_seed;                                     \
    static constexpr auto PublicOfPrivate = &BCM_mlkem##x##_public_of_private; \
    static constexpr auto PublicKeysEqual = &BCM_mlkem##x##_public_keys_equal; \
    static constexpr auto Encap = &MLKEM##x##_encap;                           \
    static constexpr auto Decap = &MLKEM##x##_decap;                           \
    static constexpr auto MarshalPublicKey = &MLKEM##x##_marshal_public_key;   \
    static constexpr auto ParsePublicKey = &MLKEM##x##_parse_public_key;       \
    static_assert(std::is_trivially_copyable_v<PublicKey>,                     \
                  "PublicKey type must be trivially copyable.");               \
  };

MAKE_MLKEM_TRAITS(768)
MAKE_MLKEM_TRAITS(1024)

template <typename Traits>
class PrivateKeyData;

// The private key type contains the public key struct in it, so we use a
// pointer to either a PrivateKeyData<Traits> or PublicKeyData<Traits>, with a
// common base class to dispatch between them.
template <typename Traits>
class KeyData {
 public:
  // Returns the underlying public key for the key.
  const typename Traits::PublicKey *GetPublicKey() const;

  // Returns the PrivateKeyData struct for the key, or nullptr if this is a
  // public key.
  PrivateKeyData<Traits> *AsPrivateKeyData();
  const PrivateKeyData<Traits> *AsPrivateKeyData() const {
    return const_cast<KeyData *>(this)->AsPrivateKeyData();
  }

  // A KeyData cannot be freed directly. Rather, it must use this wrapper which
  // calls the correct subclass's destructor.
  static void Free(KeyData *data);

 protected:
  explicit KeyData(bool is_private) : is_private_(is_private) {}
  ~KeyData() = default;
  bool is_private_;
};

template <typename Traits>
class PublicKeyData : public KeyData<Traits> {
 public:
  enum { kAllowUniquePtr = true };
  PublicKeyData() : KeyData<Traits>(/*is_private=*/false) {}

  // Allows copying the PublicKey.
  explicit PublicKeyData(const typename Traits::PublicKey &key)
      : KeyData<Traits>(/*is_private=*/false), pub(key) {}

  typename Traits::PublicKey pub;
};

template <typename Traits>
class PrivateKeyData : public KeyData<Traits> {
 public:
  enum { kAllowUniquePtr = true };
  PrivateKeyData() : KeyData<Traits>(/*is_private=*/true) {}
  typename Traits::PrivateKey priv;
  uint8_t seed[MLKEM_SEED_BYTES];
};

template <typename Traits>
const typename Traits::PublicKey *KeyData<Traits>::GetPublicKey() const {
  auto *priv_data = AsPrivateKeyData();
  if (priv_data != nullptr) {
    return Traits::PublicOfPrivate(&priv_data->priv);
  }
  return &static_cast<const PublicKeyData<Traits> *>(this)->pub;
}

template <typename Traits>
PrivateKeyData<Traits> *KeyData<Traits>::AsPrivateKeyData() {
  if (is_private_) {
    return static_cast<PrivateKeyData<Traits> *>(this);
  }
  return nullptr;
}

template <typename Traits>
void KeyData<Traits>::Free(KeyData<Traits> *data) {
  if (data == nullptr) {
    return;
  }
  // Delete the more specific subclass. This is moot for now, because neither
  // type has a non-trivial destructor.
  auto *priv_data = data->AsPrivateKeyData();
  if (priv_data) {
    Delete(priv_data);
  } else {
    Delete(static_cast<PublicKeyData<Traits> *>(data));
  }
}

template <typename Traits>
struct MLKEMImplementation {
  static KeyData<Traits> *GetKeyData(EvpPkey *pkey) {
    assert(pkey->ameth == &asn1_method);
    return static_cast<KeyData<Traits> *>(pkey->pkey);
  }

  static const KeyData<Traits> *GetKeyData(const EvpPkey *pkey) {
    return GetKeyData(const_cast<EvpPkey *>(pkey));
  }

  static void PkeyFree(EvpPkey *pkey) {
    KeyData<Traits>::Free(GetKeyData(pkey));
    pkey->pkey = nullptr;
  }

  static int SetPrivateSeed(EvpPkey *pkey, const uint8_t *in, size_t len) {
    auto priv = MakeUnique<PrivateKeyData<Traits>>();
    if (priv == nullptr) {
      return 0;
    }

    if (len != MLKEM_SEED_BYTES ||
        !Traits::PrivateKeyFromSeed(&priv->priv, in, len)) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_DECODE_ERROR);
      return 0;
    }
    OPENSSL_memcpy(priv->seed, in, len);
    evp_pkey_set0(pkey, &asn1_method, priv.release());
    return 1;
  }

  static int SetRawPublic(EvpPkey *pkey, const uint8_t *in, size_t len) {
    auto pub = MakeUnique<PublicKeyData<Traits>>();
    if (pub == nullptr) {
      return 0;
    }
    CBS cbs;
    CBS_init(&cbs, in, len);
    if (!Traits::ParsePublicKey(&pub->pub, &cbs) || CBS_len(&cbs) != 0) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_DECODE_ERROR);
      return 0;
    }
    evp_pkey_set0(pkey, &asn1_method, pub.release());
    return 1;
  }

  static int GetPrivateSeed(const EvpPkey *pkey, uint8_t *out,
                            size_t *out_len) {
    const auto *priv = GetKeyData(pkey)->AsPrivateKeyData();
    if (priv == nullptr) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_NOT_A_PRIVATE_KEY);
      return 0;
    }
    if (out == nullptr) {
      *out_len = MLKEM_SEED_BYTES;
      return 1;
    }
    if (*out_len < MLKEM_SEED_BYTES) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_BUFFER_TOO_SMALL);
      return 0;
    }
    OPENSSL_memcpy(out, priv->seed, MLKEM_SEED_BYTES);
    *out_len = MLKEM_SEED_BYTES;
    return 1;
  }

  static int GetRawPublic(const EvpPkey *pkey, uint8_t *out, size_t *out_len) {
    const auto *pub = GetKeyData(pkey)->GetPublicKey();
    if (out == nullptr) {
      *out_len = Traits::kPublicKeyBytes;
      return 1;
    }
    if (*out_len < Traits::kPublicKeyBytes) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_BUFFER_TOO_SMALL);
      return 0;
    }
    CBB cbb;
    CBB_init_fixed(&cbb, out, Traits::kPublicKeyBytes);
    BSSL_CHECK(Traits::MarshalPublicKey(&cbb, pub));
    BSSL_CHECK(CBB_len(&cbb) == Traits::kPublicKeyBytes);
    *out_len = Traits::kPublicKeyBytes;
    return 1;
  }

  static evp_decode_result_t DecodePublic(const EVP_PKEY_ALG *alg, EvpPkey *out,
                                          CBS *params, CBS *key) {
    // Parameters must be absent. See RFC 9935, section 3.
    if (CBS_len(params) != 0) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_DECODE_ERROR);
      return evp_decode_error;
    }
    return SetRawPublic(out, CBS_data(key), CBS_len(key)) ? evp_decode_ok
                                                          : evp_decode_error;
  }

  static int EncodePublic(CBB *out, const EvpPkey *pkey) {
    const auto *pub = GetKeyData(pkey)->GetPublicKey();
    // See RFC 9935, section 4.
    CBB spki, algorithm, key_bitstring;
    if (!CBB_add_asn1(out, &spki, CBS_ASN1_SEQUENCE) ||
        !CBB_add_asn1(&spki, &algorithm, CBS_ASN1_SEQUENCE) ||
        !CBB_add_asn1_element(&algorithm, CBS_ASN1_OBJECT, Traits::kOID.data(),
                              Traits::kOID.size()) ||
        !CBB_add_asn1(&spki, &key_bitstring, CBS_ASN1_BITSTRING) ||
        !CBB_add_u8(&key_bitstring, 0 /* no unused bits */) ||
        !Traits::MarshalPublicKey(&key_bitstring, pub) ||  //
        !CBB_flush(out)) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_ENCODE_ERROR);
      return 0;
    }
    return 1;
  }

  static bool EqualPublic(const EvpPkey *a, const EvpPkey *b) {
    const auto *a_pub = GetKeyData(a)->GetPublicKey();
    const auto *b_pub = GetKeyData(b)->GetPublicKey();
    return Traits::PublicKeysEqual(a_pub, b_pub);
  }

  static bool HasPublic(const EvpPkey *pk) { return true; }

  static bool CopyPublic(EvpPkey *out, const EvpPkey *pk) {
    auto *public_copy =
        New<PublicKeyData<Traits>>(*GetKeyData(pk)->GetPublicKey());
    if (public_copy == nullptr) {
      return false;
    }
    evp_pkey_set0(out, pk->ameth, public_copy);
    return true;
  }

  static evp_decode_result_t DecodePrivate(const EVP_PKEY_ALG *alg,
                                           EvpPkey *out, CBS *params,
                                           CBS *key) {
    // Parameters must be absent. See RFC 9935, section 3.
    if (CBS_len(params) != 0) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_DECODE_ERROR);
      return evp_decode_error;
    }

    // See RFC 9935, section 6. Three different encodings are specified. We only
    // implement the "seed" representation.
    CBS seed;
    if (!CBS_get_asn1(key, &seed, kSeedTag)) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_PRIVATE_KEY_WAS_NOT_SEED);
      return evp_decode_error;
    }
    if (CBS_len(key) != 0) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_DECODE_ERROR);
      return evp_decode_error;
    }
    return SetPrivateSeed(out, CBS_data(&seed), CBS_len(&seed))
               ? evp_decode_ok
               : evp_decode_error;
  }

  static int EncodePrivate(CBB *out, const EvpPkey *pkey) {
    const auto *priv = GetKeyData(pkey)->AsPrivateKeyData();
    if (priv == nullptr) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_NOT_A_PRIVATE_KEY);
      return 0;
    }

    // See RFC 9935, section 6. Three different encodings are specified. We only
    // implement the "seed" representation.
    CBB pkcs8, algorithm, private_key;
    if (!CBB_add_asn1(out, &pkcs8, CBS_ASN1_SEQUENCE) ||
        !CBB_add_asn1_uint64(&pkcs8, 0 /* version */) ||
        !CBB_add_asn1(&pkcs8, &algorithm, CBS_ASN1_SEQUENCE) ||
        !CBB_add_asn1_element(&algorithm, CBS_ASN1_OBJECT, Traits::kOID.data(),
                              Traits::kOID.size()) ||
        !CBB_add_asn1(&pkcs8, &private_key, CBS_ASN1_OCTETSTRING) ||
        !CBB_add_asn1_element(&private_key, kSeedTag, priv->seed,
                              sizeof(priv->seed)) ||
        !CBB_flush(out)) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_ENCODE_ERROR);
      return 0;
    }
    return 1;
  }

  static bool HasPrivate(const EvpPkey *pk) {
    return GetKeyData(pk)->AsPrivateKeyData() != nullptr;
  }

  static int PkeySize(const EvpPkey *pkey) { return Traits::kCiphertextBytes; }
  static int PkeyBits(const EvpPkey *pkey) {
    return Traits::kPublicKeyBytes * 8;
  }

  static int CopyCtx(EvpPkeyCtx *dst, EvpPkeyCtx *src) { return 1; }

  static int KeyGen(EvpPkeyCtx *ctx, EvpPkey *pkey) {
    auto priv = MakeUnique<PrivateKeyData<Traits>>();
    if (priv == nullptr) {
      OPENSSL_PUT_ERROR(EVP, ERR_R_INTERNAL_ERROR);
      return 0;
    }
    uint8_t unused_public[Traits::kPublicKeyBytes];
    Traits::GenerateKey(unused_public, priv->seed, &priv->priv);
    evp_pkey_set0(pkey, &asn1_method, priv.release());
    return 1;
  }

  static int KemEncap(uint8_t *out_ciphertext, size_t ciphertext_len,
                      uint8_t *out_secret, size_t secret_len,
                      const EVP_PKEY *peer_key) {
    const auto *peer_pubkey = GetKeyData(FromOpaque(peer_key))->GetPublicKey();
    if (ciphertext_len != Traits::kCiphertextBytes) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_CIPHERTEXT_LENGTH);
      return 0;
    }
    if (secret_len != MLKEM_SHARED_SECRET_BYTES) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_SECRET_LENGTH);
      return 0;
    }
    Traits::Encap(out_ciphertext, out_secret, peer_pubkey);
    return 1;
  }

  static int KemDecap(uint8_t *out_secret, size_t secret_len,
                      const uint8_t *ciphertext, size_t ciphertext_len,
                      const EVP_PKEY *key) {
    const auto *priv = GetKeyData(FromOpaque(key))->AsPrivateKeyData();
    if (priv == nullptr) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_NOT_A_PRIVATE_KEY);
      return 0;
    }
    if (secret_len != MLKEM_SHARED_SECRET_BYTES) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_SECRET_LENGTH);
      return 0;
    }
    return Traits::Decap(out_secret, ciphertext, ciphertext_len, &priv->priv);
  }

  static constexpr EVP_KEM evp_kem = {
      /*pkey_id=*/Traits::kType,
      /*ciphertext_len=*/Traits::kCiphertextBytes,
      /*secret_len=*/MLKEM_SHARED_SECRET_BYTES,
      &KemEncap,
      &KemDecap,
  };

  static constexpr EVP_PKEY_CTX_METHOD pkey_method = {
      Traits::kType,
      /*init=*/nullptr,
      &CopyCtx,
      /*cleanup=*/nullptr,
      &KeyGen,
      /*sign=*/nullptr,
      /*sign_message=*/nullptr,
      /*verify=*/nullptr,
      /*verify_message=*/nullptr,
      /*verify_recover=*/nullptr,
      /*encrypt=*/nullptr,
      /*decrypt=*/nullptr,
      /*derive=*/nullptr,
      /*paramgen=*/nullptr,
      &KemAdapter<evp_kem>::EncapMethod,
      &KemAdapter<evp_kem>::DecapMethod,
      /*ctrl=*/nullptr,
  };

  static constexpr EVP_PKEY_ASN1_METHOD BuildASN1Method() {
    EVP_PKEY_ASN1_METHOD ret = {
        Traits::kType,
        // The OID is filled in below.
        /*oid=*/{},
        /*oid_len=*/0,

        &pkey_method,

        &DecodePublic,
        &EncodePublic,
        &EqualPublic,
        &HasPublic,
        &CopyPublic,
        &DecodePrivate,
        &EncodePrivate,
        &HasPrivate,

        /*set_priv_raw=*/nullptr,
        &SetPrivateSeed,
        &SetRawPublic,
        /*get_priv_raw=*/nullptr,
        &GetPrivateSeed,
        &GetRawPublic,

        /*set1_tls_encodedpoint=*/nullptr,
        /*get1_tls_encodedpoint=*/nullptr,
        /*pkey_opaque=*/nullptr,
        &PkeySize,
        &PkeyBits,

        /*param_missing=*/nullptr,
        /*param_copy=*/nullptr,
        /*param_equal=*/nullptr,

        &PkeyFree,
    };
    // TODO(crbug.com/404286922): Use std::copy in C++20, when it's constexpr.
    // TODO(crbug.com/450823446): Better yet, make this field an InplaceVector
    // and give it a suitable constructor.
    constexpr auto oid = Traits::kOID;
    static_assert(oid.size() <= sizeof(ret.oid));
    for (size_t i = 0; i < oid.size(); i++) {
      ret.oid[i] = oid[i];
    }
    ret.oid_len = oid.size();
    return ret;
  }

  static constexpr EVP_PKEY_ASN1_METHOD asn1_method = BuildASN1Method();
  static constexpr EVP_PKEY_ALG pkey_alg = {&asn1_method, &pkey_method};
};

}  // namespace

const EVP_PKEY_ALG *EVP_pkey_ml_kem_768() {
  return &MLKEMImplementation<MLKEM768Traits>::pkey_alg;
}

const EVP_PKEY_ALG *EVP_pkey_ml_kem_1024() {
  return &MLKEMImplementation<MLKEM1024Traits>::pkey_alg;
}

const EVP_KEM *EVP_kem_ml_kem_768() {
  return &MLKEMImplementation<MLKEM768Traits>::evp_kem;
}

const EVP_KEM *EVP_kem_ml_kem_1024() {
  return &MLKEMImplementation<MLKEM1024Traits>::evp_kem;
}

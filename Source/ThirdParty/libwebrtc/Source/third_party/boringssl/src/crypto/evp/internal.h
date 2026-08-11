// Copyright 2000-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#ifndef OPENSSL_HEADER_CRYPTO_EVP_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_EVP_INTERNAL_H

#include <openssl/evp.h>

#include <array>

#include <openssl/span.h>

#include "../internal.h"
#include "../mem_internal.h"


DECLARE_OPAQUE_STRUCT(evp_pkey_st, EvpPkey)
DECLARE_OPAQUE_STRUCT(evp_pkey_ctx_st, EvpPkeyCtx)

BSSL_NAMESPACE_BEGIN

typedef struct evp_pkey_asn1_method_st EVP_PKEY_ASN1_METHOD;
typedef struct evp_pkey_ctx_method_st EVP_PKEY_CTX_METHOD;

BSSL_NAMESPACE_END

struct evp_pkey_alg_st {
  // method and pkey_method implement operations for this `EVP_PKEY_ALG`.
  const bssl::EVP_PKEY_ASN1_METHOD *method;
  const bssl::EVP_PKEY_CTX_METHOD *pkey_method;
};

BSSL_NAMESPACE_BEGIN

enum evp_decode_result_t {
  evp_decode_error = 0,
  evp_decode_ok = 1,
  evp_decode_unsupported = 2,
};

struct evp_pkey_asn1_method_st {
  // pkey_id contains one of the `EVP_PKEY_*` values and corresponds to the OID
  // in the key type's AlgorithmIdentifier.
  int pkey_id;
  uint8_t oid[9];
  uint8_t oid_len;

  const EVP_PKEY_CTX_METHOD *pkey_method;

  // pub_decode decodes `params` and `key` as a SubjectPublicKeyInfo
  // and writes the result into `out`. It returns `evp_decode_ok` on success,
  // and `evp_decode_error` on error, and `evp_decode_unsupported` if the input
  // was not supported by this `EVP_PKEY_ALG`. In case of
  // `evp_decode_unsupported`, it does not add an error to the error queue. May
  // modify `params` and `key`. Callers must make a copy if calling in a loop.
  //
  // `params` is the AlgorithmIdentifier after the OBJECT IDENTIFIER type field,
  // and `key` is the contents of the subjectPublicKey with the leading padding
  // byte checked and removed. Although X.509 uses BIT STRINGs to represent
  // SubjectPublicKeyInfo, every key type defined encodes the key as a byte
  // string with the same conversion to BIT STRING.
  evp_decode_result_t (*pub_decode)(const EVP_PKEY_ALG *alg, EvpPkey *out,
                                    CBS *params, CBS *key);

  // pub_encode encodes `key` as a SubjectPublicKeyInfo and appends the result
  // to `out`. It returns one on success and zero on error.
  int (*pub_encode)(CBB *out, const EvpPkey *key);

  bool (*pub_equal)(const EvpPkey *a, const EvpPkey *b);

  // pub_present returns true iff the `pk` has a public key. (If so, validity
  // is not guaranteed and should be checked separately.)
  bool (*pub_present)(const EvpPkey *pk);

  // pub_copy sets the key data of `out` to a newly allocated key data structure
  // which contains a copy of only the public key of `pk`, freeing any key
  // previously in `out`. Returns true on success or false on failure.
  bool (*pub_copy)(EvpPkey *out, const EvpPkey *pk);

  // priv_decode decodes `params` and `key` as a PrivateKeyInfo and writes the
  // result into `out`.  It returns `evp_decode_ok` on success, and
  // `evp_decode_error` on error, and `evp_decode_unsupported` if the key type
  // was not supported by this `EVP_PKEY_ALG`. In case of
  // `evp_decode_unsupported`, it does not add an error to the error queue. May
  // modify `params` and `key`. Callers must make a copy if calling in a loop.
  //
  // `params` is the AlgorithmIdentifier after the OBJECT IDENTIFIER type field,
  // and `key` is the contents of the OCTET STRING privateKey field.
  evp_decode_result_t (*priv_decode)(const EVP_PKEY_ALG *alg, EvpPkey *out,
                                     CBS *params, CBS *key);

  // priv_encode encodes `key` as a PrivateKeyInfo and appends the result to
  // `out`. It returns one on success and zero on error.
  int (*priv_encode)(CBB *out, const EvpPkey *key);

  // priv_present returns true iff the `pk` has a private key. (If so, validity
  // is not guaranteed and should be checked separately.)
  bool (*priv_present)(const EvpPkey *pk);

  int (*set_priv_raw)(EvpPkey *pkey, const uint8_t *in, size_t len);
  int (*set_priv_seed)(EvpPkey *pkey, const uint8_t *in, size_t len);
  int (*set_pub_raw)(EvpPkey *pkey, const uint8_t *in, size_t len);
  int (*get_priv_raw)(const EvpPkey *pkey, uint8_t *out, size_t *out_len);
  int (*get_priv_seed)(const EvpPkey *pkey, uint8_t *out, size_t *out_len);
  int (*get_pub_raw)(const EvpPkey *pkey, uint8_t *out, size_t *out_len);

  // TODO(davidben): Can these be merged with the functions above? OpenSSL does
  // not implement `EVP_PKEY_get_raw_public_key`, etc., for `EVP_PKEY_EC`, but
  // the distinction seems unimportant. OpenSSL 3.0 has since renamed
  // `EVP_PKEY_get1_tls_encodedpoint` to `EVP_PKEY_get1_encoded_public_key`, and
  // what is the difference between "raw" and an "encoded" public key.
  //
  // One nuisance is the notion of "raw" is slightly ambiguous for EC keys. Is
  // it a DER ECPrivateKey or just the scalar?
  int (*set1_tls_encodedpoint)(EvpPkey *pkey, const uint8_t *in, size_t len);
  size_t (*get1_tls_encodedpoint)(const EvpPkey *pkey, uint8_t **out_ptr);

  // pkey_opaque returns 1 if the `pk` is opaque. Opaque keys are backed by
  // custom implementations which do not expose key material and parameters.
  int (*pkey_opaque)(const EvpPkey *pk);

  int (*pkey_size)(const EvpPkey *pk);
  int (*pkey_bits)(const EvpPkey *pk);

  int (*param_missing)(const EvpPkey *pk);
  int (*param_copy)(EvpPkey *to, const EvpPkey *from);
  bool (*param_equal)(const EvpPkey *a, const EvpPkey *b);

  void (*pkey_free)(EvpPkey *pkey);
} /* EVP_PKEY_ASN1_METHOD */;

class EvpPkey : public evp_pkey_st, public RefCounted<EvpPkey> {
 public:
  EvpPkey();

  // pkey contains a pointer to a structure dependent on `ameth`.
  void *pkey = nullptr;

  // ameth contains a pointer to a method table that determines the key type, or
  // nullptr if the key is empty.
  const bssl::EVP_PKEY_ASN1_METHOD *ameth = nullptr;

 private:
  ~EvpPkey();
  friend RefCounted;
} /* EVP_PKEY */;

#define EVP_PKEY_OP_UNDEFINED 0
#define EVP_PKEY_OP_KEYGEN (1 << 2)
#define EVP_PKEY_OP_SIGN (1 << 3)
#define EVP_PKEY_OP_VERIFY (1 << 4)
#define EVP_PKEY_OP_VERIFYRECOVER (1 << 5)
#define EVP_PKEY_OP_ENCRYPT (1 << 6)
#define EVP_PKEY_OP_DECRYPT (1 << 7)
#define EVP_PKEY_OP_DERIVE (1 << 8)
#define EVP_PKEY_OP_PARAMGEN (1 << 9)
#define EVP_PKEY_OP_ENCAPSULATE (1 << 10)
#define EVP_PKEY_OP_DECAPSULATE (1 << 11)

#define EVP_PKEY_OP_TYPE_SIG \
  (EVP_PKEY_OP_SIGN | EVP_PKEY_OP_VERIFY | EVP_PKEY_OP_VERIFYRECOVER)

#define EVP_PKEY_OP_TYPE_CRYPT (EVP_PKEY_OP_ENCRYPT | EVP_PKEY_OP_DECRYPT)

#define EVP_PKEY_OP_TYPE_GEN (EVP_PKEY_OP_KEYGEN | EVP_PKEY_OP_PARAMGEN)

// EVP_PKEY_CTX_ctrl performs `cmd` on `ctx`. The `keytype` and `optype`
// arguments can be -1 to specify that any type and operation are acceptable,
// otherwise `keytype` must match the type of `ctx` and the bits of `optype`
// must intersect the operation flags set on `ctx`.
//
// The `p1` and `p2` arguments depend on the value of `cmd`.
//
// It returns one on success and zero on error.
OPENSSL_EXPORT int EVP_PKEY_CTX_ctrl(EVP_PKEY_CTX *ctx, int keytype, int optype,
                                     int cmd, int p1, void *p2);

#define EVP_PKEY_CTRL_MD 1
#define EVP_PKEY_CTRL_GET_MD 2

// EVP_PKEY_CTRL_PEER_KEY is called with different values of `p1`:
//   0: Is called from `EVP_PKEY_derive_set_peer` and `p2` contains a peer key.
//      If the return value is <= 0, the key is rejected.
//   1: Is called at the end of `EVP_PKEY_derive_set_peer` and `p2` contains a
//      peer key. If the return value is <= 0, the key is rejected.
//   2: Is called with `p2` == NULL to test whether the peer's key was used.
//      (EC)DH always return one in this case.
//   3: Is called with `p2` == NULL to set whether the peer's key was used.
//      (EC)DH always return one in this case. This was only used for GOST.
#define EVP_PKEY_CTRL_PEER_KEY 3

// EVP_PKEY_ALG_CTRL is the base value from which key-type specific ctrl
// commands are numbered.
#define EVP_PKEY_ALG_CTRL 0x1000

#define EVP_PKEY_CTRL_RSA_PADDING (EVP_PKEY_ALG_CTRL + 1)
#define EVP_PKEY_CTRL_GET_RSA_PADDING (EVP_PKEY_ALG_CTRL + 2)
#define EVP_PKEY_CTRL_RSA_PSS_SALTLEN (EVP_PKEY_ALG_CTRL + 3)
#define EVP_PKEY_CTRL_GET_RSA_PSS_SALTLEN (EVP_PKEY_ALG_CTRL + 4)
#define EVP_PKEY_CTRL_RSA_KEYGEN_BITS (EVP_PKEY_ALG_CTRL + 5)
#define EVP_PKEY_CTRL_RSA_KEYGEN_PUBEXP (EVP_PKEY_ALG_CTRL + 6)
#define EVP_PKEY_CTRL_RSA_OAEP_MD (EVP_PKEY_ALG_CTRL + 7)
#define EVP_PKEY_CTRL_GET_RSA_OAEP_MD (EVP_PKEY_ALG_CTRL + 8)
#define EVP_PKEY_CTRL_RSA_MGF1_MD (EVP_PKEY_ALG_CTRL + 9)
#define EVP_PKEY_CTRL_GET_RSA_MGF1_MD (EVP_PKEY_ALG_CTRL + 10)
#define EVP_PKEY_CTRL_RSA_OAEP_LABEL (EVP_PKEY_ALG_CTRL + 11)
#define EVP_PKEY_CTRL_GET_RSA_OAEP_LABEL (EVP_PKEY_ALG_CTRL + 12)
#define EVP_PKEY_CTRL_EC_PARAMGEN_GROUP (EVP_PKEY_ALG_CTRL + 13)
#define EVP_PKEY_CTRL_HKDF_MODE (EVP_PKEY_ALG_CTRL + 14)
#define EVP_PKEY_CTRL_HKDF_MD (EVP_PKEY_ALG_CTRL + 15)
#define EVP_PKEY_CTRL_HKDF_KEY (EVP_PKEY_ALG_CTRL + 16)
#define EVP_PKEY_CTRL_HKDF_SALT (EVP_PKEY_ALG_CTRL + 17)
#define EVP_PKEY_CTRL_HKDF_INFO (EVP_PKEY_ALG_CTRL + 18)
#define EVP_PKEY_CTRL_DH_PAD (EVP_PKEY_ALG_CTRL + 19)
#define EVP_PKEY_CTRL_SIGNATURE_CONTEXT_STRING (EVP_PKEY_ALG_CTRL + 20)

class EvpPkeyCtx : public evp_pkey_ctx_st {
 public:
  static constexpr bool kAllowUniquePtr = true;

  // TODO(crbug.com/487376811): Ideally this destructor should be virtual so
  // that we can emit vtables in libcrypto. In that case we would be able to
  // replace `pmeth` with virtual methods and subclassing.
  ~EvpPkeyCtx();

  // Method associated with this operation
  const bssl::EVP_PKEY_CTX_METHOD *pmeth = nullptr;
  // Key: may be nullptr
  bssl::UniquePtr<EvpPkey> pkey;
  // Peer key for key agreement, may be nullptr
  bssl::UniquePtr<EvpPkey> peerkey;
  // operation contains one of the `EVP_PKEY_OP_*` values.
  int operation = EVP_PKEY_OP_UNDEFINED;
  // Algorithm specific data.
  // TODO(crbug.com/487376811): Since a `EVP_PKEY_CTX` never has its type change
  // after creation, this should instead be a base class, with the
  // algorithm-specific data on the subclass, coming from the same allocation.
  void *data = nullptr;
};

struct evp_pkey_ctx_method_st {
  int pkey_id;

  // `alg` may be nullptr. If non-null, `ctx` will have a key set.
  int (*init)(EvpPkeyCtx *ctx, const EVP_PKEY_ALG *alg);
  int (*copy)(EvpPkeyCtx *dst, EvpPkeyCtx *src);
  void (*cleanup)(EvpPkeyCtx *ctx);

  int (*keygen)(EvpPkeyCtx *ctx, EvpPkey *pkey);

  int (*sign)(EvpPkeyCtx *ctx, uint8_t *sig, size_t *siglen, const uint8_t *tbs,
              size_t tbslen);

  int (*sign_message)(EvpPkeyCtx *ctx, uint8_t *sig, size_t *siglen,
                      const uint8_t *tbs, size_t tbslen);

  int (*verify)(EvpPkeyCtx *ctx, const uint8_t *sig, size_t siglen,
                const uint8_t *tbs, size_t tbslen);

  int (*verify_message)(EvpPkeyCtx *ctx, const uint8_t *sig, size_t siglen,
                        const uint8_t *tbs, size_t tbslen);

  int (*verify_recover)(EvpPkeyCtx *ctx, uint8_t *out, size_t *out_len,
                        const uint8_t *sig, size_t sig_len);

  int (*encrypt)(EvpPkeyCtx *ctx, uint8_t *out, size_t *outlen,
                 const uint8_t *in, size_t inlen);

  int (*decrypt)(EvpPkeyCtx *ctx, uint8_t *out, size_t *outlen,
                 const uint8_t *in, size_t inlen);

  int (*derive)(EvpPkeyCtx *ctx, uint8_t *key, size_t *keylen);

  int (*paramgen)(EvpPkeyCtx *ctx, EvpPkey *pkey);

  int (*encap)(EvpPkeyCtx *ctx, uint8_t *out_ciphertext,
               size_t *out_ciphertext_len, uint8_t *out_secret,
               size_t *out_secret_len);

  int (*decap)(EvpPkeyCtx *ctx, uint8_t *out_secret, size_t *out_secret_len,
               const uint8_t *ciphertext, size_t ciphertext_len);

  int (*ctrl)(EvpPkeyCtx *ctx, int type, int p1, void *p2);
} /* EVP_PKEY_CTX_METHOD */;

BSSL_NAMESPACE_END

// TODO(chlily): Make compatible with `EVP_HPKE_KEM`.
struct evp_kem_st {
  // Identifies the type of EVP_PKEYs compatible with this KEM.
  int pkey_id;

  // Constant lengths of ciphertexts and secrets produced/consumed by this KEM.
  size_t ciphertext_len;
  size_t secret_len;

  int (*encap)(uint8_t *out_ciphertext, size_t ciphertext_len,
               uint8_t *out_secret, size_t secret_len,
               const EVP_PKEY *peer_key);
  int (*decap)(uint8_t *out_secret, size_t secret_len,
               const uint8_t *ciphertext, size_t ciphertext_len,
               const EVP_PKEY *key);
} /* EVP_KEM */;

BSSL_NAMESPACE_BEGIN

// KemAdapter is templated on an instance of EVP_KEM, and generates static
// methods matching the behavior and function signatures for `encap` and `decap`
// in EVP_PKEY_CTX_METHOD.
template <const evp_kem_st &KEM>
struct KemAdapter {
  KemAdapter() = delete;

  static int EncapMethod(EvpPkeyCtx *ctx, uint8_t *out_ciphertext,
                         size_t *out_ciphertext_len, uint8_t *out_secret,
                         size_t *out_secret_len) {
    if (out_ciphertext == nullptr) {
      if (out_ciphertext_len != nullptr) {
        *out_ciphertext_len = KEM.ciphertext_len;
      }
      if (out_secret_len != nullptr) {
        *out_secret_len = KEM.secret_len;
      }
      return 1;
    }
    if (*out_ciphertext_len < KEM.ciphertext_len ||
        *out_secret_len < KEM.secret_len) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_BUFFER_TOO_SMALL);
      return 0;
    }
    if (KEM.encap(out_ciphertext, KEM.ciphertext_len, out_secret,
                  KEM.secret_len, ctx->pkey.get())) {
      *out_ciphertext_len = KEM.ciphertext_len;
      *out_secret_len = KEM.secret_len;
      return 1;
    }
    return 0;
  }

  static int DecapMethod(EvpPkeyCtx *ctx, uint8_t *out_secret,
                         size_t *out_secret_len, const uint8_t *ciphertext,
                         size_t ciphertext_len) {
    if (out_secret == nullptr) {
      *out_secret_len = KEM.secret_len;
      return 1;
    }
    if (*out_secret_len < KEM.secret_len) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_BUFFER_TOO_SMALL);
      return 0;
    }
    if (KEM.decap(out_secret, KEM.secret_len, ciphertext, ciphertext_len,
                  ctx->pkey.get())) {
      *out_secret_len = KEM.secret_len;
      return 1;
    }
    return 0;
  }
};

// evp_pkey_ec_no_curve returns an internal curveless EC `EVP_PKEY_ALG`. This
// cannot be used to parse anything and is only useful for key generation.
const EVP_PKEY_ALG *evp_pkey_ec_no_curve();

// evp_pkey_hkdf returns an internal `EVP_PKEY_ALG` used to implement
// `EVP_PKEY_HKDF`. It has no associated key type.
const EVP_PKEY_ALG *evp_pkey_hkdf();

// evp_pkey_ctx_new_alg behaves like `EVP_PKEY_CTX_new_id` but takes an
// `EVP_PKEY_ALG`.
UniquePtr<EvpPkeyCtx> evp_pkey_ctx_new_alg(const EVP_PKEY_ALG *alg);

// evp_pkey_set0 sets `pkey`'s method to `method` and data to `pkey_data`,
// freeing any key that may previously have been configured. This function takes
// ownership of `pkey_data`, which must be of the type expected by `method`.
void evp_pkey_set0(EvpPkey *pkey, const EVP_PKEY_ASN1_METHOD *method,
                   void *pkey_data);

inline auto GetDefaultEVPAlgorithms() {
  // A set of algorithms to use by default in `EVP_parse_public_key` and
  // `EVP_parse_private_key`.
  return std::array{
      EVP_pkey_ec_p224(),
      EVP_pkey_ec_p256(),
      EVP_pkey_ec_p384(),
      EVP_pkey_ec_p521(),
      EVP_pkey_ed25519(),
      EVP_pkey_rsa(),
      EVP_pkey_x25519(),
      EVP_pkey_ml_dsa_44(),
      EVP_pkey_ml_dsa_65(),
      EVP_pkey_ml_dsa_87(),
      EVP_pkey_ml_kem_768(),
      EVP_pkey_ml_kem_1024(),
      // TODO(crbug.com/438761503): Remove DSA from this set, after callers that
      // need DSA pass in `EVP_pkey_dsa` explicitly.
      EVP_pkey_dsa(),
  };
}

BSSL_NAMESPACE_END

#endif  // OPENSSL_HEADER_CRYPTO_EVP_INTERNAL_H

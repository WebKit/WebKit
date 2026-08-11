// Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#ifndef OPENSSL_HEADER_CRYPTO_FIPSMODULE_RSA_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_FIPSMODULE_RSA_INTERNAL_H

#include <openssl/base.h>

#include <openssl/bn.h>
#include <openssl/rsa.h>

#include "../../internal.h"
#include "../../mem_internal.h"


DECLARE_OPAQUE_STRUCT(rsa_st, RSAImpl)

BSSL_NAMESPACE_BEGIN

// TODO(crbug.com/42290480): Raise this limit. 512-bit RSA was factored in 1999.
#define OPENSSL_RSA_MIN_MODULUS_BITS 512

// TODO(davidben): This is inside BCM because `RSA` is inside BCM, but BCM never
// uses this. Split the RSA type in two.
enum rsa_pss_params_t {
  // No parameters.
  // TODO(davidben): Remove this and use std::optional where appropriate.
  rsa_pss_none = 0,
  // RSA-PSS using SHA-256, MGF1 with SHA-256, salt length 32.
  rsa_pss_sha256,
  // RSA-PSS using SHA-384, MGF1 with SHA-384, salt length 48.
  rsa_pss_sha384,
  // RSA-PSS using SHA-512, MGF1 with SHA-512, salt length 64.
  rsa_pss_sha512,
};

class RSAImpl : public rsa_st, public RefCounted<RSAImpl> {
 public:
  explicit RSAImpl(const ENGINE *engine);

  RSA_METHOD *meth = nullptr;

  UniquePtr<BIGNUM> n;
  UniquePtr<BIGNUM> e;
  UniquePtr<BIGNUM> d;
  UniquePtr<BIGNUM> p;
  UniquePtr<BIGNUM> q;
  UniquePtr<BIGNUM> dmp1;
  UniquePtr<BIGNUM> dmq1;
  UniquePtr<BIGNUM> iqmp;

  // be careful using this if the RSA structure is shared
  CRYPTO_EX_DATA ex_data = {};
  int flags = 0;

  Mutex lock;

  // Used to cache montgomery values. The creation of these values is protected
  // by `lock`.
  UniquePtr<BN_MONT_CTX> mont_n;
  UniquePtr<BN_MONT_CTX> mont_p;
  UniquePtr<BN_MONT_CTX> mont_q;

  // The following fields are copies of `d`, `dmp1`, and `dmq1`, respectively,
  // but with the correct widths to prevent side channels. These must use
  // separate copies due to threading concerns caused by OpenSSL's API
  // mistakes. See https://github.com/openssl/openssl/issues/5158 and
  // the `freeze_private_key` implementation.
  UniquePtr<BIGNUM> d_fixed, dmp1_fixed, dmq1_fixed;

  // iqmp_mont is q^-1 mod p in Montgomery form, using `mont_p`.
  UniquePtr<BIGNUM> iqmp_mont;

  // pss_params is the RSA-PSS parameters associated with the key. This is not
  // used by the low-level RSA implementation, just the EVP layer.
  bssl::rsa_pss_params_t pss_params = bssl::rsa_pss_none;

  // private_key_frozen is one if the key has been used for a private key
  // operation and may no longer be mutated.
  unsigned private_key_frozen = 0;

 private:
  friend RefCounted;
  ~RSAImpl();
};

#define RSA_PKCS1_PADDING_SIZE 11

// Default implementations of RSA operations.

const RSA_METHOD *RSA_default_method();

int rsa_default_sign_raw(RSA *rsa, size_t *out_len, uint8_t *out,
                         size_t max_out, const uint8_t *in, size_t in_len,
                         int padding);
int rsa_default_private_transform(RSA *rsa, uint8_t *out, const uint8_t *in,
                                  size_t len);


int PKCS1_MGF1(uint8_t *out, size_t len, const uint8_t *seed, size_t seed_len,
               const EVP_MD *md);
int RSA_padding_add_PKCS1_type_1(uint8_t *to, size_t to_len,
                                 const uint8_t *from, size_t from_len);
int RSA_padding_check_PKCS1_type_1(uint8_t *out, size_t *out_len,
                                   size_t max_out, const uint8_t *from,
                                   size_t from_len);
int RSA_padding_add_none(uint8_t *to, size_t to_len, const uint8_t *from,
                         size_t from_len);

// rsa_check_public_key checks that `rsa`'s public modulus and exponent are
// within DoS bounds.
int rsa_check_public_key(const RSA *rsa);

// rsa_private_transform_no_self_test calls either the method-specific
// `private_transform` function (if given) or the generic one. See the comment
// for `private_transform` in `rsa_meth_st`.
int rsa_private_transform_no_self_test(RSA *rsa, uint8_t *out,
                                       const uint8_t *in, size_t len);

// rsa_private_transform acts the same as `rsa_private_transform_no_self_test`
// but, in FIPS mode, performs an RSA self test before calling the default RSA
// implementation.
int rsa_private_transform(RSA *rsa, uint8_t *out, const uint8_t *in,
                          size_t len);

// rsa_invalidate_key is called after `rsa` has been mutated, to invalidate
// fields derived from the original structure. This function assumes exclusive
// access to `rsa`. In particular, no other thread may be concurrently signing,
// etc., with `rsa`.
void rsa_invalidate_key(RSA *rsa);


// Functions that avoid self-tests.
//
// Self-tests need to call functions that don't try and ensure that the
// self-tests have passed. These functions, in turn, need to limit themselves
// to such functions too.
//
// These functions are the same as their public versions, but skip the self-test
// check.

int rsa_verify_no_self_test(int hash_nid, const uint8_t *digest,
                            size_t digest_len, const uint8_t *sig,
                            size_t sig_len, RSA *rsa);

int rsa_verify_raw_no_self_test(RSA *rsa, size_t *out_len, uint8_t *out,
                                size_t max_out, const uint8_t *in,
                                size_t in_len, int padding);

int rsa_sign_no_self_test(int hash_nid, const uint8_t *digest,
                          size_t digest_len, uint8_t *out, unsigned *out_len,
                          RSA *rsa);

BSSL_NAMESPACE_END

#endif  // OPENSSL_HEADER_CRYPTO_FIPSMODULE_RSA_INTERNAL_H

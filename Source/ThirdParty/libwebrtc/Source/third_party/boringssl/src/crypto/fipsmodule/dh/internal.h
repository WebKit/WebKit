// Copyright 2022 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_CRYPTO_FIPSMODULE_DH_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_FIPSMODULE_DH_INTERNAL_H

#include <openssl/base.h>

#include "../../internal.h"
#include "../../mem_internal.h"


DECLARE_OPAQUE_STRUCT(dh_st, DHImpl)

BSSL_NAMESPACE_BEGIN

class DHImpl : public dh_st, public RefCounted<DHImpl> {
 public:
  DHImpl() : RefCounted(CheckSubClass()) {}

  UniquePtr<BIGNUM> p;
  UniquePtr<BIGNUM> g;
  UniquePtr<BIGNUM> q;
  UniquePtr<BIGNUM> pub_key;   // g^x mod p
  UniquePtr<BIGNUM> priv_key;  // x

  // priv_length contains the length, in bits, of the private value. If zero,
  // the private value will be the same length as `p`.
  unsigned priv_length = 0;

  mutable Mutex method_mont_p_lock;
  mutable UniquePtr<BN_MONT_CTX> method_mont_p;

 private:
  friend RefCounted;
  ~DHImpl() = default;
};

// dh_check_params_fast checks basic invariants on `dh`'s domain parameters. It
// does not check that `dh` forms a valid group, only that the sizes are within
// DoS bounds.
int dh_check_params_fast(const DH *dh);

// dh_compute_key_padded_no_self_test does the same as `DH_compute_key_padded`,
// but doesn't try to run the self-test first. This is for use in the self tests
// themselves, to prevent an infinite loop.
int dh_compute_key_padded_no_self_test(unsigned char *out,
                                       const BIGNUM *peers_key, DH *dh);

BSSL_NAMESPACE_END

#endif  // OPENSSL_HEADER_CRYPTO_FIPSMODULE_DH_INTERNAL_H

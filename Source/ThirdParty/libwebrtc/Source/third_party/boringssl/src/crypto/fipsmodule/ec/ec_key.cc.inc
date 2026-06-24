// Copyright 2002-2016 The OpenSSL Project Authors. All Rights Reserved.
// Copyright (c) 2002, Oracle and/or its affiliates. All rights reserved.
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

#include <openssl/ec_key.h>

#include <string.h>

#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/ex_data.h>
#include <openssl/mem.h>

#include "../../internal.h"
#include "../../mem_internal.h"
#include "../bcm_interface.h"
#include "../delocate.h"
#include "../ecdsa/internal.h"
#include "../service_indicator/internal.h"
#include "internal.h"


using namespace bssl;

DEFINE_STATIC_EX_DATA_CLASS(g_ec_ex_data_class)

static bssl::EC_WRAPPED_SCALAR *ec_wrapped_scalar_new(const EC_GROUP *group) {
  EC_WRAPPED_SCALAR *wrapped = New<EC_WRAPPED_SCALAR>();
  if (wrapped == nullptr) {
    return nullptr;
  }

  wrapped->bignum.d = wrapped->scalar.words;
  wrapped->bignum.width = group->order.N.width;
  wrapped->bignum.dmax = group->order.N.width;
  wrapped->bignum.flags = BN_FLG_STATIC_DATA;
  return wrapped;
}

static void ec_wrapped_scalar_free(EC_WRAPPED_SCALAR *scalar) {
  Delete(scalar);
}

ECKey::ECKey(const ENGINE *engine)
    : RefCounted(CheckSubClass()),
      ecdsa_meth(engine ? ENGINE_get_ECDSA_method(engine) : nullptr) {
  if (ecdsa_meth) {
    METHOD_ref(ecdsa_meth);
  }
  CRYPTO_new_ex_data(&ex_data);
}

EC_KEY *EC_KEY_new() { return EC_KEY_new_method(nullptr); }

EC_KEY *EC_KEY_new_method(const ENGINE *engine) {
  UniquePtr<ECKey> ret(New<ECKey>(engine));
  if (ret == nullptr) {
    return nullptr;
  }
  if (ret->ecdsa_meth && ret->ecdsa_meth->init &&
      !ret->ecdsa_meth->init(ret.get())) {
    METHOD_unref(ret->ecdsa_meth);
    ret->ecdsa_meth = nullptr;
    return nullptr;
  }
  return ret.release();
}

EC_KEY *EC_KEY_new_by_curve_name(int nid) {
  ECKey *ret = FromOpaque(EC_KEY_new());
  if (ret == nullptr) {
    return nullptr;
  }
  ret->group = EC_GROUP_new_by_curve_name(nid);
  if (ret->group == nullptr) {
    EC_KEY_free(ret);
    return nullptr;
  }
  return ret;
}

ECKey::~ECKey() {
  if (ecdsa_meth) {
    if (ecdsa_meth->finish) {
      ecdsa_meth->finish(this);
    }
    METHOD_unref(ecdsa_meth);
  }

  CRYPTO_free_ex_data(g_ec_ex_data_class_bss_get(), &ex_data);

  EC_GROUP_free(group);
  EC_POINT_free(pub_key);
  ec_wrapped_scalar_free(priv_key);
}

void EC_KEY_free(EC_KEY *r) {
  if (r == nullptr) {
    return;
  }

  auto *impl = FromOpaque(r);
  impl->DecRefInternal();
}

EC_KEY *EC_KEY_dup(const EC_KEY *src) {
  if (src == nullptr) {
    OPENSSL_PUT_ERROR(EC, ERR_R_PASSED_NULL_PARAMETER);
    return nullptr;
  }

  UniquePtr<ECKey> ret(FromOpaque(EC_KEY_new()));
  if (ret == nullptr) {
    return nullptr;
  }

  auto *impl = FromOpaque(src);
  if ((impl->group != nullptr && !EC_KEY_set_group(ret.get(), impl->group)) ||
      (impl->pub_key != nullptr &&
       !EC_KEY_set_public_key(ret.get(), impl->pub_key)) ||
      (impl->priv_key != nullptr &&
       !EC_KEY_set_private_key(ret.get(), EC_KEY_get0_private_key(impl)))) {
    return nullptr;
  }

  ret->enc_flag = impl->enc_flag;
  ret->conv_form = impl->conv_form;
  return ret.release();
}

int EC_KEY_up_ref(EC_KEY *r) {
  auto *impl = FromOpaque(r);
  impl->UpRefInternal();
  return 1;
}

int EC_KEY_is_opaque(const EC_KEY *key) {
  auto *impl = FromOpaque(key);
  return impl->ecdsa_meth && (impl->ecdsa_meth->flags & ECDSA_FLAG_OPAQUE);
}

const EC_GROUP *EC_KEY_get0_group(const EC_KEY *key) {
  auto *impl = FromOpaque(key);
  return impl->group;
}

int EC_KEY_set_group(EC_KEY *key, const EC_GROUP *group) {
  auto *impl = FromOpaque(key);

  // If |impl| already has a group, it is an error to switch to another one.
  if (impl->group != nullptr) {
    if (EC_GROUP_cmp(impl->group, group, nullptr) != 0) {
      OPENSSL_PUT_ERROR(EC, EC_R_GROUP_MISMATCH);
      return 0;
    }
    return 1;
  }

  assert(impl->priv_key == nullptr);
  assert(impl->pub_key == nullptr);

  EC_GROUP_free(impl->group);
  impl->group = EC_GROUP_dup(group);
  return impl->group != nullptr;
}

const BIGNUM *EC_KEY_get0_private_key(const EC_KEY *key) {
  auto *impl = FromOpaque(key);
  return impl->priv_key != nullptr ? &impl->priv_key->bignum : nullptr;
}

int EC_KEY_set_private_key(EC_KEY *key, const BIGNUM *priv_key) {
  auto *impl = FromOpaque(key);
  if (impl->group == nullptr) {
    OPENSSL_PUT_ERROR(EC, EC_R_MISSING_PARAMETERS);
    return 0;
  }

  EC_WRAPPED_SCALAR *scalar = ec_wrapped_scalar_new(impl->group);
  if (scalar == nullptr) {
    return 0;
  }
  if (!ec_bignum_to_scalar(impl->group, &scalar->scalar, priv_key) ||
      // Zero is not a valid private key, so it is safe to leak the result of
      // this comparison.
      constant_time_declassify_int(
          ec_scalar_is_zero(impl->group, &scalar->scalar))) {
    OPENSSL_PUT_ERROR(EC, EC_R_INVALID_PRIVATE_KEY);
    ec_wrapped_scalar_free(scalar);
    return 0;
  }
  ec_wrapped_scalar_free(impl->priv_key);
  impl->priv_key = scalar;
  return 1;
}

const EC_POINT *EC_KEY_get0_public_key(const EC_KEY *key) {
  auto *impl = FromOpaque(key);
  return impl->pub_key;
}

int EC_KEY_set_public_key(EC_KEY *key, const EC_POINT *pub_key) {
  auto *impl = FromOpaque(key);
  if (impl->group == nullptr) {
    OPENSSL_PUT_ERROR(EC, EC_R_MISSING_PARAMETERS);
    return 0;
  }

  if (pub_key != nullptr && EC_POINT_is_at_infinity(pub_key->group, pub_key)) {
    OPENSSL_PUT_ERROR(EC, EC_R_POINT_AT_INFINITY);
    return 0;
  }

  if (pub_key != nullptr &&
      EC_GROUP_cmp(impl->group, pub_key->group, nullptr) != 0) {
    OPENSSL_PUT_ERROR(EC, EC_R_GROUP_MISMATCH);
    return 0;
  }

  EC_POINT_free(impl->pub_key);
  impl->pub_key = EC_POINT_dup(pub_key, impl->group);
  return (impl->pub_key == nullptr) ? 0 : 1;
}

unsigned int EC_KEY_get_enc_flags(const EC_KEY *key) {
  auto *impl = FromOpaque(key);
  return impl->enc_flag;
}

void EC_KEY_set_enc_flags(EC_KEY *key, unsigned int flags) {
  auto *impl = FromOpaque(key);
  impl->enc_flag = flags;
}

point_conversion_form_t EC_KEY_get_conv_form(const EC_KEY *key) {
  auto *impl = FromOpaque(key);
  return impl->conv_form;
}

void EC_KEY_set_conv_form(EC_KEY *key, point_conversion_form_t cform) {
  auto *impl = FromOpaque(key);
  impl->conv_form = cform;
}

int EC_KEY_check_key(const EC_KEY *eckey) {
  auto *impl = FromOpaque(eckey);

  if (!eckey || !impl->group || !impl->pub_key) {
    OPENSSL_PUT_ERROR(EC, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
  }

  if (EC_POINT_is_at_infinity(impl->group, impl->pub_key)) {
    OPENSSL_PUT_ERROR(EC, EC_R_POINT_AT_INFINITY);
    return 0;
  }

  // Test whether the public key is on the elliptic curve.
  if (!EC_POINT_is_on_curve(impl->group, impl->pub_key, nullptr)) {
    OPENSSL_PUT_ERROR(EC, EC_R_POINT_IS_NOT_ON_CURVE);
    return 0;
  }

  // Check the public and private keys match.
  //
  // NOTE: this is a FIPS pair-wise consistency check for the ECDH case. See SP
  // 800-56Ar3, page 36.
  if (impl->priv_key != nullptr) {
    EC_JACOBIAN point;
    if (!ec_point_mul_scalar_base(impl->group, &point,
                                  &impl->priv_key->scalar)) {
      OPENSSL_PUT_ERROR(EC, ERR_R_EC_LIB);
      return 0;
    }
    // Leaking this comparison only leaks whether |eckey|'s public key was
    // correct.
    if (!constant_time_declassify_int(ec_GFp_simple_points_equal(
            impl->group, &point, &impl->pub_key->raw))) {
      OPENSSL_PUT_ERROR(EC, EC_R_INVALID_PRIVATE_KEY);
      return 0;
    }
  }

  return 1;
}

int EC_KEY_check_fips(const EC_KEY *key) {
  auto *impl = FromOpaque(key);

  int ret = 0;
  FIPS_service_indicator_lock_state();

  if (!EC_KEY_check_key(impl)) {
    goto end;
  }

  if (impl->priv_key) {
    uint8_t digest[SHA256_DIGEST_LENGTH] = {0};
    uint8_t sig[ECDSA_MAX_FIXED_LEN];
    size_t sig_len;
    if (!ecdsa_sign_fixed(digest, sizeof(digest), sig, &sig_len, sizeof(sig),
                          impl)) {
      goto end;
    }
    if (boringssl_fips_break_test("ECDSA_PWCT")) {
      digest[0] = ~digest[0];
    }
    if (!ecdsa_verify_fixed(digest, sizeof(digest), sig, sig_len, impl)) {
      OPENSSL_PUT_ERROR(EC, EC_R_PUBLIC_KEY_VALIDATION_FAILED);
      goto end;
    }
  }

  ret = 1;

end:
  FIPS_service_indicator_unlock_state();
  if (ret) {
    EC_KEY_keygen_verify_service_indicator(impl);
  }

  return ret;
}

int EC_KEY_set_public_key_affine_coordinates(EC_KEY *key, const BIGNUM *x,
                                             const BIGNUM *y) {
  auto *impl = FromOpaque(key);

  if (!key || !impl->group || !x || !y) {
    OPENSSL_PUT_ERROR(EC, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
  }

  UniquePtr<EC_POINT> point(EC_POINT_new(impl->group));
  if (point == nullptr ||
      !EC_POINT_set_affine_coordinates_GFp(impl->group, point.get(), x, y,
                                           nullptr) ||
      !EC_KEY_set_public_key(key, point.get()) ||  //
      !EC_KEY_check_key(key)) {
    return 0;
  }

  return 1;
}

int EC_KEY_oct2key(EC_KEY *key, const uint8_t *in, size_t len, BN_CTX *ctx) {
  auto *impl = FromOpaque(key);

  if (impl->group == nullptr) {
    OPENSSL_PUT_ERROR(EC, EC_R_MISSING_PARAMETERS);
    return 0;
  }

  UniquePtr<EC_POINT> point(EC_POINT_new(impl->group));
  return point != nullptr &&
         EC_POINT_oct2point(impl->group, point.get(), in, len, ctx) &&
         EC_KEY_set_public_key(key, point.get());
}

size_t EC_KEY_key2buf(const EC_KEY *key, point_conversion_form_t form,
                      uint8_t **out_buf, BN_CTX *ctx) {
  auto *impl = FromOpaque(key);

  if (impl == nullptr || impl->pub_key == nullptr || impl->group == nullptr) {
    OPENSSL_PUT_ERROR(EC, EC_R_MISSING_PARAMETERS);
    return 0;
  }

  return EC_POINT_point2buf(impl->group, impl->pub_key, form, out_buf, ctx);
}

int EC_KEY_oct2priv(EC_KEY *key, const uint8_t *in, size_t len) {
  auto *impl = FromOpaque(key);

  if (impl->group == nullptr) {
    OPENSSL_PUT_ERROR(EC, EC_R_MISSING_PARAMETERS);
    return 0;
  }

  if (len != BN_num_bytes(EC_GROUP_get0_order(impl->group))) {
    OPENSSL_PUT_ERROR(EC, EC_R_DECODE_ERROR);
    return 0;
  }

  BIGNUM *priv_key = BN_bin2bn(in, len, nullptr);
  int ok = priv_key != nullptr &&  //
           EC_KEY_set_private_key(key, priv_key);
  BN_free(priv_key);
  return ok;
}

size_t EC_KEY_priv2oct(const EC_KEY *key, uint8_t *out, size_t max_out) {
  auto *impl = FromOpaque(key);

  if (impl->group == nullptr || impl->priv_key == nullptr) {
    OPENSSL_PUT_ERROR(EC, EC_R_MISSING_PARAMETERS);
    return 0;
  }

  size_t len = BN_num_bytes(EC_GROUP_get0_order(impl->group));
  if (out == nullptr) {
    return len;
  }

  if (max_out < len) {
    OPENSSL_PUT_ERROR(EC, EC_R_BUFFER_TOO_SMALL);
    return 0;
  }

  size_t bytes_written;
  ec_scalar_to_bytes(impl->group, out, &bytes_written, &impl->priv_key->scalar);
  assert(bytes_written == len);
  return len;
}

size_t EC_KEY_priv2buf(const EC_KEY *key, uint8_t **out_buf) {
  *out_buf = nullptr;
  size_t len = EC_KEY_priv2oct(key, nullptr, 0);
  if (len == 0) {
    return 0;
  }

  uint8_t *buf = reinterpret_cast<uint8_t *>(OPENSSL_malloc(len));
  if (buf == nullptr) {
    return 0;
  }

  len = EC_KEY_priv2oct(key, buf, len);
  if (len == 0) {
    OPENSSL_free(buf);
    return 0;
  }

  *out_buf = buf;
  return len;
}

int EC_KEY_generate_key(EC_KEY *key) {
  auto *impl = FromOpaque(key);

  if (impl == nullptr || impl->group == nullptr) {
    OPENSSL_PUT_ERROR(EC, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
  }

  // Generate an ECDSA key pair via rejection sampling. This function implements
  // FIPS 186-5, A.2.2, repeating the process on failure.

  // Check the group order is large enough. See step 1 of FIPS 186-5, A.2.2.
  if (EC_GROUP_order_bits(impl->group) < 224) {
    OPENSSL_PUT_ERROR(EC, EC_R_INVALID_GROUP_ORDER);
    return 0;
  }

  static const uint8_t kDefaultAdditionalData[32] = {0};
  EC_WRAPPED_SCALAR *priv_key = ec_wrapped_scalar_new(impl->group);
  EC_POINT *pub_key = EC_POINT_new(impl->group);
  if (priv_key == nullptr || pub_key == nullptr ||
      !ec_random_nonzero_scalar(impl->group, &priv_key->scalar,
                                kDefaultAdditionalData) ||
      !ec_point_mul_scalar_base(impl->group, &pub_key->raw,
                                &priv_key->scalar)) {
    EC_POINT_free(pub_key);
    ec_wrapped_scalar_free(priv_key);
    return 0;
  }

  // The public key is derived from the private key, but it is public.
  //
  // TODO(crbug.com/boringssl/677): This isn't quite right. While |pub_key|
  // represents a public point, it is still in Jacobian form and the exact
  // Jacobian representation is secret. We need to make it affine first. See
  // discussion in the bug.
  CONSTTIME_DECLASSIFY(&pub_key->raw, sizeof(pub_key->raw));

  ec_wrapped_scalar_free(impl->priv_key);
  impl->priv_key = priv_key;
  EC_POINT_free(impl->pub_key);
  impl->pub_key = pub_key;
  return 1;
}

int EC_KEY_generate_key_fips(EC_KEY *eckey) {
  auto *impl = FromOpaque(eckey);

  if (impl == nullptr || impl->group == nullptr) {
    OPENSSL_PUT_ERROR(EC, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
  }

  boringssl_ensure_ecc_self_test();

  if (EC_KEY_generate_key(impl) && EC_KEY_check_fips(impl)) {
    return 1;
  }

  EC_POINT_free(impl->pub_key);
  ec_wrapped_scalar_free(impl->priv_key);
  impl->pub_key = nullptr;
  impl->priv_key = nullptr;
  return 0;
}

int EC_KEY_get_ex_new_index(long argl, void *argp, CRYPTO_EX_unused *unused,
                            CRYPTO_EX_dup *dup_unused,
                            CRYPTO_EX_free *free_func) {
  return CRYPTO_get_ex_new_index_ex(g_ec_ex_data_class_bss_get(), argl, argp,
                                    free_func);
}

int EC_KEY_set_ex_data(EC_KEY *d, int idx, void *arg) {
  auto *impl = FromOpaque(d);

  return CRYPTO_set_ex_data(&impl->ex_data, idx, arg);
}

void *EC_KEY_get_ex_data(const EC_KEY *d, int idx) {
  auto *impl = FromOpaque(d);

  return CRYPTO_get_ex_data(&impl->ex_data, idx);
}

void EC_KEY_set_asn1_flag(EC_KEY *key, int flag) {}

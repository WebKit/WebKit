// Copyright 2011-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#include <openssl/ec.h>

#include <openssl/bn.h>
#include <openssl/err.h>

#include "internal.h"


using namespace bssl;

size_t bssl::ec_point_byte_len(const EC_GROUP *group,
                               point_conversion_form_t form) {
  if (form != POINT_CONVERSION_COMPRESSED &&
      form != POINT_CONVERSION_UNCOMPRESSED) {
    OPENSSL_PUT_ERROR(EC, EC_R_INVALID_FORM);
    return 0;
  }

  const size_t field_len = BN_num_bytes(&group->field.N);
  size_t output_len = 1 /* type byte */ + field_len;
  if (form == POINT_CONVERSION_UNCOMPRESSED) {
    // Uncompressed points have a second coordinate.
    output_len += field_len;
  }
  return output_len;
}

size_t bssl::ec_point_to_bytes(const EC_GROUP *group, const EC_AFFINE *point,
                               point_conversion_form_t form, uint8_t *buf,
                               size_t max_out) {
  size_t output_len = ec_point_byte_len(group, form);
  if (max_out < output_len) {
    OPENSSL_PUT_ERROR(EC, EC_R_BUFFER_TOO_SMALL);
    return 0;
  }

  size_t field_len;
  ec_felem_to_bytes(group, buf + 1, &field_len, &point->X);
  assert(field_len == BN_num_bytes(&group->field.N));

  if (form == POINT_CONVERSION_UNCOMPRESSED) {
    ec_felem_to_bytes(group, buf + 1 + field_len, &field_len, &point->Y);
    assert(field_len == BN_num_bytes(&group->field.N));
    buf[0] = form;
  } else {
    uint8_t y_buf[EC_MAX_BYTES];
    ec_felem_to_bytes(group, y_buf, &field_len, &point->Y);
    buf[0] = form + (y_buf[field_len - 1] & 1);
  }

  return output_len;
}

int bssl::ec_point_from_uncompressed(const EC_GROUP *group, EC_AFFINE *out,
                                     const uint8_t *in, size_t len) {
  const size_t field_len = BN_num_bytes(&group->field.N);
  if (len != 1 /* form */ + 2 * field_len ||
      in[0] != POINT_CONVERSION_UNCOMPRESSED) {
    OPENSSL_PUT_ERROR(EC, EC_R_INVALID_ENCODING);
    return 0;
  }

  EC_FELEM x, y;
  return ec_felem_from_bytes(group, &x, in + 1, field_len) &&
         ec_felem_from_bytes(group, &y, in + 1 + field_len, field_len) &&
         ec_point_set_affine_coordinates(group, out, &x, &y);
}

int bssl::ec_point_from_compressed(const EC_GROUP *group, EC_AFFINE *out,
                                   const uint8_t *in, size_t len) {
  const size_t field_len = BN_num_bytes(&group->field.N);
  if (len != 1 /* form */ + field_len ||
      (in[0] & ~1u) != POINT_CONVERSION_COMPRESSED) {
    OPENSSL_PUT_ERROR(EC, EC_R_INVALID_ENCODING);
    return 0;
  }

  crypto_word_t y_bit = in[0] & 1;
  EC_FELEM x;
  return ec_felem_from_bytes(group, &x, in + 1, field_len) &&
         ec_point_set_compressed_coordinates(group, out, &x, y_bit);
}

static int ec_GFp_simple_oct2point(const EC_GROUP *group, EC_POINT *point,
                                   const uint8_t *buf, size_t len,
                                   BN_CTX *ctx) {
  if (len == 0) {
    OPENSSL_PUT_ERROR(EC, EC_R_BUFFER_TOO_SMALL);
    return 0;
  }

  const uint8_t form = buf[0];
  EC_AFFINE affine;
  bool ok = form == static_cast<uint8_t>(POINT_CONVERSION_UNCOMPRESSED)
                ? ec_point_from_uncompressed(group, &affine, buf, len)
                : ec_point_from_compressed(group, &affine, buf, len);
  if (!ok) {
    // In the event of an error, defend against the caller not checking the
    // return value by setting a known safe value.
    ec_set_to_safe_point(group, &point->raw);
    return 0;
  }
  ec_affine_to_jacobian(group, &point->raw, &affine);
  return 1;
}

int EC_POINT_oct2point(const EC_GROUP *group, EC_POINT *point,
                       const uint8_t *buf, size_t len, BN_CTX *ctx) {
  if (EC_GROUP_cmp(group, point->group, nullptr) != 0) {
    OPENSSL_PUT_ERROR(EC, EC_R_INCOMPATIBLE_OBJECTS);
    return 0;
  }
  return ec_GFp_simple_oct2point(group, point, buf, len, ctx);
}

size_t EC_POINT_point2oct(const EC_GROUP *group, const EC_POINT *point,
                          point_conversion_form_t form, uint8_t *buf,
                          size_t max_out, BN_CTX *ctx) {
  if (EC_GROUP_cmp(group, point->group, nullptr) != 0) {
    OPENSSL_PUT_ERROR(EC, EC_R_INCOMPATIBLE_OBJECTS);
    return 0;
  }
  if (buf == nullptr) {
    // When `buf` is NULL, just return the number of bytes that would be
    // written, without doing an expensive Jacobian-to-affine conversion.
    if (constant_time_declassify_int(
            ec_GFp_simple_is_at_infinity(group, &point->raw))) {
      OPENSSL_PUT_ERROR(EC, EC_R_POINT_AT_INFINITY);
      return 0;
    }
    return ec_point_byte_len(group, form);
  }
  EC_AFFINE affine;
  if (!ec_jacobian_to_affine(group, &affine, &point->raw)) {
    return 0;
  }
  return ec_point_to_bytes(group, &affine, form, buf, max_out);
}

size_t EC_POINT_point2buf(const EC_GROUP *group, const EC_POINT *point,
                          point_conversion_form_t form, uint8_t **out_buf,
                          BN_CTX *ctx) {
  *out_buf = nullptr;
  size_t len = EC_POINT_point2oct(group, point, form, nullptr, 0, ctx);
  if (len == 0) {
    return 0;
  }
  uint8_t *buf = reinterpret_cast<uint8_t *>(OPENSSL_malloc(len));
  if (buf == nullptr) {
    return 0;
  }
  len = EC_POINT_point2oct(group, point, form, buf, len, ctx);
  if (len == 0) {
    OPENSSL_free(buf);
    return 0;
  }
  *out_buf = buf;
  return len;
}

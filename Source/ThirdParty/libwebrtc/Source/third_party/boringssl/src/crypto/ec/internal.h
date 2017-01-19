/* Originally written by Bodo Moeller for the OpenSSL project.
 * ====================================================================
 * Copyright (c) 1998-2005 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
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
 * Hudson (tjh@cryptsoft.com).
 *
 */
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * Portions of the attached software ("Contribution") are developed by
 * SUN MICROSYSTEMS, INC., and are contributed to the OpenSSL project.
 *
 * The Contribution is licensed pursuant to the OpenSSL open source
 * license provided above.
 *
 * The elliptic curve binary polynomial software is originally written by
 * Sheueling Chang Shantz and Douglas Stebila of Sun Microsystems
 * Laboratories. */

#ifndef OPENSSL_HEADER_EC_INTERNAL_H
#define OPENSSL_HEADER_EC_INTERNAL_H

#include <openssl/base.h>

#include <openssl/bn.h>
#include <openssl/ex_data.h>
#include <openssl/thread.h>

#if defined(__cplusplus)
extern "C" {
#endif


struct ec_method_st {
  int (*group_init)(EC_GROUP *);
  void (*group_finish)(EC_GROUP *);
  int (*group_copy)(EC_GROUP *, const EC_GROUP *);
  int (*group_set_curve)(EC_GROUP *, const BIGNUM *p, const BIGNUM *a,
                         const BIGNUM *b, BN_CTX *);
  int (*point_get_affine_coordinates)(const EC_GROUP *, const EC_POINT *,
                                      BIGNUM *x, BIGNUM *y, BN_CTX *);

  /* Computes |r = g_scalar*generator + p_scalar*p| if |g_scalar| and |p_scalar|
   * are both non-null. Computes |r = g_scalar*generator| if |p_scalar| is null.
   * Computes |r = p_scalar*p| if g_scalar is null. At least one of |g_scalar|
   * and |p_scalar| must be non-null, and |p| must be non-null if |p_scalar| is
   * non-null. */
  int (*mul)(const EC_GROUP *group, EC_POINT *r, const BIGNUM *g_scalar,
             const EC_POINT *p, const BIGNUM *p_scalar, BN_CTX *ctx);

  /* 'field_mul' and 'field_sqr' can be used by 'add' and 'dbl' so that the
   * same implementations of point operations can be used with different
   * optimized implementations of expensive field operations: */
  int (*field_mul)(const EC_GROUP *, BIGNUM *r, const BIGNUM *a,
                   const BIGNUM *b, BN_CTX *);
  int (*field_sqr)(const EC_GROUP *, BIGNUM *r, const BIGNUM *a, BN_CTX *);

  int (*field_encode)(const EC_GROUP *, BIGNUM *r, const BIGNUM *a,
                      BN_CTX *); /* e.g. to Montgomery */
  int (*field_decode)(const EC_GROUP *, BIGNUM *r, const BIGNUM *a,
                      BN_CTX *); /* e.g. from Montgomery */
} /* EC_METHOD */;

extern const EC_METHOD EC_GFp_mont_method;

struct ec_group_st {
  const EC_METHOD *meth;

  EC_POINT *generator;
  BIGNUM order;

  int curve_name; /* optional NID for named curve */

  const BN_MONT_CTX *mont_data; /* data for ECDSA inverse */

  /* The following members are handled by the method functions,
   * even if they appear generic */

  BIGNUM field; /* For curves over GF(p), this is the modulus. */

  BIGNUM a, b; /* Curve coefficients. */

  int a_is_minus3; /* enable optimized point arithmetics for special case */

  BN_MONT_CTX *mont; /* Montgomery structure. */

  BIGNUM one; /* The value one. */
} /* EC_GROUP */;

struct ec_point_st {
  const EC_METHOD *meth;

  BIGNUM X;
  BIGNUM Y;
  BIGNUM Z; /* Jacobian projective coordinates:
             * (X, Y, Z)  represents  (X/Z^2, Y/Z^3)  if  Z != 0 */
} /* EC_POINT */;

EC_GROUP *ec_group_new(const EC_METHOD *meth);
int ec_group_copy(EC_GROUP *dest, const EC_GROUP *src);

/* ec_group_get_mont_data returns a Montgomery context for operations in the
 * scalar field of |group|. It may return NULL in the case that |group| is not
 * a built-in group. */
const BN_MONT_CTX *ec_group_get_mont_data(const EC_GROUP *group);

int ec_wNAF_mul(const EC_GROUP *group, EC_POINT *r, const BIGNUM *g_scalar,
                const EC_POINT *p, const BIGNUM *p_scalar, BN_CTX *ctx);

/* method functions in simple.c */
int ec_GFp_simple_group_init(EC_GROUP *);
void ec_GFp_simple_group_finish(EC_GROUP *);
int ec_GFp_simple_group_copy(EC_GROUP *, const EC_GROUP *);
int ec_GFp_simple_group_set_curve(EC_GROUP *, const BIGNUM *p, const BIGNUM *a,
                                  const BIGNUM *b, BN_CTX *);
int ec_GFp_simple_group_get_curve(const EC_GROUP *, BIGNUM *p, BIGNUM *a,
                                  BIGNUM *b, BN_CTX *);
unsigned ec_GFp_simple_group_get_degree(const EC_GROUP *);
int ec_GFp_simple_point_init(EC_POINT *);
void ec_GFp_simple_point_finish(EC_POINT *);
void ec_GFp_simple_point_clear_finish(EC_POINT *);
int ec_GFp_simple_point_copy(EC_POINT *, const EC_POINT *);
int ec_GFp_simple_point_set_to_infinity(const EC_GROUP *, EC_POINT *);
int ec_GFp_simple_set_Jprojective_coordinates_GFp(const EC_GROUP *, EC_POINT *,
                                                  const BIGNUM *x,
                                                  const BIGNUM *y,
                                                  const BIGNUM *z, BN_CTX *);
int ec_GFp_simple_get_Jprojective_coordinates_GFp(const EC_GROUP *,
                                                  const EC_POINT *, BIGNUM *x,
                                                  BIGNUM *y, BIGNUM *z,
                                                  BN_CTX *);
int ec_GFp_simple_point_set_affine_coordinates(const EC_GROUP *, EC_POINT *,
                                               const BIGNUM *x, const BIGNUM *y,
                                               BN_CTX *);
int ec_GFp_simple_set_compressed_coordinates(const EC_GROUP *, EC_POINT *,
                                             const BIGNUM *x, int y_bit,
                                             BN_CTX *);
int ec_GFp_simple_add(const EC_GROUP *, EC_POINT *r, const EC_POINT *a,
                      const EC_POINT *b, BN_CTX *);
int ec_GFp_simple_dbl(const EC_GROUP *, EC_POINT *r, const EC_POINT *a,
                      BN_CTX *);
int ec_GFp_simple_invert(const EC_GROUP *, EC_POINT *, BN_CTX *);
int ec_GFp_simple_is_at_infinity(const EC_GROUP *, const EC_POINT *);
int ec_GFp_simple_is_on_curve(const EC_GROUP *, const EC_POINT *, BN_CTX *);
int ec_GFp_simple_cmp(const EC_GROUP *, const EC_POINT *a, const EC_POINT *b,
                      BN_CTX *);
int ec_GFp_simple_make_affine(const EC_GROUP *, EC_POINT *, BN_CTX *);
int ec_GFp_simple_points_make_affine(const EC_GROUP *, size_t num,
                                     EC_POINT * [], BN_CTX *);
int ec_GFp_simple_field_mul(const EC_GROUP *, BIGNUM *r, const BIGNUM *a,
                            const BIGNUM *b, BN_CTX *);
int ec_GFp_simple_field_sqr(const EC_GROUP *, BIGNUM *r, const BIGNUM *a,
                            BN_CTX *);

/* method functions in montgomery.c */
int ec_GFp_mont_group_init(EC_GROUP *);
int ec_GFp_mont_group_set_curve(EC_GROUP *, const BIGNUM *p, const BIGNUM *a,
                                const BIGNUM *b, BN_CTX *);
void ec_GFp_mont_group_finish(EC_GROUP *);
int ec_GFp_mont_group_copy(EC_GROUP *, const EC_GROUP *);
int ec_GFp_mont_field_mul(const EC_GROUP *, BIGNUM *r, const BIGNUM *a,
                          const BIGNUM *b, BN_CTX *);
int ec_GFp_mont_field_sqr(const EC_GROUP *, BIGNUM *r, const BIGNUM *a,
                          BN_CTX *);
int ec_GFp_mont_field_encode(const EC_GROUP *, BIGNUM *r, const BIGNUM *a,
                             BN_CTX *);
int ec_GFp_mont_field_decode(const EC_GROUP *, BIGNUM *r, const BIGNUM *a,
                             BN_CTX *);

int ec_point_set_Jprojective_coordinates_GFp(const EC_GROUP *group,
                                             EC_POINT *point, const BIGNUM *x,
                                             const BIGNUM *y, const BIGNUM *z,
                                             BN_CTX *ctx);

void ec_GFp_nistp_recode_scalar_bits(uint8_t *sign, uint8_t *digit, uint8_t in);

extern const EC_METHOD EC_GFp_nistp224_method;
extern const EC_METHOD EC_GFp_nistp256_method;

/* EC_GFp_nistz256_method is a GFp method using montgomery multiplication, with
 * x86-64 optimized P256. See http://eprint.iacr.org/2013/816. */
extern const EC_METHOD EC_GFp_nistz256_method;

struct ec_key_st {
  EC_GROUP *group;

  EC_POINT *pub_key;
  BIGNUM *priv_key;

  unsigned int enc_flag;
  point_conversion_form_t conv_form;

  CRYPTO_refcount_t references;

  ECDSA_METHOD *ecdsa_meth;

  CRYPTO_EX_DATA ex_data;
} /* EC_KEY */;

/* curve_data contains data about a built-in elliptic curve. */
struct curve_data {
  /* comment is a human-readable string describing the curve. */
  const char *comment;
  /* param_len is the number of bytes needed to store a field element. */
  uint8_t param_len;
  /* data points to an array of 6*|param_len| bytes which hold the field
   * elements of the following (in big-endian order): prime, a, b, generator x,
   * generator y, order. */
  const uint8_t data[];
};

struct built_in_curve {
  int nid;
  uint8_t oid[8];
  uint8_t oid_len;
  const struct curve_data *data;
  const EC_METHOD *method;
};

/* OPENSSL_built_in_curves is terminated with an entry where |nid| is
 * |NID_undef|. */
extern const struct built_in_curve OPENSSL_built_in_curves[];

#if defined(__cplusplus)
}  /* extern C */
#endif

#endif  /* OPENSSL_HEADER_EC_INTERNAL_H */

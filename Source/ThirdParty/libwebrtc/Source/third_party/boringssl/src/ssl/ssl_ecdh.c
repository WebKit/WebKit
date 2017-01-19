/* Copyright (c) 2015, Google Inc.
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

#include <openssl/ssl.h>

#include <assert.h>
#include <string.h>

#include <openssl/bn.h>
#include <openssl/bytestring.h>
#include <openssl/curve25519.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/newhope.h>
#include <openssl/nid.h>

#include "internal.h"
#include "../crypto/internal.h"


/* |EC_POINT| implementation. */

static void ssl_ec_point_cleanup(SSL_ECDH_CTX *ctx) {
  BIGNUM *private_key = (BIGNUM *)ctx->data;
  BN_clear_free(private_key);
}

static int ssl_ec_point_offer(SSL_ECDH_CTX *ctx, CBB *out) {
  assert(ctx->data == NULL);
  BIGNUM *private_key = BN_new();
  if (private_key == NULL) {
    return 0;
  }
  ctx->data = private_key;

  /* Set up a shared |BN_CTX| for all operations. */
  BN_CTX *bn_ctx = BN_CTX_new();
  if (bn_ctx == NULL) {
    return 0;
  }
  BN_CTX_start(bn_ctx);

  int ret = 0;
  EC_POINT *public_key = NULL;
  EC_GROUP *group = EC_GROUP_new_by_curve_name(ctx->method->nid);
  if (group == NULL) {
    goto err;
  }

  /* Generate a private key. */
  if (!BN_rand_range_ex(private_key, 1, EC_GROUP_get0_order(group))) {
    goto err;
  }

  /* Compute the corresponding public key and serialize it. */
  public_key = EC_POINT_new(group);
  if (public_key == NULL ||
      !EC_POINT_mul(group, public_key, private_key, NULL, NULL, bn_ctx) ||
      !EC_POINT_point2cbb(out, group, public_key, POINT_CONVERSION_UNCOMPRESSED,
                          bn_ctx)) {
    goto err;
  }

  ret = 1;

err:
  EC_GROUP_free(group);
  EC_POINT_free(public_key);
  BN_CTX_end(bn_ctx);
  BN_CTX_free(bn_ctx);
  return ret;
}

static int ssl_ec_point_finish(SSL_ECDH_CTX *ctx, uint8_t **out_secret,
                               size_t *out_secret_len, uint8_t *out_alert,
                               const uint8_t *peer_key, size_t peer_key_len) {
  BIGNUM *private_key = (BIGNUM *)ctx->data;
  assert(private_key != NULL);
  *out_alert = SSL_AD_INTERNAL_ERROR;

  /* Set up a shared |BN_CTX| for all operations. */
  BN_CTX *bn_ctx = BN_CTX_new();
  if (bn_ctx == NULL) {
    return 0;
  }
  BN_CTX_start(bn_ctx);

  int ret = 0;
  EC_GROUP *group = EC_GROUP_new_by_curve_name(ctx->method->nid);
  EC_POINT *peer_point = NULL, *result = NULL;
  uint8_t *secret = NULL;
  if (group == NULL) {
    goto err;
  }

  /* Compute the x-coordinate of |peer_key| * |private_key|. */
  peer_point = EC_POINT_new(group);
  result = EC_POINT_new(group);
  if (peer_point == NULL || result == NULL) {
    goto err;
  }
  BIGNUM *x = BN_CTX_get(bn_ctx);
  if (x == NULL) {
    goto err;
  }
  if (!EC_POINT_oct2point(group, peer_point, peer_key, peer_key_len, bn_ctx)) {
    *out_alert = SSL_AD_DECODE_ERROR;
    goto err;
  }
  if (!EC_POINT_mul(group, result, NULL, peer_point, private_key, bn_ctx) ||
      !EC_POINT_get_affine_coordinates_GFp(group, result, x, NULL, bn_ctx)) {
    goto err;
  }

  /* Encode the x-coordinate left-padded with zeros. */
  size_t secret_len = (EC_GROUP_get_degree(group) + 7) / 8;
  secret = OPENSSL_malloc(secret_len);
  if (secret == NULL || !BN_bn2bin_padded(secret, secret_len, x)) {
    goto err;
  }

  *out_secret = secret;
  *out_secret_len = secret_len;
  secret = NULL;
  ret = 1;

err:
  EC_GROUP_free(group);
  EC_POINT_free(peer_point);
  EC_POINT_free(result);
  BN_CTX_end(bn_ctx);
  BN_CTX_free(bn_ctx);
  OPENSSL_free(secret);
  return ret;
}

static int ssl_ec_point_accept(SSL_ECDH_CTX *ctx, CBB *out_public_key,
                               uint8_t **out_secret, size_t *out_secret_len,
                               uint8_t *out_alert, const uint8_t *peer_key,
                               size_t peer_key_len) {
  *out_alert = SSL_AD_INTERNAL_ERROR;
  if (!ssl_ec_point_offer(ctx, out_public_key) ||
      !ssl_ec_point_finish(ctx, out_secret, out_secret_len, out_alert, peer_key,
                           peer_key_len)) {
    return 0;
  }
  return 1;
}

/* X25119 implementation. */

static void ssl_x25519_cleanup(SSL_ECDH_CTX *ctx) {
  if (ctx->data == NULL) {
    return;
  }
  OPENSSL_cleanse(ctx->data, 32);
  OPENSSL_free(ctx->data);
}

static int ssl_x25519_offer(SSL_ECDH_CTX *ctx, CBB *out) {
  assert(ctx->data == NULL);

  ctx->data = OPENSSL_malloc(32);
  if (ctx->data == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return 0;
  }
  uint8_t public_key[32];
  X25519_keypair(public_key, (uint8_t *)ctx->data);
  return CBB_add_bytes(out, public_key, sizeof(public_key));
}

static int ssl_x25519_finish(SSL_ECDH_CTX *ctx, uint8_t **out_secret,
                             size_t *out_secret_len, uint8_t *out_alert,
                             const uint8_t *peer_key, size_t peer_key_len) {
  assert(ctx->data != NULL);
  *out_alert = SSL_AD_INTERNAL_ERROR;

  uint8_t *secret = OPENSSL_malloc(32);
  if (secret == NULL) {
    return 0;
  }

  if (peer_key_len != 32 ||
      !X25519(secret, (uint8_t *)ctx->data, peer_key)) {
    OPENSSL_free(secret);
    *out_alert = SSL_AD_DECODE_ERROR;
    OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_ECPOINT);
    return 0;
  }

  *out_secret = secret;
  *out_secret_len = 32;
  return 1;
}

static int ssl_x25519_accept(SSL_ECDH_CTX *ctx, CBB *out_public_key,
                             uint8_t **out_secret, size_t *out_secret_len,
                             uint8_t *out_alert, const uint8_t *peer_key,
                             size_t peer_key_len) {
  *out_alert = SSL_AD_INTERNAL_ERROR;
  if (!ssl_x25519_offer(ctx, out_public_key) ||
      !ssl_x25519_finish(ctx, out_secret, out_secret_len, out_alert, peer_key,
                         peer_key_len)) {
    return 0;
  }
  return 1;
}


/* Combined X25119 + New Hope (post-quantum) implementation. */

typedef struct {
  uint8_t x25519_key[32];
  NEWHOPE_POLY *newhope_sk;
} cecpq1_data;

#define CECPQ1_OFFERMSG_LENGTH (32 + NEWHOPE_OFFERMSG_LENGTH)
#define CECPQ1_ACCEPTMSG_LENGTH (32 + NEWHOPE_ACCEPTMSG_LENGTH)
#define CECPQ1_SECRET_LENGTH (32 + SHA256_DIGEST_LENGTH)

static void ssl_cecpq1_cleanup(SSL_ECDH_CTX *ctx) {
  if (ctx->data == NULL) {
    return;
  }
  cecpq1_data *data = ctx->data;
  NEWHOPE_POLY_free(data->newhope_sk);
  OPENSSL_cleanse(data, sizeof(cecpq1_data));
  OPENSSL_free(data);
}

static int ssl_cecpq1_offer(SSL_ECDH_CTX *ctx, CBB *out) {
  assert(ctx->data == NULL);
  cecpq1_data *data = OPENSSL_malloc(sizeof(cecpq1_data));
  if (data == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return 0;
  }
  ctx->data = data;
  data->newhope_sk = NEWHOPE_POLY_new();
  if (data->newhope_sk == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return 0;
  }

  uint8_t x25519_public_key[32];
  X25519_keypair(x25519_public_key, data->x25519_key);

  uint8_t newhope_offermsg[NEWHOPE_OFFERMSG_LENGTH];
  NEWHOPE_offer(newhope_offermsg, data->newhope_sk);

  if (!CBB_add_bytes(out, x25519_public_key, sizeof(x25519_public_key)) ||
      !CBB_add_bytes(out, newhope_offermsg, sizeof(newhope_offermsg))) {
    return 0;
  }
  return 1;
}

static int ssl_cecpq1_accept(SSL_ECDH_CTX *ctx, CBB *cbb, uint8_t **out_secret,
                             size_t *out_secret_len, uint8_t *out_alert,
                             const uint8_t *peer_key, size_t peer_key_len) {
  if (peer_key_len != CECPQ1_OFFERMSG_LENGTH) {
    *out_alert = SSL_AD_DECODE_ERROR;
    return 0;
  }

  *out_alert = SSL_AD_INTERNAL_ERROR;

  assert(ctx->data == NULL);
  cecpq1_data *data = OPENSSL_malloc(sizeof(cecpq1_data));
  if (data == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return 0;
  }
  data->newhope_sk = NULL;
  ctx->data = data;

  uint8_t *secret = OPENSSL_malloc(CECPQ1_SECRET_LENGTH);
  if (secret == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return 0;
  }

  /* Generate message to server, and secret key, at once. */

  uint8_t x25519_public_key[32];
  X25519_keypair(x25519_public_key, data->x25519_key);
  if (!X25519(secret, data->x25519_key, peer_key)) {
    *out_alert = SSL_AD_DECODE_ERROR;
    OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_ECPOINT);
    goto err;
  }

  uint8_t newhope_acceptmsg[NEWHOPE_ACCEPTMSG_LENGTH];
  if (!NEWHOPE_accept(secret + 32, newhope_acceptmsg, peer_key + 32,
                      NEWHOPE_OFFERMSG_LENGTH)) {
    *out_alert = SSL_AD_DECODE_ERROR;
    goto err;
  }

  if (!CBB_add_bytes(cbb, x25519_public_key, sizeof(x25519_public_key)) ||
      !CBB_add_bytes(cbb, newhope_acceptmsg, sizeof(newhope_acceptmsg))) {
    goto err;
  }

  *out_secret = secret;
  *out_secret_len = CECPQ1_SECRET_LENGTH;
  return 1;

 err:
  OPENSSL_cleanse(secret, CECPQ1_SECRET_LENGTH);
  OPENSSL_free(secret);
  return 0;
}

static int ssl_cecpq1_finish(SSL_ECDH_CTX *ctx, uint8_t **out_secret,
                             size_t *out_secret_len, uint8_t *out_alert,
                             const uint8_t *peer_key, size_t peer_key_len) {
  if (peer_key_len != CECPQ1_ACCEPTMSG_LENGTH) {
    *out_alert = SSL_AD_DECODE_ERROR;
    return 0;
  }

  *out_alert = SSL_AD_INTERNAL_ERROR;

  assert(ctx->data != NULL);
  cecpq1_data *data = ctx->data;

  uint8_t *secret = OPENSSL_malloc(CECPQ1_SECRET_LENGTH);
  if (secret == NULL) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return 0;
  }

  if (!X25519(secret, data->x25519_key, peer_key)) {
    *out_alert = SSL_AD_DECODE_ERROR;
    OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_ECPOINT);
    goto err;
  }

  if (!NEWHOPE_finish(secret + 32, data->newhope_sk, peer_key + 32,
                      NEWHOPE_ACCEPTMSG_LENGTH)) {
    *out_alert = SSL_AD_DECODE_ERROR;
    goto err;
  }

  *out_secret = secret;
  *out_secret_len = CECPQ1_SECRET_LENGTH;
  return 1;

 err:
  OPENSSL_cleanse(secret, CECPQ1_SECRET_LENGTH);
  OPENSSL_free(secret);
  return 0;
}


/* Legacy DHE-based implementation. */

static void ssl_dhe_cleanup(SSL_ECDH_CTX *ctx) {
  DH_free((DH *)ctx->data);
}

static int ssl_dhe_offer(SSL_ECDH_CTX *ctx, CBB *out) {
  DH *dh = (DH *)ctx->data;
  /* The group must have been initialized already, but not the key. */
  assert(dh != NULL);
  assert(dh->priv_key == NULL);

  /* Due to a bug in yaSSL, the public key must be zero padded to the size of
   * the prime. */
  return DH_generate_key(dh) &&
         BN_bn2cbb_padded(out, BN_num_bytes(dh->p), dh->pub_key);
}

static int ssl_dhe_finish(SSL_ECDH_CTX *ctx, uint8_t **out_secret,
                          size_t *out_secret_len, uint8_t *out_alert,
                          const uint8_t *peer_key, size_t peer_key_len) {
  DH *dh = (DH *)ctx->data;
  assert(dh != NULL);
  assert(dh->priv_key != NULL);
  *out_alert = SSL_AD_INTERNAL_ERROR;

  int secret_len = 0;
  uint8_t *secret = NULL;
  BIGNUM *peer_point = BN_bin2bn(peer_key, peer_key_len, NULL);
  if (peer_point == NULL) {
    goto err;
  }

  secret = OPENSSL_malloc(DH_size(dh));
  if (secret == NULL) {
    goto err;
  }
  secret_len = DH_compute_key(secret, peer_point, dh);
  if (secret_len <= 0) {
    goto err;
  }

  *out_secret = secret;
  *out_secret_len = (size_t)secret_len;
  BN_free(peer_point);
  return 1;

err:
  if (secret_len > 0) {
    OPENSSL_cleanse(secret, (size_t)secret_len);
  }
  OPENSSL_free(secret);
  BN_free(peer_point);
  return 0;
}

static int ssl_dhe_accept(SSL_ECDH_CTX *ctx, CBB *out_public_key,
                          uint8_t **out_secret, size_t *out_secret_len,
                          uint8_t *out_alert, const uint8_t *peer_key,
                          size_t peer_key_len) {
  *out_alert = SSL_AD_INTERNAL_ERROR;
  if (!ssl_dhe_offer(ctx, out_public_key) ||
      !ssl_dhe_finish(ctx, out_secret, out_secret_len, out_alert, peer_key,
                      peer_key_len)) {
    return 0;
  }
  return 1;
}

static const SSL_ECDH_METHOD kDHEMethod = {
    NID_undef, 0, "",
    ssl_dhe_cleanup,
    ssl_dhe_offer,
    ssl_dhe_accept,
    ssl_dhe_finish,
    CBS_get_u16_length_prefixed,
    CBB_add_u16_length_prefixed,
};

static const SSL_ECDH_METHOD kCECPQ1Method = {
    NID_undef, 0, "",
    ssl_cecpq1_cleanup,
    ssl_cecpq1_offer,
    ssl_cecpq1_accept,
    ssl_cecpq1_finish,
    CBS_get_u16_length_prefixed,
    CBB_add_u16_length_prefixed,
};

static const SSL_ECDH_METHOD kMethods[] = {
    {
        NID_X9_62_prime256v1,
        SSL_CURVE_SECP256R1,
        "P-256",
        ssl_ec_point_cleanup,
        ssl_ec_point_offer,
        ssl_ec_point_accept,
        ssl_ec_point_finish,
        CBS_get_u8_length_prefixed,
        CBB_add_u8_length_prefixed,
    },
    {
        NID_secp384r1,
        SSL_CURVE_SECP384R1,
        "P-384",
        ssl_ec_point_cleanup,
        ssl_ec_point_offer,
        ssl_ec_point_accept,
        ssl_ec_point_finish,
        CBS_get_u8_length_prefixed,
        CBB_add_u8_length_prefixed,
    },
    {
        NID_secp521r1,
        SSL_CURVE_SECP521R1,
        "P-521",
        ssl_ec_point_cleanup,
        ssl_ec_point_offer,
        ssl_ec_point_accept,
        ssl_ec_point_finish,
        CBS_get_u8_length_prefixed,
        CBB_add_u8_length_prefixed,
    },
    {
        NID_X25519,
        SSL_CURVE_X25519,
        "X25519",
        ssl_x25519_cleanup,
        ssl_x25519_offer,
        ssl_x25519_accept,
        ssl_x25519_finish,
        CBS_get_u8_length_prefixed,
        CBB_add_u8_length_prefixed,
    },
};

static const SSL_ECDH_METHOD *method_from_group_id(uint16_t group_id) {
  for (size_t i = 0; i < OPENSSL_ARRAY_SIZE(kMethods); i++) {
    if (kMethods[i].group_id == group_id) {
      return &kMethods[i];
    }
  }
  return NULL;
}

static const SSL_ECDH_METHOD *method_from_nid(int nid) {
  for (size_t i = 0; i < OPENSSL_ARRAY_SIZE(kMethods); i++) {
    if (kMethods[i].nid == nid) {
      return &kMethods[i];
    }
  }
  return NULL;
}

static const SSL_ECDH_METHOD *method_from_name(const char *name, size_t len) {
  for (size_t i = 0; i < OPENSSL_ARRAY_SIZE(kMethods); i++) {
    if (len == strlen(kMethods[i].name) &&
        !strncmp(kMethods[i].name, name, len)) {
      return &kMethods[i];
    }
  }
  return NULL;
}

const char* SSL_get_curve_name(uint16_t group_id) {
  const SSL_ECDH_METHOD *method = method_from_group_id(group_id);
  if (method == NULL) {
    return NULL;
  }
  return method->name;
}

int ssl_nid_to_group_id(uint16_t *out_group_id, int nid) {
  const SSL_ECDH_METHOD *method = method_from_nid(nid);
  if (method == NULL) {
    return 0;
  }
  *out_group_id = method->group_id;
  return 1;
}

int ssl_name_to_group_id(uint16_t *out_group_id, const char *name, size_t len) {
  const SSL_ECDH_METHOD *method = method_from_name(name, len);
  if (method == NULL) {
    return 0;
  }
  *out_group_id = method->group_id;
  return 1;
}

int SSL_ECDH_CTX_init(SSL_ECDH_CTX *ctx, uint16_t group_id) {
  SSL_ECDH_CTX_cleanup(ctx);

  const SSL_ECDH_METHOD *method = method_from_group_id(group_id);
  if (method == NULL) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNSUPPORTED_ELLIPTIC_CURVE);
    return 0;
  }
  ctx->method = method;
  return 1;
}

void SSL_ECDH_CTX_init_for_dhe(SSL_ECDH_CTX *ctx, DH *params) {
  SSL_ECDH_CTX_cleanup(ctx);

  ctx->method = &kDHEMethod;
  ctx->data = params;
}

void SSL_ECDH_CTX_init_for_cecpq1(SSL_ECDH_CTX *ctx) {
  SSL_ECDH_CTX_cleanup(ctx);

  ctx->method = &kCECPQ1Method;
}

void SSL_ECDH_CTX_cleanup(SSL_ECDH_CTX *ctx) {
  if (ctx->method == NULL) {
    return;
  }
  ctx->method->cleanup(ctx);
  ctx->method = NULL;
  ctx->data = NULL;
}

uint16_t SSL_ECDH_CTX_get_id(const SSL_ECDH_CTX *ctx) {
  return ctx->method->group_id;
}

int SSL_ECDH_CTX_get_key(SSL_ECDH_CTX *ctx, CBS *cbs, CBS *out) {
  if (ctx->method == NULL) {
    return 0;
  }
  return ctx->method->get_key(cbs, out);
}

int SSL_ECDH_CTX_add_key(SSL_ECDH_CTX *ctx, CBB *cbb, CBB *out_contents) {
  if (ctx->method == NULL) {
    return 0;
  }
  return ctx->method->add_key(cbb, out_contents);
}

int SSL_ECDH_CTX_offer(SSL_ECDH_CTX *ctx, CBB *out_public_key) {
  return ctx->method->offer(ctx, out_public_key);
}

int SSL_ECDH_CTX_accept(SSL_ECDH_CTX *ctx, CBB *out_public_key,
                        uint8_t **out_secret, size_t *out_secret_len,
                        uint8_t *out_alert, const uint8_t *peer_key,
                        size_t peer_key_len) {
  return ctx->method->accept(ctx, out_public_key, out_secret, out_secret_len,
                             out_alert, peer_key, peer_key_len);
}

int SSL_ECDH_CTX_finish(SSL_ECDH_CTX *ctx, uint8_t **out_secret,
                        size_t *out_secret_len, uint8_t *out_alert,
                        const uint8_t *peer_key, size_t peer_key_len) {
  return ctx->method->finish(ctx, out_secret, out_secret_len, out_alert,
                             peer_key, peer_key_len);
}

/* Copyright (c) 2019, Google Inc.
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

#include <openssl/bytestring.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/sha.h>
#include <openssl/trust_token.h>

#include "internal.h"


// The Trust Token API is described in
// https://github.com/WICG/trust-token-api/blob/master/README.md and provides a
// protocol for issuing and redeeming tokens built on top of the PMBTokens
// construction.

const TRUST_TOKEN_METHOD *TRUST_TOKEN_experiment_v1(void) {
  static const TRUST_TOKEN_METHOD kMethod = {
      pmbtoken_exp1_generate_key,
      pmbtoken_exp1_client_key_from_bytes,
      pmbtoken_exp1_issuer_key_from_bytes,
      pmbtoken_exp1_blind,
      pmbtoken_exp1_sign,
      pmbtoken_exp1_unblind,
      pmbtoken_exp1_read,
      1, /* has_private_metadata */
      3, /* max_keys */
      1, /* has_srr */
  };
  return &kMethod;
}

const TRUST_TOKEN_METHOD *TRUST_TOKEN_experiment_v2_voprf(void) {
  static const TRUST_TOKEN_METHOD kMethod = {
      voprf_exp2_generate_key,
      voprf_exp2_client_key_from_bytes,
      voprf_exp2_issuer_key_from_bytes,
      voprf_exp2_blind,
      voprf_exp2_sign,
      voprf_exp2_unblind,
      voprf_exp2_read,
      0, /* has_private_metadata */
      6, /* max_keys */
      0, /* has_srr */
  };
  return &kMethod;
}

const TRUST_TOKEN_METHOD *TRUST_TOKEN_experiment_v2_pmb(void) {
  static const TRUST_TOKEN_METHOD kMethod = {
      pmbtoken_exp2_generate_key,
      pmbtoken_exp2_client_key_from_bytes,
      pmbtoken_exp2_issuer_key_from_bytes,
      pmbtoken_exp2_blind,
      pmbtoken_exp2_sign,
      pmbtoken_exp2_unblind,
      pmbtoken_exp2_read,
      1, /* has_private_metadata */
      3, /* max_keys */
      0, /* has_srr */
  };
  return &kMethod;
}

void TRUST_TOKEN_PRETOKEN_free(TRUST_TOKEN_PRETOKEN *pretoken) {
  OPENSSL_free(pretoken);
}

TRUST_TOKEN *TRUST_TOKEN_new(const uint8_t *data, size_t len) {
  TRUST_TOKEN *ret = OPENSSL_malloc(sizeof(TRUST_TOKEN));
  if (ret == NULL) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, ERR_R_MALLOC_FAILURE);
    return NULL;
  }
  OPENSSL_memset(ret, 0, sizeof(TRUST_TOKEN));
  ret->data = OPENSSL_memdup(data, len);
  if (len != 0 && ret->data == NULL) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, ERR_R_MALLOC_FAILURE);
    OPENSSL_free(ret);
    return NULL;
  }
  ret->len = len;
  return ret;
}

void TRUST_TOKEN_free(TRUST_TOKEN *token) {
  if (token == NULL) {
    return;
  }
  OPENSSL_free(token->data);
  OPENSSL_free(token);
}

int TRUST_TOKEN_generate_key(const TRUST_TOKEN_METHOD *method,
                             uint8_t *out_priv_key, size_t *out_priv_key_len,
                             size_t max_priv_key_len, uint8_t *out_pub_key,
                             size_t *out_pub_key_len, size_t max_pub_key_len,
                             uint32_t id) {
  // Prepend the key ID in front of the PMBTokens format.
  int ret = 0;
  CBB priv_cbb, pub_cbb;
  CBB_zero(&priv_cbb);
  CBB_zero(&pub_cbb);
  if (!CBB_init_fixed(&priv_cbb, out_priv_key, max_priv_key_len) ||
      !CBB_init_fixed(&pub_cbb, out_pub_key, max_pub_key_len) ||
      !CBB_add_u32(&priv_cbb, id) ||
      !CBB_add_u32(&pub_cbb, id)) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_BUFFER_TOO_SMALL);
    goto err;
  }

  if (!method->generate_key(&priv_cbb, &pub_cbb)) {
    goto err;
  }

  if (!CBB_finish(&priv_cbb, NULL, out_priv_key_len) ||
      !CBB_finish(&pub_cbb, NULL, out_pub_key_len)) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_BUFFER_TOO_SMALL);
    goto err;
  }

  ret = 1;

err:
  CBB_cleanup(&priv_cbb);
  CBB_cleanup(&pub_cbb);
  return ret;
}

TRUST_TOKEN_CLIENT *TRUST_TOKEN_CLIENT_new(const TRUST_TOKEN_METHOD *method,
                                           size_t max_batchsize) {
  if (max_batchsize > 0xffff) {
    // The protocol supports only two-byte token counts.
    OPENSSL_PUT_ERROR(TRUST_TOKEN, ERR_R_OVERFLOW);
    return NULL;
  }

  TRUST_TOKEN_CLIENT *ret = OPENSSL_malloc(sizeof(TRUST_TOKEN_CLIENT));
  if (ret == NULL) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, ERR_R_MALLOC_FAILURE);
    return NULL;
  }
  OPENSSL_memset(ret, 0, sizeof(TRUST_TOKEN_CLIENT));
  ret->method = method;
  ret->max_batchsize = (uint16_t)max_batchsize;
  return ret;
}

void TRUST_TOKEN_CLIENT_free(TRUST_TOKEN_CLIENT *ctx) {
  if (ctx == NULL) {
    return;
  }
  EVP_PKEY_free(ctx->srr_key);
  sk_TRUST_TOKEN_PRETOKEN_pop_free(ctx->pretokens, TRUST_TOKEN_PRETOKEN_free);
  OPENSSL_free(ctx);
}

int TRUST_TOKEN_CLIENT_add_key(TRUST_TOKEN_CLIENT *ctx, size_t *out_key_index,
                               const uint8_t *key, size_t key_len) {
  if (ctx->num_keys == OPENSSL_ARRAY_SIZE(ctx->keys) ||
      ctx->num_keys >= ctx->method->max_keys) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_TOO_MANY_KEYS);
    return 0;
  }

  struct trust_token_client_key_st *key_s = &ctx->keys[ctx->num_keys];
  CBS cbs;
  CBS_init(&cbs, key, key_len);
  uint32_t key_id;
  if (!CBS_get_u32(&cbs, &key_id) ||
      !ctx->method->client_key_from_bytes(&key_s->key, CBS_data(&cbs),
                                          CBS_len(&cbs))) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_DECODE_FAILURE);
    return 0;
  }
  key_s->id = key_id;
  *out_key_index = ctx->num_keys;
  ctx->num_keys += 1;
  return 1;
}

int TRUST_TOKEN_CLIENT_set_srr_key(TRUST_TOKEN_CLIENT *ctx, EVP_PKEY *key) {
  if (!ctx->method->has_srr) {
    return 1;
  }
  EVP_PKEY_free(ctx->srr_key);
  EVP_PKEY_up_ref(key);
  ctx->srr_key = key;
  return 1;
}

int TRUST_TOKEN_CLIENT_begin_issuance(TRUST_TOKEN_CLIENT *ctx, uint8_t **out,
                                      size_t *out_len, size_t count) {
  if (count > ctx->max_batchsize) {
    count = ctx->max_batchsize;
  }

  int ret = 0;
  CBB request;
  STACK_OF(TRUST_TOKEN_PRETOKEN) *pretokens = NULL;
  if (!CBB_init(&request, 0) ||
      !CBB_add_u16(&request, count)) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, ERR_R_MALLOC_FAILURE);
    goto err;
  }

  pretokens = ctx->method->blind(&request, count);
  if (pretokens == NULL) {
    goto err;
  }

  if (!CBB_finish(&request, out, out_len)) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, ERR_R_MALLOC_FAILURE);
    goto err;
  }

  sk_TRUST_TOKEN_PRETOKEN_pop_free(ctx->pretokens, TRUST_TOKEN_PRETOKEN_free);
  ctx->pretokens = pretokens;
  pretokens = NULL;
  ret = 1;

err:
  CBB_cleanup(&request);
  sk_TRUST_TOKEN_PRETOKEN_pop_free(pretokens, TRUST_TOKEN_PRETOKEN_free);
  return ret;
}

STACK_OF(TRUST_TOKEN) *
    TRUST_TOKEN_CLIENT_finish_issuance(TRUST_TOKEN_CLIENT *ctx,
                                       size_t *out_key_index,
                                       const uint8_t *response,
                                       size_t response_len) {
  CBS in;
  CBS_init(&in, response, response_len);
  uint16_t count;
  uint32_t key_id;
  if (!CBS_get_u16(&in, &count) ||
      !CBS_get_u32(&in, &key_id)) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_DECODE_FAILURE);
    return NULL;
  }

  size_t key_index = 0;
  const struct trust_token_client_key_st *key = NULL;
  for (size_t i = 0; i < ctx->num_keys; i++) {
    if (ctx->keys[i].id == key_id) {
      key_index = i;
      key = &ctx->keys[i];
      break;
    }
  }

  if (key == NULL) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_INVALID_KEY_ID);
    return NULL;
  }

  if (count > sk_TRUST_TOKEN_PRETOKEN_num(ctx->pretokens)) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_DECODE_FAILURE);
    return NULL;
  }

  STACK_OF(TRUST_TOKEN) *tokens =
      ctx->method->unblind(&key->key, ctx->pretokens, &in, count, key_id);
  if (tokens == NULL) {
    return NULL;
  }

  if (CBS_len(&in) != 0) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_DECODE_FAILURE);
    sk_TRUST_TOKEN_pop_free(tokens, TRUST_TOKEN_free);
    return NULL;
  }

  sk_TRUST_TOKEN_PRETOKEN_pop_free(ctx->pretokens, TRUST_TOKEN_PRETOKEN_free);
  ctx->pretokens = NULL;

  *out_key_index = key_index;
  return tokens;
}

int TRUST_TOKEN_CLIENT_begin_redemption(TRUST_TOKEN_CLIENT *ctx, uint8_t **out,
                                        size_t *out_len,
                                        const TRUST_TOKEN *token,
                                        const uint8_t *data, size_t data_len,
                                        uint64_t time) {
  CBB request, token_inner, inner;
  if (!CBB_init(&request, 0) ||
      !CBB_add_u16_length_prefixed(&request, &token_inner) ||
      !CBB_add_bytes(&token_inner, token->data, token->len) ||
      !CBB_add_u16_length_prefixed(&request, &inner) ||
      !CBB_add_bytes(&inner, data, data_len) ||
      (ctx->method->has_srr && !CBB_add_u64(&request, time)) ||
      !CBB_finish(&request, out, out_len)) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, ERR_R_MALLOC_FAILURE);
    CBB_cleanup(&request);
    return 0;
  }
  return 1;
}

int TRUST_TOKEN_CLIENT_finish_redemption(TRUST_TOKEN_CLIENT *ctx,
                                         uint8_t **out_rr, size_t *out_rr_len,
                                         uint8_t **out_sig, size_t *out_sig_len,
                                         const uint8_t *response,
                                         size_t response_len) {
  CBS in, srr, sig;
  CBS_init(&in, response, response_len);
  if (!ctx->method->has_srr) {
    if (!CBS_stow(&in, out_rr, out_rr_len)) {
      OPENSSL_PUT_ERROR(TRUST_TOKEN, ERR_R_MALLOC_FAILURE);
      return 0;
    }

    *out_sig = NULL;
    *out_sig_len = 0;
    return 1;
  }

  if (!CBS_get_u16_length_prefixed(&in, &srr) ||
      !CBS_get_u16_length_prefixed(&in, &sig) ||
      CBS_len(&in) != 0) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_DECODE_ERROR);
    return 0;
  }

  if (ctx->srr_key == NULL) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_NO_SRR_KEY_CONFIGURED);
    return 0;
  }

  EVP_MD_CTX md_ctx;
  EVP_MD_CTX_init(&md_ctx);
  int sig_ok = EVP_DigestVerifyInit(&md_ctx, NULL, NULL, NULL, ctx->srr_key) &&
               EVP_DigestVerify(&md_ctx, CBS_data(&sig), CBS_len(&sig),
                                CBS_data(&srr), CBS_len(&srr));
  EVP_MD_CTX_cleanup(&md_ctx);

  if (!sig_ok) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_SRR_SIGNATURE_ERROR);
    return 0;
  }

  uint8_t *srr_buf = NULL, *sig_buf = NULL;
  size_t srr_len, sig_len;
  if (!CBS_stow(&srr, &srr_buf, &srr_len) ||
      !CBS_stow(&sig, &sig_buf, &sig_len)) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, ERR_R_MALLOC_FAILURE);
    OPENSSL_free(srr_buf);
    OPENSSL_free(sig_buf);
    return 0;
  }

  *out_rr = srr_buf;
  *out_rr_len = srr_len;
  *out_sig = sig_buf;
  *out_sig_len = sig_len;
  return 1;
}

TRUST_TOKEN_ISSUER *TRUST_TOKEN_ISSUER_new(const TRUST_TOKEN_METHOD *method,
                                           size_t max_batchsize) {
  if (max_batchsize > 0xffff) {
    // The protocol supports only two-byte token counts.
    OPENSSL_PUT_ERROR(TRUST_TOKEN, ERR_R_OVERFLOW);
    return NULL;
  }

  TRUST_TOKEN_ISSUER *ret = OPENSSL_malloc(sizeof(TRUST_TOKEN_ISSUER));
  if (ret == NULL) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, ERR_R_MALLOC_FAILURE);
    return NULL;
  }
  OPENSSL_memset(ret, 0, sizeof(TRUST_TOKEN_ISSUER));
  ret->method = method;
  ret->max_batchsize = (uint16_t)max_batchsize;
  return ret;
}

void TRUST_TOKEN_ISSUER_free(TRUST_TOKEN_ISSUER *ctx) {
  if (ctx == NULL) {
    return;
  }
  EVP_PKEY_free(ctx->srr_key);
  OPENSSL_free(ctx->metadata_key);
  OPENSSL_free(ctx);
}

int TRUST_TOKEN_ISSUER_add_key(TRUST_TOKEN_ISSUER *ctx, const uint8_t *key,
                               size_t key_len) {
  if (ctx->num_keys == OPENSSL_ARRAY_SIZE(ctx->keys) ||
      ctx->num_keys >= ctx->method->max_keys) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_TOO_MANY_KEYS);
    return 0;
  }

  struct trust_token_issuer_key_st *key_s = &ctx->keys[ctx->num_keys];
  CBS cbs;
  CBS_init(&cbs, key, key_len);
  uint32_t key_id;
  if (!CBS_get_u32(&cbs, &key_id) ||
      !ctx->method->issuer_key_from_bytes(&key_s->key, CBS_data(&cbs),
                                          CBS_len(&cbs))) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_DECODE_FAILURE);
    return 0;
  }

  key_s->id = key_id;
  ctx->num_keys += 1;
  return 1;
}

int TRUST_TOKEN_ISSUER_set_srr_key(TRUST_TOKEN_ISSUER *ctx, EVP_PKEY *key) {
  EVP_PKEY_free(ctx->srr_key);
  EVP_PKEY_up_ref(key);
  ctx->srr_key = key;
  return 1;
}

int TRUST_TOKEN_ISSUER_set_metadata_key(TRUST_TOKEN_ISSUER *ctx,
                                        const uint8_t *key, size_t len) {
  if (len < 32) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_INVALID_METADATA_KEY);
  }
  OPENSSL_free(ctx->metadata_key);
  ctx->metadata_key_len = 0;
  ctx->metadata_key = OPENSSL_memdup(key, len);
  if (ctx->metadata_key == NULL) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, ERR_R_MALLOC_FAILURE);
    return 0;
  }
  ctx->metadata_key_len = len;
  return 1;
}

static const struct trust_token_issuer_key_st *trust_token_issuer_get_key(
    const TRUST_TOKEN_ISSUER *ctx, uint32_t key_id) {
  for (size_t i = 0; i < ctx->num_keys; i++) {
    if (ctx->keys[i].id == key_id) {
      return &ctx->keys[i];
    }
  }
  return NULL;
}

int TRUST_TOKEN_ISSUER_issue(const TRUST_TOKEN_ISSUER *ctx, uint8_t **out,
                             size_t *out_len, size_t *out_tokens_issued,
                             const uint8_t *request, size_t request_len,
                             uint32_t public_metadata, uint8_t private_metadata,
                             size_t max_issuance) {
  if (max_issuance > ctx->max_batchsize) {
    max_issuance = ctx->max_batchsize;
  }

  const struct trust_token_issuer_key_st *key =
      trust_token_issuer_get_key(ctx, public_metadata);
  if (key == NULL || private_metadata > 1 ||
      (!ctx->method->has_private_metadata && private_metadata != 0)) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_INVALID_METADATA);
    return 0;
  }

  CBS in;
  uint16_t num_requested;
  CBS_init(&in, request, request_len);
  if (!CBS_get_u16(&in, &num_requested)) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_DECODE_FAILURE);
    return 0;
  }

  size_t num_to_issue = num_requested;
  if (num_to_issue > max_issuance) {
    num_to_issue = max_issuance;
  }

  int ret = 0;
  CBB response;
  if (!CBB_init(&response, 0) ||
      !CBB_add_u16(&response, num_to_issue) ||
      !CBB_add_u32(&response, public_metadata)) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, ERR_R_MALLOC_FAILURE);
    goto err;
  }

  if (!ctx->method->sign(&key->key, &response, &in, num_requested, num_to_issue,
                         private_metadata)) {
    goto err;
  }

  if (CBS_len(&in) != 0) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_DECODE_FAILURE);
    goto err;
  }

  if (!CBB_finish(&response, out, out_len)) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, ERR_R_MALLOC_FAILURE);
    goto err;
  }

  *out_tokens_issued = num_to_issue;
  ret = 1;

err:
  CBB_cleanup(&response);
  return ret;
}


int TRUST_TOKEN_ISSUER_redeem_raw(const TRUST_TOKEN_ISSUER *ctx,
                                  uint32_t *out_public, uint8_t *out_private,
                                  TRUST_TOKEN **out_token,
                                  uint8_t **out_client_data,
                                  size_t *out_client_data_len,
                                  const uint8_t *request, size_t request_len) {
  CBS request_cbs, token_cbs;
  CBS_init(&request_cbs, request, request_len);
  if (!CBS_get_u16_length_prefixed(&request_cbs, &token_cbs)) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_DECODE_ERROR);
    return 0;
  }

  uint32_t public_metadata = 0;
  uint8_t private_metadata = 0;

  // Parse the token. If there is an error, treat it as an invalid token.
  if (!CBS_get_u32(&token_cbs, &public_metadata)) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_INVALID_TOKEN);
    return 0;
  }

  const struct trust_token_issuer_key_st *key =
      trust_token_issuer_get_key(ctx, public_metadata);
  uint8_t nonce[TRUST_TOKEN_NONCE_SIZE];
  if (key == NULL ||
      !ctx->method->read(&key->key, nonce, &private_metadata,
                         CBS_data(&token_cbs), CBS_len(&token_cbs))) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_INVALID_TOKEN);
    return 0;
  }

  CBS client_data;
  if (!CBS_get_u16_length_prefixed(&request_cbs, &client_data) ||
      (ctx->method->has_srr && !CBS_skip(&request_cbs, 8)) ||
      CBS_len(&request_cbs) != 0) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_DECODE_ERROR);
    return 0;
  }

  uint8_t *client_data_buf = NULL;
  size_t client_data_len = 0;
  if (!CBS_stow(&client_data, &client_data_buf, &client_data_len)) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, ERR_R_MALLOC_FAILURE);
    goto err;
  }

  TRUST_TOKEN *token = TRUST_TOKEN_new(nonce, TRUST_TOKEN_NONCE_SIZE);
  if (token == NULL) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, ERR_R_MALLOC_FAILURE);
    goto err;
  }
  *out_public = public_metadata;
  *out_private = private_metadata;
  *out_token = token;
  *out_client_data = client_data_buf;
  *out_client_data_len = client_data_len;

  return 1;

err:
  OPENSSL_free(client_data_buf);
  return 0;
}

// https://tools.ietf.org/html/rfc7049#section-2.1
static int add_cbor_int_with_type(CBB *cbb, uint8_t major_type,
                                  uint64_t value) {
  if (value <= 23) {
    return CBB_add_u8(cbb, value | major_type);
  }
  if (value <= 0xff) {
    return CBB_add_u8(cbb, 0x18 | major_type) && CBB_add_u8(cbb, value);
  }
  if (value <= 0xffff) {
    return CBB_add_u8(cbb, 0x19 | major_type) && CBB_add_u16(cbb, value);
  }
  if (value <= 0xffffffff) {
    return CBB_add_u8(cbb, 0x1a | major_type) && CBB_add_u32(cbb, value);
  }
  if (value <= 0xffffffffffffffff) {
    return CBB_add_u8(cbb, 0x1b | major_type) && CBB_add_u64(cbb, value);
  }

  return 0;
}

// https://tools.ietf.org/html/rfc7049#section-2.1
static int add_cbor_int(CBB *cbb, uint64_t value) {
  return add_cbor_int_with_type(cbb, 0, value);
}

// https://tools.ietf.org/html/rfc7049#section-2.1
static int add_cbor_bytes(CBB *cbb, const uint8_t *data, size_t len) {
  return add_cbor_int_with_type(cbb, 0x40, len) &&
         CBB_add_bytes(cbb, data, len);
}

// https://tools.ietf.org/html/rfc7049#section-2.1
static int add_cbor_text(CBB *cbb, const char *data, size_t len) {
  return add_cbor_int_with_type(cbb, 0x60, len) &&
         CBB_add_bytes(cbb, (const uint8_t *)data, len);
}

// https://tools.ietf.org/html/rfc7049#section-2.1
static int add_cbor_map(CBB *cbb, uint8_t size) {
  return add_cbor_int_with_type(cbb, 0xa0, size);
}

static uint8_t get_metadata_obfuscator(const uint8_t *key, size_t key_len,
                                       const uint8_t *client_data,
                                       size_t client_data_len) {
  uint8_t metadata_obfuscator[SHA256_DIGEST_LENGTH];
  SHA256_CTX sha_ctx;
  SHA256_Init(&sha_ctx);
  SHA256_Update(&sha_ctx, key, key_len);
  SHA256_Update(&sha_ctx, client_data, client_data_len);
  SHA256_Final(metadata_obfuscator, &sha_ctx);
  return metadata_obfuscator[0] >> 7;
}

int TRUST_TOKEN_ISSUER_redeem(const TRUST_TOKEN_ISSUER *ctx, uint8_t **out,
                              size_t *out_len, TRUST_TOKEN **out_token,
                              uint8_t **out_client_data,
                              size_t *out_client_data_len,
                              uint64_t *out_redemption_time,
                              const uint8_t *request, size_t request_len,
                              uint64_t lifetime) {
  CBS request_cbs, token_cbs;
  CBS_init(&request_cbs, request, request_len);
  if (!CBS_get_u16_length_prefixed(&request_cbs, &token_cbs)) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_DECODE_ERROR);
    return 0;
  }

  uint32_t public_metadata = 0;
  uint8_t private_metadata = 0;

  CBS token_copy = token_cbs;

  // Parse the token. If there is an error, treat it as an invalid token.
  if (!CBS_get_u32(&token_cbs, &public_metadata)) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_INVALID_TOKEN);
    return 0;
  }

  const struct trust_token_issuer_key_st *key =
      trust_token_issuer_get_key(ctx, public_metadata);
  uint8_t nonce[TRUST_TOKEN_NONCE_SIZE];
  if (key == NULL ||
      !ctx->method->read(&key->key, nonce, &private_metadata,
                         CBS_data(&token_cbs), CBS_len(&token_cbs))) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_INVALID_TOKEN);
    return 0;
  }

  int ok = 0;
  CBB response, srr;
  uint8_t *srr_buf = NULL, *sig_buf = NULL, *client_data_buf = NULL;
  size_t srr_len = 0, sig_len = 0, client_data_len = 0;
  EVP_MD_CTX md_ctx;
  EVP_MD_CTX_init(&md_ctx);
  CBB_zero(&srr);
  if (!CBB_init(&response, 0)) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, ERR_R_MALLOC_FAILURE);
    goto err;
  }

  CBS client_data;
  uint64_t redemption_time = 0;
  if (!CBS_get_u16_length_prefixed(&request_cbs, &client_data) ||
      (ctx->method->has_srr && !CBS_get_u64(&request_cbs, &redemption_time))) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_DECODE_ERROR);
    goto err;
  }

  const uint8_t kTokenHashDSTLabel[] = "TrustTokenV0 TokenHash";
  uint8_t token_hash[SHA256_DIGEST_LENGTH];
  SHA256_CTX sha_ctx;
  SHA256_Init(&sha_ctx);
  SHA256_Update(&sha_ctx, kTokenHashDSTLabel, sizeof(kTokenHashDSTLabel));
  SHA256_Update(&sha_ctx, CBS_data(&token_copy), CBS_len(&token_copy));
  SHA256_Final(token_hash, &sha_ctx);

  uint8_t metadata_obfuscator = get_metadata_obfuscator(
      ctx->metadata_key, ctx->metadata_key_len, token_hash, sizeof(token_hash));

  // The SRR is constructed as per the format described in
  // https://docs.google.com/document/d/1TNnya6B8pyomDK2F1R9CL3dY10OAmqWlnCxsWyOBDVQ/edit#heading=h.7mkzvhpqb8l5

  // The V2 protocol is intended to be used with
  // |TRUST_TOKEN_ISSUER_redeem_raw|. However, we temporarily support it with
  // |TRUST_TOKEN_ISSUER_redeem| to ease the transition for existing issuer
  // callers. Those callers' consumers currently expect an expiry-timestamp
  // field, so we fill in a placeholder value.
  //
  // TODO(svaldez): After the existing issues have migrated to
  // |TRUST_TOKEN_ISSUER_redeem_raw| remove this logic.
  uint64_t expiry_time = 0;
  if (ctx->method->has_srr) {
    expiry_time = redemption_time + lifetime;
  }

  static const char kClientDataLabel[] = "client-data";
  static const char kExpiryTimestampLabel[] = "expiry-timestamp";
  static const char kMetadataLabel[] = "metadata";
  static const char kPrivateLabel[] = "private";
  static const char kPublicLabel[] = "public";
  static const char kTokenHashLabel[] = "token-hash";

  // CBOR requires map keys to be sorted by length then sorted lexically.
  // https://tools.ietf.org/html/rfc7049#section-3.9
  assert(strlen(kMetadataLabel) < strlen(kTokenHashLabel));
  assert(strlen(kTokenHashLabel) < strlen(kClientDataLabel));
  assert(strlen(kClientDataLabel) < strlen(kExpiryTimestampLabel));
  assert(strlen(kPublicLabel) < strlen(kPrivateLabel));

  size_t map_entries = 4;

  if (!CBB_init(&srr, 0) ||
      !add_cbor_map(&srr, map_entries) ||  // SRR map
      !add_cbor_text(&srr, kMetadataLabel, strlen(kMetadataLabel)) ||
      !add_cbor_map(&srr, 2) ||  // Metadata map
      !add_cbor_text(&srr, kPublicLabel, strlen(kPublicLabel)) ||
      !add_cbor_int(&srr, public_metadata) ||
      !add_cbor_text(&srr, kPrivateLabel, strlen(kPrivateLabel)) ||
      !add_cbor_int(&srr, private_metadata ^ metadata_obfuscator) ||
      !add_cbor_text(&srr, kTokenHashLabel, strlen(kTokenHashLabel)) ||
      !add_cbor_bytes(&srr, token_hash, sizeof(token_hash)) ||
      !add_cbor_text(&srr, kClientDataLabel, strlen(kClientDataLabel)) ||
      !CBB_add_bytes(&srr, CBS_data(&client_data), CBS_len(&client_data)) ||
      !add_cbor_text(&srr, kExpiryTimestampLabel,
                     strlen(kExpiryTimestampLabel)) ||
      !add_cbor_int(&srr, expiry_time) ||
      !CBB_finish(&srr, &srr_buf, &srr_len)) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, ERR_R_MALLOC_FAILURE);
    goto err;
  }

  if (!EVP_DigestSignInit(&md_ctx, NULL, NULL, NULL, ctx->srr_key) ||
      !EVP_DigestSign(&md_ctx, NULL, &sig_len, srr_buf, srr_len)) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, TRUST_TOKEN_R_SRR_SIGNATURE_ERROR);
    goto err;
  }

  // Merge SRR and Signature into single string.
  // TODO(svaldez): Expose API to construct this from the caller.
  if (!ctx->method->has_srr) {
    static const char kSRRHeader[] = "body=:";
    static const char kSRRSplit[] = ":, signature=:";
    static const char kSRREnd[] = ":";

    size_t srr_b64_len, sig_b64_len;
    if (!EVP_EncodedLength(&srr_b64_len, srr_len) ||
        !EVP_EncodedLength(&sig_b64_len, sig_len)) {
      goto err;
    }

    sig_buf = OPENSSL_malloc(sig_len);
    uint8_t *srr_b64_buf = OPENSSL_malloc(srr_b64_len);
    uint8_t *sig_b64_buf = OPENSSL_malloc(sig_b64_len);
    if (!sig_buf ||
        !srr_b64_buf ||
        !sig_b64_buf ||
        !EVP_DigestSign(&md_ctx, sig_buf, &sig_len, srr_buf, srr_len) ||
        !CBB_add_bytes(&response, (const uint8_t *)kSRRHeader,
                       strlen(kSRRHeader)) ||
        !CBB_add_bytes(&response, srr_b64_buf,
                       EVP_EncodeBlock(srr_b64_buf, srr_buf, srr_len)) ||
        !CBB_add_bytes(&response, (const uint8_t *)kSRRSplit,
                       strlen(kSRRSplit)) ||
        !CBB_add_bytes(&response, sig_b64_buf,
                       EVP_EncodeBlock(sig_b64_buf, sig_buf, sig_len)) ||
        !CBB_add_bytes(&response, (const uint8_t *)kSRREnd, strlen(kSRREnd))) {
      OPENSSL_PUT_ERROR(TRUST_TOKEN, ERR_R_MALLOC_FAILURE);
      OPENSSL_free(srr_b64_buf);
      OPENSSL_free(sig_b64_buf);
      goto err;
    }

    OPENSSL_free(srr_b64_buf);
    OPENSSL_free(sig_b64_buf);
  } else {
    CBB child;
    uint8_t *ptr;
    if (!CBB_add_u16_length_prefixed(&response, &child) ||
        !CBB_add_bytes(&child, srr_buf, srr_len) ||
        !CBB_add_u16_length_prefixed(&response, &child) ||
        !CBB_reserve(&child, &ptr, sig_len) ||
        !EVP_DigestSign(&md_ctx, ptr, &sig_len, srr_buf, srr_len) ||
        !CBB_did_write(&child, sig_len) ||
        !CBB_flush(&response)) {
      OPENSSL_PUT_ERROR(TRUST_TOKEN, ERR_R_MALLOC_FAILURE);
      goto err;
    }
  }

  if (!CBS_stow(&client_data, &client_data_buf, &client_data_len) ||
      !CBB_finish(&response, out, out_len)) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, ERR_R_MALLOC_FAILURE);
    goto err;
  }

  TRUST_TOKEN *token = TRUST_TOKEN_new(nonce, TRUST_TOKEN_NONCE_SIZE);
  if (token == NULL) {
    OPENSSL_PUT_ERROR(TRUST_TOKEN, ERR_R_MALLOC_FAILURE);
    goto err;
  }
  *out_token = token;
  *out_client_data = client_data_buf;
  *out_client_data_len = client_data_len;
  *out_redemption_time = redemption_time;

  ok = 1;

err:
  CBB_cleanup(&response);
  CBB_cleanup(&srr);
  OPENSSL_free(srr_buf);
  OPENSSL_free(sig_buf);
  EVP_MD_CTX_cleanup(&md_ctx);
  if (!ok) {
    OPENSSL_free(client_data_buf);
  }
  return ok;
}

int TRUST_TOKEN_decode_private_metadata(const TRUST_TOKEN_METHOD *method,
                                        uint8_t *out_value, const uint8_t *key,
                                        size_t key_len, const uint8_t *nonce,
                                        size_t nonce_len,
                                        uint8_t encrypted_bit) {
  uint8_t metadata_obfuscator =
      get_metadata_obfuscator(key, key_len, nonce, nonce_len);
  *out_value = encrypted_bit ^ metadata_obfuscator;
  return 1;
}

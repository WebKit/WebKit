/* Copyright (c) 2016, Google Inc.
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

#include <openssl/bytestring.h>
#include <openssl/err.h>
#include <openssl/hkdf.h>
#include <openssl/mem.h>
#include <openssl/stack.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "internal.h"


/* kMaxKeyUpdates is the number of consecutive KeyUpdates that will be
 * processed. Without this limit an attacker could force unbounded processing
 * without being able to return application data. */
static const uint8_t kMaxKeyUpdates = 32;

int tls13_handshake(SSL *ssl) {
  SSL_HANDSHAKE *hs = ssl->s3->hs;

  for (;;) {
    /* Resolve the operation the handshake was waiting on. */
    switch (hs->wait) {
      case ssl_hs_error:
        OPENSSL_PUT_ERROR(SSL, SSL_R_SSL_HANDSHAKE_FAILURE);
        return -1;

      case ssl_hs_flush:
      case ssl_hs_flush_and_read_message: {
        int ret = BIO_flush(ssl->wbio);
        if (ret <= 0) {
          ssl->rwstate = SSL_WRITING;
          return ret;
        }
        if (hs->wait != ssl_hs_flush_and_read_message) {
          break;
        }
        ssl->method->expect_flight(ssl);
        hs->wait = ssl_hs_read_message;
        /* Fall-through. */
      }

      case ssl_hs_read_message: {
        int ret = ssl->method->ssl_get_message(ssl, -1, ssl_dont_hash_message);
        if (ret <= 0) {
          return ret;
        }
        break;
      }

      case ssl_hs_write_message: {
        int ret = ssl->method->write_message(ssl);
        if (ret <= 0) {
          return ret;
        }
        break;
      }

      case ssl_hs_x509_lookup:
        ssl->rwstate = SSL_X509_LOOKUP;
        hs->wait = ssl_hs_ok;
        return -1;

      case ssl_hs_channel_id_lookup:
        ssl->rwstate = SSL_CHANNEL_ID_LOOKUP;
        hs->wait = ssl_hs_ok;
        return -1;

      case ssl_hs_private_key_operation:
        ssl->rwstate = SSL_PRIVATE_KEY_OPERATION;
        hs->wait = ssl_hs_ok;
        return -1;

      case ssl_hs_ok:
        break;
    }

    /* Run the state machine again. */
    hs->wait = hs->do_handshake(ssl);
    if (hs->wait == ssl_hs_error) {
      /* Don't loop around to avoid a stray |SSL_R_SSL_HANDSHAKE_FAILURE| the
       * first time around. */
      return -1;
    }
    if (hs->wait == ssl_hs_ok) {
      /* The handshake has completed. */
      return 1;
    }

    /* Otherwise, loop to the beginning and resolve what was blocking the
     * handshake. */
  }
}

int tls13_get_cert_verify_signature_input(
    SSL *ssl, uint8_t **out, size_t *out_len,
    enum ssl_cert_verify_context_t cert_verify_context) {
  CBB cbb;
  if (!CBB_init(&cbb, 64 + 33 + 1 + 2 * EVP_MAX_MD_SIZE)) {
    goto err;
  }

  for (size_t i = 0; i < 64; i++) {
    if (!CBB_add_u8(&cbb, 0x20)) {
      goto err;
    }
  }

  const uint8_t *context;
  size_t context_len;
  if (cert_verify_context == ssl_cert_verify_server) {
    /* Include the NUL byte. */
    static const char kContext[] = "TLS 1.3, server CertificateVerify";
    context = (const uint8_t *)kContext;
    context_len = sizeof(kContext);
  } else if (cert_verify_context == ssl_cert_verify_client) {
    static const char kContext[] = "TLS 1.3, client CertificateVerify";
    context = (const uint8_t *)kContext;
    context_len = sizeof(kContext);
  } else if (cert_verify_context == ssl_cert_verify_channel_id) {
    static const char kContext[] = "TLS 1.3, Channel ID";
    context = (const uint8_t *)kContext;
    context_len = sizeof(kContext);
  } else {
    goto err;
  }

  if (!CBB_add_bytes(&cbb, context, context_len)) {
    goto err;
  }

  uint8_t context_hashes[2 * EVP_MAX_MD_SIZE];
  size_t context_hashes_len;
  if (!tls13_get_context_hashes(ssl, context_hashes, &context_hashes_len) ||
      !CBB_add_bytes(&cbb, context_hashes, context_hashes_len) ||
      !CBB_finish(&cbb, out, out_len)) {
    goto err;
  }

  return 1;

err:
  OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
  CBB_cleanup(&cbb);
  return 0;
}

int tls13_process_certificate(SSL *ssl, int allow_anonymous) {
  CBS cbs, context;
  CBS_init(&cbs, ssl->init_msg, ssl->init_num);
  if (!CBS_get_u8_length_prefixed(&cbs, &context) ||
      CBS_len(&context) != 0) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    return 0;
  }

  const int retain_sha256 =
      ssl->server && ssl->ctx->retain_only_sha256_of_client_certs;
  int ret = 0;
  uint8_t alert;
  STACK_OF(X509) *chain = ssl_parse_cert_chain(
      ssl, &alert, retain_sha256 ? ssl->s3->new_session->peer_sha256 : NULL,
      &cbs);
  if (chain == NULL) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, alert);
    goto err;
  }

  if (CBS_len(&cbs) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    goto err;
  }

  if (sk_X509_num(chain) == 0) {
    if (!allow_anonymous) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_PEER_DID_NOT_RETURN_A_CERTIFICATE);
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_CERTIFICATE_REQUIRED);
      goto err;
    }

    /* OpenSSL returns X509_V_OK when no certificates are requested. This is
     * classed by them as a bug, but it's assumed by at least NGINX. */
    ssl->s3->new_session->verify_result = X509_V_OK;

    /* No certificate, so nothing more to do. */
    ret = 1;
    goto err;
  }

  ssl->s3->new_session->peer_sha256_valid = retain_sha256;

  if (!ssl_verify_cert_chain(ssl, &ssl->s3->new_session->verify_result,
                             chain)) {
    goto err;
  }

  X509_free(ssl->s3->new_session->peer);
  X509 *leaf = sk_X509_value(chain, 0);
  X509_up_ref(leaf);
  ssl->s3->new_session->peer = leaf;

  sk_X509_pop_free(ssl->s3->new_session->cert_chain, X509_free);
  ssl->s3->new_session->cert_chain = chain;
  chain = NULL;

  ret = 1;

err:
  sk_X509_pop_free(chain, X509_free);
  return ret;
}

int tls13_process_certificate_verify(SSL *ssl) {
  int ret = 0;
  X509 *peer = ssl->s3->new_session->peer;
  EVP_PKEY *pkey = NULL;
  uint8_t *msg = NULL;
  size_t msg_len;

  /* Filter out unsupported certificate types. */
  pkey = X509_get_pubkey(peer);
  if (pkey == NULL) {
    goto err;
  }

  CBS cbs, signature;
  uint16_t signature_algorithm;
  CBS_init(&cbs, ssl->init_msg, ssl->init_num);
  if (!CBS_get_u16(&cbs, &signature_algorithm) ||
      !CBS_get_u16_length_prefixed(&cbs, &signature) ||
      CBS_len(&cbs) != 0) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    goto err;
  }

  int al;
  if (!tls12_check_peer_sigalg(ssl, &al, signature_algorithm)) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, al);
    goto err;
  }
  ssl->s3->tmp.peer_signature_algorithm = signature_algorithm;

  if (!tls13_get_cert_verify_signature_input(
          ssl, &msg, &msg_len,
          ssl->server ? ssl_cert_verify_client : ssl_cert_verify_server)) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
    goto err;
  }

  int sig_ok =
      ssl_public_key_verify(ssl, CBS_data(&signature), CBS_len(&signature),
                            signature_algorithm, pkey, msg, msg_len);
#if defined(BORINGSSL_UNSAFE_FUZZER_MODE)
  sig_ok = 1;
  ERR_clear_error();
#endif
  if (!sig_ok) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_SIGNATURE);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECRYPT_ERROR);
    goto err;
  }

  ret = 1;

err:
  EVP_PKEY_free(pkey);
  OPENSSL_free(msg);
  return ret;
}

int tls13_check_message_type(SSL *ssl, int type) {
  if (ssl->s3->tmp.message_type != type) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNEXPECTED_MESSAGE);
    OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_MESSAGE);
    ERR_add_error_dataf("got type %d, wanted type %d",
                        ssl->s3->tmp.message_type, type);
    return 0;
  }

  return 1;
}

int tls13_process_finished(SSL *ssl) {
  uint8_t verify_data[EVP_MAX_MD_SIZE];
  size_t verify_data_len;
  if (!tls13_finished_mac(ssl, verify_data, &verify_data_len, !ssl->server)) {
    return 0;
  }

  int finished_ok =
      ssl->init_num == verify_data_len &&
      CRYPTO_memcmp(verify_data, ssl->init_msg, verify_data_len) == 0;
#if defined(BORINGSSL_UNSAFE_FUZZER_MODE)
  finished_ok = 1;
#endif
  if (!finished_ok) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECRYPT_ERROR);
    OPENSSL_PUT_ERROR(SSL, SSL_R_DIGEST_CHECK_FAILED);
    return 0;
  }

  return 1;
}

int tls13_prepare_certificate(SSL *ssl) {
  CBB cbb, body;
  if (!ssl->method->init_message(ssl, &cbb, &body, SSL3_MT_CERTIFICATE) ||
      /* The request context is always empty in the handshake. */
      !CBB_add_u8(&body, 0) ||
      !ssl_add_cert_chain(ssl, &body) ||
      !ssl->method->finish_message(ssl, &cbb)) {
    CBB_cleanup(&cbb);
    return 0;
  }

  return 1;
}

enum ssl_private_key_result_t tls13_prepare_certificate_verify(
    SSL *ssl, int is_first_run) {
  enum ssl_private_key_result_t ret = ssl_private_key_failure;
  uint8_t *msg = NULL;
  size_t msg_len;
  CBB cbb, body;
  CBB_zero(&cbb);

  uint16_t signature_algorithm;
  if (!tls1_choose_signature_algorithm(ssl, &signature_algorithm)) {
    goto err;
  }
  if (!ssl->method->init_message(ssl, &cbb, &body,
                                 SSL3_MT_CERTIFICATE_VERIFY) ||
      !CBB_add_u16(&body, signature_algorithm)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    goto err;
  }

  /* Sign the digest. */
  CBB child;
  const size_t max_sig_len = ssl_private_key_max_signature_len(ssl);
  uint8_t *sig;
  size_t sig_len;
  if (!CBB_add_u16_length_prefixed(&body, &child) ||
      !CBB_reserve(&child, &sig, max_sig_len)) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
    goto err;
  }

  enum ssl_private_key_result_t sign_result;
  if (is_first_run) {
    if (!tls13_get_cert_verify_signature_input(
            ssl, &msg, &msg_len,
            ssl->server ? ssl_cert_verify_server : ssl_cert_verify_client)) {
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
      goto err;
    }
    sign_result = ssl_private_key_sign(ssl, sig, &sig_len, max_sig_len,
                                       signature_algorithm, msg, msg_len);
  } else {
    sign_result = ssl_private_key_complete(ssl, sig, &sig_len, max_sig_len);
  }

  if (sign_result != ssl_private_key_success) {
    ret = sign_result;
    goto err;
  }

  if (!CBB_did_write(&child, sig_len) ||
      !ssl->method->finish_message(ssl, &cbb)) {
    goto err;
  }

  ret = ssl_private_key_success;

err:
  CBB_cleanup(&cbb);
  OPENSSL_free(msg);
  return ret;
}

int tls13_prepare_finished(SSL *ssl) {
  size_t verify_data_len;
  uint8_t verify_data[EVP_MAX_MD_SIZE];

  if (!tls13_finished_mac(ssl, verify_data, &verify_data_len, ssl->server)) {
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_INTERNAL_ERROR);
    OPENSSL_PUT_ERROR(SSL, SSL_R_DIGEST_CHECK_FAILED);
    return 0;
  }

  CBB cbb, body;
  if (!ssl->method->init_message(ssl, &cbb, &body, SSL3_MT_FINISHED) ||
      !CBB_add_bytes(&body, verify_data, verify_data_len) ||
      !ssl->method->finish_message(ssl, &cbb)) {
    CBB_cleanup(&cbb);
    return 0;
  }

  return 1;
}

static int tls13_receive_key_update(SSL *ssl) {
  CBS cbs;
  uint8_t key_update_request;
  CBS_init(&cbs, ssl->init_msg, ssl->init_num);
  if (!CBS_get_u8(&cbs, &key_update_request) ||
      CBS_len(&cbs) != 0 ||
      (key_update_request != SSL_KEY_UPDATE_NOT_REQUESTED &&
       key_update_request != SSL_KEY_UPDATE_REQUESTED)) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_DECODE_ERROR);
    ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_DECODE_ERROR);
    return 0;
  }

  /* TODO(svaldez): Send KeyUpdate if |key_update_request| is
   * |SSL_KEY_UPDATE_REQUESTED|. */
  return tls13_rotate_traffic_key(ssl, evp_aead_open);
}

int tls13_post_handshake(SSL *ssl) {
  if (ssl->s3->tmp.message_type == SSL3_MT_KEY_UPDATE) {
    ssl->s3->key_update_count++;
    if (ssl->s3->key_update_count > kMaxKeyUpdates) {
      OPENSSL_PUT_ERROR(SSL, SSL_R_TOO_MANY_KEY_UPDATES);
      ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNEXPECTED_MESSAGE);
      return 0;
    }

    return tls13_receive_key_update(ssl);
  }

  ssl->s3->key_update_count = 0;

  if (ssl->s3->tmp.message_type == SSL3_MT_NEW_SESSION_TICKET &&
      !ssl->server) {
    return tls13_process_new_session_ticket(ssl);
  }

  // TODO(svaldez): Handle post-handshake authentication.

  ssl3_send_alert(ssl, SSL3_AL_FATAL, SSL_AD_UNEXPECTED_MESSAGE);
  OPENSSL_PUT_ERROR(SSL, SSL_R_UNEXPECTED_MESSAGE);
  return 0;
}

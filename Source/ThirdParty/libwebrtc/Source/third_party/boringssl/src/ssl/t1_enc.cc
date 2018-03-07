/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright (c) 1998-2007 The OpenSSL Project.  All rights reserved.
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
 * Copyright 2005 Nokia. All rights reserved.
 *
 * The portions of the attached software ("Contribution") is developed by
 * Nokia Corporation and is licensed pursuant to the OpenSSL open source
 * license.
 *
 * The Contribution, originally written by Mika Kousa and Pasi Eronen of
 * Nokia Corporation, consists of the "PSK" (Pre-Shared Key) ciphersuites
 * support (see RFC 4279) to OpenSSL.
 *
 * No patent licenses or other rights except those expressly stated in
 * the OpenSSL open source license shall be deemed granted or received
 * expressly, by implication, estoppel, or otherwise.
 *
 * No assurances are provided by Nokia that the Contribution does not
 * infringe the patent or other intellectual property rights of any third
 * party or that the license provides you with all the necessary rights
 * to make use of the Contribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. IN
 * ADDITION TO THE DISCLAIMERS INCLUDED IN THE LICENSE, NOKIA
 * SPECIFICALLY DISCLAIMS ANY LIABILITY FOR CLAIMS BROUGHT BY YOU OR ANY
 * OTHER ENTITY BASED ON INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS OR
 * OTHERWISE. */

#include <openssl/ssl.h>

#include <assert.h>
#include <string.h>

#include <utility>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>
#include <openssl/mem.h>
#include <openssl/nid.h>
#include <openssl/rand.h>

#include "../crypto/internal.h"
#include "internal.h"


namespace bssl {

// tls1_P_hash computes the TLS P_<hash> function as described in RFC 5246,
// section 5. It XORs |out.size()| bytes to |out|, using |md| as the hash and
// |secret| as the secret. |label|, |seed1|, and |seed2| are concatenated to
// form the seed parameter. It returns true on success and false on failure.
static bool tls1_P_hash(Span<uint8_t> out, const EVP_MD *md,
                        Span<const uint8_t> secret, Span<const char> label,
                        Span<const uint8_t> seed1, Span<const uint8_t> seed2) {
  ScopedHMAC_CTX ctx, ctx_tmp, ctx_init;
  uint8_t A1[EVP_MAX_MD_SIZE];
  unsigned A1_len;
  bool ret = false;

  size_t chunk = EVP_MD_size(md);

  if (!HMAC_Init_ex(ctx_init.get(), secret.data(), secret.size(), md,
                    nullptr) ||
      !HMAC_CTX_copy_ex(ctx.get(), ctx_init.get()) ||
      !HMAC_Update(ctx.get(), reinterpret_cast<const uint8_t *>(label.data()),
                   label.size()) ||
      !HMAC_Update(ctx.get(), seed1.data(), seed1.size()) ||
      !HMAC_Update(ctx.get(), seed2.data(), seed2.size()) ||
      !HMAC_Final(ctx.get(), A1, &A1_len)) {
    goto err;
  }

  for (;;) {
    unsigned len;
    uint8_t hmac[EVP_MAX_MD_SIZE];
    if (!HMAC_CTX_copy_ex(ctx.get(), ctx_init.get()) ||
        !HMAC_Update(ctx.get(), A1, A1_len) ||
        // Save a copy of |ctx| to compute the next A1 value below.
        (out.size() > chunk && !HMAC_CTX_copy_ex(ctx_tmp.get(), ctx.get())) ||
        !HMAC_Update(ctx.get(), reinterpret_cast<const uint8_t *>(label.data()),
                     label.size()) ||
        !HMAC_Update(ctx.get(), seed1.data(), seed1.size()) ||
        !HMAC_Update(ctx.get(), seed2.data(), seed2.size()) ||
        !HMAC_Final(ctx.get(), hmac, &len)) {
      goto err;
    }
    assert(len == chunk);

    // XOR the result into |out|.
    if (len > out.size()) {
      len = out.size();
    }
    for (unsigned i = 0; i < len; i++) {
      out[i] ^= hmac[i];
    }
    out = out.subspan(len);

    if (out.empty()) {
      break;
    }

    // Calculate the next A1 value.
    if (!HMAC_Final(ctx_tmp.get(), A1, &A1_len)) {
      goto err;
    }
  }

  ret = true;

err:
  OPENSSL_cleanse(A1, sizeof(A1));
  return ret;
}

bool tls1_prf(const EVP_MD *digest, Span<uint8_t> out,
              Span<const uint8_t> secret, Span<const char> label,
              Span<const uint8_t> seed1, Span<const uint8_t> seed2) {
  if (out.empty()) {
    return true;
  }

  OPENSSL_memset(out.data(), 0, out.size());

  if (digest == EVP_md5_sha1()) {
    // If using the MD5/SHA1 PRF, |secret| is partitioned between MD5 and SHA-1.
    size_t secret_half = secret.size() - (secret.size() / 2);
    if (!tls1_P_hash(out, EVP_md5(), secret.subspan(0, secret_half), label,
                     seed1, seed2)) {
      return false;
    }

    // Note that, if |secret.size()| is odd, the two halves share a byte.
    secret = secret.subspan(secret.size() - secret_half);
    digest = EVP_sha1();
  }

  return tls1_P_hash(out, digest, secret, label, seed1, seed2);
}

static bool ssl3_prf(Span<uint8_t> out, Span<const uint8_t> secret,
                     Span<const char> label, Span<const uint8_t> seed1,
                     Span<const uint8_t> seed2) {
  ScopedEVP_MD_CTX md5;
  ScopedEVP_MD_CTX sha1;
  uint8_t buf[16], smd[SHA_DIGEST_LENGTH];
  uint8_t c = 'A';
  size_t k = 0;
  while (!out.empty()) {
    k++;
    if (k > sizeof(buf)) {
      // bug: 'buf' is too small for this ciphersuite
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return false;
    }

    for (size_t j = 0; j < k; j++) {
      buf[j] = c;
    }
    c++;
    if (!EVP_DigestInit_ex(sha1.get(), EVP_sha1(), NULL)) {
      OPENSSL_PUT_ERROR(SSL, ERR_LIB_EVP);
      return false;
    }
    EVP_DigestUpdate(sha1.get(), buf, k);
    EVP_DigestUpdate(sha1.get(), secret.data(), secret.size());
    // |label| is ignored for SSLv3.
    EVP_DigestUpdate(sha1.get(), seed1.data(), seed1.size());
    EVP_DigestUpdate(sha1.get(), seed2.data(), seed2.size());
    EVP_DigestFinal_ex(sha1.get(), smd, NULL);

    if (!EVP_DigestInit_ex(md5.get(), EVP_md5(), NULL)) {
      OPENSSL_PUT_ERROR(SSL, ERR_LIB_EVP);
      return false;
    }
    EVP_DigestUpdate(md5.get(), secret.data(), secret.size());
    EVP_DigestUpdate(md5.get(), smd, SHA_DIGEST_LENGTH);
    if (out.size() < MD5_DIGEST_LENGTH) {
      EVP_DigestFinal_ex(md5.get(), smd, NULL);
      OPENSSL_memcpy(out.data(), smd, out.size());
      break;
    }
    EVP_DigestFinal_ex(md5.get(), out.data(), NULL);
    out = out.subspan(MD5_DIGEST_LENGTH);
  }

  OPENSSL_cleanse(smd, SHA_DIGEST_LENGTH);
  return true;
}

static bool get_key_block_lengths(const SSL *ssl, size_t *out_mac_secret_len,
                                  size_t *out_key_len, size_t *out_iv_len,
                                  const SSL_CIPHER *cipher) {
  const EVP_AEAD *aead = NULL;
  if (!ssl_cipher_get_evp_aead(&aead, out_mac_secret_len, out_iv_len, cipher,
                               ssl_protocol_version(ssl), SSL_is_dtls(ssl))) {
    OPENSSL_PUT_ERROR(SSL, SSL_R_CIPHER_OR_HASH_UNAVAILABLE);
    return false;
  }

  *out_key_len = EVP_AEAD_key_length(aead);
  if (*out_mac_secret_len > 0) {
    // For "stateful" AEADs (i.e. compatibility with pre-AEAD cipher suites) the
    // key length reported by |EVP_AEAD_key_length| will include the MAC key
    // bytes and initial implicit IV.
    if (*out_key_len < *out_mac_secret_len + *out_iv_len) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return false;
    }
    *out_key_len -= *out_mac_secret_len + *out_iv_len;
  }

  return true;
}

static bool setup_key_block(SSL_HANDSHAKE *hs) {
  SSL *const ssl = hs->ssl;
  if (!hs->key_block.empty()) {
    return true;
  }

  size_t mac_secret_len, key_len, fixed_iv_len;
  Array<uint8_t> key_block;
  if (!get_key_block_lengths(ssl, &mac_secret_len, &key_len, &fixed_iv_len,
                             hs->new_cipher) ||
      !key_block.Init(2 * (mac_secret_len + key_len + fixed_iv_len)) ||
      !SSL_generate_key_block(ssl, key_block.data(), key_block.size())) {
    return false;
  }

  hs->key_block = std::move(key_block);
  return true;
}

int tls1_change_cipher_state(SSL_HANDSHAKE *hs,
                             evp_aead_direction_t direction) {
  SSL *const ssl = hs->ssl;
  // Ensure the key block is set up.
  size_t mac_secret_len, key_len, iv_len;
  if (!setup_key_block(hs) ||
      !get_key_block_lengths(ssl, &mac_secret_len, &key_len, &iv_len,
                             hs->new_cipher)) {
    return 0;
  }

  if ((mac_secret_len + key_len + iv_len) * 2 != hs->key_block.size()) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  Span<const uint8_t> key_block = hs->key_block;
  Span<const uint8_t> mac_secret, key, iv;
  if (direction == (ssl->server ? evp_aead_open : evp_aead_seal)) {
    // Use the client write (server read) keys.
    mac_secret = key_block.subspan(0, mac_secret_len);
    key = key_block.subspan(2 * mac_secret_len, key_len);
    iv = key_block.subspan(2 * mac_secret_len + 2 * key_len, iv_len);
  } else {
    // Use the server write (client read) keys.
    mac_secret = key_block.subspan(mac_secret_len, mac_secret_len);
    key = key_block.subspan(2 * mac_secret_len + key_len, key_len);
    iv = key_block.subspan(2 * mac_secret_len + 2 * key_len + iv_len, iv_len);
  }

  UniquePtr<SSLAEADContext> aead_ctx =
      SSLAEADContext::Create(direction, ssl->version, SSL_is_dtls(ssl),
                             hs->new_cipher, key, mac_secret, iv);
  if (!aead_ctx) {
    return 0;
  }

  if (direction == evp_aead_open) {
    return ssl->method->set_read_state(ssl, std::move(aead_ctx));
  }

  return ssl->method->set_write_state(ssl, std::move(aead_ctx));
}

int tls1_generate_master_secret(SSL_HANDSHAKE *hs, uint8_t *out,
                                Span<const uint8_t> premaster) {
  static const char kMasterSecretLabel[] = "master secret";
  static const char kExtendedMasterSecretLabel[] = "extended master secret";

  const SSL *ssl = hs->ssl;
  auto out_span = MakeSpan(out, SSL3_MASTER_SECRET_SIZE);
  if (hs->extended_master_secret) {
    auto label = MakeConstSpan(kExtendedMasterSecretLabel,
                               sizeof(kExtendedMasterSecretLabel) - 1);
    uint8_t digests[EVP_MAX_MD_SIZE];
    size_t digests_len;
    if (!hs->transcript.GetHash(digests, &digests_len) ||
        !tls1_prf(hs->transcript.Digest(), out_span, premaster, label,
                  MakeConstSpan(digests, digests_len), {})) {
      return 0;
    }
  } else {
    auto label =
        MakeConstSpan(kMasterSecretLabel, sizeof(kMasterSecretLabel) - 1);
    if (ssl_protocol_version(ssl) == SSL3_VERSION) {
      if (!ssl3_prf(out_span, premaster, label, ssl->s3->client_random,
                    ssl->s3->server_random)) {
        return 0;
      }
    } else {
      if (!tls1_prf(hs->transcript.Digest(), out_span, premaster, label,
                    ssl->s3->client_random, ssl->s3->server_random)) {
        return 0;
      }
    }
  }

  return SSL3_MASTER_SECRET_SIZE;
}

}  // namespace bssl

using namespace bssl;

size_t SSL_get_key_block_len(const SSL *ssl) {
  size_t mac_secret_len, key_len, fixed_iv_len;
  if (!get_key_block_lengths(ssl, &mac_secret_len, &key_len, &fixed_iv_len,
                             SSL_get_current_cipher(ssl))) {
    ERR_clear_error();
    return 0;
  }

  return 2 * (mac_secret_len + key_len + fixed_iv_len);
}

int SSL_generate_key_block(const SSL *ssl, uint8_t *out, size_t out_len) {
  const SSL_SESSION *session = SSL_get_session(ssl);
  auto out_span = MakeSpan(out, out_len);
  auto master_key =
      MakeConstSpan(session->master_key, session->master_key_length);
  static const char kLabel[] = "key expansion";
  auto label = MakeConstSpan(kLabel, sizeof(kLabel) - 1);

  if (ssl_protocol_version(ssl) == SSL3_VERSION) {
    return ssl3_prf(out_span, master_key, label, ssl->s3->server_random,
                    ssl->s3->client_random);
  }

  const EVP_MD *digest = ssl_session_get_digest(session);
  return tls1_prf(digest, out_span, master_key, label, ssl->s3->server_random,
                  ssl->s3->client_random);
}

int SSL_export_keying_material(SSL *ssl, uint8_t *out, size_t out_len,
                               const char *label, size_t label_len,
                               const uint8_t *context, size_t context_len,
                               int use_context) {
  if (!ssl->s3->have_version || ssl->version == SSL3_VERSION) {
    return 0;
  }

  // Exporters may not be used in the middle of a renegotiation.
  if (SSL_in_init(ssl) && !SSL_in_false_start(ssl)) {
    return 0;
  }

  if (ssl_protocol_version(ssl) >= TLS1_3_VERSION) {
    return tls13_export_keying_material(ssl, out, out_len, label, label_len,
                                        context, context_len, use_context);
  }

  size_t seed_len = 2 * SSL3_RANDOM_SIZE;
  if (use_context) {
    if (context_len >= 1u << 16) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_OVERFLOW);
      return 0;
    }
    seed_len += 2 + context_len;
  }
  Array<uint8_t> seed;
  if (!seed.Init(seed_len)) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
    return 0;
  }

  OPENSSL_memcpy(seed.data(), ssl->s3->client_random, SSL3_RANDOM_SIZE);
  OPENSSL_memcpy(seed.data() + SSL3_RANDOM_SIZE, ssl->s3->server_random,
                 SSL3_RANDOM_SIZE);
  if (use_context) {
    seed[2 * SSL3_RANDOM_SIZE] = static_cast<uint8_t>(context_len >> 8);
    seed[2 * SSL3_RANDOM_SIZE + 1] = static_cast<uint8_t>(context_len);
    OPENSSL_memcpy(seed.data() + 2 * SSL3_RANDOM_SIZE + 2, context, context_len);
  }

  const SSL_SESSION *session = SSL_get_session(ssl);
  const EVP_MD *digest = ssl_session_get_digest(session);
  return tls1_prf(
      digest, MakeSpan(out, out_len),
      MakeConstSpan(session->master_key, session->master_key_length),
      MakeConstSpan(label, label_len), seed, {});
}

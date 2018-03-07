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

#include <openssl/buf.h>
#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/md5.h>
#include <openssl/nid.h>
#include <openssl/sha.h>

#include "../crypto/internal.h"
#include "internal.h"


namespace bssl {

SSLTranscript::SSLTranscript() {}

SSLTranscript::~SSLTranscript() {}

bool SSLTranscript::Init() {
  buffer_.reset(BUF_MEM_new());
  if (!buffer_) {
    return false;
  }

  hash_.Reset();
  md5_.Reset();
  return true;
}

// InitDigestWithData calls |EVP_DigestInit_ex| on |ctx| with |md| and then
// writes the data in |buf| to it.
static bool InitDigestWithData(EVP_MD_CTX *ctx, const EVP_MD *md,
                               const BUF_MEM *buf) {
  if (!EVP_DigestInit_ex(ctx, md, NULL)) {
    return false;
  }
  EVP_DigestUpdate(ctx, buf->data, buf->length);
  return true;
}

bool SSLTranscript::InitHash(uint16_t version, const SSL_CIPHER *cipher) {
  const EVP_MD *md = ssl_get_handshake_digest(version, cipher);

  // To support SSL 3.0's Finished and CertificateVerify constructions,
  // EVP_md5_sha1() is split into MD5 and SHA-1 halves. When SSL 3.0 is removed,
  // we can simplify this.
  if (md == EVP_md5_sha1()) {
    if (!InitDigestWithData(md5_.get(), EVP_md5(), buffer_.get())) {
      return false;
    }
    md = EVP_sha1();
  }

  return InitDigestWithData(hash_.get(), md, buffer_.get());
}

void SSLTranscript::FreeBuffer() {
  buffer_.reset();
}

size_t SSLTranscript::DigestLen() const {
  return EVP_MD_size(Digest());
}

const EVP_MD *SSLTranscript::Digest() const {
  if (EVP_MD_CTX_md(md5_.get()) != nullptr) {
    return EVP_md5_sha1();
  }
  return EVP_MD_CTX_md(hash_.get());
}

bool SSLTranscript::UpdateForHelloRetryRequest() {
  if (buffer_) {
    buffer_->length = 0;
  }

  uint8_t old_hash[EVP_MAX_MD_SIZE];
  size_t hash_len;
  if (!GetHash(old_hash, &hash_len)) {
    return false;
  }
  const uint8_t header[4] = {SSL3_MT_MESSAGE_HASH, 0, 0,
                             static_cast<uint8_t>(hash_len)};
  if (!EVP_DigestInit_ex(hash_.get(), Digest(), nullptr) ||
      !Update(header) ||
      !Update(MakeConstSpan(old_hash, hash_len))) {
    return false;
  }
  return true;
}

bool SSLTranscript::CopyHashContext(EVP_MD_CTX *ctx) {
  return EVP_MD_CTX_copy_ex(ctx, hash_.get());
}

bool SSLTranscript::Update(Span<const uint8_t> in) {
  // Depending on the state of the handshake, either the handshake buffer may be
  // active, the rolling hash, or both.
  if (buffer_ &&
      !BUF_MEM_append(buffer_.get(), in.data(), in.size())) {
    return false;
  }

  if (EVP_MD_CTX_md(hash_.get()) != NULL) {
    EVP_DigestUpdate(hash_.get(), in.data(), in.size());
  }
  if (EVP_MD_CTX_md(md5_.get()) != NULL) {
    EVP_DigestUpdate(md5_.get(), in.data(), in.size());
  }

  return true;
}

bool SSLTranscript::GetHash(uint8_t *out, size_t *out_len) {
  ScopedEVP_MD_CTX ctx;
  unsigned md5_len = 0;
  if (EVP_MD_CTX_md(md5_.get()) != NULL) {
    if (!EVP_MD_CTX_copy_ex(ctx.get(), md5_.get()) ||
        !EVP_DigestFinal_ex(ctx.get(), out, &md5_len)) {
      return false;
    }
  }

  unsigned len;
  if (!EVP_MD_CTX_copy_ex(ctx.get(), hash_.get()) ||
      !EVP_DigestFinal_ex(ctx.get(), out + md5_len, &len)) {
    return false;
  }

  *out_len = md5_len + len;
  return true;
}

static bool SSL3HandshakeMAC(const SSL_SESSION *session,
                             const EVP_MD_CTX *ctx_template, const char *sender,
                             size_t sender_len, uint8_t *p, size_t *out_len) {
  ScopedEVP_MD_CTX ctx;
  if (!EVP_MD_CTX_copy_ex(ctx.get(), ctx_template)) {
    OPENSSL_PUT_ERROR(SSL, ERR_LIB_EVP);
    return false;
  }

  static const uint8_t kPad1[48] = {
      0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
      0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
      0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
      0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
  };

  static const uint8_t kPad2[48] = {
      0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
      0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
      0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
      0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
  };

  size_t n = EVP_MD_CTX_size(ctx.get());

  size_t npad = (48 / n) * n;
  EVP_DigestUpdate(ctx.get(), sender, sender_len);
  EVP_DigestUpdate(ctx.get(), session->master_key, session->master_key_length);
  EVP_DigestUpdate(ctx.get(), kPad1, npad);
  unsigned md_buf_len;
  uint8_t md_buf[EVP_MAX_MD_SIZE];
  EVP_DigestFinal_ex(ctx.get(), md_buf, &md_buf_len);

  if (!EVP_DigestInit_ex(ctx.get(), EVP_MD_CTX_md(ctx.get()), NULL)) {
    OPENSSL_PUT_ERROR(SSL, ERR_LIB_EVP);
    return false;
  }
  EVP_DigestUpdate(ctx.get(), session->master_key, session->master_key_length);
  EVP_DigestUpdate(ctx.get(), kPad2, npad);
  EVP_DigestUpdate(ctx.get(), md_buf, md_buf_len);
  unsigned len;
  EVP_DigestFinal_ex(ctx.get(), p, &len);

  *out_len = len;
  return true;
}

bool SSLTranscript::GetSSL3CertVerifyHash(uint8_t *out, size_t *out_len,
                                          const SSL_SESSION *session,
                                          uint16_t signature_algorithm) {
  if (Digest() != EVP_md5_sha1()) {
    OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
    return false;
  }

  if (signature_algorithm == SSL_SIGN_RSA_PKCS1_MD5_SHA1) {
    size_t md5_len, len;
    if (!SSL3HandshakeMAC(session, md5_.get(), NULL, 0, out, &md5_len) ||
        !SSL3HandshakeMAC(session, hash_.get(), NULL, 0, out + md5_len, &len)) {
      return false;
    }
    *out_len = md5_len + len;
    return true;
  }

  if (signature_algorithm == SSL_SIGN_ECDSA_SHA1) {
    return SSL3HandshakeMAC(session, hash_.get(), NULL, 0, out, out_len);
  }

  OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
  return false;
}

bool SSLTranscript::GetFinishedMAC(uint8_t *out, size_t *out_len,
                                   const SSL_SESSION *session,
                                   bool from_server) {
  if (session->ssl_version == SSL3_VERSION) {
    if (Digest() != EVP_md5_sha1()) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
      return false;
    }

    const char *sender = from_server ? SSL3_MD_SERVER_FINISHED_CONST
                                     : SSL3_MD_CLIENT_FINISHED_CONST;
    const size_t sender_len = 4;
    size_t md5_len, len;
    if (!SSL3HandshakeMAC(session, md5_.get(), sender, sender_len, out,
                          &md5_len) ||
        !SSL3HandshakeMAC(session, hash_.get(), sender, sender_len,
                          out + md5_len, &len)) {
      return false;
    }

    *out_len = md5_len + len;
    return true;
  }

  // At this point, the handshake should have released the handshake buffer on
  // its own.
  assert(!buffer_);

  static const char kClientLabel[] = "client finished";
  static const char kServerLabel[] = "server finished";
  auto label = from_server
                   ? MakeConstSpan(kServerLabel, sizeof(kServerLabel) - 1)
                   : MakeConstSpan(kClientLabel, sizeof(kClientLabel) - 1);

  uint8_t digests[EVP_MAX_MD_SIZE];
  size_t digests_len;
  if (!GetHash(digests, &digests_len)) {
    return false;
  }

  static const size_t kFinishedLen = 12;
  if (!tls1_prf(Digest(), MakeSpan(out, kFinishedLen),
                MakeConstSpan(session->master_key, session->master_key_length),
                label, MakeConstSpan(digests, digests_len), {})) {
    return false;
  }

  *out_len = kFinishedLen;
  return true;
}

}  // namespace bssl

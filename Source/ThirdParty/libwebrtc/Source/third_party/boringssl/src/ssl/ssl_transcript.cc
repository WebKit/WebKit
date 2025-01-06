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

#include <openssl/buf.h>
#include <openssl/digest.h>
#include <openssl/err.h>

#include "internal.h"


BSSL_NAMESPACE_BEGIN

SSLTranscript::SSLTranscript(bool is_dtls) : is_dtls_(is_dtls) {}

SSLTranscript::~SSLTranscript() {}

bool SSLTranscript::Init() {
  buffer_.reset(BUF_MEM_new());
  if (!buffer_) {
    return false;
  }

  hash_.Reset();
  return true;
}

bool SSLTranscript::InitHash(uint16_t version, const SSL_CIPHER *cipher) {
  version_ = version;
  const EVP_MD *md = ssl_get_handshake_digest(version, cipher);
  if (Digest() == md) {
    // No need to re-hash the buffer.
    return true;
  }
  if (!HashBuffer(hash_.get(), md)) {
    return false;
  }
  if (is_dtls_ && version_ >= TLS1_3_VERSION) {
    // In DTLS 1.3, prior to the call to InitHash, the message (if present) in
    // the buffer has the DTLS 1.2 header. After the call to InitHash, the TLS
    // 1.3 header is written by SSLTranscript::Update. If the buffer isn't freed
    // here, it would have a mix of different header formats and using it would
    // yield wrong results. However, there's no need for the buffer once the
    // version and the digest for the cipher suite are known, so the buffer is
    // freed here to avoid potential misuse of the SSLTranscript object.
    FreeBuffer();
  }
  return true;
}

bool SSLTranscript::HashBuffer(EVP_MD_CTX *ctx, const EVP_MD *digest) const {
  if (!EVP_DigestInit_ex(ctx, digest, nullptr)) {
    return false;
  }
  if (!is_dtls_ || version_ < TLS1_3_VERSION) {
    return EVP_DigestUpdate(ctx, buffer_->data, buffer_->length);
  }

  // If the version is DTLS 1.3 and we still have a buffer, then there should be
  // at most a single DTLSHandshake message in the buffer, for the ClientHello.
  // On the server side, the version (DTLS 1.3) and cipher suite are chosen in
  // response to the first ClientHello, and InitHash is called before that
  // ClientHello is added to the SSLTranscript, so the buffer is empty if this
  // SSLTranscript is on the server.
  if (buffer_->length == 0) {
    return true;
  }

  // On the client side, we can receive either a ServerHello or
  // HelloRetryRequest in response to the ClientHello. Regardless of which
  // message we receive, the client code calls InitHash before updating the
  // transcript with that message, so the ClientHello is the only message in the
  // buffer. In DTLS 1.3, we need to skip the message_seq, fragment_offset, and
  // fragment_length fields from the DTLSHandshake message in the buffer. The
  // structure of a DTLSHandshake message is as follows (RFC 9147, section 5.2):
  //
  //   struct {
  //       HandshakeType msg_type;    /* handshake type */
  //       uint24 length;             /* bytes in message */
  //       uint16 message_seq;        /* DTLS-required field */
  //       uint24 fragment_offset;    /* DTLS-required field */
  //       uint24 fragment_length;    /* DTLS-required field */
  //       select (msg_type) {
  //         /* omitted for brevity */
  //       } body;
  //   } DTLSHandshake;
  CBS buf, header;
  CBS_init(&buf, reinterpret_cast<uint8_t *>(buffer_->data), buffer_->length);
  if (!CBS_get_bytes(&buf, &header, 4) ||                             //
      !CBS_skip(&buf, 8) ||                                           //
      !EVP_DigestUpdate(ctx, CBS_data(&header), CBS_len(&header)) ||  //
      !EVP_DigestUpdate(ctx, CBS_data(&buf), CBS_len(&buf))) {
    return false;
  }
  return true;
}

void SSLTranscript::FreeBuffer() {
  buffer_.reset();
}

size_t SSLTranscript::DigestLen() const {
  return EVP_MD_size(Digest());
}

const EVP_MD *SSLTranscript::Digest() const {
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
      !AddToBufferOrHash(header) ||
      !AddToBufferOrHash(MakeConstSpan(old_hash, hash_len))) {
    return false;
  }
  return true;
}

bool SSLTranscript::CopyToHashContext(EVP_MD_CTX *ctx,
                                      const EVP_MD *digest) const {
  const EVP_MD *transcript_digest = Digest();
  if (transcript_digest != nullptr &&
      EVP_MD_type(transcript_digest) == EVP_MD_type(digest)) {
    return EVP_MD_CTX_copy_ex(ctx, hash_.get());
  }

  if (buffer_) {
    return HashBuffer(ctx, digest);
  }

  OPENSSL_PUT_ERROR(SSL, ERR_R_INTERNAL_ERROR);
  return false;
}

bool SSLTranscript::Update(Span<const uint8_t> in) {
  if (!is_dtls_ || version_ < TLS1_3_VERSION) {
    return AddToBufferOrHash(in);
  }
  if (in.size() < DTLS1_HM_HEADER_LENGTH) {
    return false;
  }
  // The message passed into Update is the whole Handshake or DTLSHandshake
  // message, including the msg_type and length. In DTLS, the DTLSHandshake
  // message also has message_seq, fragment_offset, and fragment_length
  // fields. In DTLS 1.3, those fields are omitted so that the same
  // transcript format as TLS 1.3 is used. This means we write the 1-byte
  // msg_type, 3-byte length, then skip 2+3+3 bytes for the DTLS-specific
  // fields that get omitted.
  if (!AddToBufferOrHash(in.subspan(0, 4)) ||
      !AddToBufferOrHash(in.subspan(12))) {
    return false;
  }
  return true;
}

bool SSLTranscript::AddToBufferOrHash(Span<const uint8_t> in) {
  // Depending on the state of the handshake, either the handshake buffer may be
  // active, the rolling hash, or both.
  if (buffer_ &&
      !BUF_MEM_append(buffer_.get(), in.data(), in.size())) {
    return false;
  }

  if (EVP_MD_CTX_md(hash_.get()) != NULL) {
    EVP_DigestUpdate(hash_.get(), in.data(), in.size());
  }

  return true;
}

bool SSLTranscript::GetHash(uint8_t *out, size_t *out_len) const {
  ScopedEVP_MD_CTX ctx;
  unsigned len;
  if (!EVP_MD_CTX_copy_ex(ctx.get(), hash_.get()) ||
      !EVP_DigestFinal_ex(ctx.get(), out, &len)) {
    return false;
  }
  *out_len = len;
  return true;
}

bool SSLTranscript::GetFinishedMAC(uint8_t *out, size_t *out_len,
                                   const SSL_SESSION *session,
                                   bool from_server) const {
  static const char kClientLabel[] = "client finished";
  static const char kServerLabel[] = "server finished";
  auto label = from_server
                   ? MakeConstSpan(kServerLabel, sizeof(kServerLabel) - 1)
                   : MakeConstSpan(kClientLabel, sizeof(kClientLabel) - 1);

  uint8_t digest[EVP_MAX_MD_SIZE];
  size_t digest_len;
  if (!GetHash(digest, &digest_len)) {
    return false;
  }

  static const size_t kFinishedLen = 12;
  if (!tls1_prf(Digest(), MakeSpan(out, kFinishedLen), session->secret, label,
                MakeConstSpan(digest, digest_len), {})) {
    return false;
  }

  *out_len = kFinishedLen;
  return true;
}

BSSL_NAMESPACE_END

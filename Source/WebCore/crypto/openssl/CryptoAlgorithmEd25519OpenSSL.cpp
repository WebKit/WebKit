/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CryptoAlgorithmEd25519.h"

#if HAVE(OPENSSL_OKP)

#include "CryptoKeyOKP.h"
#include "ExceptionOr.h"
#include "OpenSSLUtilities.h"

namespace WebCore {

static constexpr size_t ed25519PointSize = 32;
static constexpr size_t ed25519SignatureSize = 64;

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmEd25519::platformSign(const CryptoKeyOKP& key, const Vector<uint8_t>& data)
{
    auto pkey = createOKPPrivateKey(key.namedCurve(), key.platformKey().span());
    if (!pkey)
        return Exception { ExceptionCode::OperationError };

    auto context = EvpDigestCtxPtr(EVP_MD_CTX_new());
    if (!context || EVP_DigestSignInit(context.get(), nullptr, nullptr, nullptr, pkey.get()) <= 0)
        return Exception { ExceptionCode::OperationError };

    Vector<uint8_t> signature(ed25519SignatureSize);
    size_t signatureLength = signature.size();
    if (EVP_DigestSign(context.get(), signature.mutableSpan().data(), &signatureLength, data.span().data(), data.size()) <= 0)
        return Exception { ExceptionCode::OperationError };

    ASSERT_UNUSED(signatureLength, signatureLength == ed25519SignatureSize);
    return signature;
}

ExceptionOr<bool> CryptoAlgorithmEd25519::platformVerify(const CryptoKeyOKP& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    if (signature.size() != ed25519SignatureSize)
        return false;

    // https://w3c.github.io/webcrypto/#ed25519-operations-verify requires rejecting a small-order
    // public key or R, which OpenSSL does not do.
    if (isSmallOrderPoint(key.platformKey().span()) || isSmallOrderPoint(signature.span().first(ed25519PointSize)))
        return false;

    auto pkey = createOKPPublicKey(key.namedCurve(), key.platformKey().span());
    if (!pkey)
        return false;

    auto context = EvpDigestCtxPtr(EVP_MD_CTX_new());
    if (!context || EVP_DigestVerifyInit(context.get(), nullptr, nullptr, nullptr, pkey.get()) <= 0)
        return false;

    return EVP_DigestVerify(context.get(), signature.span().data(), signature.size(), data.span().data(), data.size()) == 1;
}

} // namespace WebCore

#endif // HAVE(OPENSSL_OKP)

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
#include "CryptoAlgorithmX25519.h"

#if HAVE(OPENSSL_OKP)

#include "CryptoKeyOKP.h"
#include "OpenSSLUtilities.h"

namespace WebCore {

static constexpr size_t x25519SharedSecretSize = 32;

std::optional<Vector<uint8_t>> CryptoAlgorithmX25519::platformDeriveBits(const CryptoKeyOKP& baseKey, const CryptoKeyOKP& publicKey)
{
    auto privatePKey = createOKPPrivateKey(baseKey.namedCurve(), baseKey.platformKey().span());
    auto peerPKey = createOKPPublicKey(publicKey.namedCurve(), publicKey.platformKey().span());
    if (!privatePKey || !peerPKey)
        return std::nullopt;

    auto context = EvpPKeyCtxPtr(EVP_PKEY_CTX_new(privatePKey.get(), nullptr));
    if (!context || EVP_PKEY_derive_init(context.get()) <= 0 || EVP_PKEY_derive_set_peer(context.get(), peerPKey.get()) <= 0)
        return std::nullopt;

    // Derivation fails on an all-zero shared secret, which WebCrypto requires (RFC 7748, section 6.1).
    Vector<uint8_t> secret(x25519SharedSecretSize);
    size_t secretLength = secret.size();
    if (EVP_PKEY_derive(context.get(), secret.mutableSpan().data(), &secretLength) <= 0 || secretLength != secret.size())
        return std::nullopt;

    return secret;
}

} // namespace WebCore

#endif // HAVE(OPENSSL_OKP)

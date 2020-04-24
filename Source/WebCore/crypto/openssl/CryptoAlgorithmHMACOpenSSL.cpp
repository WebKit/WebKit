/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
#include "CryptoAlgorithmHMAC.h"

#if ENABLE(WEB_CRYPTO)

#include "CryptoKeyHMAC.h"
#include "OpenSSLCryptoUniquePtr.h"
#include <openssl/evp.h>
#include <wtf/CryptographicUtilities.h>

namespace WebCore {

static const EVP_MD* HMACAlgorithm(CryptoAlgorithmIdentifier hashFunction)
{
    switch (hashFunction) {
    case CryptoAlgorithmIdentifier::SHA_1:
        return EVP_sha1();
    case CryptoAlgorithmIdentifier::SHA_224:
        return EVP_sha224();
    case CryptoAlgorithmIdentifier::SHA_256:
        return EVP_sha256();
    case CryptoAlgorithmIdentifier::SHA_384:
        return EVP_sha384();
    case CryptoAlgorithmIdentifier::SHA_512:
        return EVP_sha512();
    default:
        return nullptr;
    }
}

static Optional<Vector<uint8_t>> calculateSignature(const EVP_MD* algorithm, const Vector<uint8_t>& key, const uint8_t* data, size_t dataLength)
{
    // Create the Message Digest Context
    EvpDigestCtxPtr ctx;
    if (!(ctx = EvpDigestCtxPtr(EVP_MD_CTX_create())))
        return WTF::nullopt;

    // Initialize the DigestSign operation
    EvpPKeyPtr hkey;
    if (!(hkey = EvpPKeyPtr(EVP_PKEY_new_mac_key(EVP_PKEY_HMAC, nullptr, key.data(), key.size()))))
        return WTF::nullopt;

    if (1 != EVP_DigestSignInit(ctx.get(), nullptr, algorithm, nullptr, hkey.get()))
        return WTF::nullopt;

    // Call update with the message
    if (1 != EVP_DigestSignUpdate(ctx.get(), data, dataLength))
        return WTF::nullopt;

    // Finalize the DigestSign operation
    size_t len = 0;
    if (1 != EVP_DigestSignFinal(ctx.get(), nullptr, &len))
        return WTF::nullopt;

    // Obtain the signature
    Vector<uint8_t> cipherText(len);
    if (1 != EVP_DigestSignFinal(ctx.get(), cipherText.data(), &len))
        return WTF::nullopt;

    return cipherText;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmHMAC::platformSign(const CryptoKeyHMAC& key, const Vector<uint8_t>& data)
{
    auto algorithm = HMACAlgorithm(key.hashAlgorithmIdentifier());
    if (!algorithm)
        return Exception { OperationError };

    auto result = calculateSignature(algorithm, key.key(), data.data(), data.size());
    if (!result)
        return Exception { OperationError };
    return WTFMove(*result);
}

ExceptionOr<bool> CryptoAlgorithmHMAC::platformVerify(const CryptoKeyHMAC& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    auto algorithm = HMACAlgorithm(key.hashAlgorithmIdentifier());
    if (!algorithm)
        return Exception { OperationError };

    auto expectedSignature = calculateSignature(algorithm, key.key(), data.data(), data.size());
    if (!expectedSignature)
        return Exception { OperationError };
    // Using a constant time comparison to prevent timing attacks.
    return signature.size() == expectedSignature->size() && !constantTimeMemcmp(expectedSignature->data(), signature.data(), expectedSignature->size());
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)

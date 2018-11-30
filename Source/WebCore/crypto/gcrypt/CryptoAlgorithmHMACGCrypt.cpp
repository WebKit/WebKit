/*
 * Copyright (C) 2014 Igalia S.L.
 * Copyright (C) 2016 SoftAtHome
 * Copyright (C) 2016 Apple Inc.
 * Copyright (C) 2016 Yusuke Suzuki <utatane.tea@gmail.com>.
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
#include <pal/crypto/gcrypt/Handle.h>
#include <wtf/CryptographicUtilities.h>

namespace WebCore {

static int getGCryptDigestAlgorithm(CryptoAlgorithmIdentifier hashFunction)
{
    switch (hashFunction) {
    case CryptoAlgorithmIdentifier::SHA_1:
        return GCRY_MAC_HMAC_SHA1;
    case CryptoAlgorithmIdentifier::SHA_224:
        return GCRY_MAC_HMAC_SHA224;
    case CryptoAlgorithmIdentifier::SHA_256:
        return GCRY_MAC_HMAC_SHA256;
    case CryptoAlgorithmIdentifier::SHA_384:
        return GCRY_MAC_HMAC_SHA384;
    case CryptoAlgorithmIdentifier::SHA_512:
        return GCRY_MAC_HMAC_SHA512;
    default:
        return GCRY_MAC_NONE;
    }
}

static std::optional<Vector<uint8_t>> calculateSignature(int algorithm, const Vector<uint8_t>& key, const uint8_t* data, size_t dataLength)
{
    const void* keyData = key.data() ? key.data() : reinterpret_cast<const uint8_t*>("");

    PAL::GCrypt::Handle<gcry_mac_hd_t> hd;
    gcry_error_t err = gcry_mac_open(&hd, algorithm, 0, nullptr);
    if (err)
        return std::nullopt;

    err = gcry_mac_setkey(hd, keyData, key.size());
    if (err)
        return std::nullopt;

    err = gcry_mac_write(hd, data, dataLength);
    if (err)
        return std::nullopt;

    size_t digestLength = gcry_mac_get_algo_maclen(algorithm);
    Vector<uint8_t> signature(digestLength);
    err = gcry_mac_read(hd, signature.data(), &digestLength);
    if (err)
        return std::nullopt;

    signature.resize(digestLength);
    return WTFMove(signature);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmHMAC::platformSign(const CryptoKeyHMAC& key, const Vector<uint8_t>& data)
{
    auto algorithm = getGCryptDigestAlgorithm(key.hashAlgorithmIdentifier());
    if (algorithm == GCRY_MAC_NONE)
        return Exception { OperationError };

    auto result = calculateSignature(algorithm, key.key(), data.data(), data.size());
    if (!result)
        return Exception { OperationError };
    return WTFMove(*result);
}

ExceptionOr<bool> CryptoAlgorithmHMAC::platformVerify(const CryptoKeyHMAC& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    auto algorithm = getGCryptDigestAlgorithm(key.hashAlgorithmIdentifier());
    if (algorithm == GCRY_MAC_NONE)
        return Exception { OperationError };

    auto expectedSignature = calculateSignature(algorithm, key.key(), data.data(), data.size());
    if (!expectedSignature)
        return Exception { OperationError };
    // Using a constant time comparison to prevent timing attacks.
    return signature.size() == expectedSignature->size() && !constantTimeMemcmp(expectedSignature->data(), signature.data(), expectedSignature->size());
}

}

#endif // ENABLE(WEB_CRYPTO)

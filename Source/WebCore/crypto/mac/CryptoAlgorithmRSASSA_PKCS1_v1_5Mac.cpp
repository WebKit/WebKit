/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "CryptoAlgorithmRSASSA_PKCS1_v1_5.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CommonCryptoUtilities.h"
#include "CryptoDigestAlgorithm.h"
#include "CryptoKeyRSA.h"

namespace WebCore {

static ExceptionOr<Vector<uint8_t>> signRSASSA_PKCS1_v1_5(CryptoAlgorithmIdentifier hash, const PlatformRSAKey key, size_t keyLength, const Vector<uint8_t>& data)
{
    CCDigestAlgorithm digestAlgorithm;
    if (!getCommonCryptoDigestAlgorithm(hash, digestAlgorithm))
        return Exception { OperationError };

    auto cryptoDigestAlgorithm = WebCore::cryptoDigestAlgorithm(hash);
    if (!cryptoDigestAlgorithm)
        return Exception { OperationError };
    auto digest = PAL::CryptoDigest::create(*cryptoDigestAlgorithm);
    if (!digest)
        return Exception { OperationError };
    digest->addBytes(data.data(), data.size());
    auto digestData = digest->computeHash();

    Vector<uint8_t> signature(keyLength / 8); // Per https://tools.ietf.org/html/rfc3447#section-8.2.1
    size_t signatureSize = signature.size();

    CCCryptorStatus status = CCRSACryptorSign(key, ccPKCS1Padding, digestData.data(), digestData.size(), digestAlgorithm, 0, signature.data(), &signatureSize);
    if (status)
        return Exception { OperationError };

    return WTFMove(signature);
}

static ExceptionOr<bool> verifyRSASSA_PKCS1_v1_5(CryptoAlgorithmIdentifier hash, const PlatformRSAKey key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    CCDigestAlgorithm digestAlgorithm;
    if (!getCommonCryptoDigestAlgorithm(hash, digestAlgorithm))
        return Exception { OperationError };

    auto cryptoDigestAlgorithm = WebCore::cryptoDigestAlgorithm(hash);
    if (!cryptoDigestAlgorithm)
        return Exception { OperationError };
    auto digest = PAL::CryptoDigest::create(*cryptoDigestAlgorithm);
    if (!digest)
        return Exception { OperationError };
    digest->addBytes(data.data(), data.size());
    auto digestData = digest->computeHash();

    auto status = CCRSACryptorVerify(key, ccPKCS1Padding, digestData.data(), digestData.size(), digestAlgorithm, 0, signature.data(), signature.size());
    if (!status)
        return true;
    if (status == kCCDecodeError)
        return false;

    return Exception { OperationError };
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSASSA_PKCS1_v1_5::platformSign(const CryptoKeyRSA& key, const Vector<uint8_t>& data)
{
    return signRSASSA_PKCS1_v1_5(key.hashAlgorithmIdentifier(), key.platformKey(), key.keySizeInBits(), data);
}

ExceptionOr<bool> CryptoAlgorithmRSASSA_PKCS1_v1_5::platformVerify(const CryptoKeyRSA& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    return verifyRSASSA_PKCS1_v1_5(key.hashAlgorithmIdentifier(), key.platformKey(), signature, data);
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)

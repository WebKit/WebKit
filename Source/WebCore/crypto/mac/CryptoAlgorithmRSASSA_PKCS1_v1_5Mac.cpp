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
#include "CryptoAlgorithmRsaSsaParamsDeprecated.h"
#include "CryptoDigest.h"
#include "CryptoKeyRSA.h"
#include "ExceptionCode.h"

namespace WebCore {

inline Optional<CryptoDigest::Algorithm> cryptoDigestAlgorithm(CryptoAlgorithmIdentifier hashFunction)
{
    switch (hashFunction) {
    case CryptoAlgorithmIdentifier::SHA_1:
        return CryptoDigest::Algorithm::SHA_1;
    case CryptoAlgorithmIdentifier::SHA_224:
        return CryptoDigest::Algorithm::SHA_224;
    case CryptoAlgorithmIdentifier::SHA_256:
        return CryptoDigest::Algorithm::SHA_256;
    case CryptoAlgorithmIdentifier::SHA_384:
        return CryptoDigest::Algorithm::SHA_384;
    case CryptoAlgorithmIdentifier::SHA_512:
        return CryptoDigest::Algorithm::SHA_512;
    default:
        return Nullopt;
    }
}

ExceptionOr<void> CryptoAlgorithmRSASSA_PKCS1_v1_5::platformSign(const CryptoAlgorithmRsaSsaParamsDeprecated& parameters, const CryptoKeyRSA& key, const CryptoOperationData& data, VectorCallback&& callback, VoidCallback&& failureCallback)
{
    CCDigestAlgorithm digestAlgorithm;
    if (!getCommonCryptoDigestAlgorithm(parameters.hash, digestAlgorithm))
        return Exception { NOT_SUPPORTED_ERR };

    auto cryptoDigestAlgorithm = WebCore::cryptoDigestAlgorithm(parameters.hash);
    if (!cryptoDigestAlgorithm)
        return Exception { NOT_SUPPORTED_ERR };

    auto digest = CryptoDigest::create(*cryptoDigestAlgorithm);
    if (!digest)
        return Exception { NOT_SUPPORTED_ERR };

    digest->addBytes(data.first, data.second);

    auto digestData = digest->computeHash();

    Vector<uint8_t> signature(512);
    size_t signatureSize = signature.size();

    CCCryptorStatus status = CCRSACryptorSign(key.platformKey(), ccPKCS1Padding, digestData.data(), digestData.size(), digestAlgorithm, 0, signature.data(), &signatureSize);
    if (status) {
        failureCallback();
        return { };
    }

    signature.resize(signatureSize);
    callback(signature);
    return { };
}

ExceptionOr<void> CryptoAlgorithmRSASSA_PKCS1_v1_5::platformVerify(const CryptoAlgorithmRsaSsaParamsDeprecated& parameters, const CryptoKeyRSA& key, const CryptoOperationData& signature, const CryptoOperationData& data, BoolCallback&& callback, VoidCallback&& failureCallback)
{
    CCDigestAlgorithm digestAlgorithm;
    if (!getCommonCryptoDigestAlgorithm(parameters.hash, digestAlgorithm))
        return Exception { NOT_SUPPORTED_ERR };

    auto cryptoDigestAlgorithm = WebCore::cryptoDigestAlgorithm(parameters.hash);
    if (!cryptoDigestAlgorithm)
        return Exception { NOT_SUPPORTED_ERR };

    auto digest = CryptoDigest::create(*cryptoDigestAlgorithm);
    if (!digest)
        return Exception { NOT_SUPPORTED_ERR };

    digest->addBytes(data.first, data.second);

    auto digestData = digest->computeHash();

    auto status = CCRSACryptorVerify(key.platformKey(), ccPKCS1Padding, digestData.data(), digestData.size(), digestAlgorithm, 0, signature.first, signature.second);
    if (!status)
        callback(true);
    else if (status == kCCNotVerified || status == kCCDecodeError) // <rdar://problem/15464982> CCRSACryptorVerify returns kCCDecodeError instead of kCCNotVerified sometimes
        callback(false);
    else
        failureCallback();
    return { };
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)

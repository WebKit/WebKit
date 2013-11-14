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

#include "CryptoAlgorithmRsaSsaParams.h"
#include "CryptoDigest.h"
#include "CryptoKeyRSA.h"
#include "ExceptionCode.h"
#include "JSDOMPromise.h"
#include <CommonCrypto/CommonCryptor.h>

#if defined(__has_include)
#if __has_include(<CommonCrypto/CommonRSACryptor.h>)
#include <CommonCrypto/CommonRSACryptor.h>
#endif
#endif

#ifndef _CC_RSACRYPTOR_H_
enum {
    ccPKCS1Padding = 1001
};
typedef uint32_t CCAsymmetricPadding;

enum {
    kCCDigestSHA1 = 8,
    kCCDigestSHA224 = 9,
    kCCDigestSHA256 = 10,
    kCCDigestSHA384 = 11,
    kCCDigestSHA512 = 12,
};
typedef uint32_t CCDigestAlgorithm;

enum {
    kCCNotVerified    = -4306
};
#endif

extern "C" CCCryptorStatus CCRSACryptorSign(CCRSACryptorRef privateKey, CCAsymmetricPadding padding, const void *hashToSign, size_t hashSignLen, CCDigestAlgorithm digestType, size_t saltLen, void *signedData, size_t *signedDataLen);
extern "C" CCCryptorStatus CCRSACryptorVerify(CCRSACryptorRef publicKey, CCAsymmetricPadding padding, const void *hash, size_t hashLen, CCDigestAlgorithm digestType, size_t saltLen, const void *signedData, size_t signedDataLen);

namespace WebCore {

static bool getCommonCryptoDigestAlgorithm(CryptoAlgorithmIdentifier hashFunction, CCDigestAlgorithm& algorithm)
{
    switch (hashFunction) {
    case CryptoAlgorithmIdentifier::SHA_1:
        algorithm = kCCDigestSHA1;
        return true;
    case CryptoAlgorithmIdentifier::SHA_224:
        algorithm = kCCDigestSHA224;
        return true;
    case CryptoAlgorithmIdentifier::SHA_256:
        algorithm = kCCDigestSHA256;
        return true;
    case CryptoAlgorithmIdentifier::SHA_384:
        algorithm = kCCDigestSHA384;
        return true;
    case CryptoAlgorithmIdentifier::SHA_512:
        algorithm = kCCDigestSHA512;
        return true;
    default:
        return false;
    }
}

void CryptoAlgorithmRSASSA_PKCS1_v1_5::sign(const CryptoAlgorithmParameters& parameters, const CryptoKey& key, const Vector<CryptoOperationData>& data, std::unique_ptr<PromiseWrapper> promise, ExceptionCode& ec)
{
    const CryptoAlgorithmRsaSsaParams& rsaSSAParameters = toCryptoAlgorithmRsaSsaParams(parameters);

    if (!isCryptoKeyRSA(key)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    const CryptoKeyRSA& rsaKey = toCryptoKeyRSA(key);

    CCDigestAlgorithm digestAlgorithm;
    if (!getCommonCryptoDigestAlgorithm(rsaSSAParameters.hash, digestAlgorithm)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    std::unique_ptr<CryptoDigest> digest = CryptoDigest::create(rsaSSAParameters.hash);
    if (!digest) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    for (size_t i = 0; i != data.size(); ++i)
        digest->addBytes(data[i].first, data[i].second);

    Vector<unsigned char> digestData = digest->computeHash();

    Vector<unsigned char> signature(512);
    size_t signatureSize = signature.size();

    CCCryptorStatus status = CCRSACryptorSign(rsaKey.platformKey(), ccPKCS1Padding, digestData.data(), digestData.size(), digestAlgorithm, 0, signature.data(), &signatureSize);
    if (status) {
        promise->reject(nullptr);
        return;
    }

    signature.resize(signatureSize);
    promise->fulfill(signature);
}

void CryptoAlgorithmRSASSA_PKCS1_v1_5::verify(const CryptoAlgorithmParameters& parameters, const CryptoKey& key, const CryptoOperationData& signature, const Vector<CryptoOperationData>& data, std::unique_ptr<PromiseWrapper> promise, ExceptionCode& ec)
{
    const CryptoAlgorithmRsaSsaParams& rsaSSAParameters = toCryptoAlgorithmRsaSsaParams(parameters);

    if (!isCryptoKeyRSA(key)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    const CryptoKeyRSA& rsaKey = toCryptoKeyRSA(key);

    CCDigestAlgorithm digestAlgorithm;
    if (!getCommonCryptoDigestAlgorithm(rsaSSAParameters.hash, digestAlgorithm)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    std::unique_ptr<CryptoDigest> digest = CryptoDigest::create(rsaSSAParameters.hash);
    if (!digest) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    for (size_t i = 0; i != data.size(); ++i)
        digest->addBytes(data[i].first, data[i].second);

    Vector<unsigned char> digestData = digest->computeHash();

    CCCryptorStatus status = CCRSACryptorVerify(rsaKey.platformKey(), ccPKCS1Padding, digestData.data(), digestData.size(), digestAlgorithm, 0, signature.first, signature.second);
    if (!status)
        promise->fulfill(true);
    else if (status == kCCNotVerified || kCCDecodeError) // <rdar://problem/15464982> CCRSACryptorVerify returns kCCDecodeError instead of kCCNotVerified sometimes
        promise->fulfill(false);
    else
        promise->reject(nullptr);
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)

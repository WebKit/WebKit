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
#include "CryptoAlgorithmHMAC.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithmHmacParams.h"
#include "CryptoKeyHMAC.h"
#include "ExceptionCode.h"
#include "JSDOMPromise.h"
#include <CommonCrypto/CommonHMAC.h>

namespace WebCore {

static bool getCommonCryptoAlgorithm(CryptoAlgorithmIdentifier hashFunction, CCHmacAlgorithm& algorithm)
{
    switch (hashFunction) {
    case CryptoAlgorithmIdentifier::SHA_1:
        algorithm = kCCHmacAlgSHA1;
        return true;
    case CryptoAlgorithmIdentifier::SHA_224:
        algorithm = kCCHmacAlgSHA224;
        return true;
    case CryptoAlgorithmIdentifier::SHA_256:
        algorithm = kCCHmacAlgSHA256;
        return true;
    case CryptoAlgorithmIdentifier::SHA_384:
        algorithm = kCCHmacAlgSHA384;
        return true;
    case CryptoAlgorithmIdentifier::SHA_512:
        algorithm = kCCHmacAlgSHA512;
        return true;
    default:
        return false;
    }
}

static Vector<unsigned char> calculateSignature(CCHmacAlgorithm algorithm, const Vector<char>& key, const Vector<CryptoOperationData>& data)
{
    size_t digestLength;
    switch (algorithm) {
    case kCCHmacAlgSHA1:
        digestLength = CC_SHA1_DIGEST_LENGTH;
        break;
    case kCCHmacAlgSHA224:
        digestLength = CC_SHA224_DIGEST_LENGTH;
        break;
    case kCCHmacAlgSHA256:
        digestLength = CC_SHA256_DIGEST_LENGTH;
        break;
    case kCCHmacAlgSHA384:
        digestLength = CC_SHA384_DIGEST_LENGTH;
        break;
    case kCCHmacAlgSHA512:
        digestLength = CC_SHA512_DIGEST_LENGTH;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    CCHmacContext context;
    CCHmacInit(&context, algorithm, key.data(), key.size());
    for (size_t i = 0, size = data.size(); i < size; ++i)
        CCHmacUpdate(&context, data[i].first, data[i].second);

    Vector<unsigned char> result(digestLength);
    CCHmacFinal(&context, result.data());
    return result;
}

void CryptoAlgorithmHMAC::sign(const CryptoAlgorithmParameters& parameters, const CryptoKey& key, const Vector<CryptoOperationData>& data, std::unique_ptr<PromiseWrapper> promise, ExceptionCode& ec)
{
    const CryptoAlgorithmHmacParams& hmacParameters = static_cast<const CryptoAlgorithmHmacParams&>(parameters);

    if (!isCryptoKeyHMAC(key)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    const CryptoKeyHMAC& hmacKey = toCryptoKeyHMAC(key);

    CCHmacAlgorithm algorithm;
    if (!getCommonCryptoAlgorithm(hmacParameters.hash, algorithm)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    Vector<unsigned char> signature = calculateSignature(algorithm, hmacKey.key(), data);

    promise->fulfill(signature);
}

void CryptoAlgorithmHMAC::verify(const CryptoAlgorithmParameters& parameters, const CryptoKey& key, const CryptoOperationData& expectedSignature, const Vector<CryptoOperationData>& data, std::unique_ptr<PromiseWrapper> promise, ExceptionCode& ec)
{
    const CryptoAlgorithmHmacParams& hmacParameters = static_cast<const CryptoAlgorithmHmacParams&>(parameters);

    if (!isCryptoKeyHMAC(key)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    const CryptoKeyHMAC& hmacKey = toCryptoKeyHMAC(key);

    CCHmacAlgorithm algorithm;
    if (!getCommonCryptoAlgorithm(hmacParameters.hash, algorithm)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    Vector<unsigned char> signature = calculateSignature(algorithm, hmacKey.key(), data);

    bool result = signature.size() == expectedSignature.second && !memcmp(signature.data(), expectedSignature.first, signature.size());

    promise->fulfill(result);
}

}

#endif // ENABLE(SUBTLE_CRYPTO)

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
#include <CommonCrypto/CommonHMAC.h>
#include <wtf/CryptographicUtilities.h>

namespace WebCore {

static bool getCommonCryptoHMACAlgorithm(CryptoAlgorithmIdentifier hashFunction, CCHmacAlgorithm& algorithm)
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

static Vector<uint8_t> calculateSignature(CCHmacAlgorithm algorithm, const Vector<uint8_t>& key, const CryptoOperationData& data)
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
        return Vector<uint8_t>();
    }

    Vector<uint8_t> result(digestLength);
    const void* keyData = key.data() ? key.data() : reinterpret_cast<const uint8_t*>(""); // <rdar://problem/15467425> HMAC crashes when key pointer is null.
    CCHmac(algorithm, keyData, key.size(), data.first, data.second, result.data());
    return result;
}

void CryptoAlgorithmHMAC::platformSign(const CryptoAlgorithmHmacParams& parameters, const CryptoKeyHMAC& key, const CryptoOperationData& data, VectorCallback callback, VoidCallback, ExceptionCode& ec)
{
    CCHmacAlgorithm algorithm;
    if (!getCommonCryptoHMACAlgorithm(parameters.hash, algorithm)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    Vector<uint8_t> signature = calculateSignature(algorithm, key.key(), data);

    callback(signature);
}

void CryptoAlgorithmHMAC::platformVerify(const CryptoAlgorithmHmacParams& parameters, const CryptoKeyHMAC& key, const CryptoOperationData& expectedSignature, const CryptoOperationData& data, BoolCallback callback, VoidCallback, ExceptionCode& ec)
{
    CCHmacAlgorithm algorithm;
    if (!getCommonCryptoHMACAlgorithm(parameters.hash, algorithm)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    Vector<uint8_t> signature = calculateSignature(algorithm, key.key(), data);

    // Using a constant time comparison to prevent timing attacks.
    bool result = signature.size() == expectedSignature.second && !constantTimeMemcmp(signature.data(), expectedSignature.first, signature.size());

    callback(result);
}

}

#endif // ENABLE(SUBTLE_CRYPTO)

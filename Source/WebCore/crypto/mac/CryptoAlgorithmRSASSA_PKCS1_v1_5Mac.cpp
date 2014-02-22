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
#include "CryptoAlgorithmRsaSsaParams.h"
#include "CryptoDigest.h"
#include "CryptoKeyRSA.h"
#include "ExceptionCode.h"

namespace WebCore {

void CryptoAlgorithmRSASSA_PKCS1_v1_5::platformSign(const CryptoAlgorithmRsaSsaParams& parameters, const CryptoKeyRSA& key, const CryptoOperationData& data, VectorCallback callback, VoidCallback failureCallback, ExceptionCode& ec)
{
    CCDigestAlgorithm digestAlgorithm;
    if (!getCommonCryptoDigestAlgorithm(parameters.hash, digestAlgorithm)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    std::unique_ptr<CryptoDigest> digest = CryptoDigest::create(parameters.hash);
    if (!digest) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    digest->addBytes(data.first, data.second);

    Vector<uint8_t> digestData = digest->computeHash();

    Vector<uint8_t> signature(512);
    size_t signatureSize = signature.size();

    CCCryptorStatus status = CCRSACryptorSign(key.platformKey(), ccPKCS1Padding, digestData.data(), digestData.size(), digestAlgorithm, 0, signature.data(), &signatureSize);
    if (status) {
        failureCallback();
        return;
    }

    signature.resize(signatureSize);
    callback(signature);
}

void CryptoAlgorithmRSASSA_PKCS1_v1_5::platformVerify(const CryptoAlgorithmRsaSsaParams& parameters, const CryptoKeyRSA& key, const CryptoOperationData& signature, const CryptoOperationData& data, BoolCallback callback, VoidCallback failureCallback, ExceptionCode& ec)
{
    CCDigestAlgorithm digestAlgorithm;
    if (!getCommonCryptoDigestAlgorithm(parameters.hash, digestAlgorithm)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    std::unique_ptr<CryptoDigest> digest = CryptoDigest::create(parameters.hash);
    if (!digest) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    digest->addBytes(data.first, data.second);

    Vector<uint8_t> digestData = digest->computeHash();

    CCCryptorStatus status = CCRSACryptorVerify(key.platformKey(), ccPKCS1Padding, digestData.data(), digestData.size(), digestAlgorithm, 0, signature.first, signature.second);
    if (!status)
        callback(true);
    else if (status == kCCNotVerified || status == kCCDecodeError) // <rdar://problem/15464982> CCRSACryptorVerify returns kCCDecodeError instead of kCCNotVerified sometimes
        callback(false);
    else
        failureCallback();
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)

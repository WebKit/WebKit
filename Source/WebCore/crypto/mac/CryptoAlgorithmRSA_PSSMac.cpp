/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "CryptoAlgorithmRSA_PSS.h"

#if ENABLE(WEB_CRYPTO) && HAVE(RSA_PSS)

#include "CommonCryptoUtilities.h"
#include "CryptoAlgorithmRsaPssParams.h"
#include "CryptoDigestAlgorithm.h"
#include "CryptoKeyRSA.h"

namespace WebCore {

static ExceptionOr<Vector<uint8_t>> signRSA_PSS(CryptoAlgorithmIdentifier hash, const PlatformRSAKey key, size_t keyLength, const Vector<uint8_t>& data, size_t saltLength)
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

    Vector<uint8_t> signature(keyLength / 8); // Per https://tools.ietf.org/html/rfc3447#section-8.1.1
    size_t signatureSize = signature.size();

    CCCryptorStatus status = CCRSACryptorSign(key, ccRSAPSSPadding, digestData.data(), digestData.size(), digestAlgorithm, saltLength, signature.data(), &signatureSize);
    if (status)
        return Exception { OperationError };

    return WTFMove(signature);
}

static ExceptionOr<bool> verifyRSA_PSS(CryptoAlgorithmIdentifier hash, const PlatformRSAKey key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data, size_t saltLength)
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

    auto status = CCRSACryptorVerify(key, ccRSAPSSPadding, digestData.data(), digestData.size(), digestAlgorithm, saltLength, signature.data(), signature.size());
    if (!status)
        return true;
    return false;
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSA_PSS::platformSign(const CryptoAlgorithmRsaPssParams& parameters, const CryptoKeyRSA& key, const Vector<uint8_t>& data)
{
    return signRSA_PSS(key.hashAlgorithmIdentifier(), key.platformKey(), key.keySizeInBits(), data, parameters.saltLength);
}

ExceptionOr<bool> CryptoAlgorithmRSA_PSS::platformVerify(const CryptoAlgorithmRsaPssParams& parameters, const CryptoKeyRSA& key, const Vector<uint8_t>& signature, const Vector<uint8_t>& data)
{
    return verifyRSA_PSS(key.hashAlgorithmIdentifier(), key.platformKey(), signature, data, parameters.saltLength);
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO) && HAVE(RSA_PSS)

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
#include "CryptoAlgorithmRSA_OAEP.h"

#if ENABLE(WEB_CRYPTO)

#include "CommonCryptoUtilities.h"
#include "CryptoAlgorithmRsaOaepParams.h"
#include "CryptoKeyRSA.h"

namespace WebCore {

static ExceptionOr<Vector<uint8_t>> encryptRSA_OAEP(CryptoAlgorithmIdentifier hash, const Vector<uint8_t>& label, const PlatformRSAKey key, size_t keyLength, const Vector<uint8_t>& data)
{
    CCDigestAlgorithm digestAlgorithm;
    if (!getCommonCryptoDigestAlgorithm(hash, digestAlgorithm))
        return Exception { OperationError };

    Vector<uint8_t> cipherText(keyLength / 8); // Per Step 3.c of https://tools.ietf.org/html/rfc3447#section-7.1.1
    size_t cipherTextLength = cipherText.size();
    if (CCRSACryptorEncrypt(key, ccOAEPPadding, data.data(), data.size(), cipherText.data(), &cipherTextLength, label.data(), label.size(), digestAlgorithm))
        return Exception { OperationError };

    return WTFMove(cipherText);
}

static ExceptionOr<Vector<uint8_t>> decryptRSA_OAEP(CryptoAlgorithmIdentifier hash, const Vector<uint8_t>& label, const PlatformRSAKey key, size_t keyLength, const Vector<uint8_t>& data)
{
    CCDigestAlgorithm digestAlgorithm;
    if (!getCommonCryptoDigestAlgorithm(hash, digestAlgorithm))
        return Exception { OperationError };

    Vector<uint8_t> plainText(keyLength / 8); // Per Step 1.b of https://tools.ietf.org/html/rfc3447#section-7.1.1
    size_t plainTextLength = plainText.size();
    if (CCRSACryptorDecrypt(key, ccOAEPPadding, data.data(), data.size(), plainText.data(), &plainTextLength, label.data(), label.size(), digestAlgorithm))
        return Exception { OperationError };

    plainText.resize(plainTextLength);
    return WTFMove(plainText);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSA_OAEP::platformEncrypt(const CryptoAlgorithmRsaOaepParams& parameters, const CryptoKeyRSA& key, const Vector<uint8_t>& plainText)
{
    return encryptRSA_OAEP(key.hashAlgorithmIdentifier(), parameters.labelVector(), key.platformKey(), key.keySizeInBits(), plainText);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSA_OAEP::platformDecrypt(const CryptoAlgorithmRsaOaepParams& parameters, const CryptoKeyRSA& key, const Vector<uint8_t>& cipherText)
{
    return decryptRSA_OAEP(key.hashAlgorithmIdentifier(), parameters.labelVector(), key.platformKey(), key.keySizeInBits(), cipherText);
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)

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
#include "CryptoAlgorithmRSAES_PKCS1_v1_5.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CommonCryptoUtilities.h"
#include "CryptoKeyRSA.h"

namespace WebCore {

void CryptoAlgorithmRSAES_PKCS1_v1_5::platformEncrypt(const CryptoKeyRSA& key, const CryptoOperationData& data, VectorCallback callback, VoidCallback failureCallback, ExceptionCode&)
{
    Vector<uint8_t> cipherText(1024);
    size_t cipherTextLength = cipherText.size();
    CCCryptorStatus status = CCRSACryptorEncrypt(key.platformKey(), ccPKCS1Padding, data.first, data.second, cipherText.data(), &cipherTextLength, 0, 0, kCCDigestNone);
    if (status) {
        failureCallback();
        return;
    }

    cipherText.resize(cipherTextLength);
    callback(cipherText);
}

void CryptoAlgorithmRSAES_PKCS1_v1_5::platformDecrypt(const CryptoKeyRSA& key, const CryptoOperationData& data, VectorCallback callback, VoidCallback failureCallback, ExceptionCode&)
{
    Vector<uint8_t> plainText(1024);
    size_t plainTextLength = plainText.size();
    CCCryptorStatus status = CCRSACryptorDecrypt(key.platformKey(), ccPKCS1Padding, data.first, data.second, plainText.data(), &plainTextLength, 0, 0, kCCDigestNone);
    if (status) {
        failureCallback();
        return;
    }

    plainText.resize(plainTextLength);
    callback(plainText);
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)

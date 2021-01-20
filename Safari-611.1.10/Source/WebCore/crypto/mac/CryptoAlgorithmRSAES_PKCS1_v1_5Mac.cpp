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

#if ENABLE(WEB_CRYPTO)

#include "CommonCryptoUtilities.h"
#include "CryptoKeyRSA.h"

namespace WebCore {

static ExceptionOr<Vector<uint8_t>> encryptRSAES_PKCS1_v1_5(const PlatformRSAKey key, size_t keyLength, const Vector<uint8_t>& data)
{
    Vector<uint8_t> cipherText(keyLength / 8); // Per Step 3.c of https://tools.ietf.org/html/rfc3447#section-7.2.1
    size_t cipherTextLength = cipherText.size();
    if (CCRSACryptorEncrypt(key, ccPKCS1Padding, data.data(), data.size(), cipherText.data(), &cipherTextLength, 0, 0, kCCDigestNone))
        return Exception { OperationError };

    return WTFMove(cipherText);
}

static ExceptionOr<Vector<uint8_t>> decryptRSAES_PKCS1_v1_5(const PlatformRSAKey key, size_t keyLength, const Vector<uint8_t>& data)
{
    Vector<uint8_t> plainText(keyLength / 8); // Per Step 1 of https://tools.ietf.org/html/rfc3447#section-7.2.1
    size_t plainTextLength = plainText.size();
    if (CCRSACryptorDecrypt(key, ccPKCS1Padding, data.data(), data.size(), plainText.data(), &plainTextLength, 0, 0, kCCDigestNone))
        return Exception { OperationError };

    plainText.resize(plainTextLength);
    return WTFMove(plainText);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSAES_PKCS1_v1_5::platformEncrypt(const CryptoKeyRSA& key, const Vector<uint8_t>& plainText)
{
    return encryptRSAES_PKCS1_v1_5(key.platformKey(), key.keySizeInBits(), plainText);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmRSAES_PKCS1_v1_5::platformDecrypt(const CryptoKeyRSA& key, const Vector<uint8_t>& cipherText)
{
    return decryptRSAES_PKCS1_v1_5(key.platformKey(), key.keySizeInBits(), cipherText);
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)

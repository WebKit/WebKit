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
#include "CryptoAlgorithmAES_CBC.h"

#if ENABLE(WEB_CRYPTO)

#include "CryptoAlgorithmAesCbcCfbParams.h"
#include "CryptoKeyAES.h"
#include <CommonCrypto/CommonCrypto.h>

namespace WebCore {

static ExceptionOr<Vector<uint8_t>> transformAES_CBC(CCOperation operation, const Vector<uint8_t>& iv, const Vector<uint8_t>& key, const Vector<uint8_t>& data, CryptoAlgorithmAES_CBC::Padding padding)
{
    CCOptions options = padding == CryptoAlgorithmAES_CBC::Padding::Yes ? kCCOptionPKCS7Padding : 0;
    CCCryptorRef cryptor;
    CCCryptorStatus status = CCCryptorCreate(operation, kCCAlgorithmAES, options, key.data(), key.size(), iv.data(), &cryptor);
    if (status)
        return Exception { OperationError };

    Vector<uint8_t> result(CCCryptorGetOutputLength(cryptor, data.size(), true));

    size_t bytesWritten;
    status = CCCryptorUpdate(cryptor, data.data(), data.size(), result.data(), result.size(), &bytesWritten);
    if (status)
        return Exception { OperationError };

    uint8_t* p = result.data() + bytesWritten;
    if (padding == CryptoAlgorithmAES_CBC::Padding::Yes) {
        status = CCCryptorFinal(cryptor, p, result.end() - p, &bytesWritten);
        p += bytesWritten;
        if (status)
            return Exception { OperationError };
    }

    ASSERT(p <= result.end());
    result.shrink(p - result.begin());

    CCCryptorRelease(cryptor);

    return WTFMove(result);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAES_CBC::platformEncrypt(const CryptoAlgorithmAesCbcCfbParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& plainText, Padding padding)
{
    ASSERT(parameters.ivVector().size() == kCCBlockSizeAES128 || parameters.ivVector().isEmpty());
    ASSERT(padding == Padding::Yes || !(plainText.size() % kCCBlockSizeAES128));
    return transformAES_CBC(kCCEncrypt, parameters.ivVector(), key.key(), plainText, padding);
}

ExceptionOr<Vector<uint8_t>> CryptoAlgorithmAES_CBC::platformDecrypt(const CryptoAlgorithmAesCbcCfbParams& parameters, const CryptoKeyAES& key, const Vector<uint8_t>& cipherText, Padding padding)
{
    ASSERT(parameters.ivVector().size() == kCCBlockSizeAES128 || parameters.ivVector().isEmpty());
    ASSERT(padding == Padding::Yes || !(cipherText.size() % kCCBlockSizeAES128));
    auto result = transformAES_CBC(kCCDecrypt, parameters.ivVector(), key.key(), cipherText, Padding::No);
    if (result.hasException())
        return result.releaseException();

    auto data = result.releaseReturnValue();
    if (padding == Padding::Yes && !data.isEmpty()) {
        // Validate and remove padding as per https://www.w3.org/TR/WebCryptoAPI/#aes-cbc-operations (Decrypt section).
        auto paddingByte = data.last();
        if (!paddingByte || paddingByte > 16 || paddingByte > data.size())
            return Exception { OperationError };
        for (size_t i = data.size() - paddingByte; i < data.size() - 1; ++i) {
            if (data[i] != paddingByte)
                return Exception { OperationError };
        }
        data.shrink(data.size() - paddingByte);
    }
    return data;
}

} // namespace WebCore

#endif // ENABLE(WEB_CRYPTO)

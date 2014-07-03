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

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithmAesCbcParams.h"
#include "CryptoKeyAES.h"
#include "ExceptionCode.h"
#include <CommonCrypto/CommonCrypto.h>

namespace WebCore {

static void transformAES_CBC(CCOperation operation, const CryptoAlgorithmAesCbcParams& parameters, const CryptoKeyAES& key, const CryptoOperationData& data, CryptoAlgorithm::VectorCallback callback, CryptoAlgorithm::VoidCallback failureCallback)
{
    static_assert(sizeof(parameters.iv) == kCCBlockSizeAES128, "Initialization vector size must be the same as algorithm block size");

    size_t keyLengthInBytes = key.key().size();
    if (keyLengthInBytes != 16 && keyLengthInBytes != 24 && keyLengthInBytes != 32) {
        failureCallback();
        return;
    }

    CCCryptorRef cryptor;
#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
    CCAlgorithm aesAlgorithm = kCCAlgorithmAES;
#else
    CCAlgorithm aesAlgorithm = kCCAlgorithmAES128;
#endif
    CCCryptorStatus status = CCCryptorCreate(operation, aesAlgorithm, kCCOptionPKCS7Padding, key.key().data(), keyLengthInBytes, parameters.iv.data(), &cryptor);
    if (status) {
        failureCallback();
        return;
    }

    Vector<uint8_t> result(CCCryptorGetOutputLength(cryptor, data.second, true));

    size_t bytesWritten;
    status = CCCryptorUpdate(cryptor, data.first, data.second, result.data(), result.size(), &bytesWritten);
    if (status) {
        failureCallback();
        return;
    }

    uint8_t* p = result.data() + bytesWritten;
    status = CCCryptorFinal(cryptor, p, result.end() - p, &bytesWritten);
    p += bytesWritten;
    if (status) {
        failureCallback();
        return;
    }

    ASSERT(p <= result.end());
    result.shrink(p - result.begin());

    CCCryptorRelease(cryptor);

    callback(result);
}

void CryptoAlgorithmAES_CBC::platformEncrypt(const CryptoAlgorithmAesCbcParams& parameters, const CryptoKeyAES& key, const CryptoOperationData& data, VectorCallback callback, VoidCallback failureCallback, ExceptionCode&)
{
    transformAES_CBC(kCCEncrypt, parameters, key, data, WTF::move(callback), WTF::move(failureCallback));
}

void CryptoAlgorithmAES_CBC::platformDecrypt(const CryptoAlgorithmAesCbcParams& parameters, const CryptoKeyAES& key, const CryptoOperationData& data, VectorCallback callback, VoidCallback failureCallback, ExceptionCode&)
{
    transformAES_CBC(kCCDecrypt, parameters, key, data, WTF::move(callback), WTF::move(failureCallback));
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)

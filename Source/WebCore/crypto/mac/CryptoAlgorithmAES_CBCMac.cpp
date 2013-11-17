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
#include "JSDOMPromise.h"
#include <CommonCrypto/CommonCrypto.h>

namespace WebCore {

static void transformAES_CBC(CCOperation operation, const CryptoAlgorithmAesCbcParams& parameters, const CryptoKeyAES& key, const CryptoOperationData& data, std::unique_ptr<PromiseWrapper> promise)
{
    static_assert(sizeof(parameters.iv) == kCCBlockSizeAES128, "Initialization vector size must be the same as algorithm block size");

    size_t keyLengthInBytes = key.key().size();
    if (keyLengthInBytes != 16 && keyLengthInBytes != 24 && keyLengthInBytes != 32) {
        promise->reject(nullptr);
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
        promise->reject(nullptr);
        return;
    }

    Vector<uint8_t> result(CCCryptorGetOutputLength(cryptor, data.second, true));

    size_t bytesWritten;
    status = CCCryptorUpdate(cryptor, data.first, data.second, result.data(), result.size(), &bytesWritten);
    if (status) {
        promise->reject(nullptr);
        return;
    }

    uint8_t* p = result.data() + bytesWritten;
    status = CCCryptorFinal(cryptor, p, result.end() - p, &bytesWritten);
    p += bytesWritten;
    if (status) {
        promise->reject(nullptr);
        return;
    }

    ASSERT(p <= result.end());
    result.shrink(p - result.begin());

    CCCryptorRelease(cryptor);

    promise->fulfill(result);
}

void CryptoAlgorithmAES_CBC::encrypt(const CryptoAlgorithmParameters& parameters, const CryptoKey& key, const CryptoOperationData& data, std::unique_ptr<PromiseWrapper> promise, ExceptionCode& ec)
{
    const CryptoAlgorithmAesCbcParams& aesCBCParameters = toCryptoAlgorithmAesCbcParams(parameters);

    if (!isCryptoKeyAES(key)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    const CryptoKeyAES& aesKey = toCryptoKeyAES(key);

    transformAES_CBC(kCCEncrypt, aesCBCParameters, aesKey, data, std::move(promise));
}

void CryptoAlgorithmAES_CBC::decrypt(const CryptoAlgorithmParameters& parameters, const CryptoKey& key, const CryptoOperationData& data, std::unique_ptr<PromiseWrapper> promise, ExceptionCode& ec)
{
    const CryptoAlgorithmAesCbcParams& aesCBCParameters = toCryptoAlgorithmAesCbcParams(parameters);

    if (!isCryptoKeyAES(key)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    const CryptoKeyAES& aesKey = toCryptoKeyAES(key);

    transformAES_CBC(kCCDecrypt, aesCBCParameters, aesKey, data, std::move(promise));
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)

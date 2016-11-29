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
#include "CryptoAlgorithmAesCbcParamsDeprecated.h"
#include "CryptoKeyAES.h"
#include "ExceptionCode.h"
#include "ScriptExecutionContext.h"
#include <CommonCrypto/CommonCrypto.h>

namespace WebCore {

// FIXME: We should change iv and data to Vector<uint8_t> type once WebKitSubtleCrypto is deprecated.
// https://bugs.webkit.org/show_bug.cgi?id=164939
static ExceptionOr<Vector<uint8_t>> transformAES_CBC(CCOperation operation, const uint8_t* iv, const Vector<uint8_t>& key, const uint8_t* data, size_t dataLength)
{
    size_t keyLengthInBytes = key.size();
    CCCryptorRef cryptor;
#if PLATFORM(COCOA)
    CCAlgorithm aesAlgorithm = kCCAlgorithmAES;
#else
    CCAlgorithm aesAlgorithm = kCCAlgorithmAES128;
#endif
    CCCryptorStatus status = CCCryptorCreate(operation, aesAlgorithm, kCCOptionPKCS7Padding, key.data(), keyLengthInBytes, iv, &cryptor);
    if (status)
        return Exception { OperationError };

    Vector<uint8_t> result(CCCryptorGetOutputLength(cryptor, dataLength, true));

    size_t bytesWritten;
    status = CCCryptorUpdate(cryptor, data, dataLength, result.data(), result.size(), &bytesWritten);
    if (status)
        return Exception { OperationError };

    uint8_t* p = result.data() + bytesWritten;
    status = CCCryptorFinal(cryptor, p, result.end() - p, &bytesWritten);
    p += bytesWritten;
    if (status)
        return Exception { OperationError };

    ASSERT(p <= result.end());
    result.shrink(p - result.begin());

    CCCryptorRelease(cryptor);

    return WTFMove(result);
}

void CryptoAlgorithmAES_CBC::platformEncrypt(std::unique_ptr<CryptoAlgorithmParameters>&& parameters, Ref<CryptoKey>&& key, Vector<uint8_t>&& plainText, VectorCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    context.ref();
    workQueue.dispatch([parameters = WTFMove(parameters), key = WTFMove(key), plainText = WTFMove(plainText), callback = WTFMove(callback), exceptionCallback = WTFMove(exceptionCallback), &context]() mutable {
        auto& aesParameters = downcast<CryptoAlgorithmAesCbcParams>(*parameters);
        auto& aesKey = downcast<CryptoKeyAES>(key.get());
        ASSERT(aesParameters.ivVector().size() == kCCBlockSizeAES128);
        auto result = transformAES_CBC(kCCEncrypt, aesParameters.ivVector().data(), aesKey.key(), plainText.data(), plainText.size());
        if (result.hasException()) {
            // We should only dereference callbacks after being back to the Document/Worker threads.
            context.postTask([exceptionCallback = WTFMove(exceptionCallback), ec = result.releaseException().code(), callback = WTFMove(callback)](ScriptExecutionContext& context) {
                exceptionCallback(ec);
                context.deref();
            });
            return;
        }
        // We should only dereference callbacks after being back to the Document/Worker threads.
        context.postTask([callback = WTFMove(callback), result = result.releaseReturnValue(), exceptionCallback = WTFMove(exceptionCallback)](ScriptExecutionContext& context) {
            callback(result);
            context.deref();
        });
    });
}

void CryptoAlgorithmAES_CBC::platformDecrypt(std::unique_ptr<CryptoAlgorithmParameters>&& parameters, Ref<CryptoKey>&& key, Vector<uint8_t>&& cipherText, VectorCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    context.ref();
    workQueue.dispatch([parameters = WTFMove(parameters), key = WTFMove(key), cipherText = WTFMove(cipherText), callback = WTFMove(callback), exceptionCallback = WTFMove(exceptionCallback), &context]() mutable {
        auto& aesParameters = downcast<CryptoAlgorithmAesCbcParams>(*parameters);
        auto& aesKey = downcast<CryptoKeyAES>(key.get());
        assert(aesParameters.ivVector().size() == kCCBlockSizeAES128);
        auto result = transformAES_CBC(kCCDecrypt, aesParameters.ivVector().data(), aesKey.key(), cipherText.data(), cipherText.size());
        if (result.hasException()) {
            // We should only dereference callbacks after being back to the Document/Worker threads.
            context.postTask([exceptionCallback = WTFMove(exceptionCallback), ec = result.releaseException().code(), callback = WTFMove(callback)](ScriptExecutionContext& context) {
                exceptionCallback(ec);
                context.deref();
            });
            return;
        }
        // We should only dereference callbacks after being back to the Document/Worker threads.
        context.postTask([callback = WTFMove(callback), result = result.releaseReturnValue(), exceptionCallback = WTFMove(exceptionCallback)](ScriptExecutionContext& context) {
            callback(result);
            context.deref();
        });
    });
}

ExceptionOr<void> CryptoAlgorithmAES_CBC::platformEncrypt(const CryptoAlgorithmAesCbcParamsDeprecated& parameters, const CryptoKeyAES& key, const CryptoOperationData& data, VectorCallback&& callback, VoidCallback&& failureCallback)
{
    ASSERT(sizeof(parameters.iv) == kCCBlockSizeAES128);
    auto result = transformAES_CBC(kCCEncrypt, parameters.iv.data(), key.key(), data.first, data.second);
    if (result.hasException()) {
        failureCallback();
        return { };
    }
    callback(result.releaseReturnValue());
    return { };
}

ExceptionOr<void> CryptoAlgorithmAES_CBC::platformDecrypt(const CryptoAlgorithmAesCbcParamsDeprecated& parameters, const CryptoKeyAES& key, const CryptoOperationData& data, VectorCallback&& callback, VoidCallback&& failureCallback)
{
    ASSERT(sizeof(parameters.iv) == kCCBlockSizeAES128);
    auto result = transformAES_CBC(kCCDecrypt, parameters.iv.data(), key.key(), data.first, data.second);
    if (result.hasException()) {
        failureCallback();
        return { };
    }
    callback(result.releaseReturnValue());
    return { };
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)

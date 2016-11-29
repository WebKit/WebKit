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

#if ENABLE(SUBTLE_CRYPTO)

#include "CommonCryptoUtilities.h"
#include "CryptoAlgorithmRsaOaepParams.h"
#include "CryptoAlgorithmRsaOaepParamsDeprecated.h"
#include "CryptoKeyRSA.h"
#include "ExceptionCode.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

// FIXME: We should change data to Vector<uint8_t> type once WebKitSubtleCrypto is deprecated.
// https://bugs.webkit.org/show_bug.cgi?id=164939
static ExceptionOr<Vector<uint8_t>> encryptRSA_OAEP(CryptoAlgorithmIdentifier hash, const Vector<uint8_t>& label, const PlatformRSAKey key, size_t keyLength, const uint8_t* data, size_t dataLength)
{
    CCDigestAlgorithm digestAlgorithm;
    if (!getCommonCryptoDigestAlgorithm(hash, digestAlgorithm))
        return Exception { OperationError };

    Vector<uint8_t> cipherText(keyLength / 8); // Per Step 3.c of https://tools.ietf.org/html/rfc3447#section-7.1.1
    size_t cipherTextLength = cipherText.size();
    if (CCRSACryptorEncrypt(key, ccOAEPPadding, data, dataLength, cipherText.data(), &cipherTextLength, label.data(), label.size(), digestAlgorithm))
        return Exception { OperationError };

    return WTFMove(cipherText);
}

// FIXME: We should change data to Vector<uint8_t> type once WebKitSubtleCrypto is deprecated.
// https://bugs.webkit.org/show_bug.cgi?id=164939
static ExceptionOr<Vector<uint8_t>> decryptRSA_OAEP(CryptoAlgorithmIdentifier hash, const Vector<uint8_t>& label, const PlatformRSAKey key, size_t keyLength, const uint8_t* data, size_t dataLength)
{
    CCDigestAlgorithm digestAlgorithm;
    if (!getCommonCryptoDigestAlgorithm(hash, digestAlgorithm))
        return Exception { OperationError };

    Vector<uint8_t> plainText(keyLength / 8); // Per Step 1.b of https://tools.ietf.org/html/rfc3447#section-7.1.1
    size_t plainTextLength = plainText.size();
    if (CCRSACryptorDecrypt(key, ccOAEPPadding, data, dataLength, plainText.data(), &plainTextLength, label.data(), label.size(), digestAlgorithm))
        return Exception { OperationError };

    plainText.resize(plainTextLength);
    return WTFMove(plainText);
}

void CryptoAlgorithmRSA_OAEP::platformEncrypt(std::unique_ptr<CryptoAlgorithmParameters>&& parameters, Ref<CryptoKey>&& key, Vector<uint8_t>&& plainText, VectorCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    context.ref();
    workQueue.dispatch([parameters = WTFMove(parameters), key = WTFMove(key), plainText = WTFMove(plainText), callback = WTFMove(callback), exceptionCallback = WTFMove(exceptionCallback), &context]() mutable {
        auto& rsaParameters = downcast<CryptoAlgorithmRsaOaepParams>(*parameters);
        auto& rsaKey = downcast<CryptoKeyRSA>(key.get());
        auto result = encryptRSA_OAEP(rsaKey.hashAlgorithmIdentifier(), rsaParameters.labelVector(), rsaKey.platformKey(), rsaKey.keySizeInBits(), plainText.data(), plainText.size());
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

void CryptoAlgorithmRSA_OAEP::platformDecrypt(std::unique_ptr<CryptoAlgorithmParameters>&& parameters, Ref<CryptoKey>&& key, Vector<uint8_t>&& cipherText, VectorCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    context.ref();
    workQueue.dispatch([parameters = WTFMove(parameters), key = WTFMove(key), cipherText = WTFMove(cipherText), callback = WTFMove(callback), exceptionCallback = WTFMove(exceptionCallback), &context]() mutable {
        auto& rsaParameters = downcast<CryptoAlgorithmRsaOaepParams>(*parameters);
        auto& rsaKey = downcast<CryptoKeyRSA>(key.get());
        auto result = decryptRSA_OAEP(rsaKey.hashAlgorithmIdentifier(), rsaParameters.labelVector(), rsaKey.platformKey(), rsaKey.keySizeInBits(), cipherText.data(), cipherText.size());
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

ExceptionOr<void> CryptoAlgorithmRSA_OAEP::platformEncrypt(const CryptoAlgorithmRsaOaepParamsDeprecated& parameters, const CryptoKeyRSA& key, const CryptoOperationData& data, VectorCallback&& callback, VoidCallback&& failureCallback)
{
    ASSERT(parameters.hasLabel || parameters.label.isEmpty());
    auto result = encryptRSA_OAEP(parameters.hash, parameters.label, key.platformKey(), key.keySizeInBits(), data.first, data.second);
    if (result.hasException()) {
        failureCallback();
        return { };
    }
    callback(result.releaseReturnValue());
    return { };
}

ExceptionOr<void> CryptoAlgorithmRSA_OAEP::platformDecrypt(const CryptoAlgorithmRsaOaepParamsDeprecated& parameters, const CryptoKeyRSA& key, const CryptoOperationData& data, VectorCallback&& callback, VoidCallback&& failureCallback)
{
    ASSERT(parameters.hasLabel || parameters.label.isEmpty());
    auto result = decryptRSA_OAEP(parameters.hash, parameters.label, key.platformKey(), key.keySizeInBits(), data.first, data.second);
    if (result.hasException()) {
        failureCallback();
        return { };
    }
    callback(result.releaseReturnValue());
    return { };
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)

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
#include "CryptoAlgorithmRsaSsaParamsDeprecated.h"
#include "CryptoDigest.h"
#include "CryptoKeyRSA.h"
#include "ExceptionCode.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

inline std::optional<CryptoDigest::Algorithm> cryptoDigestAlgorithm(CryptoAlgorithmIdentifier hashFunction)
{
    switch (hashFunction) {
    case CryptoAlgorithmIdentifier::SHA_1:
        return CryptoDigest::Algorithm::SHA_1;
    case CryptoAlgorithmIdentifier::SHA_224:
        return CryptoDigest::Algorithm::SHA_224;
    case CryptoAlgorithmIdentifier::SHA_256:
        return CryptoDigest::Algorithm::SHA_256;
    case CryptoAlgorithmIdentifier::SHA_384:
        return CryptoDigest::Algorithm::SHA_384;
    case CryptoAlgorithmIdentifier::SHA_512:
        return CryptoDigest::Algorithm::SHA_512;
    default:
        return std::nullopt;
    }
}

// FIXME: We should change data to Vector<uint8_t> type once WebKitSubtleCrypto is deprecated.
// https://bugs.webkit.org/show_bug.cgi?id=164939
static ExceptionOr<Vector<uint8_t>> signRSASSA_PKCS1_v1_5(CryptoAlgorithmIdentifier hash, const PlatformRSAKey key, size_t keyLength, const uint8_t* data, size_t dataLength)
{
    CCDigestAlgorithm digestAlgorithm;
    if (!getCommonCryptoDigestAlgorithm(hash, digestAlgorithm))
        return Exception { OperationError };

    auto cryptoDigestAlgorithm = WebCore::cryptoDigestAlgorithm(hash);
    if (!cryptoDigestAlgorithm)
        return Exception { OperationError };
    auto digest = CryptoDigest::create(*cryptoDigestAlgorithm);
    if (!digest)
        return Exception { OperationError };
    digest->addBytes(data, dataLength);
    auto digestData = digest->computeHash();

    Vector<uint8_t> signature(keyLength / 8); // Per https://tools.ietf.org/html/rfc3447#section-8.2.1
    size_t signatureSize = signature.size();

    CCCryptorStatus status = CCRSACryptorSign(key, ccPKCS1Padding, digestData.data(), digestData.size(), digestAlgorithm, 0, signature.data(), &signatureSize);
    if (status)
        return Exception { OperationError };

    return WTFMove(signature);
}

// FIXME: We should change signature, data to Vector<uint8_t> type once WebKitSubtleCrypto is deprecated.
// https://bugs.webkit.org/show_bug.cgi?id=164939
static ExceptionOr<bool> verifyRSASSA_PKCS1_v1_5(CryptoAlgorithmIdentifier hash, const PlatformRSAKey key, const uint8_t* signature, size_t signatureLength, const uint8_t* data, size_t dataLength)
{
    CCDigestAlgorithm digestAlgorithm;
    if (!getCommonCryptoDigestAlgorithm(hash, digestAlgorithm))
        return Exception { OperationError };

    auto cryptoDigestAlgorithm = WebCore::cryptoDigestAlgorithm(hash);
    if (!cryptoDigestAlgorithm)
        return Exception { OperationError };
    auto digest = CryptoDigest::create(*cryptoDigestAlgorithm);
    if (!digest)
        return Exception { OperationError };
    digest->addBytes(data, dataLength);
    auto digestData = digest->computeHash();

    auto status = CCRSACryptorVerify(key, ccPKCS1Padding, digestData.data(), digestData.size(), digestAlgorithm, 0, signature, signatureLength);
    if (!status)
        return true;
    if (status == kCCNotVerified || status == kCCDecodeError) // <rdar://problem/15464982> CCRSACryptorVerify returns kCCDecodeError instead of kCCNotVerified sometimes
        return false;

    return Exception { OperationError };
}

void CryptoAlgorithmRSASSA_PKCS1_v1_5::platformSign(Ref<CryptoKey>&& key, Vector<uint8_t>&& data, VectorCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    context.ref();
    workQueue.dispatch([key = WTFMove(key), data = WTFMove(data), callback = WTFMove(callback), exceptionCallback = WTFMove(exceptionCallback), &context]() mutable {
        auto& rsaKey = downcast<CryptoKeyRSA>(key.get());
        auto result = signRSASSA_PKCS1_v1_5(rsaKey.hashAlgorithmIdentifier(), rsaKey.platformKey(), rsaKey.keySizeInBits(), data.data(), data.size());
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

void CryptoAlgorithmRSASSA_PKCS1_v1_5::platformVerify(Ref<CryptoKey>&& key, Vector<uint8_t>&& signature, Vector<uint8_t>&& data, BoolCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    context.ref();
    workQueue.dispatch([key = WTFMove(key), signature = WTFMove(signature), data = WTFMove(data), callback = WTFMove(callback), exceptionCallback = WTFMove(exceptionCallback), &context]() mutable {
        auto& rsaKey = downcast<CryptoKeyRSA>(key.get());
        auto result = verifyRSASSA_PKCS1_v1_5(rsaKey.hashAlgorithmIdentifier(), rsaKey.platformKey(), signature.data(), signature.size(), data.data(), data.size());
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

ExceptionOr<void> CryptoAlgorithmRSASSA_PKCS1_v1_5::platformSign(const CryptoAlgorithmRsaSsaParamsDeprecated& parameters, const CryptoKeyRSA& key, const CryptoOperationData& data, VectorCallback&& callback, VoidCallback&& failureCallback)
{
    auto result = signRSASSA_PKCS1_v1_5(parameters.hash, key.platformKey(), key.keySizeInBits(), data.first, data.second);
    if (result.hasException()) {
        failureCallback();
        return { };
    }
    callback(result.releaseReturnValue());
    return { };
}

ExceptionOr<void> CryptoAlgorithmRSASSA_PKCS1_v1_5::platformVerify(const CryptoAlgorithmRsaSsaParamsDeprecated& parameters, const CryptoKeyRSA& key, const CryptoOperationData& signature, const CryptoOperationData& data, BoolCallback&& callback, VoidCallback&& failureCallback)
{
    auto result = verifyRSASSA_PKCS1_v1_5(parameters.hash, key.platformKey(), signature.first, signature.second, data.first, data.second);
    if (result.hasException()) {
        failureCallback();
        return { };
    }
    callback(result.releaseReturnValue());
    return { };
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)

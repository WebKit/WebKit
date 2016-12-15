/*
 * Copyright (C) 2014 Igalia S.L.
 * Copyright (C) 2016 SoftAtHome
 * Copyright (C) 2016 Apple Inc.
 * Copyright (C) 2016 Yusuke Suzuki <utatane.tea@gmail.com>.
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
#include "CryptoAlgorithmHMAC.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithmHmacParamsDeprecated.h"
#include "CryptoKeyHMAC.h"
#include "ExceptionCode.h"
#include "ScriptExecutionContext.h"
#include <gcrypt.h>
#include <wtf/CryptographicUtilities.h>

namespace WebCore {

static int getGCryptDigestAlgorithm(CryptoAlgorithmIdentifier hashFunction)
{
    switch (hashFunction) {
    case CryptoAlgorithmIdentifier::SHA_1:
        return GCRY_MAC_HMAC_SHA1;
    case CryptoAlgorithmIdentifier::SHA_224:
        return GCRY_MAC_HMAC_SHA224;
    case CryptoAlgorithmIdentifier::SHA_256:
        return GCRY_MAC_HMAC_SHA256;
    case CryptoAlgorithmIdentifier::SHA_384:
        return GCRY_MAC_HMAC_SHA384;
    case CryptoAlgorithmIdentifier::SHA_512:
        return GCRY_MAC_HMAC_SHA512;
    default:
        return GCRY_MAC_NONE;
    }
}

static std::optional<Vector<uint8_t>> calculateSignature(int algorithm, const Vector<uint8_t>& key, const uint8_t* data, size_t dataLength)
{
    size_t digestLength = gcry_mac_get_algo_maclen(algorithm);
    const void* keyData = key.data() ? key.data() : reinterpret_cast<const uint8_t*>("");

    bool result = false;
    Vector<uint8_t> signature;

    gcry_mac_hd_t hd;
    gcry_error_t err;

    err = gcry_mac_open(&hd, algorithm, 0, nullptr);
    if (err)
        goto cleanup;

    err = gcry_mac_setkey(hd, keyData, key.size());
    if (err)
        goto cleanup;

    err = gcry_mac_write(hd, data, dataLength);
    if (err)
        goto cleanup;

    signature.resize(digestLength);
    err = gcry_mac_read(hd, signature.data(), &digestLength);
    if (err)
        goto cleanup;

    signature.resize(digestLength);
    result = true;

cleanup:
    if (hd)
        gcry_mac_close(hd);

    if (!result)
        return std::nullopt;

    return WTFMove(signature);
}

static std::optional<Vector<uint8_t>> calculateSignature(int algorithm, const Vector<uint8_t>& key, const CryptoOperationData& data)
{
    return calculateSignature(algorithm, key, data.first, data.second);
}

void CryptoAlgorithmHMAC::platformSign(Ref<CryptoKey>&& key, Vector<uint8_t>&& data, VectorCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    context.ref();
    workQueue.dispatch([key = WTFMove(key), data = WTFMove(data), callback = WTFMove(callback), exceptionCallback = WTFMove(exceptionCallback), &context]() mutable {
        auto& hmacKey = downcast<CryptoKeyHMAC>(key.get());
        auto algorithm = getGCryptDigestAlgorithm(hmacKey.hashAlgorithmIdentifier());
        if (algorithm != GCRY_MAC_NONE) {
            auto result = calculateSignature(algorithm, hmacKey.key(), data.data(), data.size());
            if (result) {
                // We should only dereference callbacks after being back to the Document/Worker threads.
                context.postTask([callback = WTFMove(callback), result = WTFMove(*result), exceptionCallback = WTFMove(exceptionCallback)](ScriptExecutionContext& context) {
                    callback(result);
                    context.deref();
                });
                return;
            }
        }
        // We should only dereference callbacks after being back to the Document/Worker threads.
        context.postTask([exceptionCallback = WTFMove(exceptionCallback), callback = WTFMove(callback)](ScriptExecutionContext& context) {
            exceptionCallback(OperationError);
            context.deref();
        });
    });
}

void CryptoAlgorithmHMAC::platformVerify(Ref<CryptoKey>&& key, Vector<uint8_t>&& signature, Vector<uint8_t>&& data, BoolCallback&& callback, ExceptionCallback&& exceptionCallback, ScriptExecutionContext& context, WorkQueue& workQueue)
{
    context.ref();
    workQueue.dispatch([key = WTFMove(key), signature = WTFMove(signature), data = WTFMove(data), callback = WTFMove(callback), exceptionCallback = WTFMove(exceptionCallback), &context]() mutable {
        auto& hmacKey = downcast<CryptoKeyHMAC>(key.get());
        auto algorithm = getGCryptDigestAlgorithm(hmacKey.hashAlgorithmIdentifier());
        if (algorithm != GCRY_MAC_NONE) {
            auto expectedSignature = calculateSignature(algorithm, hmacKey.key(), data.data(), data.size());
            if (expectedSignature) {
                // Using a constant time comparison to prevent timing attacks.
                bool result = signature.size() == expectedSignature->size() && !constantTimeMemcmp(expectedSignature->data(), signature.data(), expectedSignature->size());
                // We should only dereference callbacks after being back to the Document/Worker threads.
                context.postTask([callback = WTFMove(callback), result, exceptionCallback = WTFMove(exceptionCallback)](ScriptExecutionContext& context) {
                    callback(result);
                    context.deref();
                });
                return;
            }
        }
        // We should only dereference callbacks after being back to the Document/Worker threads.
        context.postTask([exceptionCallback = WTFMove(exceptionCallback), callback = WTFMove(callback)](ScriptExecutionContext& context) {
            exceptionCallback(OperationError);
            context.deref();
        });
    });
}

ExceptionOr<void> CryptoAlgorithmHMAC::platformSign(const CryptoAlgorithmHmacParamsDeprecated& parameters, const CryptoKeyHMAC& key, const CryptoOperationData& data, VectorCallback&& callback, VoidCallback&& failureCallback)
{
    int algorithm = getGCryptDigestAlgorithm(parameters.hash);
    if (algorithm == GCRY_MAC_NONE)
        return Exception { NOT_SUPPORTED_ERR };

    auto signature = calculateSignature(algorithm, key.key(), data);
    if (signature)
        callback(*signature);
    else
        failureCallback();
    return { };
}

ExceptionOr<void> CryptoAlgorithmHMAC::platformVerify(const CryptoAlgorithmHmacParamsDeprecated& parameters, const CryptoKeyHMAC& key, const CryptoOperationData& expectedSignature, const CryptoOperationData& data, BoolCallback&& callback, VoidCallback&& failureCallback)
{
    int algorithm = getGCryptDigestAlgorithm(parameters.hash);
    if (algorithm == GCRY_MAC_NONE)
        return Exception { NOT_SUPPORTED_ERR };

    auto signature = calculateSignature(algorithm, key.key(), data);
    if (!signature) {
        failureCallback();
        return { };
    }

    // Using a constant time comparison to prevent timing attacks.
    bool result = signature.value().size() == expectedSignature.second && !constantTimeMemcmp(signature.value().data(), expectedSignature.first, signature.value().size());

    callback(result);
    return { };
}

}

#endif // ENABLE(SUBTLE_CRYPTO)

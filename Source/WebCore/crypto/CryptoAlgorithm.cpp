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
#include "CryptoAlgorithm.h"

#include "ScriptExecutionContext.h"

namespace WebCore {

void CryptoAlgorithm::encrypt(const CryptoAlgorithmParameters&, Ref<CryptoKey>&&, Vector<uint8_t>&&, VectorCallback&&, ExceptionCallback&& exceptionCallback, ScriptExecutionContext&, WorkQueue&)
{
    exceptionCallback(ExceptionCode::NotSupportedError);
}

void CryptoAlgorithm::decrypt(const CryptoAlgorithmParameters&, Ref<CryptoKey>&&, Vector<uint8_t>&&, VectorCallback&&, ExceptionCallback&& exceptionCallback, ScriptExecutionContext&, WorkQueue&)
{
    exceptionCallback(ExceptionCode::NotSupportedError);
}

void CryptoAlgorithm::sign(const CryptoAlgorithmParameters&, Ref<CryptoKey>&&, Vector<uint8_t>&&, VectorCallback&&, ExceptionCallback&& exceptionCallback, ScriptExecutionContext&, WorkQueue&)
{
    exceptionCallback(ExceptionCode::NotSupportedError);
}

void CryptoAlgorithm::verify(const CryptoAlgorithmParameters&, Ref<CryptoKey>&&, Vector<uint8_t>&&, Vector<uint8_t>&&, BoolCallback&&, ExceptionCallback&& exceptionCallback, ScriptExecutionContext&, WorkQueue&)
{
    exceptionCallback(ExceptionCode::NotSupportedError);
}

void CryptoAlgorithm::digest(Vector<uint8_t>&&, VectorCallback&&, ExceptionCallback&& exceptionCallback, ScriptExecutionContext&, WorkQueue&)
{
    exceptionCallback(ExceptionCode::NotSupportedError);
}

void CryptoAlgorithm::generateKey(const CryptoAlgorithmParameters&, bool, CryptoKeyUsageBitmap, KeyOrKeyPairCallback&&, ExceptionCallback&& exceptionCallback, ScriptExecutionContext&)
{
    exceptionCallback(ExceptionCode::NotSupportedError);
}

void CryptoAlgorithm::deriveBits(const CryptoAlgorithmParameters&, Ref<CryptoKey>&&, std::optional<size_t>, VectorCallback&&, ExceptionCallback&& exceptionCallback, ScriptExecutionContext&, WorkQueue&)
{
    exceptionCallback(ExceptionCode::NotSupportedError);
}

void CryptoAlgorithm::importKey(CryptoKeyFormat, KeyData&&, const CryptoAlgorithmParameters&, bool, CryptoKeyUsageBitmap, KeyCallback&&, ExceptionCallback&& exceptionCallback)
{
    exceptionCallback(ExceptionCode::NotSupportedError);
}

void CryptoAlgorithm::exportKey(CryptoKeyFormat, Ref<CryptoKey>&&, KeyDataCallback&&, ExceptionCallback&& exceptionCallback)
{
    exceptionCallback(ExceptionCode::NotSupportedError);
}

void CryptoAlgorithm::wrapKey(Ref<CryptoKey>&&, Vector<uint8_t>&&, VectorCallback&&, ExceptionCallback&& exceptionCallback)
{
    exceptionCallback(ExceptionCode::NotSupportedError);
}

void CryptoAlgorithm::unwrapKey(Ref<CryptoKey>&&, Vector<uint8_t>&&, VectorCallback&&, ExceptionCallback&& exceptionCallback)
{
    exceptionCallback(ExceptionCode::NotSupportedError);
}

ExceptionOr<size_t> CryptoAlgorithm::getKeyLength(const CryptoAlgorithmParameters&)
{
    return Exception { ExceptionCode::NotSupportedError };
}

template<typename ResultCallbackType, typename OperationType>
static void dispatchAlgorithmOperation(WorkQueue& workQueue, ScriptExecutionContext& context, ResultCallbackType&& callback, CryptoAlgorithm::ExceptionCallback&& exceptionCallback, OperationType&& operation)
{
    workQueue.dispatch(
        [operation = std::forward<OperationType>(operation), callback = std::forward<ResultCallbackType>(callback), exceptionCallback = WTFMove(exceptionCallback), contextIdentifier = context.identifier()]() mutable {
            auto result = operation();
            ScriptExecutionContext::postTaskTo(contextIdentifier, [result = crossThreadCopy(WTFMove(result)), callback = WTFMove(callback), exceptionCallback = WTFMove(exceptionCallback)](auto&) mutable {
                if (result.hasException()) {
                    exceptionCallback(result.releaseException().code());
                    return;
                }
                callback(result.releaseReturnValue());
            });
        });
}

void CryptoAlgorithm::dispatchOperationInWorkQueue(WorkQueue& workQueue, ScriptExecutionContext& context, VectorCallback&& callback, ExceptionCallback&& exceptionCallback, Function<ExceptionOr<Vector<uint8_t>>()>&& operation)
{
    dispatchAlgorithmOperation(workQueue, context, WTFMove(callback), WTFMove(exceptionCallback), WTFMove(operation));
}

void CryptoAlgorithm::dispatchOperationInWorkQueue(WorkQueue& workQueue, ScriptExecutionContext& context, BoolCallback&& callback, ExceptionCallback&& exceptionCallback, Function<ExceptionOr<bool>()>&& operation)
{
    dispatchAlgorithmOperation(workQueue, context, WTFMove(callback), WTFMove(exceptionCallback), WTFMove(operation));
}

void CryptoAlgorithm::dispatchDigest(WorkQueue& workQueue, ScriptExecutionContext& context, VectorCallback&& callback, ExceptionCallback&&exceptionCallback, Vector<uint8_t>&& message, PAL::CryptoDigest::Algorithm algo)
{
    workQueue.dispatch([message = WTFMove(message), callback = WTFMove(callback), contextIdentifier = context.identifier(), exceptionCallback = WTFMove(exceptionCallback), algo]() mutable {
        auto digest = PAL::CryptoDigest::create(algo);
        if (!digest) {
            exceptionCallback(ExceptionCode::OperationError);
            return;
        }
        digest->addBytes(message.span());
        auto result = digest->computeHash();

        ScriptExecutionContext::postTaskTo(contextIdentifier, [callback = WTFMove(callback), result = WTFMove(result), exceptionCallback = WTFMove(exceptionCallback)](auto&) mutable {
            callback(WTFMove(result));
        });
    });
}

} // namespace WebCore

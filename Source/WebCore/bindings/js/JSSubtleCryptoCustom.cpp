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
#include "JSSubtleCrypto.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithm.h"
#include "CryptoAlgorithmParameters.h"
#include "CryptoAlgorithmRegistry.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "JSCryptoAlgorithmDictionary.h"
#include "JSDOMPromise.h"
#include <runtime/Error.h>

using namespace JSC;

namespace WebCore {

static std::unique_ptr<CryptoAlgorithm> createAlgorithmFromJSValue(ExecState* exec, JSValue value)
{
    CryptoAlgorithmIdentifier algorithmIdentifier;
    if (!JSCryptoAlgorithmDictionary::getAlgorithmIdentifier(exec, value, algorithmIdentifier)) {
        ASSERT(exec->hadException());
        return nullptr;
    }

    std::unique_ptr<CryptoAlgorithm> result(CryptoAlgorithmRegistry::shared().create(algorithmIdentifier));
    if (!result)
        setDOMException(exec, NOT_SUPPORTED_ERR);
    return result;
}

static bool sequenceOfCryptoOperationDataFromJSValue(ExecState* exec, JSValue value, Vector<CryptoOperationData>& result)
{
    unsigned sequenceLength;
    JSObject* sequence = toJSSequence(exec, value, sequenceLength);
    if (!sequence) {
        ASSERT(exec->hadException());
        return false;
    }

    for (unsigned i = 0; i < sequenceLength; ++i) {
        JSValue item = sequence->get(exec, i);
        if (ArrayBuffer* buffer = toArrayBuffer(item))
            result.append(std::make_pair(static_cast<char*>(buffer->data()), buffer->byteLength()));
        else if (RefPtr<ArrayBufferView> bufferView = toArrayBufferView(item))
            result.append(std::make_pair(static_cast<char*>(bufferView->baseAddress()), bufferView->byteLength()));
        else {
            throwTypeError(exec, "Only ArrayBuffer and ArrayBufferView objects can be part of CryptoOperationData sequence");
            return false;
        }
    }
    return true;
}

JSValue JSSubtleCrypto::digest(ExecState* exec)
{
    if (exec->argumentCount() < 2)
        return exec->vm().throwException(exec, createNotEnoughArgumentsError(exec));

    std::unique_ptr<CryptoAlgorithm> algorithm = createAlgorithmFromJSValue(exec, exec->uncheckedArgument(0));
    if (!algorithm) {
        ASSERT(exec->hadException());
        return jsUndefined();
    }

    std::unique_ptr<CryptoAlgorithmParameters> parameters = JSCryptoAlgorithmDictionary::createParametersForDigest(exec, algorithm->identifier(), exec->uncheckedArgument(0));
    if (!parameters) {
        ASSERT(exec->hadException());
        return jsUndefined();
    }

    Vector<CryptoOperationData> data;
    if (!sequenceOfCryptoOperationDataFromJSValue(exec, exec->uncheckedArgument(1), data)) {
        ASSERT(exec->hadException());
        return jsUndefined();
    }

    JSPromise* promise = JSPromise::createWithResolver(exec->vm(), globalObject());
    std::unique_ptr<PromiseWrapper> promiseWrapper = PromiseWrapper::create(globalObject(), promise);

    ExceptionCode ec = 0;
    algorithm->digest(*parameters, data, std::move(promiseWrapper), ec);
    if (ec) {
        setDOMException(exec, ec);
        return jsUndefined();
    }

    return promise;
}

} // namespace WebCore

#endif

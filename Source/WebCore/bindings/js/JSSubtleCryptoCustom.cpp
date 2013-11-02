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
#include "JSCryptoOperationData.h"
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

    auto result = CryptoAlgorithmRegistry::shared().create(algorithmIdentifier);
    if (!result)
        setDOMException(exec, NOT_SUPPORTED_ERR);
    return result;
}

static bool cryptoKeyFormatFromJSValue(ExecState* exec, JSValue value, CryptoKeyFormat& result)
{
    String keyFormatString = value.toString(exec)->value(exec);
    if (exec->hadException())
        return false;
    if (keyFormatString == "raw")
        result = CryptoKeyFormat::Raw;
    else if (keyFormatString == "pkcs8")
        result = CryptoKeyFormat::PKCS8;
    else if (keyFormatString == "spki")
        result = CryptoKeyFormat::SPKI;
    else if (keyFormatString == "jwk")
        result = CryptoKeyFormat::JWK;
    else {
        throwTypeError(exec);
        return false;
    }
    return true;
}

static bool cryptoKeyUsagesFromJSValue(ExecState* exec, JSValue value, CryptoKeyUsage& result)
{
    if (!isJSArray(value)) {
        throwTypeError(exec);
        return false;
    }

    result = 0;

    JSC::JSArray* array = asArray(value);
    for (size_t i = 0; i < array->length(); ++i) {
        JSC::JSValue element = array->getIndex(exec, i);
        String usageString = element.toString(exec)->value(exec);
        if (exec->hadException())
            return false;
        if (usageString == "encrypt")
            result |= CryptoKeyUsageEncrypt;
        else if (usageString == "decrypt")
            result |= CryptoKeyUsageDecrypt;
        else if (usageString == "sign")
            result |= CryptoKeyUsageSign;
        else if (usageString == "verify")
            result |= CryptoKeyUsageVerify;
        else if (usageString == "deriveKey")
            result |= CryptoKeyUsageDeriveKey;
        else if (usageString == "deriveBits")
            result |= CryptoKeyUsageDeriveBits;
        else if (usageString == "wrapKey")
            result |= CryptoKeyUsageWrapKey;
        else if (usageString == "unwrapKey")
            result |= CryptoKeyUsageUnwrapKey;
    }
    return true;
}

JSValue JSSubtleCrypto::encrypt(ExecState* exec)
{
    if (exec->argumentCount() < 3)
        return exec->vm().throwException(exec, createNotEnoughArgumentsError(exec));

    auto algorithm = createAlgorithmFromJSValue(exec, exec->uncheckedArgument(0));
    if (!algorithm) {
        ASSERT(exec->hadException());
        return jsUndefined();
    }

    auto parameters = JSCryptoAlgorithmDictionary::createParametersForEncrypt(exec, algorithm->identifier(), exec->uncheckedArgument(0));
    if (!parameters) {
        ASSERT(exec->hadException());
        return jsUndefined();
    }

    RefPtr<CryptoKey> key = toCryptoKey(exec->uncheckedArgument(1));
    if (!key)
        return throwTypeError(exec);

    if (!key->allows(CryptoKeyUsageEncrypt)) {
        m_impl->document()->addConsoleMessage(JSMessageSource, ErrorMessageLevel, "Key usages does not include 'encrypt'");
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return jsUndefined();
    }

    Vector<CryptoOperationData> data;
    if (!sequenceOfCryptoOperationDataFromJSValue(exec, exec->uncheckedArgument(2), data)) {
        ASSERT(exec->hadException());
        return jsUndefined();
    }

    JSPromise* promise = JSPromise::createWithResolver(exec->vm(), globalObject());
    auto promiseWrapper = PromiseWrapper::create(globalObject(), promise);

    ExceptionCode ec = 0;
    algorithm->encrypt(*parameters, *key, data, std::move(promiseWrapper), ec);
    if (ec) {
        setDOMException(exec, ec);
        return jsUndefined();
    }

    return promise;
}

JSValue JSSubtleCrypto::decrypt(ExecState* exec)
{
    if (exec->argumentCount() < 3)
        return exec->vm().throwException(exec, createNotEnoughArgumentsError(exec));

    auto algorithm = createAlgorithmFromJSValue(exec, exec->uncheckedArgument(0));
    if (!algorithm) {
        ASSERT(exec->hadException());
        return jsUndefined();
    }

    auto parameters = JSCryptoAlgorithmDictionary::createParametersForDecrypt(exec, algorithm->identifier(), exec->uncheckedArgument(0));
    if (!parameters) {
        ASSERT(exec->hadException());
        return jsUndefined();
    }

    RefPtr<CryptoKey> key = toCryptoKey(exec->uncheckedArgument(1));
    if (!key)
        return throwTypeError(exec);

    if (!key->allows(CryptoKeyUsageDecrypt)) {
        m_impl->document()->addConsoleMessage(JSMessageSource, ErrorMessageLevel, "Key usages does not include 'decrypt'");
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return jsUndefined();
    }

    Vector<CryptoOperationData> data;
    if (!sequenceOfCryptoOperationDataFromJSValue(exec, exec->uncheckedArgument(2), data)) {
        ASSERT(exec->hadException());
        return jsUndefined();
    }

    JSPromise* promise = JSPromise::createWithResolver(exec->vm(), globalObject());
    auto promiseWrapper = PromiseWrapper::create(globalObject(), promise);

    ExceptionCode ec = 0;
    algorithm->decrypt(*parameters, *key, data, std::move(promiseWrapper), ec);
    if (ec) {
        setDOMException(exec, ec);
        return jsUndefined();
    }

    return promise;
}

JSValue JSSubtleCrypto::sign(ExecState* exec)
{
    if (exec->argumentCount() < 3)
        return exec->vm().throwException(exec, createNotEnoughArgumentsError(exec));

    auto algorithm = createAlgorithmFromJSValue(exec, exec->uncheckedArgument(0));
    if (!algorithm) {
        ASSERT(exec->hadException());
        return jsUndefined();
    }

    auto parameters = JSCryptoAlgorithmDictionary::createParametersForSign(exec, algorithm->identifier(), exec->uncheckedArgument(0));
    if (!parameters) {
        ASSERT(exec->hadException());
        return jsUndefined();
    }

    RefPtr<CryptoKey> key = toCryptoKey(exec->uncheckedArgument(1));
    if (!key)
        return throwTypeError(exec);

    if (!key->allows(CryptoKeyUsageSign)) {
        m_impl->document()->addConsoleMessage(JSMessageSource, ErrorMessageLevel, "Key usages does not include 'sign'");
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return jsUndefined();
    }

    Vector<CryptoOperationData> data;
    if (!sequenceOfCryptoOperationDataFromJSValue(exec, exec->uncheckedArgument(2), data)) {
        ASSERT(exec->hadException());
        return jsUndefined();
    }

    JSPromise* promise = JSPromise::createWithResolver(exec->vm(), globalObject());
    auto promiseWrapper = PromiseWrapper::create(globalObject(), promise);

    ExceptionCode ec = 0;
    algorithm->sign(*parameters, *key, data, std::move(promiseWrapper), ec);
    if (ec) {
        setDOMException(exec, ec);
        return jsUndefined();
    }

    return promise;
}

JSValue JSSubtleCrypto::verify(ExecState* exec)
{
    if (exec->argumentCount() < 4)
        return exec->vm().throwException(exec, createNotEnoughArgumentsError(exec));

    auto algorithm = createAlgorithmFromJSValue(exec, exec->uncheckedArgument(0));
    if (!algorithm) {
        ASSERT(exec->hadException());
        return jsUndefined();
    }

    auto parameters = JSCryptoAlgorithmDictionary::createParametersForVerify(exec, algorithm->identifier(), exec->uncheckedArgument(0));
    if (!parameters) {
        ASSERT(exec->hadException());
        return jsUndefined();
    }

    RefPtr<CryptoKey> key = toCryptoKey(exec->uncheckedArgument(1));
    if (!key)
        return throwTypeError(exec);

    if (!key->allows(CryptoKeyUsageVerify)) {
        m_impl->document()->addConsoleMessage(JSMessageSource, ErrorMessageLevel, "Key usages does not include 'verify'");
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return jsUndefined();
    }

    CryptoOperationData signature;
    if (!cryptoOperationDataFromJSValue(exec, exec->uncheckedArgument(2), signature)) {
        ASSERT(exec->hadException());
        return jsUndefined();
    }

    Vector<CryptoOperationData> data;
    if (!sequenceOfCryptoOperationDataFromJSValue(exec, exec->uncheckedArgument(3), data)) {
        ASSERT(exec->hadException());
        return jsUndefined();
    }

    JSPromise* promise = JSPromise::createWithResolver(exec->vm(), globalObject());
    auto promiseWrapper = PromiseWrapper::create(globalObject(), promise);

    ExceptionCode ec = 0;
    algorithm->verify(*parameters, *key, signature, data, std::move(promiseWrapper), ec);
    if (ec) {
        setDOMException(exec, ec);
        return jsUndefined();
    }

    return promise;
}

JSValue JSSubtleCrypto::digest(ExecState* exec)
{
    if (exec->argumentCount() < 2)
        return exec->vm().throwException(exec, createNotEnoughArgumentsError(exec));

    auto algorithm = createAlgorithmFromJSValue(exec, exec->uncheckedArgument(0));
    if (!algorithm) {
        ASSERT(exec->hadException());
        return jsUndefined();
    }

    auto parameters = JSCryptoAlgorithmDictionary::createParametersForDigest(exec, algorithm->identifier(), exec->uncheckedArgument(0));
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

JSValue JSSubtleCrypto::importKey(JSC::ExecState* exec)
{
    if (exec->argumentCount() < 3)
        return exec->vm().throwException(exec, createNotEnoughArgumentsError(exec));

    CryptoKeyFormat keyFormat;
    if (!cryptoKeyFormatFromJSValue(exec, exec->argument(0), keyFormat)) {
        ASSERT(exec->hadException());
        return jsUndefined();
    }

    CryptoOperationData data;
    if (!cryptoOperationDataFromJSValue(exec, exec->uncheckedArgument(1), data)) {
        ASSERT(exec->hadException());
        return jsUndefined();
    }

    std::unique_ptr<CryptoAlgorithm> algorithm;
    if (!exec->uncheckedArgument(2).isNull()) {
        algorithm = createAlgorithmFromJSValue(exec, exec->uncheckedArgument(2));
        if (!algorithm) {
            ASSERT(exec->hadException());
            return jsUndefined();
        }
    }
    // The algorithm can presumably be null when we can deduce it from the key (JWE perhaps?)
    // But we only support raw key format right now.
    if (!algorithm) {
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return jsUndefined();
    }

    auto parameters = JSCryptoAlgorithmDictionary::createParametersForImportKey(exec, algorithm->identifier(), exec->uncheckedArgument(2));
    if (!parameters) {
        ASSERT(exec->hadException());
        return jsUndefined();
    }

    bool extractable = false;
    if (exec->argumentCount() >= 4) {
        extractable = exec->uncheckedArgument(3).toBoolean(exec);
        if (exec->hadException())
            return jsUndefined();
    }

    CryptoKeyUsage keyUsages = 0;
    if (exec->argumentCount() >= 5) {
        if (!cryptoKeyUsagesFromJSValue(exec, exec->argument(4), keyUsages)) {
            ASSERT(exec->hadException());
            return jsUndefined();
        }
    }

    JSPromise* promise = JSPromise::createWithResolver(exec->vm(), globalObject());
    auto promiseWrapper = PromiseWrapper::create(globalObject(), promise);

    ExceptionCode ec = 0;
    algorithm->importKey(*parameters, keyFormat, data, extractable, keyUsages, std::move(promiseWrapper), ec);
    if (ec) {
        setDOMException(exec, ec);
        return jsUndefined();
    }

    return promise;
}

} // namespace WebCore

#endif

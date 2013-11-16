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
#include "CryptoKeyData.h"
#include "CryptoKeySerializationRaw.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "JSCryptoAlgorithmDictionary.h"
#include "JSCryptoKeySerializationJWK.h"
#include "JSCryptoOperationData.h"
#include "JSDOMPromise.h"
#include <runtime/Error.h>

using namespace JSC;

namespace WebCore {

ENUM_CLASS(CryptoKeyFormat) {
    // An unformatted sequence of bytes. Intended for secret keys.
    Raw,

    // The DER encoding of the PrivateKeyInfo structure from RFC 5208.
    PKCS8,

    // The DER encoding of the SubjectPublicKeyInfo structure from RFC 5280.
    SPKI,

    // The key is represented as JSON according to the JSON Web Key format.
    JWK
};

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
        throwTypeError(exec, "Unknown key format");
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

    CryptoOperationData data;
    if (!cryptoOperationDataFromJSValue(exec, exec->uncheckedArgument(2), data)) {
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

    CryptoOperationData data;
    if (!cryptoOperationDataFromJSValue(exec, exec->uncheckedArgument(2), data)) {
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

    CryptoOperationData data;
    if (!cryptoOperationDataFromJSValue(exec, exec->uncheckedArgument(2), data)) {
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

    CryptoOperationData data;
    if (!cryptoOperationDataFromJSValue(exec, exec->uncheckedArgument(3), data)) {
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

    CryptoOperationData data;
    if (!cryptoOperationDataFromJSValue(exec, exec->uncheckedArgument(1), data)) {
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

JSValue JSSubtleCrypto::generateKey(JSC::ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return exec->vm().throwException(exec, createNotEnoughArgumentsError(exec));

    auto algorithm = createAlgorithmFromJSValue(exec, exec->uncheckedArgument(0));
    if (!algorithm) {
        ASSERT(exec->hadException());
        return jsUndefined();
    }

    auto parameters = JSCryptoAlgorithmDictionary::createParametersForGenerateKey(exec, algorithm->identifier(), exec->uncheckedArgument(0));
    if (!parameters) {
        ASSERT(exec->hadException());
        return jsUndefined();
    }

    bool extractable = false;
    if (exec->argumentCount() >= 2) {
        extractable = exec->uncheckedArgument(1).toBoolean(exec);
        if (exec->hadException())
            return jsUndefined();
    }

    CryptoKeyUsage keyUsages = 0;
    if (exec->argumentCount() >= 3) {
        if (!cryptoKeyUsagesFromJSValue(exec, exec->argument(2), keyUsages)) {
            ASSERT(exec->hadException());
            return jsUndefined();
        }
    }

    JSPromise* promise = JSPromise::createWithResolver(exec->vm(), globalObject());
    auto promiseWrapper = PromiseWrapper::create(globalObject(), promise);

    ExceptionCode ec = 0;
    algorithm->generateKey(*parameters, extractable, keyUsages, std::move(promiseWrapper), ec);
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
    std::unique_ptr<CryptoAlgorithmParameters> parameters;
    if (!exec->uncheckedArgument(2).isNull()) {
        algorithm = createAlgorithmFromJSValue(exec, exec->uncheckedArgument(2));
        if (!algorithm) {
            ASSERT(exec->hadException());
            return jsUndefined();
        }
        parameters = JSCryptoAlgorithmDictionary::createParametersForImportKey(exec, algorithm->identifier(), exec->uncheckedArgument(2));
        if (!parameters) {
            ASSERT(exec->hadException());
            return jsUndefined();
        }
    }

    std::unique_ptr<CryptoKeySerialization> keySerialization;
    switch (keyFormat) {
    case CryptoKeyFormat::Raw:
        keySerialization = CryptoKeySerializationRaw::create(data);
        break;
    case CryptoKeyFormat::JWK: {
        String jwkString = String::fromUTF8(data.first, data.second);
        if (jwkString.isNull()) {
            throwTypeError(exec, "JWK JSON serialization is not valid UTF-8");
            return jsUndefined();
        }
        keySerialization = JSCryptoKeySerializationJWK::create(exec, jwkString);
        if (exec->hadException())
            return jsUndefined();
        break;
    }
    default:
        throwTypeError(exec, "Unsupported key format for import");
        return jsUndefined();
    }

    ASSERT(keySerialization);

    if (!keySerialization->reconcileAlgorithm(algorithm, parameters)) {
        if (!exec->hadException())
            throwTypeError(exec, "Algorithm specified in key is not compatible with one passed to importKey as argument");
        return jsUndefined();
    }
    if (exec->hadException())
        return jsUndefined();

    if (!algorithm) {
        throwTypeError(exec, "Neither key nor function argument has crypto algorithm specified");
        return jsUndefined();
    }
    ASSERT(parameters);

    bool extractable = false;
    if (exec->argumentCount() >= 4) {
        extractable = exec->uncheckedArgument(3).toBoolean(exec);
        if (exec->hadException())
            return jsUndefined();
    }
    keySerialization->reconcileExtractable(extractable);
    if (exec->hadException())
        return jsUndefined();

    CryptoKeyUsage keyUsages = 0;
    if (exec->argumentCount() >= 5) {
        if (!cryptoKeyUsagesFromJSValue(exec, exec->argument(4), keyUsages)) {
            ASSERT(exec->hadException());
            return jsUndefined();
        }
    }
    keySerialization->reconcileUsages(keyUsages);
    if (exec->hadException())
        return jsUndefined();

    auto keyData = keySerialization->keyData();
    if (exec->hadException())
        return jsUndefined();

    JSPromise* promise = JSPromise::createWithResolver(exec->vm(), globalObject());
    auto promiseWrapper = PromiseWrapper::create(globalObject(), promise);

    ExceptionCode ec = 0;
    algorithm->importKey(*parameters, *keyData, extractable, keyUsages, std::move(promiseWrapper), ec);
    if (ec) {
        setDOMException(exec, ec);
        return jsUndefined();
    }

    return promise;
}

JSValue JSSubtleCrypto::exportKey(JSC::ExecState* exec)
{
    if (exec->argumentCount() < 2)
        return exec->vm().throwException(exec, createNotEnoughArgumentsError(exec));

    CryptoKeyFormat keyFormat;
    if (!cryptoKeyFormatFromJSValue(exec, exec->argument(0), keyFormat)) {
        ASSERT(exec->hadException());
        return jsUndefined();
    }

    RefPtr<CryptoKey> key = toCryptoKey(exec->uncheckedArgument(1));
    if (!key)
        return throwTypeError(exec);

    JSPromise* promise = JSPromise::createWithResolver(exec->vm(), globalObject());
    auto promiseWrapper = PromiseWrapper::create(globalObject(), promise);

    if (!key->extractable()) {
        m_impl->document()->addConsoleMessage(JSMessageSource, ErrorMessageLevel, "Key is not extractable");
        promiseWrapper->reject(nullptr);
        return promise;
    }

    switch (keyFormat) {
    case CryptoKeyFormat::Raw: {
        Vector<unsigned char> result;
        if (CryptoKeySerializationRaw::serialize(*key, result))
            promiseWrapper->fulfill(result);
        else {
            m_impl->document()->addConsoleMessage(JSMessageSource, ErrorMessageLevel, "Key cannot be exported to raw format");
            promiseWrapper->reject(nullptr);
        }
        break;
    }
    case CryptoKeyFormat::JWK: {
        String result = JSCryptoKeySerializationJWK::serialize(exec, *key);
        if (exec->hadException())
            return jsUndefined();
        CString utf8String = result.utf8(StrictConversion);
        Vector<unsigned char> resultBuffer;
        resultBuffer.append(utf8String.data(), utf8String.length());
        promiseWrapper->fulfill(result);
        break;
    }
    default:
        throwTypeError(exec, "Unsupported key format for export");
        return jsUndefined();
    }

    return promise;
}

} // namespace WebCore

#endif

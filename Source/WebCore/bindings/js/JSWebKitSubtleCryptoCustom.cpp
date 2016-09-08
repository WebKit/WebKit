/*
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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
#include "JSWebKitSubtleCrypto.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithm.h"
#include "CryptoAlgorithmParameters.h"
#include "CryptoAlgorithmRegistry.h"
#include "CryptoKeyData.h"
#include "CryptoKeySerializationRaw.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "JSCryptoAlgorithmDictionary.h"
#include "JSCryptoKey.h"
#include "JSCryptoKeyPair.h"
#include "JSCryptoKeySerializationJWK.h"
#include "JSCryptoOperationData.h"
#include "JSDOMPromise.h"
#include <runtime/Error.h>

using namespace JSC;

namespace WebCore {

enum class CryptoKeyFormat {
    // An unformatted sequence of bytes. Intended for secret keys.
    Raw,

    // The DER encoding of the PrivateKeyInfo structure from RFC 5208.
    PKCS8,

    // The DER encoding of the SubjectPublicKeyInfo structure from RFC 5280.
    SPKI,

    // The key is represented as JSON according to the JSON Web Key format.
    JWK
};

static RefPtr<CryptoAlgorithm> createAlgorithmFromJSValue(ExecState& state, JSValue value)
{
    CryptoAlgorithmIdentifier algorithmIdentifier;
    if (!JSCryptoAlgorithmDictionary::getAlgorithmIdentifier(&state, value, algorithmIdentifier)) {
        ASSERT(state.hadException());
        return nullptr;
    }

    auto result = CryptoAlgorithmRegistry::singleton().create(algorithmIdentifier);
    if (!result)
        setDOMException(&state, NOT_SUPPORTED_ERR);
    return result;
}

static bool cryptoKeyFormatFromJSValue(ExecState& state, JSValue value, CryptoKeyFormat& result)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String keyFormatString = value.toString(&state)->value(&state);
    if (state.hadException())
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
        throwTypeError(&state, scope, ASCIILiteral("Unknown key format"));
        return false;
    }
    return true;
}

static bool cryptoKeyUsagesFromJSValue(ExecState& state, JSValue value, CryptoKeyUsage& result)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!isJSArray(value)) {
        throwTypeError(&state, scope);
        return false;
    }

    result = 0;

    JSArray* array = asArray(value);
    for (size_t i = 0; i < array->length(); ++i) {
        JSValue element = array->getIndex(&state, i);
        String usageString = element.toString(&state)->value(&state);
        if (state.hadException())
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

JSValue JSWebKitSubtleCrypto::encrypt(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() < 3)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    auto algorithm = createAlgorithmFromJSValue(state, state.uncheckedArgument(0));
    if (!algorithm) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    auto parameters = JSCryptoAlgorithmDictionary::createParametersForEncrypt(&state, algorithm->identifier(), state.uncheckedArgument(0));
    if (!parameters) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    RefPtr<CryptoKey> key = JSCryptoKey::toWrapped(state.uncheckedArgument(1));
    if (!key)
        return throwTypeError(&state, scope);

    if (!key->allows(CryptoKeyUsageEncrypt)) {
        wrapped().document()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, ASCIILiteral("Key usages do not include 'encrypt'"));
        setDOMException(&state, NOT_SUPPORTED_ERR);
        return jsUndefined();
    }

    CryptoOperationData data;
    if (!cryptoOperationDataFromJSValue(&state, state.uncheckedArgument(2), data)) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    
    JSPromiseDeferred* promiseDeferred = JSPromiseDeferred::create(&state, globalObject());
    DeferredWrapper wrapper(&state, globalObject(), promiseDeferred);
    auto successCallback = [wrapper](const Vector<uint8_t>& result) mutable {
        fulfillPromiseWithArrayBuffer(wrapper, result.data(), result.size());
    };
    auto failureCallback = [wrapper]() mutable {
        wrapper.reject(nullptr);
    };

    ExceptionCode ec = 0;
    algorithm->encrypt(*parameters, *key, data, WTFMove(successCallback), WTFMove(failureCallback), ec);
    if (ec) {
        setDOMException(&state, ec);
        return jsUndefined();
    }

    return promiseDeferred->promise();
}

JSValue JSWebKitSubtleCrypto::decrypt(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() < 3)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    auto algorithm = createAlgorithmFromJSValue(state, state.uncheckedArgument(0));
    if (!algorithm) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    auto parameters = JSCryptoAlgorithmDictionary::createParametersForDecrypt(&state, algorithm->identifier(), state.uncheckedArgument(0));
    if (!parameters) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    RefPtr<CryptoKey> key = JSCryptoKey::toWrapped(state.uncheckedArgument(1));
    if (!key)
        return throwTypeError(&state, scope);

    if (!key->allows(CryptoKeyUsageDecrypt)) {
        wrapped().document()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, ASCIILiteral("Key usages do not include 'decrypt'"));
        setDOMException(&state, NOT_SUPPORTED_ERR);
        return jsUndefined();
    }

    CryptoOperationData data;
    if (!cryptoOperationDataFromJSValue(&state, state.uncheckedArgument(2), data)) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    JSPromiseDeferred* promiseDeferred = JSPromiseDeferred::create(&state, globalObject());
    DeferredWrapper wrapper(&state, globalObject(), promiseDeferred);
    auto successCallback = [wrapper](const Vector<uint8_t>& result) mutable {
        fulfillPromiseWithArrayBuffer(wrapper, result.data(), result.size());
    };
    auto failureCallback = [wrapper]() mutable {
        wrapper.reject(nullptr);
    };

    ExceptionCode ec = 0;
    algorithm->decrypt(*parameters, *key, data, WTFMove(successCallback), WTFMove(failureCallback), ec);
    if (ec) {
        setDOMException(&state, ec);
        return jsUndefined();
    }

    return promiseDeferred->promise();
}

JSValue JSWebKitSubtleCrypto::sign(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() < 3)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    auto algorithm = createAlgorithmFromJSValue(state, state.uncheckedArgument(0));
    if (!algorithm) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    auto parameters = JSCryptoAlgorithmDictionary::createParametersForSign(&state, algorithm->identifier(), state.uncheckedArgument(0));
    if (!parameters) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    RefPtr<CryptoKey> key = JSCryptoKey::toWrapped(state.uncheckedArgument(1));
    if (!key)
        return throwTypeError(&state, scope);

    if (!key->allows(CryptoKeyUsageSign)) {
        wrapped().document()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, ASCIILiteral("Key usages do not include 'sign'"));
        setDOMException(&state, NOT_SUPPORTED_ERR);
        return jsUndefined();
    }

    CryptoOperationData data;
    if (!cryptoOperationDataFromJSValue(&state, state.uncheckedArgument(2), data)) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    JSPromiseDeferred* promiseDeferred = JSPromiseDeferred::create(&state, globalObject());
    DeferredWrapper wrapper(&state, globalObject(), promiseDeferred);
    auto successCallback = [wrapper](const Vector<uint8_t>& result) mutable {
        fulfillPromiseWithArrayBuffer(wrapper, result.data(), result.size());
    };
    auto failureCallback = [wrapper]() mutable {
        wrapper.reject(nullptr);
    };

    ExceptionCode ec = 0;
    algorithm->sign(*parameters, *key, data, WTFMove(successCallback), WTFMove(failureCallback), ec);
    if (ec) {
        setDOMException(&state, ec);
        return jsUndefined();
    }

    return promiseDeferred->promise();
}

JSValue JSWebKitSubtleCrypto::verify(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() < 4)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    auto algorithm = createAlgorithmFromJSValue(state, state.uncheckedArgument(0));
    if (!algorithm) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    auto parameters = JSCryptoAlgorithmDictionary::createParametersForVerify(&state, algorithm->identifier(), state.uncheckedArgument(0));
    if (!parameters) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    RefPtr<CryptoKey> key = JSCryptoKey::toWrapped(state.uncheckedArgument(1));
    if (!key)
        return throwTypeError(&state, scope);

    if (!key->allows(CryptoKeyUsageVerify)) {
        wrapped().document()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, ASCIILiteral("Key usages do not include 'verify'"));
        setDOMException(&state, NOT_SUPPORTED_ERR);
        return jsUndefined();
    }

    CryptoOperationData signature;
    if (!cryptoOperationDataFromJSValue(&state, state.uncheckedArgument(2), signature)) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    CryptoOperationData data;
    if (!cryptoOperationDataFromJSValue(&state, state.uncheckedArgument(3), data)) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    JSPromiseDeferred* promiseDeferred = JSPromiseDeferred::create(&state, globalObject());
    DeferredWrapper wrapper(&state, globalObject(), promiseDeferred);
    auto successCallback = [wrapper](bool result) mutable {
        wrapper.resolve(result);
    };
    auto failureCallback = [wrapper]() mutable {
        wrapper.reject(nullptr);
    };

    ExceptionCode ec = 0;
    algorithm->verify(*parameters, *key, signature, data, WTFMove(successCallback), WTFMove(failureCallback), ec);
    if (ec) {
        setDOMException(&state, ec);
        return jsUndefined();
    }

    return promiseDeferred->promise();
}

JSValue JSWebKitSubtleCrypto::digest(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() < 2)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    auto algorithm = createAlgorithmFromJSValue(state, state.uncheckedArgument(0));
    if (!algorithm) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    auto parameters = JSCryptoAlgorithmDictionary::createParametersForDigest(&state, algorithm->identifier(), state.uncheckedArgument(0));
    if (!parameters) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    CryptoOperationData data;
    if (!cryptoOperationDataFromJSValue(&state, state.uncheckedArgument(1), data)) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    JSPromiseDeferred* promiseDeferred = JSPromiseDeferred::create(&state, globalObject());
    DeferredWrapper wrapper(&state, globalObject(), promiseDeferred);
    auto successCallback = [wrapper](const Vector<uint8_t>& result) mutable {
        fulfillPromiseWithArrayBuffer(wrapper, result.data(), result.size());
    };
    auto failureCallback = [wrapper]() mutable {
        wrapper.reject(nullptr);
    };

    ExceptionCode ec = 0;
    algorithm->digest(*parameters, data, WTFMove(successCallback), WTFMove(failureCallback), ec);
    if (ec) {
        setDOMException(&state, ec);
        return jsUndefined();
    }

    return promiseDeferred->promise();
}

JSValue JSWebKitSubtleCrypto::generateKey(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() < 1)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    auto algorithm = createAlgorithmFromJSValue(state, state.uncheckedArgument(0));
    if (!algorithm) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    auto parameters = JSCryptoAlgorithmDictionary::createParametersForGenerateKey(&state, algorithm->identifier(), state.uncheckedArgument(0));
    if (!parameters) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    bool extractable = false;
    if (state.argumentCount() >= 2) {
        extractable = state.uncheckedArgument(1).toBoolean(&state);
        if (state.hadException())
            return jsUndefined();
    }

    CryptoKeyUsage keyUsages = 0;
    if (state.argumentCount() >= 3) {
        if (!cryptoKeyUsagesFromJSValue(state, state.argument(2), keyUsages)) {
            ASSERT(state.hadException());
            return jsUndefined();
        }
    }

    JSPromiseDeferred* promiseDeferred = JSPromiseDeferred::create(&state, globalObject());
    DeferredWrapper wrapper(&state, globalObject(), promiseDeferred);
    auto successCallback = [wrapper](CryptoKey* key, CryptoKeyPair* keyPair) mutable {
        ASSERT(key || keyPair);
        ASSERT(!key || !keyPair);
        if (key)
            wrapper.resolve(key);
        else
            wrapper.resolve(keyPair);
    };
    auto failureCallback = [wrapper]() mutable {
        wrapper.reject(nullptr);
    };

    ExceptionCode ec = 0;
    algorithm->generateKey(*parameters, extractable, keyUsages, WTFMove(successCallback), WTFMove(failureCallback), ec);
    if (ec) {
        setDOMException(&state, ec);
        return jsUndefined();
    }

    return promiseDeferred->promise();
}

static void importKey(ExecState& state, CryptoKeyFormat keyFormat, CryptoOperationData data, RefPtr<CryptoAlgorithm> algorithm, RefPtr<CryptoAlgorithmParameters> parameters, bool extractable, CryptoKeyUsage keyUsages, CryptoAlgorithm::KeyCallback callback, CryptoAlgorithm::VoidCallback failureCallback)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    std::unique_ptr<CryptoKeySerialization> keySerialization;
    switch (keyFormat) {
    case CryptoKeyFormat::Raw:
        keySerialization = CryptoKeySerializationRaw::create(data);
        break;
    case CryptoKeyFormat::JWK: {
        String jwkString = String::fromUTF8(data.first, data.second);
        if (jwkString.isNull()) {
            throwTypeError(&state, scope, ASCIILiteral("JWK JSON serialization is not valid UTF-8"));
            return;
        }
        keySerialization = std::make_unique<JSCryptoKeySerializationJWK>(&state, jwkString);
        if (state.hadException())
            return;
        break;
    }
    default:
        throwTypeError(&state, scope, ASCIILiteral("Unsupported key format for import"));
        return;
    }

    ASSERT(keySerialization);

    Optional<CryptoAlgorithmPair> reconciledResult = keySerialization->reconcileAlgorithm(algorithm.get(), parameters.get());
    if (!reconciledResult) {
        if (!state.hadException())
            throwTypeError(&state, scope, ASCIILiteral("Algorithm specified in key is not compatible with one passed to importKey as argument"));
        return;
    }
    if (state.hadException())
        return;

    algorithm = reconciledResult->algorithm;
    parameters = reconciledResult->parameters;
    if (!algorithm) {
        throwTypeError(&state, scope, ASCIILiteral("Neither key nor function argument has crypto algorithm specified"));
        return;
    }
    ASSERT(parameters);

    keySerialization->reconcileExtractable(extractable);
    if (state.hadException())
        return;

    keySerialization->reconcileUsages(keyUsages);
    if (state.hadException())
        return;

    auto keyData = keySerialization->keyData();
    if (state.hadException())
        return;

    ExceptionCode ec = 0;
    algorithm->importKey(*parameters, *keyData, extractable, keyUsages, WTFMove(callback), WTFMove(failureCallback), ec);
    if (ec)
        setDOMException(&state, ec);
}

JSValue JSWebKitSubtleCrypto::importKey(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() < 3)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    CryptoKeyFormat keyFormat;
    if (!cryptoKeyFormatFromJSValue(state, state.argument(0), keyFormat)) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    CryptoOperationData data;
    if (!cryptoOperationDataFromJSValue(&state, state.uncheckedArgument(1), data)) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    RefPtr<CryptoAlgorithm> algorithm;
    RefPtr<CryptoAlgorithmParameters> parameters;
    if (!state.uncheckedArgument(2).isNull()) {
        algorithm = createAlgorithmFromJSValue(state, state.uncheckedArgument(2));
        if (!algorithm) {
            ASSERT(state.hadException());
            return jsUndefined();
        }
        parameters = JSCryptoAlgorithmDictionary::createParametersForImportKey(&state, algorithm->identifier(), state.uncheckedArgument(2));
        if (!parameters) {
            ASSERT(state.hadException());
            return jsUndefined();
        }
    }

    bool extractable = false;
    if (state.argumentCount() >= 4) {
        extractable = state.uncheckedArgument(3).toBoolean(&state);
        if (state.hadException())
            return jsUndefined();
    }

    CryptoKeyUsage keyUsages = 0;
    if (state.argumentCount() >= 5) {
        if (!cryptoKeyUsagesFromJSValue(state, state.argument(4), keyUsages)) {
            ASSERT(state.hadException());
            return jsUndefined();
        }
    }

    JSPromiseDeferred* promiseDeferred = JSPromiseDeferred::create(&state, globalObject());
    DeferredWrapper wrapper(&state, globalObject(), promiseDeferred);
    auto successCallback = [wrapper](CryptoKey& result) mutable {
        wrapper.resolve(result);
    };
    auto failureCallback = [wrapper]() mutable {
        wrapper.reject(nullptr);
    };

    WebCore::importKey(state, keyFormat, data, WTFMove(algorithm), WTFMove(parameters), extractable, keyUsages, WTFMove(successCallback), WTFMove(failureCallback));
    if (state.hadException())
        return jsUndefined();

    return promiseDeferred->promise();
}

static void exportKey(ExecState& state, CryptoKeyFormat keyFormat, const CryptoKey& key, CryptoAlgorithm::VectorCallback callback, CryptoAlgorithm::VoidCallback failureCallback)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!key.extractable()) {
        throwTypeError(&state, scope, ASCIILiteral("Key is not extractable"));
        return;
    }

    switch (keyFormat) {
    case CryptoKeyFormat::Raw: {
        Vector<uint8_t> result;
        if (CryptoKeySerializationRaw::serialize(key, result))
            callback(result);
        else
            failureCallback();
        break;
    }
    case CryptoKeyFormat::JWK: {
        String result = JSCryptoKeySerializationJWK::serialize(&state, key);
        if (state.hadException())
            return;
        CString utf8String = result.utf8(StrictConversion);
        Vector<uint8_t> resultBuffer;
        resultBuffer.append(utf8String.data(), utf8String.length());
        callback(resultBuffer);
        break;
    }
    default:
        throwTypeError(&state, scope, ASCIILiteral("Unsupported key format for export"));
        break;
    }
}

JSValue JSWebKitSubtleCrypto::exportKey(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() < 2)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    CryptoKeyFormat keyFormat;
    if (!cryptoKeyFormatFromJSValue(state, state.argument(0), keyFormat)) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    RefPtr<CryptoKey> key = JSCryptoKey::toWrapped(state.uncheckedArgument(1));
    if (!key)
        return throwTypeError(&state, scope);

    JSPromiseDeferred* promiseDeferred = JSPromiseDeferred::create(&state, globalObject());
    DeferredWrapper wrapper(&state, globalObject(), promiseDeferred);
    auto successCallback = [wrapper](const Vector<uint8_t>& result) mutable {
        fulfillPromiseWithArrayBuffer(wrapper, result.data(), result.size());
    };
    auto failureCallback = [wrapper]() mutable {
        wrapper.reject(nullptr);
    };

    WebCore::exportKey(state, keyFormat, *key, WTFMove(successCallback), WTFMove(failureCallback));
    if (state.hadException())
        return jsUndefined();

    return promiseDeferred->promise();
}

JSValue JSWebKitSubtleCrypto::wrapKey(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() < 4)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    CryptoKeyFormat keyFormat;
    if (!cryptoKeyFormatFromJSValue(state, state.argument(0), keyFormat)) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    RefPtr<CryptoKey> key = JSCryptoKey::toWrapped(state.uncheckedArgument(1));
    if (!key)
        return throwTypeError(&state, scope);

    RefPtr<CryptoKey> wrappingKey = JSCryptoKey::toWrapped(state.uncheckedArgument(2));
    if (!key)
        return throwTypeError(&state, scope);

    if (!wrappingKey->allows(CryptoKeyUsageWrapKey)) {
        wrapped().document()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, ASCIILiteral("Key usages do not include 'wrapKey'"));
        setDOMException(&state, NOT_SUPPORTED_ERR);
        return jsUndefined();
    }

    auto algorithm = createAlgorithmFromJSValue(state, state.uncheckedArgument(3));
    if (!algorithm) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    auto parameters = JSCryptoAlgorithmDictionary::createParametersForEncrypt(&state, algorithm->identifier(), state.uncheckedArgument(3));
    if (!parameters) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    JSPromiseDeferred* promiseDeferred = JSPromiseDeferred::create(&state, globalObject());
    DeferredWrapper wrapper(&state, globalObject(), promiseDeferred);

    auto exportSuccessCallback = [keyFormat, algorithm, parameters, wrappingKey, wrapper](const Vector<uint8_t>& exportedKeyData) mutable {
        auto encryptSuccessCallback = [wrapper](const Vector<uint8_t>& encryptedData) mutable {
            fulfillPromiseWithArrayBuffer(wrapper, encryptedData.data(), encryptedData.size());
        };
        auto encryptFailureCallback = [wrapper]() mutable {
            wrapper.reject(nullptr);
        };
        ExceptionCode ec = 0;
        algorithm->encryptForWrapKey(*parameters, *wrappingKey, std::make_pair(exportedKeyData.data(), exportedKeyData.size()), WTFMove(encryptSuccessCallback), WTFMove(encryptFailureCallback), ec);
        if (ec) {
            // FIXME: Report failure details to console, and possibly to calling script once there is a standardized way to pass errors to WebCrypto promise reject functions.
            wrapper.reject(nullptr);
        }
    };

    auto exportFailureCallback = [wrapper]() mutable {
        wrapper.reject(nullptr);
    };

    ExceptionCode ec = 0;
    WebCore::exportKey(state, keyFormat, *key, WTFMove(exportSuccessCallback), WTFMove(exportFailureCallback));
    if (ec) {
        setDOMException(&state, ec);
        return jsUndefined();
    }

    return promiseDeferred->promise();
}

JSValue JSWebKitSubtleCrypto::unwrapKey(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() < 5)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    CryptoKeyFormat keyFormat;
    if (!cryptoKeyFormatFromJSValue(state, state.argument(0), keyFormat)) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    CryptoOperationData wrappedKeyData;
    if (!cryptoOperationDataFromJSValue(&state, state.uncheckedArgument(1), wrappedKeyData)) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    RefPtr<CryptoKey> unwrappingKey = JSCryptoKey::toWrapped(state.uncheckedArgument(2));
    if (!unwrappingKey)
        return throwTypeError(&state, scope);

    if (!unwrappingKey->allows(CryptoKeyUsageUnwrapKey)) {
        wrapped().document()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, ASCIILiteral("Key usages do not include 'unwrapKey'"));
        setDOMException(&state, NOT_SUPPORTED_ERR);
        return jsUndefined();
    }

    auto unwrapAlgorithm = createAlgorithmFromJSValue(state, state.uncheckedArgument(3));
    if (!unwrapAlgorithm) {
        ASSERT(state.hadException());
        return jsUndefined();
    }
    auto unwrapAlgorithmParameters = JSCryptoAlgorithmDictionary::createParametersForDecrypt(&state, unwrapAlgorithm->identifier(), state.uncheckedArgument(3));
    if (!unwrapAlgorithmParameters) {
        ASSERT(state.hadException());
        return jsUndefined();
    }

    RefPtr<CryptoAlgorithm> unwrappedKeyAlgorithm;
    RefPtr<CryptoAlgorithmParameters> unwrappedKeyAlgorithmParameters;
    if (!state.uncheckedArgument(4).isNull()) {
        unwrappedKeyAlgorithm = createAlgorithmFromJSValue(state, state.uncheckedArgument(4));
        if (!unwrappedKeyAlgorithm) {
            ASSERT(state.hadException());
            return jsUndefined();
        }
        unwrappedKeyAlgorithmParameters = JSCryptoAlgorithmDictionary::createParametersForImportKey(&state, unwrappedKeyAlgorithm->identifier(), state.uncheckedArgument(4));
        if (!unwrappedKeyAlgorithmParameters) {
            ASSERT(state.hadException());
            return jsUndefined();
        }
    }

    bool extractable = false;
    if (state.argumentCount() >= 6) {
        extractable = state.uncheckedArgument(5).toBoolean(&state);
        if (state.hadException())
            return jsUndefined();
    }

    CryptoKeyUsage keyUsages = 0;
    if (state.argumentCount() >= 7) {
        if (!cryptoKeyUsagesFromJSValue(state, state.argument(6), keyUsages)) {
            ASSERT(state.hadException());
            return jsUndefined();
        }
    }

    JSPromiseDeferred* promiseDeferred = JSPromiseDeferred::create(&state, globalObject());
    DeferredWrapper wrapper(&state, globalObject(), promiseDeferred);
    Strong<JSDOMGlobalObject> domGlobalObject(state.vm(), globalObject());

    auto decryptSuccessCallback = [domGlobalObject, keyFormat, unwrappedKeyAlgorithm, unwrappedKeyAlgorithmParameters, extractable, keyUsages, wrapper](const Vector<uint8_t>& result) mutable {
        auto importSuccessCallback = [wrapper](CryptoKey& key) mutable {
            wrapper.resolve(key);
        };
        auto importFailureCallback = [wrapper]() mutable {
            wrapper.reject(nullptr);
        };
        ExecState& state = *domGlobalObject->globalExec();
        WebCore::importKey(state, keyFormat, std::make_pair(result.data(), result.size()), unwrappedKeyAlgorithm, unwrappedKeyAlgorithmParameters, extractable, keyUsages, WTFMove(importSuccessCallback), WTFMove(importFailureCallback));
        if (state.hadException()) {
            // FIXME: Report exception details to console, and possibly to calling script once there is a standardized way to pass errors to WebCrypto promise reject functions.
            state.clearException();
            wrapper.reject(nullptr);
        }
    };

    auto decryptFailureCallback = [wrapper]() mutable {
        wrapper.reject(nullptr);
    };

    ExceptionCode ec = 0;
    unwrapAlgorithm->decryptForUnwrapKey(*unwrapAlgorithmParameters, *unwrappingKey, wrappedKeyData, WTFMove(decryptSuccessCallback), WTFMove(decryptFailureCallback), ec);
    if (ec) {
        setDOMException(&state, ec);
        return jsUndefined();
    }

    return promiseDeferred->promise();
}

} // namespace WebCore

#endif

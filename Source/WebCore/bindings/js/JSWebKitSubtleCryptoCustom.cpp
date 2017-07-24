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

#include "BufferSource.h"
#include "CryptoAlgorithm.h"
#include "CryptoAlgorithmParametersDeprecated.h"
#include "CryptoAlgorithmRegistry.h"
#include "CryptoKeyData.h"
#include "CryptoKeySerializationRaw.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "JSCryptoAlgorithmDictionary.h"
#include "JSCryptoKey.h"
#include "JSCryptoKeyPair.h"
#include "JSCryptoKeySerializationJWK.h"
#include "JSDOMPromise.h"
#include "ScriptState.h"
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

static RefPtr<CryptoAlgorithm> createAlgorithmFromJSValue(ExecState& state, ThrowScope& scope, JSValue value)
{
    auto algorithmIdentifier = JSCryptoAlgorithmDictionary::parseAlgorithmIdentifier(state, scope, value);
    RETURN_IF_EXCEPTION(scope, { });

    auto result = CryptoAlgorithmRegistry::singleton().create(algorithmIdentifier);
    if (!result)
        throwNotSupportedError(state, scope);

    return result;
}

static CryptoKeyFormat cryptoKeyFormatFromJSValue(ExecState& state, ThrowScope& scope, JSValue value)
{
    auto keyFormatString = value.toWTFString(&state);
    RETURN_IF_EXCEPTION(scope, { });

    if (keyFormatString == "raw")
        return CryptoKeyFormat::Raw;
    if (keyFormatString == "pkcs8")
        return CryptoKeyFormat::PKCS8;
    if (keyFormatString == "spki")
        return CryptoKeyFormat::SPKI;
    if (keyFormatString == "jwk")
        return CryptoKeyFormat::JWK;

    throwTypeError(&state, scope, ASCIILiteral("Unknown key format"));
    return { };
}

static CryptoKeyUsageBitmap cryptoKeyUsagesFromJSValue(ExecState& state, ThrowScope& scope, JSValue value)
{
    if (!isJSArray(value)) {
        throwTypeError(&state, scope);
        return { };
    }

    CryptoKeyUsageBitmap result = 0;
    JSArray* array = asArray(value);
    for (unsigned i = 0; i < array->length(); ++i) {
        auto usageString = array->getIndex(&state, i).toWTFString(&state);
        RETURN_IF_EXCEPTION(scope, { });
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
    return result;
}

JSValue JSWebKitSubtleCrypto::encrypt(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() < 3)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    auto algorithm = createAlgorithmFromJSValue(state, scope, state.uncheckedArgument(0));
    RETURN_IF_EXCEPTION(scope, { });

    auto parameters = JSCryptoAlgorithmDictionary::createParametersForEncrypt(state, scope, algorithm->identifier(), state.uncheckedArgument(0));
    RETURN_IF_EXCEPTION(scope, { });

    RefPtr<CryptoKey> key = JSCryptoKey::toWrapped(vm, state.uncheckedArgument(1));
    if (!key)
        return throwTypeError(&state, scope);

    if (!key->allows(CryptoKeyUsageEncrypt)) {
        wrapped().document()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, ASCIILiteral("Key usages do not include 'encrypt'"));
        throwNotSupportedError(state, scope);
        return jsUndefined();
    }

    auto data = BufferSource(convert<IDLBufferSource>(state, state.uncheckedArgument(2)));
    RETURN_IF_EXCEPTION(scope, { });

    RefPtr<DeferredPromise> wrapper = createDeferredPromise(state, domWindow());
    auto promise = wrapper->promise();
    auto successCallback = [wrapper](const Vector<uint8_t>& result) mutable {
        fulfillPromiseWithArrayBuffer(wrapper.releaseNonNull(), result.data(), result.size());
    };
    auto failureCallback = [wrapper]() mutable {
        wrapper->reject(); // FIXME: This should reject with an Exception.
    };

    auto result = algorithm->encrypt(*parameters, *key, { data.data(), data.length() }, WTFMove(successCallback), WTFMove(failureCallback));
    if (result.hasException()) {
        propagateException(state, scope, result.releaseException());
        return { };
    }

    return promise;
}

JSValue JSWebKitSubtleCrypto::decrypt(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() < 3)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    auto algorithm = createAlgorithmFromJSValue(state, scope, state.uncheckedArgument(0));
    RETURN_IF_EXCEPTION(scope, { });

    auto parameters = JSCryptoAlgorithmDictionary::createParametersForDecrypt(state, scope, algorithm->identifier(), state.uncheckedArgument(0));
    RETURN_IF_EXCEPTION(scope, { });

    RefPtr<CryptoKey> key = JSCryptoKey::toWrapped(vm, state.uncheckedArgument(1));
    if (!key)
        return throwTypeError(&state, scope);

    if (!key->allows(CryptoKeyUsageDecrypt)) {
        wrapped().document()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, ASCIILiteral("Key usages do not include 'decrypt'"));
        throwNotSupportedError(state, scope);
        return jsUndefined();
    }

    auto data = BufferSource(convert<IDLBufferSource>(state, state.uncheckedArgument(2)));
    RETURN_IF_EXCEPTION(scope, { });

    RefPtr<DeferredPromise> wrapper = createDeferredPromise(state, domWindow());
    auto promise = wrapper->promise();
    auto successCallback = [wrapper](const Vector<uint8_t>& result) mutable {
        fulfillPromiseWithArrayBuffer(wrapper.releaseNonNull(), result.data(), result.size());
    };
    auto failureCallback = [wrapper]() mutable {
        wrapper->reject(); // FIXME: This should reject with an Exception.
    };

    auto result = algorithm->decrypt(*parameters, *key, { data.data(), data.length() }, WTFMove(successCallback), WTFMove(failureCallback));
    if (result.hasException()) {
        propagateException(state, scope, result.releaseException());
        return { };
    }

    return promise;
}

JSValue JSWebKitSubtleCrypto::sign(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() < 3)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    auto algorithm = createAlgorithmFromJSValue(state, scope, state.uncheckedArgument(0));
    RETURN_IF_EXCEPTION(scope, { });

    auto parameters = JSCryptoAlgorithmDictionary::createParametersForSign(state, scope, algorithm->identifier(), state.uncheckedArgument(0));
    RETURN_IF_EXCEPTION(scope, { });

    RefPtr<CryptoKey> key = JSCryptoKey::toWrapped(vm, state.uncheckedArgument(1));
    if (!key)
        return throwTypeError(&state, scope);

    if (!key->allows(CryptoKeyUsageSign)) {
        wrapped().document()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, ASCIILiteral("Key usages do not include 'sign'"));
        throwNotSupportedError(state, scope);
        return jsUndefined();
    }

    auto data = BufferSource(convert<IDLBufferSource>(state, state.uncheckedArgument(2)));
    RETURN_IF_EXCEPTION(scope, { });

    RefPtr<DeferredPromise> wrapper = createDeferredPromise(state, domWindow());
    auto promise = wrapper->promise();
    auto successCallback = [wrapper](const Vector<uint8_t>& result) mutable {
        fulfillPromiseWithArrayBuffer(wrapper.releaseNonNull(), result.data(), result.size());
    };
    auto failureCallback = [wrapper]() mutable {
        wrapper->reject(); // FIXME: This should reject with an Exception.
    };

    auto result = algorithm->sign(*parameters, *key, { data.data(), data.length() }, WTFMove(successCallback), WTFMove(failureCallback));
    if (result.hasException()) {
        propagateException(state, scope, result.releaseException());
        return { };
    }

    return promise;
}

JSValue JSWebKitSubtleCrypto::verify(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() < 4)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    auto algorithm = createAlgorithmFromJSValue(state, scope, state.uncheckedArgument(0));
    RETURN_IF_EXCEPTION(scope, { });

    auto parameters = JSCryptoAlgorithmDictionary::createParametersForVerify(state, scope, algorithm->identifier(), state.uncheckedArgument(0));
    RETURN_IF_EXCEPTION(scope, { });

    RefPtr<CryptoKey> key = JSCryptoKey::toWrapped(vm, state.uncheckedArgument(1));
    if (!key)
        return throwTypeError(&state, scope);

    if (!key->allows(CryptoKeyUsageVerify)) {
        wrapped().document()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, ASCIILiteral("Key usages do not include 'verify'"));
        throwNotSupportedError(state, scope);
        return jsUndefined();
    }

    auto signature = BufferSource(convert<IDLBufferSource>(state, state.uncheckedArgument(2)));
    RETURN_IF_EXCEPTION(scope, { });

    auto data = BufferSource(convert<IDLBufferSource>(state, state.uncheckedArgument(3)));
    RETURN_IF_EXCEPTION(scope, { });

    RefPtr<DeferredPromise> wrapper = createDeferredPromise(state, domWindow());
    auto promise = wrapper->promise();
    auto successCallback = [wrapper](bool result) mutable {
        wrapper->resolve<IDLBoolean>(result);
    };
    auto failureCallback = [wrapper]() mutable {
        wrapper->reject(); // FIXME: This should reject with an Exception.
    };

    auto result = algorithm->verify(*parameters, *key, { signature.data(), signature.length() }, { data.data(), data.length() }, WTFMove(successCallback), WTFMove(failureCallback));
    if (result.hasException()) {
        propagateException(state, scope, result.releaseException());
        return { };
    }

    return promise;
}

JSValue JSWebKitSubtleCrypto::digest(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() < 2)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    auto algorithm = createAlgorithmFromJSValue(state, scope, state.uncheckedArgument(0));
    RETURN_IF_EXCEPTION(scope, { });

    auto parameters = JSCryptoAlgorithmDictionary::createParametersForDigest(state, scope, algorithm->identifier(), state.uncheckedArgument(0));
    RETURN_IF_EXCEPTION(scope, { });

    auto data = BufferSource(convert<IDLBufferSource>(state, state.uncheckedArgument(1)));
    RETURN_IF_EXCEPTION(scope, { });

    RefPtr<DeferredPromise> wrapper = createDeferredPromise(state, domWindow());
    auto promise = wrapper->promise();
    auto successCallback = [wrapper](const Vector<uint8_t>& result) mutable {
        fulfillPromiseWithArrayBuffer(wrapper.releaseNonNull(), result.data(), result.size());
    };
    auto failureCallback = [wrapper]() mutable {
        wrapper->reject(); // FIXME: This should reject with an Exception.
    };

    auto result = algorithm->digest(*parameters, { data.data(), data.length() }, WTFMove(successCallback), WTFMove(failureCallback));
    if (result.hasException()) {
        propagateException(state, scope, result.releaseException());
        return { };
    }

    return promise;
}

JSValue JSWebKitSubtleCrypto::generateKey(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() < 1)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    auto algorithm = createAlgorithmFromJSValue(state, scope, state.uncheckedArgument(0));
    RETURN_IF_EXCEPTION(scope, { });

    auto parameters = JSCryptoAlgorithmDictionary::createParametersForGenerateKey(state, scope, algorithm->identifier(), state.uncheckedArgument(0));
    RETURN_IF_EXCEPTION(scope, { });

    bool extractable = state.argument(1).toBoolean(&state);
    RETURN_IF_EXCEPTION(scope, { });

    CryptoKeyUsageBitmap keyUsages = 0;
    if (state.argumentCount() >= 3) {
        keyUsages = cryptoKeyUsagesFromJSValue(state, scope, state.uncheckedArgument(2));
        RETURN_IF_EXCEPTION(scope, { });
    }

    RefPtr<DeferredPromise> wrapper = createDeferredPromise(state, domWindow());
    auto promise = wrapper->promise();
    auto successCallback = [wrapper](KeyOrKeyPair&& keyOrKeyPair) mutable {
        WTF::switchOn(keyOrKeyPair,
            [&wrapper] (RefPtr<CryptoKey>& key) {
                wrapper->resolve<IDLInterface<CryptoKey>>(*key);
            },
            [&wrapper] (CryptoKeyPair& keyPair) {
                wrapper->resolve<IDLDictionary<CryptoKeyPair>>(keyPair);
            }
        );
    };
    auto failureCallback = [wrapper]() mutable {
        wrapper->reject(); // FIXME: This should reject with an Exception.
    };

    auto result = algorithm->generateKey(*parameters, extractable, keyUsages, WTFMove(successCallback), WTFMove(failureCallback), *scriptExecutionContextFromExecState(&state));
    if (result.hasException()) {
        propagateException(state, scope, result.releaseException());
        return { };
    }

    return promise;
}

static void importKey(ExecState& state, CryptoKeyFormat keyFormat, CryptoOperationData data, RefPtr<CryptoAlgorithm> algorithm, RefPtr<CryptoAlgorithmParametersDeprecated> parameters, bool extractable, CryptoKeyUsageBitmap keyUsages, CryptoAlgorithm::KeyCallback callback, CryptoAlgorithm::VoidCallback failureCallback)
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
        RETURN_IF_EXCEPTION(scope, void());
        break;
    }
    default:
        throwTypeError(&state, scope, ASCIILiteral("Unsupported key format for import"));
        return;
    }

    ASSERT(keySerialization);

    std::optional<CryptoAlgorithmPair> reconciledResult = keySerialization->reconcileAlgorithm(algorithm.get(), parameters.get());
    if (!reconciledResult) {
        if (!scope.exception())
            throwTypeError(&state, scope, ASCIILiteral("Algorithm specified in key is not compatible with one passed to importKey as argument"));
        return;
    }
    RETURN_IF_EXCEPTION(scope, void());

    algorithm = reconciledResult->algorithm;
    parameters = reconciledResult->parameters;
    if (!algorithm) {
        throwTypeError(&state, scope, ASCIILiteral("Neither key nor function argument has crypto algorithm specified"));
        return;
    }
    ASSERT(parameters);

    keySerialization->reconcileExtractable(extractable);
    RETURN_IF_EXCEPTION(scope, void());

    keySerialization->reconcileUsages(keyUsages);
    RETURN_IF_EXCEPTION(scope, void());

    auto keyData = keySerialization->keyData();
    RETURN_IF_EXCEPTION(scope, void());

    propagateException(state, scope, algorithm->importKey(*parameters, *keyData, extractable, keyUsages, WTFMove(callback), WTFMove(failureCallback)));
}

JSValue JSWebKitSubtleCrypto::importKey(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() < 3)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    auto keyFormat = cryptoKeyFormatFromJSValue(state, scope, state.uncheckedArgument(0));
    RETURN_IF_EXCEPTION(scope, { });

    auto data = BufferSource(convert<IDLBufferSource>(state, state.uncheckedArgument(1)));
    RETURN_IF_EXCEPTION(scope, { });

    RefPtr<CryptoAlgorithm> algorithm;
    RefPtr<CryptoAlgorithmParametersDeprecated> parameters;
    if (!state.uncheckedArgument(2).isNull()) {
        algorithm = createAlgorithmFromJSValue(state, scope, state.uncheckedArgument(2));
        RETURN_IF_EXCEPTION(scope, { });

        parameters = JSCryptoAlgorithmDictionary::createParametersForImportKey(state, scope, algorithm->identifier(), state.uncheckedArgument(2));
        RETURN_IF_EXCEPTION(scope, { });
    }

    bool extractable = state.argument(3).toBoolean(&state);
    RETURN_IF_EXCEPTION(scope, JSValue());

    CryptoKeyUsageBitmap keyUsages = 0;
    if (state.argumentCount() >= 5) {
        keyUsages = cryptoKeyUsagesFromJSValue(state, scope, state.uncheckedArgument(4));
        RETURN_IF_EXCEPTION(scope, { });
    }

    RefPtr<DeferredPromise> wrapper = createDeferredPromise(state, domWindow());
    auto promise = wrapper->promise();
    auto successCallback = [wrapper](CryptoKey& result) mutable {
        wrapper->resolve<IDLInterface<CryptoKey>>(result);
    };
    auto failureCallback = [wrapper]() mutable {
        wrapper->reject(); // FIXME: This should reject with an Exception.
    };

    WebCore::importKey(state, keyFormat, { data.data(), data.length() }, WTFMove(algorithm), WTFMove(parameters), extractable, keyUsages, WTFMove(successCallback), WTFMove(failureCallback));
    RETURN_IF_EXCEPTION(scope, JSValue());

    return promise;
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
        RETURN_IF_EXCEPTION(scope, void());
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

    auto keyFormat = cryptoKeyFormatFromJSValue(state, scope, state.uncheckedArgument(0));
    RETURN_IF_EXCEPTION(scope, { });

    RefPtr<CryptoKey> key = JSCryptoKey::toWrapped(vm, state.uncheckedArgument(1));
    if (!key)
        return throwTypeError(&state, scope);

    RefPtr<DeferredPromise> wrapper = createDeferredPromise(state, domWindow());
    auto promise = wrapper->promise();
    auto successCallback = [wrapper](const Vector<uint8_t>& result) mutable {
        fulfillPromiseWithArrayBuffer(wrapper.releaseNonNull(), result.data(), result.size());
    };
    auto failureCallback = [wrapper]() mutable {
        wrapper->reject(); // FIXME: This should reject with an Exception.
    };

    WebCore::exportKey(state, keyFormat, *key, WTFMove(successCallback), WTFMove(failureCallback));
    RETURN_IF_EXCEPTION(scope, JSValue());

    return promise;
}

JSValue JSWebKitSubtleCrypto::wrapKey(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() < 4)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    auto keyFormat = cryptoKeyFormatFromJSValue(state, scope, state.uncheckedArgument(0));
    RETURN_IF_EXCEPTION(scope, { });

    RefPtr<CryptoKey> key = JSCryptoKey::toWrapped(vm, state.uncheckedArgument(1));
    if (!key)
        return throwTypeError(&state, scope);

    RefPtr<CryptoKey> wrappingKey = JSCryptoKey::toWrapped(vm, state.uncheckedArgument(2));
    if (!key)
        return throwTypeError(&state, scope);

    if (!wrappingKey->allows(CryptoKeyUsageWrapKey)) {
        wrapped().document()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, ASCIILiteral("Key usages do not include 'wrapKey'"));
        throwNotSupportedError(state, scope);
        return jsUndefined();
    }

    auto algorithm = createAlgorithmFromJSValue(state, scope, state.uncheckedArgument(3));
    RETURN_IF_EXCEPTION(scope, { });

    auto parameters = JSCryptoAlgorithmDictionary::createParametersForEncrypt(state, scope, algorithm->identifier(), state.uncheckedArgument(3));
    RETURN_IF_EXCEPTION(scope, { });

    RefPtr<DeferredPromise> wrapper = createDeferredPromise(state, domWindow());
    auto promise = wrapper->promise();

    auto exportSuccessCallback = [keyFormat, algorithm, parameters, wrappingKey, wrapper](const Vector<uint8_t>& exportedKeyData) mutable {
        auto encryptSuccessCallback = [wrapper](const Vector<uint8_t>& encryptedData) mutable {
            fulfillPromiseWithArrayBuffer(wrapper.releaseNonNull(), encryptedData.data(), encryptedData.size());
        };
        auto encryptFailureCallback = [wrapper]() mutable {
        wrapper->reject(); // FIXME: This should reject with an Exception.
        };
        auto result = algorithm->encryptForWrapKey(*parameters, *wrappingKey, std::make_pair(exportedKeyData.data(), exportedKeyData.size()), WTFMove(encryptSuccessCallback), WTFMove(encryptFailureCallback));
        if (result.hasException()) {
            // FIXME: Report failure details to console, and possibly to calling script once there is a standardized way to pass errors to WebCrypto promise reject functions.
            wrapper->reject(); // FIXME: This should reject with an Exception.
        }
    };

    auto exportFailureCallback = [wrapper]() mutable {
        wrapper->reject(); // FIXME: This should reject with an Exception.
    };

    WebCore::exportKey(state, keyFormat, *key, WTFMove(exportSuccessCallback), WTFMove(exportFailureCallback));

    return promise;
}

JSValue JSWebKitSubtleCrypto::unwrapKey(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() < 5)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    auto keyFormat = cryptoKeyFormatFromJSValue(state, scope, state.uncheckedArgument(0));
    RETURN_IF_EXCEPTION(scope, { });

    auto wrappedKeyData = BufferSource(convert<IDLBufferSource>(state, state.uncheckedArgument(1)));
    RETURN_IF_EXCEPTION(scope, { });

    RefPtr<CryptoKey> unwrappingKey = JSCryptoKey::toWrapped(vm, state.uncheckedArgument(2));
    if (!unwrappingKey)
        return throwTypeError(&state, scope);

    if (!unwrappingKey->allows(CryptoKeyUsageUnwrapKey)) {
        wrapped().document()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, ASCIILiteral("Key usages do not include 'unwrapKey'"));
        throwNotSupportedError(state, scope);
        return jsUndefined();
    }

    auto unwrapAlgorithm = createAlgorithmFromJSValue(state, scope, state.uncheckedArgument(3));
    RETURN_IF_EXCEPTION(scope, { });

    auto unwrapAlgorithmParameters = JSCryptoAlgorithmDictionary::createParametersForDecrypt(state, scope, unwrapAlgorithm->identifier(), state.uncheckedArgument(3));
    RETURN_IF_EXCEPTION(scope, { });

    RefPtr<CryptoAlgorithm> unwrappedKeyAlgorithm;
    RefPtr<CryptoAlgorithmParametersDeprecated> unwrappedKeyAlgorithmParameters;
    if (!state.uncheckedArgument(4).isNull()) {
        unwrappedKeyAlgorithm = createAlgorithmFromJSValue(state, scope, state.uncheckedArgument(4));
        RETURN_IF_EXCEPTION(scope, { });

        unwrappedKeyAlgorithmParameters = JSCryptoAlgorithmDictionary::createParametersForImportKey(state, scope, unwrappedKeyAlgorithm->identifier(), state.uncheckedArgument(4));
        RETURN_IF_EXCEPTION(scope, { });
    }

    bool extractable = state.argument(5).toBoolean(&state);
    RETURN_IF_EXCEPTION(scope, { });

    CryptoKeyUsageBitmap keyUsages = 0;
    if (state.argumentCount() >= 7) {
        keyUsages = cryptoKeyUsagesFromJSValue(state, scope, state.uncheckedArgument(6));
        RETURN_IF_EXCEPTION(scope, { });
    }

    RefPtr<DeferredPromise> wrapper = createDeferredPromise(state, domWindow());
    auto promise = wrapper->promise();
    Strong<JSDOMGlobalObject> domGlobalObject(state.vm(), globalObject());

    auto decryptSuccessCallback = [domGlobalObject, keyFormat, unwrappedKeyAlgorithm, unwrappedKeyAlgorithmParameters, extractable, keyUsages, wrapper](const Vector<uint8_t>& result) mutable {
        auto importSuccessCallback = [wrapper](CryptoKey& key) mutable {
            wrapper->resolve<IDLInterface<CryptoKey>>(key);
        };
        auto importFailureCallback = [wrapper]() mutable {
            wrapper->reject(); // FIXME: This should reject with an Exception.
        };

        VM& vm = domGlobalObject->vm();
        auto scope = DECLARE_CATCH_SCOPE(vm);

        ExecState& state = *domGlobalObject->globalExec();
        WebCore::importKey(state, keyFormat, std::make_pair(result.data(), result.size()), unwrappedKeyAlgorithm, unwrappedKeyAlgorithmParameters, extractable, keyUsages, WTFMove(importSuccessCallback), WTFMove(importFailureCallback));
        if (UNLIKELY(scope.exception())) {
            // FIXME: Report exception details to console, and possibly to calling script once there is a standardized way to pass errors to WebCrypto promise reject functions.
            scope.clearException();
            wrapper->reject(); // FIXME: This should reject with an Exception.
        }
    };

    auto decryptFailureCallback = [wrapper]() mutable {
        wrapper->reject(); // FIXME: This should reject with an Exception.
    };

    auto result = unwrapAlgorithm->decryptForUnwrapKey(*unwrapAlgorithmParameters, *unwrappingKey, { wrappedKeyData.data(), wrappedKeyData.length() }, WTFMove(decryptSuccessCallback), WTFMove(decryptFailureCallback));
    if (result.hasException()) {
        propagateException(state, scope, result.releaseException());
        return { };
    }

    return promise;
}

} // namespace WebCore

#endif

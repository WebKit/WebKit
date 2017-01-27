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
#include "JSCryptoAlgorithmDictionary.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithmAesCbcParamsDeprecated.h"
#include "CryptoAlgorithmAesKeyGenParamsDeprecated.h"
#include "CryptoAlgorithmHmacKeyParamsDeprecated.h"
#include "CryptoAlgorithmHmacParamsDeprecated.h"
#include "CryptoAlgorithmParameters.h"
#include "CryptoAlgorithmRegistry.h"
#include "CryptoAlgorithmRsaKeyGenParamsDeprecated.h"
#include "CryptoAlgorithmRsaKeyParamsWithHashDeprecated.h"
#include "CryptoAlgorithmRsaOaepParamsDeprecated.h"
#include "CryptoAlgorithmRsaSsaParamsDeprecated.h"
#include "ExceptionCode.h"
#include "JSCryptoOperationData.h"
#include "JSDOMBinding.h"
#include "JSDOMConvert.h"

using namespace JSC;

namespace WebCore {

static inline JSValue getProperty(ExecState& state, JSObject* object, const char* name)
{
    return object->get(&state, Identifier::fromString(&state, name));
}

CryptoAlgorithmIdentifier JSCryptoAlgorithmDictionary::parseAlgorithmIdentifier(ExecState& state, ThrowScope& scope, JSValue value)
{
    // typedef (Algorithm or DOMString) AlgorithmIdentifier;

    String algorithmName;
    VM& vm = state.vm();

    if (value.isString()) {
        algorithmName = asString(value)->value(&state);
        RETURN_IF_EXCEPTION(scope, { });
    } else if (value.isObject()) {
        if (asObject(value)->inherits(vm, StringObject::info())) {
            algorithmName = asStringObject(value)->internalValue()->value(&state);
            RETURN_IF_EXCEPTION(scope, { });
        } else {
            // FIXME: This doesn't perform some checks mandated by WebIDL for dictionaries:
            // - null and undefined input should be treated as if all elements in the dictionary were undefined;
            // - undefined elements should be treated as having a default value, or as not present if there isn't such;
            // - RegExp and Date objects cannot be converted to dictionaries.
            //
            // This is partially because we don't implement it elsewhere in WebCore yet, and partially because
            // WebCrypto doesn't yet clearly specify what to do with non-present values in most cases anyway.

            auto nameValue = getProperty(state, asObject(value), "name");
            RETURN_IF_EXCEPTION(scope, { });

            algorithmName = convert<IDLDOMString>(state, nameValue);
            RETURN_IF_EXCEPTION(scope, { });
        }
    }

    if (!algorithmName.containsOnlyASCII()) {
        throwSyntaxError(&state, scope);
        return { };
    }

    auto identifier = CryptoAlgorithmRegistry::singleton().identifier(algorithmName);
    if (!identifier) {
        throwNotSupportedError(state, scope);
        return { };
    }

    return identifier.value();
}

static std::optional<CryptoAlgorithmIdentifier> optionalHashAlgorithm(ExecState& state, ThrowScope& scope, JSObject* object)
{
    auto hash = getProperty(state, object, "hash");
    RETURN_IF_EXCEPTION(scope, { });

    if (hash.isUndefinedOrNull())
        return std::nullopt;

    return JSCryptoAlgorithmDictionary::parseAlgorithmIdentifier(state, scope, hash);
}

static CryptoAlgorithmIdentifier requiredHashAlgorithm(ExecState& state, ThrowScope& scope, JSObject* object)
{
    auto result = optionalHashAlgorithm(state, scope, object);
    RETURN_IF_EXCEPTION(scope, { });

    if (!result) {
        throwNotSupportedError(state, scope);
        return { };
    }

    return result.value();
}

static RefPtr<CryptoAlgorithmParametersDeprecated> createAesCbcParams(ExecState& state, JSValue value)
{
    auto scope = DECLARE_THROW_SCOPE(state.vm());

    if (!value.isObject()) {
        throwTypeError(&state, scope);
        return nullptr;
    }

    auto iv = getProperty(state, asObject(value), "iv");
    RETURN_IF_EXCEPTION(scope, { });

    auto result = adoptRef(*new CryptoAlgorithmAesCbcParamsDeprecated);

    auto ivData = cryptoOperationDataFromJSValue(state, scope, iv);
    RETURN_IF_EXCEPTION(scope, { });

    if (ivData.second != 16) {
        throwException(&state, scope, createError(&state, "AES-CBC initialization data must be 16 bytes"));
        return nullptr;
    }

    memcpy(result->iv.data(), ivData.first, ivData.second);

    return WTFMove(result);
}

static RefPtr<CryptoAlgorithmParametersDeprecated> createAesKeyGenParams(ExecState& state, JSValue value)
{
    auto scope = DECLARE_THROW_SCOPE(state.vm());

    if (!value.isObject()) {
        throwTypeError(&state, scope);
        return nullptr;
    }

    auto result = adoptRef(*new CryptoAlgorithmAesKeyGenParamsDeprecated);

    auto lengthValue = getProperty(state, asObject(value), "length");
    RETURN_IF_EXCEPTION(scope, nullptr);

    result->length = convert<IDLUnsignedShort>(state, lengthValue, IntegerConversionConfiguration::EnforceRange);

    return WTFMove(result);
}

static RefPtr<CryptoAlgorithmParametersDeprecated> createHmacParams(ExecState& state, JSValue value)
{
    auto scope = DECLARE_THROW_SCOPE(state.vm());

    if (!value.isObject()) {
        throwTypeError(&state, scope);
        return nullptr;
    }

    auto result = adoptRef(*new CryptoAlgorithmHmacParamsDeprecated);

    result->hash = requiredHashAlgorithm(state, scope, asObject(value));
    RETURN_IF_EXCEPTION(scope, { });

    return WTFMove(result);
}

static RefPtr<CryptoAlgorithmParametersDeprecated> createHmacKeyParams(ExecState& state, JSValue value)
{
    auto scope = DECLARE_THROW_SCOPE(state.vm());

    if (!value.isObject()) {
        throwTypeError(&state, scope);
        return nullptr;
    }

    auto result = adoptRef(*new CryptoAlgorithmHmacKeyParamsDeprecated);

    result->hash = requiredHashAlgorithm(state, scope, asObject(value));
    RETURN_IF_EXCEPTION(scope, { });

    auto lengthValue = getProperty(state, asObject(value), "length");
    RETURN_IF_EXCEPTION(scope, nullptr);

    result->length = convert<IDLUnsignedShort>(state, lengthValue, IntegerConversionConfiguration::Normal);
    RETURN_IF_EXCEPTION(scope, nullptr);

    result->hasLength = true;

    return WTFMove(result);
}

static RefPtr<CryptoAlgorithmParametersDeprecated> createRsaKeyGenParams(ExecState& state, JSValue value)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!value.isObject()) {
        throwTypeError(&state, scope);
        return nullptr;
    }

    auto result = adoptRef(*new CryptoAlgorithmRsaKeyGenParamsDeprecated);

    auto modulusLengthValue = getProperty(state, asObject(value), "modulusLength");
    RETURN_IF_EXCEPTION(scope, nullptr);

    // FIXME: Why no EnforceRange? Filed as <https://www.w3.org/Bugs/Public/show_bug.cgi?id=23779>.
    result->modulusLength = convert<IDLUnsignedLong>(state, modulusLengthValue, IntegerConversionConfiguration::Normal);
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto publicExponentValue = getProperty(state, asObject(value), "publicExponent");
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto publicExponentArray = toUnsharedUint8Array(vm, publicExponentValue);
    if (!publicExponentArray) {
        throwTypeError(&state, scope, ASCIILiteral("Expected a Uint8Array in publicExponent"));
        return nullptr;
    }
    result->publicExponent.append(publicExponentArray->data(), publicExponentArray->byteLength());

    if (auto optionalHash = optionalHashAlgorithm(state, scope, asObject(value))) {
        result->hasHash = true;
        result->hash = optionalHash.value();
    }

    return WTFMove(result);
}

static RefPtr<CryptoAlgorithmParametersDeprecated> createRsaKeyParamsWithHash(ExecState&, JSValue)
{
    // WebCrypto RSA algorithms currently do not take any parameters to importKey.
    return adoptRef(*new CryptoAlgorithmRsaKeyParamsWithHashDeprecated);
}

static RefPtr<CryptoAlgorithmParametersDeprecated> createRsaOaepParams(ExecState& state, JSValue value)
{
    auto scope = DECLARE_THROW_SCOPE(state.vm());

    if (!value.isObject()) {
        throwTypeError(&state, scope);
        return nullptr;
    }

    auto result = adoptRef(*new CryptoAlgorithmRsaOaepParamsDeprecated);

    result->hash = requiredHashAlgorithm(state, scope, asObject(value));
    RETURN_IF_EXCEPTION(scope, { });

    auto labelValue = getProperty(state, asObject(value), "label");
    RETURN_IF_EXCEPTION(scope, { });

    result->hasLabel = !labelValue.isUndefinedOrNull();
    if (!result->hasLabel)
        return WTFMove(result);

    auto labelData = cryptoOperationDataFromJSValue(state, scope, labelValue);
    RETURN_IF_EXCEPTION(scope, { });

    result->label.append(labelData.first, labelData.second);

    return WTFMove(result);
}

static RefPtr<CryptoAlgorithmParametersDeprecated> createRsaSsaParams(ExecState& state, JSValue value)
{
    auto scope = DECLARE_THROW_SCOPE(state.vm());

    if (!value.isObject()) {
        throwTypeError(&state, scope);
        return nullptr;
    }

    auto result = adoptRef(*new CryptoAlgorithmRsaSsaParamsDeprecated);

    result->hash = requiredHashAlgorithm(state, scope, asObject(value));
    RETURN_IF_EXCEPTION(scope, { });

    return WTFMove(result);
}

RefPtr<CryptoAlgorithmParametersDeprecated> JSCryptoAlgorithmDictionary::createParametersForEncrypt(ExecState& state, ThrowScope& scope, CryptoAlgorithmIdentifier algorithm, JSValue value)
{
    switch (algorithm) {
    case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
        return adoptRef(*new CryptoAlgorithmParametersDeprecated);
    case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSA_PSS:
        throwNotSupportedError(state, scope);
        return nullptr;
    case CryptoAlgorithmIdentifier::RSA_OAEP:
        return createRsaOaepParams(state, value);
    case CryptoAlgorithmIdentifier::ECDSA:
    case CryptoAlgorithmIdentifier::ECDH:
    case CryptoAlgorithmIdentifier::AES_CTR:
        throwNotSupportedError(state, scope);
        return nullptr;
    case CryptoAlgorithmIdentifier::AES_CBC:
        return createAesCbcParams(state, value);
    case CryptoAlgorithmIdentifier::AES_CMAC:
    case CryptoAlgorithmIdentifier::AES_GCM:
    case CryptoAlgorithmIdentifier::AES_CFB:
        throwNotSupportedError(state, scope);
        return nullptr;
    case CryptoAlgorithmIdentifier::AES_KW:
        return adoptRef(*new CryptoAlgorithmParametersDeprecated);
    case CryptoAlgorithmIdentifier::HMAC:
    case CryptoAlgorithmIdentifier::DH:
    case CryptoAlgorithmIdentifier::SHA_1:
    case CryptoAlgorithmIdentifier::SHA_224:
    case CryptoAlgorithmIdentifier::SHA_256:
    case CryptoAlgorithmIdentifier::SHA_384:
    case CryptoAlgorithmIdentifier::SHA_512:
    case CryptoAlgorithmIdentifier::CONCAT:
    case CryptoAlgorithmIdentifier::HKDF_CTR:
    case CryptoAlgorithmIdentifier::PBKDF2:
        throwNotSupportedError(state, scope);
        return nullptr;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<CryptoAlgorithmParametersDeprecated> JSCryptoAlgorithmDictionary::createParametersForDecrypt(ExecState& state, ThrowScope& scope, CryptoAlgorithmIdentifier algorithm, JSValue value)
{
    switch (algorithm) {
    case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
        return adoptRef(*new CryptoAlgorithmParametersDeprecated);
    case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSA_PSS:
        throwNotSupportedError(state, scope);
        return nullptr;
    case CryptoAlgorithmIdentifier::RSA_OAEP:
        return createRsaOaepParams(state, value);
    case CryptoAlgorithmIdentifier::ECDSA:
    case CryptoAlgorithmIdentifier::ECDH:
    case CryptoAlgorithmIdentifier::AES_CTR:
        throwNotSupportedError(state, scope);
        return nullptr;
    case CryptoAlgorithmIdentifier::AES_CBC:
        return createAesCbcParams(state, value);
    case CryptoAlgorithmIdentifier::AES_CMAC:
    case CryptoAlgorithmIdentifier::AES_GCM:
    case CryptoAlgorithmIdentifier::AES_CFB:
        throwNotSupportedError(state, scope);
        return nullptr;
    case CryptoAlgorithmIdentifier::AES_KW:
        return adoptRef(*new CryptoAlgorithmParametersDeprecated);
    case CryptoAlgorithmIdentifier::HMAC:
    case CryptoAlgorithmIdentifier::DH:
    case CryptoAlgorithmIdentifier::SHA_1:
    case CryptoAlgorithmIdentifier::SHA_224:
    case CryptoAlgorithmIdentifier::SHA_256:
    case CryptoAlgorithmIdentifier::SHA_384:
    case CryptoAlgorithmIdentifier::SHA_512:
    case CryptoAlgorithmIdentifier::CONCAT:
    case CryptoAlgorithmIdentifier::HKDF_CTR:
    case CryptoAlgorithmIdentifier::PBKDF2:
        throwNotSupportedError(state, scope);
        return nullptr;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<CryptoAlgorithmParametersDeprecated> JSCryptoAlgorithmDictionary::createParametersForSign(ExecState& state, ThrowScope& scope, CryptoAlgorithmIdentifier algorithm, JSValue value)
{
    switch (algorithm) {
    case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
        throwNotSupportedError(state, scope);
        return nullptr;
    case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5:
        return createRsaSsaParams(state, value);
    case CryptoAlgorithmIdentifier::RSA_PSS:
    case CryptoAlgorithmIdentifier::RSA_OAEP:
    case CryptoAlgorithmIdentifier::ECDSA:
    case CryptoAlgorithmIdentifier::ECDH:
    case CryptoAlgorithmIdentifier::AES_CTR:
    case CryptoAlgorithmIdentifier::AES_CBC:
    case CryptoAlgorithmIdentifier::AES_CMAC:
    case CryptoAlgorithmIdentifier::AES_GCM:
    case CryptoAlgorithmIdentifier::AES_CFB:
    case CryptoAlgorithmIdentifier::AES_KW:
        throwNotSupportedError(state, scope);
        return nullptr;
    case CryptoAlgorithmIdentifier::HMAC:
        return createHmacParams(state, value);
    case CryptoAlgorithmIdentifier::DH:
    case CryptoAlgorithmIdentifier::SHA_1:
    case CryptoAlgorithmIdentifier::SHA_224:
    case CryptoAlgorithmIdentifier::SHA_256:
    case CryptoAlgorithmIdentifier::SHA_384:
    case CryptoAlgorithmIdentifier::SHA_512:
    case CryptoAlgorithmIdentifier::CONCAT:
    case CryptoAlgorithmIdentifier::HKDF_CTR:
    case CryptoAlgorithmIdentifier::PBKDF2:
        throwNotSupportedError(state, scope);
        return nullptr;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<CryptoAlgorithmParametersDeprecated> JSCryptoAlgorithmDictionary::createParametersForVerify(ExecState& state, ThrowScope& scope, CryptoAlgorithmIdentifier algorithm, JSValue value)
{
    switch (algorithm) {
    case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
        throwNotSupportedError(state, scope);
        return nullptr;
    case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5:
        return createRsaSsaParams(state, value);
    case CryptoAlgorithmIdentifier::RSA_PSS:
    case CryptoAlgorithmIdentifier::RSA_OAEP:
    case CryptoAlgorithmIdentifier::ECDSA:
    case CryptoAlgorithmIdentifier::ECDH:
    case CryptoAlgorithmIdentifier::AES_CTR:
    case CryptoAlgorithmIdentifier::AES_CBC:
    case CryptoAlgorithmIdentifier::AES_CMAC:
    case CryptoAlgorithmIdentifier::AES_GCM:
    case CryptoAlgorithmIdentifier::AES_CFB:
    case CryptoAlgorithmIdentifier::AES_KW:
        throwNotSupportedError(state, scope);
        return nullptr;
    case CryptoAlgorithmIdentifier::HMAC:
        return createHmacParams(state, value);
    case CryptoAlgorithmIdentifier::DH:
    case CryptoAlgorithmIdentifier::SHA_1:
    case CryptoAlgorithmIdentifier::SHA_224:
    case CryptoAlgorithmIdentifier::SHA_256:
    case CryptoAlgorithmIdentifier::SHA_384:
    case CryptoAlgorithmIdentifier::SHA_512:
    case CryptoAlgorithmIdentifier::CONCAT:
    case CryptoAlgorithmIdentifier::HKDF_CTR:
    case CryptoAlgorithmIdentifier::PBKDF2:
        throwNotSupportedError(state, scope);
        return nullptr;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<CryptoAlgorithmParametersDeprecated> JSCryptoAlgorithmDictionary::createParametersForDigest(ExecState& state, ThrowScope& scope, CryptoAlgorithmIdentifier algorithm, JSValue)
{
    switch (algorithm) {
    case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSA_PSS:
    case CryptoAlgorithmIdentifier::RSA_OAEP:
    case CryptoAlgorithmIdentifier::ECDSA:
    case CryptoAlgorithmIdentifier::ECDH:
    case CryptoAlgorithmIdentifier::AES_CTR:
    case CryptoAlgorithmIdentifier::AES_CBC:
    case CryptoAlgorithmIdentifier::AES_CMAC:
    case CryptoAlgorithmIdentifier::AES_GCM:
    case CryptoAlgorithmIdentifier::AES_CFB:
    case CryptoAlgorithmIdentifier::AES_KW:
    case CryptoAlgorithmIdentifier::HMAC:
    case CryptoAlgorithmIdentifier::DH:
        throwNotSupportedError(state, scope);
        return nullptr;
    case CryptoAlgorithmIdentifier::SHA_1:
    case CryptoAlgorithmIdentifier::SHA_224:
    case CryptoAlgorithmIdentifier::SHA_256:
    case CryptoAlgorithmIdentifier::SHA_384:
    case CryptoAlgorithmIdentifier::SHA_512:
        return adoptRef(*new CryptoAlgorithmParametersDeprecated);
    case CryptoAlgorithmIdentifier::CONCAT:
    case CryptoAlgorithmIdentifier::HKDF_CTR:
    case CryptoAlgorithmIdentifier::PBKDF2:
        throwNotSupportedError(state, scope);
        return nullptr;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<CryptoAlgorithmParametersDeprecated> JSCryptoAlgorithmDictionary::createParametersForGenerateKey(ExecState& state, ThrowScope& scope, CryptoAlgorithmIdentifier algorithm, JSValue value)
{
    switch (algorithm) {
    case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSA_PSS:
    case CryptoAlgorithmIdentifier::RSA_OAEP:
        return createRsaKeyGenParams(state, value);
    case CryptoAlgorithmIdentifier::ECDSA:
    case CryptoAlgorithmIdentifier::ECDH:
        throwNotSupportedError(state, scope);
        return nullptr;
    case CryptoAlgorithmIdentifier::AES_CTR:
    case CryptoAlgorithmIdentifier::AES_CBC:
    case CryptoAlgorithmIdentifier::AES_CMAC:
    case CryptoAlgorithmIdentifier::AES_GCM:
    case CryptoAlgorithmIdentifier::AES_CFB:
    case CryptoAlgorithmIdentifier::AES_KW:
        return createAesKeyGenParams(state, value);
    case CryptoAlgorithmIdentifier::HMAC:
        return createHmacKeyParams(state, value);
    case CryptoAlgorithmIdentifier::DH:
    case CryptoAlgorithmIdentifier::SHA_1:
    case CryptoAlgorithmIdentifier::SHA_224:
    case CryptoAlgorithmIdentifier::SHA_256:
    case CryptoAlgorithmIdentifier::SHA_384:
    case CryptoAlgorithmIdentifier::SHA_512:
    case CryptoAlgorithmIdentifier::CONCAT:
    case CryptoAlgorithmIdentifier::HKDF_CTR:
    case CryptoAlgorithmIdentifier::PBKDF2:
        throwNotSupportedError(state, scope);
        return nullptr;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<CryptoAlgorithmParametersDeprecated> JSCryptoAlgorithmDictionary::createParametersForDeriveKey(ExecState& state, ThrowScope& scope, CryptoAlgorithmIdentifier algorithm, JSValue)
{
    switch (algorithm) {
    case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSA_PSS:
    case CryptoAlgorithmIdentifier::RSA_OAEP:
    case CryptoAlgorithmIdentifier::ECDSA:
    case CryptoAlgorithmIdentifier::ECDH:
    case CryptoAlgorithmIdentifier::AES_CTR:
    case CryptoAlgorithmIdentifier::AES_CBC:
    case CryptoAlgorithmIdentifier::AES_CMAC:
    case CryptoAlgorithmIdentifier::AES_GCM:
    case CryptoAlgorithmIdentifier::AES_CFB:
    case CryptoAlgorithmIdentifier::AES_KW:
    case CryptoAlgorithmIdentifier::HMAC:
    case CryptoAlgorithmIdentifier::DH:
    case CryptoAlgorithmIdentifier::SHA_1:
    case CryptoAlgorithmIdentifier::SHA_224:
    case CryptoAlgorithmIdentifier::SHA_256:
    case CryptoAlgorithmIdentifier::SHA_384:
    case CryptoAlgorithmIdentifier::SHA_512:
    case CryptoAlgorithmIdentifier::CONCAT:
    case CryptoAlgorithmIdentifier::HKDF_CTR:
    case CryptoAlgorithmIdentifier::PBKDF2:
        throwNotSupportedError(state, scope);
        return nullptr;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<CryptoAlgorithmParametersDeprecated> JSCryptoAlgorithmDictionary::createParametersForDeriveBits(ExecState& state, ThrowScope& scope, CryptoAlgorithmIdentifier algorithm, JSValue)
{
    switch (algorithm) {
    case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSA_PSS:
    case CryptoAlgorithmIdentifier::RSA_OAEP:
    case CryptoAlgorithmIdentifier::ECDSA:
    case CryptoAlgorithmIdentifier::ECDH:
    case CryptoAlgorithmIdentifier::AES_CTR:
    case CryptoAlgorithmIdentifier::AES_CBC:
    case CryptoAlgorithmIdentifier::AES_CMAC:
    case CryptoAlgorithmIdentifier::AES_GCM:
    case CryptoAlgorithmIdentifier::AES_CFB:
    case CryptoAlgorithmIdentifier::AES_KW:
    case CryptoAlgorithmIdentifier::HMAC:
    case CryptoAlgorithmIdentifier::DH:
    case CryptoAlgorithmIdentifier::SHA_1:
    case CryptoAlgorithmIdentifier::SHA_224:
    case CryptoAlgorithmIdentifier::SHA_256:
    case CryptoAlgorithmIdentifier::SHA_384:
    case CryptoAlgorithmIdentifier::SHA_512:
    case CryptoAlgorithmIdentifier::CONCAT:
    case CryptoAlgorithmIdentifier::HKDF_CTR:
    case CryptoAlgorithmIdentifier::PBKDF2:
        throwNotSupportedError(state, scope);
        return nullptr;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<CryptoAlgorithmParametersDeprecated> JSCryptoAlgorithmDictionary::createParametersForImportKey(ExecState& state, ThrowScope& scope, CryptoAlgorithmIdentifier algorithm, JSValue value)
{
    switch (algorithm) {
    case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSA_PSS:
    case CryptoAlgorithmIdentifier::RSA_OAEP:
        return createRsaKeyParamsWithHash(state, value);
    case CryptoAlgorithmIdentifier::ECDSA:
    case CryptoAlgorithmIdentifier::ECDH:
    case CryptoAlgorithmIdentifier::AES_CTR:
    case CryptoAlgorithmIdentifier::AES_CBC:
    case CryptoAlgorithmIdentifier::AES_CMAC:
    case CryptoAlgorithmIdentifier::AES_GCM:
    case CryptoAlgorithmIdentifier::AES_CFB:
    case CryptoAlgorithmIdentifier::AES_KW:
        return adoptRef(*new CryptoAlgorithmParametersDeprecated);
    case CryptoAlgorithmIdentifier::HMAC:
        return createHmacParams(state, value);
    case CryptoAlgorithmIdentifier::DH:
        return adoptRef(*new CryptoAlgorithmParametersDeprecated);
    case CryptoAlgorithmIdentifier::SHA_1:
    case CryptoAlgorithmIdentifier::SHA_224:
    case CryptoAlgorithmIdentifier::SHA_256:
    case CryptoAlgorithmIdentifier::SHA_384:
    case CryptoAlgorithmIdentifier::SHA_512:
    case CryptoAlgorithmIdentifier::CONCAT:
    case CryptoAlgorithmIdentifier::HKDF_CTR:
    case CryptoAlgorithmIdentifier::PBKDF2:
        throwNotSupportedError(state, scope);
        return nullptr;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<CryptoAlgorithmParametersDeprecated> JSCryptoAlgorithmDictionary::createParametersForExportKey(ExecState& state, ThrowScope& scope, CryptoAlgorithmIdentifier algorithm, JSValue)
{
    switch (algorithm) {
    case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSA_PSS:
    case CryptoAlgorithmIdentifier::RSA_OAEP:
    case CryptoAlgorithmIdentifier::ECDSA:
    case CryptoAlgorithmIdentifier::ECDH:
    case CryptoAlgorithmIdentifier::AES_CTR:
    case CryptoAlgorithmIdentifier::AES_CBC:
    case CryptoAlgorithmIdentifier::AES_CMAC:
    case CryptoAlgorithmIdentifier::AES_GCM:
    case CryptoAlgorithmIdentifier::AES_CFB:
    case CryptoAlgorithmIdentifier::AES_KW:
    case CryptoAlgorithmIdentifier::HMAC:
    case CryptoAlgorithmIdentifier::DH:
        return adoptRef(*new CryptoAlgorithmParametersDeprecated);
    case CryptoAlgorithmIdentifier::SHA_1:
    case CryptoAlgorithmIdentifier::SHA_224:
    case CryptoAlgorithmIdentifier::SHA_256:
    case CryptoAlgorithmIdentifier::SHA_384:
    case CryptoAlgorithmIdentifier::SHA_512:
    case CryptoAlgorithmIdentifier::CONCAT:
    case CryptoAlgorithmIdentifier::HKDF_CTR:
    case CryptoAlgorithmIdentifier::PBKDF2:
        throwNotSupportedError(state, scope);
        return nullptr;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

}

#endif // ENABLE(SUBTLE_CRYPTO)

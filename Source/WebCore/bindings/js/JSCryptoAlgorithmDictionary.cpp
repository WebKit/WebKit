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

enum class HashRequirement {
    Optional,
    Required, 
};

static inline JSValue getProperty(ExecState& state, JSObject* object, const char* name)
{
    return object->get(&state, Identifier::fromString(&state, name));
}

bool JSCryptoAlgorithmDictionary::getAlgorithmIdentifier(ExecState& state, JSValue value, CryptoAlgorithmIdentifier& algorithmIdentifier)
{
    auto scope = DECLARE_THROW_SCOPE(state.vm());

    // typedef (Algorithm or DOMString) AlgorithmIdentifier;

    String algorithmName;

    if (value.isString()) {
        algorithmName = asString(value)->value(&state);
        RETURN_IF_EXCEPTION(scope, false);
    } else if (value.isObject()) {
        if (asObject(value)->inherits(StringObject::info())) {
            algorithmName = asString(asStringObject(value)->internalValue())->value(&state);
            RETURN_IF_EXCEPTION(scope, false);
        } else {
            // FIXME: This doesn't perform some checks mandated by WebIDL for dictionaries:
            // - null and undefined input should be treated as if all elements in the dictionary were undefined;
            // - undefined elements should be treated as having a default value, or as not present if there isn't such;
            // - RegExp and Date objects cannot be converted to dictionaries.
            //
            // This is partially because we don't implement it elsewhere in WebCore yet, and partially because
            // WebCrypto doesn't yet clearly specify what to do with non-present values in most cases anyway.

            auto nameValue = getProperty(state, asObject(value), "name");
            RETURN_IF_EXCEPTION(scope, false);

            algorithmName = convert<IDLDOMString>(state, nameValue);
            RETURN_IF_EXCEPTION(scope, false);
        }
    }

    if (!algorithmName.containsOnlyASCII()) {
        throwSyntaxError(&state, scope);
        return false;
    }

    auto identifier = CryptoAlgorithmRegistry::singleton().identifier(algorithmName);
    if (!identifier) {
        setDOMException(&state, NOT_SUPPORTED_ERR);
        return false;
    }

    algorithmIdentifier = *identifier;
    return true;
}

static bool getHashAlgorithm(ExecState& state, JSObject* object, CryptoAlgorithmIdentifier& result, HashRequirement isRequired)
{
    auto scope = DECLARE_THROW_SCOPE(state.vm());

    auto hash = getProperty(state, object, "hash");
    RETURN_IF_EXCEPTION(scope, false);

    if (hash.isUndefinedOrNull()) {
        if (isRequired == HashRequirement::Required)
            setDOMException(&state, NOT_SUPPORTED_ERR);
        return false;
    }

    return JSCryptoAlgorithmDictionary::getAlgorithmIdentifier(state, hash, result);
}

static RefPtr<CryptoAlgorithmParametersDeprecated> createAesCbcParams(ExecState& state, JSValue value)
{
    auto scope = DECLARE_THROW_SCOPE(state.vm());

    if (!value.isObject()) {
        throwTypeError(&state, scope);
        return nullptr;
    }

    auto iv = getProperty(state, asObject(value), "iv");
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto result = adoptRef(*new CryptoAlgorithmAesCbcParamsDeprecated);

    CryptoOperationData ivData;
    auto success = cryptoOperationDataFromJSValue(state, iv, ivData);
    ASSERT(scope.exception() || success);
    if (!success)
        return nullptr;

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

    auto success = getHashAlgorithm(state, asObject(value), result->hash, HashRequirement::Required);
    ASSERT_UNUSED(scope, scope.exception() || success);
    if (!success)
        return nullptr;

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

    auto success = getHashAlgorithm(state, asObject(value), result->hash, HashRequirement::Required);
    ASSERT(scope.exception() || success);
    if (!success)
        return nullptr;

    auto lengthValue = getProperty(state, asObject(value), "length");
    RETURN_IF_EXCEPTION(scope, nullptr);

    result->length = convert<IDLUnsignedShort>(state, lengthValue, IntegerConversionConfiguration::Normal);
    RETURN_IF_EXCEPTION(scope, nullptr);

    result->hasLength = true;

    return WTFMove(result);
}

static RefPtr<CryptoAlgorithmParametersDeprecated> createRsaKeyGenParams(ExecState& state, JSValue value)
{
    auto scope = DECLARE_THROW_SCOPE(state.vm());

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

    auto publicExponentArray = toUnsharedUint8Array(publicExponentValue);
    if (!publicExponentArray) {
        throwTypeError(&state, scope, ASCIILiteral("Expected a Uint8Array in publicExponent"));
        return nullptr;
    }
    result->publicExponent.append(publicExponentArray->data(), publicExponentArray->byteLength());

    result->hasHash = getHashAlgorithm(state, asObject(value), result->hash, HashRequirement::Optional);

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

    auto success = getHashAlgorithm(state, asObject(value), result->hash, HashRequirement::Required);
    ASSERT(scope.exception() || success);
    if (!success)
        return nullptr;

    auto labelValue = getProperty(state, asObject(value), "label");
    RETURN_IF_EXCEPTION(scope, nullptr);

    result->hasLabel = !labelValue.isUndefinedOrNull();
    if (!result->hasLabel)
        return WTFMove(result);

    CryptoOperationData labelData;
    success = cryptoOperationDataFromJSValue(state, labelValue, labelData);
    ASSERT(scope.exception() || success);
    if (!success)
        return nullptr;

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

    auto success = getHashAlgorithm(state, asObject(value), result->hash, HashRequirement::Required);
    ASSERT(scope.exception() || success);
    if (!success)
        return nullptr;

    return WTFMove(result);
}

RefPtr<CryptoAlgorithmParametersDeprecated> JSCryptoAlgorithmDictionary::createParametersForEncrypt(ExecState& state, CryptoAlgorithmIdentifier algorithm, JSValue value)
{
    switch (algorithm) {
    case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
        return adoptRef(*new CryptoAlgorithmParametersDeprecated);
    case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSA_PSS:
        setDOMException(&state, NOT_SUPPORTED_ERR);
        return nullptr;
    case CryptoAlgorithmIdentifier::RSA_OAEP:
        return createRsaOaepParams(state, value);
    case CryptoAlgorithmIdentifier::ECDSA:
    case CryptoAlgorithmIdentifier::ECDH:
    case CryptoAlgorithmIdentifier::AES_CTR:
        setDOMException(&state, NOT_SUPPORTED_ERR);
        return nullptr;
    case CryptoAlgorithmIdentifier::AES_CBC:
        return createAesCbcParams(state, value);
    case CryptoAlgorithmIdentifier::AES_CMAC:
    case CryptoAlgorithmIdentifier::AES_GCM:
    case CryptoAlgorithmIdentifier::AES_CFB:
        setDOMException(&state, NOT_SUPPORTED_ERR);
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
        setDOMException(&state, NOT_SUPPORTED_ERR);
        return nullptr;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<CryptoAlgorithmParametersDeprecated> JSCryptoAlgorithmDictionary::createParametersForDecrypt(ExecState& state, CryptoAlgorithmIdentifier algorithm, JSValue value)
{
    switch (algorithm) {
    case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
        return adoptRef(*new CryptoAlgorithmParametersDeprecated);
    case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSA_PSS:
        setDOMException(&state, NOT_SUPPORTED_ERR);
        return nullptr;
    case CryptoAlgorithmIdentifier::RSA_OAEP:
        return createRsaOaepParams(state, value);
    case CryptoAlgorithmIdentifier::ECDSA:
    case CryptoAlgorithmIdentifier::ECDH:
    case CryptoAlgorithmIdentifier::AES_CTR:
        setDOMException(&state, NOT_SUPPORTED_ERR);
        return nullptr;
    case CryptoAlgorithmIdentifier::AES_CBC:
        return createAesCbcParams(state, value);
    case CryptoAlgorithmIdentifier::AES_CMAC:
    case CryptoAlgorithmIdentifier::AES_GCM:
    case CryptoAlgorithmIdentifier::AES_CFB:
        setDOMException(&state, NOT_SUPPORTED_ERR);
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
        setDOMException(&state, NOT_SUPPORTED_ERR);
        return nullptr;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<CryptoAlgorithmParametersDeprecated> JSCryptoAlgorithmDictionary::createParametersForSign(ExecState& state, CryptoAlgorithmIdentifier algorithm, JSValue value)
{
    switch (algorithm) {
    case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
        setDOMException(&state, NOT_SUPPORTED_ERR);
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
        setDOMException(&state, NOT_SUPPORTED_ERR);
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
        setDOMException(&state, NOT_SUPPORTED_ERR);
        return nullptr;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<CryptoAlgorithmParametersDeprecated> JSCryptoAlgorithmDictionary::createParametersForVerify(ExecState& state, CryptoAlgorithmIdentifier algorithm, JSValue value)
{
    switch (algorithm) {
    case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
        setDOMException(&state, NOT_SUPPORTED_ERR);
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
        setDOMException(&state, NOT_SUPPORTED_ERR);
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
        setDOMException(&state, NOT_SUPPORTED_ERR);
        return nullptr;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<CryptoAlgorithmParametersDeprecated> JSCryptoAlgorithmDictionary::createParametersForDigest(ExecState& state, CryptoAlgorithmIdentifier algorithm, JSValue)
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
        setDOMException(&state, NOT_SUPPORTED_ERR);
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
        setDOMException(&state, NOT_SUPPORTED_ERR);
        return nullptr;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<CryptoAlgorithmParametersDeprecated> JSCryptoAlgorithmDictionary::createParametersForGenerateKey(ExecState& state, CryptoAlgorithmIdentifier algorithm, JSValue value)
{
    switch (algorithm) {
    case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSA_PSS:
    case CryptoAlgorithmIdentifier::RSA_OAEP:
        return createRsaKeyGenParams(state, value);
    case CryptoAlgorithmIdentifier::ECDSA:
    case CryptoAlgorithmIdentifier::ECDH:
        setDOMException(&state, NOT_SUPPORTED_ERR);
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
        setDOMException(&state, NOT_SUPPORTED_ERR);
        return nullptr;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<CryptoAlgorithmParametersDeprecated> JSCryptoAlgorithmDictionary::createParametersForDeriveKey(ExecState& state, CryptoAlgorithmIdentifier algorithm, JSValue)
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
        setDOMException(&state, NOT_SUPPORTED_ERR);
        return nullptr;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<CryptoAlgorithmParametersDeprecated> JSCryptoAlgorithmDictionary::createParametersForDeriveBits(ExecState& state, CryptoAlgorithmIdentifier algorithm, JSValue)
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
        setDOMException(&state, NOT_SUPPORTED_ERR);
        return nullptr;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<CryptoAlgorithmParametersDeprecated> JSCryptoAlgorithmDictionary::createParametersForImportKey(ExecState& state, CryptoAlgorithmIdentifier algorithm, JSValue value)
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
        setDOMException(&state, NOT_SUPPORTED_ERR);
        return nullptr;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<CryptoAlgorithmParametersDeprecated> JSCryptoAlgorithmDictionary::createParametersForExportKey(ExecState& state, CryptoAlgorithmIdentifier algorithm, JSValue)
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
        setDOMException(&state, NOT_SUPPORTED_ERR);
        return nullptr;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

}

#endif // ENABLE(SUBTLE_CRYPTO)

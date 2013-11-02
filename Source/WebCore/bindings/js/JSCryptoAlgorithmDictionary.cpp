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
#include "JSCryptoAlgorithmDictionary.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithmAesCbcParams.h"
#include "CryptoAlgorithmHmacKeyParams.h"
#include "CryptoAlgorithmHmacParams.h"
#include "CryptoAlgorithmRegistry.h"
#include "ExceptionCode.h"
#include "JSCryptoOperationData.h"
#include "JSDOMBinding.h"
#include "JSDictionary.h"

using namespace JSC;

namespace WebCore {

bool JSCryptoAlgorithmDictionary::getAlgorithmIdentifier(ExecState* exec, JSValue value, CryptoAlgorithmIdentifier& algorithmIdentifier)
{
    // typedef (Algorithm or DOMString) AlgorithmIdentifier;

    String algorithmName;

    if (value.isString())
        algorithmName = value.toString(exec)->value(exec);
    else if (value.isObject()) {
        if (value.getObject()->inherits(StringObject::info()))
            algorithmName = asString(asStringObject(value)->internalValue())->value(exec);
        else {
            // FIXME: This doesn't perform some checks mandated by WebIDL for dictionaries:
            // - null and undefined input should be treated as if all elements in the dictionary were undefined;
            // - undefined elements should be treated as having a default value, or as not present if there isn't such;
            // - RegExp and Date objects cannot be converted to dictionaries.
            //
            // This is partially because we don't implement it elsewhere in WebCore yet, and partially because
            // WebCrypto doesn't yet clearly specify what to do with non-present values in most cases anyway.

            JSDictionary dictionary(exec, value.getObject());
            dictionary.get("name", algorithmName);
        }
    }

    if (exec->hadException())
        return false;

    if (!algorithmName.containsOnlyASCII()) {
        throwSyntaxError(exec);
        return false;
    }

    if (!CryptoAlgorithmRegistry::shared().getIdentifierForName(algorithmName, algorithmIdentifier)) {
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return false;
    }

    return true;
}

static JSValue getProperty(ExecState* exec, JSObject* object, const char* name)
{
    Identifier identifier(exec, name);
    PropertySlot slot(object);

    if (object->getPropertySlot(exec, identifier, slot))
        return slot.getValue(exec, identifier);

    return jsUndefined();
}

static bool getHashAlgorithm(JSDictionary& dictionary, CryptoAlgorithmIdentifier& result)
{
    // FXIME: Teach JSDictionary how to return JSValues, and use that to get hash element value.

    ExecState* exec = dictionary.execState();
    JSObject* object = dictionary.initializerObject();

    Identifier identifier(exec, "hash");
    PropertySlot slot(object);

    JSValue hash = getProperty(exec, object, "hash");
    if (exec->hadException())
        return false;

    if (hash.isUndefinedOrNull()) {
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return false;
    }

    return JSCryptoAlgorithmDictionary::getAlgorithmIdentifier(exec, hash, result);
}

static std::unique_ptr<CryptoAlgorithmParameters> createAesCbcParams(JSC::ExecState* exec, JSC::JSValue value)
{
    if (!value.isObject()) {
        throwTypeError(exec);
        return nullptr;
    }

    JSValue iv = getProperty(exec, value.getObject(), "iv");
    if (exec->hadException())
        return nullptr;

    std::unique_ptr<CryptoAlgorithmAesCbcParams> result = std::make_unique<CryptoAlgorithmAesCbcParams>();

    CryptoOperationData ivData;
    if (!cryptoOperationDataFromJSValue(exec, iv, ivData)) {
        ASSERT(exec->hadException());
        return nullptr;
    }

    if (ivData.second != 16) {
        exec->vm().throwException(exec, createError(exec, "AES-CBC initialization data must be 16 bytes"));
        return nullptr;
    }

    memcpy(result->iv.data(), ivData.first, ivData.second);

    return std::move(result);
}

static std::unique_ptr<CryptoAlgorithmParameters> createHmacParams(JSC::ExecState* exec, JSC::JSValue value)
{
    if (!value.isObject()) {
        throwTypeError(exec);
        return nullptr;
    }

    JSDictionary jsDictionary(exec, value.getObject());
    std::unique_ptr<CryptoAlgorithmHmacParams> result = std::make_unique<CryptoAlgorithmHmacParams>();

    if (!getHashAlgorithm(jsDictionary, result->hash)) {
        ASSERT(exec->hadException());
        return nullptr;
    }

    return std::move(result);
}

static std::unique_ptr<CryptoAlgorithmParameters> createHmacKeyParams(JSC::ExecState* exec, JSC::JSValue value)
{
    if (!value.isObject()) {
        throwTypeError(exec);
        return nullptr;
    }

    JSDictionary jsDictionary(exec, value.getObject());
    std::unique_ptr<CryptoAlgorithmHmacKeyParams> result = std::make_unique<CryptoAlgorithmHmacKeyParams>();

    if (!getHashAlgorithm(jsDictionary, result->hash)) {
        ASSERT(exec->hadException());
        return nullptr;
    }

    result->hasLength = jsDictionary.get("length", result->length);
    if (exec->hadException())
        return nullptr;

    return std::move(result);
}

std::unique_ptr<CryptoAlgorithmParameters> JSCryptoAlgorithmDictionary::createParametersForEncrypt(JSC::ExecState* exec, CryptoAlgorithmIdentifier algorithm, JSC::JSValue value)
{
    switch (algorithm) {
    case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSA_PSS:
    case CryptoAlgorithmIdentifier::RSA_OAEP:
    case CryptoAlgorithmIdentifier::ECDSA:
    case CryptoAlgorithmIdentifier::ECDH:
    case CryptoAlgorithmIdentifier::AES_CTR:
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return nullptr;
    case CryptoAlgorithmIdentifier::AES_CBC:
        return createAesCbcParams(exec, value);
    case CryptoAlgorithmIdentifier::AES_CMAC:
    case CryptoAlgorithmIdentifier::AES_GCM:
    case CryptoAlgorithmIdentifier::AES_CFB:
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
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return nullptr;
    }
}

std::unique_ptr<CryptoAlgorithmParameters> JSCryptoAlgorithmDictionary::createParametersForDecrypt(JSC::ExecState* exec, CryptoAlgorithmIdentifier algorithm, JSC::JSValue value)
{
    switch (algorithm) {
    case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5:
    case CryptoAlgorithmIdentifier::RSA_PSS:
    case CryptoAlgorithmIdentifier::RSA_OAEP:
    case CryptoAlgorithmIdentifier::ECDSA:
    case CryptoAlgorithmIdentifier::ECDH:
    case CryptoAlgorithmIdentifier::AES_CTR:
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return nullptr;
    case CryptoAlgorithmIdentifier::AES_CBC:
        return createAesCbcParams(exec, value);
    case CryptoAlgorithmIdentifier::AES_CMAC:
    case CryptoAlgorithmIdentifier::AES_GCM:
    case CryptoAlgorithmIdentifier::AES_CFB:
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
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return nullptr;
    }
}

std::unique_ptr<CryptoAlgorithmParameters> JSCryptoAlgorithmDictionary::createParametersForSign(JSC::ExecState* exec, CryptoAlgorithmIdentifier algorithm, JSC::JSValue value)
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
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return nullptr;
    case CryptoAlgorithmIdentifier::HMAC:
        return createHmacParams(exec, value);
    case CryptoAlgorithmIdentifier::DH:
    case CryptoAlgorithmIdentifier::SHA_1:
    case CryptoAlgorithmIdentifier::SHA_224:
    case CryptoAlgorithmIdentifier::SHA_256:
    case CryptoAlgorithmIdentifier::SHA_384:
    case CryptoAlgorithmIdentifier::SHA_512:
    case CryptoAlgorithmIdentifier::CONCAT:
    case CryptoAlgorithmIdentifier::HKDF_CTR:
    case CryptoAlgorithmIdentifier::PBKDF2:
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return nullptr;
    }
}

std::unique_ptr<CryptoAlgorithmParameters> JSCryptoAlgorithmDictionary::createParametersForVerify(JSC::ExecState* exec, CryptoAlgorithmIdentifier algorithm, JSC::JSValue value)
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
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return nullptr;
    case CryptoAlgorithmIdentifier::HMAC:
        return createHmacParams(exec, value);
    case CryptoAlgorithmIdentifier::DH:
    case CryptoAlgorithmIdentifier::SHA_1:
    case CryptoAlgorithmIdentifier::SHA_224:
    case CryptoAlgorithmIdentifier::SHA_256:
    case CryptoAlgorithmIdentifier::SHA_384:
    case CryptoAlgorithmIdentifier::SHA_512:
    case CryptoAlgorithmIdentifier::CONCAT:
    case CryptoAlgorithmIdentifier::HKDF_CTR:
    case CryptoAlgorithmIdentifier::PBKDF2:
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return nullptr;
    }
}

std::unique_ptr<CryptoAlgorithmParameters> JSCryptoAlgorithmDictionary::createParametersForDigest(JSC::ExecState* exec, CryptoAlgorithmIdentifier algorithm, JSC::JSValue)
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
    case CryptoAlgorithmIdentifier::HMAC:
    case CryptoAlgorithmIdentifier::DH:
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return nullptr;
    case CryptoAlgorithmIdentifier::SHA_1:
    case CryptoAlgorithmIdentifier::SHA_224:
    case CryptoAlgorithmIdentifier::SHA_256:
    case CryptoAlgorithmIdentifier::SHA_384:
    case CryptoAlgorithmIdentifier::SHA_512:
        return std::make_unique<CryptoAlgorithmParameters>();
    case CryptoAlgorithmIdentifier::CONCAT:
    case CryptoAlgorithmIdentifier::HKDF_CTR:
    case CryptoAlgorithmIdentifier::PBKDF2:
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return nullptr;
    }
}

std::unique_ptr<CryptoAlgorithmParameters> JSCryptoAlgorithmDictionary::createParametersForGenerateKey(JSC::ExecState* exec, CryptoAlgorithmIdentifier algorithm, JSC::JSValue)
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
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return nullptr;
    }
}

std::unique_ptr<CryptoAlgorithmParameters> JSCryptoAlgorithmDictionary::createParametersForDeriveKey(JSC::ExecState* exec, CryptoAlgorithmIdentifier algorithm, JSC::JSValue)
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
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return nullptr;
    }
}

std::unique_ptr<CryptoAlgorithmParameters> JSCryptoAlgorithmDictionary::createParametersForDeriveBits(JSC::ExecState* exec, CryptoAlgorithmIdentifier algorithm, JSC::JSValue)
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
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return nullptr;
    }
}

std::unique_ptr<CryptoAlgorithmParameters> JSCryptoAlgorithmDictionary::createParametersForImportKey(JSC::ExecState* exec, CryptoAlgorithmIdentifier algorithm, JSC::JSValue value)
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
        return std::make_unique<CryptoAlgorithmParameters>();
    case CryptoAlgorithmIdentifier::HMAC:
        return createHmacKeyParams(exec, value);
    case CryptoAlgorithmIdentifier::DH:
        return std::make_unique<CryptoAlgorithmParameters>();
    case CryptoAlgorithmIdentifier::SHA_1:
    case CryptoAlgorithmIdentifier::SHA_224:
    case CryptoAlgorithmIdentifier::SHA_256:
    case CryptoAlgorithmIdentifier::SHA_384:
    case CryptoAlgorithmIdentifier::SHA_512:
    case CryptoAlgorithmIdentifier::CONCAT:
    case CryptoAlgorithmIdentifier::HKDF_CTR:
    case CryptoAlgorithmIdentifier::PBKDF2:
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return nullptr;
    }
}

std::unique_ptr<CryptoAlgorithmParameters> JSCryptoAlgorithmDictionary::createParametersForExportKey(JSC::ExecState* exec, CryptoAlgorithmIdentifier algorithm, JSC::JSValue)
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
    case CryptoAlgorithmIdentifier::HMAC:
    case CryptoAlgorithmIdentifier::DH:
        return std::make_unique<CryptoAlgorithmParameters>();
    case CryptoAlgorithmIdentifier::SHA_1:
    case CryptoAlgorithmIdentifier::SHA_224:
    case CryptoAlgorithmIdentifier::SHA_256:
    case CryptoAlgorithmIdentifier::SHA_384:
    case CryptoAlgorithmIdentifier::SHA_512:
    case CryptoAlgorithmIdentifier::CONCAT:
    case CryptoAlgorithmIdentifier::HKDF_CTR:
    case CryptoAlgorithmIdentifier::PBKDF2:
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return nullptr;
    }
}

std::unique_ptr<CryptoAlgorithmParameters> JSCryptoAlgorithmDictionary::createParametersForWrapKey(JSC::ExecState* exec, CryptoAlgorithmIdentifier algorithm, JSC::JSValue)
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
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return nullptr;
    }
}

std::unique_ptr<CryptoAlgorithmParameters> JSCryptoAlgorithmDictionary::createParametersForUnwrapKey(JSC::ExecState* exec, CryptoAlgorithmIdentifier algorithm, JSC::JSValue)
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
        setDOMException(exec, NOT_SUPPORTED_ERR);
        return nullptr;
    }
}

}

#endif // ENABLE(SUBTLE_CRYPTO)

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
#include "JSCryptoKeySerializationJWK.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoAlgorithm.h"
#include "CryptoAlgorithmHmacParams.h"
#include "CryptoAlgorithmRegistry.h"
#include "CryptoKeyDataOctetSequence.h"
#include "ExceptionCode.h"
#include "JSDOMBinding.h"
#include <heap/StrongInlines.h>
#include <runtime/JSONObject.h>
#include <wtf/text/Base64.h>

using namespace JSC;

namespace WebCore {

static bool getStringFromJSON(ExecState* exec, JSObject* json, const char* key, String& result)
{
    Identifier identifier(exec, key);
    PropertySlot slot(json);

    if (!json->getPropertySlot(exec, identifier, slot))
        return false;

    JSValue jsValue = slot.getValue(exec, identifier);
    ASSERT(!exec->hadException());
    if (!jsValue.getString(exec, result)) {
        // Can get an out of memory exception.
        if (exec->hadException())
            return false;
        throwTypeError(exec, String::format("Expected a string value for \"%s\" JSON key",  key));
        return false;
    }

    return true;
}

static bool getBooleanFromJSON(ExecState* exec, JSObject* json, const char* key, bool& result)
{
    Identifier identifier(exec, key);
    PropertySlot slot(json);

    if (!json->getPropertySlot(exec, identifier, slot))
        return false;

    JSValue jsValue = slot.getValue(exec, identifier);
    ASSERT(!exec->hadException());
    if (!jsValue.isBoolean()) {
        throwTypeError(exec, String::format("Expected a boolean value for \"%s\" JSON key",  key));
        return false;
    }

    result = jsValue.asBoolean();
    return true;
}

JSCryptoKeySerializationJWK::JSCryptoKeySerializationJWK(ExecState* exec, const String& jsonString)
    : m_exec(exec)
{
    JSValue jsonValue = JSONParse(exec, jsonString);
    if (exec->hadException())
        return;

    if (!jsonValue || !jsonValue.isObject()) {
        throwTypeError(exec, "Invalid JWK serialization");
        return;
    }

    m_json.set(m_exec->vm(), asObject(jsonValue));
}

JSCryptoKeySerializationJWK::~JSCryptoKeySerializationJWK()
{
}

static std::unique_ptr<CryptoAlgorithmParameters> createHMACParameters(CryptoAlgorithmIdentifier hashFunction)
{
    std::unique_ptr<CryptoAlgorithmHmacParams> hmacParameters = std::make_unique<CryptoAlgorithmHmacParams>();
    hmacParameters->hash = hashFunction;
    return std::move(hmacParameters);
}

bool JSCryptoKeySerializationJWK::reconcileAlgorithm(std::unique_ptr<CryptoAlgorithm>& suggestedAlgorithm, std::unique_ptr<CryptoAlgorithmParameters>& suggestedParameters) const
{
    if (!getStringFromJSON(m_exec, m_json.get(), "alg", m_jwkAlgorithmName)) {
        // Algorithm is optional in JWK.
        return true;
    }

    std::unique_ptr<CryptoAlgorithm> algorithm;
    std::unique_ptr<CryptoAlgorithmParameters> parameters;
    if (m_jwkAlgorithmName == "HS256") {
        algorithm = CryptoAlgorithmRegistry::shared().create(CryptoAlgorithmIdentifier::HMAC);
        parameters = createHMACParameters(CryptoAlgorithmIdentifier::SHA_256);
    } else if (m_jwkAlgorithmName == "HS384") {
        algorithm = CryptoAlgorithmRegistry::shared().create(CryptoAlgorithmIdentifier::HMAC);
        parameters = createHMACParameters(CryptoAlgorithmIdentifier::SHA_384);
    } else if (m_jwkAlgorithmName == "HS512") {
        algorithm = CryptoAlgorithmRegistry::shared().create(CryptoAlgorithmIdentifier::HMAC);
        parameters = createHMACParameters(CryptoAlgorithmIdentifier::SHA_512);
    } else if (m_jwkAlgorithmName == "A128CBC") {
        algorithm = CryptoAlgorithmRegistry::shared().create(CryptoAlgorithmIdentifier::AES_CBC);
        parameters = std::make_unique<CryptoAlgorithmParameters>();
    } else if (m_jwkAlgorithmName == "A192CBC") {
        algorithm = CryptoAlgorithmRegistry::shared().create(CryptoAlgorithmIdentifier::AES_CBC);
        parameters = std::make_unique<CryptoAlgorithmParameters>();
    } else if (m_jwkAlgorithmName == "A256CBC") {
        algorithm = CryptoAlgorithmRegistry::shared().create(CryptoAlgorithmIdentifier::AES_CBC);
        parameters = std::make_unique<CryptoAlgorithmParameters>();
    } else {
        throwTypeError(m_exec, "Unsupported JWK algorithm " + m_jwkAlgorithmName);
        return false;
    }

    if (!suggestedAlgorithm) {
        suggestedAlgorithm = std::move(algorithm);
        suggestedParameters =  std::move(parameters);
        return true;
    }

    if (!algorithm)
        return true;

    if (algorithm->identifier() != suggestedAlgorithm->identifier())
        return false;

    // HMAC is the only algorithm that has parameters in importKey.
    if (algorithm->identifier() != CryptoAlgorithmIdentifier::HMAC)
        return true;

    return static_cast<CryptoAlgorithmHmacParams&>(*parameters).hash == static_cast<CryptoAlgorithmHmacParams&>(*suggestedParameters).hash;
}

void JSCryptoKeySerializationJWK::reconcileUsages(CryptoKeyUsage& suggestedUsage) const
{
    String jwkUseString;
    if (!getStringFromJSON(m_exec, m_json.get(), "use", jwkUseString)) {
        // "use" is optional in JWK.
        return;
    }

    // FIXME: CryptoKeyUsageDeriveKey, CryptoKeyUsageDeriveBits - should these be implicitly allowed by any JWK use value?
    // FIXME: There is a mismatch between specs for wrap/unwrap usages, <http://lists.w3.org/Archives/Public/public-webcrypto/2013Nov/0016.html>.
    if (jwkUseString == "sig")
        suggestedUsage = suggestedUsage & (CryptoKeyUsageSign | CryptoKeyUsageVerify);
    else if (jwkUseString == "enc")
        suggestedUsage = suggestedUsage & (CryptoKeyUsageEncrypt | CryptoKeyUsageDecrypt | CryptoKeyUsageWrapKey | CryptoKeyUsageUnwrapKey);
    else if (jwkUseString == "wrap")
        suggestedUsage = suggestedUsage & (CryptoKeyUsageWrapKey | CryptoKeyUsageUnwrapKey);
    else
        suggestedUsage = 0; // Unknown usage, better be safe.
}

void JSCryptoKeySerializationJWK::reconcileExtractable(bool& suggestedExtractable) const
{
    bool jwkExtractable;
    if (!getBooleanFromJSON(m_exec, m_json.get(), "extractable", jwkExtractable)) {
        // "extractable" is a Netflix proposal that's not in any spec yet. It will certainly be optional once specified.
        return;
    }

    suggestedExtractable = suggestedExtractable && jwkExtractable;
}

bool JSCryptoKeySerializationJWK::keySizeIsValid(size_t sizeInBits) const
{
    if (m_jwkAlgorithmName == "HS256")
        return sizeInBits >= 256;
    if (m_jwkAlgorithmName == "HS384")
        return sizeInBits >= 384;
    if (m_jwkAlgorithmName == "HS512")
        return sizeInBits >= 512;
    if (m_jwkAlgorithmName == "A128CBC")
        return sizeInBits == 128;
    if (m_jwkAlgorithmName == "A192CBC")
        return sizeInBits == 192;
    if (m_jwkAlgorithmName == "A256CBC")
        return sizeInBits == 256;
    return true;
}

std::unique_ptr<CryptoKeyData> JSCryptoKeySerializationJWK::keyData() const
{
    String jwkKeyType;
    if (!getStringFromJSON(m_exec, m_json.get(), "kty", jwkKeyType)) {
        if (!m_exec->hadException())
            throwTypeError(m_exec, "Required JWK \"kty\" member is missing");
        return nullptr;
    }

    if (jwkKeyType != "oct") {
        throwTypeError(m_exec, "Unsupported JWK key type " + jwkKeyType);
        return nullptr;
    }

    String keyBase64URL;
    if (!getStringFromJSON(m_exec, m_json.get(), "k", keyBase64URL)) {
        if (!m_exec->hadException())
            throwTypeError(m_exec, "Secret key data is not present is JWK");
        return nullptr;
    }

    Vector<char> octetSequence;
    if (!base64URLDecode(keyBase64URL, octetSequence)) {
        throwTypeError(m_exec, "Cannot decode base64url key data in JWK");
        return nullptr;
    }

    if (!keySizeIsValid(octetSequence.size() * 8)) {
        throwTypeError(m_exec, "Key size is not valid for " + m_jwkAlgorithmName);
        return nullptr;
    }

    return CryptoKeyDataOctetSequence::create(octetSequence);
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)

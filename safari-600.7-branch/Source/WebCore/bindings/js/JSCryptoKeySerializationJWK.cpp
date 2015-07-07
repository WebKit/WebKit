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
#include "CryptoAlgorithmRsaKeyParamsWithHash.h"
#include "CryptoKey.h"
#include "CryptoKeyAES.h"
#include "CryptoKeyDataOctetSequence.h"
#include "CryptoKeyDataRSAComponents.h"
#include "CryptoKeyHMAC.h"
#include "CryptoKeyRSA.h"
#include "ExceptionCode.h"
#include "JSDOMBinding.h"
#include <heap/StrongInlines.h>
#include <runtime/JSCInlines.h>
#include <runtime/JSONObject.h>
#include <runtime/ObjectConstructor.h>
#include <wtf/text/Base64.h>

using namespace JSC;

namespace WebCore {

static bool getJSArrayFromJSON(ExecState* exec, JSObject* json, const char* key, JSArray*& result)
{
    Identifier identifier(exec, key);
    PropertySlot slot(json);

    if (!json->getPropertySlot(exec, identifier, slot))
        return false;

    JSValue value = slot.getValue(exec, identifier);
    ASSERT(!exec->hadException());
    if (!isJSArray(value)) {
        throwTypeError(exec, String::format("Expected an array for \"%s\" JSON key",  key));
        return false;
    }

    result = asArray(value);

    return true;
}

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

static bool getBigIntegerVectorFromJSON(ExecState* exec, JSObject* json, const char* key, Vector<uint8_t>& result)
{
    String base64urlEncodedNumber;
    if (!getStringFromJSON(exec, json, key, base64urlEncodedNumber))
        return false;

    if (!base64URLDecode(base64urlEncodedNumber, result)) {
        throwTypeError(exec, "Cannot decode base64url key data in JWK");
        return false;
    }

    if (result[0] == 0) {
        throwTypeError(exec, "JWK BigInteger must utilize the minimum number of octets to represent the value");
        return false;
    }

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
    return WTF::move(hmacParameters);
}

static std::unique_ptr<CryptoAlgorithmParameters> createRSAKeyParametersWithHash(CryptoAlgorithmIdentifier hashFunction)
{
    std::unique_ptr<CryptoAlgorithmRsaKeyParamsWithHash> rsaKeyParameters = std::make_unique<CryptoAlgorithmRsaKeyParamsWithHash>();
    rsaKeyParameters->hasHash = true;
    rsaKeyParameters->hash = hashFunction;
    return WTF::move(rsaKeyParameters);
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
    } else if (m_jwkAlgorithmName == "RS256") {
        algorithm = CryptoAlgorithmRegistry::shared().create(CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5);
        parameters = createRSAKeyParametersWithHash(CryptoAlgorithmIdentifier::SHA_256);
    } else if (m_jwkAlgorithmName == "RS384") {
        algorithm = CryptoAlgorithmRegistry::shared().create(CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5);
        parameters = createRSAKeyParametersWithHash(CryptoAlgorithmIdentifier::SHA_384);
    } else if (m_jwkAlgorithmName == "RS512") {
        algorithm = CryptoAlgorithmRegistry::shared().create(CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5);
        parameters = createRSAKeyParametersWithHash(CryptoAlgorithmIdentifier::SHA_512);
    } else if (m_jwkAlgorithmName == "RSA1_5") {
        algorithm = CryptoAlgorithmRegistry::shared().create(CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5);
        parameters = std::make_unique<CryptoAlgorithmParameters>();
    } else if (m_jwkAlgorithmName == "RSA-OAEP") {
        algorithm = CryptoAlgorithmRegistry::shared().create(CryptoAlgorithmIdentifier::RSA_OAEP);
        parameters = createRSAKeyParametersWithHash(CryptoAlgorithmIdentifier::SHA_1);
    } else if (m_jwkAlgorithmName == "A128CBC") {
        algorithm = CryptoAlgorithmRegistry::shared().create(CryptoAlgorithmIdentifier::AES_CBC);
        parameters = std::make_unique<CryptoAlgorithmParameters>();
    } else if (m_jwkAlgorithmName == "A192CBC") {
        algorithm = CryptoAlgorithmRegistry::shared().create(CryptoAlgorithmIdentifier::AES_CBC);
        parameters = std::make_unique<CryptoAlgorithmParameters>();
    } else if (m_jwkAlgorithmName == "A256CBC") {
        algorithm = CryptoAlgorithmRegistry::shared().create(CryptoAlgorithmIdentifier::AES_CBC);
        parameters = std::make_unique<CryptoAlgorithmParameters>();
    } else if (m_jwkAlgorithmName == "A128KW") {
        algorithm = CryptoAlgorithmRegistry::shared().create(CryptoAlgorithmIdentifier::AES_KW);
        parameters = std::make_unique<CryptoAlgorithmParameters>();
    } else if (m_jwkAlgorithmName == "A192KW") {
        algorithm = CryptoAlgorithmRegistry::shared().create(CryptoAlgorithmIdentifier::AES_KW);
        parameters = std::make_unique<CryptoAlgorithmParameters>();
    } else if (m_jwkAlgorithmName == "A256KW") {
        algorithm = CryptoAlgorithmRegistry::shared().create(CryptoAlgorithmIdentifier::AES_KW);
        parameters = std::make_unique<CryptoAlgorithmParameters>();
    } else {
        throwTypeError(m_exec, "Unsupported JWK algorithm " + m_jwkAlgorithmName);
        return false;
    }

    if (!suggestedAlgorithm) {
        suggestedAlgorithm = WTF::move(algorithm);
        suggestedParameters =  WTF::move(parameters);
        return true;
    }

    if (!algorithm)
        return true;

    if (algorithm->identifier() != suggestedAlgorithm->identifier())
        return false;

    if (algorithm->identifier() == CryptoAlgorithmIdentifier::HMAC)
        return toCryptoAlgorithmHmacParams(*parameters).hash == toCryptoAlgorithmHmacParams(*suggestedParameters).hash;
    if (algorithm->identifier() == CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5
        || algorithm->identifier() == CryptoAlgorithmIdentifier::RSA_OAEP) {
        CryptoAlgorithmRsaKeyParamsWithHash& rsaKeyParameters = toCryptoAlgorithmRsaKeyParamsWithHash(*parameters);
        CryptoAlgorithmRsaKeyParamsWithHash& suggestedRSAKeyParameters = toCryptoAlgorithmRsaKeyParamsWithHash(*suggestedParameters);
        ASSERT(rsaKeyParameters.hasHash);
        if (suggestedRSAKeyParameters.hasHash)
            return suggestedRSAKeyParameters.hash == rsaKeyParameters.hash;
        suggestedRSAKeyParameters.hasHash = true;
        suggestedRSAKeyParameters.hash = rsaKeyParameters.hash;
    }

    // Other algorithms don't have parameters.
    return true;
}

static bool tryJWKKeyOpsValue(ExecState* exec, CryptoKeyUsage& usages, const String& operation, const String& tryOperation, CryptoKeyUsage tryUsage)
{
    if (operation == tryOperation) {
        if (usages & tryUsage) {
            throwTypeError(exec, "JWK key_ops contains a duplicate operation");
            return false;
        }
        usages |= tryUsage;
    }
    return true;
}

void JSCryptoKeySerializationJWK::reconcileUsages(CryptoKeyUsage& suggestedUsages) const
{
    CryptoKeyUsage jwkUsages = 0;

    JSArray* keyOps;
    if (getJSArrayFromJSON(m_exec, m_json.get(), "key_ops", keyOps)) {
        for (size_t i = 0; i < keyOps->length(); ++i) {
            JSValue jsValue = keyOps->getIndex(m_exec, i);
            String operation;
            if (!jsValue.getString(m_exec, operation)) {
                if (!m_exec->hadException())
                    throwTypeError(m_exec, "JWK key_ops attribute could not be processed");
                return;
            }
            if (!tryJWKKeyOpsValue(m_exec, jwkUsages, operation, ASCIILiteral("sign"), CryptoKeyUsageSign))
                return;
            if (!tryJWKKeyOpsValue(m_exec, jwkUsages, operation, ASCIILiteral("verify"), CryptoKeyUsageVerify))
                return;
            if (!tryJWKKeyOpsValue(m_exec, jwkUsages, operation, ASCIILiteral("encrypt"), CryptoKeyUsageEncrypt))
                return;
            if (!tryJWKKeyOpsValue(m_exec, jwkUsages, operation, ASCIILiteral("decrypt"), CryptoKeyUsageDecrypt))
                return;
            if (!tryJWKKeyOpsValue(m_exec, jwkUsages, operation, ASCIILiteral("wrapKey"), CryptoKeyUsageWrapKey))
                return;
            if (!tryJWKKeyOpsValue(m_exec, jwkUsages, operation, ASCIILiteral("unwrapKey"), CryptoKeyUsageUnwrapKey))
                return;
            if (!tryJWKKeyOpsValue(m_exec, jwkUsages, operation, ASCIILiteral("deriveKey"), CryptoKeyUsageDeriveKey))
                return;
            if (!tryJWKKeyOpsValue(m_exec, jwkUsages, operation, ASCIILiteral("deriveBits"), CryptoKeyUsageDeriveBits))
                return;
        }
    } else {
        if (m_exec->hadException())
            return;

        String jwkUseString;
        if (!getStringFromJSON(m_exec, m_json.get(), "use", jwkUseString)) {
            // We have neither key_ops nor use.
            return;
        }

        if (jwkUseString == "enc")
            jwkUsages |= (CryptoKeyUsageEncrypt | CryptoKeyUsageDecrypt | CryptoKeyUsageWrapKey | CryptoKeyUsageUnwrapKey);
        else if (jwkUseString == "sig")
            jwkUsages |= (CryptoKeyUsageSign | CryptoKeyUsageVerify);
        else {
            throwTypeError(m_exec, "Unsupported JWK key use value \"" + jwkUseString + "\"");
            return;
        }
    }

    suggestedUsages = suggestedUsages & jwkUsages;
}

void JSCryptoKeySerializationJWK::reconcileExtractable(bool& suggestedExtractable) const
{
    bool jwkExtractable;
    if (!getBooleanFromJSON(m_exec, m_json.get(), "ext", jwkExtractable))
        return;

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
    if (m_jwkAlgorithmName == "A128KW")
        return sizeInBits == 128;
    if (m_jwkAlgorithmName == "A192KW")
        return sizeInBits == 192;
    if (m_jwkAlgorithmName == "A256KW")
        return sizeInBits == 256;
    if (m_jwkAlgorithmName == "RS256")
        return sizeInBits >= 2048;
    if (m_jwkAlgorithmName == "RS384")
        return sizeInBits >= 2048;
    if (m_jwkAlgorithmName == "RS512")
        return sizeInBits >= 2048;
    if (m_jwkAlgorithmName == "RSA1_5")
        return sizeInBits >= 2048;
    if (m_jwkAlgorithmName == "RSA_OAEP")
        return sizeInBits >= 2048;
    return true;
}

std::unique_ptr<CryptoKeyData> JSCryptoKeySerializationJWK::keyDataOctetSequence() const
{
    String keyBase64URL;
    if (!getStringFromJSON(m_exec, m_json.get(), "k", keyBase64URL)) {
        if (!m_exec->hadException())
            throwTypeError(m_exec, "Secret key data is not present is JWK");
        return nullptr;
    }

    Vector<uint8_t> octetSequence;
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

std::unique_ptr<CryptoKeyData> JSCryptoKeySerializationJWK::keyDataRSAComponents() const
{
    Vector<uint8_t> modulus;
    Vector<uint8_t> exponent;
    Vector<uint8_t> privateExponent;

    if (!getBigIntegerVectorFromJSON(m_exec, m_json.get(), "n", modulus)) {
        if (!m_exec->hadException())
            throwTypeError(m_exec, "Required JWK \"n\" member is missing");
        return nullptr;
    }

    if (!keySizeIsValid(modulus.size() * 8)) {
        throwTypeError(m_exec, "Key size is not valid for " + m_jwkAlgorithmName);
        return nullptr;
    }

    if (!getBigIntegerVectorFromJSON(m_exec, m_json.get(), "e", exponent)) {
        if (!m_exec->hadException())
            throwTypeError(m_exec, "Required JWK \"e\" member is missing");
        return nullptr;
    }

    if (!getBigIntegerVectorFromJSON(m_exec, m_json.get(), "d", modulus)) {
        if (m_exec->hadException())
            return nullptr;
        return CryptoKeyDataRSAComponents::createPublic(modulus, exponent);
    }

    CryptoKeyDataRSAComponents::PrimeInfo firstPrimeInfo;
    CryptoKeyDataRSAComponents::PrimeInfo secondPrimeInfo;
    Vector<CryptoKeyDataRSAComponents::PrimeInfo> otherPrimeInfos;
    if (!getBigIntegerVectorFromJSON(m_exec, m_json.get(), "p", firstPrimeInfo.primeFactor)) {
        if (m_exec->hadException())
            return nullptr;
        return CryptoKeyDataRSAComponents::createPrivate(modulus, exponent, privateExponent);
    }

    if (!getBigIntegerVectorFromJSON(m_exec, m_json.get(), "dp", firstPrimeInfo.factorCRTExponent)) {
        if (m_exec->hadException())
            return nullptr;
        return CryptoKeyDataRSAComponents::createPrivate(modulus, exponent, privateExponent);
    }

    if (!getBigIntegerVectorFromJSON(m_exec, m_json.get(), "q", secondPrimeInfo.primeFactor)) {
        if (m_exec->hadException())
            return nullptr;
        return CryptoKeyDataRSAComponents::createPrivate(modulus, exponent, privateExponent);
    }

    if (!getBigIntegerVectorFromJSON(m_exec, m_json.get(), "dq", secondPrimeInfo.factorCRTExponent)) {
        if (m_exec->hadException())
            return nullptr;
        return CryptoKeyDataRSAComponents::createPrivate(modulus, exponent, privateExponent);
    }

    if (!getBigIntegerVectorFromJSON(m_exec, m_json.get(), "qi", secondPrimeInfo.factorCRTCoefficient)) {
        if (m_exec->hadException())
            return nullptr;
        return CryptoKeyDataRSAComponents::createPrivate(modulus, exponent, privateExponent);
    }

    JSArray* otherPrimeInfoJSArray;
    if (!getJSArrayFromJSON(m_exec, m_json.get(), "oth", otherPrimeInfoJSArray)) {
        if (m_exec->hadException())
            return nullptr;
        return CryptoKeyDataRSAComponents::createPrivateWithAdditionalData(modulus, exponent, privateExponent, firstPrimeInfo, secondPrimeInfo, otherPrimeInfos);
    }

    for (size_t i = 0; i < otherPrimeInfoJSArray->length(); ++i) {
        CryptoKeyDataRSAComponents::PrimeInfo info;
        JSValue element = otherPrimeInfoJSArray->getIndex(m_exec, i);
        if (m_exec->hadException())
            return nullptr;
        if (!element.isObject()) {
            throwTypeError(m_exec, "JWK \"oth\" array member is not an object");
            return nullptr;
        }
        if (!getBigIntegerVectorFromJSON(m_exec, asObject(element), "r", info.primeFactor)) {
            if (!m_exec->hadException())
                throwTypeError(m_exec, "Cannot get prime factor for a prime in \"oth\" dictionary");
            return nullptr;
        }
        if (!getBigIntegerVectorFromJSON(m_exec, asObject(element), "d", info.factorCRTExponent)) {
            if (!m_exec->hadException())
                throwTypeError(m_exec, "Cannot get factor CRT exponent for a prime in \"oth\" dictionary");
            return nullptr;
        }
        if (!getBigIntegerVectorFromJSON(m_exec, asObject(element), "t", info.factorCRTCoefficient)) {
            if (!m_exec->hadException())
                throwTypeError(m_exec, "Cannot get factor CRT coefficient for a prime in \"oth\" dictionary");
            return nullptr;
        }
        otherPrimeInfos.append(info);
    }

    return CryptoKeyDataRSAComponents::createPrivateWithAdditionalData(modulus, exponent, privateExponent, firstPrimeInfo, secondPrimeInfo, otherPrimeInfos);
}

std::unique_ptr<CryptoKeyData> JSCryptoKeySerializationJWK::keyData() const
{
    String jwkKeyType;
    if (!getStringFromJSON(m_exec, m_json.get(), "kty", jwkKeyType)) {
        if (!m_exec->hadException())
            throwTypeError(m_exec, "Required JWK \"kty\" member is missing");
        return nullptr;
    }

    if (jwkKeyType == "oct")
        return keyDataOctetSequence();

    if (jwkKeyType == "RSA")
        return keyDataRSAComponents();

    throwTypeError(m_exec, "Unsupported JWK key type " + jwkKeyType);
    return nullptr;
}

static void addToJSON(ExecState* exec, JSObject* json, const char* key, const String& value)
{
    VM& vm = exec->vm();
    Identifier identifier(&vm, key);
    json->putDirect(vm, identifier, jsString(exec, value));
}

static void buildJSONForOctetSequence(ExecState* exec, const Vector<uint8_t>& keyData, JSObject* result)
{
    addToJSON(exec, result, "kty", "oct");
    addToJSON(exec, result, "k", base64URLEncode(keyData));
}

static void buildJSONForRSAComponents(JSC::ExecState* exec, const CryptoKeyDataRSAComponents& data, JSC::JSObject* result)
{
    addToJSON(exec, result, "kty", "RSA");
    addToJSON(exec, result, "n", base64URLEncode(data.modulus()));
    addToJSON(exec, result, "e", base64URLEncode(data.exponent()));

    if (data.type() == CryptoKeyDataRSAComponents::Type::Public)
        return;

    addToJSON(exec, result, "d", base64URLEncode(data.privateExponent()));

    if (!data.hasAdditionalPrivateKeyParameters())
        return;

    addToJSON(exec, result, "p", base64URLEncode(data.firstPrimeInfo().primeFactor));
    addToJSON(exec, result, "q", base64URLEncode(data.secondPrimeInfo().primeFactor));
    addToJSON(exec, result, "dp", base64URLEncode(data.firstPrimeInfo().factorCRTExponent));
    addToJSON(exec, result, "dq", base64URLEncode(data.secondPrimeInfo().factorCRTExponent));
    addToJSON(exec, result, "qi", base64URLEncode(data.secondPrimeInfo().factorCRTCoefficient));

    if (data.otherPrimeInfos().isEmpty())
        return;

    JSArray* oth = constructEmptyArray(exec, 0, exec->lexicalGlobalObject(), data.otherPrimeInfos().size());
    for (size_t i = 0, size = data.otherPrimeInfos().size(); i < size; ++i) {
        JSObject* jsPrimeInfo = constructEmptyObject(exec);
        addToJSON(exec, jsPrimeInfo, "r", base64URLEncode(data.otherPrimeInfos()[i].primeFactor));
        addToJSON(exec, jsPrimeInfo, "d", base64URLEncode(data.otherPrimeInfos()[i].factorCRTExponent));
        addToJSON(exec, jsPrimeInfo, "t", base64URLEncode(data.otherPrimeInfos()[i].factorCRTCoefficient));
        oth->putDirectIndex(exec, i, jsPrimeInfo);
    }
    result->putDirect(exec->vm(), Identifier(exec, "oth"), oth);
}

static void addBoolToJSON(ExecState* exec, JSObject* json, const char* key, bool value)
{
    VM& vm = exec->vm();
    Identifier identifier(&vm, key);
    json->putDirect(vm, identifier, jsBoolean(value));
}

static void addJWKAlgorithmToJSON(ExecState* exec, JSObject* json, const CryptoKey& key)
{
    String jwkAlgorithm;
    switch (key.algorithmIdentifier()) {
    case CryptoAlgorithmIdentifier::HMAC:
        switch (toCryptoKeyHMAC(key).hashAlgorithmIdentifier()) {
        case CryptoAlgorithmIdentifier::SHA_256:
            if (toCryptoKeyHMAC(key).key().size() * 8 >= 256)
                jwkAlgorithm = "HS256";
            break;
        case CryptoAlgorithmIdentifier::SHA_384:
            if (toCryptoKeyHMAC(key).key().size() * 8 >= 384)
                jwkAlgorithm = "HS384";
            break;
        case CryptoAlgorithmIdentifier::SHA_512:
            if (toCryptoKeyHMAC(key).key().size() * 8 >= 512)
                jwkAlgorithm = "HS512";
            break;
        default:
            break;
        }
        break;
    case CryptoAlgorithmIdentifier::AES_CBC:
        switch (toCryptoKeyAES(key).key().size() * 8) {
        case 128:
            jwkAlgorithm = "A128CBC";
            break;
        case 192:
            jwkAlgorithm = "A192CBC";
            break;
        case 256:
            jwkAlgorithm = "A256CBC";
            break;
        }
        break;
    case CryptoAlgorithmIdentifier::AES_KW:
        switch (toCryptoKeyAES(key).key().size() * 8) {
        case 128:
            jwkAlgorithm = "A128KW";
            break;
        case 192:
            jwkAlgorithm = "A192KW";
            break;
        case 256:
            jwkAlgorithm = "A256KW";
            break;
        }
        break;
    case CryptoAlgorithmIdentifier::RSASSA_PKCS1_v1_5: {
        const CryptoKeyRSA& rsaKey = toCryptoKeyRSA(key);
        CryptoAlgorithmIdentifier hash;
        if (!rsaKey.isRestrictedToHash(hash))
            break;
        if (rsaKey.keySizeInBits() < 2048)
            break;
        switch (hash) {
        case CryptoAlgorithmIdentifier::SHA_256:
            jwkAlgorithm = "RS256";
            break;
        case CryptoAlgorithmIdentifier::SHA_384:
            jwkAlgorithm = "RS384";
            break;
        case CryptoAlgorithmIdentifier::SHA_512:
            jwkAlgorithm = "RS512";
            break;
        default:
            break;
        }
        break;
    }
    case CryptoAlgorithmIdentifier::RSAES_PKCS1_v1_5: {
        const CryptoKeyRSA& rsaKey = toCryptoKeyRSA(key);
        if (rsaKey.keySizeInBits() < 2048)
            break;
        jwkAlgorithm = "RSA1_5";
        break;
    }
    case CryptoAlgorithmIdentifier::RSA_OAEP: {
        const CryptoKeyRSA& rsaKey = toCryptoKeyRSA(key);
        CryptoAlgorithmIdentifier hash;
        // WebCrypto RSA-OAEP keys are not tied to any particular hash, unless previously imported from JWK, which only supports SHA-1.
        if (rsaKey.isRestrictedToHash(hash) && hash != CryptoAlgorithmIdentifier::SHA_1)
            break;
        if (rsaKey.keySizeInBits() < 2048)
            break;
        jwkAlgorithm = "RSA-OAEP";
        break;
    }
    default:
        break;
    }

    if (jwkAlgorithm.isNull()) {
        // The spec doesn't currently tell whether export should fail, or just skip "alg" (which is an optional key in JWK).
        throwTypeError(exec, "Key algorithm and size do not map to any JWK algorithm identifier");
        return;
    }

    addToJSON(exec, json, "alg", jwkAlgorithm);
}

static void addUsagesToJSON(ExecState* exec, JSObject* json, CryptoKeyUsage usages)
{
    JSArray* keyOps = constructEmptyArray(exec, 0, exec->lexicalGlobalObject(), 0);

    unsigned index = 0;
    if (usages & CryptoKeyUsageSign)
        keyOps->putDirectIndex(exec, index++, jsString(exec, ASCIILiteral("sign")));
    if (usages & CryptoKeyUsageVerify)
        keyOps->putDirectIndex(exec, index++, jsString(exec, ASCIILiteral("verify")));
    if (usages & CryptoKeyUsageEncrypt)
        keyOps->putDirectIndex(exec, index++, jsString(exec, ASCIILiteral("encrypt")));
    if (usages & CryptoKeyUsageDecrypt)
        keyOps->putDirectIndex(exec, index++, jsString(exec, ASCIILiteral("decrypt")));
    if (usages & CryptoKeyUsageWrapKey)
        keyOps->putDirectIndex(exec, index++, jsString(exec, ASCIILiteral("wrapKey")));
    if (usages & CryptoKeyUsageUnwrapKey)
        keyOps->putDirectIndex(exec, index++, jsString(exec, ASCIILiteral("unwrapKey")));
    if (usages & CryptoKeyUsageDeriveKey)
        keyOps->putDirectIndex(exec, index++, jsString(exec, ASCIILiteral("deriveKey")));
    if (usages & CryptoKeyUsageDeriveBits)
        keyOps->putDirectIndex(exec, index++, jsString(exec, ASCIILiteral("deriveBits")));

    json->putDirect(exec->vm(), Identifier(exec, "key_ops"), keyOps);
}

String JSCryptoKeySerializationJWK::serialize(ExecState* exec, const CryptoKey& key)
{
    std::unique_ptr<CryptoKeyData> keyData = key.exportData();
    if (!keyData) {
        // This generally shouldn't happen as long as all key types implement exportData(), but as underlying libraries return errors, there may be some rare failure conditions.
        throwTypeError(exec, "Couldn't export key material");
        return String();
    }

    JSObject* result = constructEmptyObject(exec);

    addJWKAlgorithmToJSON(exec, result, key);
    if (exec->hadException())
        return String();

    addBoolToJSON(exec, result, "ext", key.extractable());

    addUsagesToJSON(exec, result, key.usagesBitmap());
    if (exec->hadException())
        return String();

    if (isCryptoKeyDataOctetSequence(*keyData))
        buildJSONForOctetSequence(exec, toCryptoKeyDataOctetSequence(*keyData).octetSequence(), result);
    else if (isCryptoKeyDataRSAComponents(*keyData))
        buildJSONForRSAComponents(exec, toCryptoKeyDataRSAComponents(*keyData), result);
    else {
        throwTypeError(exec, "Key doesn't support exportKey");
        return String();
    }
    if (exec->hadException())
        return String();

    return JSONStringify(exec, result, 0);
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)

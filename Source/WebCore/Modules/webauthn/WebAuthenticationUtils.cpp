/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "WebAuthenticationUtils.h"

#if ENABLE(WEB_AUTHN)

#include "CBORReader.h"
#include "CBORWriter.h"
#include "FidoConstants.h"
#include "WebAuthenticationConstants.h"
#include <pal/crypto/CryptoDigest.h>
#include <wtf/JSONValues.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/Base64.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

Vector<uint8_t> produceRpIdHash(const String& rpId)
{
    auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
    auto rpIdUTF8 = rpId.utf8();
    crypto->addBytes(byteCast<uint8_t>(rpIdUTF8.span()));
    return crypto->computeHash();
}

Vector<uint8_t> encodeES256PublicKeyAsCBOR(Vector<uint8_t>&& x, Vector<uint8_t>&& y)
{
    cbor::CBORValue::MapValue publicKeyMap;
    publicKeyMap[cbor::CBORValue(COSE::kty)] = cbor::CBORValue(COSE::EC2);
    publicKeyMap[cbor::CBORValue(COSE::alg)] = cbor::CBORValue(COSE::ES256);
    publicKeyMap[cbor::CBORValue(COSE::crv)] = cbor::CBORValue(COSE::P_256);
    publicKeyMap[cbor::CBORValue(COSE::x)] = cbor::CBORValue(WTFMove(x));
    publicKeyMap[cbor::CBORValue(COSE::y)] = cbor::CBORValue(WTFMove(y));

    auto cosePublicKey = cbor::CBORWriter::write(cbor::CBORValue(WTFMove(publicKeyMap)));
    ASSERT(cosePublicKey);
    return *cosePublicKey;
}

Vector<uint8_t> buildAttestedCredentialData(const Vector<uint8_t>& aaguid, const Vector<uint8_t>& credentialId, const Vector<uint8_t>& coseKey)
{
    Vector<uint8_t> attestedCredentialData;
    attestedCredentialData.reserveInitialCapacity(aaguidLength + credentialIdLengthLength + credentialId.size() + coseKey.size());

    // aaguid
    ASSERT(aaguid.size() == aaguidLength);
    attestedCredentialData.appendVector(aaguid);

    // credentialIdLength
    ASSERT(credentialId.size() <= std::numeric_limits<uint16_t>::max());
    attestedCredentialData.append(credentialId.size() >> 8 & 0xff);
    attestedCredentialData.append(credentialId.size() & 0xff);

    // credentialId
    attestedCredentialData.appendVector(credentialId);

    // credentialPublicKey
    attestedCredentialData.appendVector(coseKey);

    return attestedCredentialData;
}

cbor::CBORValue::MapValue buildUserEntityMap(const Vector<uint8_t>& userId, const String& name, const String& displayName)
{
    cbor::CBORValue::MapValue userEntityMap;
    userEntityMap[cbor::CBORValue(fido::kEntityIdMapKey)] = cbor::CBORValue(userId);
    userEntityMap[cbor::CBORValue(fido::kEntityNameMapKey)] = cbor::CBORValue(name);
    userEntityMap[cbor::CBORValue(fido::kDisplayNameMapKey)] = cbor::CBORValue(displayName);
    return userEntityMap;
}

cbor::CBORValue::MapValue buildCredentialDescriptor(const Vector<uint8_t>& credentialId)
{
    cbor::CBORValue::MapValue credential;
    credential[cbor::CBORValue("id")] = cbor::CBORValue(credentialId);
    return credential;
}

Vector<uint8_t> buildAuthData(const String& rpId, const uint8_t flags, const uint32_t counter, const Vector<uint8_t>& optionalAttestedCredentialData)
{
    Vector<uint8_t> authData;
    authData.reserveInitialCapacity(rpIdHashLength + flagsLength + signCounterLength + optionalAttestedCredentialData.size());

    // RP ID hash
    authData.appendVector(produceRpIdHash(rpId));

    // FLAGS
    authData.append(flags);

    // COUNTER
    authData.append(counter >> 24 & 0xff);
    authData.append(counter >> 16 & 0xff);
    authData.append(counter >> 8 & 0xff);
    authData.append(counter & 0xff);

    // ATTESTED CRED. DATA
    authData.appendVector(optionalAttestedCredentialData);

    return authData;
}

cbor::CBORValue::MapValue buildAttestationMap(Vector<uint8_t>&& authData, String&& format, cbor::CBORValue::MapValue&& statementMap, const AttestationConveyancePreference& attestation, ShouldZeroAAGUID shouldZero)
{
    cbor::CBORValue::MapValue attestationObjectMap;
    // The following implements Step 20 with regard to AttestationConveyancePreference
    // of https://www.w3.org/TR/webauthn/#createCredential as of 4 March 2019.
    // None attestation is always returned if it is requested to keep consistency, and therefore skip the
    // step to return self attestation.
    if (attestation == AttestationConveyancePreference::None) {
        const size_t aaguidOffset = rpIdHashLength + flagsLength + signCounterLength;
        if (authData.size() >= aaguidOffset + aaguidLength && shouldZero == ShouldZeroAAGUID::Yes)
            zeroSpan(authData.mutableSpan().subspan(aaguidOffset, aaguidLength));
        format = String::fromLatin1(noneAttestationValue);
        statementMap.clear();
    }
    attestationObjectMap[cbor::CBORValue("authData")] = cbor::CBORValue(WTFMove(authData));
    attestationObjectMap[cbor::CBORValue("fmt")] = cbor::CBORValue(WTFMove(format));
    attestationObjectMap[cbor::CBORValue("attStmt")] = cbor::CBORValue(WTFMove(statementMap));
    return attestationObjectMap;
}

Vector<uint8_t> buildAttestationObject(Vector<uint8_t>&& authData, String&& format, cbor::CBORValue::MapValue&& statementMap, const AttestationConveyancePreference& attestation, ShouldZeroAAGUID shouldZero)
{
    cbor::CBORValue::MapValue attestationObjectMap = buildAttestationMap(WTFMove(authData), WTFMove(format), WTFMove(statementMap), attestation, shouldZero);

    auto attestationObject = cbor::CBORWriter::write(cbor::CBORValue(WTFMove(attestationObjectMap)));
    ASSERT(attestationObject);
    return *attestationObject;
}

Ref<ArrayBuffer> buildClientDataJson(ClientDataType type, const BufferSource& challenge, const SecurityOrigin& origin, WebAuthn::Scope scope, const String& topOrigin)
{
    // https://www.w3.org/TR/webauthn-2/#clientdatajson-verification
    auto object = JSON::Object::create();
    switch (type) {
    case ClientDataType::Create:
        object->setString("type"_s, "webauthn.create"_s);
        break;
    case ClientDataType::Get:
        object->setString("type"_s, "webauthn.get"_s);
        break;
    }
    object->setString("challenge"_s, base64URLEncodeToString(challenge.span()));
    object->setString("origin"_s, origin.toRawString());

    if (scope != WebAuthn::Scope::SameOrigin)
        object->setBoolean("crossOrigin"_s, scope != WebAuthn::Scope::SameOrigin);

    if (!topOrigin.isNull())
        object->setString("topOrigin"_s, topOrigin);

    return ArrayBuffer::create(byteCast<uint8_t>(object->toJSONString().utf8().span()));
}

Vector<uint8_t> buildClientDataJsonHash(const ArrayBuffer& clientDataJson)
{
    auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
    crypto->addBytes(clientDataJson.span());
    return crypto->computeHash();
}

Vector<uint8_t> encodeRawPublicKey(const Vector<uint8_t>& x, const Vector<uint8_t>& y)
{
    Vector<uint8_t> rawKey;
    rawKey.reserveInitialCapacity(1 + x.size() + y.size());
    rawKey.append(0x04);
    rawKey.appendVector(x);
    rawKey.appendVector(y);
    return rawKey;
}

String toString(AuthenticatorTransport transport)
{
    switch (transport) {
    case AuthenticatorTransport::Usb:
        return authenticatorTransportUsb;
        break;
    case AuthenticatorTransport::Nfc:
        return authenticatorTransportNfc;
        break;
    case AuthenticatorTransport::Ble:
        return authenticatorTransportBle;
        break;
    case AuthenticatorTransport::Internal:
        return authenticatorTransportInternal;
        break;
    case AuthenticatorTransport::Cable:
        return authenticatorTransportCable;
    case AuthenticatorTransport::Hybrid:
        return authenticatorTransportHybrid;
    case AuthenticatorTransport::SmartCard:
        return authenticatorTransportSmartCard;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return nullString();
}

std::optional<AuthenticatorTransport> convertStringToAuthenticatorTransport(const String& transport)
{
    if (transport == authenticatorTransportUsb)
        return AuthenticatorTransport::Usb;
    if (transport == authenticatorTransportNfc)
        return AuthenticatorTransport::Nfc;
    if (transport == authenticatorTransportBle)
        return AuthenticatorTransport::Ble;
    if (transport == authenticatorTransportInternal)
        return AuthenticatorTransport::Internal;
    if (transport == authenticatorTransportCable)
        return AuthenticatorTransport::Cable;
    if (transport == authenticatorTransportHybrid)
        return AuthenticatorTransport::Hybrid;
    if (transport == authenticatorTransportSmartCard)
        return AuthenticatorTransport::SmartCard;
    return std::nullopt;
}

static Ref<JSON::Value> cborValueToJSON(const cbor::CBORValue& value)
{
    if (value.isUnsigned())
        return JSON::Value::create(static_cast<double>(value.getUnsigned()));
    if (value.isNegative())
        return JSON::Value::create(static_cast<double>(value.getNegative()));
    if (value.isBool())
        return JSON::Value::create(value.getBool());
    if (value.isSimple() && value.getSimpleValue() == cbor::CBORValue::SimpleValue::NullValue)
        return JSON::Value::null();
    if (value.isString())
        return JSON::Value::create(value.getString());
    if (value.isByteString())
        return JSON::Value::create(base64EncodeToString(value.getByteString().span()));
    if (value.isArray()) {
        auto array = JSON::Array::create();
        for (auto& item : value.getArray())
            array->pushValue(cborValueToJSON(item));
        return array;
    }
    if (value.isMap()) {
        auto object = JSON::Object::create();
        for (auto& [key, val] : value.getMap()) {
            String keyStr;
            if (key.isString())
                keyStr = key.getString();
            else if (key.isUnsigned())
                keyStr = String::number(key.getUnsigned());
            else if (key.isNegative())
                keyStr = String::number(key.getNegative());
            else
                continue; // Skip unsupported key types
            object->setValue(keyStr, cborValueToJSON(val));
        }
        return object;
    }
    // Unknown/unsupported type
    return JSON::Value::null();
}

Ref<JSON::Object> ParsedCOSEKey::toJSONObject() const
{
    auto object = JSON::Object::create();

    object->setInteger("keyType"_s, keyType);
    if (algorithm)
        object->setInteger("algorithm"_s, *algorithm);
    if (curve)
        object->setInteger("curve"_s, *curve);
    if (keyId)
        object->setString("keyId"_s, *keyId);
    if (keyOps) {
        auto array = JSON::Array::create();
        for (int op : *keyOps)
            array->pushInteger(op);
        object->setArray("keyOps"_s, WTFMove(array));
    }
    if (baseIV)
        object->setString("baseIV"_s, *baseIV);

    // EC2/OKP parameters
    if (x)
        object->setString("x"_s, *x);
    if (y)
        object->setString("y"_s, *y);
    if (d)
        object->setString("d"_s, *d);

    // RSA parameters
    if (n)
        object->setString("n"_s, *n);
    if (e)
        object->setString("e"_s, *e);
    if (p)
        object->setString("p"_s, *p);
    if (q)
        object->setString("q"_s, *q);
    if (dP)
        object->setString("dP"_s, *dP);
    if (dQ)
        object->setString("dQ"_s, *dQ);
    if (qInv)
        object->setString("qInv"_s, *qInv);

    // Symmetric/HSS-LMS parameters
    if (keyValue)
        object->setString("keyValue"_s, *keyValue);
    if (lmsType)
        object->setInteger("lmsType"_s, *lmsType);
    if (lmotsType)
        object->setInteger("lmotsType"_s, *lmotsType);

    object->setString("rawCBOR"_s, rawCBOR);

    return object;
}

Ref<JSON::Object> ParsedAuthenticatorData::toJSONObject() const
{
    auto object = JSON::Object::create();

    object->setString("rpIdHash"_s, rpIdHash);

    auto flagsObject = JSON::Object::create();
    flagsObject->setBoolean("userPresent"_s, flags.userPresent);
    flagsObject->setBoolean("rfu1"_s, flags.rfu1);
    flagsObject->setBoolean("userVerified"_s, flags.userVerified);
    flagsObject->setBoolean("backupEligible"_s, flags.backupEligible);
    flagsObject->setBoolean("backupState"_s, flags.backupState);
    flagsObject->setBoolean("rfu2"_s, flags.rfu2);
    flagsObject->setBoolean("attestedCredentialDataIncluded"_s, flags.attestedCredentialDataIncluded);
    flagsObject->setBoolean("extensionDataIncluded"_s, flags.extensionDataIncluded);
    object->setObject("flags"_s, WTFMove(flagsObject));

    object->setInteger("signCount"_s, signCount);

    if (aaguid)
        object->setString("aaguid"_s, *aaguid);
    if (credentialId)
        object->setString("credentialId"_s, *credentialId);
    if (credentialPublicKey)
        object->setObject("credentialPublicKey"_s, credentialPublicKey->toJSONObject());
    if (extensions)
        object->setValue("extensions"_s, extensions->get());
    if (extensionsRaw)
        object->setString("extensionsRaw"_s, *extensionsRaw);

    return object;
}

std::optional<ParsedAuthenticatorData> parseAuthenticatorData(const Vector<uint8_t>& authData)
{
    constexpr size_t minAuthDataLength = rpIdHashLength + flagsLength + signCounterLength;
    if (authData.size() < minAuthDataLength)
        return std::nullopt;

    ParsedAuthenticatorData result;

    // RP ID Hash (32 bytes)
    result.rpIdHash = base64EncodeToString(authData.span().first(rpIdHashLength));

    // Flags (1 byte)
    uint8_t flagsByte = authData[rpIdHashLength];
    result.flags.userPresent = flagsByte & WebAuthn::userPresenceFlag;
    result.flags.rfu1 = flagsByte & WebAuthn::reservedFlag1;
    result.flags.userVerified = flagsByte & WebAuthn::userVerifiedFlag;
    result.flags.backupEligible = flagsByte & WebAuthn::backupEligibilityFlag;
    result.flags.backupState = flagsByte & WebAuthn::backupStateFlag;
    result.flags.rfu2 = flagsByte & WebAuthn::reservedFlag2;
    result.flags.attestedCredentialDataIncluded = flagsByte & WebAuthn::attestedCredentialDataIncludedFlag;
    result.flags.extensionDataIncluded = flagsByte & WebAuthn::extensionDataIncludedFlag;

    // Signature counter (4 bytes, big-endian)
    size_t offset = rpIdHashLength + flagsLength;
    result.signCount = (static_cast<uint32_t>(authData[offset]) << 24)
        | (static_cast<uint32_t>(authData[offset + 1]) << 16)
        | (static_cast<uint32_t>(authData[offset + 2]) << 8)
        | static_cast<uint32_t>(authData[offset + 3]);
    offset += signCounterLength;

    // Attested credential data (if AT flag set)
    if (result.flags.attestedCredentialDataIncluded) {
        // AAGUID (16 bytes) - format as RFC4122 UUID string
        if (authData.size() < offset + aaguidLength)
            return std::nullopt;

        auto aaguidBytes = authData.span().subspan(offset, aaguidLength);
        // Format as: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
        StringBuilder aaguidBuilder;
        for (size_t i = 0; i < aaguidLength; ++i) {
            if (i == 4 || i == 6 || i == 8 || i == 10)
                aaguidBuilder.append('-');
            aaguidBuilder.append(hex(aaguidBytes[i], 2, Lowercase));
        }
        result.aaguid = aaguidBuilder.toString();
        offset += aaguidLength;

        // Credential ID length (2 bytes, big-endian)
        if (authData.size() < offset + credentialIdLengthLength)
            return std::nullopt;
        size_t credentialIdLength = (static_cast<size_t>(authData[offset]) << 8) | static_cast<size_t>(authData[offset + 1]);
        offset += credentialIdLengthLength;

        // Credential ID
        if (authData.size() < offset + credentialIdLength)
            return std::nullopt;
        result.credentialId = base64EncodeToString(authData.span().subspan(offset, credentialIdLength));
        offset += credentialIdLength;

        // Credential public key (CBOR encoded)
        if (authData.size() <= offset)
            return std::nullopt;

        auto publicKeyBytes = authData.subspan(offset);
        auto publicKeyResult = cbor::CBORReader::readWithBytesConsumed(publicKeyBytes);
        if (!publicKeyResult || !publicKeyResult->first.isMap())
            return std::nullopt;

        // Parse COSE key structure
        ParsedCOSEKey coseKey;
        coseKey.rawCBOR = base64EncodeToString(publicKeyBytes.first(publicKeyResult->second));

        auto& keyMap = publicKeyResult->first.getMap();

        // Extract common COSE parameters
        auto it = keyMap.find(cbor::CBORValue(COSE::kty));
        if (it != keyMap.end() && it->second.isUnsigned())
            coseKey.keyType = static_cast<int>(it->second.getUnsigned());
        else
            return std::nullopt; // keyType is required

        it = keyMap.find(cbor::CBORValue(COSE::alg));
        if (it != keyMap.end() && it->second.isNegative())
            coseKey.algorithm = static_cast<int>(it->second.getNegative());
        else if (it != keyMap.end() && it->second.isUnsigned())
            coseKey.algorithm = static_cast<int>(it->second.getUnsigned());

        it = keyMap.find(cbor::CBORValue(COSE::crv));
        if (it != keyMap.end() && it->second.isUnsigned())
            coseKey.curve = static_cast<int>(it->second.getUnsigned());

        it = keyMap.find(cbor::CBORValue(COSE::kid));
        if (it != keyMap.end() && it->second.isByteString())
            coseKey.keyId = base64EncodeToString(it->second.getByteString().span());

        it = keyMap.find(cbor::CBORValue(COSE::keyOps));
        if (it != keyMap.end() && it->second.isArray()) {
            Vector<int> ops;
            for (auto& op : it->second.getArray()) {
                if (op.isUnsigned())
                    ops.append(static_cast<int>(op.getUnsigned()));
                else if (op.isNegative())
                    ops.append(static_cast<int>(op.getNegative()));
            }
            if (!ops.isEmpty())
                coseKey.keyOps = WTFMove(ops);
        }

        it = keyMap.find(cbor::CBORValue(COSE::baseIV));
        if (it != keyMap.end() && it->second.isByteString())
            coseKey.baseIV = base64EncodeToString(it->second.getByteString().span());

        // Extract key-type-specific parameters based on keyType
        if (coseKey.keyType == COSE::OKP || coseKey.keyType == COSE::EC2) {
            // OKP or EC2 keys
            it = keyMap.find(cbor::CBORValue(COSE::x));
            if (it != keyMap.end() && it->second.isByteString())
                coseKey.x = base64EncodeToString(it->second.getByteString().span());

            it = keyMap.find(cbor::CBORValue(COSE::y));
            if (it != keyMap.end() && it->second.isByteString())
                coseKey.y = base64EncodeToString(it->second.getByteString().span());

            it = keyMap.find(cbor::CBORValue(COSE::d));
            if (it != keyMap.end() && it->second.isByteString())
                coseKey.d = base64EncodeToString(it->second.getByteString().span());
        } else if (coseKey.keyType == COSE::RSA) {
            // RSA keys
            it = keyMap.find(cbor::CBORValue(COSE::n));
            if (it != keyMap.end() && it->second.isByteString())
                coseKey.n = base64EncodeToString(it->second.getByteString().span());

            it = keyMap.find(cbor::CBORValue(COSE::e));
            if (it != keyMap.end() && it->second.isByteString())
                coseKey.e = base64EncodeToString(it->second.getByteString().span());

            it = keyMap.find(cbor::CBORValue(COSE::rsaD));
            if (it != keyMap.end() && it->second.isByteString())
                coseKey.d = base64EncodeToString(it->second.getByteString().span());

            it = keyMap.find(cbor::CBORValue(COSE::p));
            if (it != keyMap.end() && it->second.isByteString())
                coseKey.p = base64EncodeToString(it->second.getByteString().span());

            it = keyMap.find(cbor::CBORValue(COSE::q));
            if (it != keyMap.end() && it->second.isByteString())
                coseKey.q = base64EncodeToString(it->second.getByteString().span());

            it = keyMap.find(cbor::CBORValue(COSE::dP));
            if (it != keyMap.end() && it->second.isByteString())
                coseKey.dP = base64EncodeToString(it->second.getByteString().span());

            it = keyMap.find(cbor::CBORValue(COSE::dQ));
            if (it != keyMap.end() && it->second.isByteString())
                coseKey.dQ = base64EncodeToString(it->second.getByteString().span());

            it = keyMap.find(cbor::CBORValue(COSE::qInv));
            if (it != keyMap.end() && it->second.isByteString())
                coseKey.qInv = base64EncodeToString(it->second.getByteString().span());
        } else if (coseKey.keyType == COSE::Symmetric) {
            // Symmetric keys
            it = keyMap.find(cbor::CBORValue(COSE::k));
            if (it != keyMap.end() && it->second.isByteString())
                coseKey.keyValue = base64EncodeToString(it->second.getByteString().span());
        } else if (coseKey.keyType == COSE::HSSLMS) {
            // HSS-LMS keys
            it = keyMap.find(cbor::CBORValue(COSE::lmsType));
            if (it != keyMap.end() && it->second.isUnsigned())
                coseKey.lmsType = static_cast<int>(it->second.getUnsigned());

            it = keyMap.find(cbor::CBORValue(COSE::lmotsType));
            if (it != keyMap.end() && it->second.isUnsigned())
                coseKey.lmotsType = static_cast<int>(it->second.getUnsigned());

            it = keyMap.find(cbor::CBORValue(COSE::publicKey));
            if (it != keyMap.end() && it->second.isByteString())
                coseKey.keyValue = base64EncodeToString(it->second.getByteString().span());
        }

        result.credentialPublicKey = WTFMove(coseKey);
        offset += publicKeyResult->second;
    }

    // Extensions (if ED flag set)
    if (result.flags.extensionDataIncluded) {
        if (authData.size() <= offset)
            return std::nullopt;

        auto extensionsBytes = authData.subspan(offset);
        result.extensionsRaw = base64EncodeToString(extensionsBytes);

        auto extensionsCBOR = cbor::CBORReader::read(extensionsBytes);
        if (extensionsCBOR && extensionsCBOR->isMap())
            result.extensions = cborValueToJSON(*extensionsCBOR);
    }

    return result;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)

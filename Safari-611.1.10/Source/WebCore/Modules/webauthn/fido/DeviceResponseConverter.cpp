// Copyright 2018 The Chromium Authors. All rights reserved.
// Copyright (C) 2018 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "DeviceResponseConverter.h"

#if ENABLE(WEB_AUTHN)

#include "AuthenticatorSupportedOptions.h"
#include "CBORReader.h"
#include "CBORWriter.h"
#include "WebAuthenticationConstants.h"
#include "WebAuthenticationUtils.h"
#include <wtf/StdSet.h>
#include <wtf/Vector.h>

namespace fido {
using namespace WebCore;
using CBOR = cbor::CBORValue;

static ProtocolVersion convertStringToProtocolVersion(const String& version)
{
    if (version == kCtap2Version)
        return ProtocolVersion::kCtap;
    if (version == kU2fVersion)
        return ProtocolVersion::kU2f;

    return ProtocolVersion::kUnknown;
}

Optional<cbor::CBORValue> decodeResponseMap(const Vector<uint8_t>& inBuffer)
{
    if (inBuffer.size() <= kResponseCodeLength || getResponseCode(inBuffer) != CtapDeviceResponseCode::kSuccess)
        return WTF::nullopt;

    Vector<uint8_t> buffer;
    buffer.append(inBuffer.data() + 1, inBuffer.size() - 1);
    Optional<CBOR> decodedResponse = cbor::CBORReader::read(buffer);
    if (!decodedResponse || !decodedResponse->isMap())
        return WTF::nullopt;
    return decodedResponse;
}

CtapDeviceResponseCode getResponseCode(const Vector<uint8_t>& buffer)
{
    if (buffer.isEmpty())
        return CtapDeviceResponseCode::kCtap2ErrInvalidCBOR;

    auto code = static_cast<CtapDeviceResponseCode>(buffer[0]);
    return isCtapDeviceResponseCode(code) ? code : CtapDeviceResponseCode::kCtap2ErrInvalidCBOR;
}

static Vector<uint8_t> getCredentialId(const Vector<uint8_t>& authenticatorData)
{
    const size_t credentialIdLengthOffset = rpIdHashLength + flagsLength + signCounterLength + aaguidLength;

    if (authenticatorData.size() < credentialIdLengthOffset + credentialIdLengthLength)
        return { };
    size_t credentialIdLength = (static_cast<size_t>(authenticatorData[credentialIdLengthOffset]) << 8) | static_cast<size_t>(authenticatorData[credentialIdLengthOffset + 1]);

    if (authenticatorData.size() < credentialIdLengthOffset + credentialIdLengthLength + credentialIdLength)
        return { };
    Vector<uint8_t> credentialId;
    credentialId.reserveInitialCapacity(credentialIdLength);
    auto beginIt = authenticatorData.begin() + credentialIdLengthOffset + credentialIdLengthLength;
    credentialId.appendRange(beginIt, beginIt + credentialIdLength);
    return credentialId;
}


// Decodes byte array response from authenticator to CBOR value object and
// checks for correct encoding format.
RefPtr<AuthenticatorAttestationResponse> readCTAPMakeCredentialResponse(const Vector<uint8_t>& inBuffer, const AttestationConveyancePreference& attestation)
{
    auto decodedMap = decodeResponseMap(inBuffer);
    if (!decodedMap)
        return nullptr;
    const auto& responseMap = decodedMap->getMap();

    auto it = responseMap.find(CBOR(1));
    if (it == responseMap.end() || !it->second.isString())
        return nullptr;
    auto format = it->second.clone();

    it = responseMap.find(CBOR(2));
    if (it == responseMap.end() || !it->second.isByteString())
        return nullptr;
    auto authenticatorData = it->second.clone();

    auto credentialId = getCredentialId(authenticatorData.getByteString());
    if (credentialId.isEmpty())
        return nullptr;

    it = responseMap.find(CBOR(3));
    if (it == responseMap.end() || !it->second.isMap())
        return nullptr;
    auto attStmt = it->second.clone();

    Optional<Vector<uint8_t>> attestationObject;
    if (attestation == AttestationConveyancePreference::None) {
        // The reason why we can't directly pass authenticatorData/format/attStmt to buildAttestationObject
        // is that they are CBORValue instead of the raw type.
        // Also, format and attStmt are omitted as they are not useful in none attestation.
        attestationObject = buildAttestationObject(Vector<uint8_t>(authenticatorData.getByteString()), "", { }, attestation);
    } else {
        CBOR::MapValue attestationObjectMap;
        attestationObjectMap[CBOR("authData")] = WTFMove(authenticatorData);
        attestationObjectMap[CBOR("fmt")] = WTFMove(format);
        attestationObjectMap[CBOR("attStmt")] = WTFMove(attStmt);
        attestationObject = cbor::CBORWriter::write(CBOR(WTFMove(attestationObjectMap)));
    }

    return AuthenticatorAttestationResponse::create(credentialId, *attestationObject);
}

RefPtr<AuthenticatorAssertionResponse> readCTAPGetAssertionResponse(const Vector<uint8_t>& inBuffer)
{
    auto decodedMap = decodeResponseMap(inBuffer);
    if (!decodedMap)
        return nullptr;
    const auto& responseMap = decodedMap->getMap();

    auto it = responseMap.find(CBOR(1));
    if (it == responseMap.end() || !it->second.isMap())
        return nullptr;
    auto& credential = it->second.getMap();
    auto itr = credential.find(CBOR(kCredentialIdKey));
    if (itr == credential.end() || !itr->second.isByteString())
        return nullptr;
    auto& credentialId = itr->second.getByteString();

    it = responseMap.find(CBOR(2));
    if (it == responseMap.end() || !it->second.isByteString())
        return nullptr;
    auto& authData = it->second.getByteString();

    it = responseMap.find(CBOR(3));
    if (it == responseMap.end() || !it->second.isByteString())
        return nullptr;
    auto& signature = it->second.getByteString();

    RefPtr<AuthenticatorAssertionResponse> response;
    it = responseMap.find(CBOR(4));
    if (it != responseMap.end() && it->second.isMap()) {
        auto& user = it->second.getMap();
        auto itr = user.find(CBOR(kEntityIdMapKey));
        if (itr == user.end() || !itr->second.isByteString())
            return nullptr;
        auto& userHandle = itr->second.getByteString();
        response = AuthenticatorAssertionResponse::create(credentialId, authData, signature, userHandle);

        itr = user.find(CBOR(kEntityNameMapKey));
        if (itr != user.end()) {
            if (!itr->second.isString())
                return nullptr;
            response->setName(itr->second.getString());
        }

        itr = user.find(CBOR(kDisplayNameMapKey));
        if (itr != user.end()) {
            if (!itr->second.isString())
                return nullptr;
            response->setDisplayName(itr->second.getString());
        }
    } else {
        response = AuthenticatorAssertionResponse::create(credentialId, authData, signature, { });
    }

    it = responseMap.find(CBOR(5));
    if (it != responseMap.end() && it->second.isUnsigned())
        response->setNumberOfCredentials(it->second.getUnsigned());

    return response;
}

Optional<AuthenticatorGetInfoResponse> readCTAPGetInfoResponse(const Vector<uint8_t>& inBuffer)
{
    auto decodedMap = decodeResponseMap(inBuffer);
    if (!decodedMap)
        return WTF::nullopt;
    const auto& responseMap = decodedMap->getMap();

    auto it = responseMap.find(CBOR(1));
    if (it == responseMap.end() || !it->second.isArray())
        return WTF::nullopt;
    StdSet<ProtocolVersion> protocolVersions;
    for (const auto& version : it->second.getArray()) {
        if (!version.isString())
            return WTF::nullopt;

        auto protocol = convertStringToProtocolVersion(version.getString());
        if (protocol == ProtocolVersion::kUnknown) {
            LOG_ERROR("Unexpected protocol version received.");
            continue;
        }

        if (!protocolVersions.insert(protocol).second)
            return WTF::nullopt;
    }
    if (protocolVersions.empty())
        return WTF::nullopt;

    it = responseMap.find(CBOR(3));
    if (it == responseMap.end() || !it->second.isByteString() || it->second.getByteString().size() != aaguidLength)
        return WTF::nullopt;

    AuthenticatorGetInfoResponse response(WTFMove(protocolVersions), Vector<uint8_t>(it->second.getByteString()));

    it = responseMap.find(CBOR(2));
    if (it != responseMap.end()) {
        if (!it->second.isArray())
            return WTF::nullopt;

        Vector<String> extensions;
        for (const auto& extension : it->second.getArray()) {
            if (!extension.isString())
                return WTF::nullopt;

            extensions.append(extension.getString());
        }
        response.setExtensions(WTFMove(extensions));
    }

    AuthenticatorSupportedOptions options;
    it = responseMap.find(CBOR(4));
    if (it != responseMap.end()) {
        if (!it->second.isMap())
            return WTF::nullopt;
        const auto& optionMap = it->second.getMap();
        auto optionMapIt = optionMap.find(CBOR(kPlatformDeviceMapKey));
        if (optionMapIt != optionMap.end()) {
            if (!optionMapIt->second.isBool())
                return WTF::nullopt;

            options.setIsPlatformDevice(optionMapIt->second.getBool());
        }

        optionMapIt = optionMap.find(CBOR(kResidentKeyMapKey));
        if (optionMapIt != optionMap.end()) {
            if (!optionMapIt->second.isBool())
                return WTF::nullopt;

            options.setSupportsResidentKey(optionMapIt->second.getBool());
        }

        optionMapIt = optionMap.find(CBOR(kUserPresenceMapKey));
        if (optionMapIt != optionMap.end()) {
            if (!optionMapIt->second.isBool())
                return WTF::nullopt;

            options.setUserPresenceRequired(optionMapIt->second.getBool());
        }

        optionMapIt = optionMap.find(CBOR(kUserVerificationMapKey));
        if (optionMapIt != optionMap.end()) {
            if (!optionMapIt->second.isBool())
                return WTF::nullopt;

            if (optionMapIt->second.getBool())
                options.setUserVerificationAvailability(AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedAndConfigured);
            else
                options.setUserVerificationAvailability(AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedButNotConfigured);
        }

        optionMapIt = optionMap.find(CBOR(kClientPinMapKey));
        if (optionMapIt != optionMap.end()) {
            if (!optionMapIt->second.isBool())
                return WTF::nullopt;

            if (optionMapIt->second.getBool())
                options.setClientPinAvailability(AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedAndPinSet);
            else
                options.setClientPinAvailability(AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedButPinNotSet);
        }
        response.setOptions(WTFMove(options));
    }

    it = responseMap.find(CBOR(5));
    if (it != responseMap.end()) {
        if (!it->second.isUnsigned())
            return WTF::nullopt;

        response.setMaxMsgSize(it->second.getUnsigned());
    }

    it = responseMap.find(CBOR(6));
    if (it != responseMap.end()) {
        if (!it->second.isArray())
            return WTF::nullopt;

        Vector<uint8_t> supportedPinProtocols;
        for (const auto& protocol : it->second.getArray()) {
            if (!protocol.isUnsigned())
                return WTF::nullopt;

            supportedPinProtocols.append(protocol.getUnsigned());
        }
        response.setPinProtocols(WTFMove(supportedPinProtocols));
    }

    return WTFMove(response);
}

} // namespace fido

#endif // ENABLE(WEB_AUTHN)

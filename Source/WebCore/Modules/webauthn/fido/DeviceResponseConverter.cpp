// Copyright 2018 The Chromium Authors. All rights reserved.
// Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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
    if (version == kCtap21Version)
        return ProtocolVersion::kCtap21;
    if (version == kCtap21PreVersion)
        return ProtocolVersion::kCtap21Pre;
    if (version == kCtap2Version)
        return ProtocolVersion::kCtap2;
    if (version == kU2fVersion)
        return ProtocolVersion::kU2f;

    return ProtocolVersion::kUnknown;
}

std::optional<cbor::CBORValue> decodeResponseMap(const Vector<uint8_t>& inBuffer)
{
    if (inBuffer.size() <= kResponseCodeLength || getResponseCode(inBuffer) != CtapDeviceResponseCode::kSuccess)
        return std::nullopt;

    auto buffer = inBuffer.subvector(1);
    std::optional<CBOR> decodedResponse = cbor::CBORReader::read(buffer);
    if (!decodedResponse || !decodedResponse->isMap())
        return std::nullopt;
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

    return Vector<uint8_t>(authenticatorData.subspan(credentialIdLengthOffset + credentialIdLengthLength, credentialIdLength));
}

static std::optional<AuthenticationExtensionsClientOutputs> parseAuthenticatorDataExtensions(const Vector<uint8_t>& authData)
{
    constexpr size_t minAuthDataLength = rpIdHashLength + flagsLength + signCounterLength;
    if (authData.size() < minAuthDataLength)
        return std::nullopt;

    uint8_t flags = authData[rpIdHashLength];
    bool extensionsPresent = flags & WebAuthn::extensionDataIncludedFlag;
    if (!extensionsPresent)
        return std::nullopt;

    size_t offset = minAuthDataLength;

    bool attestedCredentialDataPresent = flags & WebAuthn::attestedCredentialDataIncludedFlag;
    if (attestedCredentialDataPresent) {
        offset += aaguidLength;
        if (authData.size() < offset + credentialIdLengthLength)
            return std::nullopt;

        size_t credentialIdLength = (static_cast<size_t>(authData[offset]) << 8) | static_cast<size_t>(authData[offset + 1]);
        offset += credentialIdLengthLength + credentialIdLength;

        if (authData.size() <= offset)
            return std::nullopt;

        auto publicKeyBytes = authData.subspan(offset);
        auto publicKeyResult = cbor::CBORReader::readWithBytesConsumed(publicKeyBytes);
        if (!publicKeyResult)
            return std::nullopt;

        offset += publicKeyResult->second;
    }

    if (authData.size() <= offset)
        return std::nullopt;

    auto extensionsBytes = authData.subspan(offset);
    auto extensionsCBOR = cbor::CBORReader::read(extensionsBytes);
    if (!extensionsCBOR || !extensionsCBOR->isMap())
        return std::nullopt;

    AuthenticationExtensionsClientOutputs outputs;
    auto& extensionsMap = extensionsCBOR->getMap();

    auto hmacIt = extensionsMap.find(CBOR(kExtensionHmacSecret));
    if (hmacIt != extensionsMap.end()) {
        if (hmacIt->second.isBool()) {
            outputs.prf = AuthenticationExtensionsClientOutputs::PRFOutputs { };
            outputs.prf->enabled = hmacIt->second.getBool();
        } else if (hmacIt->second.isByteString()) {
            auto& hmacOutput = hmacIt->second.getByteString();

            RefPtr<ArrayBuffer> first = ArrayBuffer::tryCreate(hmacOutput.span().first(std::min<size_t>(32, hmacOutput.size())));
            RefPtr<ArrayBuffer> second;

            if (hmacOutput.size() == 64)
                second = ArrayBuffer::tryCreate(hmacOutput.span().subspan(32, 32));

            if (first) {
                outputs.prf = AuthenticationExtensionsClientOutputs::PRFOutputs { };
                outputs.prf->results = AuthenticationExtensionsClientOutputs::PRFValues { first, second };
            }
        }
    }

    return outputs;
}

RefPtr<AuthenticatorAttestationResponse> readCTAPMakeCredentialResponse(const Vector<uint8_t>& inBuffer, WebCore::AuthenticatorAttachment attachment, Vector<AuthenticatorTransport>&& transports, const AttestationConveyancePreference& attestation)
{
    auto decodedMap = decodeResponseMap(inBuffer);
    if (!decodedMap)
        return nullptr;
    const auto& responseMap = decodedMap->getMap();

    auto it = responseMap.find(CBOR(kCtapMakeCredentialResponseFormatKey));
    if (it == responseMap.end() || !it->second.isString())
        return nullptr;
    auto format = it->second.clone();

    it = responseMap.find(CBOR(kCtapMakeCredentialResponseAuthDataKey));
    if (it == responseMap.end() || !it->second.isByteString())
        return nullptr;
    auto authenticatorData = it->second.clone();
    auto& authDataBytes = authenticatorData.getByteString();

    auto credentialId = getCredentialId(authDataBytes);
    if (credentialId.isEmpty())
        return nullptr;

    it = responseMap.find(CBOR(kCtapMakeCredentialResponseAttStmtKey));
    if (it == responseMap.end() || !it->second.isMap())
        return nullptr;
    auto attStmt = it->second.clone();

    auto extensions = parseAuthenticatorDataExtensions(authDataBytes);
    if (!extensions)
        extensions = AuthenticationExtensionsClientOutputs { };

    std::optional<Vector<uint8_t>> attestationObject;
    if (attestation == AttestationConveyancePreference::None) {
        attestationObject = buildAttestationObject(Vector<uint8_t>(authDataBytes), String { emptyString() }, { }, attestation, ShouldZeroAAGUID::Yes);
    } else {
        CBOR::MapValue attestationObjectMap;
        attestationObjectMap[CBOR("authData")] = WTFMove(authenticatorData);
        attestationObjectMap[CBOR("fmt")] = WTFMove(format);
        attestationObjectMap[CBOR("attStmt")] = WTFMove(attStmt);
        attestationObject = cbor::CBORWriter::write(CBOR(WTFMove(attestationObjectMap)));
    }

    return AuthenticatorAttestationResponse::create(credentialId, *attestationObject, WTFMove(extensions), attachment, WTFMove(transports));
}

RefPtr<AuthenticatorAssertionResponse> readCTAPGetAssertionResponse(const Vector<uint8_t>& inBuffer, WebCore::AuthenticatorAttachment attachment)
{
    auto decodedMap = decodeResponseMap(inBuffer);
    if (!decodedMap)
        return nullptr;
    const auto& responseMap = decodedMap->getMap();

    auto it = responseMap.find(CBOR(kCtapGetAssertionResponseCredentialKey));
    if (it == responseMap.end() || !it->second.isMap())
        return nullptr;
    auto& credential = it->second.getMap();
    auto itr = credential.find(CBOR(kCredentialIdKey));
    if (itr == credential.end() || !itr->second.isByteString())
        return nullptr;
    auto& credentialId = itr->second.getByteString();

    it = responseMap.find(CBOR(kCtapGetAssertionResponseAuthDataKey));
    if (it == responseMap.end() || !it->second.isByteString())
        return nullptr;
    auto& authData = it->second.getByteString();

    it = responseMap.find(CBOR(kCtapGetAssertionResponseSignatureKey));
    if (it == responseMap.end() || !it->second.isByteString())
        return nullptr;
    auto& signature = it->second.getByteString();

    // Parse extensions from authenticatorData
    auto extensions = parseAuthenticatorDataExtensions(authData);
    if (!extensions)
        extensions = AuthenticationExtensionsClientOutputs { };

    RefPtr<AuthenticatorAssertionResponse> response;
    it = responseMap.find(CBOR(kCtapGetAssertionResponseUserKey));
    if (it != responseMap.end() && it->second.isMap()) {
        auto& user = it->second.getMap();
        auto itr = user.find(CBOR(kEntityIdMapKey));
        if (itr == user.end() || !itr->second.isByteString())
            return nullptr;
        auto& userHandle = itr->second.getByteString();
        response = AuthenticatorAssertionResponse::create(credentialId, authData, signature, userHandle, WTFMove(extensions), attachment);

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
        response = AuthenticatorAssertionResponse::create(credentialId, authData, signature, { }, WTFMove(extensions), attachment);
    }

    it = responseMap.find(CBOR(kCtapGetAssertionResponseNumberOfCredentialsKey));
    if (it != responseMap.end() && it->second.isUnsigned())
        response->setNumberOfCredentials(it->second.getUnsigned());

    return response;
}

std::optional<AuthenticatorGetInfoResponse> readCTAPGetInfoResponse(const Vector<uint8_t>& inBuffer)
{
    auto decodedMap = decodeResponseMap(inBuffer);
    if (!decodedMap)
        return std::nullopt;
    const auto& responseMap = decodedMap->getMap();

    auto it = responseMap.find(CBOR(kCtapAuthenticatorGetInfoVersionsKey));
    if (it == responseMap.end() || !it->second.isArray())
        return std::nullopt;
    StdSet<ProtocolVersion> protocolVersions;
    for (const auto& version : it->second.getArray()) {
        if (!version.isString())
            return std::nullopt;

        auto protocol = convertStringToProtocolVersion(version.getString());
        if (protocol == ProtocolVersion::kUnknown) {
            LOG_ERROR("Unexpected protocol version received.");
            continue;
        }

        if (!protocolVersions.insert(protocol).second)
            return std::nullopt;
    }
    if (protocolVersions.empty())
        return std::nullopt;

    it = responseMap.find(CBOR(kCtapAuthenticatorGetInfoAAGUIDKey));
    if (it == responseMap.end() || !it->second.isByteString() || it->second.getByteString().size() != aaguidLength)
        return std::nullopt;

    AuthenticatorGetInfoResponse response(WTFMove(protocolVersions), Vector<uint8_t>(it->second.getByteString()));

    it = responseMap.find(CBOR(kCtapAuthenticatorGetInfoExtensionsKey));
    if (it != responseMap.end()) {
        if (!it->second.isArray())
            return std::nullopt;

        Vector<String> extensions;
        for (const auto& extension : it->second.getArray()) {
            if (!extension.isString())
                return std::nullopt;

            extensions.append(extension.getString());
        }
        response.setExtensions(WTFMove(extensions));
    }

    AuthenticatorSupportedOptions options;
    it = responseMap.find(CBOR(kCtapAuthenticatorGetInfoOptionsKey));
    if (it != responseMap.end()) {
        if (!it->second.isMap())
            return std::nullopt;
        const auto& optionMap = it->second.getMap();
        auto optionMapIt = optionMap.find(CBOR(kPlatformDeviceMapKey));
        if (optionMapIt != optionMap.end()) {
            if (!optionMapIt->second.isBool())
                return std::nullopt;

            options.setIsPlatformDevice(optionMapIt->second.getBool());
        }

        optionMapIt = optionMap.find(CBOR(kResidentKeyMapKey));
        if (optionMapIt != optionMap.end()) {
            if (!optionMapIt->second.isBool())
                return std::nullopt;
            if (optionMapIt->second.getBool())
                options.setResidentKeyAvailability(AuthenticatorSupportedOptions::ResidentKeyAvailability::kSupported);
            else
                options.setResidentKeyAvailability(AuthenticatorSupportedOptions::ResidentKeyAvailability::kNotSupported);
        }

        optionMapIt = optionMap.find(CBOR(kUserPresenceMapKey));
        if (optionMapIt != optionMap.end()) {
            if (!optionMapIt->second.isBool())
                return std::nullopt;

            options.setUserPresenceRequired(optionMapIt->second.getBool());
        }

        optionMapIt = optionMap.find(CBOR(kUserVerificationMapKey));
        if (optionMapIt != optionMap.end()) {
            if (!optionMapIt->second.isBool())
                return std::nullopt;

            if (optionMapIt->second.getBool())
                options.setUserVerificationAvailability(AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedAndConfigured);
            else
                options.setUserVerificationAvailability(AuthenticatorSupportedOptions::UserVerificationAvailability::kSupportedButNotConfigured);
        }

        optionMapIt = optionMap.find(CBOR(kClientPinMapKey));
        if (optionMapIt != optionMap.end()) {
            if (!optionMapIt->second.isBool())
                return std::nullopt;

            if (optionMapIt->second.getBool())
                options.setClientPinAvailability(AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedAndPinSet);
            else
                options.setClientPinAvailability(AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedButPinNotSet);
        }
        response.setOptions(WTFMove(options));
    }

    it = responseMap.find(CBOR(kCtapAuthenticatorGetInfoMaxMsgSizeKey));
    if (it != responseMap.end()) {
        if (!it->second.isUnsigned())
            return std::nullopt;

        response.setMaxMsgSize(it->second.getUnsigned());
    }

    it = responseMap.find(CBOR(kCtapAuthenticatorGetInfoPinUVAuthProtocolsKey));
    if (it != responseMap.end()) {
        if (!it->second.isArray())
            return std::nullopt;

        StdSet<PINUVAuthProtocol> supportedPinProtocols;
        for (const auto& protocol : it->second.getArray()) {
            if (!protocol.isUnsigned())
                return std::nullopt;

            auto value = protocol.getUnsigned();
            if (value == static_cast<uint8_t>(PINUVAuthProtocol::kPinProtocol1))
                supportedPinProtocols.insert(PINUVAuthProtocol::kPinProtocol1);
            else if (value == static_cast<uint8_t>(PINUVAuthProtocol::kPinProtocol2))
                supportedPinProtocols.insert(PINUVAuthProtocol::kPinProtocol2);
            // Ignore unknown protocols
        }
        response.setPinProtocols(WTFMove(supportedPinProtocols));
    }

    it = responseMap.find(CBOR(kCtapAuthenticatorGetInfoMaxCredentialCountInListKey));
    if (it != responseMap.end()) {
        if (!it->second.isUnsigned())
            return std::nullopt;

        response.setMaxCredentialCountInList(it->second.getUnsigned());
    }
    it = responseMap.find(CBOR(kCtapAuthenticatorGetInfoMaxCredentialIdLengthKey));
    if (it != responseMap.end()) {
        if (!it->second.isUnsigned())
            return std::nullopt;

        response.setMaxCredentialIDLength(it->second.getUnsigned());
    }

    it = responseMap.find(CBOR(kCtapAuthenticatorGetInfoTransportsKey));
    if (it != responseMap.end()) {
        if (!it->second.isArray())
            return std::nullopt;

        Vector<AuthenticatorTransport> transports;
        for (const auto& transportString : it->second.getArray()) {
            if (!transportString.isString())
                return std::nullopt;
            auto transport = convertStringToAuthenticatorTransport(transportString.getString());
            if (transport)
                transports.append(*transport);
        }
        response.setTransports(WTFMove(transports));
    }

    it = responseMap.find(CBOR(kCtapAuthenticatorGetInfoMinPINLengthKey));
    if (it != responseMap.end()) {
        if (!it->second.isUnsigned())
            return std::nullopt;
        response.setMinPINLength(it->second.getUnsigned());
    }

    it = responseMap.find(CBOR(kCtapAuthenticatorGetInfoRemainingDiscoverableCredentialsKey));
    if (it != responseMap.end()) {
        if (!it->second.isUnsigned())
            return std::nullopt;

        response.setRemainingDiscoverableCredentials(it->second.getUnsigned());
    }

    return WTFMove(response);
}

} // namespace fido

#endif // ENABLE(WEB_AUTHN)

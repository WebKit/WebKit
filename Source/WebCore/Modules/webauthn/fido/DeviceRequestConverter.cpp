// Copyright 2017 The Chromium Authors. All rights reserved.
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
#include "DeviceRequestConverter.h"

#if ENABLE(WEB_AUTHN)

#include "CBORWriter.h"
#include "Pin.h"
#include "PublicKeyCredentialCreationOptions.h"
#include "PublicKeyCredentialDescriptor.h"
#include "PublicKeyCredentialRequestOptions.h"
#include "PublicKeyCredentialRpEntity.h"
#include "PublicKeyCredentialUserEntity.h"
#include "ResidentKeyRequirement.h"
#include "WebAuthenticationConstants.h"
#include <wtf/Vector.h>

namespace fido {
using namespace WebCore;
using namespace cbor;

using UVAvailability = AuthenticatorSupportedOptions::UserVerificationAvailability;

static CBORValue convertRpEntityToCBOR(const PublicKeyCredentialRpEntity& rpEntity)
{
    CBORValue::MapValue rpMap;
    rpMap.emplace(CBORValue(kEntityNameMapKey), CBORValue(rpEntity.name));
    if (!rpEntity.icon.isEmpty())
        rpMap.emplace(CBORValue(kIconUrlMapKey), CBORValue(rpEntity.icon));
    if (!rpEntity.id.isEmpty())
        rpMap.emplace(CBORValue(kEntityIdMapKey), CBORValue(rpEntity.id));

    return CBORValue(WTFMove(rpMap));
}

static CBORValue convertUserEntityToCBOR(const PublicKeyCredentialUserEntity& userEntity)
{
    CBORValue::MapValue userMap;
    userMap.emplace(CBORValue(kEntityNameMapKey), CBORValue(userEntity.name));
    if (!userEntity.icon.isEmpty())
        userMap.emplace(CBORValue(kIconUrlMapKey), CBORValue(userEntity.icon));
    userMap.emplace(CBORValue(kEntityIdMapKey), CBORValue(userEntity.id));
    userMap.emplace(CBORValue(kDisplayNameMapKey), CBORValue(userEntity.displayName));
    return CBORValue(WTFMove(userMap));
}

static CBORValue convertParametersToCBOR(const Vector<PublicKeyCredentialParameters>& parameters)
{
    auto credentialParamArray = parameters.map([](auto& credential) {
        CBORValue::MapValue cborCredentialMap;
        cborCredentialMap.emplace(CBORValue(kCredentialTypeMapKey), CBORValue(publicKeyCredentialTypeToString(credential.type)));
        cborCredentialMap.emplace(CBORValue(kCredentialAlgorithmMapKey), CBORValue(credential.alg));
        return CBORValue { WTFMove(cborCredentialMap) };
    });
    return CBORValue(WTFMove(credentialParamArray));
}

static CBORValue convertDescriptorToCBOR(const PublicKeyCredentialDescriptor& descriptor)
{
    CBORValue::MapValue cborDescriptorMap;
    cborDescriptorMap[CBORValue(kCredentialTypeKey)] = CBORValue(publicKeyCredentialTypeToString(descriptor.type));
    cborDescriptorMap[CBORValue(kCredentialIdKey)] = CBORValue(descriptor.id);
    return CBORValue(WTFMove(cborDescriptorMap));
}

static Vector<PublicKeyCredentialParameters> trimmedParameters(const Vector<PublicKeyCredentialParameters>& parameters, const std::optional<Vector<WebCore::PublicKeyCredentialParameters>>& authenticatorSupportedParameters)
{
    HashSet<int64_t> authenticatorSupportedAlgorithms;
    if (authenticatorSupportedParameters) {
        for (auto& parameters : *authenticatorSupportedParameters) {
            if (parameters.type == PublicKeyCredentialType::PublicKey)
                authenticatorSupportedAlgorithms.add(parameters.alg);
        }
    }

    for (auto& parameter : parameters) {
        if (parameter.type != PublicKeyCredentialType::PublicKey)
            continue;
        if (authenticatorSupportedAlgorithms.contains(parameter.alg))
            return { parameter };
        // Support for ES256 required by U2F backwards compatibility.
        // https://fidoalliance.org/specs/fido-v2.0-id-20180227/fido-client-to-authenticator-protocol-v2.0-id-20180227.html#u2f-authenticatorMakeCredential-interoperability
        if (parameter.alg == COSE::ES256)
            return { parameter };
    }

    // We know the algorithms the authenticator supports and none of those were requested.
    if (authenticatorSupportedAlgorithms.size())
        return { parameters.first() };

    return parameters;
}

Vector<uint8_t> encodeMakeCredentialRequestAsCBOR(const Vector<uint8_t>& hash, const PublicKeyCredentialCreationOptions& options, UVAvailability uvCapability, AuthenticatorSupportedOptions::ResidentKeyAvailability residentKeyAvailability, const Vector<String>& authenticatorSupportedExtensions, std::optional<PinParameters> pin, const std::optional<Vector<WebCore::PublicKeyCredentialParameters>>& authenticatorSupportedParameters, std::optional<Vector<PublicKeyCredentialDescriptor>>&& overrideExcludeCredentials, std::optional<HmacSecretParameters>&& hmacSecretParams)
{
    CBORValue::MapValue cborMap;
    cborMap[CBORValue(kCtapMakeCredentialClientDataHashKey)] = CBORValue(hash);
    cborMap[CBORValue(kCtapMakeCredentialRpKey)] = convertRpEntityToCBOR(options.rp);
    cborMap[CBORValue(kCtapMakeCredentialUserKey)] = convertUserEntityToCBOR(options.user);
    cborMap[CBORValue(kCtapMakeCredentialPubKeyCredParamsKey)] = convertParametersToCBOR(trimmedParameters(options.pubKeyCredParams, authenticatorSupportedParameters));
    if (overrideExcludeCredentials) {
        if (!overrideExcludeCredentials->isEmpty()) {
            CBORValue::ArrayValue excludeListArray;
            for (const auto& descriptor : *overrideExcludeCredentials)
                excludeListArray.append(convertDescriptorToCBOR(descriptor));
            cborMap[CBORValue(kCtapMakeCredentialExcludeListKey)] = CBORValue(WTFMove(excludeListArray));
        }
    } else if (!options.excludeCredentials.isEmpty()) {
        CBORValue::ArrayValue excludeListArray;
        for (const auto& descriptor : options.excludeCredentials)
            excludeListArray.append(convertDescriptorToCBOR(descriptor));
        cborMap[CBORValue(kCtapMakeCredentialExcludeListKey)] = CBORValue(WTFMove(excludeListArray));
    }

    if ((authenticatorSupportedExtensions.size() && options.extensions) || hmacSecretParams) {
        CBORValue::MapValue extensionsMap;

        if (options.extensions) {
            auto largeBlobInputs = options.extensions->largeBlob;
            if (largeBlobInputs && authenticatorSupportedExtensions.contains("largeBlob"_s)) {
                CBORValue::MapValue largeBlobMap;

                if (!largeBlobInputs->support.isNull())
                    largeBlobMap[CBORValue("support"_s)] = CBORValue(largeBlobInputs->support);

                extensionsMap[CBORValue("largeBlob"_s)] = CBORValue(WTFMove(largeBlobMap));
            }

            // Convert prf extension to hmac-secret: true for makeCredential
            if (options.extensions->prf && authenticatorSupportedExtensions.contains(kExtensionHmacSecret))
                extensionsMap[CBORValue(kExtensionHmacSecret)] = CBORValue(true);
        }

        // Add hmac-secret-mc extension with encrypted parameters
        if (hmacSecretParams && authenticatorSupportedExtensions.contains(kExtensionHmacSecretMc)) {
            CBORValue::MapValue hmacSecretMcMap;
            hmacSecretMcMap[CBORValue(kHmacSecretKeyAgreementKey)] = CBORValue(WTFMove(hmacSecretParams->keyAgreement));
            hmacSecretMcMap[CBORValue(kHmacSecretSaltEncKey)] = CBORValue(WTFMove(hmacSecretParams->saltEnc));
            hmacSecretMcMap[CBORValue(kHmacSecretSaltAuthKey)] = CBORValue(WTFMove(hmacSecretParams->saltAuth));
            hmacSecretMcMap[CBORValue(kHmacSecretPinUvAuthProtocolKey)] = CBORValue(hmacSecretParams->protocol);
            extensionsMap[CBORValue(kExtensionHmacSecretMc)] = CBORValue(WTFMove(hmacSecretMcMap));
        }

        if (!extensionsMap.empty())
            cborMap[CBORValue(kCtapMakeCredentialExtensionsKey)] = CBORValue(WTFMove(extensionsMap));
    }

    CBORValue::MapValue optionMap;
    if (options.authenticatorSelection) {
        // Resident keys are not supported by default.
        if (auto residentKey = options.authenticatorSelection->residentKey()) {
            if (*residentKey == ResidentKeyRequirement::Required
                || (*residentKey == ResidentKeyRequirement::Preferred && residentKeyAvailability == AuthenticatorSupportedOptions::ResidentKeyAvailability::kSupported))
                optionMap[CBORValue(kResidentKeyMapKey)] = CBORValue(true);
        } else if (options.authenticatorSelection->requireResidentKey)
            optionMap[CBORValue(kResidentKeyMapKey)] = CBORValue(true);

        // User verification is not required by default.
        bool requireUserVerification = false;
        switch (options.authenticatorSelection->userVerification()) {
        case UserVerificationRequirement::Required:
        case UserVerificationRequirement::Preferred:
            requireUserVerification = uvCapability == UVAvailability::kSupportedAndConfigured;
            break;
        case UserVerificationRequirement::Discouraged:
            requireUserVerification = false;
        }
        if (requireUserVerification)
            optionMap[CBORValue(kUserVerificationMapKey)] = CBORValue(requireUserVerification);
    }
    if (!optionMap.empty())
        cborMap[CBORValue(kCtapMakeCredentialRequestOptionsKey)] = CBORValue(WTFMove(optionMap));

    if (pin) {
        ASSERT(pin->protocol >= 0);
        cborMap[CBORValue(kCtapMakeCredentialPinUvAuthParamKey)] = CBORValue(WTFMove(pin->auth));
        cborMap[CBORValue(kCtapMakeCredentialPinUvAuthProtocolKey)] = CBORValue(pin->protocol);
    }

    auto serializedParam = CBORWriter::write(CBORValue(WTFMove(cborMap)));
    ASSERT(serializedParam);

    Vector<uint8_t> cborRequest({ static_cast<uint8_t>(CtapRequestCommand::kAuthenticatorMakeCredential) });
    cborRequest.appendVector(*serializedParam);
    return cborRequest;
}

Vector<uint8_t> encodeSilentGetAssertion(const String& rpId, const Vector<uint8_t>& hash, const Vector<PublicKeyCredentialDescriptor>& credentials, std::optional<PinParameters> pin)
{
    CBORValue::MapValue cborMap;
    cborMap[CBORValue(kCtapGetAssertionRpIdKey)] = CBORValue(rpId);
    cborMap[CBORValue(kCtapGetAssertionClientDataHashKey)] = CBORValue(hash);

    CBORValue::ArrayValue allowListArray;
    for (const auto& descriptor : credentials)
        allowListArray.append(convertDescriptorToCBOR(descriptor));
    cborMap[CBORValue(kCtapGetAssertionAllowListKey)] = CBORValue(WTFMove(allowListArray));

    if (pin) {
        ASSERT(pin->protocol >= 0);
        cborMap[CBORValue(kCtapGetAssertionPinUvAuthParamKey)] = CBORValue(WTFMove(pin->auth));
        cborMap[CBORValue(kCtapGetAssertionPinUvAuthProtocolKey)] = CBORValue(pin->protocol);
    }

    CBORValue::MapValue optionMap;
    optionMap[CBORValue(kUserPresenceMapKey)] = CBORValue(false);
    cborMap[CBORValue(kCtapGetAssertionRequestOptionsKey)] = CBORValue(WTFMove(optionMap));

    auto serializedParam = CBORWriter::write(CBORValue(WTFMove(cborMap)));
    ASSERT(serializedParam);

    Vector<uint8_t> cborRequest({ static_cast<uint8_t>(CtapRequestCommand::kAuthenticatorGetAssertion) });
    cborRequest.appendVector(*serializedParam);
    return cborRequest;
}

Vector<uint8_t> encodeGetAssertionRequestAsCBOR(const Vector<uint8_t>& hash, const PublicKeyCredentialRequestOptions& options, UVAvailability uvCapability, const Vector<String>& authenticatorSupportedExtensions, std::optional<PinParameters> pin, std::optional<Vector<PublicKeyCredentialDescriptor>>&& overrideAllowCredentials, std::optional<HmacSecretParameters>&& hmacSecretParams)
{
    CBORValue::MapValue cborMap;
    cborMap[CBORValue(kCtapGetAssertionRpIdKey)] = CBORValue(options.rpId);
    cborMap[CBORValue(kCtapGetAssertionClientDataHashKey)] = CBORValue(hash);
    if (overrideAllowCredentials) {
        CBORValue::ArrayValue allowListArray;
        for (const auto& descriptor : *overrideAllowCredentials)
            allowListArray.append(convertDescriptorToCBOR(descriptor));
        cborMap[CBORValue(kCtapGetAssertionAllowListKey)] = CBORValue(WTFMove(allowListArray));
    } else if (!options.allowCredentials.isEmpty()) {
        CBORValue::ArrayValue allowListArray;
        for (const auto& descriptor : options.allowCredentials)
            allowListArray.append(convertDescriptorToCBOR(descriptor));
        cborMap[CBORValue(kCtapGetAssertionAllowListKey)] = CBORValue(WTFMove(allowListArray));
    }

    if ((authenticatorSupportedExtensions.size() && options.extensions) || hmacSecretParams) {
        CBORValue::MapValue extensionsMap;

        if (options.extensions) {
            auto largeBlobInputs = options.extensions->largeBlob;
            if (largeBlobInputs && authenticatorSupportedExtensions.contains("largeBlob"_s)) {
                CBORValue::MapValue largeBlobMap;

                if (largeBlobInputs->read)
                    largeBlobMap[CBORValue("read"_s)] = CBORValue(largeBlobInputs->read.value());

                if (largeBlobInputs->write)
                    largeBlobMap[CBORValue("write"_s)] = CBORValue(BufferSource { WTFMove(*largeBlobInputs->write) });

                extensionsMap[CBORValue("largeBlob"_s)] = CBORValue(WTFMove(largeBlobMap));
            }
        }

        // Add hmac-secret extension with encrypted salt parameters
        if (hmacSecretParams && authenticatorSupportedExtensions.contains(kExtensionHmacSecret)) {
            CBORValue::MapValue hmacSecretMap;
            hmacSecretMap[CBORValue(kHmacSecretKeyAgreementKey)] = CBORValue(WTFMove(hmacSecretParams->keyAgreement));
            hmacSecretMap[CBORValue(kHmacSecretSaltEncKey)] = CBORValue(WTFMove(hmacSecretParams->saltEnc));
            hmacSecretMap[CBORValue(kHmacSecretSaltAuthKey)] = CBORValue(WTFMove(hmacSecretParams->saltAuth));
            hmacSecretMap[CBORValue(kHmacSecretPinUvAuthProtocolKey)] = CBORValue(hmacSecretParams->protocol);
            extensionsMap[CBORValue(kExtensionHmacSecret)] = CBORValue(WTFMove(hmacSecretMap));
        }

        if (!extensionsMap.empty())
            cborMap[CBORValue(kCtapGetAssertionExtensionsKey)] = CBORValue(WTFMove(extensionsMap));
    }

    CBORValue::MapValue optionMap;
    // User verification is not required by default.
    bool requireUserVerification = false;
    switch (options.userVerification()) {
    case UserVerificationRequirement::Required:
    case UserVerificationRequirement::Preferred:
        requireUserVerification = uvCapability == UVAvailability::kSupportedAndConfigured;
        break;
    case UserVerificationRequirement::Discouraged:
        requireUserVerification = false;
    }
    if (requireUserVerification)
        optionMap[CBORValue(kUserVerificationMapKey)] = CBORValue(requireUserVerification);
    optionMap[CBORValue(kUserPresenceMapKey)] = CBORValue(true);

    if (!optionMap.empty())
        cborMap[CBORValue(kCtapGetAssertionRequestOptionsKey)] = CBORValue(WTFMove(optionMap));

    if (pin) {
        ASSERT(pin->protocol >= 0);
        cborMap[CBORValue(kCtapGetAssertionPinUvAuthParamKey)] = CBORValue(WTFMove(pin->auth));
        cborMap[CBORValue(kCtapGetAssertionPinUvAuthProtocolKey)] = CBORValue(pin->protocol);
    }

    auto serializedParam = CBORWriter::write(CBORValue(WTFMove(cborMap)));
    ASSERT(serializedParam);

    Vector<uint8_t> cborRequest({ static_cast<uint8_t>(CtapRequestCommand::kAuthenticatorGetAssertion) });
    cborRequest.appendVector(*serializedParam);
    return cborRequest;
}

Vector<uint8_t> encodeBogusRequestForAuthenticatorSelection()
{
    CBORValue::MapValue cborMap;
    cborMap[CBORValue(kCtapMakeCredentialClientDataHashKey)] = CBORValue(Vector<uint8_t>(0, 32));
    CBORValue::MapValue rpMap;
    rpMap.emplace(CBORValue(kEntityNameMapKey), CBORValue("notarealwebsite.com"));
    rpMap.emplace(CBORValue(kEntityIdMapKey), CBORValue("notarealwebsite.com"));
    cborMap[CBORValue(kCtapMakeCredentialRpKey)] = CBORValue(rpMap);

    CBORValue::MapValue userMap;
    userMap.emplace(CBORValue(kEntityNameMapKey), CBORValue("bogus"_s));
    userMap.emplace(CBORValue(kEntityIdMapKey), CBORValue(Vector<uint8_t> { 0 }));
    userMap.emplace(CBORValue(kDisplayNameMapKey), CBORValue("bogus"_s));

    cborMap[CBORValue(kCtapMakeCredentialUserKey)] = CBORValue(userMap);
    cborMap[CBORValue(kCtapMakeCredentialPubKeyCredParamsKey)] = convertParametersToCBOR({ { PublicKeyCredentialType::PublicKey, COSE::ES256 } });
    cborMap[CBORValue(kCtapMakeCredentialPinUvAuthParamKey)] = CBORValue(Vector<uint8_t> { });
    cborMap[CBORValue(kCtapMakeCredentialPinUvAuthProtocolKey)] = CBORValue(pin::kProtocolVersion);

    auto serializedParam = CBORWriter::write(CBORValue(WTFMove(cborMap)));
    ASSERT(serializedParam);

    Vector<uint8_t> cborRequest({ static_cast<uint8_t>(CtapRequestCommand::kAuthenticatorMakeCredential) });
    cborRequest.appendVector(*serializedParam);
    return cborRequest;
}

Vector<uint8_t> encodeEmptyAuthenticatorRequest(CtapRequestCommand cmd)
{
    return { static_cast<uint8_t>(cmd) };
}

} // namespace fido

#endif // ENABLE(WEB_AUTHN)

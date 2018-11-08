// Copyright 2017 The Chromium Authors. All rights reserved.
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
#include "DeviceRequestConverter.h"

#if ENABLE(WEB_AUTHN)

#include "CBORWriter.h"
#include "PublicKeyCredentialCreationOptions.h"
#include "PublicKeyCredentialRequestOptions.h"
#include <wtf/Vector.h>

namespace fido {
using namespace WebCore;
using namespace cbor;

using UVAvailability = AuthenticatorSupportedOptions::UserVerificationAvailability;

static CBORValue convertRpEntityToCBOR(const PublicKeyCredentialCreationOptions::RpEntity& rpEntity)
{
    CBORValue::MapValue rpMap;
    rpMap.emplace(CBORValue(kEntityNameMapKey), CBORValue(rpEntity.name));
    if (!rpEntity.icon.isEmpty())
        rpMap.emplace(CBORValue(kIconUrlMapKey), CBORValue(rpEntity.icon));
    if (!rpEntity.id.isEmpty())
        rpMap.emplace(CBORValue(kEntityIdMapKey), CBORValue(rpEntity.id));

    return CBORValue(WTFMove(rpMap));
}

static CBORValue convertUserEntityToCBOR(const PublicKeyCredentialCreationOptions::UserEntity& userEntity)
{
    CBORValue::MapValue userMap;
    userMap.emplace(CBORValue(kEntityNameMapKey), CBORValue(userEntity.name));
    if (!userEntity.icon.isEmpty())
        userMap.emplace(CBORValue(kIconUrlMapKey), CBORValue(userEntity.icon));
    userMap.emplace(CBORValue(kEntityIdMapKey), CBORValue(userEntity.idVector));
    userMap.emplace(CBORValue(kDisplayNameMapKey), CBORValue(userEntity.displayName));
    return CBORValue(WTFMove(userMap));
}

static CBORValue convertParametersToCBOR(const Vector<PublicKeyCredentialCreationOptions::Parameters>& parameters)
{
    CBORValue::ArrayValue credentialParamArray;
    credentialParamArray.reserveInitialCapacity(parameters.size());
    for (const auto& credential : parameters) {
        CBORValue::MapValue cborCredentialMap;
        cborCredentialMap.emplace(CBORValue(kCredentialTypeMapKey), CBORValue(publicKeyCredentialTypeToString(credential.type)));
        cborCredentialMap.emplace(CBORValue(kCredentialAlgorithmMapKey), CBORValue(credential.alg));
        credentialParamArray.append(WTFMove(cborCredentialMap));
    }
    return CBORValue(WTFMove(credentialParamArray));
}

static CBORValue convertDescriptorToCBOR(const PublicKeyCredentialDescriptor& descriptor)
{
    CBORValue::MapValue cborDescriptorMap;
    cborDescriptorMap[CBORValue(kCredentialTypeKey)] = CBORValue(publicKeyCredentialTypeToString(descriptor.type));
    cborDescriptorMap[CBORValue(kCredentialIdKey)] = CBORValue(descriptor.idVector);
    return CBORValue(WTFMove(cborDescriptorMap));
}

Vector<uint8_t> encodeMakeCredenitalRequestAsCBOR(const Vector<uint8_t>& hash, const PublicKeyCredentialCreationOptions& options, UVAvailability uvCapability)
{
    CBORValue::MapValue cborMap;
    cborMap[CBORValue(1)] = CBORValue(hash);
    cborMap[CBORValue(2)] = convertRpEntityToCBOR(options.rp);
    cborMap[CBORValue(3)] = convertUserEntityToCBOR(options.user);
    cborMap[CBORValue(4)] = convertParametersToCBOR(options.pubKeyCredParams);
    if (!options.excludeCredentials.isEmpty()) {
        CBORValue::ArrayValue excludeListArray;
        for (const auto& descriptor : options.excludeCredentials)
            excludeListArray.append(convertDescriptorToCBOR(descriptor));
        cborMap[CBORValue(5)] = CBORValue(WTFMove(excludeListArray));
    }

    CBORValue::MapValue optionMap;
    if (options.authenticatorSelection) {
        // Resident keys are not supported by default.
        if (options.authenticatorSelection->requireResidentKey)
            optionMap[CBORValue(kResidentKeyMapKey)] = CBORValue(true);

        // User verification is not required by default.
        bool requireUserVerification = false;
        switch (options.authenticatorSelection->userVerification) {
        case UserVerificationRequirement::Required:
            requireUserVerification = true;
            break;
        case UserVerificationRequirement::Preferred:
            requireUserVerification = uvCapability == UVAvailability::kNotSupported ? false : true;
            break;
        case UserVerificationRequirement::Discouraged:
            requireUserVerification = false;
        }
        optionMap[CBORValue(kUserVerificationMapKey)] = CBORValue(requireUserVerification);
    }
    if (!optionMap.empty())
        cborMap[CBORValue(7)] = CBORValue(WTFMove(optionMap));

    auto serializedParam = CBORWriter::write(CBORValue(WTFMove(cborMap)));
    ASSERT(serializedParam);

    Vector<uint8_t> cborRequest({ static_cast<uint8_t>(CtapRequestCommand::kAuthenticatorMakeCredential) });
    cborRequest.appendVector(*serializedParam);
    return cborRequest;
}

Vector<uint8_t> encodeGetAssertionRequestAsCBOR(const Vector<uint8_t>& hash, const PublicKeyCredentialRequestOptions& options, UVAvailability uvCapability)
{
    CBORValue::MapValue cborMap;
    cborMap[CBORValue(1)] = CBORValue(options.rpId);
    cborMap[CBORValue(2)] = CBORValue(hash);

    if (!options.allowCredentials.isEmpty()) {
        CBORValue::ArrayValue allowListArray;
        for (const auto& descriptor : options.allowCredentials)
            allowListArray.append(convertDescriptorToCBOR(descriptor));
        cborMap[CBORValue(3)] = CBORValue(WTFMove(allowListArray));
    }

    CBORValue::MapValue optionMap;
    // User verification is not required by default.
    bool requireUserVerification = false;
    switch (options.userVerification) {
    case UserVerificationRequirement::Required:
        requireUserVerification = true;
        break;
    case UserVerificationRequirement::Preferred:
        requireUserVerification = uvCapability == UVAvailability::kNotSupported ? false : true;
        break;
    case UserVerificationRequirement::Discouraged:
        requireUserVerification = false;
    }
    optionMap[CBORValue(kUserVerificationMapKey)] = CBORValue(requireUserVerification);
    optionMap[CBORValue(kUserPresenceMapKey)] = CBORValue(!requireUserVerification);

    if (!optionMap.empty())
        cborMap[CBORValue(5)] = CBORValue(WTFMove(optionMap));

    auto serializedParam = CBORWriter::write(CBORValue(WTFMove(cborMap)));
    ASSERT(serializedParam);

    Vector<uint8_t> cborRequest({ static_cast<uint8_t>(CtapRequestCommand::kAuthenticatorGetAssertion) });
    cborRequest.appendVector(*serializedParam);
    return cborRequest;
}

Vector<uint8_t> encodeEmptyAuthenticatorRequest(CtapRequestCommand cmd)
{
    return { static_cast<uint8_t>(cmd) };
}

} // namespace fido

#endif // ENABLE(WEB_AUTHN)

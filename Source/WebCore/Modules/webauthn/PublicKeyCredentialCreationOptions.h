/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEB_AUTHN)
#include "AttestationConveyancePreference.h"
#include "AuthenticationExtensionsClientInputs.h"
#include "BufferSource.h"
#include "PublicKeyCredentialDescriptor.h"
#include "PublicKeyCredentialType.h"
#include "ResidentKeyRequirement.h"
#include "UserVerificationRequirement.h"
#include <wtf/Forward.h>
#endif // ENABLE(WEB_AUTHN)

namespace WebCore {

enum class AuthenticatorAttachment;

struct PublicKeyCredentialCreationOptions {
#if ENABLE(WEB_AUTHN)
    struct Entity {
        String name;
        String icon;
    };

    struct RpEntity : public Entity {
        mutable std::optional<String> id;
    };

    struct UserEntity : public Entity {
        BufferSource id;
        String displayName;
    };

    struct Parameters {
        PublicKeyCredentialType type;
        int64_t alg;

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static std::optional<Parameters> decode(Decoder&);
    };

    struct AuthenticatorSelectionCriteria {
        std::optional<AuthenticatorAttachment> authenticatorAttachment;
        // residentKey replaces requireResidentKey, see: https://www.w3.org/TR/webauthn-2/#dictionary-authenticatorSelection
        std::optional<ResidentKeyRequirement> residentKey;
        bool requireResidentKey { false };
        UserVerificationRequirement userVerification { UserVerificationRequirement::Preferred };

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static std::optional<AuthenticatorSelectionCriteria> decode(Decoder&);
    };

    RpEntity rp;
    UserEntity user;

    BufferSource challenge;
    mutable Vector<Parameters> pubKeyCredParams;

    std::optional<unsigned> timeout;
    Vector<PublicKeyCredentialDescriptor> excludeCredentials;
    std::optional<AuthenticatorSelectionCriteria> authenticatorSelection;
    AttestationConveyancePreference attestation;
    mutable std::optional<AuthenticationExtensionsClientInputs> extensions;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<PublicKeyCredentialCreationOptions> decode(Decoder&);
#endif // ENABLE(WEB_AUTHN)
};

#if ENABLE(WEB_AUTHN)
template<class Encoder>
void PublicKeyCredentialCreationOptions::Parameters::encode(Encoder& encoder) const
{
    encoder << type << alg;
}

template<class Decoder>
std::optional<PublicKeyCredentialCreationOptions::Parameters> PublicKeyCredentialCreationOptions::Parameters::decode(Decoder& decoder)
{
    PublicKeyCredentialCreationOptions::Parameters result;
    if (!decoder.decode(result.type))
        return std::nullopt;
    if (!decoder.decode(result.alg))
        return std::nullopt;
    return result;
}

template<class Encoder>
void PublicKeyCredentialCreationOptions::AuthenticatorSelectionCriteria::encode(Encoder& encoder) const
{
    encoder << authenticatorAttachment << requireResidentKey << userVerification << residentKey;
}

template<class Decoder>
std::optional<PublicKeyCredentialCreationOptions::AuthenticatorSelectionCriteria> PublicKeyCredentialCreationOptions::AuthenticatorSelectionCriteria::decode(Decoder& decoder)
{
    PublicKeyCredentialCreationOptions::AuthenticatorSelectionCriteria result;

    std::optional<std::optional<AuthenticatorAttachment>> authenticatorAttachment;
    decoder >> authenticatorAttachment;
    if (!authenticatorAttachment)
        return std::nullopt;
    result.authenticatorAttachment = WTFMove(*authenticatorAttachment);

    std::optional<bool> requireResidentKey;
    decoder >> requireResidentKey;
    if (!requireResidentKey)
        return std::nullopt;
    result.requireResidentKey = *requireResidentKey;

    if (!decoder.decode(result.userVerification))
        return std::nullopt;

    std::optional<std::optional<ResidentKeyRequirement>> residentKey;
    decoder >> residentKey;
    if (!residentKey)
        return std::nullopt;
    result.residentKey = *residentKey;

    return result;
}

// Not every member is encoded.
template<class Encoder>
void PublicKeyCredentialCreationOptions::encode(Encoder& encoder) const
{
    encoder << rp.id << rp.name << rp.icon;
    encoder << user.id;
    encoder << user.displayName << user.name << user.icon << pubKeyCredParams << timeout << excludeCredentials << authenticatorSelection << attestation << extensions;
    encoder << static_cast<uint64_t>(challenge.length());
    encoder.encodeFixedLengthData(challenge.data(), challenge.length(), 1);
}

template<class Decoder>
std::optional<PublicKeyCredentialCreationOptions> PublicKeyCredentialCreationOptions::decode(Decoder& decoder)
{
    PublicKeyCredentialCreationOptions result;
    if (!decoder.decode(result.rp.id))
        return std::nullopt;
    if (!decoder.decode(result.rp.name))
        return std::nullopt;
    if (!decoder.decode(result.rp.icon))
        return std::nullopt;
    if (!decoder.decode(result.user.id))
        return std::nullopt;
    if (!decoder.decode(result.user.displayName))
        return std::nullopt;
    if (!decoder.decode(result.user.name))
        return std::nullopt;
    if (!decoder.decode(result.user.icon))
        return std::nullopt;
    if (!decoder.decode(result.pubKeyCredParams))
        return std::nullopt;

    std::optional<std::optional<unsigned>> timeout;
    decoder >> timeout;
    if (!timeout)
        return std::nullopt;
    result.timeout = WTFMove(*timeout);

    if (!decoder.decode(result.excludeCredentials))
        return std::nullopt;

    std::optional<std::optional<AuthenticatorSelectionCriteria>> authenticatorSelection;
    decoder >> authenticatorSelection;
    if (!authenticatorSelection)
        return std::nullopt;
    result.authenticatorSelection = WTFMove(*authenticatorSelection);

    std::optional<AttestationConveyancePreference> attestation;
    decoder >> attestation;
    if (!attestation)
        return std::nullopt;
    result.attestation = WTFMove(*attestation);

    std::optional<std::optional<AuthenticationExtensionsClientInputs>> extensions;
    decoder >> extensions;
    if (!extensions)
        return std::nullopt;
    result.extensions = WTFMove(*extensions);

    if (!decoder.decode(result.challenge))
        return std::nullopt;

    return result;
}
#endif // ENABLE(WEB_AUTHN)

} // namespace WebCore

/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "UserVerificationRequirement.h"
#include <wtf/CrossThreadCopier.h>
#include <wtf/Forward.h>

namespace WebCore {

struct PublicKeyCredentialCreationOptions {
    enum class AuthenticatorAttachment {
        Platform,
        CrossPlatform
    };

    struct Entity {
        String name;
        String icon;
    };

    struct RpEntity : public Entity {
        mutable String id;
    };

    struct UserEntity : public Entity {
        BufferSource id; // id becomes idVector once it is passed to UIProcess.
        Vector<uint8_t> idVector;
        String displayName;
    };

    struct Parameters {
        PublicKeyCredentialType type;
        int64_t alg;

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static Optional<Parameters> decode(Decoder&);
    };

    struct AuthenticatorSelectionCriteria {
        Optional<AuthenticatorAttachment> authenticatorAttachment;
        bool requireResidentKey { false };
        UserVerificationRequirement userVerification { UserVerificationRequirement::Preferred };

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static Optional<AuthenticatorSelectionCriteria> decode(Decoder&);
    };

    RpEntity rp;
    UserEntity user;

    BufferSource challenge;
    Vector<Parameters> pubKeyCredParams;

    Optional<unsigned> timeout;
    Vector<PublicKeyCredentialDescriptor> excludeCredentials;
    Optional<AuthenticatorSelectionCriteria> authenticatorSelection;
    AttestationConveyancePreference attestation;
    Optional<AuthenticationExtensionsClientInputs> extensions; // A place holder, but never used.

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<PublicKeyCredentialCreationOptions> decode(Decoder&);
};

template<class Encoder>
void PublicKeyCredentialCreationOptions::Parameters::encode(Encoder& encoder) const
{
    encoder << type << alg;
}

template<class Decoder>
Optional<PublicKeyCredentialCreationOptions::Parameters> PublicKeyCredentialCreationOptions::Parameters::decode(Decoder& decoder)
{
    PublicKeyCredentialCreationOptions::Parameters result;
    if (!decoder.decodeEnum(result.type))
        return WTF::nullopt;
    if (!decoder.decode(result.alg))
        return WTF::nullopt;
    return result;
}

template<class Encoder>
void PublicKeyCredentialCreationOptions::AuthenticatorSelectionCriteria::encode(Encoder& encoder) const
{
    encoder << authenticatorAttachment << requireResidentKey << userVerification;
}

template<class Decoder>
Optional<PublicKeyCredentialCreationOptions::AuthenticatorSelectionCriteria> PublicKeyCredentialCreationOptions::AuthenticatorSelectionCriteria::decode(Decoder& decoder)
{
    PublicKeyCredentialCreationOptions::AuthenticatorSelectionCriteria result;

    Optional<Optional<AuthenticatorAttachment>> authenticatorAttachment;
    decoder >> authenticatorAttachment;
    if (!authenticatorAttachment)
        return WTF::nullopt;
    result.authenticatorAttachment = WTFMove(*authenticatorAttachment);

    Optional<bool> requireResidentKey;
    decoder >> requireResidentKey;
    if (!requireResidentKey)
        return WTF::nullopt;
    result.requireResidentKey = *requireResidentKey;

    if (!decoder.decodeEnum(result.userVerification))
        return WTF::nullopt;
    return result;
}

// Not every member is encoded.
template<class Encoder>
void PublicKeyCredentialCreationOptions::encode(Encoder& encoder) const
{
    encoder << rp.id << rp.name << rp.icon;
    encoder << static_cast<uint64_t>(user.id.length());
    encoder.encodeFixedLengthData(user.id.data(), user.id.length(), 1);
    encoder << user.displayName << user.name << user.icon << pubKeyCredParams << timeout << excludeCredentials << authenticatorSelection << attestation;
}

template<class Decoder>
Optional<PublicKeyCredentialCreationOptions> PublicKeyCredentialCreationOptions::decode(Decoder& decoder)
{
    PublicKeyCredentialCreationOptions result;
    if (!decoder.decode(result.rp.id))
        return WTF::nullopt;
    if (!decoder.decode(result.rp.name))
        return WTF::nullopt;
    if (!decoder.decode(result.rp.icon))
        return WTF::nullopt;
    if (!decoder.decode(result.user.idVector))
        return WTF::nullopt;
    if (!decoder.decode(result.user.displayName))
        return WTF::nullopt;
    if (!decoder.decode(result.user.name))
        return WTF::nullopt;
    if (!decoder.decode(result.user.icon))
        return WTF::nullopt;
    if (!decoder.decode(result.pubKeyCredParams))
        return WTF::nullopt;

    Optional<Optional<unsigned>> timeout;
    decoder >> timeout;
    if (!timeout)
        return WTF::nullopt;
    result.timeout = WTFMove(*timeout);

    if (!decoder.decode(result.excludeCredentials))
        return WTF::nullopt;

    Optional<Optional<AuthenticatorSelectionCriteria>> authenticatorSelection;
    decoder >> authenticatorSelection;
    if (!authenticatorSelection)
        return WTF::nullopt;
    result.authenticatorSelection = WTFMove(*authenticatorSelection);

    Optional<AttestationConveyancePreference> attestation;
    decoder >> attestation;
    if (!attestation)
        return WTF::nullopt;
    result.attestation = WTFMove(*attestation);

    return result;
}

} // namespace WebCore

namespace WTF {
// Not every member is copied.
template<> struct CrossThreadCopierBase<false, false, WebCore::PublicKeyCredentialCreationOptions> {
    typedef WebCore::PublicKeyCredentialCreationOptions Type;
    static Type copy(const Type& source)
    {
        Type result;
        result.rp.name = source.rp.name.isolatedCopy();
        result.rp.icon = source.rp.icon.isolatedCopy();
        result.rp.id = source.rp.id.isolatedCopy();

        result.user.name = source.user.name.isolatedCopy();
        result.user.icon = source.user.icon.isolatedCopy();
        result.user.displayName = source.user.displayName.isolatedCopy();
        result.user.idVector = source.user.idVector;
        return result;
    }
};

template<> struct EnumTraits<WebCore::PublicKeyCredentialCreationOptions::AuthenticatorAttachment> {
    using values = EnumValues<
        WebCore::PublicKeyCredentialCreationOptions::AuthenticatorAttachment,
        WebCore::PublicKeyCredentialCreationOptions::AuthenticatorAttachment::Platform,
        WebCore::PublicKeyCredentialCreationOptions::AuthenticatorAttachment::CrossPlatform
    >;
};

} // namespace WTF

#endif // ENABLE(WEB_AUTHN)

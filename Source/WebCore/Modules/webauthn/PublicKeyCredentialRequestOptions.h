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
#include "AuthenticationExtensionsClientInputs.h"
#include "BufferSource.h"
#include "PublicKeyCredentialDescriptor.h"
#include "UserVerificationRequirement.h"
#include <wtf/Forward.h>
#endif // ENABLE(WEB_AUTHN)

namespace WebCore {

struct PublicKeyCredentialRequestOptions {
#if ENABLE(WEB_AUTHN)
    BufferSource challenge;
    std::optional<unsigned> timeout;
    mutable String rpId;
    Vector<PublicKeyCredentialDescriptor> allowCredentials;
    UserVerificationRequirement userVerification { UserVerificationRequirement::Preferred };
    std::optional<AuthenticatorAttachment> authenticatorAttachment;
    mutable std::optional<AuthenticationExtensionsClientInputs> extensions;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<PublicKeyCredentialRequestOptions> decode(Decoder&);
#endif // ENABLE(WEB_AUTHN)
};

#if ENABLE(WEB_AUTHN)
// Not every member is encoded.
template<class Encoder>
void PublicKeyCredentialRequestOptions::encode(Encoder& encoder) const
{
    encoder << timeout << rpId << allowCredentials << userVerification << extensions;
}

template<class Decoder>
std::optional<PublicKeyCredentialRequestOptions> PublicKeyCredentialRequestOptions::decode(Decoder& decoder)
{
    PublicKeyCredentialRequestOptions result;

    std::optional<std::optional<unsigned>> timeout;
    decoder >> timeout;
    if (!timeout)
        return std::nullopt;
    result.timeout = WTFMove(*timeout);

    if (!decoder.decode(result.rpId))
        return std::nullopt;
    if (!decoder.decode(result.allowCredentials))
        return std::nullopt;

    std::optional<UserVerificationRequirement> userVerification;
    decoder >> userVerification;
    if (!userVerification)
        return std::nullopt;
    result.userVerification = WTFMove(*userVerification);

    std::optional<std::optional<AuthenticationExtensionsClientInputs>> extensions;
    decoder >> extensions;
    if (!extensions)
        return std::nullopt;
    result.extensions = WTFMove(*extensions);

    return result;
}
#endif // ENABLE(WEB_AUTHN)

} // namespace WebCore

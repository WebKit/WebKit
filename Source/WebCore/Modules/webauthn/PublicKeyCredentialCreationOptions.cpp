/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "PublicKeyCredentialCreationOptions.h"

#include "AuthenticatorAttachment.h"

#if ENABLE(WEB_AUTHN)

namespace WebCore {

AttestationConveyancePreference PublicKeyCredentialCreationOptions::attestation() const
{
    if (attestationString == "indirect"_s)
        return AttestationConveyancePreference::Indirect;
    if (attestationString == "direct"_s)
        return AttestationConveyancePreference::Direct;
    return AttestationConveyancePreference::None;
}

std::optional<ResidentKeyRequirement> PublicKeyCredentialCreationOptions::AuthenticatorSelectionCriteria::residentKey() const
{
    if (residentKeyString == "required"_s)
        return ResidentKeyRequirement::Required;
    if (residentKeyString == "discouraged"_s)
        return ResidentKeyRequirement::Discouraged;
    return ResidentKeyRequirement::Preferred;
}

UserVerificationRequirement PublicKeyCredentialCreationOptions::AuthenticatorSelectionCriteria::userVerification() const
{
    if (userVerificationString == "required"_s)
        return UserVerificationRequirement::Required;
    if (userVerificationString == "preferred"_s)
        return UserVerificationRequirement::Preferred;
    return UserVerificationRequirement::Discouraged;
}

std::optional<AuthenticatorAttachment> PublicKeyCredentialCreationOptions::AuthenticatorSelectionCriteria::authenticatorAttachment() const
{
    if (authenticatorAttachmentString == "platform"_s)
        return AuthenticatorAttachment::Platform;
    if (authenticatorAttachmentString == "cross-platform"_s)
        return AuthenticatorAttachment::CrossPlatform;
    return std::nullopt;
}

}

#endif

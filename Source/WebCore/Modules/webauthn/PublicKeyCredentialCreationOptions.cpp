/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_AUTHN)

#include "BufferSource.h"
#include "JSAttestationConveyancePreference.h"
#include "JSPublicKeyCredentialCreationOptionsJSON.h"
#include "JSPublicKeyCredentialType.h"
#include "PublicKeyCredentialDescriptorJSON.h"
#include <wtf/text/Base64.h>

namespace WebCore {

AttestationConveyancePreference PublicKeyCredentialCreationOptions::attestation() const
{
    if (auto parsed = parseEnumerationFromString<AttestationConveyancePreference>(attestationString))
        return *parsed;
    // Default value if string is invalid/unknown
    return AttestationConveyancePreference::None;
}

PublicKeyCredentialCreationOptionsJSON PublicKeyCredentialCreationOptions::toJSON() const
{
    PublicKeyCredentialCreationOptionsJSON value;
    value.rp = this->rp;
    value.user = this->user.toJSON();
    value.challenge = base64EncodeToString(this->challenge.span());
    value.pubKeyCredParams = this->pubKeyCredParams;
    value.timeout = this->timeout;
    value.excludeCredentials = this->excludeCredentials.map([](auto& cred) {
        return cred.toJSON();
    });
    value.authenticatorSelection = this->authenticatorSelection;
    value.attestation = this->attestationString;
    if (this->extensions)
        value.extensions = this->extensions->toJSON();

    return value;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)

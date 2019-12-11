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
#include "AuthenticatorResponse.h"

#if ENABLE(WEB_AUTHN)

#include "AuthenticatorAssertionResponse.h"
#include "AuthenticatorAttestationResponse.h"
#include "AuthenticatorResponseData.h"

namespace WebCore {

RefPtr<AuthenticatorResponse> AuthenticatorResponse::tryCreate(AuthenticatorResponseData&& data)
{
    if (!data.rawId)
        return nullptr;

    if (data.isAuthenticatorAttestationResponse) {
        if (!data.attestationObject)
            return nullptr;

        return AuthenticatorAttestationResponse::create(data.rawId.releaseNonNull(), data.attestationObject.releaseNonNull());
    }

    if (!data.authenticatorData || !data.signature)
        return nullptr;

    return AuthenticatorAssertionResponse::create(data.rawId.releaseNonNull(), data.authenticatorData.releaseNonNull(), data.signature.releaseNonNull(), WTFMove(data.userHandle), AuthenticationExtensionsClientOutputs { data.appid });
}

AuthenticatorResponseData AuthenticatorResponse::data() const
{
    AuthenticatorResponseData data;
    data.rawId = m_rawId.copyRef();
    data.appid = m_extensions.appid;
    return data;
}

ArrayBuffer* AuthenticatorResponse::rawId() const
{
    return m_rawId.ptr();
}

void AuthenticatorResponse::setExtensions(AuthenticationExtensionsClientOutputs&& extensions)
{
    m_extensions = WTFMove(extensions);
}

AuthenticationExtensionsClientOutputs AuthenticatorResponse::extensions() const
{
    return m_extensions;
}

void AuthenticatorResponse::setClientDataJSON(Ref<ArrayBuffer>&& clientDataJSON)
{
    m_clientDataJSON = WTFMove(clientDataJSON);
}

ArrayBuffer* AuthenticatorResponse::clientDataJSON() const
{
    return m_clientDataJSON.get();
}

AuthenticatorResponse::AuthenticatorResponse(Ref<ArrayBuffer>&& rawId)
    : m_rawId(WTFMove(rawId))
{
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)

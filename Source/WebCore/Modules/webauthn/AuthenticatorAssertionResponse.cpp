/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#include "AuthenticatorAssertionResponse.h"

#if ENABLE(WEB_AUTHN)

#include "AuthenticatorResponseData.h"
#include <wtf/text/Base64.h>

namespace WebCore {

Ref<AuthenticatorAssertionResponse> AuthenticatorAssertionResponse::create(Ref<ArrayBuffer>&& rawId, Ref<ArrayBuffer>&& authenticatorData, Ref<ArrayBuffer>&& signature, RefPtr<ArrayBuffer>&& userHandle, std::optional<AuthenticationExtensionsClientOutputs>&& extensions, AuthenticatorAttachment attachment)
{
    auto response = adoptRef(*new AuthenticatorAssertionResponse(WTFMove(rawId), WTFMove(authenticatorData), WTFMove(signature), WTFMove(userHandle), attachment));
    if (extensions)
        response->setExtensions(WTFMove(*extensions));
    return response;
}

Ref<AuthenticatorAssertionResponse> AuthenticatorAssertionResponse::create(const Vector<uint8_t>& rawId, const Vector<uint8_t>& authenticatorData, const Vector<uint8_t>& signature, const Vector<uint8_t>& userHandle, AuthenticatorAttachment attachment)
{
    RefPtr<ArrayBuffer> userhandleBuffer;
    if (!userHandle.isEmpty())
        userhandleBuffer = ArrayBuffer::create(userHandle);
    return create(ArrayBuffer::create(rawId), ArrayBuffer::create(authenticatorData), ArrayBuffer::create(signature), WTFMove(userhandleBuffer), std::nullopt, attachment);
}

Ref<AuthenticatorAssertionResponse> AuthenticatorAssertionResponse::create(const Vector<uint8_t>& rawId, const Vector<uint8_t>& authenticatorData, const Vector<uint8_t>& signature, const Vector<uint8_t>& userHandle, std::optional<AuthenticationExtensionsClientOutputs>&& extensions, AuthenticatorAttachment attachment)
{
    RefPtr<ArrayBuffer> userhandleBuffer;
    if (!userHandle.isEmpty())
        userhandleBuffer = ArrayBuffer::create(userHandle);
    return create(ArrayBuffer::create(rawId), ArrayBuffer::create(authenticatorData), ArrayBuffer::create(signature), WTFMove(userhandleBuffer), WTFMove(extensions), attachment);
}

Ref<AuthenticatorAssertionResponse> AuthenticatorAssertionResponse::create(Ref<ArrayBuffer>&& rawId, RefPtr<ArrayBuffer>&& userHandle, String&& name, SecAccessControlRef accessControl, AuthenticatorAttachment attachment)
{
    return adoptRef(*new AuthenticatorAssertionResponse(WTFMove(rawId), WTFMove(userHandle), WTFMove(name), accessControl, attachment));
}

void AuthenticatorAssertionResponse::setAuthenticatorData(Vector<uint8_t>&& authenticatorData)
{
    m_authenticatorData = ArrayBuffer::create(authenticatorData);
}

AuthenticatorAssertionResponse::AuthenticatorAssertionResponse(Ref<ArrayBuffer>&& rawId, Ref<ArrayBuffer>&& authenticatorData, Ref<ArrayBuffer>&& signature, RefPtr<ArrayBuffer>&& userHandle, AuthenticatorAttachment attachment)
    : AuthenticatorResponse(WTFMove(rawId), attachment)
    , m_authenticatorData(WTFMove(authenticatorData))
    , m_signature(WTFMove(signature))
    , m_userHandle(WTFMove(userHandle))
{
}

AuthenticatorAssertionResponse::AuthenticatorAssertionResponse(Ref<ArrayBuffer>&& rawId, RefPtr<ArrayBuffer>&& userHandle, String&& name, SecAccessControlRef accessControl, AuthenticatorAttachment attachment)
    : AuthenticatorResponse(WTFMove(rawId), attachment)
    , m_userHandle(WTFMove(userHandle))
    , m_name(WTFMove(name))
    , m_accessControl(accessControl)
{
}

AuthenticatorResponseData AuthenticatorAssertionResponse::data() const
{
    auto data = AuthenticatorResponse::data();
    data.isAuthenticatorAttestationResponse = false;
    data.authenticatorData = m_authenticatorData.copyRef();
    data.signature = m_signature.copyRef();
    data.userHandle = m_userHandle;
    return data;
}

AuthenticationResponseJSON::AuthenticatorAssertionResponseJSON AuthenticatorAssertionResponse::toJSON()
{
    AuthenticationResponseJSON::AuthenticatorAssertionResponseJSON value;
    if (auto authData = authenticatorData())
        value.authenticatorData = base64URLEncodeToString(authData->span());
    if (auto sig = signature())
        value.signature = base64URLEncodeToString(sig->span());
    if (auto handle = userHandle())
        value.userHandle = base64URLEncodeToString(handle->span());
    if (auto clientData = clientDataJSON())
        value.clientDataJSON = base64URLEncodeToString(clientData->span());
    return value;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)

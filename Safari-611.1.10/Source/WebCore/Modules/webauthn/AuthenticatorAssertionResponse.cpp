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
#include "AuthenticatorAssertionResponse.h"

#if ENABLE(WEB_AUTHN)

#include "AuthenticatorResponseData.h"

namespace WebCore {

Ref<AuthenticatorAssertionResponse> AuthenticatorAssertionResponse::create(Ref<ArrayBuffer>&& rawId, Ref<ArrayBuffer>&& authenticatorData, Ref<ArrayBuffer>&& signature, RefPtr<ArrayBuffer>&& userHandle, Optional<AuthenticationExtensionsClientOutputs>&& extensions)
{
    auto response = adoptRef(*new AuthenticatorAssertionResponse(WTFMove(rawId), WTFMove(authenticatorData), WTFMove(signature), WTFMove(userHandle)));
    if (extensions)
        response->setExtensions(WTFMove(*extensions));
    return response;
}

Ref<AuthenticatorAssertionResponse> AuthenticatorAssertionResponse::create(const Vector<uint8_t>& rawId, const Vector<uint8_t>& authenticatorData, const Vector<uint8_t>& signature, const Vector<uint8_t>& userHandle)
{
    RefPtr<ArrayBuffer> userhandleBuffer;
    if (!userHandle.isEmpty())
        userhandleBuffer = ArrayBuffer::create(userHandle.data(), userHandle.size());
    return create(ArrayBuffer::create(rawId.data(), rawId.size()), ArrayBuffer::create(authenticatorData.data(), authenticatorData.size()), ArrayBuffer::create(signature.data(), signature.size()), WTFMove(userhandleBuffer), WTF::nullopt);
}

Ref<AuthenticatorAssertionResponse> AuthenticatorAssertionResponse::create(Ref<ArrayBuffer>&& rawId, Ref<ArrayBuffer>&& userHandle, String&& name, SecAccessControlRef accessControl)
{
    return adoptRef(*new AuthenticatorAssertionResponse(WTFMove(rawId), WTFMove(userHandle), WTFMove(name), accessControl));
}

void AuthenticatorAssertionResponse::setAuthenticatorData(Vector<uint8_t>&& authenticatorData)
{
    m_authenticatorData = ArrayBuffer::create(authenticatorData.data(), authenticatorData.size());
}

AuthenticatorAssertionResponse::AuthenticatorAssertionResponse(Ref<ArrayBuffer>&& rawId, Ref<ArrayBuffer>&& authenticatorData, Ref<ArrayBuffer>&& signature, RefPtr<ArrayBuffer>&& userHandle)
    : AuthenticatorResponse(WTFMove(rawId))
    , m_authenticatorData(WTFMove(authenticatorData))
    , m_signature(WTFMove(signature))
    , m_userHandle(WTFMove(userHandle))
{
}

AuthenticatorAssertionResponse::AuthenticatorAssertionResponse(Ref<ArrayBuffer>&& rawId, Ref<ArrayBuffer>&& userHandle, String&& name, SecAccessControlRef accessControl)
    : AuthenticatorResponse(WTFMove(rawId))
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

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)

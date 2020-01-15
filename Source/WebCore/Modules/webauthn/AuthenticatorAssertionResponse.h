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

#include "AuthenticatorResponse.h"

namespace WebCore {

class AuthenticatorAssertionResponse : public AuthenticatorResponse {
public:
    static Ref<AuthenticatorAssertionResponse> create(Ref<ArrayBuffer>&& rawId, Ref<ArrayBuffer>&& authenticatorData, Ref<ArrayBuffer>&& signature, RefPtr<ArrayBuffer>&& userHandle, Optional<AuthenticationExtensionsClientOutputs>&&);
    WEBCORE_EXPORT static Ref<AuthenticatorAssertionResponse> create(const Vector<uint8_t>& rawId, const Vector<uint8_t>& authenticatorData, const Vector<uint8_t>& signature,  const Vector<uint8_t>& userHandle);
    virtual ~AuthenticatorAssertionResponse() = default;

    ArrayBuffer* authenticatorData() const { return m_authenticatorData.ptr(); }
    ArrayBuffer* signature() const { return m_signature.ptr(); }
    ArrayBuffer* userHandle() const { return m_userHandle.get(); }

    void setName(const String& name) { m_name = name; }
    const String& name() const { return m_name; }
    void setDisplayName(const String& displayName) { m_displayName = displayName; }
    const String& displayName() const { return m_displayName; }
    void setNumberOfCredentials(size_t numberOfCredentials) { m_numberOfCredentials = numberOfCredentials; }
    size_t numberOfCredentials() const { return m_numberOfCredentials; }

private:
    AuthenticatorAssertionResponse(Ref<ArrayBuffer>&&, Ref<ArrayBuffer>&&, Ref<ArrayBuffer>&&, RefPtr<ArrayBuffer>&&);

    Type type() const final { return Type::Assertion; }
    AuthenticatorResponseData data() const final;

    Ref<ArrayBuffer> m_authenticatorData;
    Ref<ArrayBuffer> m_signature;
    RefPtr<ArrayBuffer> m_userHandle;

    String m_name;
    String m_displayName;
    size_t m_numberOfCredentials { 0 };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_AUTHENTICATOR_RESPONSE(AuthenticatorAssertionResponse, AuthenticatorResponse::Type::Assertion)

#endif // ENABLE(WEB_AUTHN)

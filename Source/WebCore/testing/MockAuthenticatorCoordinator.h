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

#include "AuthenticatorCoordinatorClient.h"
#include "BufferSource.h"
#include <wtf/Forward.h>

namespace WebCore {

class Internals;

class MockAuthenticatorCoordinator final : public AuthenticatorCoordinatorClient {
public:
    void setDidTimeOut() { m_didTimeOut = true; }
    void setDidUserCancel() { m_didUserCancel = true; }
    void setDidUserVerifyingPlatformAuthenticatorPresent() { m_didUserVerifyingPlatformAuthenticatorPresent = true; }
    void setCreationReturnBundle(const BufferSource& credentialId, const BufferSource& attestationObject);
    void setAssertionReturnBundle(const BufferSource& credentialId, const BufferSource& authenticatorData, const BufferSource& signature, const BufferSource& userHandle);

    // RefCounted is required for JS wrapper. Therefore, fake them to compile.
    void ref() const { }
    void deref() const { }

private:
    void makeCredential(const Vector<uint8_t>& hash, const PublicKeyCredentialCreationOptions&, RequestCompletionHandler&&) final;
    void getAssertion(const Vector<uint8_t>& hash, const PublicKeyCredentialRequestOptions&, RequestCompletionHandler&&) final;
    void isUserVerifyingPlatformAuthenticatorAvailable(QueryCompletionHandler&&) final;

    // All following fields are disposable.
    bool m_didTimeOut { false };
    bool m_didUserCancel { false };
    bool m_didUserVerifyingPlatformAuthenticatorPresent { false };
    RefPtr<ArrayBuffer> m_credentialId;
    RefPtr<ArrayBuffer> m_attestationObject;
    RefPtr<ArrayBuffer> m_authenticatorData;
    RefPtr<ArrayBuffer> m_signature;
    RefPtr<ArrayBuffer> m_userHandle;
};

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)

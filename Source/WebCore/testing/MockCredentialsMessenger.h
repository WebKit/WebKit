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

#include "BufferSource.h"
#include "CredentialsMessenger.h"
#include <wtf/Forward.h>

namespace WebCore {

class Internals;

class MockCredentialsMessenger final : public CredentialsMessenger {
public:
    explicit MockCredentialsMessenger(Internals&);
    ~MockCredentialsMessenger();

    void setDidTimeOut() { m_didTimeOut = true; }
    void setDidUserCancel() { m_didUserCancel = true; }
    void setDidUserVerifyingPlatformAuthenticatorPresent() { m_didUserVerifyingPlatformAuthenticatorPresent = true; }
    void setCreationReturnBundle(const BufferSource& credentialId, const BufferSource& attestationObject);
    void setAssertionReturnBundle(const BufferSource& credentialId, const BufferSource& authenticatorData, const BufferSource& signature, const BufferSource& userHandle);

    void ref();
    void deref();

private:
    void makeCredential(const Vector<uint8_t>&, const PublicKeyCredentialCreationOptions&, CreationCompletionHandler&&) final;
    void getAssertion(const Vector<uint8_t>& hash, const PublicKeyCredentialRequestOptions&, RequestCompletionHandler&&) final;
    void isUserVerifyingPlatformAuthenticatorAvailable(QueryCompletionHandler&&) final;
    void makeCredentialReply(uint64_t messageId, const Vector<uint8_t>& credentialId, const Vector<uint8_t>& attestationObject) final;
    void getAssertionReply(uint64_t messageId, const Vector<uint8_t>& credentialId, const Vector<uint8_t>& authenticatorData, const Vector<uint8_t>& signature, const Vector<uint8_t>& userHandle) final;
    void isUserVerifyingPlatformAuthenticatorAvailableReply(uint64_t messageId, bool) final;

    Internals& m_internals;
    // All following fields are disposable.
    bool m_didTimeOut { false };
    bool m_didUserCancel { false };
    bool m_didUserVerifyingPlatformAuthenticatorPresent { false };
    Vector<uint8_t> m_attestationObject;
    Vector<uint8_t> m_credentialId; // Overlapped between CreationReturnBundle and AssertionReturnBundle.
    Vector<uint8_t> m_authenticatorData;
    Vector<uint8_t> m_signature;
    Vector<uint8_t> m_userHandle;

    // To clean up completion handlers.
    Vector<uint64_t> m_timeOutMessageIds;
};

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)

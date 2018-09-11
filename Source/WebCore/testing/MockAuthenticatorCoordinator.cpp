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

#include "config.h"
#include "MockAuthenticatorCoordinator.h"

#if ENABLE(WEB_AUTHN)

#include "Internals.h"
#include <WebCore/PublicKeyCredentialData.h>
#include <wtf/Vector.h>

namespace WebCore {

void MockAuthenticatorCoordinator::setCreationReturnBundle(const BufferSource& credentialId, const BufferSource& attestationObject)
{
    ASSERT(!m_credentialId && !m_attestationObject);
    m_credentialId = ArrayBuffer::create(credentialId.data(), credentialId.length());
    m_attestationObject = ArrayBuffer::create(attestationObject.data(), attestationObject.length());
}

void MockAuthenticatorCoordinator::setAssertionReturnBundle(const BufferSource& credentialId, const BufferSource& authenticatorData, const BufferSource& signature, const BufferSource& userHandle)
{
    ASSERT(!m_credentialId && !m_authenticatorData && !m_signature && !m_userHandle);
    m_credentialId = ArrayBuffer::create(credentialId.data(), credentialId.length());
    m_authenticatorData = ArrayBuffer::create(authenticatorData.data(), authenticatorData.length());
    m_signature = ArrayBuffer::create(signature.data(), signature.length());
    m_userHandle = ArrayBuffer::create(userHandle.data(), userHandle.length());
}

void MockAuthenticatorCoordinator::makeCredential(const Vector<uint8_t>&, const PublicKeyCredentialCreationOptions&, RequestCompletionHandler&& handler)
{
    if (!setRequestCompletionHandler(WTFMove(handler)))
        return;

    if (m_didTimeOut) {
        m_didTimeOut = false;
        return;
    }
    if (m_didUserCancel) {
        m_didUserCancel = false;
        requestReply({ }, { NotAllowedError, "User cancelled."_s });
        return;
    }
    if (m_credentialId) {
        ASSERT(m_attestationObject);
        requestReply(PublicKeyCredentialData { WTFMove(m_credentialId), true, nullptr, WTFMove(m_attestationObject), nullptr, nullptr, nullptr }, { });
        m_credentialId = nullptr;
        m_attestationObject = nullptr;
        return;
    }
    ASSERT_NOT_REACHED();
}

void MockAuthenticatorCoordinator::getAssertion(const Vector<uint8_t>&, const PublicKeyCredentialRequestOptions&, RequestCompletionHandler&& handler)
{
    if (!setRequestCompletionHandler(WTFMove(handler)))
        return;

    if (m_didTimeOut) {
        m_didTimeOut = false;
        return;
    }
    if (m_didUserCancel) {
        m_didUserCancel = false;
        requestReply({ }, { NotAllowedError, "User cancelled."_s });
        return;
    }
    if (m_credentialId) {
        ASSERT(m_authenticatorData && m_signature && m_userHandle);
        requestReply(PublicKeyCredentialData { WTFMove(m_credentialId), false, nullptr, nullptr, WTFMove(m_authenticatorData), WTFMove(m_signature), WTFMove(m_userHandle) }, { });
        m_credentialId = nullptr;
        m_authenticatorData = nullptr;
        m_signature = nullptr;
        m_userHandle = nullptr;
        return;
    }
    ASSERT_NOT_REACHED();
}

void MockAuthenticatorCoordinator::isUserVerifyingPlatformAuthenticatorAvailable(QueryCompletionHandler&& handler)
{
    auto messageId = addQueryCompletionHandler(WTFMove(handler));
    if (m_didUserVerifyingPlatformAuthenticatorPresent) {
        isUserVerifyingPlatformAuthenticatorAvailableReply(messageId, true);
        m_didUserVerifyingPlatformAuthenticatorPresent = false;
    } else
        isUserVerifyingPlatformAuthenticatorAvailableReply(messageId, false);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)

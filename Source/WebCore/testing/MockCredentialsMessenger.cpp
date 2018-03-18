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
#include "MockCredentialsMessenger.h"

#if ENABLE(WEB_AUTHN)

#include "Internals.h"
#include <wtf/Vector.h>

namespace WebCore {

MockCredentialsMessenger::MockCredentialsMessenger(Internals& internals)
    : m_internals(internals)
{
}

MockCredentialsMessenger::~MockCredentialsMessenger()
{
    // Have no effects to original promises. Just to call handlers to avoid any assertion failures.
    for (auto messageId : m_timeOutMessageIds)
        exceptionReply(messageId, ExceptionData { NotAllowedError, ASCIILiteral("Operation timed out.") });
}

void MockCredentialsMessenger::setCreationReturnBundle(const BufferSource& credentialId, const BufferSource& attestationObject)
{
    ASSERT(m_credentialId.isEmpty() && m_attestationObject.isEmpty());
    m_credentialId.append(credentialId.data(), credentialId.length());
    m_attestationObject.append(attestationObject.data(), attestationObject.length());
}

void MockCredentialsMessenger::setAssertionReturnBundle(const BufferSource& credentialId, const BufferSource& authenticatorData, const BufferSource& signature, const BufferSource& userHandle)
{
    ASSERT(m_credentialId.isEmpty() && m_authenticatorData.isEmpty() && m_signature.isEmpty() && m_userHandle.isEmpty());
    m_credentialId.append(credentialId.data(), credentialId.length());
    m_authenticatorData.append(authenticatorData.data(), authenticatorData.length());
    m_signature.append(signature.data(), signature.length());
    m_userHandle.append(userHandle.data(), userHandle.length());
}

void MockCredentialsMessenger::ref()
{
    m_internals.ref();
}

void MockCredentialsMessenger::deref()
{
    m_internals.deref();
}

void MockCredentialsMessenger::makeCredential(const Vector<uint8_t>&, const PublicKeyCredentialCreationOptions&, CreationCompletionHandler&& handler)
{
    auto messageId = addCreationCompletionHandler(WTFMove(handler));
    if (m_didTimeOut) {
        m_didTimeOut = false;
        m_timeOutMessageIds.append(messageId);
        return;
    }
    if (m_didUserCancel) {
        m_didUserCancel = false;
        exceptionReply(messageId, ExceptionData { NotAllowedError, ASCIILiteral("User cancelled.") });
        return;
    }
    if (!m_credentialId.isEmpty()) {
        ASSERT(!m_attestationObject.isEmpty());
        makeCredentialReply(messageId, m_credentialId, m_attestationObject);
        m_credentialId.clear();
        m_attestationObject.clear();
        return;
    }
    ASSERT_NOT_REACHED();
}

void MockCredentialsMessenger::getAssertion(const Vector<uint8_t>&, const PublicKeyCredentialRequestOptions&, RequestCompletionHandler&& handler)
{
    auto messageId = addRequestCompletionHandler(WTFMove(handler));
    if (m_didTimeOut) {
        m_didTimeOut = false;
        m_timeOutMessageIds.append(messageId);
        return;
    }
    if (m_didUserCancel) {
        m_didUserCancel = false;
        exceptionReply(messageId, ExceptionData { NotAllowedError, ASCIILiteral("User cancelled.") });
        return;
    }
    if (!m_credentialId.isEmpty()) {
        ASSERT(!m_authenticatorData.isEmpty() && !m_signature.isEmpty() && !m_userHandle.isEmpty());
        getAssertionReply(messageId, m_credentialId, m_authenticatorData, m_signature, m_userHandle);
        m_credentialId.clear();
        m_authenticatorData.clear();
        m_signature.clear();
        m_userHandle.clear();
        return;
    }
    ASSERT_NOT_REACHED();
}

void MockCredentialsMessenger::isUserVerifyingPlatformAuthenticatorAvailable(QueryCompletionHandler&& handler)
{
    auto messageId = addQueryCompletionHandler(WTFMove(handler));
    if (m_didUserVerifyingPlatformAuthenticatorPresent) {
        isUserVerifyingPlatformAuthenticatorAvailableReply(messageId, true);
        m_didUserVerifyingPlatformAuthenticatorPresent = false;
    } else
        isUserVerifyingPlatformAuthenticatorAvailableReply(messageId, false);
}

void MockCredentialsMessenger::makeCredentialReply(uint64_t messageId, const Vector<uint8_t>& credentialId, const Vector<uint8_t>& attestationObject)
{
    auto handler = takeCreationCompletionHandler(messageId);
    handler(CreationReturnBundle(ArrayBuffer::create(credentialId.data(), credentialId.size()), ArrayBuffer::create(attestationObject.data(), attestationObject.size())));
}

void MockCredentialsMessenger::getAssertionReply(uint64_t messageId, const Vector<uint8_t>& credentialId, const Vector<uint8_t>& authenticatorData, const Vector<uint8_t>& signature, const Vector<uint8_t>& userHandle)
{
    auto handler = takeRequestCompletionHandler(messageId);
    handler(AssertionReturnBundle(ArrayBuffer::create(credentialId.data(), credentialId.size()), ArrayBuffer::create(authenticatorData.data(), authenticatorData.size()), ArrayBuffer::create(signature.data(), signature.size()), ArrayBuffer::create(userHandle.data(), userHandle.size())));
}

void MockCredentialsMessenger::isUserVerifyingPlatformAuthenticatorAvailableReply(uint64_t messageId, bool result)
{
    auto handler = takeQueryCompletionHandler(messageId);
    handler(result);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)

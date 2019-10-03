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
#include "AuthenticatorManager.h"

#if ENABLE(WEB_AUTHN)

#include <WebCore/AuthenticatorTransport.h>
#include <WebCore/PublicKeyCredentialCreationOptions.h>
#include <wtf/MonotonicTime.h>

namespace WebKit {
using namespace WebCore;

namespace AuthenticatorManagerInternal {

const size_t maxTransportNumber = 3;

// Suggested by WebAuthN spec as of 7 August 2018.
const unsigned maxTimeOutValue = 120000;

// FIXME(188625): Support BLE authenticators.
static AuthenticatorManager::TransportSet collectTransports(const Optional<PublicKeyCredentialCreationOptions::AuthenticatorSelectionCriteria>& authenticatorSelection)
{
    AuthenticatorManager::TransportSet result;
    if (!authenticatorSelection || !authenticatorSelection->authenticatorAttachment) {
        auto addResult = result.add(AuthenticatorTransport::Internal);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
        addResult = result.add(AuthenticatorTransport::Usb);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
        addResult = result.add(AuthenticatorTransport::Nfc);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
        return result;
    }

    if (authenticatorSelection->authenticatorAttachment == PublicKeyCredentialCreationOptions::AuthenticatorAttachment::Platform) {
        auto addResult = result.add(AuthenticatorTransport::Internal);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
        return result;
    }
    if (authenticatorSelection->authenticatorAttachment == PublicKeyCredentialCreationOptions::AuthenticatorAttachment::CrossPlatform) {
        auto addResult = result.add(AuthenticatorTransport::Usb);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
        addResult = result.add(AuthenticatorTransport::Nfc);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
        return result;
    }

    ASSERT_NOT_REACHED();
    return result;
}

// FIXME(188624, 188625): Support NFC and BLE authenticators.
// The goal is to find a union of different transports from allowCredentials.
// If it is not specified or any of its credentials doesn't specify its own. We should discover all.
// This is a variant of Step. 18.*.4 from https://www.w3.org/TR/webauthn/#discover-from-external-source
// as of 7 August 2018.
static AuthenticatorManager::TransportSet collectTransports(const Vector<PublicKeyCredentialDescriptor>& allowCredentials)
{
    AuthenticatorManager::TransportSet result;
    if (allowCredentials.isEmpty()) {
        auto addResult = result.add(AuthenticatorTransport::Internal);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
        addResult = result.add(AuthenticatorTransport::Usb);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
        addResult = result.add(AuthenticatorTransport::Nfc);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
        return result;
    }

    for (auto& allowCredential : allowCredentials) {
        if (allowCredential.transports.isEmpty()) {
            result.add(AuthenticatorTransport::Internal);
            result.add(AuthenticatorTransport::Usb);
            result.add(AuthenticatorTransport::Nfc);
            return result;
        }
        if (!result.contains(AuthenticatorTransport::Internal) && allowCredential.transports.contains(AuthenticatorTransport::Internal))
            result.add(AuthenticatorTransport::Internal);
        if (!result.contains(AuthenticatorTransport::Usb) && allowCredential.transports.contains(AuthenticatorTransport::Usb))
            result.add(AuthenticatorTransport::Usb);
        if (!result.contains(AuthenticatorTransport::Nfc) && allowCredential.transports.contains(AuthenticatorTransport::Nfc))
            result.add(AuthenticatorTransport::Nfc);
        if (result.size() >= maxTransportNumber)
            return result;
    }

    ASSERT(result.size() < maxTransportNumber);
    return result;
}

} // namespace AuthenticatorManagerInternal

AuthenticatorManager::AuthenticatorManager()
    : m_requestTimeOutTimer(RunLoop::main(), this, &AuthenticatorManager::timeOutTimerFired)
{
}

void AuthenticatorManager::makeCredential(const Vector<uint8_t>& hash, const PublicKeyCredentialCreationOptions& options, Callback&& callback)
{
    using namespace AuthenticatorManagerInternal;

    if (m_pendingCompletionHandler) {
        m_pendingCompletionHandler(ExceptionData { NotAllowedError, "This request has been cancelled by a new request."_s });
        m_requestTimeOutTimer.stop();
    }
    clearState();

    // 1. Save request for async operations.
    m_pendingRequestData = { hash, true, options, { } };
    m_pendingCompletionHandler = WTFMove(callback);
    initTimeOutTimer(options.timeout);

    // 2. Get available transports and start discovering authenticators on them.
    startDiscovery(collectTransports(options.authenticatorSelection));
}

void AuthenticatorManager::getAssertion(const Vector<uint8_t>& hash, const PublicKeyCredentialRequestOptions& options, Callback&& callback)
{
    using namespace AuthenticatorManagerInternal;

    if (m_pendingCompletionHandler) {
        m_pendingCompletionHandler(ExceptionData { NotAllowedError, "This request has been cancelled by a new request."_s });
        m_requestTimeOutTimer.stop();
    }
    clearState();

    // 1. Save request for async operations.
    m_pendingRequestData = { hash, false, { }, options };
    m_pendingCompletionHandler = WTFMove(callback);
    initTimeOutTimer(options.timeout);

    // 2. Get available transports and start discovering authenticators on them.
    ASSERT(m_services.isEmpty());
    startDiscovery(collectTransports(options.allowCredentials));
}

void AuthenticatorManager::clearStateAsync()
{
    RunLoop::main().dispatch([weakThis = makeWeakPtr(*this)] {
        if (!weakThis)
            return;
        weakThis->clearState();
    });
}

void AuthenticatorManager::clearState()
{
    if (m_pendingCompletionHandler)
        return;
    m_pendingRequestData = { };
    m_services.clear();
    m_authenticators.clear();
}

void AuthenticatorManager::authenticatorAdded(Ref<Authenticator>&& authenticator)
{
    ASSERT(RunLoop::isMain());
    authenticator->setObserver(*this);
    authenticator->handleRequest(m_pendingRequestData);
    auto addResult = m_authenticators.add(WTFMove(authenticator));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

void AuthenticatorManager::respondReceived(Respond&& respond)
{
    ASSERT(RunLoop::isMain());
    if (!m_requestTimeOutTimer.isActive())
        return;
    ASSERT(m_pendingCompletionHandler);

    auto shouldComplete = WTF::holds_alternative<PublicKeyCredentialData>(respond);
    if (!shouldComplete)
        shouldComplete = WTF::get<ExceptionData>(respond).code == InvalidStateError;
    if (shouldComplete) {
        m_pendingCompletionHandler(WTFMove(respond));
        clearStateAsync();
        m_requestTimeOutTimer.stop();
        return;
    }
    respondReceivedInternal(WTFMove(respond));
}

void AuthenticatorManager::downgrade(Authenticator* id, Ref<Authenticator>&& downgradedAuthenticator)
{
    RunLoop::main().dispatch([weakThis = makeWeakPtr(*this), id] {
        if (!weakThis)
            return;
        auto removed = weakThis->m_authenticators.remove(id);
        ASSERT_UNUSED(removed, removed);
    });
    authenticatorAdded(WTFMove(downgradedAuthenticator));
}

UniqueRef<AuthenticatorTransportService> AuthenticatorManager::createService(WebCore::AuthenticatorTransport transport, AuthenticatorTransportService::Observer& observer) const
{
    return AuthenticatorTransportService::create(transport, observer);
}

void AuthenticatorManager::respondReceivedInternal(Respond&&)
{
}

void AuthenticatorManager::startDiscovery(const TransportSet& transports)
{
    using namespace AuthenticatorManagerInternal;

    ASSERT(m_services.isEmpty() && transports.size() <= maxTransportNumber);
    for (auto& transport : transports) {
        auto service = createService(transport, *this);
        service->startDiscovery();
        m_services.append(WTFMove(service));
    }
}

void AuthenticatorManager::initTimeOutTimer(const Optional<unsigned>& timeOutInMs)
{
    using namespace AuthenticatorManagerInternal;

    unsigned timeOutInMsValue = std::min(maxTimeOutValue, timeOutInMs.valueOr(maxTimeOutValue));
    m_requestTimeOutTimer.startOneShot(Seconds::fromMilliseconds(timeOutInMsValue));
}

void AuthenticatorManager::timeOutTimerFired()
{
    ASSERT(m_requestTimeOutTimer.isActive());
    m_pendingCompletionHandler((ExceptionData { NotAllowedError, "Operation timed out."_s }));
    clearState();
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)

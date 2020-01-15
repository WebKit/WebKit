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

#include "APIUIClient.h"
#include "APIWebAuthenticationPanel.h"
#include "APIWebAuthenticationPanelClient.h"
#include "LocalService.h"
#include "NfcService.h"
#include "WebPageProxy.h"
#include "WebPreferencesKeys.h"
#include <WebCore/AuthenticatorTransport.h>
#include <WebCore/PublicKeyCredentialCreationOptions.h>
#include <WebCore/WebAuthenticationConstants.h>
#include <wtf/MonotonicTime.h>

namespace WebKit {
using namespace WebCore;

namespace {

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

// FIXME(188625): Support BLE authenticators.
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

        for (const auto& transport : allowCredential.transports) {
            if (transport == AuthenticatorTransport::Ble)
                continue;
            result.add(transport);
            if (result.size() >= AuthenticatorManager::maxTransportNumber)
                return result;
        }
    }

    ASSERT(result.size() < AuthenticatorManager::maxTransportNumber);
    return result;
}

// Only roaming authenticators are supported for Google legacy AppID support.
static void processGoogleLegacyAppIdSupportExtension(const Optional<AuthenticationExtensionsClientInputs>& extensions, AuthenticatorManager::TransportSet& transports)
{
    // AuthenticatorCoordinator::create should always set it.
    ASSERT(!!extensions);
    if (!extensions->googleLegacyAppidSupport)
        return;
    transports.remove(AuthenticatorTransport::Internal);
}

static bool isFeatureEnabled(WebPageProxy* page, const String& featureKey)
{
    if (!page)
        return false;
    return page->preferences().store().getBoolValueForKey(featureKey);
}

static String getRpId(const Variant<PublicKeyCredentialCreationOptions, PublicKeyCredentialRequestOptions>& options)
{
    if (WTF::holds_alternative<PublicKeyCredentialCreationOptions>(options))
        return WTF::get<PublicKeyCredentialCreationOptions>(options).rp.id;
    return WTF::get<PublicKeyCredentialRequestOptions>(options).rpId;
}

static ClientDataType getClientDataType(const Variant<PublicKeyCredentialCreationOptions, PublicKeyCredentialRequestOptions>& options)
{
    if (WTF::holds_alternative<PublicKeyCredentialCreationOptions>(options))
        return ClientDataType::Create;
    return ClientDataType::Get;
}

} // namespace

const size_t AuthenticatorManager::maxTransportNumber = 3;

AuthenticatorManager::AuthenticatorManager()
    : m_requestTimeOutTimer(RunLoop::main(), this, &AuthenticatorManager::timeOutTimerFired)
{
}

void AuthenticatorManager::handleRequest(WebAuthenticationRequestData&& data, Callback&& callback)
{
    if (m_pendingCompletionHandler) {
        invokePendingCompletionHandler(ExceptionData { NotAllowedError, "This request has been cancelled by a new request."_s });
        m_requestTimeOutTimer.stop();
    }
    clearState();

    // 1. Save request for async operations.
    m_pendingRequestData = WTFMove(data);
    m_pendingCompletionHandler = WTFMove(callback);

    // 2. Ask clients to show appropriate UI if any and then start the request.
    initTimeOutTimer();
    runPanel();
}

void AuthenticatorManager::cancelRequest(const PageIdentifier& pageID, const Optional<FrameIdentifier>& frameID)
{
    if (!m_pendingCompletionHandler)
        return;
    if (auto pendingFrameID = m_pendingRequestData.frameID) {
        if (pendingFrameID->pageID != pageID)
            return;
        if (frameID && frameID != pendingFrameID->frameID)
            return;
    }
    invokePendingCompletionHandler(ExceptionData { NotAllowedError, "Operation timed out."_s });
    clearState();
    m_requestTimeOutTimer.stop();
}

// The following implements part of Step 20. of https://www.w3.org/TR/webauthn/#createCredential
// and part of Step 18. of https://www.w3.org/TR/webauthn/#getAssertion as of 4 March 2019:
// "If the user exercises a user agent user-interface option to cancel the process,".
void AuthenticatorManager::cancelRequest(const API::WebAuthenticationPanel& panel)
{
    RELEASE_ASSERT(RunLoop::isMain());
    if (!m_pendingCompletionHandler || m_pendingRequestData.panel.get() != &panel)
        return;
    invokePendingCompletionHandler(ExceptionData { NotAllowedError, "This request has been cancelled by the user."_s });
    clearState();
    m_requestTimeOutTimer.stop();
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
    m_authenticators.clear();
    m_services.clear();
    m_pendingRequestData = { };
}

void AuthenticatorManager::authenticatorAdded(Ref<Authenticator>&& authenticator)
{
    ASSERT(RunLoop::isMain());
    authenticator->setObserver(*this);
    authenticator->handleRequest(m_pendingRequestData);
    auto addResult = m_authenticators.add(WTFMove(authenticator));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

void AuthenticatorManager::serviceStatusUpdated(WebAuthenticationStatus status)
{
    if (auto *panel = m_pendingRequestData.panel.get())
        panel->client().updatePanel(status);
}

void AuthenticatorManager::respondReceived(Respond&& respond)
{
    ASSERT(RunLoop::isMain());
    if (!m_requestTimeOutTimer.isActive())
        return;
    ASSERT(m_pendingCompletionHandler);

    auto shouldComplete = WTF::holds_alternative<Ref<AuthenticatorResponse>>(respond);
    if (!shouldComplete)
        shouldComplete = WTF::get<ExceptionData>(respond).code == InvalidStateError;
    if (shouldComplete) {
        invokePendingCompletionHandler(WTFMove(respond));
        clearStateAsync();
        m_requestTimeOutTimer.stop();
        return;
    }
    respondReceivedInternal(WTFMove(respond));
    restartDiscovery();
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

void AuthenticatorManager::authenticatorStatusUpdated(WebAuthenticationStatus status)
{
    if (auto* panel = m_pendingRequestData.panel.get())
        panel->client().updatePanel(status);
}

void AuthenticatorManager::requestPin(uint64_t retries, CompletionHandler<void(const WTF::String&)>&& completionHandler)
{
    if (auto* panel = m_pendingRequestData.panel.get())
        panel->client().requestPin(retries, WTFMove(completionHandler));
}

void AuthenticatorManager::selectAssertionResponse(const HashSet<Ref<WebCore::AuthenticatorAssertionResponse>>& responses, CompletionHandler<void(const WebCore::AuthenticatorAssertionResponse&)>&& completionHandler)
{
    if (auto* panel = m_pendingRequestData.panel.get())
        panel->client().selectAssertionResponse(responses, WTFMove(completionHandler));
}

UniqueRef<AuthenticatorTransportService> AuthenticatorManager::createService(AuthenticatorTransport transport, AuthenticatorTransportService::Observer& observer) const
{
    return AuthenticatorTransportService::create(transport, observer);
}

void AuthenticatorManager::filterTransports(TransportSet& transports) const
{
    if (!NfcService::isAvailable())
        transports.remove(AuthenticatorTransport::Nfc);
    if (!LocalService::isAvailable())
        transports.remove(AuthenticatorTransport::Internal);
}

void AuthenticatorManager::startDiscovery(const TransportSet& transports)
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_services.isEmpty() && transports.size() <= maxTransportNumber);
    for (auto& transport : transports) {
        // Only allow USB authenticators when clients don't have dedicated UI.
        if (transport != AuthenticatorTransport::Usb && (m_pendingRequestData.panelResult == WebAuthenticationPanelResult::Unavailable))
            continue;
        auto service = createService(transport, *this);
        service->startDiscovery();
        m_services.append(WTFMove(service));
    }
}

void AuthenticatorManager::initTimeOutTimer()
{
    Optional<unsigned> timeOutInMs;
    WTF::switchOn(m_pendingRequestData.options, [&](const PublicKeyCredentialCreationOptions& options) {
        timeOutInMs = options.timeout;
    }, [&](const PublicKeyCredentialRequestOptions& options) {
        timeOutInMs = options.timeout;
    });

    unsigned timeOutInMsValue = std::min(maxTimeOutValue, timeOutInMs.valueOr(maxTimeOutValue));
    m_requestTimeOutTimer.startOneShot(Seconds::fromMilliseconds(timeOutInMsValue));
}

void AuthenticatorManager::timeOutTimerFired()
{
    ASSERT(m_requestTimeOutTimer.isActive());
    invokePendingCompletionHandler((ExceptionData { NotAllowedError, "Operation timed out."_s }));
    clearState();
}

void AuthenticatorManager::runPanel()
{
    auto* page = m_pendingRequestData.page.get();
    if (!page)
        return;
    ASSERT(m_pendingRequestData.frameID && page->webPageID() == m_pendingRequestData.frameID->pageID);
    auto* frame = page->process().webFrame(m_pendingRequestData.frameID->frameID);
    if (!frame)
        return;

    // Get available transports and start discovering authenticators on them.
    auto& options = m_pendingRequestData.options;
    auto transports = getTransports();
    m_pendingRequestData.panel = API::WebAuthenticationPanel::create(*this, getRpId(options), transports, getClientDataType(options));
    auto& panel = *m_pendingRequestData.panel;
    page->uiClient().runWebAuthenticationPanel(*page, panel, *frame, WebCore::SecurityOriginData { m_pendingRequestData.origin }, [transports = WTFMove(transports), weakPanel = makeWeakPtr(panel), weakThis = makeWeakPtr(*this), this] (WebAuthenticationPanelResult result) {
        // The panel address is used to determine if the current pending request is still the same.
        if (!weakThis || !weakPanel
            || (result == WebAuthenticationPanelResult::DidNotPresent)
            || (weakPanel.get() != m_pendingRequestData.panel.get()))
            return;
        m_pendingRequestData.panelResult = result;
        startDiscovery(transports);
    });
}

void AuthenticatorManager::invokePendingCompletionHandler(Respond&& respond)
{
    if (auto *panel = m_pendingRequestData.panel.get()) {
        WTF::switchOn(respond, [&](const Ref<AuthenticatorResponse>&) {
            panel->client().dismissPanel(WebAuthenticationResult::Succeeded);
        }, [&](const ExceptionData&) {
            panel->client().dismissPanel(WebAuthenticationResult::Failed);
        });
    }
    m_pendingCompletionHandler(WTFMove(respond));
}

void AuthenticatorManager::restartDiscovery()
{
    for (auto& service : m_services)
        service->restartDiscovery();
}

auto AuthenticatorManager::getTransports() const -> TransportSet
{
    TransportSet transports;
    WTF::switchOn(m_pendingRequestData.options, [&](const PublicKeyCredentialCreationOptions& options) {
        transports = collectTransports(options.authenticatorSelection);
        processGoogleLegacyAppIdSupportExtension(options.extensions, transports);
    }, [&](const PublicKeyCredentialRequestOptions& options) {
        transports = collectTransports(options.allowCredentials);
    });
    if (!isFeatureEnabled(m_pendingRequestData.page.get(), WebPreferencesKey::webAuthenticationLocalAuthenticatorEnabledKey()))
        transports.remove(AuthenticatorTransport::Internal);
    filterTransports(transports);
    return transports;
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)

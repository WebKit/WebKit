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
#include "AuthenticatorPresenterCoordinator.h"
#include "LocalService.h"
#include "NfcService.h"
#include "WebPageProxy.h"
#include "WebPreferencesKeys.h"
#include "WebProcessProxy.h"
#include <WebCore/AuthenticatorAssertionResponse.h>
#include <WebCore/AuthenticatorAttachment.h>
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
static AuthenticatorManager::TransportSet collectTransports(const std::optional<PublicKeyCredentialCreationOptions::AuthenticatorSelectionCriteria>& authenticatorSelection)
{
    AuthenticatorManager::TransportSet result;
    if (!authenticatorSelection || !authenticatorSelection->authenticatorAttachment) {
        auto addResult = result.add(AuthenticatorTransport::Internal);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
        addResult = result.add(AuthenticatorTransport::Usb);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
        addResult = result.add(AuthenticatorTransport::Nfc);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
        addResult = result.add(AuthenticatorTransport::Ble);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
        addResult = result.add(AuthenticatorTransport::SmartCard);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
        return result;
    }

    if (authenticatorSelection->authenticatorAttachment == AuthenticatorAttachment::Platform) {
        auto addResult = result.add(AuthenticatorTransport::Internal);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
        return result;
    }
    if (authenticatorSelection->authenticatorAttachment == AuthenticatorAttachment::CrossPlatform) {
        auto addResult = result.add(AuthenticatorTransport::Usb);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
        addResult = result.add(AuthenticatorTransport::Nfc);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
        addResult = result.add(AuthenticatorTransport::Ble);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
        addResult = result.add(AuthenticatorTransport::SmartCard);
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
static AuthenticatorManager::TransportSet collectTransports(const Vector<PublicKeyCredentialDescriptor>& allowCredentials, const std::optional<AuthenticatorAttachment>& authenticatorAttachment)
{
    AuthenticatorManager::TransportSet result;
    if (allowCredentials.isEmpty()) {
        auto addResult = result.add(AuthenticatorTransport::Internal);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
        addResult = result.add(AuthenticatorTransport::Usb);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
        addResult = result.add(AuthenticatorTransport::Nfc);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
        addResult = result.add(AuthenticatorTransport::Ble);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
        addResult = result.add(AuthenticatorTransport::SmartCard);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }

    for (auto& allowCredential : allowCredentials) {
        if (allowCredential.transports.isEmpty()) {
            result.add(AuthenticatorTransport::Internal);
            result.add(AuthenticatorTransport::Usb);
            result.add(AuthenticatorTransport::Nfc);
            result.add(AuthenticatorTransport::Ble);
            result.add(AuthenticatorTransport::SmartCard);

            break;
        }

        for (const auto& transport : allowCredential.transports) {
            if (transport == AuthenticatorTransport::Ble)
                continue;

            result.add(transport);

            if (result.size() >= AuthenticatorManager::maxTransportNumber)
                break;
        }
    }

    if (authenticatorAttachment) {
        if (authenticatorAttachment == AuthenticatorAttachment::Platform) {
            result.remove(AuthenticatorTransport::Usb);
            result.remove(AuthenticatorTransport::Nfc);
            result.remove(AuthenticatorTransport::Ble);
            result.remove(AuthenticatorTransport::SmartCard);
        }

        if (authenticatorAttachment == AuthenticatorAttachment::CrossPlatform)
            result.remove(AuthenticatorTransport::Internal);
    }

    ASSERT(result.size() <= AuthenticatorManager::maxTransportNumber);
    return result;
}

// Only roaming authenticators are supported for Google legacy AppID support.
static void processGoogleLegacyAppIdSupportExtension(const std::optional<AuthenticationExtensionsClientInputs>& extensions, AuthenticatorManager::TransportSet& transports)
{
    if (!extensions) {
        // AuthenticatorCoordinator::create should always set it.
        ASSERT_NOT_REACHED();
        return;
    }
    if (!extensions->googleLegacyAppidSupport)
        return;
    transports.remove(AuthenticatorTransport::Internal);
}

static String getRpId(const std::variant<PublicKeyCredentialCreationOptions, PublicKeyCredentialRequestOptions>& options)
{
    if (std::holds_alternative<PublicKeyCredentialCreationOptions>(options)) {
        auto& creationOptions = std::get<PublicKeyCredentialCreationOptions>(options);
        ASSERT(creationOptions.rp.id);
        return *creationOptions.rp.id;
    }
    return std::get<PublicKeyCredentialRequestOptions>(options).rpId;
}

static String getUserName(const std::variant<PublicKeyCredentialCreationOptions, PublicKeyCredentialRequestOptions>& options)
{
    if (std::holds_alternative<PublicKeyCredentialCreationOptions>(options))
        return std::get<PublicKeyCredentialCreationOptions>(options).user.name;
    return emptyString();
}

} // namespace

const size_t AuthenticatorManager::maxTransportNumber = 5;

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

    // FIXME<rdar://problem/70822834>: The m_mode is used to determine whether or not we are in the UIProcess.
    // If so, continue to the old route. Otherwise, use the modern WebAuthn process way.
    if (m_mode == Mode::Compatible) {
        runPanel();
        return;
    }
    runPresenter();
}

void AuthenticatorManager::cancelRequest(const PageIdentifier& pageID, const std::optional<FrameIdentifier>& frameID)
{
    if (!m_pendingCompletionHandler)
        return;
    if (auto pendingFrameID = m_pendingRequestData.globalFrameID) {
        if (pendingFrameID->pageID != pageID)
            return;
        if (frameID && frameID != pendingFrameID->frameID)
            return;
    }
    cancelRequest();
}

// The following implements part of Step 20. of https://www.w3.org/TR/webauthn/#createCredential
// and part of Step 18. of https://www.w3.org/TR/webauthn/#getAssertion as of 4 March 2019:
// "If the user exercises a user agent user-interface option to cancel the process,".
void AuthenticatorManager::cancelRequest(const API::WebAuthenticationPanel& panel)
{
    RELEASE_ASSERT(RunLoop::isMain());
    if (!m_pendingCompletionHandler || m_pendingRequestData.panel.get() != &panel)
        return;
    cancelRequest();
}

void AuthenticatorManager::cancel()
{
    RELEASE_ASSERT(RunLoop::isMain());
    if (!m_pendingCompletionHandler)
        return;
    cancelRequest();
}

void AuthenticatorManager::enableNativeSupport()
{
    m_mode = Mode::Native;
}

void AuthenticatorManager::clearStateAsync()
{
    RunLoop::main().dispatch([weakThis = WeakPtr { *this }] {
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
    m_presenter = nullptr;
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
    // This is for the new UI.
    if (m_presenter) {
        m_presenter->updatePresenter(status);
        return;
    }

    dispatchPanelClientCall([status] (const API::WebAuthenticationPanel& panel) {
        panel.client().updatePanel(status);
    });
}

void AuthenticatorManager::respondReceived(Respond&& respond)
{
    ASSERT(RunLoop::isMain());
    if (!m_requestTimeOutTimer.isActive() && (m_pendingRequestData.mediation != WebCore::CredentialRequestOptions::MediationRequirement::Conditional || !m_pendingCompletionHandler))
        return;
    ASSERT(m_pendingCompletionHandler);

    auto shouldComplete = std::holds_alternative<Ref<AuthenticatorResponse>>(respond);
    if (!shouldComplete)
        shouldComplete = std::get<ExceptionData>(respond).code == InvalidStateError;
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
    RunLoop::main().dispatch([weakThis = WeakPtr { *this }, id] {
        if (!weakThis)
            return;
        auto removed = weakThis->m_authenticators.remove(id);
        ASSERT_UNUSED(removed, removed);
    });
    authenticatorAdded(WTFMove(downgradedAuthenticator));
}

void AuthenticatorManager::authenticatorStatusUpdated(WebAuthenticationStatus status)
{
    // Immediately invalidate the cache if the PIN is incorrect. A status update often means
    // an error. We don't really care what kind of error it really is.
    m_pendingRequestData.cachedPin = String();

    // This is for the new UI.
    if (m_presenter) {
        m_presenter->updatePresenter(status);
        return;
    }

    dispatchPanelClientCall([status] (const API::WebAuthenticationPanel& panel) {
        panel.client().updatePanel(status);
    });
}

void AuthenticatorManager::requestPin(uint64_t retries, CompletionHandler<void(const WTF::String&)>&& completionHandler)
{
    // Cache the PIN to improve NFC user experience so that a momentary movement of the NFC key away from the scanner doesn't
    // force the PIN entry to be re-entered.
    // We don't distinguish USB and NFC here becuase there is no harms to have this optimization for USB even though it is useless.
    if (!m_pendingRequestData.cachedPin.isNull()) {
        completionHandler(m_pendingRequestData.cachedPin);
        m_pendingRequestData.cachedPin = String();
        return;
    }

    auto callback = [weakThis = WeakPtr { *this }, this, completionHandler = WTFMove(completionHandler)] (const WTF::String& pin) mutable {
        if (!weakThis)
            return;

        m_pendingRequestData.cachedPin = pin;
        completionHandler(pin);
    };

    // This is for the new UI.
    if (m_presenter) {
        m_presenter->requestPin(retries, WTFMove(callback));
        return;
    }

    dispatchPanelClientCall([retries, callback = WTFMove(callback)] (const API::WebAuthenticationPanel& panel) mutable {
        panel.client().requestPin(retries, WTFMove(callback));
    });
}

void AuthenticatorManager::selectAssertionResponse(Vector<Ref<WebCore::AuthenticatorAssertionResponse>>&& responses, WebAuthenticationSource source, CompletionHandler<void(AuthenticatorAssertionResponse*)>&& completionHandler)
{
    // This is for the new UI.
    if (m_presenter) {
        m_presenter->selectAssertionResponse(WTFMove(responses), source, WTFMove(completionHandler));
        return;
    }

    dispatchPanelClientCall([responses = WTFMove(responses), source, completionHandler = WTFMove(completionHandler)] (const API::WebAuthenticationPanel& panel) mutable {
        panel.client().selectAssertionResponse(WTFMove(responses), source, WTFMove(completionHandler));
    });
}

void AuthenticatorManager::decidePolicyForLocalAuthenticator(CompletionHandler<void(LocalAuthenticatorPolicy)>&& completionHandler)
{
    dispatchPanelClientCall([completionHandler = WTFMove(completionHandler)] (const API::WebAuthenticationPanel& panel) mutable {
        panel.client().decidePolicyForLocalAuthenticator(WTFMove(completionHandler));
    });
}

void AuthenticatorManager::requestLAContextForUserVerification(CompletionHandler<void(LAContext *)>&& completionHandler)
{
    if (m_presenter) {
        m_presenter->requestLAContextForUserVerification(WTFMove(completionHandler));
        return;
    }

    dispatchPanelClientCall([completionHandler = WTFMove(completionHandler)] (const API::WebAuthenticationPanel& panel) mutable {
        panel.client().requestLAContextForUserVerification(WTFMove(completionHandler));
    });
}

void AuthenticatorManager::cancelRequest()
{
    invokePendingCompletionHandler(ExceptionData { NotAllowedError, "This request has been cancelled by the user."_s });
    RELEASE_LOG_ERROR(WebAuthn, "Request cancelled due to AuthenticatorManager::cancelRequest being called.");
    clearState();
    m_requestTimeOutTimer.stop();
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
    transports.remove(AuthenticatorTransport::Ble);

    // For the modern UI, we should only consider invoking it when the operation is triggered by users.
    if (!m_pendingRequestData.processingUserGesture)
        transports.clear();
}

void AuthenticatorManager::startDiscovery(const TransportSet& transports)
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_services.isEmpty() && transports.size() <= maxTransportNumber);
    m_services = WTF::map(transports, [this](auto& transport) {
        auto service = createService(transport, *this);
        service->startDiscovery();
        return service;
    });
}

void AuthenticatorManager::initTimeOutTimer()
{
    if (m_pendingRequestData.mediation == WebCore::CredentialRequestOptions::MediationRequirement::Conditional)
        return;
    std::optional<unsigned> timeOutInMs;
    WTF::switchOn(m_pendingRequestData.options, [&](const PublicKeyCredentialCreationOptions& options) {
        timeOutInMs = options.timeout;
    }, [&](const PublicKeyCredentialRequestOptions& options) {
        timeOutInMs = options.timeout;
    });

    unsigned timeOutInMsValue = std::min(maxTimeOutValue, timeOutInMs.value_or(maxTimeOutValue));
    m_requestTimeOutTimer.startOneShot(Seconds::fromMilliseconds(timeOutInMsValue));
}

void AuthenticatorManager::timeOutTimerFired()
{
    invokePendingCompletionHandler((ExceptionData { NotAllowedError, "Operation timed out."_s }));
    clearState();
}

void AuthenticatorManager::runPanel()
{
    auto* page = m_pendingRequestData.page.get();
    if (!page)
        return;
    ASSERT(m_pendingRequestData.globalFrameID && page->webPageID() == m_pendingRequestData.globalFrameID->pageID);
    auto* frame = WebFrameProxy::webFrame(m_pendingRequestData.globalFrameID->frameID);
    if (!frame)
        return;

    // Get available transports and start discovering authenticators on them.
    auto& options = m_pendingRequestData.options;
    auto transports = getTransports();
    if (transports.isEmpty()) {
        cancel();
        return;
    }

    m_pendingRequestData.panel = API::WebAuthenticationPanel::create(*this, getRpId(options), transports, getClientDataType(options), getUserName(options));
    auto& panel = *m_pendingRequestData.panel;
    page->uiClient().runWebAuthenticationPanel(*page, panel, *frame, FrameInfoData { m_pendingRequestData.frameInfo }, [transports = WTFMove(transports), weakPanel = WeakPtr { panel }, weakThis = WeakPtr { *this }, this] (WebAuthenticationPanelResult result) {
        // The panel address is used to determine if the current pending request is still the same.
        if (!weakThis || !weakPanel
            || (result == WebAuthenticationPanelResult::DidNotPresent)
            || (weakPanel.get() != m_pendingRequestData.panel.get()))
            return;
        startDiscovery(transports);
    });
}

void AuthenticatorManager::runPresenter()
{
    // Get available transports and start discovering authenticators on them.
    auto transports = getTransports();
    if (transports.isEmpty()) {
        cancel();
        return;
    }

    startDiscovery(transports);

    // For native API support, we skip the UI part. The native API will handle that.
    if (m_mode == Mode::Native)
        return;

    runPresenterInternal(transports);
}

void AuthenticatorManager::runPresenterInternal(const TransportSet& transports)
{
    auto& options = m_pendingRequestData.options;
    m_presenter = makeUnique<AuthenticatorPresenterCoordinator>(*this, getRpId(options), transports, getClientDataType(options), getUserName(options));
}

void AuthenticatorManager::invokePendingCompletionHandler(Respond&& respond)
{
    auto result = std::holds_alternative<Ref<AuthenticatorResponse>>(respond) ? WebAuthenticationResult::Succeeded : WebAuthenticationResult::Failed;

    // This is for the new UI.
    if (m_presenter)
        m_presenter->dimissPresenter(result);
    else {
        dispatchPanelClientCall([result] (const API::WebAuthenticationPanel& panel) {
            panel.client().dismissPanel(result);
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
        transports = collectTransports(options.allowCredentials, options.authenticatorAttachment);
    });
    filterTransports(transports);
    return transports;
}

void AuthenticatorManager::dispatchPanelClientCall(Function<void(const API::WebAuthenticationPanel&)>&& call) const
{
    auto weakPanel = m_pendingRequestData.weakPanel;
    if (!weakPanel)
        weakPanel = m_pendingRequestData.panel;
    if (!weakPanel)
        return;

    // Call delegates in the next run loop to prevent clients' reentrance that would potentially modify the state
    // of the current run loop in unexpected ways.
    RunLoop::main().dispatch([weakPanel = WTFMove(weakPanel), call = WTFMove(call)] () {
        if (!weakPanel)
            return;
        call(*weakPanel);
    });
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)

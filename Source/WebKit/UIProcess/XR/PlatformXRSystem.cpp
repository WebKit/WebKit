/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "PlatformXRSystem.h"

#if ENABLE(WEBXR)

#include "GPUProcessProxy.h"
#include "MessageSenderInlines.h"
#include "PlatformXRSystemMessages.h"
#include "PlatformXRSystemProxyMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/SecurityOriginData.h>
#include <wtf/TZoneMallocInlines.h>

#define MESSAGE_CHECK(assertion, connection) MESSAGE_CHECK_BASE(assertion, connection)
#define MESSAGE_CHECK_COMPLETION(assertion, connection, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, connection, completion)

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PlatformXRSystem);

PlatformXRSystem::PlatformXRSystem(WebPageProxy& page)
    : m_page(page)
{
    page.protectedLegacyMainFrameProcess()->addMessageReceiver(Messages::PlatformXRSystem::messageReceiverName(), page.webPageIDInMainFrameProcess(), *this);
}

PlatformXRSystem::~PlatformXRSystem()
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    page->protectedLegacyMainFrameProcess()->removeMessageReceiver(Messages::PlatformXRSystem::messageReceiverName(), page->webPageIDInMainFrameProcess());
}

std::optional<SharedPreferencesForWebProcess> PlatformXRSystem::sharedPreferencesForWebProcess() const
{
    if (!m_page)
        return std::nullopt;
    return m_page->legacyMainFrameProcess().sharedPreferencesForWebProcess();
}

void PlatformXRSystem::invalidate()
{
    ASSERT(RunLoop::isMain());

    RefPtr page = m_page.get();
    if (!page)
        return;

    if (m_immersiveSessionState == ImmersiveSessionState::Idle)
        return;

    if (xrCoordinator())
        xrCoordinator()->endSessionIfExists(*page);

    invalidateImmersiveSessionState();
}

void PlatformXRSystem::ensureImmersiveSessionActivity()
{
    ASSERT(RunLoop::isMain());

    RefPtr page = m_page.get();
    if (!page)
        return;

    if (m_immersiveSessionActivity && m_immersiveSessionActivity->isValid())
        return;

    m_immersiveSessionActivity = page->protectedLegacyMainFrameProcess()->throttler().foregroundActivity("XR immersive session"_s);
}

void PlatformXRSystem::enumerateImmersiveXRDevices(CompletionHandler<void(Vector<XRDeviceInfo>&&)>&& completionHandler)
{
    RefPtr page = m_page.get();
    if (!page) {
        completionHandler({ });
        return;
    }

    auto* xrCoordinator = PlatformXRSystem::xrCoordinator();
    if (!xrCoordinator) {
        completionHandler({ });
        return;
    }

    xrCoordinator->getPrimaryDeviceInfo(*page, [completionHandler = WTFMove(completionHandler)](std::optional<XRDeviceInfo> deviceInfo) mutable {
        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), deviceInfo = WTFMove(deviceInfo)]() mutable {
            if (!deviceInfo) {
                completionHandler({ });
                return;
            }

            completionHandler({ deviceInfo.value() });
        });
    });
}

static bool checkFeaturesConsent(const std::optional<PlatformXR::Device::FeatureList>& requestedFeatures, const std::optional<PlatformXR::Device::FeatureList>& grantedFeatures)
{
    if (!grantedFeatures || !requestedFeatures)
        return false;

    bool result = true;
    for (auto requestedFeature : *requestedFeatures) {
        if (!grantedFeatures->contains(requestedFeature)) {
            result = false;
            break;
        }
    }
    return result;
}

void PlatformXRSystem::requestPermissionOnSessionFeatures(IPC::Connection& connection, const WebCore::SecurityOriginData& securityOriginData, PlatformXR::SessionMode mode, const PlatformXR::Device::FeatureList& granted, const PlatformXR::Device::FeatureList& consentRequired, const PlatformXR::Device::FeatureList& consentOptional, const PlatformXR::Device::FeatureList& requiredFeaturesRequested, const PlatformXR::Device::FeatureList& optionalFeaturesRequested, CompletionHandler<void(std::optional<PlatformXR::Device::FeatureList>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    RefPtr page = m_page.get();
    if (!page) {
        completionHandler(granted);
        return;
    }

    auto* xrCoordinator = PlatformXRSystem::xrCoordinator();
    if (!xrCoordinator) {
        completionHandler(granted);
        return;
    }

    if (PlatformXR::isImmersive(mode)) {
        MESSAGE_CHECK_COMPLETION(m_immersiveSessionState == ImmersiveSessionState::Idle, connection, completionHandler({ }));
        setImmersiveSessionState(ImmersiveSessionState::RequestingPermissions, [](bool) mutable { });
        m_immersiveSessionGrantedFeatures = std::nullopt;
    }

    xrCoordinator->requestPermissionOnSessionFeatures(*page, securityOriginData, mode, granted, consentRequired, consentOptional, requiredFeaturesRequested, optionalFeaturesRequested, [weakThis = WeakPtr { *this }, mode, securityOriginData, consentRequired, completionHandler = WTFMove(completionHandler)](std::optional<PlatformXR::Device::FeatureList>&& grantedFeatures) mutable {
        ASSERT(RunLoop::isMain());
        auto protectedThis = weakThis.get();
        if (protectedThis && PlatformXR::isImmersive(mode)) {
            if (checkFeaturesConsent(consentRequired, grantedFeatures)) {
                protectedThis->m_immersiveSessionMode = mode;
                protectedThis->m_immersiveSessionGrantedFeatures = grantedFeatures;
                protectedThis->m_immersiveSessionSecurityOriginData = securityOriginData;
                protectedThis->setImmersiveSessionState(ImmersiveSessionState::PermissionsGranted, [grantedFeatures = WTFMove(grantedFeatures), completionHandler = WTFMove(completionHandler)](bool) mutable {
                    completionHandler(WTFMove(grantedFeatures));
                });
            } else {
                protectedThis->invalidateImmersiveSessionState();
                completionHandler(WTFMove(grantedFeatures));
            }
        } else
            completionHandler(WTFMove(grantedFeatures));
    });
}

void PlatformXRSystem::initializeTrackingAndRendering(IPC::Connection& connection)
{
    ASSERT(RunLoop::isMain());
    MESSAGE_CHECK(m_immersiveSessionMode, connection);
    MESSAGE_CHECK(m_immersiveSessionState == ImmersiveSessionState::PermissionsGranted, connection);
    MESSAGE_CHECK(m_immersiveSessionSecurityOriginData, connection);
    MESSAGE_CHECK(m_immersiveSessionGrantedFeatures && !m_immersiveSessionGrantedFeatures->isEmpty(), connection);

    RefPtr page = m_page.get();
    if (!page)
        return;

    auto* xrCoordinator = PlatformXRSystem::xrCoordinator();
    if (!xrCoordinator)
        return;

    setImmersiveSessionState(ImmersiveSessionState::SessionRunning, [](bool) mutable { });

    ensureImmersiveSessionActivity();

    WeakPtr weakThis { *this };
    xrCoordinator->startSession(*page, weakThis, *m_immersiveSessionSecurityOriginData, *m_immersiveSessionMode, *m_immersiveSessionGrantedFeatures);
}

void PlatformXRSystem::shutDownTrackingAndRendering(IPC::Connection& connection)
{
    ASSERT(RunLoop::isMain());
    MESSAGE_CHECK(m_immersiveSessionState == ImmersiveSessionState::SessionRunning, connection);

    RefPtr page = m_page.get();
    if (!page)
        return;

    if (auto* xrCoordinator = PlatformXRSystem::xrCoordinator())
        xrCoordinator->endSessionIfExists(*page);
    setImmersiveSessionState(ImmersiveSessionState::SessionEndingFromWebContent, [](bool) mutable { });
}

void PlatformXRSystem::requestFrame(IPC::Connection& connection, std::optional<PlatformXR::RequestData>&& requestData, CompletionHandler<void(PlatformXR::FrameData&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    MESSAGE_CHECK_COMPLETION(m_immersiveSessionState == ImmersiveSessionState::SessionRunning || m_immersiveSessionState == ImmersiveSessionState::SessionEndingFromSystem, connection, completionHandler({ }));
    if (m_immersiveSessionState != ImmersiveSessionState::SessionRunning) {
        completionHandler({ });
        return;
    }

    RefPtr page = m_page.get();
    if (!page) {
        completionHandler({ });
        return;
    }

    if (auto* xrCoordinator = PlatformXRSystem::xrCoordinator())
        xrCoordinator->scheduleAnimationFrame(*page, WTFMove(requestData), WTFMove(completionHandler));
    else
        completionHandler({ });
}

void PlatformXRSystem::submitFrame(IPC::Connection& connection)
{
    ASSERT(RunLoop::isMain());
    MESSAGE_CHECK(m_immersiveSessionState == ImmersiveSessionState::SessionRunning || m_immersiveSessionState == ImmersiveSessionState::SessionEndingFromSystem, connection);
    if (m_immersiveSessionState != ImmersiveSessionState::SessionRunning)
        return;

    RefPtr page = m_page.get();
    if (!page)
        return;

    if (auto* xrCoordinator = PlatformXRSystem::xrCoordinator())
        xrCoordinator->submitFrame(*page);
}

void PlatformXRSystem::didCompleteShutdownTriggeredBySystem(IPC::Connection& connection)
{
    ASSERT(RunLoop::isMain());
    MESSAGE_CHECK(m_immersiveSessionState == ImmersiveSessionState::SessionEndingFromSystem, connection);
    setImmersiveSessionState(ImmersiveSessionState::Idle, [](bool) mutable { });
}

void PlatformXRSystem::sessionDidEnd(XRDeviceIdentifier deviceIdentifier)
{
    ensureOnMainRunLoop([weakThis = WeakPtr { *this }, deviceIdentifier]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RefPtr page = protectedThis->m_page.get();
        if (!page)
            return;

        page->protectedLegacyMainFrameProcess()->send(Messages::PlatformXRSystemProxy::SessionDidEnd(deviceIdentifier), page->webPageIDInMainFrameProcess());
        protectedThis->m_immersiveSessionActivity = nullptr;
        // If this is called when the session is running, the ending of the session is triggered by the system side
        // and we should set the state to SessionEndingFromSystem. We expect the web process to send a
        // didCompleteShutdownTriggeredBySystem message later when it has ended the XRSession, which will
        // reset the session state to Idle.
        protectedThis->invalidateImmersiveSessionState(protectedThis->m_immersiveSessionState == ImmersiveSessionState::SessionRunning ? ImmersiveSessionState::SessionEndingFromSystem : ImmersiveSessionState::Idle);
    });
}

void PlatformXRSystem::sessionDidUpdateVisibilityState(XRDeviceIdentifier deviceIdentifier, PlatformXR::VisibilityState visibilityState)
{
    ensureOnMainRunLoop([weakThis = WeakPtr { *this }, deviceIdentifier, visibilityState]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RefPtr page = protectedThis->m_page.get();
        if (!page)
            return;

        page->protectedLegacyMainFrameProcess()->send(Messages::PlatformXRSystemProxy::SessionDidUpdateVisibilityState(deviceIdentifier, visibilityState), page->webPageIDInMainFrameProcess());
    });
}

void PlatformXRSystem::setImmersiveSessionState(ImmersiveSessionState state, CompletionHandler<void(bool)>&& completion)
{
    m_immersiveSessionState = state;
#if PLATFORM(COCOA)
    RefPtr page = m_page.get();
    if (!page) {
        completion(false);
        return;
    }

    switch (state) {
    case ImmersiveSessionState::Idle:
    case ImmersiveSessionState::RequestingPermissions:
        break;
    case ImmersiveSessionState::PermissionsGranted:
        return GPUProcessProxy::getOrCreate()->webXRPromptAccepted(page->ensureRunningProcess().processIdentity(), WTFMove(completion));
    case ImmersiveSessionState::SessionRunning:
    case ImmersiveSessionState::SessionEndingFromWebContent:
    case ImmersiveSessionState::SessionEndingFromSystem:
        break;
    }

    completion(true);
#else
    completion(true);
#endif
}

void PlatformXRSystem::invalidateImmersiveSessionState(ImmersiveSessionState nextSessionState)
{
    ASSERT(RunLoop::isMain());

    m_immersiveSessionMode = std::nullopt;
    m_immersiveSessionSecurityOriginData = std::nullopt;
    m_immersiveSessionGrantedFeatures = std::nullopt;
    setImmersiveSessionState(nextSessionState, [](bool) mutable { });
}

bool PlatformXRSystem::webXREnabled() const
{
    RefPtr page = m_page.get();
    return page && page->protectedPreferences()->webXREnabled();
}

#if !USE(APPLE_INTERNAL_SDK)

PlatformXRCoordinator* PlatformXRSystem::xrCoordinator()
{
    return nullptr;
}

#endif // !USE(APPLE_INTERNAL_SDK)

} // namespace WebKit

#undef MESSAGE_CHECK_COMPLETION
#undef MESSAGE_CHECK

#endif // ENABLE(WEBXR)

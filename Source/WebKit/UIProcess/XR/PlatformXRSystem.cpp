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

#include "MessageSenderInlines.h"
#include "PlatformXRSystemMessages.h"
#include "PlatformXRSystemProxyMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/SecurityOriginData.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, m_page.process().connection())

namespace WebKit {

PlatformXRSystem::PlatformXRSystem(WebPageProxy& page)
    : m_page(page)
{
    m_page.process().addMessageReceiver(Messages::PlatformXRSystem::messageReceiverName(), m_page.webPageID(), *this);
}

PlatformXRSystem::~PlatformXRSystem()
{
    m_page.process().removeMessageReceiver(Messages::PlatformXRSystem::messageReceiverName(), m_page.webPageID());
}

void PlatformXRSystem::invalidate()
{
    ASSERT(RunLoop::isMain());

    if (m_immersiveSessionState == ImmersiveSessionState::Idle)
        return;

    if (xrCoordinator())
        xrCoordinator()->endSessionIfExists(m_page);

    invalidateImmersiveSessionState();
}

void PlatformXRSystem::ensureImmersiveSessionActivity()
{
    ASSERT(RunLoop::isMain());

    if (m_immersiveSessionActivity && m_immersiveSessionActivity->isValid())
        return;

    m_immersiveSessionActivity = m_page.process().throttler().foregroundActivity("XR immersive session"_s).moveToUniquePtr();
}

void PlatformXRSystem::enumerateImmersiveXRDevices(CompletionHandler<void(Vector<XRDeviceInfo>&&)>&& completionHandler)
{
    auto* xrCoordinator = PlatformXRSystem::xrCoordinator();
    if (!xrCoordinator) {
        completionHandler({ });
        return;
    }

    xrCoordinator->getPrimaryDeviceInfo([completionHandler = WTFMove(completionHandler)](std::optional<XRDeviceInfo> deviceInfo) mutable {
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

void PlatformXRSystem::requestPermissionOnSessionFeatures(const WebCore::SecurityOriginData& securityOriginData, PlatformXR::SessionMode mode, const PlatformXR::Device::FeatureList& granted, const PlatformXR::Device::FeatureList& consentRequired, const PlatformXR::Device::FeatureList& consentOptional, const PlatformXR::Device::FeatureList& requiredFeaturesRequested, const PlatformXR::Device::FeatureList& optionalFeaturesRequested, CompletionHandler<void(std::optional<PlatformXR::Device::FeatureList>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    auto* xrCoordinator = PlatformXRSystem::xrCoordinator();
    if (!xrCoordinator) {
        completionHandler(granted);
        return;
    }

    if (PlatformXR::isImmersive(mode)) {
        MESSAGE_CHECK(m_immersiveSessionState == ImmersiveSessionState::Idle);
        setImmersiveSessionState(ImmersiveSessionState::RequestingPermissions);
        m_immersiveSessionGrantedFeatures = std::nullopt;
    }

    xrCoordinator->requestPermissionOnSessionFeatures(m_page, securityOriginData, mode, granted, consentRequired, consentOptional, requiredFeaturesRequested, optionalFeaturesRequested, [weakThis = WeakPtr { *this }, mode, securityOriginData, consentRequired, completionHandler = WTFMove(completionHandler)](std::optional<PlatformXR::Device::FeatureList>&& grantedFeatures) mutable {
        ASSERT(RunLoop::isMain());
        auto protectedThis = weakThis.get();
        if (protectedThis && PlatformXR::isImmersive(mode)) {
            if (checkFeaturesConsent(consentRequired, grantedFeatures)) {
                protectedThis->m_immersiveSessionMode = mode;
                protectedThis->m_immersiveSessionGrantedFeatures = grantedFeatures;
                protectedThis->m_immersiveSessionSecurityOriginData = securityOriginData;
                protectedThis->setImmersiveSessionState(ImmersiveSessionState::PermissionsGranted);
            } else
                protectedThis->invalidateImmersiveSessionState();
        }
        completionHandler(WTFMove(grantedFeatures));
    });
}

void PlatformXRSystem::initializeTrackingAndRendering()
{
    ASSERT(RunLoop::isMain());
    MESSAGE_CHECK(m_immersiveSessionMode);
    MESSAGE_CHECK(m_immersiveSessionState == ImmersiveSessionState::PermissionsGranted);
    MESSAGE_CHECK(m_immersiveSessionSecurityOriginData);
    MESSAGE_CHECK(m_immersiveSessionGrantedFeatures && !m_immersiveSessionGrantedFeatures->isEmpty());

    auto* xrCoordinator = PlatformXRSystem::xrCoordinator();
    if (!xrCoordinator)
        return;

    setImmersiveSessionState(ImmersiveSessionState::SessionRunning);

    ensureImmersiveSessionActivity();

    WeakPtr weakThis { *this };
    xrCoordinator->startSession(m_page, weakThis, *m_immersiveSessionSecurityOriginData, *m_immersiveSessionMode, *m_immersiveSessionGrantedFeatures);
}

void PlatformXRSystem::shutDownTrackingAndRendering()
{
    ASSERT(RunLoop::isMain());
    MESSAGE_CHECK(m_immersiveSessionState == ImmersiveSessionState::SessionRunning);

    if (auto* xrCoordinator = PlatformXRSystem::xrCoordinator())
        xrCoordinator->endSessionIfExists(m_page);
}

void PlatformXRSystem::requestFrame(CompletionHandler<void(PlatformXR::FrameData&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    MESSAGE_CHECK(m_immersiveSessionState == ImmersiveSessionState::SessionRunning);

    if (auto* xrCoordinator = PlatformXRSystem::xrCoordinator())
        xrCoordinator->scheduleAnimationFrame(m_page, WTFMove(completionHandler));
}

void PlatformXRSystem::submitFrame()
{
    ASSERT(RunLoop::isMain());
    MESSAGE_CHECK(m_immersiveSessionState == ImmersiveSessionState::SessionRunning);

    if (auto* xrCoordinator = PlatformXRSystem::xrCoordinator())
        xrCoordinator->submitFrame(m_page);
}

void PlatformXRSystem::sessionDidEnd(XRDeviceIdentifier deviceIdentifier)
{
    ensureOnMainRunLoop([weakThis = WeakPtr { *this }, deviceIdentifier]() mutable {
        auto protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        protectedThis->m_page.send(Messages::PlatformXRSystemProxy::SessionDidEnd(deviceIdentifier));
        protectedThis->m_immersiveSessionActivity = nullptr;
        protectedThis->invalidateImmersiveSessionState();
    });
}

void PlatformXRSystem::sessionDidUpdateVisibilityState(XRDeviceIdentifier deviceIdentifier, PlatformXR::VisibilityState visibilityState)
{
    ensureOnMainRunLoop([weakThis = WeakPtr { *this }, deviceIdentifier, visibilityState]() mutable {
        auto protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        protectedThis->m_page.send(Messages::PlatformXRSystemProxy::SessionDidUpdateVisibilityState(deviceIdentifier, visibilityState));
    });
}

void PlatformXRSystem::setImmersiveSessionState(ImmersiveSessionState state)
{
    m_immersiveSessionState = state;
}

void PlatformXRSystem::invalidateImmersiveSessionState()
{
    ASSERT(RunLoop::isMain());

    m_immersiveSessionMode = std::nullopt;
    m_immersiveSessionSecurityOriginData = std::nullopt;
    m_immersiveSessionGrantedFeatures = std::nullopt;
    setImmersiveSessionState(ImmersiveSessionState::Idle);
}

bool PlatformXRSystem::webXREnabled() const
{
    return m_page.preferences().webXREnabled();
}

}

#if !USE(APPLE_INTERNAL_SDK)
namespace WebKit {

PlatformXRCoordinator* PlatformXRSystem::xrCoordinator()
{
    return nullptr;
}

}
#endif

#endif // ENABLE(WEBXR)

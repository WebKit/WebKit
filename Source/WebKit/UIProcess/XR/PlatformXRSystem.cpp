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
    if (xrCoordinator())
        xrCoordinator()->endSessionIfExists(m_page);
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

void PlatformXRSystem::requestPermissionOnSessionFeatures(const WebCore::SecurityOriginData& securityOriginData, PlatformXR::SessionMode mode, const PlatformXR::Device::FeatureList& granted, const PlatformXR::Device::FeatureList& consentRequired, const PlatformXR::Device::FeatureList& consentOptional, const PlatformXR::Device::FeatureList& requiredFeaturesRequested, const PlatformXR::Device::FeatureList& optionalFeaturesRequested, CompletionHandler<void(std::optional<PlatformXR::Device::FeatureList>&&)>&& completionHandler)
{
    auto* xrCoordinator = PlatformXRSystem::xrCoordinator();
    if (!xrCoordinator) {
        completionHandler(granted);
        return;
    }

    xrCoordinator->requestPermissionOnSessionFeatures(m_page, securityOriginData, mode, granted, consentRequired, consentOptional, requiredFeaturesRequested, optionalFeaturesRequested, WTFMove(completionHandler));
}

void PlatformXRSystem::initializeTrackingAndRendering(const WebCore::SecurityOriginData& securityOriginData, PlatformXR::SessionMode mode, const PlatformXR::Device::FeatureList& requestedFeatures)
{
    auto* xrCoordinator = PlatformXRSystem::xrCoordinator();
    if (!xrCoordinator)
        return;

    m_immersiveSessionActivity = m_page.process().throttler().foregroundActivity("XR immersive session"_s).moveToUniquePtr();

    WeakPtr weakThis { *this };
    xrCoordinator->startSession(m_page, weakThis, securityOriginData, mode, requestedFeatures);
}

void PlatformXRSystem::shutDownTrackingAndRendering()
{
    if (auto* xrCoordinator = PlatformXRSystem::xrCoordinator())
        xrCoordinator->endSessionIfExists(m_page);
}

void PlatformXRSystem::requestFrame(CompletionHandler<void(PlatformXR::Device::FrameData&&)>&& completionHandler)
{
    if (auto* xrCoordinator = PlatformXRSystem::xrCoordinator())
        xrCoordinator->scheduleAnimationFrame(m_page, WTFMove(completionHandler));
}

void PlatformXRSystem::submitFrame()
{
    if (auto* xrCoordinator = PlatformXRSystem::xrCoordinator())
        xrCoordinator->submitFrame(m_page);
}

void PlatformXRSystem::sessionDidEnd(XRDeviceIdentifier deviceIdentifier)
{
    ensureOnMainRunLoop([weakThis = WeakPtr { *this }, deviceIdentifier]() mutable {
        auto strongThis = weakThis.get();
        if (!strongThis)
            return;

        strongThis->m_page.send(Messages::PlatformXRSystemProxy::SessionDidEnd(deviceIdentifier));
        strongThis->m_immersiveSessionActivity = nullptr;
    });
}

void PlatformXRSystem::sessionDidUpdateVisibilityState(XRDeviceIdentifier deviceIdentifier, PlatformXR::VisibilityState visibilityState)
{
    ensureOnMainRunLoop([weakThis = WeakPtr { *this }, deviceIdentifier, visibilityState]() mutable {
        auto strongThis = weakThis.get();
        if (!strongThis)
            return;

        strongThis->m_page.send(Messages::PlatformXRSystemProxy::SessionDidUpdateVisibilityState(deviceIdentifier, visibilityState));
    });
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

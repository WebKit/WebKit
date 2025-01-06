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
#include "PlatformXRSystemProxy.h"

#if ENABLE(WEBXR)

#include "MessageSenderInlines.h"
#include "PlatformXRCoordinator.h"
#include "PlatformXRSystemMessages.h"
#include "PlatformXRSystemProxyMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include "XRDeviceInfo.h"
#include <WebCore/Page.h>
#include <WebCore/SecurityOrigin.h>
#include <wtf/Vector.h>

using namespace PlatformXR;

namespace WebKit {

PlatformXRSystemProxy::PlatformXRSystemProxy(WebPage& page)
    : m_page(page)
{
    WebProcess::singleton().addMessageReceiver(Messages::PlatformXRSystemProxy::messageReceiverName(), m_page->identifier(), *this);
}

PlatformXRSystemProxy::~PlatformXRSystemProxy()
{
    WebProcess::singleton().removeMessageReceiver(Messages::PlatformXRSystemProxy::messageReceiverName(), m_page->identifier());
}

Ref<WebPage> PlatformXRSystemProxy::protectedPage() const
{
    return m_page.get();
}

void PlatformXRSystemProxy::enumerateImmersiveXRDevices(CompletionHandler<void(const Instance::DeviceList&)>&& completionHandler)
{
    protectedPage()->sendWithAsyncReply(Messages::PlatformXRSystem::EnumerateImmersiveXRDevices(), [this, weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)](Vector<XRDeviceInfo>&& devicesInfos) mutable {
        if (!weakThis)
            return;

        PlatformXR::Instance::DeviceList devices;
        for (auto& deviceInfo : devicesInfos) {
            if (auto device = deviceByIdentifier(deviceInfo.identifier))
                devices.append(*device);
            else
                devices.append(XRDeviceProxy::create(WTFMove(deviceInfo), *this));
        }
        m_devices.swap(devices);
        completionHandler(m_devices);
    });
}

void PlatformXRSystemProxy::requestPermissionOnSessionFeatures(const WebCore::SecurityOriginData& securityOriginData, PlatformXR::SessionMode mode, const PlatformXR::Device::FeatureList& granted, const PlatformXR::Device::FeatureList& consentRequired, const PlatformXR::Device::FeatureList& consentOptional, const PlatformXR::Device::FeatureList& requiredFeaturesRequested, const PlatformXR::Device::FeatureList& optionalFeaturesRequested, CompletionHandler<void(std::optional<PlatformXR::Device::FeatureList>&&)>&& completionHandler)
{
    protectedPage()->sendWithAsyncReply(Messages::PlatformXRSystem::RequestPermissionOnSessionFeatures(securityOriginData, mode, granted, consentRequired, consentOptional, requiredFeaturesRequested, optionalFeaturesRequested), WTFMove(completionHandler));
}

void PlatformXRSystemProxy::initializeTrackingAndRendering()
{
    protectedPage()->send(Messages::PlatformXRSystem::InitializeTrackingAndRendering());
}

void PlatformXRSystemProxy::shutDownTrackingAndRendering()
{
    protectedPage()->send(Messages::PlatformXRSystem::ShutDownTrackingAndRendering());
}

void PlatformXRSystemProxy::didCompleteShutdownTriggeredBySystem()
{
    protectedPage()->send(Messages::PlatformXRSystem::DidCompleteShutdownTriggeredBySystem());
}

void PlatformXRSystemProxy::requestFrame(std::optional<PlatformXR::RequestData>&& requestData, PlatformXR::Device::RequestFrameCallback&& callback)
{
    protectedPage()->sendWithAsyncReply(Messages::PlatformXRSystem::RequestFrame(WTFMove(requestData)), WTFMove(callback));
}

std::optional<PlatformXR::LayerHandle> PlatformXRSystemProxy::createLayerProjection(uint32_t, uint32_t, bool)
{
    return PlatformXRCoordinator::defaultLayerHandle();
}

void PlatformXRSystemProxy::submitFrame()
{
    protectedPage()->send(Messages::PlatformXRSystem::SubmitFrame());
}

void PlatformXRSystemProxy::sessionDidEnd(XRDeviceIdentifier deviceIdentifier)
{
    RELEASE_ASSERT(webXREnabled());

    if (auto device = deviceByIdentifier(deviceIdentifier))
        device->sessionDidEnd();
}

void PlatformXRSystemProxy::sessionDidUpdateVisibilityState(XRDeviceIdentifier deviceIdentifier, PlatformXR::VisibilityState visibilityState)
{
    RELEASE_ASSERT(webXREnabled());

    if (auto device = deviceByIdentifier(deviceIdentifier))
        device->updateSessionVisibilityState(visibilityState);
}

RefPtr<XRDeviceProxy> PlatformXRSystemProxy::deviceByIdentifier(XRDeviceIdentifier identifier)
{
    for (auto& device : m_devices) {
        auto* deviceProxy = static_cast<XRDeviceProxy*>(device.ptr());
        if (deviceProxy->identifier() == identifier)
            return deviceProxy;
    }

    return nullptr;
}

bool PlatformXRSystemProxy::webXREnabled() const
{
    Ref page = m_page.get();
    return page->corePage() && page->corePage()->settings().webXREnabled();
}

void PlatformXRSystemProxy::ref() const
{
    m_page->ref();
}

void PlatformXRSystemProxy::deref() const
{
    m_page->deref();
}

} // namespace WebKit

#endif // ENABLE(WEBXR)

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
#include <WebCore/SecurityOrigin.h>
#include <wtf/Vector.h>

using namespace PlatformXR;

namespace WebKit {

PlatformXRSystemProxy::PlatformXRSystemProxy(WebPage& page)
    : m_page(page)
{
    WebProcess::singleton().addMessageReceiver(Messages::PlatformXRSystemProxy::messageReceiverName(), m_page.identifier(), *this);
}

PlatformXRSystemProxy::~PlatformXRSystemProxy()
{
    WebProcess::singleton().removeMessageReceiver(Messages::PlatformXRSystemProxy::messageReceiverName(), m_page.identifier());
}

void PlatformXRSystemProxy::enumerateImmersiveXRDevices(CompletionHandler<void(const Instance::DeviceList&)>&& completionHandler)
{
    m_page.sendWithAsyncReply(Messages::PlatformXRSystem::EnumerateImmersiveXRDevices(), [this, weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)](Vector<XRDeviceInfo>&& devicesInfos) mutable {
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
    m_page.sendWithAsyncReply(Messages::PlatformXRSystem::RequestPermissionOnSessionFeatures(securityOriginData, mode, granted, consentRequired, consentOptional, requiredFeaturesRequested, optionalFeaturesRequested), WTFMove(completionHandler));
}

void PlatformXRSystemProxy::initializeTrackingAndRendering()
{
    m_page.send(Messages::PlatformXRSystem::InitializeTrackingAndRendering());
}

void PlatformXRSystemProxy::shutDownTrackingAndRendering()
{
    m_page.send(Messages::PlatformXRSystem::ShutDownTrackingAndRendering());
}

void PlatformXRSystemProxy::requestFrame(PlatformXR::Device::RequestFrameCallback&& callback)
{
    m_page.sendWithAsyncReply(Messages::PlatformXRSystem::RequestFrame(), WTFMove(callback));
}

std::optional<PlatformXR::LayerHandle> PlatformXRSystemProxy::createLayerProjection(uint32_t, uint32_t, bool)
{
    return PlatformXRCoordinator::defaultLayerHandle();
}

void PlatformXRSystemProxy::submitFrame()
{
    m_page.send(Messages::PlatformXRSystem::SubmitFrame());
}

void PlatformXRSystemProxy::sessionDidEnd(XRDeviceIdentifier deviceIdentifier)
{
    if (auto device = deviceByIdentifier(deviceIdentifier))
        device->sessionDidEnd();
}

void PlatformXRSystemProxy::sessionDidUpdateVisibilityState(XRDeviceIdentifier deviceIdentifier, PlatformXR::VisibilityState visibilityState)
{
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
    return m_page.corePage() && m_page.corePage()->settings().webXREnabled();
}

} // namespace WebKit

#endif // ENABLE(WEBXR)

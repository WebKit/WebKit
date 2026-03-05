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
#include <WebCore/ExceptionData.h>
#include <WebCore/Page.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/Settings.h>
#include <WebCore/XRCanvasConfiguration.h>
#include <wtf/Vector.h>

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

void PlatformXRSystemProxy::enumerateImmersiveXRDevices(CompletionHandler<void(const PlatformXR::DeviceList&)>&& completionHandler)
{
    protect(m_page)->sendWithAsyncReply(Messages::PlatformXRSystem::EnumerateImmersiveXRDevices(), [this, weakThis = WeakPtr { *this }, completionHandler = WTF::move(completionHandler)](Vector<XRDeviceInfo>&& devicesInfos) mutable {
        if (!weakThis)
            return;

        PlatformXR::DeviceList devices;
        for (auto& deviceInfo : devicesInfos) {
            if (auto device = deviceByIdentifier(deviceInfo.identifier))
                devices.append(*device);
            else
                devices.append(XRDeviceProxy::create(WTF::move(deviceInfo), *this));
        }
        m_devices.swap(devices);
        completionHandler(m_devices);
    });
}

void PlatformXRSystemProxy::requestPermissionOnSessionFeatures(const WebCore::SecurityOriginData& securityOriginData, PlatformXR::SessionMode mode, const PlatformXR::Device::FeatureList& granted, const PlatformXR::Device::FeatureList& consentRequired, const PlatformXR::Device::FeatureList& consentOptional, const PlatformXR::Device::FeatureList& requiredFeaturesRequested, const PlatformXR::Device::FeatureList& optionalFeaturesRequested, CompletionHandler<void(std::optional<PlatformXR::Device::FeatureList>&&)>&& completionHandler)
{
    protect(m_page)->sendWithAsyncReply(Messages::PlatformXRSystem::RequestPermissionOnSessionFeatures(securityOriginData, mode, granted, consentRequired, consentOptional, requiredFeaturesRequested, optionalFeaturesRequested), WTF::move(completionHandler));
}

void PlatformXRSystemProxy::initializeTrackingAndRendering(std::optional<WebCore::XRCanvasConfiguration>&& optionalInit)
{
    std::optional<WebCore::WebGPU::TextureFormat> colorFormat;
    std::optional<WebCore::WebGPU::TextureFormat> depthStencilFormat;
    if (optionalInit) {
        colorFormat = optionalInit->colorFormat;
        depthStencilFormat = optionalInit->depthStencilFormat;
    }
    protect(m_page)->send(Messages::PlatformXRSystem::InitializeTrackingAndRendering(WTF::move(colorFormat), WTF::move(depthStencilFormat)));
}

void PlatformXRSystemProxy::shutDownTrackingAndRendering()
{
    protect(m_page)->send(Messages::PlatformXRSystem::ShutDownTrackingAndRendering());
}

void PlatformXRSystemProxy::didCompleteShutdownTriggeredBySystem()
{
    protect(m_page)->send(Messages::PlatformXRSystem::DidCompleteShutdownTriggeredBySystem());
}

void PlatformXRSystemProxy::requestFrame(std::optional<PlatformXR::RequestData>&& requestData, PlatformXR::Device::RequestFrameCallback&& callback)
{
    protect(m_page)->sendWithAsyncReply(Messages::PlatformXRSystem::RequestFrame(WTF::move(requestData)), WTF::move(callback));
}

std::optional<PlatformXR::LayerHandle> PlatformXRSystemProxy::createLayerProjection(uint32_t width, uint32_t height, bool alpha)
{
#if USE(OPENXR)
    auto result = protect(m_page)->sendSync(Messages::PlatformXRSystem::CreateLayerProjection(width, height, alpha));
    if (!result.succeeded())
        return std::nullopt;
    auto [layerHandle] = result.takeReply();
    return layerHandle;
#else
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(alpha);
    return PlatformXRCoordinator::defaultLayerHandle();
#endif
}

#if USE(OPENXR)
void PlatformXRSystemProxy::submitFrame(Vector<PlatformXR::Device::Layer>&& layers)
{
    Vector<WebKit::XRDeviceLayer> deviceLayers;
    for (auto& layer : layers) {
        deviceLayers.append(WebKit::XRDeviceLayer {
            .handle = layer.handle,
            .visible = layer.visible,
            .views = layer.views,
            .fenceFD = WTF::move(layer.fenceFD)
        });
    }
    protect(m_page)->send(Messages::PlatformXRSystem::SubmitFrame(WTF::move(deviceLayers)));
}
#else
void PlatformXRSystemProxy::submitFrame()
{
    protect(m_page)->send(Messages::PlatformXRSystem::SubmitFrame());
}
#endif

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

#if ENABLE(WEBXR_HIT_TEST)
void PlatformXRSystemProxy::requestHitTestSource(const PlatformXR::HitTestOptions& options, CompletionHandler<void(WebCore::ExceptionOr<PlatformXR::HitTestSource>)>&& completionHandler)
{
    protect(m_page)->sendWithAsyncReply(Messages::PlatformXRSystem::RequestHitTestSource(options), [protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)](Expected<PlatformXR::HitTestSource, WebCore::ExceptionData> exceptionOrSource) mutable {
        if (exceptionOrSource)
            completionHandler(WTF::move(exceptionOrSource).value());
        else
            completionHandler(WTF::move(exceptionOrSource).error().toException());
    });
}

void PlatformXRSystemProxy::deleteHitTestSource(PlatformXR::HitTestSource source)
{
    protect(m_page)->send(Messages::PlatformXRSystem::DeleteHitTestSource(source));
}

void PlatformXRSystemProxy::requestTransientInputHitTestSource(const PlatformXR::TransientInputHitTestOptions& options, CompletionHandler<void(WebCore::ExceptionOr<PlatformXR::TransientInputHitTestSource>)>&& completionHandler)
{
    protect(m_page)->sendWithAsyncReply(Messages::PlatformXRSystem::RequestTransientInputHitTestSource(options), [protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)](Expected<PlatformXR::TransientInputHitTestSource, WebCore::ExceptionData> exceptionOrSource) mutable {
        if (exceptionOrSource)
            completionHandler(WTF::move(exceptionOrSource).value());
        else
            completionHandler(WTF::move(exceptionOrSource).error().toException());
    });
}

void PlatformXRSystemProxy::deleteTransientInputHitTestSource(PlatformXR::TransientInputHitTestSource source)
{
    protect(m_page)->send(Messages::PlatformXRSystem::DeleteTransientInputHitTestSource(source));
}
#endif

} // namespace WebKit

#endif // ENABLE(WEBXR)

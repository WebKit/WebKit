/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "XRDeviceProxy.h"

#if ENABLE(WEBXR)

#include "PlatformXRSystemProxy.h"
#include "XRDeviceInfo.h"
#include <WebCore/SecurityOriginData.h>

using namespace PlatformXR;

namespace WebKit {

Ref<XRDeviceProxy> XRDeviceProxy::create(XRDeviceInfo&& deviceInfo, PlatformXRSystemProxy& xrSystem)
{
    return adoptRef(*new XRDeviceProxy(WTFMove(deviceInfo), xrSystem));
}

XRDeviceProxy::XRDeviceProxy(XRDeviceInfo&& deviceInfo, PlatformXRSystemProxy& xrSystem)
    : m_identifier(deviceInfo.identifier)
    , m_xrSystem(xrSystem)
{
    m_supportsStereoRendering = deviceInfo.supportsStereoRendering;
    m_supportsOrientationTracking = deviceInfo.supportsOrientationTracking;
    m_recommendedResolution = deviceInfo.recommendedResolution;
    if (!deviceInfo.vrFeatures.isEmpty())
        setSupportedFeatures(SessionMode::ImmersiveVr, deviceInfo.vrFeatures);
    if (!deviceInfo.arFeatures.isEmpty())
        setSupportedFeatures(SessionMode::ImmersiveAr, deviceInfo.arFeatures);
}

void XRDeviceProxy::sessionDidEnd()
{
    if (trackingAndRenderingClient())
        trackingAndRenderingClient()->sessionDidEnd();
}

void XRDeviceProxy::updateSessionVisibilityState(PlatformXR::VisibilityState visibilityState)
{
    if (trackingAndRenderingClient())
        trackingAndRenderingClient()->updateSessionVisibilityState(visibilityState);
}

void XRDeviceProxy::initializeTrackingAndRendering(const WebCore::SecurityOriginData& securityOriginData, PlatformXR::SessionMode sessionMode, const PlatformXR::Device::FeatureList& requestedFeatures)
{
    if (!isImmersive(sessionMode))
        return;

    RefPtr xrSystem = m_xrSystem.get();
    if (!xrSystem)
        return;

    xrSystem->initializeTrackingAndRendering();

    // This is called from the constructor of WebXRSession. Since sessionDidInitializeInputSources()
    // ends up calling queueTaskKeepingObjectAlive() which refs the WebXRSession object, we
    // should delay this call after the WebXRSession has finished construction.
    callOnMainRunLoop([this, weakThis = ThreadSafeWeakPtr { *this }]() {
        auto protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (trackingAndRenderingClient())
            trackingAndRenderingClient()->sessionDidInitializeInputSources({ });
    });    
}

void XRDeviceProxy::shutDownTrackingAndRendering()
{
    if (RefPtr xrSystem = m_xrSystem.get())
        xrSystem->shutDownTrackingAndRendering();
}

void XRDeviceProxy::didCompleteShutdownTriggeredBySystem()
{
    if (RefPtr xrSystem = m_xrSystem.get())
        xrSystem->didCompleteShutdownTriggeredBySystem();
}

Vector<PlatformXR::Device::ViewData> XRDeviceProxy::views(SessionMode mode) const
{
    Vector<Device::ViewData> views;
    if (m_supportsStereoRendering && mode == SessionMode::ImmersiveVr) {
        views.append({ .active = true, .eye = Eye::Left });
        views.append({ .active = true, .eye = Eye::Right });
    } else
        views.append({ .active = true, .eye = Eye::None });
    return views;
}

void XRDeviceProxy::requestFrame(std::optional<PlatformXR::RequestData>&& requestData, PlatformXR::Device::RequestFrameCallback&& callback)
{
    if (RefPtr xrSystem = m_xrSystem.get())
        xrSystem->requestFrame(WTFMove(requestData), WTFMove(callback));
    else
        callback({ });
}

std::optional<PlatformXR::LayerHandle> XRDeviceProxy::createLayerProjection(uint32_t width, uint32_t height, bool alpha)
{
    RefPtr xrSystem = m_xrSystem.get();
    return xrSystem ? xrSystem->createLayerProjection(width, height, alpha) : std::nullopt;
}

void XRDeviceProxy::submitFrame(Vector<PlatformXR::Device::Layer>&&)
{
    if (RefPtr xrSystem = m_xrSystem.get())
        xrSystem->submitFrame();
}

} // namespace WebKit

#endif // ENABLE(WEBXR)

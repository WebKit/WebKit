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
#include "XRDeviceProxy.h"

#if ENABLE(WEBXR)

#include "PlatformXRSystemProxy.h"
#include "XRDeviceInfo.h"

using namespace PlatformXR;

namespace WebKit {

Ref<XRDeviceProxy> XRDeviceProxy::create(XRDeviceInfo&& deviceInfo, PlatformXRSystemProxy& xrSystem)
{
    return adoptRef(*new XRDeviceProxy(WTFMove(deviceInfo), xrSystem));
}

XRDeviceProxy::XRDeviceProxy(XRDeviceInfo&& deviceInfo, PlatformXRSystemProxy& xrSystem)
    : m_xrSystem(makeWeakPtr(xrSystem))
{
    m_identifier = deviceInfo.identifier;
    m_supportsStereoRendering = deviceInfo.supportsStereoRendering;
    m_supportsOrientationTracking = deviceInfo.supportsOrientationTracking;
    m_recommendedResolution = deviceInfo.recommendedResolution;
    if (!deviceInfo.features.isEmpty())
        setEnabledFeatures(SessionMode::ImmersiveVr, deviceInfo.features);
}

void XRDeviceProxy::sessionDidEnd()
{
    if (trackingAndRenderingClient())
        trackingAndRenderingClient()->sessionDidEnd();
}

void XRDeviceProxy::initializeTrackingAndRendering(PlatformXR::SessionMode sessionMode)
{
    if (sessionMode != PlatformXR::SessionMode::ImmersiveVr)
        return;

    if (m_xrSystem)
        m_xrSystem->initializeTrackingAndRendering();
}

void XRDeviceProxy::shutDownTrackingAndRendering()
{
    if (m_xrSystem)
        m_xrSystem->shutDownTrackingAndRendering();
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

void XRDeviceProxy::requestFrame(PlatformXR::Device::RequestFrameCallback&& callback)
{
    if (m_xrSystem)
        m_xrSystem->requestFrame(WTFMove(callback));
}

std::optional<PlatformXR::LayerHandle> XRDeviceProxy::createLayerProjection(uint32_t width, uint32_t height, bool alpha)
{
    return m_xrSystem ? m_xrSystem->createLayerProjection(width, height, alpha) : std::nullopt;
}

void XRDeviceProxy::submitFrame(Vector<PlatformXR::Device::Layer>&&)
{
    if (m_xrSystem)
        m_xrSystem->submitFrame();
}

} // namespace WebKit

#endif // ENABLE(WEBXR)

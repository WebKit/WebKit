/*
 * Copyright (C) 2025 Apple, Inc. All rights reserved.
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
#include "XREquirectLayer.h"

#if ENABLE(WEBXR_LAYERS)

#include "WebXRRigidTransform.h"
#include "WebXRSession.h"
#include "XRLayerBacking.h"
#include <numbers>
#include <wtf/MathExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(XREquirectLayer);

XREquirectLayer::XREquirectLayer(ScriptExecutionContext& scriptExecutionContext, WebXRSession& session, Ref<XRLayerBacking>&& backing, const XREquirectLayerInit& init)
    : XRCompositionLayer(&scriptExecutionContext, session, WTF::move(backing), init, init.space, init.transform)
{
    // Explicitly call setters to add validation.
    setRadius(init.radius);
    setCentralHorizontalAngle(init.centralHorizontalAngle);
    setUpperVerticalAngle(init.upperVerticalAngle);
    setLowerVerticalAngle(init.lowerVerticalAngle);

    setIsStatic(init.isStatic);
}

XREquirectLayer::~XREquirectLayer() = default;

void XREquirectLayer::setRadius(float radius)
{
    m_radius = std::max(0.f, radius);
    setNeedsRedraw(true);
}

void XREquirectLayer::setCentralHorizontalAngle(float angle)
{
    constexpr float twoPiFloat = static_cast<float>(std::numbers::pi * 2);
    m_centralHorizontalAngle = std::clamp(angle, 0.f, twoPiFloat);
    setNeedsRedraw(true);
}

void XREquirectLayer::setUpperVerticalAngle(float angle)
{
    m_upperVerticalAngle = std::clamp(angle, -piOverTwoFloat, piOverTwoFloat);
    setNeedsRedraw(true);
}

void XREquirectLayer::setLowerVerticalAngle(float angle)
{
    m_lowerVerticalAngle = std::clamp(angle, -piOverTwoFloat, piOverTwoFloat);
    setNeedsRedraw(true);
}

void XREquirectLayer::fillInTypeSpecificDeviceLayerData(PlatformXR::DeviceLayer& layerData) const
{
#if PLATFORM(GTK) || PLATFORM(WPE)
    layerData.equirectLayerData = {
        .radius = m_radius,
        .centralHorizontalAngle = m_centralHorizontalAngle,
        .upperVerticalAngle = m_upperVerticalAngle,
        .lowerVerticalAngle = m_lowerVerticalAngle,
        .poseInLocalSpace = poseInLocalSpace(),
    };
#else
    UNUSED_PARAM(layerData);
#endif
}

} // namespace WebCore

#endif // ENABLE(WEBXR_LAYERS)

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
#include "XRCylinderLayer.h"

#if ENABLE(WEBXR_LAYERS)
#include "WebXRRigidTransform.h"
#include "WebXRSession.h"
#include "XRLayerBacking.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(XRCylinderLayer);

XRCylinderLayer::XRCylinderLayer(ScriptExecutionContext& scriptExecutionContext, WebXRSession& session, Ref<XRLayerBacking>&& backing, const XRCylinderLayerInit& init)
    : XRCompositionLayer(&scriptExecutionContext, session, WTF::move(backing), init, init.space, init.transform)
{
    // Explicitly call setters to add validation.
    setRadius(init.radius);
    setCentralAngle(init.centralAngle);
    setAspectRatio(init.aspectRatio);

    setIsStatic(init.isStatic);
}

XRCylinderLayer::~XRCylinderLayer() = default;

void XRCylinderLayer::setRadius(float radius)
{
    m_radius = std::max(std::numeric_limits<float>::epsilon(), radius);
    setNeedsRedraw(true);
}

void XRCylinderLayer::setCentralAngle(float angle)
{
    // (0, 2 * pi) although specs recommend 1.9 * pi as a practical limit.
    static constexpr float MaxCentralAngle = 1.9 * static_cast<float>(M_PI);

    m_centralAngle = std::clamp(angle, std::numeric_limits<float>::epsilon(), MaxCentralAngle);
    setNeedsRedraw(true);
}

void XRCylinderLayer::setAspectRatio(float ratio)
{
    m_aspectRatio = std::max(std::numeric_limits<float>::epsilon(), ratio);
    setNeedsRedraw(true);
}

void XRCylinderLayer::fillInTypeSpecificDeviceLayerData(PlatformXR::DeviceLayer& layerData) const
{
#if PLATFORM(GTK) || PLATFORM(WPE)
    layerData.cylinderLayerData = {
        .radius = m_radius,
        .centralAngle = m_centralAngle,
        .aspectRatio = m_aspectRatio,
        .poseInLocalSpace = poseInLocalSpace(),
    };
#else
    UNUSED_PARAM(layerData);
#endif
}

} // namespace WebCore

#endif // ENABLE(WEBXR_LAYERS)

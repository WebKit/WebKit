/*
 * Copyright (C) 2024 Apple, Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEBXR_LAYERS)

#include "ExceptionOr.h"
#include "XRCompositionLayer.h"
#include "XREquirectLayerInit.h"
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class WebXRRigidTransform;
class WebXRSession;
class WebXRSpace;
class XRLayerBacking;

// https://immersive-web.github.io/layers/#xrequirectlayertype
class XREquirectLayer : public XRCompositionLayer {
    WTF_MAKE_TZONE_ALLOCATED(XREquirectLayer);
public:
    static Ref<XREquirectLayer> create(ScriptExecutionContext& scriptExecutionContext, WebXRSession& session, Ref<XRLayerBacking>&& backing, const XREquirectLayerInit& init)
    {
        return adoptRef(*new XREquirectLayer(scriptExecutionContext, session, WTF::move(backing), init));
    }

    virtual ~XREquirectLayer();

    const WebXRSpace& space() const;
    void setSpace(WebXRSpace&);
    const WebXRRigidTransform& transform() const;
    void setTransform(WebXRRigidTransform&);

    float radius() const { return m_radius; }
    void setRadius(float radius) { m_radius = radius; setNeedsRedraw(true); }
    float centralHorizontalAngle() const { return m_centralHorizontalAngle; }
    void setCentralHorizontalAngle(float angle) { m_centralHorizontalAngle = angle; setNeedsRedraw(true); }
    float upperVerticalAngle() const { return m_upperVerticalAngle; }
    void setUpperVerticalAngle(float angle) { m_upperVerticalAngle = angle; setNeedsRedraw(true); }
    float lowerVerticalAngle() const { return m_lowerVerticalAngle; }
    void setLowerVerticalAngle(float angle) { m_lowerVerticalAngle = angle; setNeedsRedraw(true); }

private:
    XREquirectLayer(ScriptExecutionContext&, WebXRSession&, Ref<XRLayerBacking>&&, const XREquirectLayerInit&);
    bool isXREquirectLayer() const final { return true; }
    void recomputePose();

    RefPtr<WebXRSpace> m_space;
    RefPtr<WebXRRigidTransform> m_transform;
    float m_radius;
    float m_centralHorizontalAngle;
    float m_upperVerticalAngle;
    float m_lowerVerticalAngle;
    PlatformXR::FrameData::Pose m_poseInLocalSpace;

    // WebXRLayer.
    void startFrame(PlatformXR::FrameData&) final;
    PlatformXR::DeviceLayer endFrame() final;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::XREquirectLayer)
    static bool isType(const WebCore::WebXRLayer& layer) { return layer.isXREquirectLayer(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WEBXR_LAYERS)

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
#include "XRCylinderLayerInit.h"
#include <wtf/Ref.h>

namespace WebCore {

class WebXRSession;
class XRLayerBacking;

// https://immersive-web.github.io/layers/#xrcylinderayertype
class XRCylinderLayer : public XRCompositionLayer {
    WTF_MAKE_TZONE_ALLOCATED(XRCylinderLayer);
public:
    static Ref<XRCylinderLayer> create(ScriptExecutionContext& scriptExecutionContext, WebXRSession& session, Ref<XRLayerBacking>&& backing, const XRCylinderLayerInit& init)
    {
        return adoptRef(*new XRCylinderLayer(scriptExecutionContext, session, WTF::move(backing), init));
    }

    virtual ~XRCylinderLayer();

    float radius() const { return m_radius; }
    void setRadius(float);
    float centralAngle() const { return m_centralAngle; }
    void setCentralAngle(float);
    float aspectRatio() const { return m_aspectRatio; }
    void setAspectRatio(float);

private:
    XRCylinderLayer(ScriptExecutionContext&, WebXRSession&, Ref<XRLayerBacking>&&, const XRCylinderLayerInit&);
    bool isXRCylinderLayer() const final { return true; }

    void fillInTypeSpecificDeviceLayerData(PlatformXR::DeviceLayer&) const final;

    float m_radius;
    float m_centralAngle;
    float m_aspectRatio;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::XRCylinderLayer)
    static bool isType(const WebCore::WebXRLayer& layer) { return layer.isXRCylinderLayer(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WEBXR_LAYERS)

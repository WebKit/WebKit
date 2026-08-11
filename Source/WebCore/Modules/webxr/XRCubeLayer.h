
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

#include "DOMPointReadOnly.h"
#include "XRCompositionLayer.h"
#include "XRCubeLayerInit.h"
#include <wtf/Ref.h>

namespace WebCore {

class WebXRSession;
class XRLayerBacking;

// https://immersive-web.github.io/layers/#xcubelayertype
class XRCubeLayer : public XRCompositionLayer {
    WTF_MAKE_TZONE_ALLOCATED(XRCubeLayer);
public:
    static Ref<XRCubeLayer> create(ScriptExecutionContext& scriptExecutionContext, WebXRSession& session, Ref<XRLayerBacking>&& backing, const XRCubeLayerInit& init)
    {
        return adoptRef(*new XRCubeLayer(scriptExecutionContext, session, WTF::move(backing), init));
    }

    virtual ~XRCubeLayer();

    const DOMPointReadOnly& orientation() const { return m_orientation.get(); }
    void setOrientation(DOMPointReadOnly&);

private:
    XRCubeLayer(ScriptExecutionContext&, WebXRSession&, Ref<XRLayerBacking>&&, const XRCubeLayerInit&);
    bool isXRCubeLayer() const final { return true; }

    void fillInTypeSpecificDeviceLayerData(PlatformXR::DeviceLayer&) const final;

    Ref<DOMPointReadOnly> m_orientation;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::XRCubeLayer)
    static bool isType(const WebCore::WebXRLayer& layer) { return layer.isXRCubeLayer(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WEBXR_LAYERS)

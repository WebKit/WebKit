/*
 * Copyright (C) 2024-2025 Apple, Inc. All rights reserved.
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

#include "PlatformXR.h"
#include "XRCompositionLayer.h"
#include "XRProjectionLayerInit.h"
#include <WebCore/IntRect.h>
#include <wtf/RefPtr.h>

#if PLATFORM(COCOA)
#include <wtf/MachSendRight.h>
#endif

namespace WebCore {

class GPUTexture;
class WebXRRigidTransform;

class XRProjectionLayer : public XRCompositionLayer {
    WTF_MAKE_TZONE_ALLOCATED(XRProjectionLayer);
public:
    static Ref<XRProjectionLayer> create(ScriptExecutionContext& scriptExecutionContext, WebXRSession& session, Ref<XRLayerBacking>&& backing, const XRProjectionLayerInit& init)
    {
        return adoptRef(*new XRProjectionLayer(scriptExecutionContext, session, WTF::move(backing), init));
    }
    virtual ~XRProjectionLayer();

    uint32_t textureWidth() const;
    uint32_t textureHeight() const;
    uint32_t textureArrayLength() const;

    bool ignoreDepthValues() const;
    void setIgnoreDepthValues(bool ignoreDepthValues) { m_ignoreDepthValues = ignoreDepthValues; }

    std::optional<float> fixedFoveation() const;
    void setFixedFoveation(std::optional<float>);
    WebXRRigidTransform* NODELETE deltaPose() const;
    void setDeltaPose(WebXRRigidTransform*);

    // WebXRLayer
    void startFrame(PlatformXR::FrameData&) final;
    PlatformXR::DeviceLayer endFrame() final;
#if ENABLE(WEBGPU)
    std::optional<PlatformXR::FrameData::LayerData> layerData() const;
#endif

private:
    XRProjectionLayer(ScriptExecutionContext&, WebXRSession&, Ref<XRLayerBacking>&&, const XRProjectionLayerInit&);

    bool isXRProjectionLayer() const final { return true; }
    Vector<IntRect> computeViewports();

    bool m_ignoreDepthValues { false };
#if ENABLE(WEBGPU)
    std::optional<PlatformXR::FrameData::LayerData> m_layerData;
#endif
    RefPtr<WebXRRigidTransform> m_transform;
    Vector<IntRect> m_viewports;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::XRProjectionLayer)
    static bool isType(const WebCore::WebXRLayer& layer) { return layer.isXRProjectionLayer(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif

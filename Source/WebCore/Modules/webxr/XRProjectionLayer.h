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

#include "WebGPUXRProjectionLayer.h"
#include "WebXRRigidTransform.h"
#include "XRCompositionLayer.h"

namespace WebCore {

namespace WebGPU {
class XRProjectionLayer;
}

class GPUTexture;

class XRProjectionLayer : public XRCompositionLayer {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(XRProjectionLayer);
public:
    static Ref<XRProjectionLayer> create(ScriptExecutionContext& scriptExecutionContext, Ref<WebCore::WebGPU::XRProjectionLayer>&& backing)
    {
        return adoptRef(*new XRProjectionLayer(scriptExecutionContext, WTFMove(backing)));
    }
    virtual ~XRProjectionLayer();

    uint32_t textureWidth() const;
    uint32_t textureHeight() const;
    uint32_t textureArrayLength() const;

    bool ignoreDepthValues() const;
    std::optional<float> fixedFoveation() const;
    [[noreturn]] void setFixedFoveation(std::optional<float>);
    WebXRRigidTransform* deltaPose() const;
    [[noreturn]] void setDeltaPose(WebXRRigidTransform*);

    // WebXRLayer
    void startFrame(const PlatformXR::FrameData&) final;
    PlatformXR::Device::Layer endFrame() final;

    WebCore::WebGPU::XRProjectionLayer& backing();
private:
    XRProjectionLayer(ScriptExecutionContext&, Ref<WebCore::WebGPU::XRProjectionLayer>&&);

    Ref<WebCore::WebGPU::XRProjectionLayer> m_backing;
};

} // namespace WebCore

#endif

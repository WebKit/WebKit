/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUPtr.h"
#include "WebGPUXRProjectionLayer.h"

#include <WebGPU/WebGPU.h>

namespace WebCore {
class Device;
class NativeImage;
}

namespace WebCore::WebGPU {

class ConvertToBackingContext;

class XRProjectionLayerImpl final : public XRProjectionLayer {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<XRProjectionLayerImpl> create(WebGPUPtr<WGPUXRProjectionLayer>&& projectionLayer, ConvertToBackingContext& convertToBackingContext)
    {
        return adoptRef(*new XRProjectionLayerImpl(WTFMove(projectionLayer), convertToBackingContext));
    }

    virtual ~XRProjectionLayerImpl();

private:
    friend class DowncastConvertToBackingContext;

    explicit XRProjectionLayerImpl(WebGPUPtr<WGPUXRProjectionLayer>&&, ConvertToBackingContext&);

    XRProjectionLayerImpl(const XRProjectionLayerImpl&) = delete;
    XRProjectionLayerImpl(XRProjectionLayerImpl&&) = delete;
    XRProjectionLayerImpl& operator=(const XRProjectionLayerImpl&) = delete;
    XRProjectionLayerImpl& operator=(XRProjectionLayerImpl&&) = delete;

    WGPUXRProjectionLayer backing() const { return m_backing.get(); }

    uint32_t textureWidth() const final;
    uint32_t textureHeight() const final;
    uint32_t textureArrayLength() const final;

    bool ignoreDepthValues() const final;
    std::optional<float> fixedFoveation() const final;
    void setFixedFoveation(std::optional<float>) final;
    WebXRRigidTransform* deltaPose() const final;
    void setDeltaPose(WebXRRigidTransform*) final;

    // WebXRLayer
    void startFrame() final;
    void endFrame() final;

    WebGPUPtr<WGPUXRProjectionLayer> m_backing;
    Ref<ConvertToBackingContext> m_convertToBackingContext;
};

} // namespace WebCore::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)

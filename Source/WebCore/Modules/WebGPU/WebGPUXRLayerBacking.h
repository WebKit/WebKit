/*
 * Copyright (C) 2025 Igalia S.L. All rights reserved.
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

#include <WebCore/PlatformXR.h>
#include <WebCore/WebGPUCppAPI.h>
#include <WebCore/XRLayerBacking.h>
#include <wtf/Ref.h>

namespace WebCore {

// Presents a ::WebGPU::XRProjectionLayer to WebXR, which knows the layers by their XRLayerBacking. The
// frames that the compositor supplies carry the sizes of the textures.
class WebGPUXRLayerBacking final : public XRLayerBacking {
    WTF_MAKE_TZONE_ALLOCATED(WebGPUXRLayerBacking);
public:
    static Ref<WebGPUXRLayerBacking> create(Ref<::WebGPU::XRProjectionLayer>&& projectionLayer)
    {
        return adoptRef(*new WebGPUXRLayerBacking(WTF::move(projectionLayer)));
    }

    ::WebGPU::XRProjectionLayer& projectionLayer() const { return m_projectionLayer; }

    uint32_t colorTextureWidth() const final { return 0; }
    uint32_t colorTextureHeight() const final { return 0; }
    uint32_t colorTextureArrayLength() const final { return 0; }
    bool allColorTexturesAreBound() const final { return false; }

#if PLATFORM(COCOA)
    void startFrame(size_t frameIndex, MachSendRight&& colorBuffer, MachSendRight&& depthBuffer, MachSendRight&& completionSyncEvent, size_t reusableTextureIndex, PlatformXR::RateMapDescription&&) final;

    void endFrame() final
    {
        m_projectionLayer->endFrame();
    }
#else
    void startFrame(PlatformXR::FrameData&) final { }

    void endFrame(PlatformXR::DeviceLayer&) final
    {
        m_projectionLayer->endFrame();
    }
#endif

private:
    explicit WebGPUXRLayerBacking(Ref<::WebGPU::XRProjectionLayer>&& projectionLayer)
        : m_projectionLayer(WTF::move(projectionLayer))
    {
    }

    const Ref<::WebGPU::XRProjectionLayer> m_projectionLayer;
};

} // namespace WebCore

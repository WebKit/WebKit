/*
 * Copyright (C) 2026 Igalia S.L. All rights reserved.
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

#include "GraphicsTypesGL.h"
#include "IntSize.h"
#include "XRLayerBacking.h"
#include <wtf/TZoneMalloc.h>

namespace PlatformXR {
struct FrameData;
class Device;
struct Layer;
}

namespace WebCore {

class WebGLOpaqueTexture;
class WebGLRenderingContextBase;
class WebXROpaqueFramebuffer;
class WebXRWebGLSwapchain;

class XRWebGLLayerBacking : public XRLayerBacking {
    WTF_MAKE_TZONE_ALLOCATED(XRWebGLLayerBacking);
public:
    uint32_t colorTextureWidth() const final;
    uint32_t colorTextureHeight() const final;
    uint32_t colorTextureArrayLength() const final;

    std::optional<uint32_t> depthTextureWidth() const final;
    std::optional<uint32_t> depthTextureHeight() const final;

#if PLATFORM(COCOA)
    void startFrame(size_t frameIndex, MachSendRight&& colorBuffer, MachSendRight&& depthBuffer, MachSendRight&& completionSyncEvent, size_t reusableTextureIndex, PlatformXR::RateMapDescription&&) final;
    void endFrame() final;
#else
    void startFrame(PlatformXR::FrameData&) final;
    void endFrame(PlatformXR::DeviceLayer&) final;
#endif

    RefPtr<WebGLOpaqueTexture> currentColorTexture() const;
    RefPtr<WebGLOpaqueTexture> currentDepthTexture() const;

    bool allColorTexturesAreBound() const final;

protected:
    XRWebGLLayerBacking(PlatformXR::LayerHandle, std::unique_ptr<WebXRWebGLSwapchain>&& colorSwapchain, std::unique_ptr<WebXRWebGLSwapchain>&& depthSwapchain);

    static std::unique_ptr<WebXRWebGLSwapchain> createDepthSwapchain(WebGLRenderingContextBase&, GCGLenum depthFormat, IntSize, bool clearOnAccess, size_t imageCount);

    std::unique_ptr<WebXRWebGLSwapchain> m_colorSwapchain;
    std::unique_ptr<WebXRWebGLSwapchain> m_depthSwapchain;
};

} // namespace WebCore

#endif // ENABLE(WEBXR_LAYERS)

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

#include "ExceptionOr.h"
#include "GraphicsTypesGL.h"
#include "IntSize.h"
#include "XRLayerBacking.h"
#include <optional>
#include <wtf/TZoneMalloc.h>

namespace PlatformXR {
struct FrameData;
class Device;
struct Layer;
enum class CompositionLayerType : uint8_t;
}

namespace WebCore {

// Based on https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/modules/xr/xr_webgl_binding.cc
struct SwapchainFormats {
    GCGLenum format { 0 };
    GCGLenum internalFormat { 0 };
};
SwapchainFormats swapchainFormatsForLayerFormat(GCGLenum layerFormat);

class WebGLOpaqueTexture;
class WebGLRenderingContextBase;
class WebXROpaqueFramebuffer;
class WebXRSession;
class WebXRWebGLSwapchain;
struct XRLayerInit;
struct XRProjectionLayerInit;

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

    RefPtr<WebGLOpaqueTexture> currentColorTexture(uint32_t index = 0) const;
    RefPtr<WebGLOpaqueTexture> currentDepthTexture(uint32_t index = 0) const;

    // Returns true for non-array stereo cube layers, which require two separate GL_TEXTURE_CUBE_MAP
    // objects (one per view). All other layer types use a single texture regardless of layout.
    bool requiresPerViewColorTextures() const;

    bool allColorTexturesAreBound() const final;

    void clearTexturesIfNeeded(const IntRect& viewport, std::optional<uint32_t> slice) final;

    bool isWebGLBacking() const final { return true; }

protected:
    XRWebGLLayerBacking(PlatformXR::LayerHandle, std::unique_ptr<WebXRWebGLSwapchain>&& colorSwapchain, std::unique_ptr<WebXRWebGLSwapchain>&& depthSwapchain, uint32_t colorTextureArrayLength);

    struct XRLayerSwapchains {
        PlatformXR::LayerHandle handle;
        std::unique_ptr<WebXRWebGLSwapchain> colorSwapchain;
        std::unique_ptr<WebXRWebGLSwapchain> depthSwapchain;
        uint32_t colorTextureArrayLength { 1 };
    };

    static ExceptionOr<XRLayerSwapchains> createCompositionLayerSwapchains(WebXRSession&, WebGLRenderingContextBase&, PlatformXR::CompositionLayerType, const XRLayerInit&);
    static ExceptionOr<XRLayerSwapchains> createProjectionLayerSwapchains(WebXRSession&, WebGLRenderingContextBase&, const XRProjectionLayerInit&);

private:
    static ExceptionOr<XRLayerSwapchains> createColorAndDepthSwapchains(WebGLRenderingContextBase&, PlatformXR::LayerHandle, GCGLenum colorFormat, std::optional<GCGLenum> depthFormat, IntSize, bool clearOnAccess, size_t numImages, uint32_t arrayLength, GCGLenum colorTextureType);
    static std::unique_ptr<WebXRWebGLSwapchain> createDepthSwapchain(WebGLRenderingContextBase&, GCGLenum depthFormat, IntSize, bool clearOnAccess, size_t imageCount, uint32_t arrayLength, GCGLenum textureType);

    std::unique_ptr<WebXRWebGLSwapchain> m_colorSwapchain;
    std::unique_ptr<WebXRWebGLSwapchain> m_depthSwapchain;
    uint32_t m_colorTextureArrayLength { 1 };
#if !PLATFORM(COCOA)
    bool m_shouldSkipFrame { true };
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::XRWebGLLayerBacking)
    static bool isType(const WebCore::XRLayerBacking& backing) { return backing.isWebGLBacking(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WEBXR_LAYERS)

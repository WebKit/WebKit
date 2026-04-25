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

#include "GraphicsContextGL.h"
#include "GraphicsTypesGL.h"
#include "PlatformXR.h"
#include "WebXRSwapchain.h"
#include <wtf/Ref.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class IntSize;
class WebGLFramebuffer;
class WebGLOpaqueTexture;
class WebGLRenderingContextBase;
struct XRWebGLLayerInit;

struct WebXRExternalImage {
    PlatformGLObject tex;
    GCGLOwnedExternalImage image;

    explicit operator bool() const { return !!image; }

    void destroyImage(GraphicsContextGL&);
    void release(GraphicsContextGL&);
    void leakObject();
};

template<typename T>
struct WebXRImageSet {
    T colorBuffer;

    operator bool() const
    {
        return !!colorBuffer;
    }

    void release(GraphicsContextGL& gl)
    {
        colorBuffer.release(gl);
    }

    void leakObject()
    {
        colorBuffer.leakObject();
    }
};

using WebXRExternalImages = WebXRImageSet<WebXRExternalImage>;

class WebXRWebGLSwapchain : public WebXRSwapchain {
public:
    ~WebXRWebGLSwapchain() override;

    RefPtr<WebGLRenderingContextBase> context();

    IntSize size() const { return m_texSize; }

    virtual void startFrame(PlatformXR::FrameData::LayerData&) = 0;
    virtual void endFrame(PlatformXR::DeviceLayer&) = 0;

    virtual bool allTexturesAreBound() const = 0;

protected:
    WebXRWebGLSwapchain(WebGLRenderingContextBase&, SwapchainTargets, bool clearOnAccess, size_t imageCount);
    void clearCurrentTexture(GraphicsContextGL&);

    PlatformXR::LayerHandle m_handle;
    RefPtr<WebGLRenderingContextBase> m_context;

    RefPtr<WebGLFramebuffer> m_framebufferForClearing;

    size_t m_currentImageIndex { 0 };
    IntSize m_texSize;

    size_t m_imageCount { 0 };
};

// This class represents a swapchain that uses shared images provided by the platform's XR compositor.
// It manages the lifecycle of these shared images and binds them to the WebGL context for rendering.
class WebXRWebGLSharedImageSwapchain final : public WebXRWebGLSwapchain {
public:
    static std::unique_ptr<WebXRWebGLSharedImageSwapchain> create(WebGLRenderingContextBase&, SwapchainTargets, GCGLenum format, bool clearOnAccess, size_t imageCount);
    ~WebXRWebGLSharedImageSwapchain() override;

    PlatformGLObject currentTexture() override;

    void startFrame(PlatformXR::FrameData::LayerData&) override;
    void endFrame(PlatformXR::DeviceLayer&) override;

    bool allTexturesAreBound() const override;

private:
    WebXRWebGLSharedImageSwapchain(WebGLRenderingContextBase&, SwapchainTargets, GCGLenum format, bool clearOnAccess, size_t imageCount);
    void setupExternalImage(GraphicsContextGL&, const PlatformXR::FrameData::LayerSetupData&);

    const WebXRExternalImages* reusableTextures(const PlatformXR::FrameData::ExternalTextureData&) const;
    void releaseTexturesAtIndex(size_t index);
    void bindCompositorTexturesForDisplay(GraphicsContextGL&, PlatformXR::FrameData::LayerData&);
    const WebXRExternalImages* reusableTexturesAtIndex(size_t);

    GCGLenum m_format;
#if USE(OPENXR)
    WTF::UnixFileDescriptor m_fenceFD;
#endif

    Vector<WebXRExternalImages> m_displayImagesSets;
};

// This class represents a swapchain that uses "static" images for display, i.e., images that are not
// shared with the platform's XR compositor. Examples of those are the textures used for depth and stencil
// which do not necesarily need to be shared with the compositor and can be managed by WebXR itself.
class WebXRWebGLStaticImageSwapchain final : public WebXRWebGLSwapchain {
public:
    struct StaticImageAttributes {
        GCGLenum format { 0 };
        GCGLenum internalFormat { 0 };
        IntSize size;
        bool clearOnAccess { false };
        SwapchainTargets targets;
        size_t imageCount { 0 };
    };
    static std::unique_ptr<WebXRWebGLStaticImageSwapchain> create(WebGLRenderingContextBase&, StaticImageAttributes);
    ~WebXRWebGLStaticImageSwapchain() override;

    PlatformGLObject currentTexture() override;

    void startFrame(PlatformXR::FrameData::LayerData&) override;
    void endFrame(PlatformXR::DeviceLayer&) override;

    bool allTexturesAreBound() const override;

private:
    WebXRWebGLStaticImageSwapchain(WebGLRenderingContextBase&, StaticImageAttributes);
    void bindCompositorTexturesForDisplay(GraphicsContextGL&, PlatformXR::FrameData::LayerData&);
    void releaseDisplayImagesAtIndex(size_t);

    StaticImageAttributes m_imageAttributes;
#if USE(OPENXR)
    WTF::UnixFileDescriptor m_fenceFD;
#endif
    Vector<PlatformGLObject> m_textures;
};

} // namespace WebCore

#endif // ENABLE(WEBXR_LAYERS)

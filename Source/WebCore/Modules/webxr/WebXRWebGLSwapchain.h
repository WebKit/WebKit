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
#include <wtf/Function.h>
#include <wtf/Ref.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class IntRect;
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

    virtual IntSize size() const { return m_texSize; }

    virtual void startFrame(PlatformXR::FrameData::LayerData&) = 0;
    virtual void endFrame(PlatformXR::DeviceLayer&) = 0;

    virtual bool allTexturesAreBound() const = 0;

    virtual GCGLenum textureTarget() const { return GraphicsContextGL::TEXTURE_2D; }

    // Per-view texture accessor; only cube layers expose more than one (one cubemap per eye).
    virtual PlatformGLObject currentTextureAtIndex(uint32_t) { return currentTexture(); }

    void clearTextureIfNeeded(const IntRect& viewport, std::optional<GCGLint> slice);

protected:
    WebXRWebGLSwapchain(WebGLRenderingContextBase&, SwapchainTargets, bool clearOnAccess, size_t imageCount);
    virtual void clearTextureRegion(GraphicsContextGL&, const IntRect& viewport, std::optional<GCGLint> slice);

    using BindAttachmentFunction = Function<void(GCGLenum attachment)>;
    void clearAttachmentRegion(GraphicsContextGL&, const IntRect& viewport, NOESCAPE const BindAttachmentFunction&);

    void setupExternalImage(const PlatformXR::FrameData::LayerSetupData&);
    void signalEndFrame(GraphicsContextGL&, PlatformXR::DeviceLayer&);

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
    static std::unique_ptr<WebXRWebGLSharedImageSwapchain> create(WebGLRenderingContextBase&, SwapchainTargets, GCGLenum format, IntSize initialSize, bool clearOnAccess, size_t imageCount);
    ~WebXRWebGLSharedImageSwapchain() override;

    PlatformGLObject currentTexture() override;

    void startFrame(PlatformXR::FrameData::LayerData&) override;
    void endFrame(PlatformXR::DeviceLayer&) override;

    bool allTexturesAreBound() const override;

private:
    WebXRWebGLSharedImageSwapchain(WebGLRenderingContextBase&, SwapchainTargets, GCGLenum format, IntSize initialSize, bool clearOnAccess, size_t imageCount);

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
        uint32_t arrayLength { 1 };
        GCGLenum textureType { GraphicsContextGL::TEXTURE_2D };
    };
    static std::unique_ptr<WebXRWebGLStaticImageSwapchain> create(WebGLRenderingContextBase&, StaticImageAttributes);
    ~WebXRWebGLStaticImageSwapchain() override;

    PlatformGLObject currentTexture() override;
    PlatformGLObject currentTextureAtIndex(uint32_t) override;

    void startFrame(PlatformXR::FrameData::LayerData&) override;
    void endFrame(PlatformXR::DeviceLayer&) override;

    bool allTexturesAreBound() const override;

    GCGLenum textureTarget() const override { return m_imageAttributes.textureType; }

private:
    WebXRWebGLStaticImageSwapchain(WebGLRenderingContextBase&, StaticImageAttributes);
    void bindCompositorTexturesForDisplay(GraphicsContextGL&, PlatformXR::FrameData::LayerData&);
    void releaseDisplayImagesAtIndex(size_t);
    void clearTextureRegion(GraphicsContextGL&, const IntRect& viewport, std::optional<GCGLint> slice) override;

    StaticImageAttributes m_imageAttributes;
#if USE(OPENXR)
    WTF::UnixFileDescriptor m_fenceFD;
#endif
    Vector<PlatformGLObject> m_textures;
};

// Base for swapchains that render into one or more per-image textures and blit them into the corresponding horizontal regions of
// a side-by-side shared 2D texture exported to the UIProcess. Used whenever the textures can't be shared directly (like with DMABuf).
// Requires an extra blit per frame from the per-image textures into the shared texture.
class WebXRWebGLMultiTextureSwapchain : public WebXRWebGLSwapchain {
public:
    ~WebXRWebGLMultiTextureSwapchain() override;

    PlatformGLObject currentTexture() override;
    PlatformGLObject currentTextureAtIndex(uint32_t) override;

    void startFrame(PlatformXR::FrameData::LayerData&) override;
    void endFrame(PlatformXR::DeviceLayer&) override;

    bool allTexturesAreBound() const override;

protected:
    struct TextureSet {
        Vector<PlatformGLObject> renderTextures;
        WebXRExternalImages sharedImage;

        explicit operator bool() const { return !renderTextures.isEmpty() && renderTextures[0] && sharedImage; }
        void release(GraphicsContextGL&);
        void leakObject();
    };

    WebXRWebGLMultiTextureSwapchain(WebGLRenderingContextBase&, SwapchainTargets, GCGLenum internalFormat, bool clearOnAccess, size_t imageCount);

    void releaseTexturesAtIndex(size_t);
    const WebXRExternalImages* reusableTextures(const PlatformXR::FrameData::ExternalTextureData&) const;

    virtual void bindCompositorTexturesForDisplay(GraphicsContextGL&, PlatformXR::FrameData::LayerData&) = 0;
    virtual void blitToSharedImage(GraphicsContextGL&) = 0;

    GCGLenum m_internalFormat;
    Vector<TextureSet> m_textureSets;
    PlatformGLObject m_blitReadFBO { 0 };
    PlatformGLObject m_blitDrawFBO { 0 };
};

// Renders into a GL_TEXTURE_2D_ARRAY and blits each slice into a side-by-side shared texture.
class WebXRWebGLTextureArraySwapchain final : public WebXRWebGLMultiTextureSwapchain {
    WTF_MAKE_TZONE_ALLOCATED(WebXRWebGLTextureArraySwapchain);
    WTF_MAKE_NONCOPYABLE(WebXRWebGLTextureArraySwapchain);
public:
    static std::unique_ptr<WebXRWebGLTextureArraySwapchain> create(WebGLRenderingContextBase&, SwapchainTargets, GCGLenum internalFormat, bool clearOnAccess, size_t imageCount, uint32_t arrayLength);

    GCGLenum textureTarget() const override { return GraphicsContextGL::TEXTURE_2D_ARRAY; }
    uint32_t arrayLength() const { return m_arrayLength; }

private:
    WebXRWebGLTextureArraySwapchain(WebGLRenderingContextBase&, SwapchainTargets, GCGLenum internalFormat, bool clearOnAccess, size_t imageCount, uint32_t arrayLength);

    void clearTextureRegion(GraphicsContextGL&, const IntRect& viewport, std::optional<GCGLint> slice) override;
    void bindCompositorTexturesForDisplay(GraphicsContextGL&, PlatformXR::FrameData::LayerData&) override;
    void blitToSharedImage(GraphicsContextGL&) override;

    uint32_t m_arrayLength;
};

// Renders into one GL_TEXTURE_CUBE_MAP per eye and blits each face into a side-by-side shared texture.
class WebXRWebGLCubeSwapchain final : public WebXRWebGLMultiTextureSwapchain {
    WTF_MAKE_TZONE_ALLOCATED(WebXRWebGLCubeSwapchain);
    WTF_MAKE_NONCOPYABLE(WebXRWebGLCubeSwapchain);
public:
    static constexpr uint32_t faceCount = 6;

    static std::unique_ptr<WebXRWebGLCubeSwapchain> create(WebGLRenderingContextBase&, SwapchainTargets, GCGLenum internalFormat, bool clearOnAccess, size_t imageCount, uint32_t cubeCount);

    GCGLenum textureTarget() const override { return GraphicsContextGL::TEXTURE_CUBE_MAP; }

private:
    WebXRWebGLCubeSwapchain(WebGLRenderingContextBase&, SwapchainTargets, GCGLenum internalFormat, bool clearOnAccess, size_t imageCount, uint32_t cubeCount);

    void clearTextureRegion(GraphicsContextGL&, const IntRect& viewport, std::optional<GCGLint> slice) override;
    void bindCompositorTexturesForDisplay(GraphicsContextGL&, PlatformXR::FrameData::LayerData&) override;
    void blitToSharedImage(GraphicsContextGL&) override;

    uint32_t m_cubeCount;
};

} // namespace WebCore

#endif // ENABLE(WEBXR_LAYERS)

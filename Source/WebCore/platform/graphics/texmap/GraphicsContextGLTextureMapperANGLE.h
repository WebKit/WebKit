/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEBGL) && USE(TEXTURE_MAPPER) && USE(ANGLE)

#include "GraphicsContextGLANGLE.h"

#if USE(TEXTURE_MAPPER_DMABUF)
#include "GBMBufferSwapchain.h"
#include <wtf/HashMap.h>
#endif

#if USE(NICOSIA)
namespace Nicosia {
class GCGLANGLELayer;
class GCGLLayer;
}

struct gbm_device;
struct gbm_bo;

typedef void *EGLImageKHR;
#endif

namespace WebCore {

class TextureMapperGCGLPlatformLayer;

class WEBCORE_EXPORT GraphicsContextGLTextureMapperANGLE : public GraphicsContextGLANGLE {
public:
    static RefPtr<GraphicsContextGLTextureMapperANGLE> create(WebCore::GraphicsContextGLAttributes&&);
    ~GraphicsContextGLTextureMapperANGLE();

    // GraphicsContextGLANGLE overrides.
    RefPtr<GraphicsLayerContentsDisplayDelegate> layerContentsDisplayDelegate() final;
#if ENABLE(VIDEO)
    bool copyTextureFromMedia(MediaPlayer&, PlatformGLObject texture, GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLenum format, GCGLenum type, bool premultiplyAlpha, bool flipY) final;
#endif
#if ENABLE(MEDIA_STREAM)
    RefPtr<VideoFrame> paintCompositedResultsToVideoFrame() final;
#endif

    void setContextVisibility(bool) final;
    bool reshapeDisplayBufferBacking() final;
    void prepareForDisplay() final;

private:
    GraphicsContextGLTextureMapperANGLE(WebCore::GraphicsContextGLAttributes&&);

    bool platformInitializeContext() final;
    bool platformInitialize() final;

    void prepareTexture() final;

    RefPtr<GraphicsLayerContentsDisplayDelegate> m_layerContentsDisplayDelegate;

#if USE(TEXTURE_MAPPER_DMABUF)
    struct Swapchain {
        Swapchain() = default;
        Swapchain(GCGLDisplay);
        ~Swapchain();

        GCGLDisplay platformDisplay { nullptr };
        RefPtr<GBMBufferSwapchain> swapchain;
        RefPtr<GBMBufferSwapchain::Buffer> drawBO;
        RefPtr<GBMBufferSwapchain::Buffer> displayBO;

        // Cache for EGLImage objects corresponding to the buffers originating from the swapchain.
        // The swapchain is regenerated (and these EGLImage objects destroyed) upon each buffer reshaping,
        // so we should be fine without managing stale buffers and corresponding EGLImages (which shouldn't occur anyway).
        HashMap<uint32_t, EGLImageKHR, WTF::DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> images;
    } m_swapchain;
#else
    GCGLuint m_compositorTexture { 0 };
#if USE(COORDINATED_GRAPHICS)
    GCGLuint m_intermediateTexture { 0 };
#endif
#endif

#if USE(NICOSIA)
    std::unique_ptr<Nicosia::GCGLANGLELayer> m_nicosiaLayer;
#else
    std::unique_ptr<TextureMapperGCGLPlatformLayer> m_texmapLayer;
#endif

#if USE(TEXTURE_MAPPER_DMABUF)
    // Required (for now) in GraphicsContextGLANGLE::makeContextCurrent()
    // to construct and set the draw buffer-object.
    friend class GraphicsContextGLANGLE;
#endif

#if USE(NICOSIA)
    friend class Nicosia::GCGLANGLELayer;
    friend class Nicosia::GCGLLayer;
#else
    friend class TextureMapperGCGLPlatformLayer;
#endif
};

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(TEXTURE_MAPPER) && USE(ANGLE)

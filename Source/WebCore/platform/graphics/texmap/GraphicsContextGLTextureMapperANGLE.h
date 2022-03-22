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

#if USE(NICOSIA)
namespace Nicosia {
class GCGLANGLELayer;
class GCGLLayer;
}

struct gbm_device;
struct gbm_bo;

typedef void *EGLImage;
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

    void prepareTextureImpl() final;

    RefPtr<GraphicsLayerContentsDisplayDelegate> m_layerContentsDisplayDelegate;

    GCGLuint m_compositorTexture { 0 };
#if USE(COORDINATED_GRAPHICS)
    GCGLuint m_intermediateTexture { 0 };
#endif

#if USE(NICOSIA)
    std::unique_ptr<Nicosia::GCGLANGLELayer> m_nicosiaLayer;

    class EGLImageBacking {
    WTF_MAKE_FAST_ALLOCATED;
    public:
        EGLImageBacking(GCGLDisplay);
        ~EGLImageBacking();

        bool reset(int width, int height, bool hasAlpha);

        EGLImage image() const { return m_image; }
        int fd() const { return m_FD; }

        uint32_t format() const;
        uint32_t stride() const;

        bool isReleased();
    private:
        void releaseResources();

        GCGLDisplay m_display;

        gbm_bo* m_BO { nullptr };
        int m_FD { -1 };
        EGLImage m_image;
    };

    std::unique_ptr<EGLImageBacking> m_textureBacking;
    std::unique_ptr<EGLImageBacking> m_compositorTextureBacking;
    std::unique_ptr<EGLImageBacking> m_intermediateTextureBacking;
#else
    std::unique_ptr<TextureMapperGCGLPlatformLayer> m_texmapLayer;
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

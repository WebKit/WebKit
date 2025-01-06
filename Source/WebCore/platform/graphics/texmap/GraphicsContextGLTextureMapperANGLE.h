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

#if ENABLE(WEBGL) && USE(TEXTURE_MAPPER)

#include "GLContextWrapper.h"
#include "GraphicsContextGLANGLE.h"

namespace WebCore {

#if PLATFORM(GTK) || PLATFORM(WPE)
class GLFence;
#endif

class TextureMapperGCGLPlatformLayer;

class GraphicsContextGLTextureMapperANGLE : public GLContextWrapper, public GraphicsContextGLANGLE {
public:
    WEBCORE_EXPORT static RefPtr<GraphicsContextGLTextureMapperANGLE> create(WebCore::GraphicsContextGLAttributes&&);
    virtual ~GraphicsContextGLTextureMapperANGLE();

    // GraphicsContextGLANGLE overrides.
    WEBCORE_EXPORT RefPtr<GraphicsLayerContentsDisplayDelegate> layerContentsDisplayDelegate() final;
#if ENABLE(VIDEO)
    bool copyTextureFromVideoFrame(VideoFrame&, PlatformGLObject texture, GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLenum format, GCGLenum type, bool premultiplyAlpha, bool flipY) final;
#endif
#if ENABLE(MEDIA_STREAM) || ENABLE(WEB_CODECS)
    RefPtr<VideoFrame> surfaceBufferToVideoFrame(SurfaceBuffer) final;
#endif
    RefPtr<PixelBuffer> readCompositedResults() final;

    bool reshapeDrawingBuffer() override;
    void prepareForDisplay() override;
#if ENABLE(WEBXR)
    bool addFoveation(IntSize, IntSize, IntSize, std::span<const GCGLfloat>, std::span<const GCGLfloat>, std::span<const GCGLfloat>) final;
    void enableFoveation(GCGLuint) final;
    void disableFoveation() final;
#endif

protected:
    explicit GraphicsContextGLTextureMapperANGLE(WebCore::GraphicsContextGLAttributes&&);

    RefPtr<GraphicsLayerContentsDisplayDelegate> m_layerContentsDisplayDelegate;

private:
    bool platformInitializeContext() final;
    bool platformInitialize() override;

    void swapCompositorTexture();

#if USE(COORDINATED_GRAPHICS) && USE(LIBEPOXY)
    GCGLuint setupCurrentTexture();
#endif

    // GLContextWrapper
    GLContextWrapper::Type type() const override;
    bool makeCurrentImpl() override;
    bool unmakeCurrentImpl() override;

    GCGLuint m_compositorTexture { 0 };
    bool m_isCompositorTextureInitialized { false };

#if PLATFORM(GTK) || PLATFORM(WPE)
    std::unique_ptr<GLFence> m_frameFence;
#endif

#if USE(COORDINATED_GRAPHICS) && USE(LIBEPOXY)
    GCGLuint m_textureID { 0 };
    GCGLuint m_compositorTextureID { 0 };
#endif

#if !USE(COORDINATED_GRAPHICS)
    std::unique_ptr<TextureMapperGCGLPlatformLayer> m_texmapLayer;

    friend class TextureMapperGCGLPlatformLayer;
#endif
};

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(TEXTURE_MAPPER)

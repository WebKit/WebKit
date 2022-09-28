/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 * Copyright (C) 2022 Metrological Group B.V.
 * Copyright (C) 2022 Igalia S.L.
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

#include "GraphicsContextGLANGLE.h"

namespace Nicosia {
class GCGLANGLELayer;
}

namespace WebCore {

class TextureMapperGCGLPlatformLayer;

class GraphicsContextGLFallback : public GraphicsContextGLANGLE {
public:
    static RefPtr<GraphicsContextGLFallback> create(WebCore::GraphicsContextGLAttributes&&);
    virtual ~GraphicsContextGLFallback();

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

    GCGLuint getInternalColorFormat() const { return m_internalColorFormat; }

private:
    GraphicsContextGLFallback(WebCore::GraphicsContextGLAttributes&&);

    bool platformInitializeContext() final;
    bool platformInitialize() final;

    void prepareTexture() final;

    std::unique_ptr<Nicosia::GCGLANGLELayer> m_nicosiaLayer;
    RefPtr<GraphicsLayerContentsDisplayDelegate> m_layerContentsDisplayDelegate;

    friend class Nicosia::GCGLANGLELayer;
};

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(TEXTURE_MAPPER)

/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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

#include "config.h"
#include "GraphicsContextGLTextureMapper.h"

#if ENABLE(WEBGL) && USE(TEXTURE_MAPPER)

#include "PixelBuffer.h"
#include "TextureMapperGCGLPlatformLayer.h"
#include <wtf/Deque.h>
#include <wtf/NeverDestroyed.h>

#if USE(ANGLE)
#include "ANGLEHeaders.h"
#elif USE(LIBEPOXY)
#include <epoxy/gl.h>
#elif !USE(OPENGL_ES)
#include "GLContext.h"
#include "OpenGLShims.h"
#endif

#if !USE(ANGLE)
#include <ANGLE/ShaderLang.h>
#endif

#if USE(NICOSIA)
#if USE(ANGLE)
#include "NicosiaGCGLANGLELayer.h"
#else
#include "NicosiaGCGLLayer.h"
#include "PlatformLayerDisplayDelegate.h"
#endif
#endif

#if ENABLE(MEDIA_STREAM)
#include "MediaSample.h"
#endif

#if USE(GSTREAMER) && ENABLE(MEDIA_STREAM)
#include "VideoFrameGStreamer.h"
#endif

#if ENABLE(VIDEO)
#include "MediaPlayerPrivate.h"
#endif

namespace WebCore {

RefPtr<GraphicsContextGLTextureMapper> GraphicsContextGLTextureMapper::create(GraphicsContextGLAttributes&& attributes)
{
    auto context = adoptRef(*new GraphicsContextGLTextureMapper(WTFMove(attributes)));
    if (!context->initialize())
        return nullptr;
    return context;
}

GraphicsContextGLTextureMapper::~GraphicsContextGLTextureMapper() = default;

GraphicsContextGLTextureMapper::GraphicsContextGLTextureMapper(GraphicsContextGLAttributes&& attributes)
    : GraphicsContextGLTextureMapperBase(WTFMove(attributes))
{
}

RefPtr<GraphicsLayerContentsDisplayDelegate> GraphicsContextGLTextureMapper::layerContentsDisplayDelegate()
{
    return m_layerContentsDisplayDelegate;
}

RefPtr<GraphicsContextGL> createWebProcessGraphicsContextGL(const GraphicsContextGLAttributes& attributes)
{
    static bool initialized = false;
    static bool success = true;
    if (!initialized) {
#if !USE(OPENGL_ES) && !USE(LIBEPOXY) && !USE(ANGLE)
        success = initializeOpenGLShims();
#endif
        initialized = true;
    }
    if (!success)
        return nullptr;

    // Create the GraphicsContextGLOpenGL object first in order to establist a current context on this thread.
    auto context = GraphicsContextGLTextureMapper::create(GraphicsContextGLAttributes { attributes });
    if (!context)
        return nullptr;
#if USE(LIBEPOXY) && USE(OPENGL_ES) && ENABLE(WEBGL2)
    // Bail if GLES3 was requested but cannot be provided.
    if (attributes.webGLVersion == GraphicsContextGLWebGLVersion::WebGL2 && !epoxy_is_desktop_gl() && epoxy_gl_version() < 30)
        return nullptr;
#endif
    return context;
}

#if ENABLE(MEDIA_STREAM)
RefPtr<MediaSample> GraphicsContextGLTextureMapper::paintCompositedResultsToMediaSample()
{
#if USE(GSTREAMER)
    if (auto pixelBuffer = readCompositedResults())
        return VideoFrameGStreamer::createFromPixelBuffer(WTFMove(*pixelBuffer));
#endif
    return nullptr;
}
#endif

#if ENABLE(VIDEO)
bool GraphicsContextGLTextureMapper::copyTextureFromMedia(MediaPlayer& player, PlatformGLObject outputTexture, GCGLenum outputTarget, GCGLint level, GCGLenum internalFormat, GCGLenum format, GCGLenum type, bool premultiplyAlpha, bool flipY)
{
#if USE(ANGLE) && USE(GSTREAMER_GL)
    UNUSED_PARAM(player);
    UNUSED_PARAM(outputTexture);
    UNUSED_PARAM(outputTarget);
    UNUSED_PARAM(level);
    UNUSED_PARAM(internalFormat);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    UNUSED_PARAM(premultiplyAlpha);
    UNUSED_PARAM(flipY);

    // FIXME: Implement copy-free (or at least, software copy-free) texture transfer via dmabuf.
    return false;
#else
    return player.copyVideoTextureToPlatformTexture(this, outputTexture, outputTarget, level, internalFormat, format, type, premultiplyAlpha, flipY);
#endif
}
#endif

#if USE(NICOSIA) && !USE(ANGLE)
bool GraphicsContextGLTextureMapper::platformInitialize()
{
    m_layerContentsDisplayDelegate = PlatformLayerDisplayDelegate::create(&m_nicosiaLayer->contentLayer());

    return true;
}
#endif

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(TEXTURE_MAPPER)

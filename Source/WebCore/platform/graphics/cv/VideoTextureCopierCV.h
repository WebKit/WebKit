/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#import "GraphicsContextGLOpenGL.h"
#import <wtf/UnsafePointer.h>

typedef struct __CVBuffer* CVImageBufferRef;
typedef struct __CVBuffer* CVPixelBufferRef;
typedef CVImageBufferRef CVOpenGLTextureRef;
typedef CVImageBufferRef CVOpenGLESTextureRef;

namespace WebCore {

class TextureCacheCV;

class VideoTextureCopierCV {
    WTF_MAKE_FAST_ALLOCATED;
public:
    VideoTextureCopierCV(GraphicsContextGLOpenGL&);
    ~VideoTextureCopierCV();

#if PLATFORM(IOS_FAMILY)
    typedef CVOpenGLESTextureRef TextureType;
#else
    typedef CVOpenGLTextureRef TextureType;
#endif

    bool copyImageToPlatformTexture(CVPixelBufferRef, size_t width, size_t height, PlatformGLObject outputTexture, GCGLenum outputTarget, GCGLint level, GCGLenum internalFormat, GCGLenum format, GCGLenum type, bool premultiplyAlpha, bool flipY);
    bool copyVideoTextureToPlatformTexture(TextureType, size_t width, size_t height, PlatformGLObject outputTexture, GCGLenum outputTarget, GCGLint level, GCGLenum internalFormat, GCGLenum format, GCGLenum type, bool premultiplyAlpha, bool flipY, bool swapColorChannels = false);

    GraphicsContextGLOpenGL& context() { return m_context; }

private:
    bool copyVideoTextureToPlatformTexture(PlatformGLObject inputTexture, GCGLenum inputTarget, size_t width, size_t height, PlatformGLObject outputTexture, GCGLenum outputTarget, GCGLint level, GCGLenum internalFormat, GCGLenum format, GCGLenum type, bool premultiplyAlpha, bool flipY, bool swapColorChannels);

    bool initializeContextObjects();
    bool initializeUVContextObjects();

    unsigned lastTextureSeed(GCGLuint texture)
    {
        return m_lastTextureSeed.get(texture);
    }

#if USE(ANGLE)
    // Returns a handle which, if non-null, must be released via the
    // detach call below.
    void* attachIOSurfaceToTexture(GCGLenum target, GCGLenum internalFormat, GCGLsizei width, GCGLsizei height, GCGLenum type, IOSurfaceRef, GCGLuint plane);
    void detachIOSurfaceFromTexture(void* handle);
#endif

    Ref<GraphicsContextGLOpenGL> m_sharedContext;
    Ref<GraphicsContextGLOpenGL> m_context;
#if !USE(ANGLE)
    std::unique_ptr<TextureCacheCV> m_textureCache;
#endif
    PlatformGLObject m_framebuffer { 0 };
    PlatformGLObject m_program { 0 };
    PlatformGLObject m_vertexBuffer { 0 };
    GCGLint m_textureUniformLocation { -1 };
    GCGLint m_textureDimensionsUniformLocation { -1 };
    GCGLint m_flipYUniformLocation { -1 };
    GCGLint m_swapColorChannelsUniformLocation { -1 };
    GCGLint m_premultiplyUniformLocation { -1 };
    GCGLint m_positionAttributeLocation { -1 };
    PlatformGLObject m_yuvProgram { 0 };
    PlatformGLObject m_yuvVertexBuffer { 0 };
    GCGLint m_yTextureUniformLocation { -1 };
    GCGLint m_uvTextureUniformLocation { -1 };
    GCGLint m_yuvFlipYUniformLocation { -1 };
    GCGLint m_colorMatrixUniformLocation { -1 };
    GCGLint m_yuvPositionAttributeLocation { -1 };
    GCGLint m_yTextureSizeUniformLocation { -1 };
    GCGLint m_uvTextureSizeUniformLocation { -1 };

    bool m_lastFlipY { false };
    UnsafePointer<IOSurfaceRef> m_lastSurface;
    uint32_t m_lastSurfaceSeed { 0 };

    using TextureSeedMap = HashMap<GCGLuint, unsigned, WTF::IntHash<GCGLuint>, WTF::UnsignedWithZeroKeyHashTraits<GCGLuint>>;
    TextureSeedMap m_lastTextureSeed;
};

}

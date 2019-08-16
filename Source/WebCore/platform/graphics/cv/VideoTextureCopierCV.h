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

#if HAVE(CORE_VIDEO)

#import "GraphicsContext3D.h"
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
    VideoTextureCopierCV(GraphicsContext3D&);
    ~VideoTextureCopierCV();

#if PLATFORM(IOS_FAMILY)
    typedef CVOpenGLESTextureRef TextureType;
#else
    typedef CVOpenGLTextureRef TextureType;
#endif

    bool copyImageToPlatformTexture(CVPixelBufferRef, size_t width, size_t height, Platform3DObject outputTexture, GC3Denum outputTarget, GC3Dint level, GC3Denum internalFormat, GC3Denum format, GC3Denum type, bool premultiplyAlpha, bool flipY);
    bool copyVideoTextureToPlatformTexture(TextureType, size_t width, size_t height, Platform3DObject outputTexture, GC3Denum outputTarget, GC3Dint level, GC3Denum internalFormat, GC3Denum format, GC3Denum type, bool premultiplyAlpha, bool flipY, bool swapColorChannels = false);

    GraphicsContext3D& context() { return m_context; }

private:
    bool copyVideoTextureToPlatformTexture(Platform3DObject inputTexture, GC3Denum inputTarget, size_t width, size_t height, Platform3DObject outputTexture, GC3Denum outputTarget, GC3Dint level, GC3Denum internalFormat, GC3Denum format, GC3Denum type, bool premultiplyAlpha, bool flipY, bool swapColorChannels);

    bool initializeContextObjects();
    bool initializeUVContextObjects();

#if HAVE(IOSURFACE)
    unsigned lastTextureSeed(GC3Duint texture)
    {
        auto iterator = m_lastTextureSeed.find(texture);
        return iterator == m_lastTextureSeed.end() ? 0 : iterator->value;
    }
#endif

    Ref<GraphicsContext3D> m_sharedContext;
    Ref<GraphicsContext3D> m_context;
    std::unique_ptr<TextureCacheCV> m_textureCache;
    Platform3DObject m_framebuffer { 0 };
    Platform3DObject m_program { 0 };
    Platform3DObject m_vertexBuffer { 0 };
    GC3Dint m_textureUniformLocation { -1 };
    GC3Dint m_textureDimensionsUniformLocation { -1 };
    GC3Dint m_flipYUniformLocation { -1 };
    GC3Dint m_swapColorChannelsUniformLocation { -1 };
    GC3Dint m_premultiplyUniformLocation { -1 };
    GC3Dint m_positionAttributeLocation { -1 };
    Platform3DObject m_yuvProgram { 0 };
    Platform3DObject m_yuvVertexBuffer { 0 };
    GC3Dint m_yTextureUniformLocation { -1 };
    GC3Dint m_uvTextureUniformLocation { -1 };
    GC3Dint m_yuvFlipYUniformLocation { -1 };
    GC3Dint m_colorMatrixUniformLocation { -1 };
    GC3Dint m_yuvPositionAttributeLocation { -1 };
    GC3Dint m_yTextureSizeUniformLocation { -1 };
    GC3Dint m_uvTextureSizeUniformLocation { -1 };

#if HAVE(IOSURFACE)
    bool m_lastFlipY { false };
    UnsafePointer<IOSurfaceRef> m_lastSurface;
    uint32_t m_lastSurfaceSeed { 0 };

    using TextureSeedMap = HashMap<GC3Duint, unsigned, WTF::IntHash<GC3Duint>, WTF::UnsignedWithZeroKeyHashTraits<GC3Duint>>;
    TextureSeedMap m_lastTextureSeed;
#endif
};

}

#endif // HAVE(CORE_VIDEO)

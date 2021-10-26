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

#if ENABLE(WEBGL) && ENABLE(VIDEO) && USE(AVFOUNDATION)

#include "GraphicsContextGLCV.h"

#include <memory>
#include <wtf/UnsafePointer.h>

namespace WebCore {
class GraphicsContextGLOpenGL;

// GraphicsContextGLCV implementation for ANGLE flavour of GraphicsContextGLOpenGL.
// This class is part of the internal implementation of GraphicsContextGLOpenGL for ANGLE Cocoa.
class GraphicsContextGLCVANGLE final : public GraphicsContextGLCV {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<GraphicsContextGLCVANGLE> create(GraphicsContextGLOpenGL&);

    ~GraphicsContextGLCVANGLE() final;

    bool copyPixelBufferToTexture(CVPixelBufferRef, PlatformGLObject outputTexture, GCGLint level, GCGLenum internalFormat, GCGLenum format, GCGLenum type, FlipY) final;

private:
    GraphicsContextGLCVANGLE(GraphicsContextGLOpenGL&);

    unsigned lastTextureSeed(GCGLuint texture)
    {
        return m_lastTextureSeed.get(texture);
    }

    GraphicsContextGLOpenGL& m_owner;
    PlatformGraphicsContextGLDisplay m_display { nullptr };
    PlatformGraphicsContextGL m_context { nullptr };
    PlatformGraphicsContextGLConfig m_config { nullptr };

    PlatformGLObject m_framebuffer { 0 };
    PlatformGLObject m_yuvVertexBuffer { 0 };
    GCGLint m_yTextureUniformLocation { -1 };
    GCGLint m_uvTextureUniformLocation { -1 };
    GCGLint m_yuvFlipYUniformLocation { -1 };
    GCGLint m_colorMatrixUniformLocation { -1 };
    GCGLint m_yuvPositionAttributeLocation { -1 };
    GCGLint m_yTextureSizeUniformLocation { -1 };
    GCGLint m_uvTextureSizeUniformLocation { -1 };

    FlipY m_lastFlipY { FlipY::No };
    UnsafePointer<IOSurfaceRef> m_lastSurface;
    uint32_t m_lastSurfaceSeed { 0 };

    using TextureSeedMap = HashMap<GCGLuint, unsigned, IntHash<GCGLuint>, WTF::UnsignedWithZeroKeyHashTraits<GCGLuint>>;
    TextureSeedMap m_lastTextureSeed;
};

}

#endif

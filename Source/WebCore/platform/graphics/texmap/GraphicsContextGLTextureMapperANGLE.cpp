/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 * Copyright (C) 2019 Google Inc. All rights reserved.
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

#if ENABLE(WEBGL) && USE(ANGLE) && USE(TEXTURE_MAPPER)

#include "ANGLEHeaders.h"
#include "ANGLEUtilities.h"

#if USE(NICOSIA)
#include "NicosiaGCGLANGLELayer.h"
#else
#include "TextureMapperGCGLPlatformLayer.h"
#endif

namespace WebCore {

PlatformGraphicsContextGLDisplay GraphicsContextGLOpenGL::platformDisplay() const
{
#if USE(NICOSIA)
    return m_nicosiaLayer->platformDisplay();
#else
    return m_texmapLayer->platformDisplay();
#endif
}

PlatformGraphicsContextGLConfig GraphicsContextGLOpenGL::platformConfig() const
{
#if USE(NICOSIA)
    return m_nicosiaLayer->platformConfig();
#else
    return m_texmapLayer->platformConfig();
#endif
}

bool GraphicsContextGLOpenGL::reshapeDisplayBufferBacking()
{
    auto attrs = contextAttributes();
    const auto size = getInternalFramebufferSize();
    const int width = size.width();
    const int height = size.height();
    GLuint colorFormat = attrs.alpha ? GL_RGBA : GL_RGB;
    GLenum textureTarget = drawingBufferTextureTarget();
    GLuint internalColorFormat = textureTarget == GL_TEXTURE_2D ? colorFormat : m_internalColorFormat;
    ScopedRestoreTextureBinding restoreBinding(drawingBufferTextureTargetQueryForDrawingTarget(textureTarget), textureTarget, textureTarget != TEXTURE_RECTANGLE_ARB);
#if USE(COORDINATED_GRAPHICS)
    if (m_compositorTexture) {
        gl::BindTexture(textureTarget, m_compositorTexture);
        gl::TexImage2D(textureTarget, 0, internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
        gl::BindTexture(textureTarget, m_intermediateTexture);
        gl::TexImage2D(textureTarget, 0, internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
    }
#endif
    gl::BindTexture(textureTarget, m_texture);
    gl::TexImage2D(textureTarget, 0, internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
    gl::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureTarget, m_texture, 0);
    return true;
}

void GraphicsContextGLOpenGL::platformReleaseThreadResources()
{
}

}

#endif

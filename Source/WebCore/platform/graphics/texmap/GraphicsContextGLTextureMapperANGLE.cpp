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
#include "ExtensionsGLANGLE.h"
#include "GraphicsContextGLOpenGLManager.h"
#include "PixelBuffer.h"

#if USE(NICOSIA)
#include "NicosiaGCGLANGLELayer.h"
#else
#include "TextureMapperGCGLPlatformLayer.h"
#endif

namespace WebCore {

GraphicsContextGLANGLE::GraphicsContextGLANGLE(GraphicsContextGLAttributes attributes)
    : GraphicsContextGL(attributes)
{
#if ENABLE(WEBGL2)
    m_isForWebGL2 = attributes.webGLVersion == GraphicsContextGLWebGLVersion::WebGL2;
#endif
#if USE(NICOSIA)
    m_nicosiaLayer = makeUnique<Nicosia::GCGLANGLELayer>(*this);
#else
    m_texmapLayer = makeUnique<TextureMapperGCGLPlatformLayer>(*this);
#endif
    bool success = makeContextCurrent();
    ASSERT_UNUSED(success, success);

    validateAttributes();
    attributes = contextAttributes(); // They may have changed during validation.

    GLenum textureTarget = drawingBufferTextureTarget();
    // Create a texture to render into.
    gl::GenTextures(1, &m_texture);
    gl::BindTexture(textureTarget, m_texture);
    gl::TexParameterf(textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl::TexParameterf(textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl::TexParameteri(textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::BindTexture(textureTarget, 0);

    // Create an FBO.
    gl::GenFramebuffers(1, &m_fbo);
    gl::BindFramebuffer(GL_FRAMEBUFFER, m_fbo);

#if USE(COORDINATED_GRAPHICS)
    gl::GenTextures(1, &m_compositorTexture);
    gl::BindTexture(textureTarget, m_compositorTexture);
    gl::TexParameterf(textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl::TexParameterf(textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl::TexParameteri(textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gl::GenTextures(1, &m_intermediateTexture);
    gl::BindTexture(textureTarget, m_intermediateTexture);
    gl::TexParameterf(textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl::TexParameterf(textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl::TexParameteri(textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gl::BindTexture(textureTarget, 0);
#endif

    // Create a multisample FBO.
    ASSERT(m_state.boundReadFBO == m_state.boundDrawFBO);
    if (attributes.antialias) {
        gl::GenFramebuffers(1, &m_multisampleFBO);
        gl::BindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
        m_state.boundDrawFBO = m_state.boundReadFBO = m_multisampleFBO;
        gl::GenRenderbuffers(1, &m_multisampleColorBuffer);
        if (attributes.stencil || attributes.depth)
            gl::GenRenderbuffers(1, &m_multisampleDepthStencilBuffer);
    } else {
        // Bind canvas FBO.
        gl::BindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        m_state.boundDrawFBO = m_state.boundReadFBO = m_fbo;
        if (attributes.stencil || attributes.depth)
            gl::GenRenderbuffers(1, &m_depthStencilBuffer);
    }

    gl::ClearColor(0, 0, 0, 0);
}

GraphicsContextGLANGLE::~GraphicsContextGLANGLE()
{
    GraphicsContextGLOpenGLManager::sharedManager().removeContext(this);
    bool success = makeContextCurrent();
    ASSERT_UNUSED(success, success);
    if (m_texture)
        gl::DeleteTextures(1, &m_texture);
#if USE(COORDINATED_GRAPHICS)
    if (m_compositorTexture)
        gl::DeleteTextures(1, &m_compositorTexture);
#endif

    auto attributes = contextAttributes();

    if (attributes.antialias) {
        gl::DeleteRenderbuffers(1, &m_multisampleColorBuffer);
        if (attributes.stencil || attributes.depth)
            gl::DeleteRenderbuffers(1, &m_multisampleDepthStencilBuffer);
        gl::DeleteFramebuffers(1, &m_multisampleFBO);
    } else if (attributes.stencil || attributes.depth) {
        if (m_depthStencilBuffer)
            gl::DeleteRenderbuffers(1, &m_depthStencilBuffer);
    }
    gl::DeleteFramebuffers(1, &m_fbo);
#if USE(COORDINATED_GRAPHICS)
    gl::DeleteTextures(1, &m_intermediateTexture);
#endif
}

PlatformGraphicsContextGLDisplay GraphicsContextGLANGLE::platformDisplay() const
{
#if USE(NICOSIA)
    return m_nicosiaLayer->platformDisplay();
#else
    return m_texmapLayer->platformDisplay();
#endif
}

PlatformGraphicsContextGLConfig GraphicsContextGLANGLE::platformConfig() const
{
#if USE(NICOSIA)
    return m_nicosiaLayer->platformConfig();
#else
    return m_texmapLayer->platformConfig();
#endif
}

bool GraphicsContextGLANGLE::makeContextCurrent()
{
#if USE(NICOSIA)
    return m_nicosiaLayer->makeContextCurrent();
#else
    return m_texmapLayer->makeContextCurrent();
#endif
}

void GraphicsContextGLANGLE::checkGPUStatus()
{
}

void GraphicsContextGLTextureMapper::setContextVisibility(bool)
{
}

void GraphicsContextGLANGLE::prepareForDisplay()
{
}

bool GraphicsContextGLANGLE::reshapeDisplayBufferBacking()
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

void GraphicsContextGLANGLE::platformReleaseThreadResources()
{
}

std::optional<PixelBuffer> GraphicsContextGLANGLE::readCompositedResults()
{
    return readRenderingResults();
}

}

#endif

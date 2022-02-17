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
#include "GraphicsContextGLOpenGLManager.h"
#include "PixelBuffer.h"

#if USE(NICOSIA)
#include "GBMDevice.h"
#include "NicosiaGCGLANGLELayer.h"

#include <fcntl.h>
#include <gbm.h>
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

    const auto& gbmDevice = GBMDevice::get();
    if (gbmDevice.device()) {
        m_textureBacking = makeUnique<EGLImageBacking>(platformDisplay());
        m_compositorTextureBacking = makeUnique<EGLImageBacking>(platformDisplay());
        m_intermediateTextureBacking = makeUnique<EGLImageBacking>(platformDisplay());
    }
#else
    m_texmapLayer = makeUnique<TextureMapperGCGLPlatformLayer>(*this);
#endif
    bool success = makeContextCurrent();
    ASSERT_UNUSED(success, success);
    success = initialize();
    ASSERT_UNUSED(success, success);

    // We require this extension to render into the dmabuf-backed EGLImage.
    RELEASE_ASSERT(supportsExtension("GL_OES_EGL_image"));
    GL_RequestExtensionANGLE("GL_OES_EGL_image");

    validateAttributes();
    attributes = contextAttributes(); // They may have changed during validation.

    GLenum textureTarget = drawingBufferTextureTarget();
    // Create a texture to render into.
    GL_GenTextures(1, &m_texture);
    GL_BindTexture(textureTarget, m_texture);
    GL_TexParameterf(textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GL_TexParameterf(textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GL_TexParameteri(textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL_TexParameteri(textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GL_BindTexture(textureTarget, 0);

    // Create an FBO.
    GL_GenFramebuffers(1, &m_fbo);
    GL_BindFramebuffer(GL_FRAMEBUFFER, m_fbo);

#if USE(COORDINATED_GRAPHICS)
    GL_GenTextures(1, &m_compositorTexture);
    GL_BindTexture(textureTarget, m_compositorTexture);
    GL_TexParameterf(textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GL_TexParameterf(textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GL_TexParameteri(textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL_TexParameteri(textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GL_GenTextures(1, &m_intermediateTexture);
    GL_BindTexture(textureTarget, m_intermediateTexture);
    GL_TexParameterf(textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GL_TexParameterf(textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GL_TexParameteri(textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL_TexParameteri(textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GL_BindTexture(textureTarget, 0);
#endif

    // Create a multisample FBO.
    ASSERT(m_state.boundReadFBO == m_state.boundDrawFBO);
    if (attributes.antialias) {
        GL_GenFramebuffers(1, &m_multisampleFBO);
        GL_BindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
        m_state.boundDrawFBO = m_state.boundReadFBO = m_multisampleFBO;
        GL_GenRenderbuffers(1, &m_multisampleColorBuffer);
        if (attributes.stencil || attributes.depth)
            GL_GenRenderbuffers(1, &m_multisampleDepthStencilBuffer);
    } else {
        // Bind canvas FBO.
        GL_BindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        m_state.boundDrawFBO = m_state.boundReadFBO = m_fbo;
        if (attributes.stencil || attributes.depth)
            GL_GenRenderbuffers(1, &m_depthStencilBuffer);
    }

    GL_ClearColor(0, 0, 0, 0);
}

#if USE(NICOSIA)
GraphicsContextGLANGLE::EGLImageBacking::EGLImageBacking(GCGLDisplay display)
    : m_display(display)
    , m_image(EGL_NO_IMAGE)
{
}

GraphicsContextGLANGLE::EGLImageBacking::~EGLImageBacking()
{
    releaseResources();
}

uint32_t GraphicsContextGLANGLE::EGLImageBacking::format() const
{
    if (m_BO)
        return gbm_bo_get_format(m_BO);
    return 0;
}

uint32_t GraphicsContextGLANGLE::EGLImageBacking::stride() const
{
    if (m_BO)
        return gbm_bo_get_stride(m_BO);
    return 0;
}

void GraphicsContextGLANGLE::EGLImageBacking::releaseResources()
{
    if (m_BO) {
        gbm_bo_destroy(m_BO);
        m_BO = nullptr;
    }
    if (m_image) {
        EGL_DestroyImageKHR(m_display, m_image);
        m_image = EGL_NO_IMAGE;
    }
    if (m_FD >= 0) {
        close(m_FD);
        m_FD = -1;
    }
}

bool GraphicsContextGLANGLE::EGLImageBacking::isReleased()
{
    return !m_BO;
}

bool GraphicsContextGLANGLE::EGLImageBacking::reset(int width, int height, bool hasAlpha)
{
    releaseResources();

    if (!width || !height)
        return false;

    const auto& gbmDevice = GBMDevice::get();
    m_BO = gbm_bo_create(gbmDevice.device(), width, height, hasAlpha ? GBM_BO_FORMAT_ARGB8888 : GBM_BO_FORMAT_XRGB8888, GBM_BO_USE_RENDERING);
    if (m_BO) {
        m_FD = gbm_bo_get_fd(m_BO);
        if (m_FD >= 0) {
            EGLint imageAttributes[] = {
                EGL_WIDTH, width,
                EGL_HEIGHT, height,
                EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLint>(gbm_bo_get_format(m_BO)),
                EGL_DMA_BUF_PLANE0_FD_EXT, m_FD,
                EGL_DMA_BUF_PLANE0_PITCH_EXT, static_cast<EGLint>(gbm_bo_get_stride(m_BO)),
                EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
                EGL_NONE
            };
            m_image = EGL_CreateImageKHR(m_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer)nullptr, imageAttributes);
            if (m_image)
                return true;
        }
    }

    releaseResources();
    return false;
}
#endif

GraphicsContextGLANGLE::~GraphicsContextGLANGLE()
{
    GraphicsContextGLOpenGLManager::sharedManager().removeContext(this);
    bool success = makeContextCurrent();
    ASSERT_UNUSED(success, success);
    if (m_texture)
        GL_DeleteTextures(1, &m_texture);
#if USE(COORDINATED_GRAPHICS)
    if (m_compositorTexture)
        GL_DeleteTextures(1, &m_compositorTexture);
#endif

    auto attributes = contextAttributes();

    if (attributes.antialias) {
        GL_DeleteRenderbuffers(1, &m_multisampleColorBuffer);
        if (attributes.stencil || attributes.depth)
            GL_DeleteRenderbuffers(1, &m_multisampleDepthStencilBuffer);
        GL_DeleteFramebuffers(1, &m_multisampleFBO);
    } else if (attributes.stencil || attributes.depth) {
        if (m_depthStencilBuffer)
            GL_DeleteRenderbuffers(1, &m_depthStencilBuffer);
    }
    GL_DeleteFramebuffers(1, &m_fbo);
#if USE(COORDINATED_GRAPHICS)
    GL_DeleteTextures(1, &m_intermediateTexture);
#endif
}

GCGLDisplay GraphicsContextGLANGLE::platformDisplay() const
{
#if USE(NICOSIA)
    return m_nicosiaLayer->platformDisplay();
#else
    return m_texmapLayer->platformDisplay();
#endif
}

GCGLConfig GraphicsContextGLANGLE::platformConfig() const
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

void GraphicsContextGLTextureMapper::prepareForDisplay()
{
}

bool GraphicsContextGLTextureMapper::reshapeDisplayBufferBacking()
{
    auto attrs = contextAttributes();
    const auto size = getInternalFramebufferSize();
    const int width = size.width();
    const int height = size.height();
    GLuint colorFormat = attrs.alpha ? GL_RGBA : GL_RGB;
    GLenum textureTarget = drawingBufferTextureTarget();
    GLuint internalColorFormat = textureTarget == GL_TEXTURE_2D ? colorFormat : m_internalColorFormat;

#if USE(COORDINATED_GRAPHICS)
    if (m_textureBacking)
        m_textureBacking->reset(width, height, attrs.alpha);
    if (m_compositorTextureBacking)
        m_compositorTextureBacking->reset(width, height, attrs.alpha);
    if (m_intermediateTextureBacking)
        m_intermediateTextureBacking->reset(width, height, attrs.alpha);
#endif

    ScopedRestoreTextureBinding restoreBinding(drawingBufferTextureTargetQueryForDrawingTarget(textureTarget), textureTarget, textureTarget != TEXTURE_RECTANGLE_ARB);
#if USE(COORDINATED_GRAPHICS)
    if (m_compositorTexture) {
        GL_BindTexture(textureTarget, m_compositorTexture);
        if (m_compositorTextureBacking && m_compositorTextureBacking->image())
            GL_EGLImageTargetTexture2DOES(textureTarget, m_compositorTextureBacking->image());
        else
            GL_TexImage2D(textureTarget, 0, internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
        GL_BindTexture(textureTarget, m_intermediateTexture);
        if (m_intermediateTextureBacking && m_intermediateTextureBacking->image())
            GL_EGLImageTargetTexture2DOES(textureTarget, m_intermediateTextureBacking->image());
        else
            GL_TexImage2D(textureTarget, 0, internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
    }
#endif
    GL_BindTexture(textureTarget, m_texture);
#if USE(COORDINATED_GRAPHICS)
    if (m_textureBacking && m_textureBacking->image())
        GL_EGLImageTargetTexture2DOES(textureTarget, m_textureBacking->image());
    else
#endif
    GL_TexImage2D(textureTarget, 0, internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
    GL_FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureTarget, m_texture, 0);

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

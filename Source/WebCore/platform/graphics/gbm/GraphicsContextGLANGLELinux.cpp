/*
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

#include "config.h"

#if ENABLE(WEBGL)
#include "GraphicsContextGLANGLE.h"

#include "ANGLEHeaders.h"
#include "GraphicsContextGLFallback.h"
#include "GraphicsContextGLGBMTextureMapper.h"
#include "PixelBuffer.h"
#include "PlatformDisplay.h"

namespace WebCore {

#if USE(EGL)
static inline bool isDMABufSupportedByNativePlatform(const PlatformDisplay::EGLExtensions& eglExtensions)
{
    return eglExtensions.KHR_image_base && eglExtensions.EXT_image_dma_buf_import;
}
#endif

RefPtr<GraphicsContextGL> createWebProcessGraphicsContextGL(const GraphicsContextGLAttributes& attributes)
{
#if USE(TEXTURE_MAPPER_DMABUF) && USE(EGL)
    if (isDMABufSupportedByNativePlatform(PlatformDisplay::sharedDisplayForCompositing().eglExtensions())) {
        auto context = GraphicsContextGLGBMTextureMapper::create(GraphicsContextGLAttributes(attributes));
        if (context)
            return context;
    }
#endif

    return GraphicsContextGLFallback::create(GraphicsContextGLAttributes(attributes));
}

GraphicsContextGLANGLE::GraphicsContextGLANGLE(GraphicsContextGLAttributes attributes)
    : GraphicsContextGL(attributes)
{
}

GraphicsContextGLANGLE::~GraphicsContextGLANGLE()
{
    bool success = makeContextCurrent();
    ASSERT_UNUSED(success, success);
    if (m_texture)
        GL_DeleteTextures(1, &m_texture);

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
}

GCGLDisplay GraphicsContextGLANGLE::platformDisplay() const
{
    return m_displayObj;
}

GCGLConfig GraphicsContextGLANGLE::platformConfig() const
{
    return m_configObj;
}

void GraphicsContextGLANGLE::checkGPUStatus()
{
}

void GraphicsContextGLANGLE::platformReleaseThreadResources()
{
}

RefPtr<PixelBuffer> GraphicsContextGLANGLE::readCompositedResults()
{
    return readRenderingResults();
}

bool GraphicsContextGLANGLE::makeContextCurrent()
{
    static thread_local TLS_MODEL_INITIAL_EXEC GraphicsContextGLANGLE* s_currentContext { nullptr };

    if (s_currentContext == this)
        return true;

    bool current = !!EGL_MakeCurrent(m_displayObj, EGL_NO_SURFACE, EGL_NO_SURFACE, m_contextObj);
    if (current)
        s_currentContext = this;
    return current;
}

} // namespace WebCore

#endif // ENABLE(WEBGL)

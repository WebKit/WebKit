/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Holger Hans Peter Freyther <zecke@selfish.org>
 * Copyright (C) 2008, 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 * Copyright (C) 2020 Apple Inc.  All rights reserved.
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
#include "ImageBufferCairoGLSurfaceBackend.h"

#if USE(CAIRO) && ENABLE(ACCELERATED_2D_CANVAS)

#include "GLContext.h"
#include "TextureMapperGL.h"
#include <wtf/IsoMallocInlines.h>

#if USE(EGL)
#if USE(LIBEPOXY)
#include "EpoxyEGL.h"
#else
#include <EGL/egl.h>
#endif
#endif
#include <cairo-gl.h>

#if USE(LIBEPOXY)
#include <epoxy/gl.h>
#elif USE(OPENGL_ES)
#include <GLES2/gl2.h>
#else
#include "OpenGLShims.h"
#endif

#if USE(COORDINATED_GRAPHICS)
#include "TextureMapperPlatformLayerBuffer.h"
#include "TextureMapperPlatformLayerProxy.h"
#endif

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ImageBufferCairoGLSurfaceBackend);

static inline void clearSurface(cairo_surface_t* surface)
{
    RefPtr<cairo_t> cr = adoptRef(cairo_create(surface));
    cairo_set_operator(cr.get(), CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr.get());
}

std::unique_ptr<ImageBufferCairoGLSurfaceBackend> ImageBufferCairoGLSurfaceBackend::create(const FloatSize& size, float resolutionScale, ColorSpace colorSpace, const HostWindow*)
{
    IntSize backendSize = calculateBackendSize(size, resolutionScale);
    if (backendSize.isEmpty())
        return nullptr;

    auto* context = PlatformDisplay::sharedDisplayForCompositing().sharingGLContext();
    context->makeContextCurrent();

    // We must generate the texture ourselves, because there is no Cairo API for extracting it
    // from a pre-existing surface.
    uint32_t texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0 /* level */, GL_RGBA, backendSize.width(), backendSize.height(), 0 /* border */, GL_RGBA, GL_UNSIGNED_BYTE, 0);

    cairo_device_t* device = context->cairoDevice();

    // Thread-awareness is a huge performance hit on non-Intel drivers.
    cairo_gl_device_set_thread_aware(device, FALSE);

    auto surface = adoptRef(cairo_gl_surface_create_for_texture(device, CAIRO_CONTENT_COLOR_ALPHA, texture, backendSize.width(), backendSize.height()));
    if (cairo_surface_status(surface.get()) != CAIRO_STATUS_SUCCESS)
        return nullptr;

    clearSurface(surface.get());

    return std::unique_ptr<ImageBufferCairoGLSurfaceBackend>(new ImageBufferCairoGLSurfaceBackend(size, backendSize, resolutionScale, colorSpace, WTFMove(surface), texture));
}

std::unique_ptr<ImageBufferCairoGLSurfaceBackend> ImageBufferCairoGLSurfaceBackend::create(const FloatSize& size, const GraphicsContext&)
{
    return ImageBufferCairoGLSurfaceBackend::create(size, 1, ColorSpace::SRGB, nullptr);
}

ImageBufferCairoGLSurfaceBackend::ImageBufferCairoGLSurfaceBackend(const FloatSize& logicalSize, const IntSize& backendSize, float resolutionScale, ColorSpace colorSpace, RefPtr<cairo_surface_t>&& surface, uint32_t texture)
    : ImageBufferCairoSurfaceBackend(logicalSize, backendSize, resolutionScale, colorSpace, WTFMove(surface))
    , m_texture(texture)
{
#if USE(COORDINATED_GRAPHICS)
#if USE(NICOSIA)
    m_nicosiaLayer = Nicosia::ContentLayer::create(Nicosia::ContentLayerTextureMapperImpl::createFactory(*this));
#else
    m_platformLayerProxy = adoptRef(new TextureMapperPlatformLayerProxy);
#endif
#endif
}

ImageBufferCairoGLSurfaceBackend::~ImageBufferCairoGLSurfaceBackend()
{
#if USE(COORDINATED_GRAPHICS) && USE(NICOSIA)
    downcast<Nicosia::ContentLayerTextureMapperImpl>(m_nicosiaLayer->impl()).invalidateClient();
#endif

    GLContext* previousActiveContext = GLContext::current();
    PlatformDisplay::sharedDisplayForCompositing().sharingGLContext()->makeContextCurrent();

    if (m_texture)
        glDeleteTextures(1, &m_texture);

#if USE(COORDINATED_GRAPHICS)
    if (m_compositorTexture)
        glDeleteTextures(1, &m_compositorTexture);
#endif

    if (previousActiveContext)
        previousActiveContext->makeContextCurrent();
}

PlatformLayer* ImageBufferCairoGLSurfaceBackend::platformLayer() const
{
#if USE(NICOSIA)
    return m_nicosiaLayer.get();
#else
    if (m_texture)
        return this;
#endif
    return nullptr;
}

bool ImageBufferCairoGLSurfaceBackend::copyToPlatformTexture(GraphicsContextGLOpenGL&, GCGLenum target, PlatformGLObject destinationTexture, GCGLenum internalformat, bool premultiplyAlpha, bool flipY) const
{
    ASSERT_WITH_MESSAGE(m_resolutionScale == 1.0, "Since the HiDPI Canvas feature is removed, the resolution factor here is always 1.");
    if (premultiplyAlpha || flipY)
        return false;

    if (!m_texture)
        return false;

    GCGLenum bindTextureTarget;
    switch (target) {
    case GL_TEXTURE_2D:
        bindTextureTarget = GL_TEXTURE_2D;
        break;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        bindTextureTarget = GL_TEXTURE_CUBE_MAP;
        break;
    default:
        return false;
    }

    cairo_surface_flush(m_surface.get());

    std::unique_ptr<GLContext> context = GLContext::createOffscreenContext(&PlatformDisplay::sharedDisplayForCompositing());
    context->makeContextCurrent();
    uint32_t fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);
    glBindTexture(bindTextureTarget, destinationTexture);
    glCopyTexImage2D(target, 0, internalformat, 0, 0, m_backendSize.width(), m_backendSize.height(), 0);
    glBindTexture(bindTextureTarget, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glFlush();
    glDeleteFramebuffers(1, &fbo);
    return true;
}

#if USE(COORDINATED_GRAPHICS)
void ImageBufferCairoGLSurfaceBackend::createCompositorBuffer()
{
    auto* context = PlatformDisplay::sharedDisplayForCompositing().sharingGLContext();
    context->makeContextCurrent();

    glGenTextures(1, &m_compositorTexture);
    glBindTexture(GL_TEXTURE_2D, m_compositorTexture);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0 , GL_RGBA, m_backendSize.width(), m_backendSize.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

    m_compositorSurface = adoptRef(cairo_gl_surface_create_for_texture(context->cairoDevice(), CAIRO_CONTENT_COLOR_ALPHA, m_compositorTexture, m_backendSize.width(), m_backendSize.height()));
    m_compositorCr = adoptRef(cairo_create(m_compositorSurface.get()));
    cairo_set_antialias(m_compositorCr.get(), CAIRO_ANTIALIAS_NONE);
}

void ImageBufferCairoGLSurfaceBackend::swapBuffersIfNeeded()
{
    GLContext* previousActiveContext = GLContext::current();

    if (!m_compositorTexture) {
        createCompositorBuffer();

        auto proxyOperation =
            [this](TextureMapperPlatformLayerProxy& proxy)
            {
                LockHolder holder(proxy.lock());
                proxy.pushNextBuffer(makeUnique<TextureMapperPlatformLayerBuffer>(m_compositorTexture, m_backendSize, TextureMapperGL::ShouldBlend, GL_RGBA));
            };
#if USE(NICOSIA)
        proxyOperation(downcast<Nicosia::ContentLayerTextureMapperImpl>(m_nicosiaLayer->impl()).proxy());
#else
        proxyOperation(*m_platformLayerProxy);
#endif
    }

    // It would be great if we could just swap the buffers here as we do with webgl, but that breaks the cases
    // where one frame uses the content already rendered in the previous frame. So we just copy the content
    // into the compositor buffer.
    cairo_set_source_surface(m_compositorCr.get(), m_surface.get(), 0, 0);
    cairo_set_operator(m_compositorCr.get(), CAIRO_OPERATOR_SOURCE);
    cairo_paint(m_compositorCr.get());

    if (previousActiveContext)
        previousActiveContext->makeContextCurrent();
}
#else
void ImageBufferCairoGLSurfaceBackend::paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, const TransformationMatrix& matrix, float opacity)
{
    ASSERT(m_texture);

    // Cairo may change the active context, so we make sure to change it back after flushing.
    GLContext* previousActiveContext = GLContext::current();
    cairo_surface_flush(m_surface.get());
    previousActiveContext->makeContextCurrent();

    static_cast<TextureMapperGL&>(textureMapper).drawTexture(m_texture, TextureMapperGL::ShouldBlend, m_backendSize, targetRect, matrix, opacity);
}
#endif

} // namespace WebCore

#endif // USE(CAIRO) && ENABLE(ACCELERATED_2D_CANVAS)

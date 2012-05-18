/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "GraphicsContext3D.h"

#if ENABLE(WEBGL)

#include "Extensions3DOpenGL.h"
#include "GraphicsContext3DPrivate.h"
#include "Image.h"
#include "OpenGLShims.h"
#include "PlatformContextCairo.h"
#include "RefPtrCairo.h"
#include "ShaderLang.h"
#include <cairo.h>
#include <wtf/NotFound.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

PassRefPtr<GraphicsContext3D> GraphicsContext3D::create(GraphicsContext3D::Attributes attributes, HostWindow* hostWindow, GraphicsContext3D::RenderStyle renderStyle)
{
    // This implementation doesn't currently support rendering directly to the HostWindow.
    if (renderStyle == RenderDirectlyToHostWindow)
        return 0;

    static bool initialized = false;
    static bool success = true;
    if (!initialized) {
        success = initializeOpenGLShims();
        initialized = true;
    }
    if (!success)
        return 0;

    RefPtr<GraphicsContext3D> context = adoptRef(new GraphicsContext3D(attributes, hostWindow, false));
    return context.release();
}

GraphicsContext3D::GraphicsContext3D(GraphicsContext3D::Attributes attributes, HostWindow*, bool)
    : m_currentWidth(0)
    , m_currentHeight(0)
    , m_attrs(attributes)
    , m_texture(0)
    , m_fbo(0)
    , m_depthStencilBuffer(0)
    , m_boundFBO(0)
    , m_multisampleFBO(0)
    , m_multisampleDepthStencilBuffer(0)
    , m_multisampleColorBuffer(0)
    , m_private(GraphicsContext3DPrivate::create(this))
{
    makeContextCurrent();

    validateAttributes();

    // Create a texture to render into.
    ::glGenTextures(1, &m_texture);
    ::glBindTexture(GL_TEXTURE_2D, m_texture);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    ::glBindTexture(GL_TEXTURE_2D, 0);

    // Create an FBO.
    ::glGenFramebuffersEXT(1, &m_fbo);
    ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);

    m_boundFBO = m_fbo;
    if (!m_attrs.antialias && (m_attrs.stencil || m_attrs.depth))
        ::glGenRenderbuffersEXT(1, &m_depthStencilBuffer);
    
    // Create a multisample FBO.
    if (m_attrs.antialias) {
        ::glGenFramebuffersEXT(1, &m_multisampleFBO);
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_multisampleFBO);
        m_boundFBO = m_multisampleFBO;
        ::glGenRenderbuffersEXT(1, &m_multisampleColorBuffer);
        if (m_attrs.stencil || m_attrs.depth)
            ::glGenRenderbuffersEXT(1, &m_multisampleDepthStencilBuffer);
    }

    // ANGLE initialization.
    ShBuiltInResources ANGLEResources;
    ShInitBuiltInResources(&ANGLEResources);

    getIntegerv(GraphicsContext3D::MAX_VERTEX_ATTRIBS, &ANGLEResources.MaxVertexAttribs);
    getIntegerv(GraphicsContext3D::MAX_VERTEX_UNIFORM_VECTORS, &ANGLEResources.MaxVertexUniformVectors);
    getIntegerv(GraphicsContext3D::MAX_VARYING_VECTORS, &ANGLEResources.MaxVaryingVectors);
    getIntegerv(GraphicsContext3D::MAX_VERTEX_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxVertexTextureImageUnits);
    getIntegerv(GraphicsContext3D::MAX_COMBINED_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxCombinedTextureImageUnits);
    getIntegerv(GraphicsContext3D::MAX_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxTextureImageUnits);
    getIntegerv(GraphicsContext3D::MAX_FRAGMENT_UNIFORM_VECTORS, &ANGLEResources.MaxFragmentUniformVectors);

    // Always set to 1 for OpenGL ES.
    ANGLEResources.MaxDrawBuffers = 1;
    m_compiler.setResources(ANGLEResources);

    ::glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    ::glEnable(GL_POINT_SPRITE);
    ::glClearColor(0, 0, 0, 0);
}

GraphicsContext3D::~GraphicsContext3D()
{
    makeContextCurrent();
    ::glDeleteTextures(1, &m_texture);
    if (m_attrs.antialias) {
        ::glDeleteRenderbuffersEXT(1, &m_multisampleColorBuffer);
        if (m_attrs.stencil || m_attrs.depth)
            ::glDeleteRenderbuffersEXT(1, &m_multisampleDepthStencilBuffer);
        ::glDeleteFramebuffersEXT(1, &m_multisampleFBO);
    } else {
        if (m_attrs.stencil || m_attrs.depth)
            ::glDeleteRenderbuffersEXT(1, &m_depthStencilBuffer);
    }
    ::glDeleteFramebuffersEXT(1, &m_fbo);
}

bool GraphicsContext3D::getImageData(Image* image, unsigned int format, unsigned int type, bool premultiplyAlpha, bool ignoreGammaAndColorProfile, Vector<uint8_t>& outputVector)
{
    if (!image)
        return false;
    // We need this to stay in scope because the native image is just a shallow copy of the data.
    ImageSource decoder(premultiplyAlpha ? ImageSource::AlphaPremultiplied : ImageSource::AlphaNotPremultiplied,
                        ignoreGammaAndColorProfile ? ImageSource::GammaAndColorProfileIgnored : ImageSource::GammaAndColorProfileApplied);
    AlphaOp alphaOp = AlphaDoNothing;
    RefPtr<cairo_surface_t> imageSurface;
    if (image->data()) {
        decoder.setData(image->data(), true);
        if (!decoder.frameCount() || !decoder.frameIsCompleteAtIndex(0))
            return false;
        imageSurface = decoder.createFrameAtIndex(0)->surface();
    } else {
        imageSurface = image->nativeImageForCurrentFrame()->surface();
        if (!premultiplyAlpha)
            alphaOp = AlphaDoUnmultiply;
    }

    if (!imageSurface)
        return false;

    int width = cairo_image_surface_get_width(imageSurface.get());
    int height = cairo_image_surface_get_height(imageSurface.get());
    if (!width || !height)
        return false;

    if (cairo_image_surface_get_format(imageSurface.get()) != CAIRO_FORMAT_ARGB32)
        return false;

    unsigned int srcUnpackAlignment = 1;
    size_t bytesPerRow = cairo_image_surface_get_stride(imageSurface.get());
    size_t bitsPerPixel = 32;
    unsigned int padding = bytesPerRow - bitsPerPixel / 8 * width;
    if (padding) {
        srcUnpackAlignment = padding + 1;
        while (bytesPerRow % srcUnpackAlignment)
            ++srcUnpackAlignment;
    }

    unsigned int packedSize;
    // Output data is tightly packed (alignment == 1).
    if (computeImageSizeInBytes(format, type, width, height, 1, &packedSize, 0) != GraphicsContext3D::NO_ERROR)
        return false;
    outputVector.resize(packedSize);

    return packPixels(cairo_image_surface_get_data(imageSurface.get()), SourceFormatBGRA8,
                      width, height, srcUnpackAlignment, format, type, alphaOp, outputVector.data());
}

void GraphicsContext3D::paintToCanvas(const unsigned char* imagePixels, int imageWidth, int imageHeight, int canvasWidth, int canvasHeight, PlatformContextCairo* context)
{
    if (!imagePixels || imageWidth <= 0 || imageHeight <= 0 || canvasWidth <= 0 || canvasHeight <= 0 || !context)
        return;

    cairo_t *cr = context->cr();
    context->save();

    cairo_rectangle(cr, 0, 0, canvasWidth, canvasHeight);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);

    RefPtr<cairo_surface_t> imageSurface = adoptRef(cairo_image_surface_create_for_data(
        const_cast<unsigned char*>(imagePixels), CAIRO_FORMAT_ARGB32, imageWidth, imageHeight, imageWidth * 4));

    // OpenGL keeps the pixels stored bottom up, so we need to flip the image here.
    cairo_translate(cr, 0, imageHeight);
    cairo_scale(cr, 1, -1);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_surface(cr, imageSurface.get(), 0, 0);
    cairo_rectangle(cr, 0, 0, canvasWidth, -canvasHeight);

    cairo_fill(cr);
    context->restore();
}

void GraphicsContext3D::setContextLostCallback(PassOwnPtr<ContextLostCallback>)
{
}

void GraphicsContext3D::setErrorMessageCallback(PassOwnPtr<ErrorMessageCallback>)
{
}

bool GraphicsContext3D::makeContextCurrent()
{
    if (!m_private)
        return false;
    return m_private->makeContextCurrent();
}
PlatformGraphicsContext3D GraphicsContext3D::platformGraphicsContext3D()
{
    return m_private->platformContext();
}

bool GraphicsContext3D::isGLES2Compliant() const
{
    return false;
}

#if USE(ACCELERATED_COMPOSITING)
PlatformLayer* GraphicsContext3D::platformLayer() const
{
    return m_private.get();
}
#endif

} // namespace WebCore

#endif // ENABLE(WEBGL)

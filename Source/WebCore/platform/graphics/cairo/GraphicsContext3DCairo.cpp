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

#if USE(CAIRO)

#if ENABLE(GRAPHICS_CONTEXT_3D)
#include "GraphicsContext3D.h"

#include "CairoUtilities.h"
#include "GraphicsContext3DPrivate.h"
#include "Image.h"
#include "ImageSource.h"
#include "NotImplemented.h"
#include "PlatformContextCairo.h"
#include "RefPtrCairo.h"
#include <cairo.h>

#if PLATFORM(WIN)
#include <GLSLANG/ShaderLang.h>
#else
#include <ANGLE/ShaderLang.h>
#endif

#if USE(LIBEPOXY)
#include <epoxy/gl.h>
#elif !USE(OPENGL_ES_2)
#include "OpenGLShims.h"
#endif

#if USE(OPENGL_ES_2)
#include "Extensions3DOpenGLES.h"
#else
#include "Extensions3DOpenGL.h"
#endif

#if USE(TEXTURE_MAPPER)
#include "TextureMapperGC3DPlatformLayer.h"
#endif

namespace WebCore {

RefPtr<GraphicsContext3D> GraphicsContext3D::create(GraphicsContext3DAttributes attributes, HostWindow* hostWindow, GraphicsContext3D::RenderStyle renderStyle)
{
    // This implementation doesn't currently support rendering directly to the HostWindow.
    if (renderStyle == RenderDirectlyToHostWindow)
        return nullptr;

    static bool initialized = false;
    static bool success = true;
    if (!initialized) {
#if !USE(OPENGL_ES_2) && !USE(LIBEPOXY)
        success = initializeOpenGLShims();
#endif
        initialized = true;
    }
    if (!success)
        return nullptr;

    // Create the GraphicsContext3D object first in order to establist a current context on this thread.
    auto context = adoptRef(new GraphicsContext3D(attributes, hostWindow, renderStyle));

#if USE(LIBEPOXY) && USE(OPENGL_ES_2)
    // Bail if GLES3 was requested but cannot be provided.
    if (attributes.useGLES3 && !epoxy_is_desktop_gl() && epoxy_gl_version() < 30)
        return nullptr;
#endif

    return context;
}

GraphicsContext3D::GraphicsContext3D(GraphicsContext3DAttributes attributes, HostWindow*, GraphicsContext3D::RenderStyle renderStyle)
    : m_attrs(attributes)
{
#if USE(TEXTURE_MAPPER)
    m_texmapLayer = std::make_unique<TextureMapperGC3DPlatformLayer>(*this, renderStyle);
#else
    m_private = std::make_unique<GraphicsContext3DPrivate>(this, renderStyle);
#endif

    makeContextCurrent();

    validateAttributes();

    if (renderStyle == RenderOffscreen) {
        // Create a texture to render into.
        ::glGenTextures(1, &m_texture);
        ::glBindTexture(GL_TEXTURE_2D, m_texture);
        ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        ::glBindTexture(GL_TEXTURE_2D, 0);

        // Create an FBO.
        ::glGenFramebuffers(1, &m_fbo);
        ::glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

#if USE(COORDINATED_GRAPHICS_THREADED)
        ::glGenTextures(1, &m_compositorTexture);
        ::glBindTexture(GL_TEXTURE_2D, m_compositorTexture);
        ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        ::glGenTextures(1, &m_intermediateTexture);
        ::glBindTexture(GL_TEXTURE_2D, m_intermediateTexture);
        ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        ::glBindTexture(GL_TEXTURE_2D, 0);
#endif


        // Create a multisample FBO.
        if (m_attrs.antialias) {
            ::glGenFramebuffers(1, &m_multisampleFBO);
            ::glBindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
            m_state.boundFBO = m_multisampleFBO;
            ::glGenRenderbuffers(1, &m_multisampleColorBuffer);
            if (m_attrs.stencil || m_attrs.depth)
                ::glGenRenderbuffers(1, &m_multisampleDepthStencilBuffer);
        } else {
            // Bind canvas FBO.
            glBindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_fbo);
            m_state.boundFBO = m_fbo;
#if USE(OPENGL_ES_2)
            if (m_attrs.depth)
                glGenRenderbuffers(1, &m_depthBuffer);
            if (m_attrs.stencil)
                glGenRenderbuffers(1, &m_stencilBuffer);
#endif
            if (m_attrs.stencil || m_attrs.depth)
                glGenRenderbuffers(1, &m_depthStencilBuffer);
        }
    }

#if !USE(OPENGL_ES_2)
    ::glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

    if (GLContext::current()->version() >= 320) {
        m_usingCoreProfile = true;

        // From version 3.2 on we use the OpenGL Core profile, so request that ouput to the shader compiler.
        // OpenGL version 3.2 uses GLSL version 1.50.
        m_compiler = ANGLEWebKitBridge(SH_GLSL_150_CORE_OUTPUT);

        // From version 3.2 on we use the OpenGL Core profile, and we need a VAO for rendering.
        // A VAO could be created and bound by each component using GL rendering (TextureMapper, WebGL, etc). This is
        // a simpler solution: the first GraphicsContext3D created on a GLContext will create and bind a VAO for that context.
        GC3Dint currentVAO = 0;
        getIntegerv(GraphicsContext3D::VERTEX_ARRAY_BINDING, &currentVAO);
        if (!currentVAO) {
            m_vao = createVertexArray();
            bindVertexArray(m_vao);
        }
    } else {
        // For lower versions request the compatibility output to the shader compiler.
        m_compiler = ANGLEWebKitBridge(SH_GLSL_COMPATIBILITY_OUTPUT);

        // GL_POINT_SPRITE is needed in lower versions.
        ::glEnable(GL_POINT_SPRITE);
    }
#else
    // Adjust the shader specification depending on whether GLES3 (i.e. WebGL2 support) was requested.
    m_compiler = ANGLEWebKitBridge(SH_ESSL_OUTPUT, m_attrs.useGLES3 ? SH_WEBGL2_SPEC : SH_WEBGL_SPEC);
#endif

    // ANGLE initialization.
    ShBuiltInResources ANGLEResources;
    sh::InitBuiltInResources(&ANGLEResources);

    getIntegerv(GraphicsContext3D::MAX_VERTEX_ATTRIBS, &ANGLEResources.MaxVertexAttribs);
    getIntegerv(GraphicsContext3D::MAX_VERTEX_UNIFORM_VECTORS, &ANGLEResources.MaxVertexUniformVectors);
    getIntegerv(GraphicsContext3D::MAX_VARYING_VECTORS, &ANGLEResources.MaxVaryingVectors);
    getIntegerv(GraphicsContext3D::MAX_VERTEX_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxVertexTextureImageUnits);
    getIntegerv(GraphicsContext3D::MAX_COMBINED_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxCombinedTextureImageUnits);
    getIntegerv(GraphicsContext3D::MAX_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxTextureImageUnits);
    getIntegerv(GraphicsContext3D::MAX_FRAGMENT_UNIFORM_VECTORS, &ANGLEResources.MaxFragmentUniformVectors);

    // Always set to 1 for OpenGL ES.
    ANGLEResources.MaxDrawBuffers = 1;

    GC3Dint range[2], precision;
    getShaderPrecisionFormat(GraphicsContext3D::FRAGMENT_SHADER, GraphicsContext3D::HIGH_FLOAT, range, &precision);
    ANGLEResources.FragmentPrecisionHigh = (range[0] || range[1] || precision);

    m_compiler.setResources(ANGLEResources);

    ::glClearColor(0, 0, 0, 0);
}

GraphicsContext3D::~GraphicsContext3D()
{
#if USE(TEXTURE_MAPPER)
    if (m_texmapLayer->renderStyle() == RenderToCurrentGLContext)
        return;
#else
    if (m_private->renderStyle() == RenderToCurrentGLContext)
        return;
#endif

    makeContextCurrent();
    if (m_texture)
        ::glDeleteTextures(1, &m_texture);
#if USE(COORDINATED_GRAPHICS_THREADED)
    if (m_compositorTexture)
        ::glDeleteTextures(1, &m_compositorTexture);
#endif

    if (m_attrs.antialias) {
        ::glDeleteRenderbuffers(1, &m_multisampleColorBuffer);
        if (m_attrs.stencil || m_attrs.depth)
            ::glDeleteRenderbuffers(1, &m_multisampleDepthStencilBuffer);
        ::glDeleteFramebuffers(1, &m_multisampleFBO);
    } else if (m_attrs.stencil || m_attrs.depth) {
#if USE(OPENGL_ES_2)
        if (m_depthBuffer)
            glDeleteRenderbuffers(1, &m_depthBuffer);

        if (m_stencilBuffer)
            glDeleteRenderbuffers(1, &m_stencilBuffer);
#endif
        if (m_depthStencilBuffer)
            ::glDeleteRenderbuffers(1, &m_depthStencilBuffer);
    }
    ::glDeleteFramebuffers(1, &m_fbo);
#if USE(COORDINATED_GRAPHICS_THREADED)
    ::glDeleteTextures(1, &m_intermediateTexture);
#endif

    if (m_vao)
        deleteVertexArray(m_vao);
}

GraphicsContext3D::ImageExtractor::~ImageExtractor()
{
    if (m_decoder)
        delete m_decoder;
}

bool GraphicsContext3D::ImageExtractor::extractImage(bool premultiplyAlpha, bool ignoreGammaAndColorProfile)
{
    if (!m_image)
        return false;
    // We need this to stay in scope because the native image is just a shallow copy of the data.
    AlphaOption alphaOption = premultiplyAlpha ? AlphaOption::Premultiplied : AlphaOption::NotPremultiplied;
    GammaAndColorProfileOption gammaAndColorProfileOption = ignoreGammaAndColorProfile ? GammaAndColorProfileOption::Ignored : GammaAndColorProfileOption::Applied;
    m_decoder = new ImageSource(nullptr, alphaOption, gammaAndColorProfileOption);
    
    if (!m_decoder)
        return false;

    ImageSource& decoder = *m_decoder;
    m_alphaOp = AlphaDoNothing;

    if (m_image->data()) {
        decoder.setData(m_image->data(), true);
        if (!decoder.frameCount())
            return false;
        m_imageSurface = decoder.createFrameImageAtIndex(0);
    } else {
        m_imageSurface = m_image->nativeImageForCurrentFrame();
        // 1. For texImage2D with HTMLVideoElment input, assume no PremultiplyAlpha had been applied and the alpha value is 0xFF for each pixel,
        // which is true at present and may be changed in the future and needs adjustment accordingly.
        // 2. For texImage2D with HTMLCanvasElement input in which Alpha is already Premultiplied in this port, 
        // do AlphaDoUnmultiply if UNPACK_PREMULTIPLY_ALPHA_WEBGL is set to false.
        if (!premultiplyAlpha && m_imageHtmlDomSource != HtmlDomVideo)
            m_alphaOp = AlphaDoUnmultiply;

        // if m_imageSurface is not an image, extract a copy of the surface
        if (m_imageSurface && cairo_surface_get_type(m_imageSurface.get()) != CAIRO_SURFACE_TYPE_IMAGE) {
            IntSize surfaceSize = cairoSurfaceSize(m_imageSurface.get());
            auto tmpSurface = adoptRef(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, surfaceSize.width(), surfaceSize.height()));
            copyRectFromOneSurfaceToAnother(m_imageSurface.get(), tmpSurface.get(), IntSize(), IntRect(IntPoint(), surfaceSize), IntSize(), CAIRO_OPERATOR_SOURCE);
            m_imageSurface = WTFMove(tmpSurface);
        }
    }

    if (!m_imageSurface)
        return false;

    ASSERT(cairo_surface_get_type(m_imageSurface.get()) == CAIRO_SURFACE_TYPE_IMAGE);

    IntSize imageSize = cairoSurfaceSize(m_imageSurface.get());
    m_imageWidth = imageSize.width();
    m_imageHeight = imageSize.height();
    if (!m_imageWidth || !m_imageHeight)
        return false;

    if (cairo_image_surface_get_format(m_imageSurface.get()) != CAIRO_FORMAT_ARGB32)
        return false;

    unsigned int srcUnpackAlignment = 1;
    size_t bytesPerRow = cairo_image_surface_get_stride(m_imageSurface.get());
    size_t bitsPerPixel = 32;
    unsigned padding = bytesPerRow - bitsPerPixel / 8 * m_imageWidth;
    if (padding) {
        srcUnpackAlignment = padding + 1;
        while (bytesPerRow % srcUnpackAlignment)
            ++srcUnpackAlignment;
    }

    m_imagePixelData = cairo_image_surface_get_data(m_imageSurface.get());
    m_imageSourceFormat = DataFormatBGRA8;
    m_imageSourceUnpackAlignment = srcUnpackAlignment;
    return true;
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

void GraphicsContext3D::setContextLostCallback(std::unique_ptr<ContextLostCallback>)
{
}

void GraphicsContext3D::setErrorMessageCallback(std::unique_ptr<ErrorMessageCallback>)
{
}

bool GraphicsContext3D::makeContextCurrent()
{
#if USE(TEXTURE_MAPPER)
    if (m_texmapLayer)
        return m_texmapLayer->makeContextCurrent();
#else
    if (m_private)
        return m_private->makeContextCurrent();
#endif
    return false;
}

void GraphicsContext3D::checkGPUStatus()
{
}

PlatformGraphicsContext3D GraphicsContext3D::platformGraphicsContext3D()
{
#if USE(TEXTURE_MAPPER)
    return m_texmapLayer->platformContext();
#else
    return m_private->platformContext();
#endif
}

Platform3DObject GraphicsContext3D::platformTexture() const
{
    return m_texture;
}

bool GraphicsContext3D::isGLES2Compliant() const
{
#if USE(OPENGL_ES_2)
    return true;
#else
    return false;
#endif
}

PlatformLayer* GraphicsContext3D::platformLayer() const
{
#if USE(TEXTURE_MAPPER)
    return m_texmapLayer.get();
#else
    return m_private.get();
#endif
}

#if PLATFORM(GTK)
Extensions3D& GraphicsContext3D::getExtensions()
{
    if (!m_extensions) {
#if USE(OPENGL_ES_2)
        // glGetStringi is not available on GLES2.
        m_extensions = std::make_unique<Extensions3DOpenGLES>(this,  false);
#else
        // From OpenGL 3.2 on we use the Core profile, and there we must use glGetStringi.
        m_extensions = std::make_unique<Extensions3DOpenGL>(this, GLContext::current()->version() >= 320);
#endif
    }
    return *m_extensions;
}
#endif

} // namespace WebCore

#endif // ENABLE(GRAPHICS_CONTEXT_3D)

#endif // USE(CAIRO)

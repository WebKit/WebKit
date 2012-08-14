/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
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

#if USE(3D_GRAPHICS)

#include "GraphicsContext3D.h"

#include "BitmapImageSingleFrameSkia.h"
#include "Extensions3DOpenGLES.h"
#include "GraphicsContext.h"
#include "OpenGLESShims.h"
#include "WebGLLayerWebKitThread.h"

#include <BlackBerryPlatformGraphics.h>
#include <BlackBerryPlatformLog.h>

namespace WebCore {

PassRefPtr<GraphicsContext3D> GraphicsContext3D::create(Attributes attribs, HostWindow* hostWindow, RenderStyle renderStyle)
{
    // This implementation doesn't currently support rendering directly to a window.
    if (renderStyle == RenderDirectlyToHostWindow)
        return 0;

    return adoptRef(new GraphicsContext3D(attribs, hostWindow, false));
}

GraphicsContext3D::GraphicsContext3D(GraphicsContext3D::Attributes attrs, HostWindow*, bool renderDirectlyToHostWindow)
    : m_currentWidth(0)
    , m_currentHeight(0)
    , m_context(BlackBerry::Platform::Graphics::createWebGLContext())
    , m_compiler(SH_ESSL_OUTPUT)
    , m_extensions(adoptPtr(new Extensions3DOpenGLES(this)))
    , m_attrs(attrs)
    , m_texture(0)
    , m_fbo(0)
    , m_depthStencilBuffer(0)
    , m_layerComposited(false)
    , m_internalColorFormat(GL_RGBA)
    , m_boundFBO(0)
    , m_activeTexture(GL_TEXTURE0)
    , m_boundTexture0(0)
    , m_isImaginationHardware(0)
{
    if (!renderDirectlyToHostWindow) {
#if USE(ACCELERATED_COMPOSITING)
        m_compositingLayer = WebGLLayerWebKitThread::create();
#endif
        makeContextCurrent();

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
        if (m_attrs.stencil || m_attrs.depth)
            ::glGenRenderbuffers(1, &m_depthStencilBuffer);
        m_boundFBO = m_fbo;

#if USE(ACCELERATED_COMPOSITING)
        static_cast<WebGLLayerWebKitThread*>(m_compositingLayer.get())->setWebGLContext(this);
#endif
    }

    makeContextCurrent();

    const char* renderer = reinterpret_cast<const char*>(::glGetString(GL_RENDERER));
    m_isImaginationHardware = std::strstr(renderer, "PowerVR SGX");

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

    ANGLEResources.MaxDrawBuffers = 1; // Always set to 1 for OpenGL ES.
    ANGLEResources.OES_standard_derivatives = m_extensions->supports("GL_OES_standard_derivatives");
    ANGLEResources.OES_EGL_image_external = m_extensions->supports("GL_EGL_image_external");
    ANGLEResources.ARB_texture_rectangle = m_extensions->supports("GL_ARB_texture_rectangle");
    m_compiler.setResources(ANGLEResources);

    ::glClearColor(0, 0, 0, 0);
}

GraphicsContext3D::~GraphicsContext3D()
{
    if (m_texture) {
        makeContextCurrent();
        ::glDeleteTextures(1, &m_texture);
        if (m_attrs.stencil || m_attrs.depth)
            ::glDeleteRenderbuffers(1, &m_depthStencilBuffer);
        ::glDeleteFramebuffers(1, &m_fbo);
    }

    BlackBerry::Platform::Graphics::destroyWebGLContext(m_context);
}

void GraphicsContext3D::prepareTexture()
{
    if (m_layerComposited)
        return;

    makeContextCurrent();
    if (m_attrs.antialias)
        resolveMultisamplingIfNecessary();

    m_layerComposited = true;
}

bool GraphicsContext3D::reshapeFBOs(const IntSize& size)
{
    // A BlackBerry-specific implementation of reshapeFBOs is necessary because it contains:
    //  - an Imagination-specific implementation of anti-aliasing
    //  - an Imagination-specific fix for FBOs of size less than (16,16)

    int fboWidth = size.width();
    int fboHeight = size.height();

    // Imagination-specific fix
    if (m_isImaginationHardware) {
        fboWidth = std::max(fboWidth, 16);
        fboHeight = std::max(fboHeight, 16);
    }

    GLuint internalColorFormat, colorFormat, internalDepthStencilFormat = 0;
    if (m_attrs.alpha) {
        internalColorFormat = GL_RGBA;
        colorFormat = GL_RGBA;
    } else {
        internalColorFormat = GL_RGB;
        colorFormat = GL_RGB;
    }
    if (m_attrs.stencil || m_attrs.depth) {
        // We don't allow the logic where stencil is required and depth is not.
        // See GraphicsContext3D constructor.
        if (m_attrs.stencil && m_attrs.depth)
            internalDepthStencilFormat = GL_DEPTH24_STENCIL8_EXT;
        else
            internalDepthStencilFormat = GL_DEPTH_COMPONENT16;
    }

    GLint sampleCount = 8;
    if (m_attrs.antialias) {
        GLint maxSampleCount;
        // Hardcode the maximum number of samples due to header issue (PR132183)
        // ::glGetIntegerv(GL_MAX_SAMPLES_IMG, &maxSampleCount);
        maxSampleCount = 4;
        sampleCount = std::min(8, maxSampleCount);
    }

    bool mustRestoreFBO = false;
    if (m_boundFBO != m_fbo) {
        mustRestoreFBO = true;
        ::glBindFramebufferEXT(GraphicsContext3D::FRAMEBUFFER, m_fbo);
    }

    ::glBindTexture(GL_TEXTURE_2D, m_texture);
    ::glTexImage2D(GL_TEXTURE_2D, 0, internalColorFormat, fboWidth, fboHeight, 0, colorFormat, GL_UNSIGNED_BYTE, 0);

    Extensions3D* extensions = getExtensions();
    if (m_attrs.antialias) {
        extensions->framebufferTexture2DMultisampleIMG(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_texture, 0, sampleCount);

        if (m_attrs.stencil || m_attrs.depth) {
            ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_depthStencilBuffer);
            extensions->renderbufferStorageMultisampleIMG(GL_RENDERBUFFER_EXT, sampleCount, internalDepthStencilFormat, fboWidth, fboHeight);

            if (m_attrs.stencil)
                ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_depthStencilBuffer);
            if (m_attrs.depth)
                ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_depthStencilBuffer);
            ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
        }
    } else {
        ::glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_texture, 0);

        if (m_attrs.stencil || m_attrs.depth) {
            ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_depthStencilBuffer);
            ::glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, internalDepthStencilFormat, fboWidth, fboHeight);

            if (m_attrs.stencil)
                ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_depthStencilBuffer);
            if (m_attrs.depth)
                ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_depthStencilBuffer);
            ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
        }
    }
    ::glBindTexture(GL_TEXTURE_2D, 0);


    logFrameBufferStatus(__LINE__);

    return mustRestoreFBO;
}

void GraphicsContext3D::logFrameBufferStatus(int line)
{
    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "Checking FrameBuffer status at line %d: ", line);
    switch (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT)) {
    case GL_FRAMEBUFFER_COMPLETE:
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "COMPLETE | ");
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "INCOMPLETE ATTACHMENT | ");
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "MISSING ATTACHMENT | ");
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "INCOMPLETE DIMENSIONS | ");
        break;
    case GL_FRAMEBUFFER_UNSUPPORTED:
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "UNSUPPORTED | ");
        break;
    case FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT:
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "INCOMPLETE MULTISAMPLE | ");
        break;
    }

    switch (glGetError()) {
    case GL_NO_ERROR:
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "NO ERROR");
        break;
    case GL_INVALID_ENUM:
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "INVALID ENUM");
        break;
    case GL_INVALID_VALUE:
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "INVALID VALUE");
        break;
    case GL_INVALID_OPERATION:
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "INVALID OPERATION");
        break;
    case GL_OUT_OF_MEMORY:
        BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "OUT OF MEMORY");
        break;
    }
    BlackBerry::Platform::log(BlackBerry::Platform::LogLevelInfo, "\n");
}

void GraphicsContext3D::readPixelsIMG(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, void* data)
{
    // Currently only format=RGBA, type=UNSIGNED_BYTE is supported by the specification: http://www.khronos.org/registry/webgl/specs/latest/
    // If this ever changes, this code will need to be updated.

    // Calculate the strides of our data and canvas
    unsigned int formatSize = 4; // RGBA UNSIGNED_BYTE
    unsigned int dataStride = width * formatSize;
    unsigned int canvasStride = m_currentWidth * formatSize;

    // If we are using a pack alignment of 8, then we need to align our strides to 8 byte boundaries
    // See: http://en.wikipedia.org/wiki/Data_structure_alignment (computing padding)
    int packAlignment;
    glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment);
    if (8 == packAlignment) {
        dataStride = (dataStride + 7) & ~7;
        canvasStride = (canvasStride + 7) & ~7;
    }

    unsigned char* canvasData = new unsigned char[canvasStride * m_currentHeight];
    ::glReadPixels(0, 0, m_currentWidth, m_currentHeight, format, type, canvasData);

    // If we failed to read our canvas data due to a GL error, don't continue
    int error = glGetError();
    if (GL_NO_ERROR != error) {
        synthesizeGLError(error);
        return;
    }

    // Clear our data in case some of it lies outside the bounds of our canvas
    // TODO: don't do this if all of the data lies inside the bounds of the canvas
    memset(data, 0, dataStride * height);

    // Calculate the intersection of our canvas and data bounds
    IntRect dataRect(x, y, width, height);
    IntRect canvasRect(0, 0, m_currentWidth, m_currentHeight);
    IntRect nonZeroDataRect = intersection(dataRect, canvasRect);

    unsigned int xDataOffset = x < 0 ? -x * formatSize : 0;
    unsigned int yDataOffset = y < 0 ? -y * dataStride : 0;
    unsigned int xCanvasOffset = nonZeroDataRect.x() * formatSize;
    unsigned int yCanvasOffset = nonZeroDataRect.y() * canvasStride;
    unsigned char* dst = static_cast<unsigned char*>(data) + xDataOffset + yDataOffset;
    unsigned char* src = canvasData + xCanvasOffset + yCanvasOffset;
    for (int row = 0; row < nonZeroDataRect.height(); row++) {
        memcpy(dst, src, nonZeroDataRect.width() * formatSize);
        dst += dataStride;
        src += canvasStride;
    }

    delete [] canvasData;
}

bool GraphicsContext3D::paintsIntoCanvasBuffer() const
{
    return true;
}

bool GraphicsContext3D::makeContextCurrent()
{
    BlackBerry::Platform::Graphics::useWebGLContext(m_context);
    return true;
}

bool GraphicsContext3D::isGLES2Compliant() const
{
    return true;
}

bool GraphicsContext3D::isGLES2NPOTStrict() const
{
    return true;
}

bool GraphicsContext3D::isErrorGeneratedOnOutOfBoundsAccesses() const
{
    return false;
}

Platform3DObject GraphicsContext3D::platformTexture() const
{
    return m_texture;
}

PlatformGraphicsContext3D GraphicsContext3D::platformGraphicsContext3D() const
{
    return m_context;
}

#if USE(ACCELERATED_COMPOSITING)
PlatformLayer* GraphicsContext3D::platformLayer() const
{
    return m_compositingLayer.get();
}
#endif

void GraphicsContext3D::paintToCanvas(const unsigned char* imagePixels, int imageWidth, int imageHeight, int canvasWidth, int canvasHeight,
       GraphicsContext* context)
{
    unsigned char* tempPixels = new unsigned char[imageWidth * imageHeight * 4];

    // 3D images have already been converted to BGRA. Don't do it twice!!
    for (int y = 0; y < imageHeight; y++) {
        const unsigned char *srcRow = imagePixels + (imageWidth * 4 * y);
        unsigned char *destRow = tempPixels + (imageWidth * 4 * (imageHeight - y - 1));
        for (int i = 0; i < imageWidth * 4; i += 4)
            memcpy(destRow + i, srcRow + i, 4);
    }

    SkBitmap canvasBitmap;

    canvasBitmap.setConfig(SkBitmap::kARGB_8888_Config, canvasWidth, canvasHeight);
    canvasBitmap.allocPixels(0, 0);
    canvasBitmap.lockPixels();
    memcpy(canvasBitmap.getPixels(), tempPixels, imageWidth * imageHeight * 4);
    canvasBitmap.unlockPixels();
    delete [] tempPixels;

    FloatRect src(0, 0, canvasWidth, canvasHeight);
    FloatRect dst(0, 0, imageWidth, imageHeight);

    RefPtr<BitmapImageSingleFrameSkia> bitmapImage = BitmapImageSingleFrameSkia::create(canvasBitmap, false);
    context->drawImage(bitmapImage.get(), ColorSpaceDeviceRGB, dst, src, CompositeCopy, RespectImageOrientation, false);
}

void GraphicsContext3D::setContextLostCallback(PassOwnPtr<ContextLostCallback> callback)
{
    static_cast<Extensions3DOpenGLES*>(getExtensions())->setEXTContextLostCallback(callback);
}

void GraphicsContext3D::setErrorMessageCallback(PassOwnPtr<ErrorMessageCallback>)
{
}

} // namespace WebCore

#endif // USE(3D_GRAPHICS)


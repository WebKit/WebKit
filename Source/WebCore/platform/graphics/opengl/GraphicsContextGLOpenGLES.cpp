/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012 ChangSeok Oh <shivamidow@gmail.com>
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
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
#include "GraphicsContextGLOpenGL.h"

#if ENABLE(GRAPHICS_CONTEXT_GL) && USE(OPENGL_ES) && !PLATFORM(IOS_FAMILY)

#include "ExtensionsGLOpenGLES.h"
#include "IntRect.h"
#include "IntSize.h"
#include "NotImplemented.h"

#include <ANGLE/ShaderLang.h>

namespace WebCore {

void GraphicsContextGLOpenGL::releaseShaderCompiler()
{
    makeContextCurrent();
    ::glReleaseShaderCompiler();
}

void GraphicsContextGLOpenGL::readPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, void* data)
{
    makeContextCurrent();

    auto attributes = contextAttributes();

    // FIXME: remove the two glFlush calls when the driver bug is fixed, i.e.,
    // all previous rendering calls should be done before reading pixels.
    ::glFlush();
    if (attributes.antialias && m_state.boundFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        ::glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        ::glFlush();
    }

    ::glReadPixels(x, y, width, height, format, type, data);

    if (attributes.antialias && m_state.boundFBO == m_multisampleFBO)
        ::glBindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
}

void GraphicsContextGLOpenGL::readPixelsAndConvertToBGRAIfNecessary(int x, int y, int width, int height, unsigned char* pixels)
{
    ::glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    int totalBytes = width * height * 4;
    if (isGLES2Compliant()) {
        for (int i = 0; i < totalBytes; i += 4)
            std::swap(pixels[i], pixels[i + 2]); // Convert to BGRA.
    }
}

bool GraphicsContextGLOpenGL::reshapeFBOs(const IntSize& size)
{
    const int width = size.width();
    const int height = size.height();
    GLuint colorFormat = 0;
    auto attributes = contextAttributes();

    if (attributes.alpha) {
        m_internalColorFormat = GL_RGBA;
        colorFormat = GL_RGBA;
    } else {
        m_internalColorFormat = GL_RGB;
        colorFormat = GL_RGB;
    }

    // We don't allow the logic where stencil is required and depth is not.
    // See GraphicsContextGLOpenGL::validateAttributes.
    bool supportPackedDepthStencilBuffer = (attributes.stencil || attributes.depth) && getExtensions().supports("GL_OES_packed_depth_stencil");

    // Resize regular FBO.
    bool mustRestoreFBO = false;
    if (m_state.boundFBO != m_fbo) {
        mustRestoreFBO = true;
        ::glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    }

    ASSERT(m_texture);
    ::glBindTexture(GL_TEXTURE_2D, m_texture);
    ::glTexImage2D(GL_TEXTURE_2D, 0, m_internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
    ::glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);

#if USE(COORDINATED_GRAPHICS)
    if (m_compositorTexture) {
        ::glBindTexture(GL_TEXTURE_2D, m_compositorTexture);
        ::glTexImage2D(GL_TEXTURE_2D, 0, m_internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
        ::glBindTexture(GL_TEXTURE_2D, 0);
    }

    ::glBindTexture(GL_TEXTURE_2D, m_intermediateTexture);
    ::glTexImage2D(GL_TEXTURE_2D, 0, m_internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
    ::glBindTexture(GL_TEXTURE_2D, 0);
#endif

    ExtensionsGLOpenGLES& extensions = static_cast<ExtensionsGLOpenGLES&>(getExtensions());
    if (extensions.isImagination() && attributes.antialias) {
        GLint maxSampleCount;
        ::glGetIntegerv(ExtensionsGL::MAX_SAMPLES_IMG, &maxSampleCount); 
        GLint sampleCount = std::min(8, maxSampleCount);

        extensions.framebufferTexture2DMultisampleIMG(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0, sampleCount);

        if (attributes.stencil || attributes.depth) {
            // Use a 24 bit depth buffer where we know we have it.
            if (supportPackedDepthStencilBuffer) {
                ::glBindRenderbuffer(GL_RENDERBUFFER, m_depthStencilBuffer);
                extensions.renderbufferStorageMultisample(GL_RENDERBUFFER, sampleCount, GL_DEPTH24_STENCIL8_OES, width, height);
                if (attributes.stencil)
                    ::glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
                if (attributes.depth)
                    ::glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
            } else {
                if (attributes.stencil) {
                    ::glBindRenderbuffer(GL_RENDERBUFFER, m_stencilBuffer);
                    extensions.renderbufferStorageMultisample(GL_RENDERBUFFER, sampleCount, GL_STENCIL_INDEX8, width, height);
                    ::glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_stencilBuffer);
                }
                if (attributes.depth) {
                    ::glBindRenderbuffer(GL_RENDERBUFFER, m_depthBuffer);
                    extensions.renderbufferStorageMultisample(GL_RENDERBUFFER, sampleCount, GL_DEPTH_COMPONENT16, width, height);
                    ::glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthBuffer);
                }
            }
            ::glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }
    } else {
        if (attributes.stencil || attributes.depth) {
            // Use a 24 bit depth buffer where we know we have it.
            if (supportPackedDepthStencilBuffer) {
                ::glBindRenderbuffer(GL_RENDERBUFFER, m_depthStencilBuffer);
                ::glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, width, height);
                if (attributes.stencil)
                    ::glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
                if (attributes.depth)
                    ::glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
            } else {
                if (attributes.stencil) {
                    ::glBindRenderbuffer(GL_RENDERBUFFER, m_stencilBuffer);
                    ::glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, width, height);
                    ::glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_stencilBuffer);
                }
                if (attributes.depth) {
                    ::glBindRenderbuffer(GL_RENDERBUFFER, m_depthBuffer);
                    ::glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
                    ::glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthBuffer);
                }
            }
            ::glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }
    }
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        // FIXME: cleanup
        notImplemented();
    }

    return mustRestoreFBO;
}

void GraphicsContextGLOpenGL::resolveMultisamplingIfNecessary(const IntRect&)
{
    // FIXME: We don't support antialiasing yet.
    notImplemented();
}

void GraphicsContextGLOpenGL::renderbufferStorage(GCGLenum target, GCGLenum internalformat, GCGLsizei width, GCGLsizei height)
{
    makeContextCurrent();
    ::glRenderbufferStorage(target, internalformat, width, height);
}

void GraphicsContextGLOpenGL::getIntegerv(GCGLenum pname, GCGLint* value)
{
    makeContextCurrent();
    ::glGetIntegerv(pname, value);
}

void GraphicsContextGLOpenGL::getShaderPrecisionFormat(GCGLenum shaderType, GCGLenum precisionType, GCGLint* range, GCGLint* precision)
{
    ASSERT(range);
    ASSERT(precision);

    makeContextCurrent();
    ::glGetShaderPrecisionFormat(shaderType, precisionType, range, precision);
}

bool GraphicsContextGLOpenGL::texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, const void* pixels)
{
    if (width && height && !pixels) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }

    texImage2DDirect(target, level, internalformat, width, height, border, format, type, pixels);
    return true;
}

void GraphicsContextGLOpenGL::validateAttributes()
{
    validateDepthStencil("GL_OES_packed_depth_stencil");

    auto attributes = contextAttributes();

    if (attributes.antialias && !getExtensions().supports("GL_IMG_multisampled_render_to_texture")) {
        attributes.antialias = false;
        setContextAttributes(attributes);
    }
}

void GraphicsContextGLOpenGL::depthRange(GCGLclampf zNear, GCGLclampf zFar)
{
    makeContextCurrent();
    ::glDepthRangef(zNear, zFar);
}

void GraphicsContextGLOpenGL::clearDepth(GCGLclampf depth)
{
    makeContextCurrent();
    ::glClearDepthf(depth);
}

#if !PLATFORM(GTK)
ExtensionsGL& GraphicsContextGLOpenGL::getExtensions()
{
    if (!m_extensions)
        m_extensions = makeUnique<ExtensionsGLOpenGLES>(this, isGLES2Compliant());
    return *m_extensions;
}
#endif

#if PLATFORM(WIN) && USE(CA)
RefPtr<GraphicsContextGLOpenGL> GraphicsContextGLOpenGL::create(GraphicsContextGLAttributes attributes, HostWindow* hostWindow, GraphicsContextGLOpenGL::Destination destination)
{
    // This implementation doesn't currently support rendering directly to the HostWindow.
    if (destination == Destination::DirectlyToHostWindow)
        return nullptr;
    
    static bool initialized = false;
    static bool success = true;
    if (!initialized) {
#if !USE(OPENGL_ES)
        success = initializeOpenGLShims();
#endif
        initialized = true;
    }
    if (!success)
        return nullptr;
    
    return adoptRef(new GraphicsContextGLOpenGL(attributes, hostWindow, renderStyle));
}

GraphicsContextGLOpenGL::GraphicsContextGLOpenGL(GraphicsContextGLAttributes attributes, HostWindow*, GraphicsContextGLOpenGL::Destination destination, GraphicsContextGLOpenGL* sharedContext)
    : GraphicsContextGL(attributes, destination, sharedContext)
    , m_compiler(isGLES2Compliant() ? SH_ESSL_OUTPUT : SH_GLSL_COMPATIBILITY_OUTPUT)
    , m_private(makeUnique<GraphicsContextGLOpenGLPrivate>(this, destination))
{
    ASSERT_UNUSED(sharedContext, !sharedContext);
    makeContextCurrent();
    
    validateAttributes();
    attributes = contextAttributes(); // They may have changed during validation.

    if (destination == Destination::Offscreen) {
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
        
        m_state.boundFBO = m_fbo;
        if (!attributes.antialias && (attributes.stencil || attributes.depth))
            ::glGenRenderbuffers(1, &m_depthStencilBuffer);
        
        // Create a multisample FBO.
        if (attributes.antialias) {
            ::glGenFramebuffers(1, &m_multisampleFBO);
            ::glBindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
            m_state.boundFBO = m_multisampleFBO;
            ::glGenRenderbuffers(1, &m_multisampleColorBuffer);
            if (attributes.stencil || attributes.depth)
                ::glGenRenderbuffers(1, &m_multisampleDepthStencilBuffer);
        }
    }
    
    // ANGLE initialization.
    ShBuiltInResources ANGLEResources;
    ShInitBuiltInResources(&ANGLEResources);
    
    getIntegerv(GraphicsContextGLOpenGL::MAX_VERTEX_ATTRIBS, &ANGLEResources.MaxVertexAttribs);
    getIntegerv(GraphicsContextGLOpenGL::MAX_VERTEX_UNIFORM_VECTORS, &ANGLEResources.MaxVertexUniformVectors);
    getIntegerv(GraphicsContextGLOpenGL::MAX_VARYING_VECTORS, &ANGLEResources.MaxVaryingVectors);
    getIntegerv(GraphicsContextGLOpenGL::MAX_VERTEX_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxVertexTextureImageUnits);
    getIntegerv(GraphicsContextGLOpenGL::MAX_COMBINED_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxCombinedTextureImageUnits);
    getIntegerv(GraphicsContextGLOpenGL::MAX_TEXTURE_IMAGE_UNITS, &ANGLEResources.MaxTextureImageUnits);
    getIntegerv(GraphicsContextGLOpenGL::MAX_FRAGMENT_UNIFORM_VECTORS, &ANGLEResources.MaxFragmentUniformVectors);
    
    // Always set to 1 for OpenGL ES.
    ANGLEResources.MaxDrawBuffers = 1;
    
    GCGLint range[2], precision;
    getShaderPrecisionFormat(GraphicsContextGLOpenGL::FRAGMENT_SHADER, GraphicsContextGLOpenGL::HIGH_FLOAT, range, &precision);
    ANGLEResources.FragmentPrecisionHigh = (range[0] || range[1] || precision);
    
    m_compiler.setResources(ANGLEResources);
    
#if !USE(OPENGL_ES)
    ::glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    ::glEnable(GL_POINT_SPRITE);
#endif
    
    ::glClearColor(0, 0, 0, 0);
}

GraphicsContextGLOpenGL::~GraphicsContextGLOpenGL()
{
    makeContextCurrent();
    ::glDeleteTextures(1, &m_texture);

    auto attributes = contextAttributes();

    if (attributes.antialias) {
        ::glDeleteRenderbuffers(1, &m_multisampleColorBuffer);
        if (attributes.stencil || attributes.depth)
            ::glDeleteRenderbuffers(1, &m_multisampleDepthStencilBuffer);
        ::glDeleteFramebuffers(1, &m_multisampleFBO);
    } else {
        if (attributes.stencil || attributes.depth)
            ::glDeleteRenderbuffers(1, &m_depthStencilBuffer);
    }
    ::glDeleteFramebuffers(1, &m_fbo);
}

void GraphicsContextGLOpenGL::setContextLostCallback(std::unique_ptr<ContextLostCallback>)
{
}

void GraphicsContextGLOpenGL::setErrorMessageCallback(std::unique_ptr<ErrorMessageCallback>)
{
}

bool GraphicsContextGLOpenGL::makeContextCurrent()
{
    if (!m_private)
        return false;
    return m_private->makeContextCurrent();
}

void GraphicsContextGLOpenGL::checkGPUStatus()
{
}

PlatformGraphicsContextGL GraphicsContextGLOpenGL::platformGraphicsContextGL()
{
    return m_private->platformContext();
}

PlatformGLObject GraphicsContextGLOpenGL::platformTexture() const
{
    return m_texture;
}

bool GraphicsContextGLOpenGL::isGLES2Compliant() const
{
#if USE(OPENGL_ES)
    return true;
#else
    return false;
#endif
}

PlatformLayer* GraphicsContextGLOpenGL::platformLayer() const
{
    return m_webGLLayer->platformLayer();
}
#endif

}

#endif // ENABLE(GRAPHICS_CONTEXT_GL) && USE(OPENGL_ES) && !PLATFORM(IOS_FAMILY)

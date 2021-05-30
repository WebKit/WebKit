/*
 * Copyright (C) 2010, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#if ENABLE(WEBGL) && USE(OPENGL)

#include "ExtensionsGLOpenGL.h"
#include "IntRect.h"
#include "IntSize.h"
#include "Logging.h"
#include "NotImplemented.h"
#include "PixelBuffer.h"
#include "TemporaryOpenGLSetting.h"
#include <algorithm>
#include <cstring>
#include <wtf/MainThread.h>
#include <wtf/text/CString.h>

#if USE(ACCELERATE)
#include <Accelerate/Accelerate.h>
#endif

#if PLATFORM(GTK) || PLATFORM(WIN)
#include "OpenGLShims.h"
#elif USE(OPENGL_ES)
#import <OpenGLES/ES2/glext.h>
// From <OpenGLES/glext.h>
#define GL_RGBA32F_ARB                      0x8814
#define GL_RGB32F_ARB                       0x8815
#elif USE(OPENGL)
#define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#include <OpenGL/gl.h>
#include <OpenGL/gl3.h>
#undef GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#endif

namespace WebCore {

std::optional<PixelBuffer> GraphicsContextGLOpenGL::readPixelsForPaintResults()
{
    PixelBufferFormat format { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, DestinationColorSpace::SRGB() };
    auto pixelBuffer = PixelBuffer::tryCreate(format, getInternalFramebufferSize());
    if (!pixelBuffer)
        return std::nullopt;

    GLint packAlignment = 4;
    bool mustRestorePackAlignment = false;
    ::glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment);
    if (packAlignment > 4) {
        ::glPixelStorei(GL_PACK_ALIGNMENT, 4);
        mustRestorePackAlignment = true;
    }

    ::glReadPixels(0, 0, pixelBuffer->size().width(), pixelBuffer->size().height(), GL_RGBA, GL_UNSIGNED_BYTE, pixelBuffer->data().data());

    if (mustRestorePackAlignment)
        ::glPixelStorei(GL_PACK_ALIGNMENT, packAlignment);

    return pixelBuffer;
}

void GraphicsContextGLOpenGL::validateAttributes()
{
    validateDepthStencil("GL_EXT_packed_depth_stencil");
}

bool GraphicsContextGLOpenGL::reshapeFBOs(const IntSize& size)
{
    auto attrs = contextAttributes();
    const int width = size.width();
    const int height = size.height();
    GLuint colorFormat, internalDepthStencilFormat = 0;
    if (attrs.alpha) {
        m_internalColorFormat = GL_RGBA8;
        colorFormat = GL_RGBA;
    } else {
        m_internalColorFormat = GL_RGB8;
        colorFormat = GL_RGB;
    }
    if (attrs.stencil || attrs.depth) {
        // We don't allow the logic where stencil is required and depth is not.
        // See GraphicsContextGLOpenGL::validateAttributes.

        ExtensionsGLOpenGLCommon& extensions = getExtensions();
        // Use a 24 bit depth buffer where we know we have it.
        if (extensions.supports("GL_EXT_packed_depth_stencil"))
            internalDepthStencilFormat = GL_DEPTH24_STENCIL8_EXT;
        else
            internalDepthStencilFormat = GL_DEPTH_COMPONENT;
    }

    // Resize multisample FBO.
    if (attrs.antialias) {
        GLint maxSampleCount;
        ::glGetIntegerv(GL_MAX_SAMPLES_EXT, &maxSampleCount);
        // Using more than 4 samples is slow on some hardware and is unlikely to
        // produce a significantly better result.
        GLint sampleCount = std::min(4, maxSampleCount);
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_multisampleFBO);
        ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_multisampleColorBuffer);
        ::glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, sampleCount, m_internalColorFormat, width, height);
        ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, m_multisampleColorBuffer);
        if (attrs.stencil || attrs.depth) {
            ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_multisampleDepthStencilBuffer);
            ::glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, sampleCount, internalDepthStencilFormat, width, height);
            if (attrs.stencil)
                ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_multisampleDepthStencilBuffer);
            if (attrs.depth)
                ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_multisampleDepthStencilBuffer);
        }
        ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
        if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT) {
            // FIXME: cleanup.
            notImplemented();
        }
    }

    // resize regular FBO
    ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
    ASSERT(m_texture);
#if PLATFORM(COCOA)
    if (!allocateIOSurfaceBackingStore(size)) {
        RELEASE_LOG(WebGL, "Fatal: Unable to allocate backing store of size %d x %d", width, height);
        forceContextLost();
        return true;
    }
    ::glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, m_texture, 0);
#else
    ::glBindTexture(GL_TEXTURE_2D, m_texture);
    ::glTexImage2D(GL_TEXTURE_2D, 0, m_internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
    ::glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_texture, 0);

#if USE(COORDINATED_GRAPHICS)
    if (m_compositorTexture) {
        ::glBindTexture(GL_TEXTURE_2D, m_compositorTexture);
        ::glTexImage2D(GL_TEXTURE_2D, 0, m_internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
        ::glBindTexture(GL_TEXTURE_2D, 0);
        ::glBindTexture(GL_TEXTURE_2D, m_intermediateTexture);
        ::glTexImage2D(GL_TEXTURE_2D, 0, m_internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
        ::glBindTexture(GL_TEXTURE_2D, 0);
    }
#endif
#endif

    attachDepthAndStencilBufferIfNeeded(internalDepthStencilFormat, width, height);

    bool mustRestoreFBO = true;
    ASSERT(m_state.boundReadFBO == m_state.boundDrawFBO);
    if (attrs.antialias) {
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_multisampleFBO);
        if (m_state.boundDrawFBO == m_multisampleFBO)
            mustRestoreFBO = false;
    } else {
        if (m_state.boundDrawFBO == m_fbo)
            mustRestoreFBO = false;
    }

    return mustRestoreFBO;
}

void GraphicsContextGLOpenGL::attachDepthAndStencilBufferIfNeeded(GLuint internalDepthStencilFormat, int width, int height)
{
    auto attrs = contextAttributes();

    if (!attrs.antialias && (attrs.stencil || attrs.depth)) {
        ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_depthStencilBuffer);
        ::glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, internalDepthStencilFormat, width, height);
        if (attrs.stencil)
            ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_depthStencilBuffer);
        if (attrs.depth)
            ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_depthStencilBuffer);
        ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
    }

    if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT) {
        // FIXME: cleanup
        notImplemented();
    }
}

void GraphicsContextGLOpenGL::resolveMultisamplingIfNecessary(const IntRect& rect)
{
    TemporaryOpenGLSetting scopedScissor(GL_SCISSOR_TEST, GL_FALSE);
    TemporaryOpenGLSetting scopedDither(GL_DITHER, GL_FALSE);
    TemporaryOpenGLSetting scopedDepth(GL_DEPTH_TEST, GL_FALSE);
    TemporaryOpenGLSetting scopedStencil(GL_STENCIL_TEST, GL_FALSE);

    ::glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_multisampleFBO);
    ::glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_fbo);
    IntRect resolveRect = rect;
    if (rect.isEmpty())
        resolveRect = IntRect(0, 0, m_currentWidth, m_currentHeight);

    ::glBlitFramebufferEXT(resolveRect.x(), resolveRect.y(), resolveRect.maxX(), resolveRect.maxY(), resolveRect.x(), resolveRect.y(), resolveRect.maxX(), resolveRect.maxY(), GL_COLOR_BUFFER_BIT, GL_LINEAR);
}

void GraphicsContextGLOpenGL::renderbufferStorage(GCGLenum target, GCGLenum internalformat, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

#if USE(OPENGL)
    switch (internalformat) {
    case DEPTH_STENCIL:
        internalformat = GL_DEPTH24_STENCIL8_EXT;
        break;
    case DEPTH_COMPONENT16:
        internalformat = GL_DEPTH_COMPONENT;
        break;
    case RGBA4:
    case RGB5_A1:
        internalformat = GL_RGBA;
        break;
    case RGB565:
        internalformat = GL_RGB;
        break;
    }
#endif
    ::glRenderbufferStorageEXT(target, internalformat, width, height);
}

void GraphicsContextGLOpenGL::getIntegerv(GCGLenum pname, GCGLSpan<GCGLint> value)
{
    // Need to emulate MAX_FRAGMENT/VERTEX_UNIFORM_VECTORS and MAX_VARYING_VECTORS
    // because desktop GL's corresponding queries return the number of components
    // whereas GLES2 return the number of vectors (each vector has 4 components).
    // Therefore, the value returned by desktop GL needs to be divided by 4.
    if (!makeContextCurrent())
        return;

    switch (pname) {
#if USE(OPENGL)
    case MAX_FRAGMENT_UNIFORM_VECTORS:
        ::glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, value.data);
        *value /= 4;
        break;
    case MAX_VERTEX_UNIFORM_VECTORS:
        ::glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, value.data);
        *value /= 4;
        break;
    case MAX_VARYING_VECTORS:
        if (isGLES2Compliant()) {
            ASSERT(::glGetError() == GL_NO_ERROR);
            ::glGetIntegerv(GL_MAX_VARYING_VECTORS, value.data);
            if (::glGetError() == GL_INVALID_ENUM) {
                ::glGetIntegerv(GL_MAX_VARYING_COMPONENTS, value.data);
                *value /= 4;
            }
        } else {
            ::glGetIntegerv(GL_MAX_VARYING_FLOATS, value.data);
            *value /= 4;
        }
        break;
#endif
    case MAX_TEXTURE_SIZE:
        ::glGetIntegerv(MAX_TEXTURE_SIZE, value.data);
        if (getExtensions().requiresRestrictedMaximumTextureSize())
            *value = std::min(4096, *value);
        break;
    case MAX_CUBE_MAP_TEXTURE_SIZE:
        ::glGetIntegerv(MAX_CUBE_MAP_TEXTURE_SIZE, value.data);
        if (getExtensions().requiresRestrictedMaximumTextureSize())
            *value = std::min(1024, *value);
        break;
    default:
        ::glGetIntegerv(pname, value.data);
    }
}

void GraphicsContextGLOpenGL::getShaderPrecisionFormat(GCGLenum shaderType, GCGLenum precisionType, GCGLSpan<GCGLint, 2> range, GCGLint* precision)
{
    UNUSED_PARAM(shaderType);
    ASSERT(range.data);
    ASSERT(precision);

    if (!makeContextCurrent())
        return;


    switch (precisionType) {
    case GraphicsContextGL::LOW_INT:
    case GraphicsContextGL::MEDIUM_INT:
    case GraphicsContextGL::HIGH_INT:
        // These values are for a 32-bit twos-complement integer format.
        range[0] = 31;
        range[1] = 30;
        precision[0] = 0;
        break;
    case GraphicsContextGL::LOW_FLOAT:
    case GraphicsContextGL::MEDIUM_FLOAT:
    case GraphicsContextGL::HIGH_FLOAT:
        // These values are for an IEEE single-precision floating-point format.
        range[0] = 127;
        range[1] = 127;
        precision[0] = 23;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

void GraphicsContextGLOpenGL::texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, GCGLSpan<const GCGLvoid> pixels)
{
    if (width && height && !pixels.data) {
        synthesizeGLError(INVALID_VALUE);
        return;
    }

    GCGLenum openGLFormat = format;
    GCGLenum openGLInternalFormat = internalformat;
#if USE(OPENGL)
    if (type == GL_FLOAT) {
        if (format == GL_RGBA)
            openGLInternalFormat = GL_RGBA32F_ARB;
        else if (format == GL_RGB)
            openGLInternalFormat = GL_RGB32F_ARB;
    } else if (type == HALF_FLOAT_OES) {
        if (format == GL_RGBA)
            openGLInternalFormat = GL_RGBA16F_ARB;
        else if (format == GL_RGB)
            openGLInternalFormat = GL_RGB16F_ARB;
        else if (format == GL_LUMINANCE)
            openGLInternalFormat = GL_LUMINANCE16F_ARB;
        else if (format == GL_ALPHA)
            openGLInternalFormat = GL_ALPHA16F_ARB;
        else if (format == GL_LUMINANCE_ALPHA)
            openGLInternalFormat = GL_LUMINANCE_ALPHA16F_ARB;
        type = GL_HALF_FLOAT_ARB;
    }

    ASSERT(format != ExtensionsGL::SRGB8_ALPHA8_EXT);
    if (format == ExtensionsGL::SRGB_ALPHA_EXT)
        openGLFormat = GL_RGBA;
    else if (format == ExtensionsGL::SRGB_EXT)
        openGLFormat = GL_RGB;
#endif

    if (m_usingCoreProfile) {
        // There are some format values used in WebGL that are deprecated when using a core profile, so we need
        // to adapt them.
        switch (openGLInternalFormat) {
        case ALPHA:
            // The format is a simple component containing an alpha value. It needs to be backed with a GL_RED plane.
            // We change the formats to GL_RED (both need to be GL_ALPHA in WebGL) and instruct the texture to swizzle
            // the red component values with the alpha component values.
            openGLInternalFormat = openGLFormat = RED;
            texParameteri(target, TEXTURE_SWIZZLE_A, RED);
            break;
        case LUMINANCE_ALPHA:
            // The format has 2 components, an alpha one and a luminance one (same value for red, green and blue).
            // It needs to be backed with a GL_RG plane, using the red component for the colors and the green component
            // for alpha. We change the formats to GL_RG and swizzle the components.
            openGLInternalFormat = openGLFormat = RG;
            texParameteri(target, TEXTURE_SWIZZLE_R, RED);
            texParameteri(target, TEXTURE_SWIZZLE_G, RED);
            texParameteri(target, TEXTURE_SWIZZLE_B, RED);
            texParameteri(target, TEXTURE_SWIZZLE_A, GREEN);
            break;
        default:
            break;
        }
    }

    texImage2DDirect(target, level, openGLInternalFormat, width, height, border, openGLFormat, type, pixels.data);
}

void GraphicsContextGLOpenGL::depthRange(GCGLclampf zNear, GCGLclampf zFar)
{
    if (!makeContextCurrent())
        return;

    ::glDepthRange(zNear, zFar);
}

void GraphicsContextGLOpenGL::clearDepth(GCGLclampf depth)
{
    if (!makeContextCurrent())
        return;

    ::glClearDepth(depth);
}

#if !PLATFORM(GTK)
ExtensionsGLOpenGLCommon& GraphicsContextGLOpenGL::getExtensions()
{
    if (!m_extensions)
        m_extensions = makeUnique<ExtensionsGLOpenGL>(this, isGLES2Compliant());
    return *m_extensions;
}
#endif

void GraphicsContextGLOpenGL::readnPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLSpan<GCGLvoid> data)
{
    auto attrs = contextAttributes();

    // FIXME: remove the two glFlush calls when the driver bug is fixed, i.e.,
    // all previous rendering calls should be done before reading pixels.
    if (!makeContextCurrent())
        return;

    ::glFlush();
    ASSERT(m_state.boundReadFBO == m_state.boundDrawFBO);
    if (attrs.antialias && m_state.boundDrawFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        ::glBindFramebufferEXT(GraphicsContextGL::FRAMEBUFFER, m_fbo);
        ::glFlush();
    }
    ::glReadPixels(x, y, width, height, format, type, data.data);
    if (attrs.antialias && m_state.boundDrawFBO == m_multisampleFBO)
        ::glBindFramebufferEXT(GraphicsContextGL::FRAMEBUFFER, m_multisampleFBO);
}

}

#endif // ENABLE(WEBGL) && USE(OPENGL)

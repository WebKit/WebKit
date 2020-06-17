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

#if ENABLE(GRAPHICS_CONTEXT_GL) && (USE(OPENGL) || (PLATFORM(COCOA) && USE(OPENGL_ES)))

#if PLATFORM(IOS_FAMILY)
#include "GraphicsContextGLOpenGLESIOS.h"
#endif
#include "ExtensionsGLOpenGL.h"
#include "IntRect.h"
#include "IntSize.h"
#include "NotImplemented.h"
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

void GraphicsContextGLOpenGL::releaseShaderCompiler()
{
    makeContextCurrent();
    notImplemented();
}

#if PLATFORM(MAC)
static void wipeAlphaChannelFromPixels(int width, int height, unsigned char* pixels)
{
    // We can assume this doesn't overflow because the calling functions
    // use checked arithmetic.
    int totalBytes = width * height * 4;
    for (int i = 0; i < totalBytes; i += 4)
        pixels[i + 3] = 255;
}
#endif

void GraphicsContextGLOpenGL::readPixelsAndConvertToBGRAIfNecessary(int x, int y, int width, int height, unsigned char* pixels)
{
    auto attrs = contextAttributes();

    // NVIDIA drivers have a bug where calling readPixels in BGRA can return the wrong values for the alpha channel when the alpha is off for the context.
    if (!attrs.alpha && getExtensions().isNVIDIA()) {
        ::glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
#if USE(ACCELERATE)
        vImage_Buffer src;
        src.height = height;
        src.width = width;
        src.rowBytes = width * 4;
        src.data = pixels;

        vImage_Buffer dest;
        dest.height = height;
        dest.width = width;
        dest.rowBytes = width * 4;
        dest.data = pixels;

        // Swap pixel channels from RGBA to BGRA.
        const uint8_t map[4] = { 2, 1, 0, 3 };
        vImagePermuteChannels_ARGB8888(&src, &dest, map, kvImageNoFlags);
#else
        int totalBytes = width * height * 4;
        for (int i = 0; i < totalBytes; i += 4)
            std::swap(pixels[i], pixels[i + 2]);
#endif
    } else
        ::glReadPixels(x, y, width, height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels);

#if PLATFORM(MAC)
    if (!attrs.alpha)
        wipeAlphaChannelFromPixels(width, height, pixels);
#endif
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

        ExtensionsGL& extensions = getExtensions();
        // Use a 24 bit depth buffer where we know we have it.
        if (extensions.supports("GL_EXT_packed_depth_stencil"))
            internalDepthStencilFormat = GL_DEPTH24_STENCIL8_EXT;
        else
#if PLATFORM(COCOA) && USE(OPENGL_ES)
            internalDepthStencilFormat = GL_DEPTH_COMPONENT16;
#else
            internalDepthStencilFormat = GL_DEPTH_COMPONENT;
#endif
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
#if PLATFORM(COCOA) && USE(OPENGL_ES)
        ::glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, sampleCount, GL_RGBA8_OES, width, height);
#else
        ::glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, sampleCount, m_internalColorFormat, width, height);
#endif
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
#if USE(OPENGL_ES)
    ::glBindRenderbuffer(GL_RENDERBUFFER, m_texture);
    ::glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_texture);
    setRenderbufferStorageFromDrawable(m_currentWidth, m_currentHeight);
#else
    if (!allocateIOSurfaceBackingStore(size)) {
        RELEASE_LOG(WebGL, "Fatal: Unable to allocate backing store of size %d x %d", width, height);
        forceContextLost();
        return true;
    }
    updateFramebufferTextureBackingStoreFromLayer();
    ::glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, m_texture, 0);
#endif // !USE(OPENGL_ES))
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

#if PLATFORM(COCOA) && USE(OPENGL_ES)
    GLint boundFrameBuffer;
    ::glGetIntegerv(GL_FRAMEBUFFER_BINDING, &boundFrameBuffer);
#endif

    ::glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_multisampleFBO);
    ::glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_fbo);
#if PLATFORM(COCOA) && USE(OPENGL_ES)
    UNUSED_PARAM(rect);
    ::glFlush();
    ::glResolveMultisampleFramebufferAPPLE();
    const GLenum discards[] = { GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT };
    ::glDiscardFramebufferEXT(GL_READ_FRAMEBUFFER_APPLE, 2, discards);
    ::glBindFramebuffer(GL_FRAMEBUFFER, boundFrameBuffer);
#else
    IntRect resolveRect = rect;
    if (rect.isEmpty())
        resolveRect = IntRect(0, 0, m_currentWidth, m_currentHeight);

    ::glBlitFramebufferEXT(resolveRect.x(), resolveRect.y(), resolveRect.maxX(), resolveRect.maxY(), resolveRect.x(), resolveRect.y(), resolveRect.maxX(), resolveRect.maxY(), GL_COLOR_BUFFER_BIT, GL_LINEAR);
#endif
}

void GraphicsContextGLOpenGL::renderbufferStorage(GCGLenum target, GCGLenum internalformat, GCGLsizei width, GCGLsizei height)
{
    makeContextCurrent();
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

void GraphicsContextGLOpenGL::getIntegerv(GCGLenum pname, GCGLint* value)
{
    // Need to emulate MAX_FRAGMENT/VERTEX_UNIFORM_VECTORS and MAX_VARYING_VECTORS
    // because desktop GL's corresponding queries return the number of components
    // whereas GLES2 return the number of vectors (each vector has 4 components).
    // Therefore, the value returned by desktop GL needs to be divided by 4.
    makeContextCurrent();
    switch (pname) {
#if USE(OPENGL)
    case MAX_FRAGMENT_UNIFORM_VECTORS:
        ::glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, value);
        *value /= 4;
        break;
    case MAX_VERTEX_UNIFORM_VECTORS:
        ::glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, value);
        *value /= 4;
        break;
    case MAX_VARYING_VECTORS:
        if (isGLES2Compliant()) {
            ASSERT(::glGetError() == GL_NO_ERROR);
            ::glGetIntegerv(GL_MAX_VARYING_VECTORS, value);
            if (::glGetError() == GL_INVALID_ENUM) {
                ::glGetIntegerv(GL_MAX_VARYING_COMPONENTS, value);
                *value /= 4;
            }
        } else {
            ::glGetIntegerv(GL_MAX_VARYING_FLOATS, value);
            *value /= 4;
        }
        break;
#endif
    case MAX_TEXTURE_SIZE:
        ::glGetIntegerv(MAX_TEXTURE_SIZE, value);
        if (getExtensions().requiresRestrictedMaximumTextureSize())
            *value = std::min(4096, *value);
        break;
    case MAX_CUBE_MAP_TEXTURE_SIZE:
        ::glGetIntegerv(MAX_CUBE_MAP_TEXTURE_SIZE, value);
        if (getExtensions().requiresRestrictedMaximumTextureSize())
            *value = std::min(1024, *value);
        break;
#if PLATFORM(MAC)
    // Some older hardware advertises a larger maximum than they
    // can actually handle. Rather than detecting such devices, simply
    // clamp the maximum to 8192, which is big enough for a 5K display.
    case MAX_RENDERBUFFER_SIZE:
        ::glGetIntegerv(MAX_RENDERBUFFER_SIZE, value);
        *value = std::min(8192, *value);
        break;
    case MAX_VIEWPORT_DIMS:
        ::glGetIntegerv(MAX_VIEWPORT_DIMS, value);
        value[0] = std::min(8192, value[0]);
        value[1] = std::min(8192, value[1]);
        break;
#endif
    default:
        ::glGetIntegerv(pname, value);
    }
}

void GraphicsContextGLOpenGL::getShaderPrecisionFormat(GCGLenum shaderType, GCGLenum precisionType, GCGLint* range, GCGLint* precision)
{
    UNUSED_PARAM(shaderType);
    ASSERT(range);
    ASSERT(precision);

    makeContextCurrent();

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

bool GraphicsContextGLOpenGL::texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, const void* pixels)
{
    if (width && height && !pixels) {
        synthesizeGLError(INVALID_VALUE);
        return false;
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

    texImage2DDirect(target, level, openGLInternalFormat, width, height, border, openGLFormat, type, pixels);
    return true;
}

void GraphicsContextGLOpenGL::depthRange(GCGLclampf zNear, GCGLclampf zFar)
{
    makeContextCurrent();
#if PLATFORM(COCOA) && USE(OPENGL_ES)
    ::glDepthRangef(static_cast<float>(zNear), static_cast<float>(zFar));
#else
    ::glDepthRange(zNear, zFar);
#endif
}

void GraphicsContextGLOpenGL::clearDepth(GCGLclampf depth)
{
    makeContextCurrent();
#if PLATFORM(COCOA) && USE(OPENGL_ES)
    ::glClearDepthf(static_cast<float>(depth));
#else
    ::glClearDepth(depth);
#endif
}

#if !PLATFORM(GTK)
ExtensionsGL& GraphicsContextGLOpenGL::getExtensions()
{
    if (!m_extensions)
#if PLATFORM(COCOA) && USE(OPENGL_ES)
        m_extensions = makeUnique<ExtensionsGLOpenGL>(this, false);
#else
        m_extensions = makeUnique<ExtensionsGLOpenGL>(this, isGLES2Compliant());
#endif
    return *m_extensions;
}
#endif

void GraphicsContextGLOpenGL::readPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, void* data)
{
    auto attrs = contextAttributes();

    // FIXME: remove the two glFlush calls when the driver bug is fixed, i.e.,
    // all previous rendering calls should be done before reading pixels.
    makeContextCurrent();
    ::glFlush();
    ASSERT(m_state.boundReadFBO == m_state.boundDrawFBO);
    if (attrs.antialias && m_state.boundDrawFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        ::glBindFramebufferEXT(GraphicsContextGL::FRAMEBUFFER, m_fbo);
        ::glFlush();
    }
    ::glReadPixels(x, y, width, height, format, type, data);
    if (attrs.antialias && m_state.boundDrawFBO == m_multisampleFBO)
        ::glBindFramebufferEXT(GraphicsContextGL::FRAMEBUFFER, m_multisampleFBO);

#if PLATFORM(MAC)
    if (!attrs.alpha && (format == GraphicsContextGL::RGBA || format == GraphicsContextGL::BGRA) && (m_state.boundDrawFBO == m_fbo || (attrs.antialias && m_state.boundDrawFBO == m_multisampleFBO)))
        wipeAlphaChannelFromPixels(width, height, static_cast<unsigned char*>(data));
#endif
}

}

#endif // ENABLE(GRAPHICS_CONTEXT_GL) && (USE(OPENGL) || (PLATFORM(COCOA) && USE(OPENGL_ES)))

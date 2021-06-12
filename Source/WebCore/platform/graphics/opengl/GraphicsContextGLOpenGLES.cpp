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

#if ENABLE(WEBGL) && USE(OPENGL_ES)

#include "ExtensionsGLOpenGLES.h"
#include "IntRect.h"
#include "IntSize.h"
#include "NotImplemented.h"
#include "PixelBuffer.h"

#include <ANGLE/ShaderLang.h>

namespace WebCore {

void GraphicsContextGLOpenGL::readnPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLSpan<GCGLvoid> data)
{
    if (!makeContextCurrent())
        return;

    auto attributes = contextAttributes();

    // FIXME: remove the two glFlush calls when the driver bug is fixed, i.e.,
    // all previous rendering calls should be done before reading pixels.
    ::glFlush();
    ASSERT(m_state.boundReadFBO == m_state.boundDrawFBO);
    if (attributes.antialias && m_state.boundDrawFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        ::glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        ::glFlush();
    }

    ::glReadPixels(x, y, width, height, format, type, data.data);

    if (attributes.antialias && m_state.boundDrawFBO == m_multisampleFBO)
        ::glBindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
}

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
    ASSERT(m_state.boundReadFBO == m_state.boundDrawFBO);
    if (m_state.boundDrawFBO != m_fbo) {
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
                extensions.renderbufferStorageMultisampleANGLE(GL_RENDERBUFFER, sampleCount, GL_DEPTH24_STENCIL8_OES, width, height);
                if (attributes.stencil)
                    ::glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
                if (attributes.depth)
                    ::glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
            } else {
                if (attributes.stencil) {
                    ::glBindRenderbuffer(GL_RENDERBUFFER, m_stencilBuffer);
                    extensions.renderbufferStorageMultisampleANGLE(GL_RENDERBUFFER, sampleCount, GL_STENCIL_INDEX8, width, height);
                    ::glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_stencilBuffer);
                }
                if (attributes.depth) {
                    ::glBindRenderbuffer(GL_RENDERBUFFER, m_depthBuffer);
                    extensions.renderbufferStorageMultisampleANGLE(GL_RENDERBUFFER, sampleCount, GL_DEPTH_COMPONENT16, width, height);
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
    if (!makeContextCurrent())
        return;

    ::glRenderbufferStorage(target, internalformat, width, height);
}

void GraphicsContextGLOpenGL::getIntegerv(GCGLenum pname, GCGLSpan<GCGLint> value)
{
    if (!makeContextCurrent())
        return;

    ::glGetIntegerv(pname, value.data);
}

void GraphicsContextGLOpenGL::getShaderPrecisionFormat(GCGLenum shaderType, GCGLenum precisionType, GCGLSpan<GCGLint, 2> range, GCGLint* precision)
{
    ASSERT(range.data);
    ASSERT(precision);

    if (!makeContextCurrent())
        return;

    ::glGetShaderPrecisionFormat(shaderType, precisionType, range.data, precision);
}

void GraphicsContextGLOpenGL::texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, GCGLSpan<const GCGLvoid> pixels)
{
    if (width && height && !pixels.data) {
        synthesizeGLError(INVALID_VALUE);
        return;
    }

    texImage2DDirect(target, level, internalformat, width, height, border, format, type, pixels.data);
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
    if (!makeContextCurrent())
        return;

    ::glDepthRangef(zNear, zFar);
}

void GraphicsContextGLOpenGL::clearDepth(GCGLclampf depth)
{
    if (!makeContextCurrent())
        return;

    ::glClearDepthf(depth);
}

#if !PLATFORM(GTK)
ExtensionsGLOpenGLCommon& GraphicsContextGLOpenGL::getExtensions()
{
    if (!m_extensions)
        m_extensions = makeUnique<ExtensionsGLOpenGLES>(this, isGLES2Compliant());
    return *m_extensions;
}
#endif

}

#endif // ENABLE(WEBGL) && USE(OPENGL_ES)

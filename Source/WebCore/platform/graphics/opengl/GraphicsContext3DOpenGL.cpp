/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#if ENABLE(WEBGL)

#include "GraphicsContext3D.h"

#include "Extensions3DOpenGL.h"
#include "NotImplemented.h"

#if PLATFORM(MAC)
#include <OpenGL/gl.h>
#elif PLATFORM(GTK)
#include "OpenGLShims.h"
#elif PLATFORM(QT)
#include <QtGlobal>
#include <cairo/OpenGLShims.h>
#endif

namespace WebCore {

void GraphicsContext3D::readRenderingResults(unsigned char *pixels, int pixelsSize)
{
    if (pixelsSize < m_currentWidth * m_currentHeight * 4)
        return;

    makeContextCurrent();

    bool mustRestoreFBO = false;
    if (m_attrs.antialias) {
        ::glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_multisampleFBO);
        ::glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_fbo);
        ::glBlitFramebufferEXT(0, 0, m_currentWidth, m_currentHeight, 0, 0, m_currentWidth, m_currentHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
        mustRestoreFBO = true;
    } else {
        if (m_boundFBO != m_fbo) {
            mustRestoreFBO = true;
            ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
        }
    }

    GLint packAlignment = 4;
    bool mustRestorePackAlignment = false;
    ::glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment);
    if (packAlignment > 4) {
        ::glPixelStorei(GL_PACK_ALIGNMENT, 4);
        mustRestorePackAlignment = true;
    }

    ::glReadPixels(0, 0, m_currentWidth, m_currentHeight, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels);

    if (mustRestorePackAlignment)
        ::glPixelStorei(GL_PACK_ALIGNMENT, packAlignment);

    if (mustRestoreFBO)
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_boundFBO);
}

void GraphicsContext3D::reshape(int width, int height)
{
    if (!platformGraphicsContext3D())
        return;

    if (width == m_currentWidth && height == m_currentHeight)
        return;
    
    m_currentWidth = width;
    m_currentHeight = height;
    
    makeContextCurrent();
    validateAttributes();

    GLuint colorFormat, internalDepthStencilFormat = 0;
    if (m_attrs.alpha) {
        m_internalColorFormat = GL_RGBA8;
        colorFormat = GL_RGBA;
    } else {
        m_internalColorFormat = GL_RGB8;
        colorFormat = GL_RGB;
    }
    if (m_attrs.stencil || m_attrs.depth) {
        // We don't allow the logic where stencil is required and depth is not.
        // See GraphicsContext3D::validateAttributes.

        Extensions3D* extensions = getExtensions();
        // Use a 24 bit depth buffer where we know we have it
        if (extensions->supports("GL_EXT_packed_depth_stencil"))
            internalDepthStencilFormat = GL_DEPTH24_STENCIL8_EXT;
        else
            internalDepthStencilFormat = GL_DEPTH_COMPONENT;
    }

    bool mustRestoreFBO = false;

    // resize multisample FBO
    if (m_attrs.antialias) {
        GLint maxSampleCount;
        ::glGetIntegerv(GL_MAX_SAMPLES_EXT, &maxSampleCount);
        GLint sampleCount = std::min(8, maxSampleCount);
        if (sampleCount > maxSampleCount)
            sampleCount = maxSampleCount;
        if (m_boundFBO != m_multisampleFBO) {
            ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_multisampleFBO);
            mustRestoreFBO = true;
        }
        ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_multisampleColorBuffer);
        ::glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, sampleCount, m_internalColorFormat, width, height);
        ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, m_multisampleColorBuffer);
        if (m_attrs.stencil || m_attrs.depth) {
            ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_multisampleDepthStencilBuffer);
            ::glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, sampleCount, internalDepthStencilFormat, width, height);
            if (m_attrs.stencil)
                ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_multisampleDepthStencilBuffer);
            if (m_attrs.depth)
                ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_multisampleDepthStencilBuffer);
        }
        ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
        if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT) {
            // FIXME: cleanup.
            notImplemented();
        }
    }

    // resize regular FBO
    if (m_boundFBO != m_fbo) {
        mustRestoreFBO = true;
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
    }
    ::glBindTexture(GL_TEXTURE_2D, m_texture);
    ::glTexImage2D(GL_TEXTURE_2D, 0, m_internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
    ::glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_texture, 0);
    ::glBindTexture(GL_TEXTURE_2D, m_compositorTexture);
    ::glTexImage2D(GL_TEXTURE_2D, 0, m_internalColorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
    ::glBindTexture(GL_TEXTURE_2D, 0);
    if (!m_attrs.antialias && (m_attrs.stencil || m_attrs.depth)) {
        ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_depthStencilBuffer);
        ::glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, internalDepthStencilFormat, width, height);
        if (m_attrs.stencil)
            ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_depthStencilBuffer);
        if (m_attrs.depth)
            ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_depthStencilBuffer);
        ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
    }
    if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT) {
        // FIXME: cleanup
        notImplemented();
    }

    if (m_attrs.antialias) {
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_multisampleFBO);
        if (m_boundFBO == m_multisampleFBO)
            mustRestoreFBO = false;
    }

    // Initialize renderbuffers to 0.
    GLfloat clearColor[] = {0, 0, 0, 0}, clearDepth = 0;
    GLint clearStencil = 0;
    GLboolean colorMask[] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE}, depthMask = GL_TRUE;
    GLuint stencilMask = 0xffffffff;
    GLboolean isScissorEnabled = GL_FALSE;
    GLboolean isDitherEnabled = GL_FALSE;
    GLbitfield clearMask = GL_COLOR_BUFFER_BIT;
    ::glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
    ::glClearColor(0, 0, 0, 0);
    ::glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
    ::glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (m_attrs.depth) {
        ::glGetFloatv(GL_DEPTH_CLEAR_VALUE, &clearDepth);
        ::glClearDepth(1);
        ::glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
        ::glDepthMask(GL_TRUE);
        clearMask |= GL_DEPTH_BUFFER_BIT;
    }
    if (m_attrs.stencil) {
        ::glGetIntegerv(GL_STENCIL_CLEAR_VALUE, &clearStencil);
        ::glClearStencil(0);
        ::glGetIntegerv(GL_STENCIL_WRITEMASK, reinterpret_cast<GLint*>(&stencilMask));
        ::glStencilMaskSeparate(GL_FRONT, 0xffffffff);
        clearMask |= GL_STENCIL_BUFFER_BIT;
    }
    isScissorEnabled = ::glIsEnabled(GL_SCISSOR_TEST);
    ::glDisable(GL_SCISSOR_TEST);
    isDitherEnabled = ::glIsEnabled(GL_DITHER);
    ::glDisable(GL_DITHER);

    ::glClear(clearMask);

    ::glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    ::glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    if (m_attrs.depth) {
        ::glClearDepth(clearDepth);
        ::glDepthMask(depthMask);
    }
    if (m_attrs.stencil) {
        ::glClearStencil(clearStencil);
        ::glStencilMaskSeparate(GL_FRONT, stencilMask);
    }
    if (isScissorEnabled)
        ::glEnable(GL_SCISSOR_TEST);
    else
        ::glDisable(GL_SCISSOR_TEST);
    if (isDitherEnabled)
        ::glEnable(GL_DITHER);
    else
        ::glDisable(GL_DITHER);

    if (mustRestoreFBO)
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_boundFBO);

    ::glFlush();
}

void GraphicsContext3D::prepareTexture()
{
    if (m_layerComposited)
        return;
    makeContextCurrent();
    if (m_attrs.antialias) {
        ::glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_multisampleFBO);
        ::glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_fbo);
        ::glBlitFramebufferEXT(0, 0, m_currentWidth, m_currentHeight, 0, 0, m_currentWidth, m_currentHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }
    ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
    ::glActiveTexture(GL_TEXTURE0);
    ::glBindTexture(GL_TEXTURE_2D, m_compositorTexture);
    ::glCopyTexImage2D(GL_TEXTURE_2D, 0, m_internalColorFormat, 0, 0, m_currentWidth, m_currentHeight, 0);
    ::glBindTexture(GL_TEXTURE_2D, m_boundTexture0);
    ::glActiveTexture(m_activeTexture);
    ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_boundFBO);
    ::glFinish();
    m_layerComposited = true;
}

void GraphicsContext3D::bindFramebuffer(GC3Denum target, Platform3DObject buffer)
{
    makeContextCurrent();
    GLuint fbo;
    if (buffer)
        fbo = buffer;
    else
        fbo = (m_attrs.antialias ? m_multisampleFBO : m_fbo);
    if (fbo != m_boundFBO) {
        ::glBindFramebufferEXT(target, fbo);
        m_boundFBO = fbo;
    }
}

void GraphicsContext3D::copyTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Dint border)
{
    makeContextCurrent();
    if (m_attrs.antialias && m_boundFBO == m_multisampleFBO) {
        ::glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_multisampleFBO);
        ::glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_fbo);
        ::glBlitFramebufferEXT(x, y, x + width, y + height, x, y, x + width, y + height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
    }
    ::glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
    if (m_attrs.antialias && m_boundFBO == m_multisampleFBO)
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_multisampleFBO);
}

void GraphicsContext3D::copyTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
    if (m_attrs.antialias && m_boundFBO == m_multisampleFBO) {
        ::glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_multisampleFBO);
        ::glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_fbo);
        ::glBlitFramebufferEXT(x, y, x + width, y + height, x, y, x + width, y + height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
    }
    ::glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
    if (m_attrs.antialias && m_boundFBO == m_multisampleFBO)
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_multisampleFBO);
}

bool GraphicsContext3D::getActiveUniform(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }
    makeContextCurrent();
    GLint maxUniformSize = 0;
    ::glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxUniformSize);
    GLchar name[maxUniformSize]; // GL_ACTIVE_UNIFORM_MAX_LENGTH includes null termination
    GLsizei nameLength = 0;
    GLint size = 0;
    GLenum type = 0;
    ::glGetActiveUniform(program, index, maxUniformSize, &nameLength, &size, &type, name);
    if (!nameLength)
        return false;
    info.name = String(name, nameLength);
    info.type = type;
    info.size = size;
    return true;
}


void GraphicsContext3D::readPixels(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, void* data)
{
    // FIXME: remove the two glFlush calls when the driver bug is fixed, i.e.,
    // all previous rendering calls should be done before reading pixels.
    makeContextCurrent();
    ::glFlush();
    if (m_attrs.antialias && m_boundFBO == m_multisampleFBO) {
        ::glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_multisampleFBO);
        ::glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_fbo);
        ::glBlitFramebufferEXT(x, y, x + width, y + height, x, y, x + width, y + height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
        ::glFlush();
    }
    ::glReadPixels(x, y, width, height, format, type, data);
    if (m_attrs.antialias && m_boundFBO == m_multisampleFBO)
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_multisampleFBO);
}

void GraphicsContext3D::renderbufferStorage(GC3Denum target, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
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
    ::glRenderbufferStorageEXT(target, internalformat, width, height);
}

void GraphicsContext3D::getIntegerv(GC3Denum pname, GC3Dint* value)
{
    // Need to emulate MAX_FRAGMENT/VERTEX_UNIFORM_VECTORS and MAX_VARYING_VECTORS
    // because desktop GL's corresponding queries return the number of components
    // whereas GLES2 return the number of vectors (each vector has 4 components).
    // Therefore, the value returned by desktop GL needs to be divided by 4.
    makeContextCurrent();
    switch (pname) {
    case MAX_FRAGMENT_UNIFORM_VECTORS:
        ::glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, value);
        *value /= 4;
        break;
    case MAX_VERTEX_UNIFORM_VECTORS:
        ::glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, value);
        *value /= 4;
        break;
    case MAX_VARYING_VECTORS:
        ::glGetIntegerv(GL_MAX_VARYING_FLOATS, value);
        *value /= 4;
        break;
    default:
        ::glGetIntegerv(pname, value);
    }
}

bool GraphicsContext3D::texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, const void* pixels)
{
    if (width && height && !pixels) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }
    makeContextCurrent();
    GC3Denum openGLInternalFormat = internalformat;
    if (type == GL_FLOAT) {
        if (format == GL_RGBA)
            openGLInternalFormat = GL_RGBA32F_ARB;
        else if (format == GL_RGB)
            openGLInternalFormat = GL_RGB32F_ARB;
    }

    ::glTexImage2D(target, level, openGLInternalFormat, width, height, border, format, type, pixels);
    return true;
}

}

#endif // ENABLE(WEBGL)

/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
#include "GLXSurface.h"

#if USE(ACCELERATED_COMPOSITING) && USE(GLX)

namespace WebCore {

static const int pbufferAttributes[] = { GLX_PBUFFER_WIDTH, 1, GLX_PBUFFER_HEIGHT, 1, 0 };

#if USE(GRAPHICS_SURFACE)
GLXTransportSurface::GLXTransportSurface()
    : X11OffScreenWindow()
{
    createOffscreenWindow(&m_bufferHandle);
    m_drawable = m_bufferHandle;
}

GLXTransportSurface::~GLXTransportSurface()
{
}

PlatformSurfaceConfig GLXTransportSurface::configuration()
{
    return m_sharedResources->surfaceContextConfig();
}

void GLXTransportSurface::setGeometry(const IntRect& newRect)
{
    GLPlatformSurface::setGeometry(newRect);
    reSizeWindow(newRect, m_drawable);
}

void GLXTransportSurface::swapBuffers()
{
    if (!m_drawable)
        return;

    if (m_restoreNeeded) {
        GLint oldFBO;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glXSwapBuffers(sharedDisplay(), m_drawable);
        glBindFramebuffer(GL_FRAMEBUFFER, oldFBO);
    } else
        glXSwapBuffers(sharedDisplay(), m_drawable);
}

void GLXTransportSurface::destroy()
{
    destroyWindow(m_bufferHandle);
    m_bufferHandle = 0;
}

#endif

GLXPBuffer::GLXPBuffer()
    : X11OffScreenWindow()
{
    initialize();
}

GLXPBuffer::~GLXPBuffer()
{
}

void GLXPBuffer::initialize()
{
    Display* display = sharedDisplay();
    GLXFBConfig config = m_sharedResources->pBufferContextConfig();
    if (!config)
        return;

    m_drawable = glXCreatePbuffer(display, config, pbufferAttributes);
    m_bufferHandle = m_drawable;
}

PlatformSurfaceConfig GLXPBuffer::configuration()
{
    return m_sharedResources->pBufferContextConfig();
}

void GLXPBuffer::destroy()
{
    freeResources();
}

void GLXPBuffer::freeResources()
{
    if (!m_drawable)
        return;

    GLPlatformSurface::destroy();
    Display* display = sharedDisplay();
    if (!display)
        return;

    glXDestroyPbuffer(display, m_drawable);
    m_drawable = 0;
    m_bufferHandle = 0;
}

void GLXPBuffer::setGeometry(const IntRect& newRect)
{
    GLPlatformSurface::setGeometry(newRect);
}

}

#endif

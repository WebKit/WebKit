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
#include "GLPlatformSurface.h"

#if USE(ACCELERATED_COMPOSITING)

#if HAVE(GLX)
#include "GLXSurface.h"
#endif

#include "NotImplemented.h"

namespace WebCore {

PassOwnPtr<GLPlatformSurface> GLPlatformSurface::createOffscreenSurface()
{
#if HAVE(GLX)
    OwnPtr<GLPlatformSurface> surface = adoptPtr(new GLXPBuffer());

    if (surface->handle())
        return surface.release();
#endif

    return nullptr;
}

PassOwnPtr<GLPlatformSurface> GLPlatformSurface::createTransportSurface()
{
#if HAVE(GLX) && USE(GRAPHICS_SURFACE)
    OwnPtr<GLPlatformSurface> surface = adoptPtr(new GLXTransportSurface());

    if (surface->handle())
        return surface.release();
#endif

    return nullptr;
}

GLPlatformSurface::GLPlatformSurface()
    : m_restoreNeeded(true)
    , m_fboId(0)
    , m_sharedDisplay(0)
    , m_drawable(0)
{
}

GLPlatformSurface::~GLPlatformSurface()
{
}

PlatformSurface GLPlatformSurface::handle() const
{
    return m_drawable;
}

const IntRect& GLPlatformSurface::geometry() const
{
    return m_rect;
}

PlatformDisplay GLPlatformSurface::sharedDisplay() const
{
    return m_sharedDisplay;
}

PlatformSurfaceConfig GLPlatformSurface::configuration()
{
    return 0;
}

void GLPlatformSurface::swapBuffers()
{
    notImplemented();
}

void GLPlatformSurface::updateContents(const uint32_t texture, const GLuint bindFboId, const uint32_t bindTexture)
{
    if (!m_fboId)
        glGenFramebuffers(1, &m_fboId);

    m_restoreNeeded = false;

    int x = 0;
    int y = 0;
    int width = m_rect.width();
    int height = m_rect.height();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fboId);
    glBindTexture(GL_TEXTURE_2D, texture);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    // Use NEAREST as no scale is performed during the blit.
    glBlitFramebuffer(x, y, width, height, x, y, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    swapBuffers();
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    glBindTexture(GL_TEXTURE_2D, bindTexture);
    glBindFramebuffer(GL_FRAMEBUFFER, bindFboId);
}

void GLPlatformSurface::setGeometry(const IntRect& newRect)
{
    m_rect = newRect;
}

void GLPlatformSurface::destroy()
{
    m_rect = IntRect();

    if (m_fboId)
        glDeleteFramebuffers(1, &m_fboId);

    m_fboId = 0;
}

}

#endif

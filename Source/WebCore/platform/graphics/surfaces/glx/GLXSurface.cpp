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

#if USE(ACCELERATED_COMPOSITING) && HAVE(GLX)

namespace WebCore {

SharedX11Resources* SharedX11Resources::m_staticSharedResource = 0;

static const int pbufferAttributes[] = { GLX_PBUFFER_WIDTH, 1, GLX_PBUFFER_HEIGHT, 1, 0 };

GLXSurface::GLXSurface()
    : GLPlatformSurface()
{
    m_sharedResources = SharedX11Resources::create();
    m_sharedDisplay = m_sharedResources->display();
}

GLXSurface::~GLXSurface()
{
}

XVisualInfo* GLXSurface::visualInfo()
{
    return m_sharedResources->visualInfo();
}

Window GLXSurface::xWindow()
{
    return m_sharedResources->getXWindow();
}

GLXFBConfig GLXSurface::pBufferConfiguration()
{
    return m_sharedResources->pBufferContextConfig();
}

GLXFBConfig GLXSurface::transportSurfaceConfiguration()
{
    return m_sharedResources->surfaceContextConfig();
}

bool GLXSurface::isXRenderExtensionSupported()
{
    return m_sharedResources->isXRenderExtensionSupported();
}

GLXTransportSurface::GLXTransportSurface()
    : GLXSurface()
{
    initialize();
}

GLXTransportSurface::~GLXTransportSurface()
{
}

PlatformSurfaceConfig GLXTransportSurface::configuration()
{
    return transportSurfaceConfiguration();
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

void GLXTransportSurface::setGeometry(const IntRect& newRect)
{
    GLPlatformSurface::setGeometry(newRect);
    int width = newRect.width();
    int height = newRect.height();
    XResizeWindow(sharedDisplay(), m_drawable, width, height);
}

void GLXTransportSurface::initialize()
{
    Display* display = sharedDisplay();
    GLXFBConfig config = transportSurfaceConfiguration();
    if (!config)
        return;

    XVisualInfo* visInfo = visualInfo();
    if (!visInfo)
        return;

    Colormap cmap = XCreateColormap(display, xWindow(), visInfo->visual, AllocNone);
    XSetWindowAttributes attribute;
    attribute.background_pixel = WhitePixel(display, 0);
    attribute.border_pixel = BlackPixel(display, 0);
    attribute.colormap = cmap;
    m_drawable = XCreateWindow(display, xWindow(), 0, 0, 1, 1, 0, visInfo->depth, InputOutput, visInfo->visual, CWBackPixel | CWBorderPixel | CWColormap, &attribute);
    if (!m_drawable)
        return;

    XSetWindowBackgroundPixmap(display, m_drawable, 0);
    XCompositeRedirectWindow(display, m_drawable, CompositeRedirectManual);

    if (isXRenderExtensionSupported())
        XMapWindow(display, m_drawable);
}

void GLXTransportSurface::destroy()
{
    freeResources();
    GLPlatformSurface::destroy();
}

void GLXTransportSurface::freeResources()
{
    if (!m_drawable)
        return;

    GLPlatformSurface::destroy();
    Display* display = sharedDisplay();
    if (!display)
        return;

    XDestroyWindow(display, m_drawable);
    m_drawable = 0;
}

GLXPBuffer::GLXPBuffer()
    : GLXSurface()
{
    initialize();
}

GLXPBuffer::~GLXPBuffer()
{
}

void GLXPBuffer::initialize()
{
    Display* display = sharedDisplay();
    GLXFBConfig config = pBufferConfiguration();
    if (!config)
        return;

    m_drawable = glXCreatePbuffer(display, config, pbufferAttributes);
}

PlatformSurfaceConfig GLXPBuffer::configuration()
{
    return pBufferConfiguration();
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
}

}

#endif

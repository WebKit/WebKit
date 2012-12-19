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
#include "X11WindowResources.h"

#if USE(ACCELERATED_COMPOSITING)

#include "GLXWindowResources.h"

namespace WebCore {

PlatformSharedResources* SharedX11Resources::m_staticSharedResource = 0;

X11OffScreenWindow::X11OffScreenWindow()
    : GLPlatformSurface()
{
    m_sharedResources = PlatformSharedResources::create();
    m_sharedDisplay = m_sharedResources->nativeDisplay();
}

X11OffScreenWindow::~X11OffScreenWindow()
{
}

void X11OffScreenWindow::setGeometry(const IntRect& newRect)
{
    GLPlatformSurface::setGeometry(newRect);
    XResizeWindow(m_sharedResources->x11Display(), m_drawable, newRect.width(), newRect.height());
}

void X11OffScreenWindow::createOffscreenWindow()
{
    Display* display = m_sharedResources->x11Display();
    if (!display)
        return;

    GLXFBConfig config = m_sharedResources->surfaceContextConfig();

    if (!config) {
        LOG_ERROR("Failed to retrieve a valid configiration.");
        return;
    }

    XVisualInfo* visInfo = m_sharedResources->visualInfo();

    if (!visInfo) {
        LOG_ERROR("Failed to find valid XVisual");
        return;
    }

    Window xWindow = m_sharedResources->getXWindow();
    if (!xWindow)
        return;

    Colormap cmap = XCreateColormap(display, xWindow, visInfo->visual, AllocNone);
    XSetWindowAttributes attribute;
    attribute.background_pixel = WhitePixel(display, 0);
    attribute.border_pixel = BlackPixel(display, 0);
    attribute.colormap = cmap;
    m_drawable = XCreateWindow(display, xWindow, 0, 0, 1, 1, 0, visInfo->depth, InputOutput, visInfo->visual, CWBackPixel | CWBorderPixel | CWColormap, &attribute);

    if (!m_drawable) {
        LOG_ERROR("Failed to create offscreen window");
        return;
    }

    XSetWindowBackgroundPixmap(display, m_drawable, 0);
    XCompositeRedirectWindow(display, m_drawable, CompositeRedirectManual);

    if (m_sharedResources->isXRenderExtensionSupported())
        XMapWindow(display, m_drawable);

}

void X11OffScreenWindow::destroyWindow()
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

}

#endif

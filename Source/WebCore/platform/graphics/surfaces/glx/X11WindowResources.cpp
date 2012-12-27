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

#if USE(GLX)
#include "GLXWindowResources.h"
#endif

namespace WebCore {

PlatformSharedResources* SharedX11Resources::m_staticSharedResource = 0;

X11OffScreenWindow::X11OffScreenWindow()
    : GLPlatformSurface()
    , m_sharedResources(0)
    , m_configVisualInfo(0)
{
    m_sharedResources = PlatformSharedResources::create();

    if (!m_sharedResources)
        return;

    m_sharedDisplay = m_sharedResources->x11Display();
}

X11OffScreenWindow::~X11OffScreenWindow()
{
    if (m_configVisualInfo) {
        XFree(m_configVisualInfo);
        m_configVisualInfo = 0;
    }
}

void X11OffScreenWindow::reSizeWindow(const IntRect& newRect, PlatformBufferHandle windowId)
{
    XResizeWindow(m_sharedResources->x11Display(), windowId, newRect.width(), newRect.height());
}

void X11OffScreenWindow::createOffscreenWindow(PlatformBufferHandle* handleId)
{
    if (!m_sharedResources)
        return;

    Display* display = m_sharedResources->x11Display();
    if (!display)
        return;

#if USE(GLX)
    XVisualInfo* visInfo = m_sharedResources->visualInfo();
#else
    XVisualInfo* visInfo = m_configVisualInfo;
#endif

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
    PlatformBufferHandle tempHandleId;
    tempHandleId = XCreateWindow(display, xWindow, 0, 0, 1, 1, 0, visInfo->depth, InputOutput, visInfo->visual, CWBackPixel | CWBorderPixel | CWColormap, &attribute);

    if (!tempHandleId) {
        LOG_ERROR("Failed to create offscreen window");
        return;
    }

    XSetWindowBackgroundPixmap(display, tempHandleId, 0);
    XCompositeRedirectWindow(display, tempHandleId, CompositeRedirectManual);
    *handleId = tempHandleId;
    m_bufferHandle = tempHandleId;

    if (m_sharedResources->isXRenderExtensionSupported())
        XMapWindow(display, tempHandleId);

}

void X11OffScreenWindow::destroyWindow(PlatformBufferHandle windowId)
{
    if (!windowId)
        return;

    GLPlatformSurface::destroy();
    Display* display = m_sharedResources->x11Display();
    if (!display)
        return;

    XDestroyWindow(display, windowId);
}

Display* X11OffScreenWindow::nativeSharedDisplay()
{
    return m_sharedResources->x11Display();
}

#if USE(EGL)
bool X11OffScreenWindow::setVisualId(const EGLint id)
{
    VisualID visualId = static_cast<VisualID>(id);

    if (!visualId)
        return false;

    // EGL has suggested a visual id, so get the rest of the visual info for that id.
    XVisualInfo visualInfoTemplate;
    memset(&visualInfoTemplate, 0, sizeof(XVisualInfo));
    visualInfoTemplate.visualid = visualId;

    XVisualInfo* chosenVisualInfo;
    int matchingCount = 0;
    chosenVisualInfo = XGetVisualInfo(m_sharedResources->x11Display(), VisualIDMask, &visualInfoTemplate, &matchingCount);
    if (chosenVisualInfo) {
#if USE(GRAPHICS_SURFACE)
        if (m_sharedResources->isXRenderExtensionSupported()) {
            XRenderPictFormat* format = XRenderFindVisualFormat(m_sharedResources->x11Display(), chosenVisualInfo->visual);
            if (format && format->direct.alphaMask > 0) {
                m_configVisualInfo = chosenVisualInfo;
                return true;
            }
        }
#endif
        if (chosenVisualInfo->depth == 32) {
            m_configVisualInfo = chosenVisualInfo;
            return true;
        }
    }

    if (!m_configVisualInfo) {
        memset(&visualInfoTemplate, 0, sizeof(XVisualInfo));
        XVisualInfo* matchingVisuals;
        int matchingCount = 0;

        visualInfoTemplate.depth = chosenVisualInfo->depth;
        matchingVisuals = XGetVisualInfo(m_sharedResources->x11Display(), VisualDepthMask, &visualInfoTemplate, &matchingCount);

        if (matchingVisuals) {
            m_configVisualInfo = &matchingVisuals[0];
            XFree(matchingVisuals);
            return true;
        }
    }

    if (!m_configVisualInfo)
        LOG_ERROR("Failed to retrieve XVisual Info.");

    return false;
}
#endif

}

#endif

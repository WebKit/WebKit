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

namespace WebCore {

SharedX11Resources* SharedX11Resources::m_staticSharedResource = 0;

X11OffScreenWindow::X11OffScreenWindow()
    : m_sharedResources(0)
{
    m_sharedResources = SharedX11Resources::create();
}

X11OffScreenWindow::~X11OffScreenWindow()
{
}

void X11OffScreenWindow::reSizeWindow(const IntRect& newRect, const uint32_t windowId)
{
    XResizeWindow(m_sharedResources->x11Display(), windowId, newRect.width(), newRect.height());
    XFlush(m_sharedResources->x11Display());
}

#if USE(GRAPHICS_SURFACE)
void X11OffScreenWindow::createOffScreenWindow(uint32_t* handleId, const XVisualInfo& visInfo, const IntSize& size)
{
    if (!m_sharedResources)
        return;

    Display* display = m_sharedResources->x11Display();
    if (!display)
        return;

    if (!visInfo.visual) {
        LOG_ERROR("Failed to find valid XVisual.");
        return;
    }

    Window xWindow = m_sharedResources->getXWindow();
    if (!xWindow)
        return;

    Colormap cmap = XCreateColormap(display, xWindow, visInfo.visual, AllocNone);
    XSetWindowAttributes attribute;
    attribute.background_pixel = WhitePixel(display, 0);
    attribute.border_pixel = BlackPixel(display, 0);
    attribute.colormap = cmap;
    attribute.event_mask = ResizeRedirectMask;
    uint32_t tempHandleId;
    tempHandleId = XCreateWindow(display, xWindow, 0, 0, size.width(), size.height(), 0, visInfo.depth, InputOutput, visInfo.visual, CWBackPixel | CWBorderPixel | CWColormap, &attribute);

    if (!tempHandleId) {
        LOG_ERROR("Failed to create offscreen window.");
        return;
    }

    XSetWindowBackgroundPixmap(display, tempHandleId, 0);
    XCompositeRedirectWindow(display, tempHandleId, CompositeRedirectManual);
    XMapWindow(display, tempHandleId);
    *handleId = tempHandleId;
}

#if USE(EGL)
void X11OffScreenWindow::createOffScreenWindow(uint32_t* handleId, const EGLint id, const IntSize& size)
{
    VisualID visualId = static_cast<VisualID>(id);

    if (!visualId)
        return;

    // EGL has suggested a visual id, so get the rest of the visual info for that id.
    XVisualInfo visualInfoTemplate;
    memset(&visualInfoTemplate, 0, sizeof(XVisualInfo));
    visualInfoTemplate.visualid = visualId;
    int matchingCount = 0;
    OwnPtrX11<XVisualInfo> matchingVisuals = adoptPtr(XGetVisualInfo(m_sharedResources->x11Display(), VisualIDMask, &visualInfoTemplate, &matchingCount));
    XVisualInfo* foundVisual = 0;

    if (matchingVisuals) {
        for (int i = 0; i< matchingCount; i++) {
            XVisualInfo* temp = &matchingVisuals[i];

            if (m_sharedResources->isXRenderExtensionSupported()) {
                XRenderPictFormat* format = XRenderFindVisualFormat(m_sharedResources->x11Display(), temp->visual);
                if (format && format->direct.alphaMask > 0) {
                    foundVisual = temp;
                    break;
                }
            }

            if (temp->depth == 32) {
                foundVisual = temp;
                break;
            }
        }

        if (foundVisual)
            createOffScreenWindow(handleId, *foundVisual, size);
    }
}
#endif

#endif

void X11OffScreenWindow::destroyWindow(const uint32_t windowId)
{
    if (!windowId)
        return;

    Display* display = m_sharedResources->x11Display();
    if (!display)
        return;

    XDestroyWindow(display, windowId);
}

Display* X11OffScreenWindow::nativeSharedDisplay() const
{
    return m_sharedResources->x11Display();
}

bool X11OffScreenWindow::isXRenderExtensionSupported() const
{
    return m_sharedResources->isXRenderExtensionSupported();
}

}

#endif

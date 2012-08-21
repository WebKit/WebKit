/*
 * Copyright (C) 2012 Igalia S.L.
 * All rights reserved.
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
#include "RedirectedXCompositeWindow.h"

#include "GLContextGLX.h"
#include <GL/glx.h>
#include <X11/extensions/Xcomposite.h>
#include <cairo-xlib.h>
#include <gdk/gdkx.h>
#include <glib.h>
#include <gtk/gtk.h>

namespace WebCore {

PassOwnPtr<RedirectedXCompositeWindow> RedirectedXCompositeWindow::create(const IntSize& size)
{
    return adoptPtr(new RedirectedXCompositeWindow(size));
}

RedirectedXCompositeWindow::RedirectedXCompositeWindow(const IntSize& size)
    : m_window(0)
    , m_parentWindow(0)
    , m_pixmap(0)
    , m_surface(0)
    , m_pendingResizeSourceId(0)
    , m_needsNewPixmapAfterResize(false)
{
    Display* display = GLContextGLX::sharedDisplay();

    // This is based on code from Chromium: src/content/common/gpu/image_transport_surface_linux.cc
    XSetWindowAttributes windowAttributes;
    windowAttributes.override_redirect = True;
    m_parentWindow = XCreateWindow(display,
                                   RootWindow(display, DefaultScreen(display)),
                                   -100, -100, 1, 1,
                                   0,
                                   CopyFromParent,
                                   InputOutput,
                                   CopyFromParent,
                                   CWOverrideRedirect,
                                   &windowAttributes);
    XMapWindow(display, m_parentWindow);

    windowAttributes.event_mask = StructureNotifyMask;
    windowAttributes.override_redirect = False;
    m_window = XCreateWindow(display,
                             m_parentWindow,
                             0, 0, size.width(), size.height(),
                             0,
                             CopyFromParent,
                             InputOutput,
                             CopyFromParent,
                             CWEventMask,
                             &windowAttributes);
    XMapWindow(display, m_window);

    while (1) {
        XEvent event;
        XWindowEvent(display, m_window, StructureNotifyMask, &event);
        if (event.type == MapNotify && event.xmap.window == m_window)
            break;
    }
    XSelectInput(display, m_window, NoEventMask);

    XCompositeRedirectWindow(display, m_window, CompositeRedirectManual);

    resize(size);
    resizeLater(); // Force update of the usable area.
}

RedirectedXCompositeWindow::~RedirectedXCompositeWindow()
{
    Display* display = GLContextGLX::sharedDisplay();
    if (m_window)
        XDestroyWindow(display, m_window);
    if (m_parentWindow)
        XDestroyWindow(display, m_parentWindow);
    cleanupPixmapAndPixmapSurface();

    if (m_pendingResizeSourceId)
        g_source_remove(m_pendingResizeSourceId);
}

gboolean RedirectedXCompositeWindow::resizeLaterCallback(RedirectedXCompositeWindow* window)
{
    window->resizeLater();
    return FALSE;
}

void RedirectedXCompositeWindow::resizeLater()
{
    m_usableSize = m_size;
    m_pendingResizeSourceId = 0;
}

void RedirectedXCompositeWindow::resize(const IntSize& size)
{
    // When enlarging a redirected window, for the first render, the newly exposed areas seem
    // to contain uninitialized memory on Intel drivers. To avoid rendering artifacts while
    // resizing, we wait to render those new areas until after a short timeout. Thus, the
    // "usable size" of the window is smaller than the actual size of the window for the first
    // render.
    m_usableSize = size.shrunkTo(m_usableSize);
    if (m_usableSize.width() < size.width() || m_usableSize.height() < size.height()) { // The window is growing.
        // We're being very conservative here. Instead of risking drawing artifacts while doing continuous
        // opaque resizing, we err on the side of having more undrawn areas.
        if (m_pendingResizeSourceId)
            g_source_remove(m_pendingResizeSourceId);
        m_pendingResizeSourceId = g_timeout_add(0, reinterpret_cast<GSourceFunc>(resizeLaterCallback), this);
    }

    Display* display = GLContextGLX::sharedDisplay();
    XResizeWindow(display, m_window, size.width(), size.height());
    glXWaitX();

    // This swap is based on code in Chromium. It tries to work-around a bug in the Intel drivers
    // where a swap is necessary to ensure the front and back buffers are properly resized.
    if (context() == GLContext::getCurrent())
        context()->swapBuffers();

    m_size = size;
    m_needsNewPixmapAfterResize = true;
}

GLContext* RedirectedXCompositeWindow::context()
{
    if (m_context)
        return m_context.get();

    ASSERT(m_window);
    m_context = GLContext::createContextForWindow(m_window, GLContext::sharingContext());
    return m_context.get();
}

void RedirectedXCompositeWindow::cleanupPixmapAndPixmapSurface()
{
    if (!m_pixmap)
        return;

    XFreePixmap(cairo_xlib_surface_get_display(m_surface.get()), m_pixmap);
    m_pixmap = 0;
    m_surface = nullptr;
}

cairo_surface_t* RedirectedXCompositeWindow::cairoSurfaceForWidget(GtkWidget* widget)
{
    if (!m_needsNewPixmapAfterResize && m_surface)
        return m_surface.get();

    m_needsNewPixmapAfterResize = false;

    // It's important that the new pixmap be created with the same Display pointer as the target
    // widget or else drawing speed can be 100x slower.
    Display* newPixmapDisplay = GDK_DISPLAY_XDISPLAY(gtk_widget_get_display(widget));
    Pixmap newPixmap = XCompositeNameWindowPixmap(newPixmapDisplay, m_window);
    if (!newPixmap) {
        cleanupPixmapAndPixmapSurface();
        return 0;
    }

    XWindowAttributes windowAttributes;
    if (!XGetWindowAttributes(newPixmapDisplay, m_window, &windowAttributes)) {
        cleanupPixmapAndPixmapSurface();
        XFreePixmap(newPixmapDisplay, newPixmap);
        return 0;
    }

    RefPtr<cairo_surface_t> newSurface = adoptRef(cairo_xlib_surface_create(newPixmapDisplay, newPixmap,
                                                                            windowAttributes.visual,
                                                                            m_size.width(), m_size.height()));

    // Nvidia drivers seem to prepare their redirected window pixmap asynchronously, so for a few fractions
    // of a second after each resize, while doing continuous resizing (which constantly destroys and creates
    // pixmap window-backings), the pixmap memory is uninitialized. To work around this issue, paint the old
    // pixmap to the new one to properly initialize it.
    if (m_surface) {
        RefPtr<cairo_t> cr = adoptRef(cairo_create(newSurface.get()));
        cairo_set_source_rgb(cr.get(), 1, 1, 1);
        cairo_paint(cr.get());
        cairo_set_source_surface(cr.get(), m_surface.get(), 0, 0);
        cairo_paint(cr.get());
    }

    cleanupPixmapAndPixmapSurface();
    m_pixmap = newPixmap;
    m_surface = newSurface;

    return m_surface.get();
}

} // namespace WebCore

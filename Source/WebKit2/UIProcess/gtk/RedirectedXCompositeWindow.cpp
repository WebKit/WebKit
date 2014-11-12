/*
 * Copyright (C) 2012,2014 Igalia S.L.
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
#include "RedirectedXCompositeWindow.h"

#if USE(TEXTURE_MAPPER_GL) && PLATFORM(X11)

#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <cairo-xlib.h>
#include <gdk/gdkx.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

using namespace WebCore;

namespace WebKit {

class XDamageNotifier {
    WTF_MAKE_NONCOPYABLE(XDamageNotifier);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static int s_damageEventBase;

    XDamageNotifier()
    {
    }

    void add(Window window, std::function<void()> notifyFunction)
    {
        if (m_notifyFunctions.isEmpty())
            gdk_window_add_filter(nullptr, reinterpret_cast<GdkFilterFunc>(&filterXDamageEvent), this);
        m_notifyFunctions.add(window, WTF::move(notifyFunction));
    }

    void remove(Window window)
    {
        m_notifyFunctions.remove(window);
        if (m_notifyFunctions.isEmpty())
            gdk_window_remove_filter(nullptr, reinterpret_cast<GdkFilterFunc>(&filterXDamageEvent), this);
    }

private:
    static GdkFilterReturn filterXDamageEvent(GdkXEvent* event, GdkEvent*, XDamageNotifier* notifier)
    {
        return notifier->filterXEvent(static_cast<XEvent*>(event));
    }

    GdkFilterReturn filterXEvent(XEvent* event) const
    {
        if (event->type != s_damageEventBase + XDamageNotify)
            return GDK_FILTER_CONTINUE;

        XDamageNotifyEvent* damageEvent = reinterpret_cast<XDamageNotifyEvent*>(event);
        if (const auto& notifyFunction = m_notifyFunctions.get(damageEvent->drawable)) {
            notifyFunction();
            XDamageSubtract(event->xany.display, damageEvent->damage, None, None);
            return GDK_FILTER_REMOVE;
        }

        return GDK_FILTER_CONTINUE;
    }

    HashMap<Window, std::function<void()>> m_notifyFunctions;
};

int XDamageNotifier::s_damageEventBase = 0;

static XDamageNotifier& xDamageNotifier()
{
    static NeverDestroyed<XDamageNotifier> notifier;
    return notifier;
}

static bool supportsXDamageAndXComposite(Display* display)
{
    static bool initialized = false;
    static bool hasExtensions = false;

    if (initialized)
        return hasExtensions;

    initialized = true;

    int errorBase;
    if (!XDamageQueryExtension(display, &XDamageNotifier::s_damageEventBase, &errorBase))
        return false;

    int eventBase;
    if (!XCompositeQueryExtension(display, &eventBase, &errorBase))
        return false;

    // We need to support XComposite version 0.2.
    int major, minor;
    XCompositeQueryVersion(display, &major, &minor);
    if (major < 0 || (!major && minor < 2))
        return false;

    hasExtensions = true;
    return true;
}

std::unique_ptr<RedirectedXCompositeWindow> RedirectedXCompositeWindow::create(Display* display, const IntSize& size, std::function<void()> damageNotify)
{
    return supportsXDamageAndXComposite(display) ? std::unique_ptr<RedirectedXCompositeWindow>(new RedirectedXCompositeWindow(display, size, damageNotify)) : nullptr;
}

RedirectedXCompositeWindow::RedirectedXCompositeWindow(Display* display, const IntSize& size, std::function<void()> damageNotify)
    : m_display(display)
    , m_size(size)
    , m_window(0)
    , m_parentWindow(0)
    , m_pixmap(0)
    , m_damage(0)
    , m_needsNewPixmapAfterResize(false)
{
    Screen* screen = DefaultScreenOfDisplay(display);

    // This is based on code from Chromium: src/content/common/gpu/image_transport_surface_linux.cc
    XSetWindowAttributes windowAttributes;
    windowAttributes.override_redirect = True;
    m_parentWindow = XCreateWindow(display,
        RootWindowOfScreen(screen),
        WidthOfScreen(screen) + 1, 0, 1, 1,
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

    xDamageNotifier().add(m_window, WTF::move(damageNotify));

    while (1) {
        XEvent event;
        XWindowEvent(display, m_window, StructureNotifyMask, &event);
        if (event.type == MapNotify && event.xmap.window == m_window)
            break;
    }
    XSelectInput(display, m_window, NoEventMask);
    XCompositeRedirectWindow(display, m_window, CompositeRedirectManual);
    m_damage = XDamageCreate(display, m_window, XDamageReportNonEmpty);
}

RedirectedXCompositeWindow::~RedirectedXCompositeWindow()
{
    ASSERT(m_display);
    ASSERT(m_damage);
    ASSERT(m_window);
    ASSERT(m_parentWindow);

    xDamageNotifier().remove(m_window);

    XDamageDestroy(m_display, m_damage);
    XDestroyWindow(m_display, m_window);
    XDestroyWindow(m_display, m_parentWindow);
    cleanupPixmapAndPixmapSurface();
}

void RedirectedXCompositeWindow::resize(const IntSize& size)
{
    XResizeWindow(m_display, m_window, size.width(), size.height());
    XFlush(m_display);

    m_size = size;
    m_needsNewPixmapAfterResize = true;
}

void RedirectedXCompositeWindow::cleanupPixmapAndPixmapSurface()
{
    if (!m_pixmap)
        return;

    XFreePixmap(m_display, m_pixmap);
    m_pixmap = 0;
    m_surface = nullptr;
}

cairo_surface_t* RedirectedXCompositeWindow::surface()
{
    if (!m_needsNewPixmapAfterResize && m_surface)
        return m_surface.get();

    m_needsNewPixmapAfterResize = false;

    Pixmap newPixmap = XCompositeNameWindowPixmap(m_display, m_window);
    if (!newPixmap) {
        cleanupPixmapAndPixmapSurface();
        return nullptr;
    }

    XWindowAttributes windowAttributes;
    if (!XGetWindowAttributes(m_display, m_window, &windowAttributes)) {
        cleanupPixmapAndPixmapSurface();
        XFreePixmap(m_display, newPixmap);
        return nullptr;
    }

    RefPtr<cairo_surface_t> newSurface = adoptRef(cairo_xlib_surface_create(m_display, newPixmap, windowAttributes.visual, m_size.width(), m_size.height()));

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

#endif // USE(TEXTURE_MAPPER_GL) && PLATFORM(X11)

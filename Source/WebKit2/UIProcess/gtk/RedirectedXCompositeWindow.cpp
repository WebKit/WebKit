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

#if USE(REDIRECTED_XCOMPOSITE_WINDOW)

#include <WebCore/CairoUtilities.h>
#include <WebCore/PlatformDisplayX11.h>
#include <X11/Xlib.h>
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
        m_notifyFunctions.add(window, WTFMove(notifyFunction));
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

static bool supportsXDamageAndXComposite(GdkWindow* window)
{
    static bool initialized = false;
    static bool hasExtensions = false;

    if (initialized)
        return hasExtensions;

    initialized = true;
    Display* display = GDK_DISPLAY_XDISPLAY(gdk_window_get_display(window));

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

std::unique_ptr<RedirectedXCompositeWindow> RedirectedXCompositeWindow::create(GdkWindow* parentWindow, std::function<void()> damageNotify)
{
    ASSERT(GDK_IS_WINDOW(parentWindow));
    return supportsXDamageAndXComposite(parentWindow) ? std::unique_ptr<RedirectedXCompositeWindow>(new RedirectedXCompositeWindow(parentWindow, damageNotify)) : nullptr;
}

RedirectedXCompositeWindow::RedirectedXCompositeWindow(GdkWindow* parentWindow, std::function<void()> damageNotify)
    : m_display(GDK_DISPLAY_XDISPLAY(gdk_window_get_display(parentWindow)))
    , m_needsNewPixmapAfterResize(false)
    , m_deviceScale(1)
{
    ASSERT(downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay()).native() == m_display);
    Screen* screen = DefaultScreenOfDisplay(m_display);

    GdkVisual* visual = gdk_window_get_visual(parentWindow);
    XUniqueColormap colormap(XCreateColormap(m_display, RootWindowOfScreen(screen), GDK_VISUAL_XVISUAL(visual), AllocNone));

    // This is based on code from Chromium: src/content/common/gpu/image_transport_surface_linux.cc
    XSetWindowAttributes windowAttributes;
    windowAttributes.override_redirect = True;
    windowAttributes.colormap = colormap.get();

    // CWBorderPixel must be present when the depth doesn't match the parent's one.
    // See http://cgit.freedesktop.org/xorg/xserver/tree/dix/window.c?id=xorg-server-1.16.0#n703.
    windowAttributes.border_pixel = 0;

    m_parentWindow = XCreateWindow(m_display,
        RootWindowOfScreen(screen),
        WidthOfScreen(screen) + 1, 0, 1, 1,
        0,
        gdk_visual_get_depth(visual),
        InputOutput,
        GDK_VISUAL_XVISUAL(visual),
        CWOverrideRedirect | CWColormap | CWBorderPixel,
        &windowAttributes);
    XMapWindow(m_display, m_parentWindow.get());

    windowAttributes.event_mask = StructureNotifyMask;
    windowAttributes.override_redirect = False;
    // Create the window of at last 1x1 since X doesn't allow to create empty windows.
    m_window = XCreateWindow(m_display,
        m_parentWindow.get(),
        0, 0,
        std::max(1, m_size.width()),
        std::max(1, m_size.height()),
        0,
        CopyFromParent,
        InputOutput,
        CopyFromParent,
        CWEventMask,
        &windowAttributes);
    XMapWindow(m_display, m_window.get());

    xDamageNotifier().add(m_window.get(), WTFMove(damageNotify));

    while (1) {
        XEvent event;
        XWindowEvent(m_display, m_window.get(), StructureNotifyMask, &event);
        if (event.type == MapNotify && event.xmap.window == m_window.get())
            break;
    }
    XSelectInput(m_display, m_window.get(), NoEventMask);
    XCompositeRedirectWindow(m_display, m_window.get(), CompositeRedirectManual);
    m_damage = XDamageCreate(m_display, m_window.get(), XDamageReportNonEmpty);
}

RedirectedXCompositeWindow::~RedirectedXCompositeWindow()
{
    ASSERT(m_display);
    ASSERT(m_damage);
    ASSERT(m_window);
    ASSERT(m_parentWindow);

    xDamageNotifier().remove(m_window.get());

    // Explicitly reset these because we need to ensure it happens in this order.
    m_damage.reset();
    m_window.reset();
    m_parentWindow.reset();
}

void RedirectedXCompositeWindow::resize(const IntSize& size)
{
    IntSize scaledSize(size);
    scaledSize.scale(m_deviceScale);

    if (scaledSize == m_size)
        return;

    // Resize the window to at last 1x1 since X doesn't allow to create empty windows.
    XResizeWindow(m_display, m_window.get(), std::max(1, scaledSize.width()), std::max(1, scaledSize.height()));
    XFlush(m_display);

    m_size = scaledSize;
    m_needsNewPixmapAfterResize = true;
    if (m_size.isEmpty())
        cleanupPixmapAndPixmapSurface();
}

void RedirectedXCompositeWindow::cleanupPixmapAndPixmapSurface()
{
    if (!m_pixmap)
        return;

    m_surface = nullptr;
    m_pixmap.reset();
}

cairo_surface_t* RedirectedXCompositeWindow::surface()
{
    // This should never be called with an empty size (not in Accelerated Compositing mode).
    ASSERT(!m_size.isEmpty());

    if (!m_needsNewPixmapAfterResize && m_surface)
        return m_surface.get();

    m_needsNewPixmapAfterResize = false;

    XUniquePixmap newPixmap(XCompositeNameWindowPixmap(m_display, m_window.get()));
    if (!newPixmap) {
        cleanupPixmapAndPixmapSurface();
        return nullptr;
    }

    XWindowAttributes windowAttributes;
    if (!XGetWindowAttributes(m_display, m_window.get(), &windowAttributes)) {
        cleanupPixmapAndPixmapSurface();
        return nullptr;
    }

    RefPtr<cairo_surface_t> newSurface = adoptRef(cairo_xlib_surface_create(m_display, newPixmap.get(), windowAttributes.visual, m_size.width(), m_size.height()));
    cairoSurfaceSetDeviceScale(newSurface.get(), m_deviceScale, m_deviceScale);

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
    m_pixmap = WTFMove(newPixmap);
    m_surface = newSurface;
    return m_surface.get();
}

} // namespace WebCore

#endif // USE(REDIRECTED_XCOMPOSITE_WINDOW)

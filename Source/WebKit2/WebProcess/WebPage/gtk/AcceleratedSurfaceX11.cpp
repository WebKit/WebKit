/*
 * Copyright (C) 2012-2016 Igalia S.L.
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
#include "AcceleratedSurfaceX11.h"

#if USE(REDIRECTED_XCOMPOSITE_WINDOW)

#include "WebPage.h"
#include <WebCore/PlatformDisplayX11.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <gdk/gdkx.h>
#include <wtf/RunLoop.h>

using namespace WebCore;

namespace WebKit {

std::unique_ptr<AcceleratedSurfaceX11> AcceleratedSurfaceX11::create(WebPage& webPage)
{
    if (!downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay()).supportsXComposite())
        return nullptr;
    return std::unique_ptr<AcceleratedSurfaceX11>(new AcceleratedSurfaceX11(webPage));
}

AcceleratedSurfaceX11::AcceleratedSurfaceX11(WebPage& webPage)
    : AcceleratedSurface(webPage)
    , m_display(downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay()).native())
{
    Screen* screen = DefaultScreenOfDisplay(m_display);

    ASSERT(downcast<PlatformDisplayX11>(PlatformDisplay::sharedDisplay()).native() == m_display);
    GdkVisual* visual = gdk_screen_get_rgba_visual(gdk_screen_get_default());
    if (!visual)
        visual = gdk_screen_get_system_visual(gdk_screen_get_default());

    XUniqueColormap colormap(XCreateColormap(m_display, RootWindowOfScreen(screen), GDK_VISUAL_XVISUAL(visual), AllocNone));

    XSetWindowAttributes windowAttributes;
    windowAttributes.override_redirect = True;
    windowAttributes.colormap = colormap.get();

    // CWBorderPixel must be present when the depth doesn't match the parent's one.
    // See http://cgit.freedesktop.org/xorg/xserver/tree/dix/window.c?id=xorg-server-1.16.0#n703.
    windowAttributes.border_pixel = 0;

    m_parentWindow = XCreateWindow(m_display,
        RootWindowOfScreen(screen),
        -1, -1, 1, 1,
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

    while (1) {
        XEvent event;
        XWindowEvent(m_display, m_window.get(), StructureNotifyMask, &event);
        if (event.type == MapNotify && event.xmap.window == m_window.get())
            break;
    }
    XSelectInput(m_display, m_window.get(), NoEventMask);
    XCompositeRedirectWindow(m_display, m_window.get(), CompositeRedirectManual);
    m_pixmap = XCompositeNameWindowPixmap(m_display, m_window.get());
}

AcceleratedSurfaceX11::~AcceleratedSurfaceX11()
{
    ASSERT(m_display);
    ASSERT(m_window);
    ASSERT(m_parentWindow);

    // Explicitly reset these because we need to ensure it happens in this order.
    m_window.reset();
    m_parentWindow.reset();
}

bool AcceleratedSurfaceX11::resize(const IntSize& size)
{
    if (!AcceleratedSurface::resize(size))
        return false;

    // Resize the window to at last 1x1 since X doesn't allow to create empty windows.
    XResizeWindow(m_display, m_window.get(), std::max(1, m_size.width()), std::max(1, m_size.height()));
    XFlush(m_display);

    // Release the previous pixmap later to give some time to the UI process to update.
    RunLoop::main().dispatchAfter(std::chrono::seconds(5), [pixmap = WTFMove(m_pixmap)] { });
    m_pixmap = XCompositeNameWindowPixmap(m_display, m_window.get());
    return true;
}

} // namespace WebCore

#endif // USE(REDIRECTED_XCOMPOSITE_WINDOW)

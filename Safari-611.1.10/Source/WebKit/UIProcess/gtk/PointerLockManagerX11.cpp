/*
 * Copyright (C) 2019 Igalia S.L.
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
#include "PointerLockManagerX11.h"

#if PLATFORM(X11)

#include "WebPageProxy.h"
#include <X11/Xlib.h>

#if USE(GTK4)
#include <gdk/x11/gdkx.h>
#else
#include <gdk/gdkx.h>
#endif

namespace WebKit {
using namespace WebCore;

PointerLockManagerX11::PointerLockManagerX11(WebPageProxy& webPage, const FloatPoint& position, const FloatPoint& globalPosition, WebMouseEvent::Button button, unsigned short buttons, OptionSet<WebEvent::Modifier> modifiers)
    : PointerLockManager(webPage, position, globalPosition, button, buttons, modifiers)
{
}

bool PointerLockManagerX11::lock()
{
    if (!PointerLockManager::lock())
        return false;

    auto* viewWidget = m_webPage.viewWidget();
    auto* display = gtk_widget_get_display(viewWidget);
    auto* xDisplay = GDK_DISPLAY_XDISPLAY(gtk_widget_get_display(viewWidget));
#if USE(GTK4)
    GRefPtr<GdkCursor> cursor = adoptGRef(gdk_cursor_new_from_name("none", nullptr));
    auto window = GDK_SURFACE_XID(gtk_native_get_surface(gtk_widget_get_native(viewWidget)));
    auto xCursor = gdk_x11_display_get_xcursor(display, cursor.get());
#else
    GRefPtr<GdkCursor> cursor = adoptGRef(gdk_cursor_new_from_name(display, "none"));
    auto window = GDK_WINDOW_XID(gtk_widget_get_window(viewWidget));
    auto xCursor = gdk_x11_cursor_get_xcursor(cursor.get());
#endif
    int eventMask = PointerMotionMask | ButtonReleaseMask | ButtonPressMask | EnterWindowMask | LeaveWindowMask;
    return XGrabPointer(xDisplay, window, true, eventMask, GrabModeAsync, GrabModeAsync, window, xCursor, 0) == GrabSuccess;
}

bool PointerLockManagerX11::unlock()
{
    if (m_device)
        XUngrabPointer(GDK_DISPLAY_XDISPLAY(gtk_widget_get_display(m_webPage.viewWidget())), 0);

    return PointerLockManager::unlock();
}

void PointerLockManagerX11::didReceiveMotionEvent(const FloatPoint& point)
{
    if (point == m_initialPoint)
        return;

    handleMotion(point - m_initialPoint);

    auto* display = GDK_DISPLAY_XDISPLAY(gtk_widget_get_display(m_webPage.viewWidget()));
    XWarpPointer(display, None, XRootWindow(display, 0), 0, 0, 0, 0, m_initialPoint.x(), m_initialPoint.y());
}

} // namespace WebKit

#endif // PLATFORM(WAYLAND)

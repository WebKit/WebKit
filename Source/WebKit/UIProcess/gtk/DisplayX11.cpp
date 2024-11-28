/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "Display.h"

#if PLATFORM(X11)

#include <WebCore/GLContext.h>
#include <WebCore/GLDisplay.h>
#include <WebCore/XErrorTrapper.h>
#include <X11/Xatom.h>
#include <epoxy/egl.h>
#include <gtk/gtk.h>

#if USE(GTK4)
#include <gdk/x11/gdkx.h>
#else
#include <gdk/gdkx.h>
#endif

namespace WebKit {
using namespace WebCore;

bool Display::isX11() const
{
    return GDK_IS_X11_DISPLAY(m_gdkDisplay.get());
}

bool Display::initializeGLDisplayX11() const
{
    if (!isX11())
        return false;

#if USE(GTK4)
    m_glDisplay = GLDisplay::create(gdk_x11_display_get_egl_display(m_gdkDisplay.get()));
    if (m_glDisplay)
        return true;
#endif

    auto* xDisplay = GDK_DISPLAY_XDISPLAY(m_gdkDisplay.get());
    const char* extensions = eglQueryString(nullptr, EGL_EXTENSIONS);
    if (GLContext::isExtensionSupported(extensions, "EGL_KHR_platform_base"))
        m_glDisplay = GLDisplay::create(eglGetPlatformDisplay(EGL_PLATFORM_X11_KHR, xDisplay, nullptr));
    if (!m_glDisplay && GLContext::isExtensionSupported(extensions, "EGL_EXT_platform_base"))
        m_glDisplay = GLDisplay::create(eglGetPlatformDisplayEXT(EGL_PLATFORM_X11_KHR, xDisplay, nullptr));
    if (!m_glDisplay)
        m_glDisplay = GLDisplay::create(eglGetDisplay(xDisplay));

    if (m_glDisplay) {
        m_glDisplayOwned = true;
        return true;
    }

    return false;
}

String Display::accessibilityBusAddressX11() const
{
    auto* xDisplay = GDK_DISPLAY_XDISPLAY(m_gdkDisplay.get());
    Atom atspiBusAtom = XInternAtom(xDisplay, "AT_SPI_BUS", False);
    Atom type;
    int format;
    unsigned long itemCount, bytesAfter;
    unsigned char* data = nullptr;
    XErrorTrapper trapper(xDisplay, XErrorTrapper::Policy::Ignore);
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    XGetWindowProperty(xDisplay, RootWindowOfScreen(DefaultScreenOfDisplay(xDisplay)), atspiBusAtom, 0L, 8192, False, XA_STRING, &type, &format, &itemCount, &bytesAfter, &data);
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

    auto atspiBusAddress = String::fromUTF8(reinterpret_cast<char*>(data));
    if (data)
        XFree(data);

    return atspiBusAddress;
}

} // namespace WebKit

#endif // PLATFORM(X11)

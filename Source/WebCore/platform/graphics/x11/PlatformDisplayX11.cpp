/*
 * Copyright (C) 2015 Igalia S.L
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformDisplayX11.h"

#include "GLContext.h"
#include "XErrorTrapper.h"
#include <cstdlib>

#if PLATFORM(X11)
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#if PLATFORM(GTK)
#if USE(GTK4)
#include <gdk/x11/gdkx.h>
#else
#include <gdk/gdkx.h>
#endif
#endif

#if USE(EGL)
#include <epoxy/egl.h>
#endif

namespace WebCore {

std::unique_ptr<PlatformDisplay> PlatformDisplayX11::create()
{
    Display* display = XOpenDisplay(getenv("DISPLAY"));
    if (!display)
        return nullptr;

    return std::unique_ptr<PlatformDisplayX11>(new PlatformDisplayX11(display));
}

#if PLATFORM(GTK)
std::unique_ptr<PlatformDisplay> PlatformDisplayX11::create(GdkDisplay* display)
{
    return std::unique_ptr<PlatformDisplayX11>(new PlatformDisplayX11(display));
}
#endif

PlatformDisplayX11::PlatformDisplayX11(Display* display)
    : m_display(display)
{
}

#if PLATFORM(GTK)
PlatformDisplayX11::PlatformDisplayX11(GdkDisplay* display)
    : PlatformDisplay(display)
    , m_display(display ? GDK_DISPLAY_XDISPLAY(display) : nullptr)
{
}
#endif

PlatformDisplayX11::~PlatformDisplayX11()
{
#if USE(EGL)
    ASSERT(!m_sharingGLContext);
#endif

#if PLATFORM(GTK)
    bool nativeDisplayOwned = !m_sharedDisplay;
#else
    bool nativeDisplayOwned = true;
#endif
    if (nativeDisplayOwned && m_display)
        XCloseDisplay(m_display);
}

#if PLATFORM(GTK)
void PlatformDisplayX11::sharedDisplayDidClose()
{
    PlatformDisplay::sharedDisplayDidClose();
    m_display = nullptr;
}
#endif

#if USE(EGL)
#if PLATFORM(GTK)
EGLDisplay PlatformDisplayX11::gtkEGLDisplay()
{
    if (m_eglDisplay != EGL_NO_DISPLAY)
        return m_eglDisplayOwned ? EGL_NO_DISPLAY : m_eglDisplay;

    if (!m_sharedDisplay)
        return EGL_NO_DISPLAY;

#if USE(GTK4)
    m_eglDisplay = gdk_x11_display_get_egl_display(m_sharedDisplay.get());
    if (m_eglDisplay != EGL_NO_DISPLAY) {
        m_eglDisplayOwned = false;
        PlatformDisplay::initializeEGLDisplay();
#if ENABLE(WEBGL)
        m_anglePlatform = EGL_PLATFORM_X11_KHR;
        m_angleNativeDisplay = m_display;
#endif
        return m_eglDisplay;
    }
#endif

    return EGL_NO_DISPLAY;
}
#endif

void PlatformDisplayX11::initializeEGLDisplay()
{
#if PLATFORM(GTK)
    if (gtkEGLDisplay() != EGL_NO_DISPLAY)
        return;
#endif

    const char* extensions = eglQueryString(nullptr, EGL_EXTENSIONS);
    if (GLContext::isExtensionSupported(extensions, "EGL_KHR_platform_base"))
        m_eglDisplay = eglGetPlatformDisplay(EGL_PLATFORM_X11_KHR, m_display, nullptr);
    else if (GLContext::isExtensionSupported(extensions, "EGL_EXT_platform_base"))
        m_eglDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_X11_KHR, m_display, nullptr);
    else
        m_eglDisplay = eglGetDisplay(m_display);

    PlatformDisplay::initializeEGLDisplay();

#if ENABLE(WEBGL)
    m_anglePlatform = EGL_PLATFORM_X11_KHR;
    m_angleNativeDisplay = m_display;
#endif
}
#endif // USE(EGL)

#if USE(LCMS)
cmsHPROFILE PlatformDisplayX11::colorProfile() const
{
    if (m_iccProfile)
        return m_iccProfile.get();

    Atom iccAtom = XInternAtom(m_display, "_ICC_PROFILE", False);
    Atom type;
    int format;
    unsigned long itemCount, bytesAfter;
    unsigned char* data = nullptr;
    auto result = XGetWindowProperty(m_display, RootWindowOfScreen(DefaultScreenOfDisplay(m_display)), iccAtom, 0L, ~0L, False, XA_CARDINAL, &type, &format, &itemCount, &bytesAfter, &data);
    if (result == Success && type == XA_CARDINAL && itemCount > 0) {
        unsigned long dataSize;
        switch (format) {
        case 8:
            dataSize = itemCount;
            break;
        case 16:
            dataSize = sizeof(short) * itemCount;
            break;
        case 32:
            dataSize = sizeof(long) * itemCount;
            break;
        default:
            dataSize = 0;
            break;
        }

        if (dataSize)
            m_iccProfile = LCMSProfilePtr(cmsOpenProfileFromMem(data, dataSize));
    }

    if (data)
        XFree(data);

    return m_iccProfile ? m_iccProfile.get() : PlatformDisplay::colorProfile();
}
#endif

#if USE(ATSPI)
String PlatformDisplayX11::platformAccessibilityBusAddress() const
{
    Atom atspiBusAtom = XInternAtom(m_display, "AT_SPI_BUS", False);
    Atom type;
    int format;
    unsigned long itemCount, bytesAfter;
    unsigned char* data = nullptr;
    XErrorTrapper trapper(m_display, XErrorTrapper::Policy::Ignore);
    XGetWindowProperty(m_display, RootWindowOfScreen(DefaultScreenOfDisplay(m_display)), atspiBusAtom, 0L, 8192, False, XA_STRING, &type, &format, &itemCount, &bytesAfter, &data);

    String atspiBusAddress = String::fromUTF8(reinterpret_cast<char*>(data));
    if (data)
        XFree(data);

    return atspiBusAddress;
}
#endif

} // namespace WebCore

#endif // PLATFORM(X11)


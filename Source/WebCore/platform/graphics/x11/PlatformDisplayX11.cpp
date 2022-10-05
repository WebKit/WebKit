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
#include <X11/extensions/Xcomposite.h>
#if PLATFORM(GTK)
#include <X11/Xutil.h>
#include <X11/extensions/Xdamage.h>
#if USE(GTK4)
#include <gdk/x11/gdkx.h>
#else
#include <gdk/gdkx.h>
#endif
#endif

#if USE(EGL)
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

#if USE(GLX)
#include <GL/glx.h>
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

static inline void clearSharingGLContextAtExit()
{
#if USE(GLX)
    // In X11 only one PlatformDisplay instance is allowed per process which is the sharedDisplay one.
    // We install an atexit handler to clear the sharing GL context to ensure it's released before
    // the constructor is called, because clearing the context in X11 requires to access PlatformDisplay::sharedDisplay()
    // and calling it from its constructor causes issues in some systems. See https://bugs.webkit.org/show_bug.cgi?id=238494.
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        std::atexit([] {
            PlatformDisplay::sharedDisplay().clearSharingGLContext();
        });
    });
#endif
}

PlatformDisplayX11::PlatformDisplayX11(Display* display)
    : m_display(display)
{
    clearSharingGLContextAtExit();
}

#if PLATFORM(GTK)
PlatformDisplayX11::PlatformDisplayX11(GdkDisplay* display)
    : PlatformDisplay(display)
    , m_display(display ? GDK_DISPLAY_XDISPLAY(display) : nullptr)
{
    clearSharingGLContextAtExit();
}
#endif

PlatformDisplayX11::~PlatformDisplayX11()
{
#if USE(EGL) || USE(GLX)
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
void PlatformDisplayX11::initializeEGLDisplay()
{
#if defined(EGL_KHR_platform_x11)
    const char* extensions = eglQueryString(nullptr, EGL_EXTENSIONS);
    if (GLContext::isExtensionSupported(extensions, "EGL_KHR_platform_base")) {
        if (auto* getPlatformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplay")))
            m_eglDisplay = getPlatformDisplay(EGL_PLATFORM_X11_KHR, m_display, nullptr);
    } else if (GLContext::isExtensionSupported(extensions, "EGL_EXT_platform_base")) {
        if (auto* getPlatformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT")))
            m_eglDisplay = getPlatformDisplay(EGL_PLATFORM_X11_KHR, m_display, nullptr);
    } else
#endif
        m_eglDisplay = eglGetDisplay(m_display);

    PlatformDisplay::initializeEGLDisplay();
}
#endif // USE(EGL)

bool PlatformDisplayX11::supportsXComposite() const
{
    if (!m_supportsXComposite) {
        if (m_display) {
            int eventBase, errorBase;
            m_supportsXComposite = XCompositeQueryExtension(m_display, &eventBase, &errorBase);
        } else
            m_supportsXComposite = false;
    }
    return m_supportsXComposite.value();
}

bool PlatformDisplayX11::supportsXDamage(std::optional<int>& damageEventBase, std::optional<int>& damageErrorBase) const
{
    if (!m_supportsXDamage) {
        m_supportsXDamage = false;
#if PLATFORM(GTK)
        if (m_display) {
            int eventBase, errorBase;
            m_supportsXDamage = XDamageQueryExtension(m_display, &eventBase, &errorBase);
            if (m_supportsXDamage.value()) {
                m_damageEventBase = eventBase;
                m_damageErrorBase = errorBase;
            }
        }
#endif
    }

    damageEventBase = m_damageEventBase;
    damageErrorBase = m_damageErrorBase;
    return m_supportsXDamage.value();
}

bool PlatformDisplayX11::supportsGLX(std::optional<int>& glxErrorBase) const
{
#if USE(GLX)
    if (!m_supportsGLX) {
        m_supportsGLX = false;
        if (m_display) {
            int eventBase, errorBase;
            m_supportsGLX = glXQueryExtension(m_display, &errorBase, &eventBase);
            if (m_supportsGLX.value())
                m_glxErrorBase = errorBase;
        }
    }

    glxErrorBase = m_glxErrorBase;
    return m_supportsGLX.value();
#else
    UNUSED_PARAM(glxErrorBase);
    return false;
#endif
}

void* PlatformDisplayX11::visual() const
{
    if (m_visual)
        return m_visual;

    XVisualInfo visualTemplate;
    visualTemplate.screen = DefaultScreen(m_display);

    int visualCount = 0;
    XVisualInfo* visualInfo = XGetVisualInfo(m_display, VisualScreenMask, &visualTemplate, &visualCount);
    for (int i = 0; i < visualCount; ++i) {
        auto& info = visualInfo[i];
        if (info.depth == 32 && info.red_mask == 0xff0000 && info.green_mask == 0x00ff00 && info.blue_mask == 0x0000ff) {
            m_visual = info.visual;
            break;
        }
    }
    XFree(visualInfo);

    if (!m_visual)
        m_visual = DefaultVisual(m_display, DefaultScreen(m_display));

    return m_visual;
}

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


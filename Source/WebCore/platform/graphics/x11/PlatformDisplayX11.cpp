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

#if PLATFORM(X11)
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#if PLATFORM(GTK)
#include <X11/extensions/Xdamage.h>
#endif

#if USE(EGL)
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

namespace WebCore {

std::unique_ptr<PlatformDisplay> PlatformDisplayX11::create()
{
    Display* display = XOpenDisplay(getenv("DISPLAY"));
    if (!display)
        return nullptr;

    return std::unique_ptr<PlatformDisplayX11>(new PlatformDisplayX11(display, NativeDisplayOwned::Yes));
}

std::unique_ptr<PlatformDisplay> PlatformDisplayX11::create(Display* display)
{
    return std::unique_ptr<PlatformDisplayX11>(new PlatformDisplayX11(display, NativeDisplayOwned::No));
}

PlatformDisplayX11::PlatformDisplayX11(Display* display, NativeDisplayOwned displayOwned)
    : PlatformDisplay(displayOwned)
    , m_display(display)
{
}

PlatformDisplayX11::~PlatformDisplayX11()
{
#if USE(EGL) || USE(GLX)
    // Clear the sharing context before releasing the display.
    m_sharingGLContext = nullptr;
#endif
    if (m_nativeDisplayOwned == NativeDisplayOwned::Yes)
        XCloseDisplay(m_display);
}

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

bool PlatformDisplayX11::supportsXDamage(Optional<int>& damageEventBase, Optional<int>& damageErrorBase) const
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

} // namespace WebCore

#endif // PLATFORM(X11)


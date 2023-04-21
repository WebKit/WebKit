/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "PlatformDisplayWayland.h"

#if PLATFORM(WAYLAND)

#include "GLContext.h"
#include <cstring>
#include <wtf/Assertions.h>

// These includes need to be in this order because wayland-egl.h defines WL_EGL_PLATFORM
// and egl.h checks that to decide whether it's Wayland platform.
#include <wayland-egl.h>
#include <epoxy/egl.h>

#if PLATFORM(GTK)
#if USE(GTK4)
#include <gdk/wayland/gdkwayland.h>
#else
#include <gdk/gdkwayland.h>
#endif
#endif

namespace WebCore {

std::unique_ptr<PlatformDisplay> PlatformDisplayWayland::create()
{
    struct wl_display* display = wl_display_connect(nullptr);
    if (!display)
        return nullptr;

    return makeUnique<PlatformDisplayWayland>(display);
}

#if PLATFORM(GTK)
std::unique_ptr<PlatformDisplay> PlatformDisplayWayland::create(GdkDisplay* display)
{
    return makeUnique<PlatformDisplayWayland>(display);
}
#endif

PlatformDisplayWayland::PlatformDisplayWayland(struct wl_display* display)
    : m_display(display)
{
}

#if PLATFORM(GTK)
PlatformDisplayWayland::PlatformDisplayWayland(GdkDisplay* display)
    : PlatformDisplay(display)
    , m_display(display ? gdk_wayland_display_get_wl_display(display) : nullptr)
{
}
#endif

PlatformDisplayWayland::~PlatformDisplayWayland()
{
#if PLATFORM(GTK)
    bool nativeDisplayOwned = !m_sharedDisplay;
#else
    bool nativeDisplayOwned = true;
#endif

    if (nativeDisplayOwned && m_display)
        wl_display_disconnect(m_display);
}

#if PLATFORM(GTK)
void PlatformDisplayWayland::sharedDisplayDidClose()
{
    PlatformDisplay::sharedDisplayDidClose();
    m_display = nullptr;
}
#endif

void PlatformDisplayWayland::initializeEGLDisplay()
{
    if (!m_display)
        return;

    const char* extensions = eglQueryString(nullptr, EGL_EXTENSIONS);
    if (GLContext::isExtensionSupported(extensions, "EGL_KHR_platform_base"))
        m_eglDisplay = eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, m_display, nullptr);

    if (m_eglDisplay == EGL_NO_DISPLAY && GLContext::isExtensionSupported(extensions, "EGL_EXT_platform_base"))
        m_eglDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_WAYLAND_EXT, m_display, nullptr);

    if (m_eglDisplay == EGL_NO_DISPLAY)
        m_eglDisplay = eglGetDisplay(m_display);

    PlatformDisplay::initializeEGLDisplay();
}

} // namespace WebCore

#endif // PLATFORM(WAYLAND)

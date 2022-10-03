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

#include "GLContextEGL.h"
#include <cstring>
// These includes need to be in this order because wayland-egl.h defines WL_EGL_PLATFORM
// and egl.h checks that to decide whether it's Wayland platform.
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <wtf/Assertions.h>

#if PLATFORM(GTK)
#if USE(GTK4)
#include <gdk/wayland/gdkwayland.h>
#else
#include <gdk/gdkwayland.h>
#endif
#endif

namespace WebCore {

const struct wl_registry_listener PlatformDisplayWayland::s_registryListener = {
    // globalCallback
    [](void* data, struct wl_registry*, uint32_t name, const char* interface, uint32_t) {
        static_cast<PlatformDisplayWayland*>(data)->registryGlobal(interface, name);
    },
    // globalRemoveCallback
    [](void*, struct wl_registry*, uint32_t)
    {
    }
};

std::unique_ptr<PlatformDisplay> PlatformDisplayWayland::create()
{
    struct wl_display* display = wl_display_connect(nullptr);
    if (!display)
        return nullptr;

    auto platformDisplay = std::unique_ptr<PlatformDisplayWayland>(new PlatformDisplayWayland(display));
    platformDisplay->initialize();
    return platformDisplay;
}

#if PLATFORM(GTK)
std::unique_ptr<PlatformDisplay> PlatformDisplayWayland::create(GdkDisplay* display)
{
    auto platformDisplay = std::unique_ptr<PlatformDisplayWayland>(new PlatformDisplayWayland(display));
    platformDisplay->initialize();
    return platformDisplay;
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

    if (nativeDisplayOwned && m_display) {
        m_compositor = nullptr;
        m_registry = nullptr;
        wl_display_disconnect(m_display);
    }
}

#if PLATFORM(GTK)
void PlatformDisplayWayland::sharedDisplayDidClose()
{
    PlatformDisplay::sharedDisplayDidClose();
    m_display = nullptr;
}
#endif

void PlatformDisplayWayland::initialize()
{
    if (!m_display)
        return;

    m_registry.reset(wl_display_get_registry(m_display));
    wl_registry_add_listener(m_registry.get(), &s_registryListener, this);
    wl_display_roundtrip(m_display);

#if defined(EGL_KHR_platform_wayland) || defined(EGL_EXT_platform_wayland)
    const char* extensions = eglQueryString(nullptr, EGL_EXTENSIONS);
#if defined(EGL_KHR_platform_wayland)
    if (GLContext::isExtensionSupported(extensions, "EGL_KHR_platform_base")) {
        if (auto* getPlatformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplay")))
            m_eglDisplay = getPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, m_display, nullptr);
    }
#endif
#if defined(EGL_EXT_platform_wayland)
    if (m_eglDisplay == EGL_NO_DISPLAY && GLContext::isExtensionSupported(extensions, "EGL_EXT_platform_base")) {
        if (auto* getPlatformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT")))
            m_eglDisplay = getPlatformDisplay(EGL_PLATFORM_WAYLAND_EXT, m_display, nullptr);
    }
#endif
#endif
    if (m_eglDisplay == EGL_NO_DISPLAY)
        m_eglDisplay = eglGetDisplay(m_display);

    PlatformDisplay::initializeEGLDisplay();
}

void PlatformDisplayWayland::registryGlobal(const char* interface, uint32_t name)
{
    if (!std::strcmp(interface, "wl_compositor"))
        m_compositor.reset(static_cast<struct wl_compositor*>(wl_registry_bind(m_registry.get(), name, &wl_compositor_interface, 1)));
}

WlUniquePtr<struct wl_surface> PlatformDisplayWayland::createSurface() const
{
    if (!m_compositor)
        return nullptr;

    return WlUniquePtr<struct wl_surface>(wl_compositor_create_surface(m_compositor.get()));
}

} // namespace WebCore

#endif // PLATFORM(WAYLAND)

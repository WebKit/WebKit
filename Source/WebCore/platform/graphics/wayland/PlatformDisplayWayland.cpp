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
#include "WaylandSurface.h"
#include <cstring>
#include <glib.h>
#include <wtf/Assertions.h>

namespace WebCore {

const struct wl_registry_listener PlatformDisplayWayland::m_registryListener = {
    PlatformDisplayWayland::globalCallback,
    PlatformDisplayWayland::globalRemoveCallback
};

void PlatformDisplayWayland::globalCallback(void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t)
{
    auto display = static_cast<PlatformDisplayWayland*>(data);
    if (!std::strcmp(interface, "wl_compositor"))
        display->m_compositor = static_cast<struct wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, 1));
    else if (!std::strcmp(interface, "wl_webkitgtk"))
        display->m_webkitgtk = static_cast<struct wl_webkitgtk*>(wl_registry_bind(registry, name, &wl_webkitgtk_interface, 1));
}

void PlatformDisplayWayland::globalRemoveCallback(void*, struct wl_registry*, uint32_t)
{
    // FIXME: if this can happen without the UI Process getting shut down
    // we should probably destroy our cached display instance.
}

std::unique_ptr<PlatformDisplayWayland> PlatformDisplayWayland::create()
{
    struct wl_display* wlDisplay = wl_display_connect(nullptr);
    if (!wlDisplay) {
        WTFLogAlways("PlatformDisplayWayland initialization: failed to connect to the Wayland server socket. Check your WAYLAND_DISPLAY or WAYLAND_SOCKET environment variables.");
        return nullptr;
    }

    auto display = std::unique_ptr<PlatformDisplayWayland>(new PlatformDisplayWayland(wlDisplay));
    if (!display->isInitialized()) {
        WTFLogAlways("PlatformDisplayWayland initialization: failed to complete the initialization of the display.");
        return nullptr;
    }

    return display;
}

PlatformDisplayWayland::PlatformDisplayWayland(struct wl_display* wlDisplay)
    : m_display(wlDisplay)
    , m_registry(wl_display_get_registry(m_display))
    , m_eglConfigChosen(false)
{
    wl_registry_add_listener(m_registry, &m_registryListener, this);
    wl_display_roundtrip(m_display);

    static const EGLint configAttributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_ALPHA_SIZE, 1,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    m_eglDisplay = eglGetDisplay(m_display);
    PlatformDisplay::initializeEGLDisplay();
    if (m_eglDisplay == EGL_NO_DISPLAY)
        return;

    EGLint numberOfConfigs;
    if (!eglChooseConfig(m_eglDisplay, configAttributes, &m_eglConfig, 1, &numberOfConfigs) || numberOfConfigs != 1) {
        g_warning("PlatformDisplayWayland initialization: failed to find the desired EGL configuration.");
        return;
    }

    m_eglConfigChosen = true;
}

PlatformDisplayWayland::~PlatformDisplayWayland()
{
    if (m_webkitgtk)
        wl_webkitgtk_destroy(m_webkitgtk);
    if (m_compositor)
        wl_compositor_destroy(m_compositor);
    if (m_registry)
        wl_registry_destroy(m_registry);
    if (m_display)
        wl_display_disconnect(m_display);
}

std::unique_ptr<WaylandSurface> PlatformDisplayWayland::createSurface(const IntSize& size, int widgetId)
{
    struct wl_surface* wlSurface = wl_compositor_create_surface(m_compositor);
    // We keep the minimum size at 1x1px since Mesa returns null values in wl_egl_window_create() for zero width or height.
    EGLNativeWindowType nativeWindow = wl_egl_window_create(wlSurface, std::max(1, size.width()), std::max(1, size.height()));

    wl_webkitgtk_set_surface_for_widget(m_webkitgtk, wlSurface, widgetId);
    wl_display_roundtrip(m_display);

    return std::make_unique<WaylandSurface>(wlSurface, nativeWindow);
}

std::unique_ptr<GLContextEGL> PlatformDisplayWayland::createSharingGLContext()
{
    class OffscreenContextData : public GLContext::Data {
    public:
        virtual ~OffscreenContextData()
        {
            wl_egl_window_destroy(nativeWindow);
            wl_surface_destroy(surface);
        }

        struct wl_surface* surface;
        EGLNativeWindowType nativeWindow;
    };

    auto contextData = std::make_unique<OffscreenContextData>();
    contextData->surface = wl_compositor_create_surface(m_compositor);
    contextData->nativeWindow = wl_egl_window_create(contextData->surface, 1, 1);

    auto nativeWindow = contextData->nativeWindow;
    return GLContextEGL::createWindowContext(nativeWindow, nullptr, WTFMove(contextData));
}

} // namespace WebCore

#endif // PLATFORM(WAYLAND)

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

#ifndef  PlatformDisplayWayland_h
#define  PlatformDisplayWayland_h

#if PLATFORM(WAYLAND)

#include "PlatformDisplay.h"
#include "WebKitGtkWaylandClientProtocol.h"
#include <memory>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>

namespace WebCore {

class GLContextEGL;
class IntSize;
class WaylandSurface;

class PlatformDisplayWayland final: public PlatformDisplay {
public:
    static std::unique_ptr<PlatformDisplayWayland> create();
    virtual ~PlatformDisplayWayland();

    struct wl_display* native() const { return m_display; }

    std::unique_ptr<WaylandSurface> createSurface(const IntSize&, int widgetID);

    std::unique_ptr<GLContextEGL> createSharingGLContext();

private:
    static const struct wl_registry_listener m_registryListener;
    static void globalCallback(void* data, struct wl_registry*, uint32_t name, const char* interface, uint32_t version);
    static void globalRemoveCallback(void* data, struct wl_registry*, uint32_t name);

    PlatformDisplayWayland(struct wl_display*);

    // FIXME: This should check also for m_webkitgtk once the UIProcess embedded Wayland subcompositer is implemented.
    bool isInitialized() { return m_compositor && m_eglDisplay != EGL_NO_DISPLAY && m_eglConfigChosen; }

    Type type() const override { return PlatformDisplay::Type::Wayland; }

    struct wl_display* m_display;
    struct wl_registry* m_registry;
    struct wl_compositor* m_compositor;
    struct wl_webkitgtk* m_webkitgtk;

    EGLConfig m_eglConfig;
    bool m_eglConfigChosen;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_PLATFORM_DISPLAY(PlatformDisplayWayland, Wayland)

#endif // PLATFORM(WAYLAND)

#endif // PlatformDisplayWayland_h

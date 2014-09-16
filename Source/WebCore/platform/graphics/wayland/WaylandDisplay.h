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

#ifndef  WaylandDisplay_h
#define  WaylandDisplay_h

#if PLATFORM(WAYLAND)

#include "WebKitGtkWaylandClientProtocol.h"
#include <memory>
#include <wayland-client.h>
#include <wtf/PassOwnPtr.h>

#include <wayland-egl.h>
#include <EGL/egl.h>

namespace WebCore {

class GLContextEGL;
class IntSize;
class WaylandSurface;

class WaylandDisplay {
public:
    static WaylandDisplay* instance();

    struct wl_display* nativeDisplay() const { return m_display; }
    EGLDisplay eglDisplay() const { return m_eglDisplay; }

    std::unique_ptr<WaylandSurface> createSurface(const IntSize&, int widgetID);

    PassOwnPtr<GLContextEGL> createSharingGLContext();

private:
    static const struct wl_registry_listener m_registryListener;
    static void globalCallback(void* data, struct wl_registry*, uint32_t name, const char* interface, uint32_t version);
    static void globalRemoveCallback(void* data, struct wl_registry*, uint32_t name);

    WaylandDisplay(struct wl_display*);
    bool isInitialized() { return m_compositor && m_webkitgtk && m_eglDisplay != EGL_NO_DISPLAY && m_eglConfigChosen; }

    struct wl_display* m_display;
    struct wl_registry* m_registry;
    struct wl_compositor* m_compositor;
    struct wl_webkitgtk* m_webkitgtk;

    EGLDisplay m_eglDisplay;
    EGLConfig m_eglConfig;
    bool m_eglConfigChosen;
};

} // namespace WebCore

#endif // PLATFORM(WAYLAND)

#endif // WaylandDisplay_h

/*
 * Copyright (C) 2016 Igalia S.L.
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
#include "AcceleratedSurfaceWayland.h"

#if PLATFORM(WAYLAND)

#include "WebKit2WaylandClientProtocol.h"
#include "WebProcess.h"
#include <WebCore/PlatformDisplayWayland.h>
#include <cstring>
#include <wayland-egl.h>
#include <wtf/NeverDestroyed.h>

using namespace WebCore;

namespace WebKit {

class WaylandCompositorDisplay final : public PlatformDisplayWayland {
    WTF_MAKE_NONCOPYABLE(WaylandCompositorDisplay);
public:
    static std::unique_ptr<WaylandCompositorDisplay> create()
    {
        struct wl_display* display = wl_display_connect(WebProcess::singleton().waylandCompositorDisplayName().utf8().data());
        if (!display) {
            WTFLogAlways("PlatformDisplayWayland initialization: failed to connect to the Wayland display: %s", WebProcess::singleton().waylandCompositorDisplayName().utf8().data());
            return nullptr;
        }

        return std::unique_ptr<WaylandCompositorDisplay>(new WaylandCompositorDisplay(display));
    }

    void bindSurfaceToPage(struct wl_surface* surface, WebPage& page)
    {
        if (!m_webkitgtk)
            return;

        wl_webkitgtk_bind_surface_to_page(reinterpret_cast<struct wl_webkitgtk*>(m_webkitgtk.get()), surface, page.pageID());
        wl_display_roundtrip(m_display);
    }

private:
    WaylandCompositorDisplay(struct wl_display* display)
    {
        initialize(display);
        PlatformDisplay::setSharedDisplayForCompositing(*this);
    }

    void registryGlobal(const char* interface, uint32_t name) override
    {
        PlatformDisplayWayland::registryGlobal(interface, name);
        if (!std::strcmp(interface, "wl_webkitgtk"))
            m_webkitgtk.reset(static_cast<struct wl_proxy*>(wl_registry_bind(m_registry.get(), name, &wl_webkitgtk_interface, 1)));
    }

    WlUniquePtr<struct wl_proxy> m_webkitgtk;
};

static std::unique_ptr<WaylandCompositorDisplay>& waylandCompositorDisplay()
{
    static NeverDestroyed<std::unique_ptr<WaylandCompositorDisplay>> waylandDisplay(WaylandCompositorDisplay::create());
    return waylandDisplay;
}

std::unique_ptr<AcceleratedSurfaceWayland> AcceleratedSurfaceWayland::create(WebPage& webPage)
{
    return waylandCompositorDisplay() ? std::unique_ptr<AcceleratedSurfaceWayland>(new AcceleratedSurfaceWayland(webPage)) : nullptr;
}

AcceleratedSurfaceWayland::AcceleratedSurfaceWayland(WebPage& webPage)
    : AcceleratedSurface(webPage)
    , m_surface(waylandCompositorDisplay()->createSurface())
    , m_window(wl_egl_window_create(m_surface.get(), std::max(1, m_size.width()), std::max(1, m_size.height())))
{
    waylandCompositorDisplay()->bindSurfaceToPage(m_surface.get(), m_webPage);
}

AcceleratedSurfaceWayland::~AcceleratedSurfaceWayland()
{
    wl_egl_window_destroy(m_window);
}

bool AcceleratedSurfaceWayland::resize(const IntSize& size)
{
    if (!AcceleratedSurface::resize(size))
        return false;

    wl_egl_window_resize(m_window, m_size.width(), m_size.height(), 0, 0);
    return true;
}

} // namespace WebKit

#endif // PLATFORM(WAYLAND)

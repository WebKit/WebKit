/*
 * Copyright (C) 2017 Igalia S.L.
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
#include "WaylandCompositorDisplay.h"

#if PLATFORM(WAYLAND)

#include "WebKitWaylandClientProtocol.h"
#include "WebPage.h"

namespace WebKit {
using namespace WebCore;

std::unique_ptr<WaylandCompositorDisplay> WaylandCompositorDisplay::create(const String& displayName)
{
    if (displayName.isNull())
        return nullptr;

    if (PlatformDisplay::sharedDisplay().type() != PlatformDisplay::Type::Wayland)
        return nullptr;

    struct wl_display* display = wl_display_connect(displayName.utf8().data());
    if (!display) {
        WTFLogAlways("WaylandCompositorDisplay initialization: failed to connect to the Wayland display: %s", displayName.utf8().data());
        return nullptr;
    }

    auto compositorDisplay = std::unique_ptr<WaylandCompositorDisplay>(new WaylandCompositorDisplay(display));
    compositorDisplay->initialize();
    return compositorDisplay;
}

void WaylandCompositorDisplay::bindSurfaceToPage(struct wl_surface* surface, WebPage& page)
{
    if (!m_webkitgtk)
        return;

    wl_webkitgtk_bind_surface_to_page(reinterpret_cast<struct wl_webkitgtk*>(m_webkitgtk.get()), surface, page.identifier().toUInt64());
    wl_display_roundtrip(m_display);
}

WaylandCompositorDisplay::WaylandCompositorDisplay(struct wl_display* display)
    : PlatformDisplayWayland(display, NativeDisplayOwned::Yes)
{
    PlatformDisplay::setSharedDisplayForCompositing(*this);
}

void WaylandCompositorDisplay::registryGlobal(const char* interface, uint32_t name)
{
    PlatformDisplayWayland::registryGlobal(interface, name);
    if (!std::strcmp(interface, "wl_webkitgtk"))
        m_webkitgtk.reset(static_cast<struct wl_proxy*>(wl_registry_bind(m_registry.get(), name, &wl_webkitgtk_interface, 1)));
}

} // namespace WebKit

#endif // PLATFORM(WAYLAND)

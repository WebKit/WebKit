/*
 * Copyright (C) 2019 Igalia S.L.
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
#include "PointerLockManagerWayland.h"

#if PLATFORM(WAYLAND)

#include "WebPageProxy.h"
#include "pointer-constraints-unstable-v1-client-protocol.h"
#include <WebCore/WlUniquePtr.h>
#include <gdk/gdkwayland.h>
#include <gtk/gtk.h>

namespace WebKit {
using namespace WebCore;

PointerLockManagerWayland::PointerLockManagerWayland(WebPageProxy& webPage, const GdkEvent* event)
    : PointerLockManager(webPage, event)
{
    auto* display = gdk_wayland_display_get_wl_display(gtk_widget_get_display(m_webPage.viewWidget()));
    WlUniquePtr<struct wl_registry> registry(wl_display_get_registry(display));
    wl_registry_add_listener(registry.get(), &s_registryListener, this);
    wl_display_roundtrip(display);
}

PointerLockManagerWayland::~PointerLockManagerWayland()
{
    g_clear_pointer(&m_relativePointerManager, zwp_relative_pointer_manager_v1_destroy);
    g_clear_pointer(&m_pointerConstraints, zwp_pointer_constraints_v1_destroy);
}

const struct wl_registry_listener PointerLockManagerWayland::s_registryListener = {
    // global
    [](void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t) {
        if (!g_strcmp0(interface, "zwp_pointer_constraints_v1")) {
            auto& manager = *reinterpret_cast<PointerLockManagerWayland*>(data);
            manager.m_pointerConstraints = static_cast<struct zwp_pointer_constraints_v1*>(wl_registry_bind(registry, name, &zwp_pointer_constraints_v1_interface, 1));
        } else if (!g_strcmp0(interface, "zwp_relative_pointer_manager_v1")) {
            auto& manager = *reinterpret_cast<PointerLockManagerWayland*>(data);
            manager.m_relativePointerManager = static_cast<struct zwp_relative_pointer_manager_v1*>(wl_registry_bind(registry, name, &zwp_relative_pointer_manager_v1_interface, 1));
        }
    },
    // global_remove
    [](void*, struct wl_registry*, uint32_t)
    {
    }
};

const struct zwp_relative_pointer_v1_listener PointerLockManagerWayland::s_relativePointerListener = {
    // relative_motion
    [](void* data, struct zwp_relative_pointer_v1*, uint32_t, uint32_t, wl_fixed_t, wl_fixed_t, wl_fixed_t deltaX, wl_fixed_t deltaY) {
        auto& manager = *reinterpret_cast<PointerLockManagerWayland*>(data);
        manager.handleMotion({ wl_fixed_to_int(deltaX), wl_fixed_to_int(deltaY) });
    }
};

bool PointerLockManagerWayland::lock()
{
    if (!m_pointerConstraints || !m_relativePointerManager)
        return false;

    if (!PointerLockManager::lock())
        return false;

    auto* pointer = gdk_wayland_device_get_wl_pointer(m_device);

    RELEASE_ASSERT(!m_relativePointer);
    m_relativePointer = zwp_relative_pointer_manager_v1_get_relative_pointer(m_relativePointerManager, pointer);
    zwp_relative_pointer_v1_add_listener(m_relativePointer, &s_relativePointerListener, this);

    RELEASE_ASSERT(!m_lockedPointer);
    auto* surface = gdk_wayland_window_get_wl_surface(gtk_widget_get_window(m_webPage.viewWidget()));
    m_lockedPointer = zwp_pointer_constraints_v1_lock_pointer(m_pointerConstraints, surface, pointer, nullptr, ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
    return true;
}

bool PointerLockManagerWayland::unlock()
{
    g_clear_pointer(&m_relativePointer, zwp_relative_pointer_v1_destroy);
    g_clear_pointer(&m_lockedPointer, zwp_locked_pointer_v1_destroy);

    return PointerLockManager::unlock();
}

} // namespace WebKit

#endif // PLATFORM(WAYLAND)

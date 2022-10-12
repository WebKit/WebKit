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
#include <gtk/gtk.h>

#if USE(GTK4)
#include <gdk/wayland/gdkwayland.h>
#else
#include <gdk/gdkwayland.h>
#endif

namespace WebKit {
using namespace WebCore;

PointerLockManagerWayland::PointerLockManagerWayland(WebPageProxy& webPage, const FloatPoint& position, const FloatPoint& globalPosition, WebMouseEventButton button, unsigned short buttons, OptionSet<WebEventModifier> modifiers)
    : PointerLockManager(webPage, position, globalPosition, button, buttons, modifiers)
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
        manager.handleMotion(FloatSize(wl_fixed_to_double(deltaX), wl_fixed_to_double(deltaY)));
    }
};

bool PointerLockManagerWayland::lock()
{
    if (!m_pointerConstraints || !m_relativePointerManager)
        return false;

    if (!PointerLockManager::lock())
        return false;

    auto* viewWidget = m_webPage.viewWidget();
#if USE(GTK4)
    GRefPtr<GdkCursor> cursor = adoptGRef(gdk_cursor_new_from_name("none", nullptr));
    gtk_widget_set_cursor(viewWidget, cursor.get());
#else
    GRefPtr<GdkCursor> cursor = adoptGRef(gdk_cursor_new_from_name(gtk_widget_get_display(viewWidget), "none"));
    gdk_window_set_cursor(gtk_widget_get_window(viewWidget), cursor.get());
#endif

    auto* pointer = gdk_wayland_device_get_wl_pointer(m_device);

    ASSERT(!m_relativePointer);
    m_relativePointer = zwp_relative_pointer_manager_v1_get_relative_pointer(m_relativePointerManager, pointer);
    zwp_relative_pointer_v1_add_listener(m_relativePointer, &s_relativePointerListener, this);

    ASSERT(!m_lockedPointer);
#if USE(GTK4)
    auto* surface = gdk_wayland_surface_get_wl_surface(gtk_native_get_surface(gtk_widget_get_native(viewWidget)));
#else
    auto* surface = gdk_wayland_window_get_wl_surface(gtk_widget_get_window(viewWidget));
#endif
    m_lockedPointer = zwp_pointer_constraints_v1_lock_pointer(m_pointerConstraints, surface, pointer, nullptr, ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);

    return true;
}

bool PointerLockManagerWayland::unlock()
{
    g_clear_pointer(&m_relativePointer, zwp_relative_pointer_v1_destroy);
    g_clear_pointer(&m_lockedPointer, zwp_locked_pointer_v1_destroy);

#if USE(GTK4)
    gtk_widget_set_cursor(m_webPage.viewWidget(), nullptr);
#else
    gdk_window_set_cursor(gtk_widget_get_window(m_webPage.viewWidget()), nullptr);
#endif

    return PointerLockManager::unlock();
}

} // namespace WebKit

#endif // PLATFORM(WAYLAND)

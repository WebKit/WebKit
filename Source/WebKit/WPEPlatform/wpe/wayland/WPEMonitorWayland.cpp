/*
 * Copyright (C) 2024 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEMonitorWayland.h"

#include <wtf/glib/WTFGType.h>

/**
 * WPEMonitorWayland:
 *
 */
struct _WPEMonitorWaylandPrivate {
    struct wl_output* wlOutput;
};
WEBKIT_DEFINE_FINAL_TYPE(WPEMonitorWayland, wpe_monitor_wayland, WPE_TYPE_MONITOR, WPEMonitor)

static void wpeMonitorWaylandInvalidate(WPEMonitor* monitor)
{
    auto* priv = WPE_MONITOR_WAYLAND(monitor)->priv;
    if (priv->wlOutput) {
        if (wl_output_get_version(priv->wlOutput) >= WL_OUTPUT_RELEASE_SINCE_VERSION)
            wl_output_release(priv->wlOutput);
        else
            wl_output_destroy(priv->wlOutput);
        priv->wlOutput = nullptr;
    }
}

static void wpeMonitorWaylandDispose(GObject* object)
{
    wpeMonitorWaylandInvalidate(WPE_MONITOR(object));

    G_OBJECT_CLASS(wpe_monitor_wayland_parent_class)->dispose(object);
}

static void wpe_monitor_wayland_class_init(WPEMonitorWaylandClass* monitorWaylandClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(monitorWaylandClass);
    objectClass->dispose = wpeMonitorWaylandDispose;

    WPEMonitorClass* monitorClass = WPE_MONITOR_CLASS(monitorWaylandClass);
    monitorClass->invalidate = wpeMonitorWaylandInvalidate;
}

static const struct wl_output_listener outputListener = {
    // geometry
    [](void* data, struct wl_output*, int32_t x, int32_t y, int32_t width, int32_t height, int32_t, const char*, const char*, int32_t transform)
    {
        WPEMonitor* monitor = WPE_MONITOR(data);
        wpe_monitor_set_position(monitor, x, y);
        switch (transform) {
        case WL_OUTPUT_TRANSFORM_90:
        case WL_OUTPUT_TRANSFORM_270:
        case WL_OUTPUT_TRANSFORM_FLIPPED_90:
        case WL_OUTPUT_TRANSFORM_FLIPPED_270:
            wpe_monitor_set_physical_size(monitor, height, width);
            break;
        default:
            wpe_monitor_set_physical_size(monitor, width, height);
            break;
        }
    },
    // mode
    [](void* data, struct wl_output*, uint32_t flags, int32_t width, int32_t height, int32_t refresh)
    {
        if (!(flags & WL_OUTPUT_MODE_CURRENT))
            return;

        WPEMonitor* monitor = WPE_MONITOR(data);
        wpe_monitor_set_size(monitor, width, height);
        wpe_monitor_set_refresh_rate(monitor, refresh);
    },
    // done
    [](void*, struct wl_output*)
    {
    },
    // scale
    [](void* data, struct wl_output*, int32_t factor)
    {
        WPEMonitor* monitor = WPE_MONITOR(data);
        wpe_monitor_set_scale(monitor, factor);
    },
#ifdef WL_OUTPUT_NAME_SINCE_VERSION
    // name
    [](void*, struct wl_output*, const char*)
    {
    },
#endif
#ifdef WL_OUTPUT_DESCRIPTION_SINCE_VERSION
    // description
    [](void*, struct wl_output*, const char*)
    {
    },
#endif
};

WPEMonitor* wpeMonitorWaylandCreate(guint32 id, struct wl_output* wlOutput)
{
    auto* monitor = WPE_MONITOR_WAYLAND(g_object_new(WPE_TYPE_MONITOR_WAYLAND, "id", id, nullptr));
    monitor->priv->wlOutput = wlOutput;
    wl_output_add_listener(monitor->priv->wlOutput, &outputListener, monitor);
    return WPE_MONITOR(monitor);
}

/**
 * wpe_monitor_wayland_get_wl_output: (skip)
 * @monitor: a #WPEMonitorWayland
 *
 * Get the Wayland output of @monitor
 *
 * Returns: (transfer none) (nullable): a Wayland `wl_output`
 */
struct wl_output* wpe_monitor_wayland_get_wl_output(WPEMonitorWayland* monitor)
{
    g_return_val_if_fail(WPE_IS_MONITOR_WAYLAND(monitor), nullptr);

    return monitor->priv->wlOutput;
}

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
#include "WPEScreenWayland.h"

#include <wtf/glib/WTFGType.h>

/**
 * WPEScreenWayland:
 *
 */
struct _WPEScreenWaylandPrivate {
    struct wl_output* wlOutput;
    struct {
        int x;
        int y;
        int width;
        int height;
        int scale;
    } pendingScreenUpdate;
};
WEBKIT_DEFINE_FINAL_TYPE(WPEScreenWayland, wpe_screen_wayland, WPE_TYPE_SCREEN, WPEScreen)

static void wpeScreenWaylandInvalidate(WPEScreen* screen)
{
    auto* priv = WPE_SCREEN_WAYLAND(screen)->priv;
    if (priv->wlOutput) {
        if (wl_output_get_version(priv->wlOutput) >= WL_OUTPUT_RELEASE_SINCE_VERSION)
            wl_output_release(priv->wlOutput);
        else
            wl_output_destroy(priv->wlOutput);
        priv->wlOutput = nullptr;
    }
}

static void wpeScreenWaylandDispose(GObject* object)
{
    wpeScreenWaylandInvalidate(WPE_SCREEN(object));

    G_OBJECT_CLASS(wpe_screen_wayland_parent_class)->dispose(object);
}

static void wpe_screen_wayland_class_init(WPEScreenWaylandClass* screenWaylandClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(screenWaylandClass);
    objectClass->dispose = wpeScreenWaylandDispose;

    WPEScreenClass* screenClass = WPE_SCREEN_CLASS(screenWaylandClass);
    screenClass->invalidate = wpeScreenWaylandInvalidate;
}

static const struct wl_output_listener outputListener = {
    // geometry
    [](void* data, struct wl_output*, int32_t x, int32_t y, int32_t width, int32_t height, int32_t, const char*, const char*, int32_t transform) {
        WPEScreen* screen = WPE_SCREEN(data);
        auto* priv = WPE_SCREEN_WAYLAND(screen)->priv;
        priv->pendingScreenUpdate.x = x;
        priv->pendingScreenUpdate.y = y;

        switch (transform) {
        case WL_OUTPUT_TRANSFORM_90:
        case WL_OUTPUT_TRANSFORM_270:
        case WL_OUTPUT_TRANSFORM_FLIPPED_90:
        case WL_OUTPUT_TRANSFORM_FLIPPED_270:
            wpe_screen_set_physical_size(screen, height, width);
            break;
        default:
            wpe_screen_set_physical_size(screen, width, height);
            break;
        }
    },
    // mode
    [](void* data, struct wl_output*, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
        if (!(flags & WL_OUTPUT_MODE_CURRENT))
            return;

        WPEScreen* screen = WPE_SCREEN(data);
        auto* priv = WPE_SCREEN_WAYLAND(screen)->priv;
        priv->pendingScreenUpdate.width = width;
        priv->pendingScreenUpdate.height = height;
        wpe_screen_set_refresh_rate(screen, refresh);
    },
    // done
    [](void* data, struct wl_output*) {
        WPEScreen* screen = WPE_SCREEN(data);
        auto* priv = WPE_SCREEN_WAYLAND(screen)->priv;
        wpe_screen_set_position(screen, priv->pendingScreenUpdate.x / priv->pendingScreenUpdate.scale, priv->pendingScreenUpdate.y / priv->pendingScreenUpdate.scale);
        wpe_screen_set_size(screen, priv->pendingScreenUpdate.width / priv->pendingScreenUpdate.scale, priv->pendingScreenUpdate.height / priv->pendingScreenUpdate.scale);
        wpe_screen_set_scale(screen, priv->pendingScreenUpdate.scale);
    },
    // scale
    [](void* data, struct wl_output*, int32_t factor) {
        auto* priv = WPE_SCREEN_WAYLAND(data)->priv;
        priv->pendingScreenUpdate.scale = factor;
    },
#ifdef WL_OUTPUT_NAME_SINCE_VERSION
    // name
    [](void*, struct wl_output*, const char*) {
    },
#endif
#ifdef WL_OUTPUT_DESCRIPTION_SINCE_VERSION
    // description
    [](void*, struct wl_output*, const char*) {
    },
#endif
};

WPEScreen* wpeScreenWaylandCreate(guint32 id, struct wl_output* wlOutput)
{
    auto* screen = WPE_SCREEN_WAYLAND(g_object_new(WPE_TYPE_SCREEN_WAYLAND, "id", id, nullptr));
    screen->priv->wlOutput = wlOutput;
    wl_output_add_listener(screen->priv->wlOutput, &outputListener, screen);
    return WPE_SCREEN(screen);
}

/**
 * wpe_screen_wayland_get_wl_output: (skip)
 * @screen: a #WPEScreenWayland
 *
 * Get the Wayland output of @screen
 *
 * Returns: (transfer none) (nullable): a Wayland `wl_output`
 */
struct wl_output* wpe_screen_wayland_get_wl_output(WPEScreenWayland* screen)
{
    g_return_val_if_fail(WPE_IS_SCREEN_WAYLAND(screen), nullptr);

    return screen->priv->wlOutput;
}

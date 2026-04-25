/*
 * Copyright (C) 2025 Igalia S.L.
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
#include "WPEClipboardWayland.h"

#include "WPEClipboardWaylandPrivate.h"
#include "WPEDisplayWaylandPrivate.h"
#include <array>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <glib-unix.h>
#include <wayland-client.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GWeakPtr.h>
#include <wtf/glib/WTFGType.h>

/**
 * WPEClipboardWayland:
 *
 */
struct _WPEClipboardWaylandPrivate {
    struct wl_data_device* wlDataDevice;
    struct wl_data_offer* offer;
    struct wl_data_source* source;
    GRefPtr<GPtrArray> formats;
};
WEBKIT_DEFINE_FINAL_TYPE(WPEClipboardWayland, wpe_clipboard_wayland, WPE_TYPE_CLIPBOARD, WPEClipboard)

static const struct wl_data_source_listener wlDataSourceListener = {
    // target
    [](void*, wl_data_source*, const char* /*mimeType*/) {

    },
    // send
    [](void* data, wl_data_source* source, const char* mimeType, int32_t fd) {
        auto* priv = WPE_CLIPBOARD_WAYLAND(data)->priv;
        if (priv->source != source) {
            close(fd);
            return;
        }

        auto* content = wpe_clipboard_get_content(WPE_CLIPBOARD(data));
        if (!content) {
            close(fd);
            return;
        }

        GRefPtr<GOutputStream> stream = adoptGRef(g_unix_output_stream_new(fd, TRUE));
        wpe_clipboard_content_serialize(content, mimeType, stream.get());
    },
    // cancelled
    [](void* data, wl_data_source* source) {
        auto* priv = WPE_CLIPBOARD_WAYLAND(data)->priv;
        if (priv->source != source)
            return;

        g_clear_pointer(&priv->offer, wl_data_offer_destroy);
        g_clear_pointer(&priv->source, wl_data_source_destroy);
        priv->formats = nullptr;

        auto* clipboard = WPE_CLIPBOARD(data);
        WPE_CLIPBOARD_GET_CLASS(clipboard)->changed(clipboard, nullptr, FALSE, nullptr);
    },
    // dnd_drop_performed
    [](void*, wl_data_source*) {
    },
    // dnd_finished
    [](void*, wl_data_source*) {
    },
    // action
    [](void*, wl_data_source*, uint32_t /*dndAction*/) {
    }
};

static const struct wl_data_offer_listener  wlDataOfferListener = {
    // offer
    [](void* data, wl_data_offer* offer, const char* mimeType) {
        auto* priv = WPE_CLIPBOARD_WAYLAND(data)->priv;
        if (priv->offer != offer)
            return;

        const auto* format = g_intern_string(mimeType);
        if (!g_ptr_array_find(priv->formats.get(), format, nullptr))
            g_ptr_array_add(priv->formats.get(), static_cast<gpointer>(const_cast<char*>(format)));
    },
    // source_actions
    [](void*, wl_data_offer*, uint32_t /*sourceActions*/) {
    },
    // action
    [](void*, wl_data_offer*, uint32_t /*dndAction*/) {
    }
};

static const struct wl_data_device_listener wlDataDeviceListener = {
    // data_offer
    [](void* data, wl_data_device*, wl_data_offer* offer) {
        auto* priv = WPE_CLIPBOARD_WAYLAND(data)->priv;
        g_clear_pointer(&priv->offer, wl_data_offer_destroy);
        priv->offer = offer;
        wl_data_offer_add_listener(priv->offer, &wlDataOfferListener, data);

        priv->formats = adoptGRef(g_ptr_array_new());
    },
    // enter
    [](void*, wl_data_device*, uint32_t /*serial*/, wl_surface*, wl_fixed_t /*x*/, wl_fixed_t /*y*/, wl_data_offer*) {
    },
    // leave
    [](void*, wl_data_device*) {
    },
    // motion
    [](void*, wl_data_device*, uint32_t /*time*/, wl_fixed_t /*x*/, wl_fixed_t /*y*/) {
    },
    // drop
    [](void*, wl_data_device*) {
    },
    // selection
    [](void* data, wl_data_device*, wl_data_offer* offer) {
        auto* priv = WPE_CLIPBOARD_WAYLAND(data)->priv;
        GRefPtr<GPtrArray> formats;
        if (offer) {
            if (priv->offer == offer) {
                g_ptr_array_add(priv->formats.get(), nullptr);
                formats = std::exchange(priv->formats, nullptr);
            } else {
                g_clear_pointer(&priv->offer, wl_data_offer_destroy);
                priv->formats = nullptr;
            }
        }

        if (priv->source)
            return;

        auto* clipboard = WPE_CLIPBOARD(data);
        WPE_CLIPBOARD_GET_CLASS(clipboard)->changed(clipboard, formats.get(), FALSE, nullptr);
    }
};

static void wpeClipboardWaylandConstructed(GObject *object)
{
    G_OBJECT_CLASS(wpe_clipboard_wayland_parent_class)->constructed(object);

    auto* clipboard = WPE_CLIPBOARD(object);
    auto* display = WPE_DISPLAY_WAYLAND(wpe_clipboard_get_display(clipboard));
    auto* seat = wpeDisplayWaylandGetSeat(display);
    ASSERT(seat);
    auto* wlDataDeviceManager = wpeDisplayWaylandGetDataDeviceManager(display);
    ASSERT(wlDataDeviceManager);
    auto* priv = WPE_CLIPBOARD_WAYLAND(clipboard)->priv;
    priv->wlDataDevice = wl_data_device_manager_get_data_device(wlDataDeviceManager, seat->seat());
    wl_data_device_add_listener(priv->wlDataDevice, &wlDataDeviceListener, clipboard);
}

static void wpeClipboardWaylandDispose(GObject* object)
{
    wpeClipboardWaylandInvalidate(WPE_CLIPBOARD_WAYLAND(object));

    G_OBJECT_CLASS(wpe_clipboard_wayland_parent_class)->dispose(object);
}

static GBytes* wpeClipboardWaylandRead(WPEClipboard* clipboard, const char* format)
{
    auto* priv = WPE_CLIPBOARD_WAYLAND(clipboard)->priv;
    if (!priv->offer)
        return nullptr;

    std::array<int, 2> pipeFD;
    if (!g_unix_open_pipe(pipeFD.data(), O_CLOEXEC, nullptr))
        return nullptr;

    wl_data_offer_receive(priv->offer, format, pipeFD[1]);
    close(pipeFD[1]);

    auto* display = WPE_DISPLAY_WAYLAND(wpe_clipboard_get_display(clipboard));
    wl_display_roundtrip(wpe_display_wayland_get_wl_display(display));

    GRefPtr<GInputStream> inStream = adoptGRef(g_unix_input_stream_new(pipeFD[0], TRUE));
    GRefPtr<GOutputStream> outStream = adoptGRef(g_memory_output_stream_new_resizable());
    auto result = g_output_stream_splice(outStream.get(), inStream.get(), static_cast<GOutputStreamSpliceFlags>(G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET), nullptr, nullptr);
    if (result == -1)
        return nullptr;

    return g_memory_output_stream_steal_as_bytes(G_MEMORY_OUTPUT_STREAM(outStream.get()));
}

static void wpeClipboardWaylandChanged(WPEClipboard* clipboard, GPtrArray* formats, gboolean isLocal, WPEClipboardContent* content)
{
    if (isLocal) {
        auto* priv = WPE_CLIPBOARD_WAYLAND(clipboard)->priv;
        g_clear_pointer(&priv->offer, wl_data_offer_destroy);
        g_clear_pointer(&priv->source, wl_data_source_destroy);
        priv->formats = nullptr;

        auto* display = WPE_DISPLAY_WAYLAND(wpe_clipboard_get_display(clipboard));
        auto* wlDataDeviceManager = wpeDisplayWaylandGetDataDeviceManager(display);
        ASSERT(wlDataDeviceManager);
        priv->source = wl_data_device_manager_create_data_source(wlDataDeviceManager);
        wl_data_source_add_listener(priv->source, &wlDataSourceListener, clipboard);

        if (formats) {
            for (unsigned i = 0; formats->pdata[i]; ++i)
                wl_data_source_offer(priv->source, static_cast<const char*>(formats->pdata[i]));
        }

        auto* seat = wpeDisplayWaylandGetSeat(display);
        wl_data_device_set_selection(priv->wlDataDevice, priv->source, seat->keyboardSerial());
    }

    WPE_CLIPBOARD_CLASS(wpe_clipboard_wayland_parent_class)->changed(clipboard, formats, isLocal, content);
}

static void wpe_clipboard_wayland_class_init(WPEClipboardWaylandClass* clipboardWaylandClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(clipboardWaylandClass);
    objectClass->constructed = wpeClipboardWaylandConstructed;
    objectClass->dispose = wpeClipboardWaylandDispose;

    WPEClipboardClass* clipboardClass = WPE_CLIPBOARD_CLASS(clipboardWaylandClass);
    clipboardClass->read = wpeClipboardWaylandRead;
    clipboardClass->changed = wpeClipboardWaylandChanged;
}

void wpeClipboardWaylandInvalidate(WPEClipboardWayland* clipboard)
{
    auto* priv = clipboard->priv;
    g_clear_pointer(&priv->offer, wl_data_offer_destroy);
    g_clear_pointer(&priv->source, wl_data_source_destroy);
    g_clear_pointer(&priv->wlDataDevice, wl_data_device_destroy);
}

/**
 * wpe_clipboard_wayland_new:
 * @display: a #WPEDisplayWayland
 *
 * Create a new #WPEClipboardWayland
 *
 * Returns: (transfer full): a #WPEClipboard
 */
WPEClipboard* wpe_clipboard_wayland_new(WPEDisplayWayland* display)
{
    return WPE_CLIPBOARD(g_object_new(WPE_TYPE_CLIPBOARD_WAYLAND, "display", display, nullptr));
}

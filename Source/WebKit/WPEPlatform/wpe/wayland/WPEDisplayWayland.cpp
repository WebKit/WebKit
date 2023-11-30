/*
 * Copyright (C) 2023 Igalia S.L.
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
#include "WPEDisplayWayland.h"

#include "WPEDisplayWaylandPrivate.h"
#include "WPEEGLError.h"
#include "WPEExtensions.h"
#include "WPEViewWayland.h"
#include "WPEWaylandCursor.h"
#include "WPEWaylandOutput.h"
#include "WPEWaylandSeat.h"
#include "linux-dmabuf-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include <epoxy/egl.h>
#include <gio/gio.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>

/**
 * WPEDisplayWayland:
 *
 */
struct _WPEDisplayWaylandPrivate {
    struct wl_display* wlDisplay;
    struct wl_compositor* wlCompositor;
    struct xdg_wm_base* xdgWMBase;
    struct wl_shm* wlSHM;
    struct zwp_linux_dmabuf_v1* linuxDMABuf;
    Vector<std::pair<uint32_t, uint64_t>> linuxDMABufFormats;
    std::unique_ptr<WPE::WaylandSeat> wlSeat;
    std::unique_ptr<WPE::WaylandCursor> wlCursor;
    Vector<std::unique_ptr<WPE::WaylandOutput>, 1> wlOutputs;
    GRefPtr<GSource> eventSource;
};
WEBKIT_DEFINE_FINAL_TYPE_WITH_CODE(WPEDisplayWayland, wpe_display_wayland, WPE_TYPE_DISPLAY, WPEDisplay,
    wpeEnsureExtensionPointsRegistered();
    g_io_extension_point_implement(WPE_DISPLAY_EXTENSION_POINT_NAME, g_define_type_id, "wpe-display-wayland", 0))

struct EventSource {
    static GSourceFuncs sourceFuncs;

    GSource source;
    GPollFD pfd;
    struct wl_display* display;
};

GSourceFuncs EventSource::sourceFuncs = {
    // prepare
    [](GSource* base, gint* timeout) -> gboolean
    {
        auto* source = reinterpret_cast<EventSource*>(base);
        struct wl_display* display = source->display;

        *timeout = -1;

        while (wl_display_prepare_read(display)) {
            if (wl_display_dispatch_pending(display) < 0)
                return FALSE;
        }

        wl_display_flush(display);

        return FALSE;
    },
    // check
    [](GSource* base) -> gboolean
    {
        auto* source = reinterpret_cast<EventSource*>(base);
        struct wl_display* display = source->display;

        if (source->pfd.revents & G_IO_IN) {
            if (wl_display_read_events(display) < 0)
                return FALSE;
        } else
            wl_display_cancel_read(display);

        return !!source->pfd.revents;
    },
    // dispatch
    [](GSource* base, GSourceFunc, gpointer) -> gboolean
    {
        auto* source = reinterpret_cast<EventSource*>(base);
        struct wl_display* display = source->display;

        if (source->pfd.revents & (G_IO_ERR | G_IO_HUP))
            return FALSE;

        if (source->pfd.revents & G_IO_IN) {
            if (wl_display_dispatch_pending(display) < 0)
                return FALSE;
        }

        source->pfd.revents = 0;
        return TRUE;
    },
    nullptr, // finalize
    nullptr, // closure_callback
    nullptr, // closure_marshall
};

static GRefPtr<GSource> wpeDisplayWaylandCreateEventSource(WPEDisplayWayland* display)
{
    auto source = adoptGRef(g_source_new(&EventSource::sourceFuncs, sizeof(EventSource)));
    auto& eventSource = *reinterpret_cast<EventSource*>(source.get());
    eventSource.display = display->priv->wlDisplay;
    eventSource.pfd.fd = wl_display_get_fd(eventSource.display);
    eventSource.pfd.events = G_IO_IN | G_IO_ERR | G_IO_HUP;
    eventSource.pfd.revents = 0;
    g_source_add_poll(&eventSource.source, &eventSource.pfd);

    g_source_set_priority(&eventSource.source, G_PRIORITY_DEFAULT);
    g_source_set_can_recurse(&eventSource.source, TRUE);
    g_source_attach(&eventSource.source, g_main_context_get_thread_default());

    return source;
}

static void wpeDisplayWaylandDispose(GObject* object)
{
    auto* priv = WPE_DISPLAY_WAYLAND(object)->priv;

    if (priv->eventSource) {
        g_source_destroy(priv->eventSource.get());
        priv->eventSource = nullptr;
    }

    g_clear_pointer(&priv->linuxDMABuf, zwp_linux_dmabuf_v1_destroy);
    g_clear_pointer(&priv->wlSHM, wl_shm_destroy);
    g_clear_pointer(&priv->xdgWMBase, xdg_wm_base_destroy);
    g_clear_pointer(&priv->wlCompositor, wl_compositor_destroy);
    g_clear_pointer(&priv->wlDisplay, wl_display_disconnect);

    G_OBJECT_CLASS(wpe_display_wayland_parent_class)->dispose(object);
}

const struct wl_registry_listener registryListener = {
    // global
    [](void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version)
    {
        auto* display = WPE_DISPLAY_WAYLAND(data);
        auto* priv = display->priv;

        if (!std::strcmp(interface, "wl_compositor"))
            priv->wlCompositor = static_cast<struct wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, std::min<uint32_t>(version, 5)));
        else if (!std::strcmp(interface, "xdg_wm_base"))
            priv->xdgWMBase = static_cast<struct xdg_wm_base*>(wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
        // FIXME: support zxdg_shell_v6?
        else if (!std::strcmp(interface, "wl_seat"))
            priv->wlSeat = makeUnique<WPE::WaylandSeat>(static_cast<struct wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, std::min<uint32_t>(version, 8))));
        else if (!std::strcmp(interface, "wl_output"))
            priv->wlOutputs.append(makeUnique<WPE::WaylandOutput>(static_cast<struct wl_output*>(wl_registry_bind(registry, name, &wl_output_interface, std::min<uint32_t>(version, 2)))));
        else if (!std::strcmp(interface, "wl_shm"))
            priv->wlSHM = static_cast<struct wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
        else if (!std::strcmp(interface, "zwp_linux_dmabuf_v1"))
            priv->linuxDMABuf = static_cast<struct zwp_linux_dmabuf_v1*>(wl_registry_bind(registry, name, &zwp_linux_dmabuf_v1_interface, std::min<uint32_t>(version, 4)));
    },
    // global_remove
    [](void*, struct wl_registry*, uint32_t)
    {
    },
};

const struct xdg_wm_base_listener xdgWMBaseListener = {
    // ping
    [](void*, struct xdg_wm_base* xdgWMBase, uint32_t serial)
    {
        xdg_wm_base_pong(xdgWMBase, serial);
    },
};

static const struct zwp_linux_dmabuf_v1_listener linuxDMABufListener = {
    // format
    [](void*, struct zwp_linux_dmabuf_v1*, uint32_t) {
    },
    // modifier
    [](void* data, struct zwp_linux_dmabuf_v1*, uint32_t format, uint32_t modifierHigh, uint32_t modifierLow)
    {
        auto* display = WPE_DISPLAY_WAYLAND(data);
        uint64_t modifier = (static_cast<uint64_t>(modifierHigh) << 32) | modifierLow;
        display->priv->linuxDMABufFormats.append({ format, modifier });
    }
};

static gboolean wpeDisplayWaylandConnect(WPEDisplay* display, GError** error)
{
    auto* displayWaylnd = WPE_DISPLAY_WAYLAND(display);
    auto* priv = displayWaylnd->priv;
    if (priv->wlDisplay) {
        g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "Wayland display is already connected");
        return FALSE;
    }

    priv->wlDisplay = wl_display_connect(nullptr);
    if (!priv->wlDisplay) {
        g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "Failed to connect to default Wayland display");
        return FALSE;
    }

    priv->eventSource = wpeDisplayWaylandCreateEventSource(displayWaylnd);

    auto* registry = wl_display_get_registry(priv->wlDisplay);
    wl_registry_add_listener(registry, &registryListener, display);
    if (wl_display_roundtrip(priv->wlDisplay) < 0) {
        g_clear_pointer(&priv->wlDisplay, wl_display_disconnect);
        g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "Failed to connect to default Wayland display");
        return FALSE;
    }

    if (priv->xdgWMBase)
        xdg_wm_base_add_listener(priv->xdgWMBase, &xdgWMBaseListener, nullptr);
    if (priv->wlSeat) {
        priv->wlCursor = makeUnique<WPE::WaylandCursor>(displayWaylnd);
        priv->wlSeat->startListening();
    }
    if (priv->linuxDMABuf) {
        zwp_linux_dmabuf_v1_add_listener(priv->linuxDMABuf, &linuxDMABufListener, display);
        wl_display_roundtrip(priv->wlDisplay);
    }

    return TRUE;
}

static WPEView* wpeDisplayWaylandCreateView(WPEDisplay* display)
{
    auto* priv = WPE_DISPLAY_WAYLAND(display)->priv;
    if (!priv->wlDisplay || !priv->wlCompositor)
        return nullptr;

    return wpe_view_wayland_new(WPE_DISPLAY_WAYLAND(display));
}

static gpointer wpeDisplayWaylandGetEGLDisplay(WPEDisplay* display, GError** error)
{
    auto* priv = WPE_DISPLAY_WAYLAND(display)->priv;
    if (!priv->wlDisplay) {
        g_set_error_literal(error, WPE_EGL_ERROR, WPE_EGL_ERROR_NOT_AVAILABLE, "Can't get EGL display: Wayland display is not connected");
        return nullptr;
    }

    if (auto* eglDisplay = eglGetDisplay(priv->wlDisplay))
        return eglDisplay;

    g_set_error_literal(error, WPE_EGL_ERROR, WPE_EGL_ERROR_NOT_AVAILABLE, "Can't get EGL display: no display connection matching wayland connection found");
    return nullptr;
}

static WPEKeymap* wpeDisplayWaylandGetKeymap(WPEDisplay* display, GError** error)
{
    auto* priv = WPE_DISPLAY_WAYLAND(display)->priv;
    if (!priv->wlSeat) {
        g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_NOT_SUPPORTED, "Operation not supported");
        return nullptr;
    }

    return priv->wlSeat->keymap();
}

static GList* wpeDisplayWaylandGetPreferredDMABufFormats(WPEDisplay* display)
{
    auto* priv = WPE_DISPLAY_WAYLAND(display)->priv;
    if (!priv->linuxDMABuf)
        return nullptr;

    GList* preferredFormats = nullptr;
    uint32_t previousFormat = 0;
    GRefPtr<GArray> modifiers = adoptGRef(g_array_new(FALSE, TRUE, sizeof(guint64)));
    auto linuxDMABufFormatsLength = priv->linuxDMABufFormats.size();
    for (size_t i = 0; i < linuxDMABufFormatsLength; ++i) {
        auto [format, modifier] = priv->linuxDMABufFormats[i];
        g_array_append_val(modifiers.get(), modifier);
        if (i && format != previousFormat) {
            preferredFormats = g_list_prepend(preferredFormats, wpe_buffer_dma_buf_format_new(WPE_BUFFER_DMA_BUF_FORMAT_USAGE_RENDERING, previousFormat, modifiers.get()));
            modifiers = adoptGRef(g_array_new(FALSE, TRUE, sizeof(guint64)));
        }
        previousFormat = format;
    }
    if (previousFormat)
        preferredFormats = g_list_prepend(preferredFormats, wpe_buffer_dma_buf_format_new(WPE_BUFFER_DMA_BUF_FORMAT_USAGE_RENDERING, previousFormat, modifiers.get()));
    return g_list_reverse(preferredFormats);
}

struct xdg_wm_base* wpeDisplayWaylandGetXDGWMBase(WPEDisplayWayland* display)
{
    return display->priv->xdgWMBase;
}

WPE::WaylandSeat* wpeDisplayWaylandGetSeat(WPEDisplayWayland* display)
{
    return display->priv->wlSeat.get();
}

WPE::WaylandCursor* wpeDisplayWaylandGetCursor(WPEDisplayWayland* display)
{
    return display->priv->wlCursor.get();
}

WPE::WaylandOutput* wpeDisplayWaylandGetOutput(WPEDisplayWayland* display, struct wl_output* output)
{
    for (const auto& wlOutput : display->priv->wlOutputs) {
        if (wlOutput->output() == output)
            return wlOutput.get();
    }

    return nullptr;
}

struct zwp_linux_dmabuf_v1* wpeDisplayWaylandGetLinuxDMABuf(WPEDisplayWayland* display)
{
    return display->priv->linuxDMABuf;
}

static void wpe_display_wayland_class_init(WPEDisplayWaylandClass* displayWaylandClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(displayWaylandClass);
    objectClass->dispose = wpeDisplayWaylandDispose;

    WPEDisplayClass* displayClass = WPE_DISPLAY_CLASS(displayWaylandClass);
    displayClass->connect = wpeDisplayWaylandConnect;
    displayClass->create_view = wpeDisplayWaylandCreateView;
    displayClass->get_egl_display = wpeDisplayWaylandGetEGLDisplay;
    displayClass->get_keymap = wpeDisplayWaylandGetKeymap;
    displayClass->get_preferred_dma_buf_formats = wpeDisplayWaylandGetPreferredDMABufFormats;
}

/**
 * wpe_display_wayland_new:
 *
 * Create a new #WPEDisplayWayland
 *
 * Returns: (transfer full): a #WPEDisplay
 */
WPEDisplay* wpe_display_wayland_new(void)
{
    return WPE_DISPLAY(g_object_new(WPE_TYPE_DISPLAY_WAYLAND, nullptr));
}

/**
 * wpe_display_wayland_connect:
 * @display: a #WPEDisplayWayland
 * @name: (nullable): the name of the display to connect to, or %NULL
 * @error: return location for error or %NULL to ignore
 *
 * Connect to the Wayland display named @name. If @name is %NULL it
 * connects to the default display.
 *
 *
 * Returns: %TRUE if connection succeeded, or %FALSE in case of error.
 */
gboolean wpe_display_wayland_connect(WPEDisplayWayland* display, const char* name, GError** error)
{
    WPEDisplayWaylandPrivate* priv;

    g_return_val_if_fail(WPE_IS_DISPLAY_WAYLAND(display), FALSE);

    priv = display->priv;
    if (priv->wlDisplay) {
        g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "Wayland display is already connected");
        return FALSE;
    }

    priv->wlDisplay = wl_display_connect(name);
    if (!priv->wlDisplay) {
        g_set_error(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "Failed to connect to Wayland display `%s`", name ? name : "default");
        return FALSE;
    }

    return TRUE;
}

/**
 * wpe_display_wayland_get_wl_display: (skip)
 * @display: a #WPEDisplayWayland
 *
 * Get the native Wayland display of @display
 *
 * Returns: (transfer none) (nullable): a Wayland `wl_display`,
 *    or %NULL if display is not connected
 */
struct wl_display* wpe_display_wayland_get_wl_display(WPEDisplayWayland* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY_WAYLAND(display), nullptr);

    return display->priv->wlDisplay;
}

/**
 * wpe_display_wayland_get_wl_compositor: (skip)
 * @display: a #WPEDisplayWayland
 *
 * Get the Wayland compositor of @display
 *
 * Returns: (transfer none) (nullable): a Wayland `wl_compositor`,
 *    or %NULL if display is not connected
 */
struct wl_compositor* wpe_display_wayland_get_wl_compositor(WPEDisplayWayland* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY_WAYLAND(display), nullptr);

    return display->priv->wlCompositor;
}

/**
 * wpe_display_wayland_get_wl_shm: (skip)
 * @display: a #WPEDisplayWayland
 *
 * Get the Wayland SHM of @display
 *
 * Returns: (transfer none) (nullable): a Wayland `wl_shm`, or %NULL
 */
struct wl_shm* wpe_display_wayland_get_wl_shm(WPEDisplayWayland* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY_WAYLAND(display), nullptr);

    return display->priv->wlSHM;
}

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
#include "WPEMonitorWaylandPrivate.h"
#include "WPEViewWayland.h"
#include "WPEWaylandCursor.h"
#include "WPEWaylandSeat.h"
#include "linux-dmabuf-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include <gio/gio.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

// These includes need to be in this order because wayland-egl.h defines WL_EGL_PLATFORM
// and egl.h checks that to decide whether it's Wayland platform.
#include <wayland-egl.h>
#include <epoxy/egl.h>

#if USE(LIBDRM)
#include <xf86drm.h>
#endif

#ifndef EGL_DRM_RENDER_NODE_FILE_EXT
#define EGL_DRM_RENDER_NODE_FILE_EXT 0x3377
#endif

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
#if USE(LIBDRM)
    struct zwp_linux_dmabuf_feedback_v1* dmabufFeedback;
#endif
    Vector<std::pair<uint32_t, uint64_t>> linuxDMABufFormats;
    std::unique_ptr<WPE::WaylandSeat> wlSeat;
    std::unique_ptr<WPE::WaylandCursor> wlCursor;
    CString drmDevice;
    CString drmRenderNode;
    Vector<GRefPtr<WPEMonitor>, 1> monitors;
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

    priv->wlSeat = nullptr;
    priv->wlCursor = nullptr;
    while (!priv->monitors.isEmpty()) {
        auto monitor = priv->monitors.takeLast();
        wpe_monitor_invalidate(monitor.get());
    }
#if USE(LIBDRM)
    g_clear_pointer(&priv->dmabufFeedback, zwp_linux_dmabuf_feedback_v1_destroy);
#endif
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
        else if (!std::strcmp(interface, "wl_output")) {
            GRefPtr<WPEMonitor> monitor = adoptGRef(wpeMonitorWaylandCreate(name, static_cast<struct wl_output*>(wl_registry_bind(registry, name, &wl_output_interface, std::min<uint32_t>(version, 2)))));
            auto* monitorPtr = monitor.get();
            priv->monitors.append(WTFMove(monitor));
            wpe_display_monitor_added(WPE_DISPLAY(display), monitorPtr);
        } else if (!std::strcmp(interface, "wl_shm"))
            priv->wlSHM = static_cast<struct wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
        else if (!std::strcmp(interface, "zwp_linux_dmabuf_v1"))
            priv->linuxDMABuf = static_cast<struct zwp_linux_dmabuf_v1*>(wl_registry_bind(registry, name, &zwp_linux_dmabuf_v1_interface, std::min<uint32_t>(version, 4)));
    },
    // global_remove
    [](void* data, struct wl_registry*, uint32_t name)
    {
        auto* display = WPE_DISPLAY_WAYLAND(data);
        auto* priv = display->priv;
        auto index = priv->monitors.findIf([name](const auto& monitor) {
            return wpe_monitor_get_id(monitor.get()) == name;
        });
        if (index != notFound) {
            auto monitor = priv->monitors[index];
            priv->monitors.remove(index);
            wpe_display_monitor_removed(WPE_DISPLAY(display), monitor.get());
        }
    },
};

const struct xdg_wm_base_listener xdgWMBaseListener = {
    // ping
    [](void*, struct xdg_wm_base* xdgWMBase, uint32_t serial)
    {
        xdg_wm_base_pong(xdgWMBase, serial);
    },
};

#if USE(LIBDRM)
static const struct zwp_linux_dmabuf_feedback_v1_listener linuxDMABufFeedbackListener = {
    // done
    [](void*, struct zwp_linux_dmabuf_feedback_v1*)
    {
    },
    // format_table
    [](void*, struct zwp_linux_dmabuf_feedback_v1*, int32_t, uint32_t)
    {
    },
    // main_device
    [](void* data, struct zwp_linux_dmabuf_feedback_v1*, struct wl_array* device)
    {
        dev_t deviceID;
        memcpy(&deviceID, device->data, sizeof(dev_t));

        drmDevicePtr drmDevice;
        if (drmGetDeviceFromDevId(deviceID, 0, &drmDevice))
            return;

        auto* priv = WPE_DISPLAY_WAYLAND(data)->priv;
        if (drmDevice->available_nodes & (1 << DRM_NODE_PRIMARY))
            priv->drmDevice = drmDevice->nodes[DRM_NODE_PRIMARY];
        if (drmDevice->available_nodes & (1 << DRM_NODE_RENDER))
            priv->drmRenderNode = drmDevice->nodes[DRM_NODE_RENDER];
        drmFreeDevice(&drmDevice);
    },
    // tranche_done
    [](void*, struct zwp_linux_dmabuf_feedback_v1*)
    {
    },
    // tranche_target_device
    [](void*, struct zwp_linux_dmabuf_feedback_v1*, struct wl_array*)
    {
    },
    // tranche_formats
    [](void*, struct zwp_linux_dmabuf_feedback_v1*, struct wl_array*)
    {
    },
    // tranche_flags
    [](void*, struct zwp_linux_dmabuf_feedback_v1*, uint32_t)
    {
    }
};
#endif

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

static void wpeDisplayWaylandInitializeDRMDeviceFromEGL(WPEDisplayWayland* display)
{
    auto* priv = display->priv;
    auto* eglDisplay = eglGetDisplay(priv->wlDisplay);
    if (!eglDisplay)
        return;

    if (!eglInitialize(eglDisplay, nullptr, nullptr))
        return;

    if (!epoxy_has_egl_extension(eglDisplay, "EGL_EXT_device_query"))
        return;

    EGLDeviceEXT eglDevice;
    if (!eglQueryDisplayAttribEXT(eglDisplay, EGL_DEVICE_EXT, reinterpret_cast<EGLAttrib*>(&eglDevice)))
        return;

    const char* extensions = eglQueryDeviceStringEXT(eglDevice, EGL_EXTENSIONS);
    if (epoxy_extension_in_string(extensions, "EGL_EXT_device_drm"))
        priv->drmDevice = eglQueryDeviceStringEXT(eglDevice, EGL_DRM_DEVICE_FILE_EXT);
    if (epoxy_extension_in_string(extensions, "EGL_EXT_device_drm_render_node"))
        priv->drmRenderNode = eglQueryDeviceStringEXT(eglDevice, EGL_DRM_RENDER_NODE_FILE_EXT);
}

static gboolean wpeDisplayWaylandConnect(WPEDisplay* display, GError** error)
{
    auto* displayWayland = WPE_DISPLAY_WAYLAND(display);
    auto* priv = displayWayland->priv;
    if (priv->wlDisplay) {
        g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "Wayland display is already connected");
        return FALSE;
    }

    priv->wlDisplay = wl_display_connect(nullptr);
    if (!priv->wlDisplay) {
        g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "Failed to connect to default Wayland display");
        return FALSE;
    }

    priv->eventSource = wpeDisplayWaylandCreateEventSource(displayWayland);

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
        priv->wlCursor = makeUnique<WPE::WaylandCursor>(displayWayland);
        priv->wlSeat->startListening();
    }

    if (priv->linuxDMABuf) {
#if USE(LIBDRM)
        if (zwp_linux_dmabuf_v1_get_version(priv->linuxDMABuf) >= ZWP_LINUX_DMABUF_V1_GET_DEFAULT_FEEDBACK_SINCE_VERSION) {
            priv->dmabufFeedback = zwp_linux_dmabuf_v1_get_default_feedback(priv->linuxDMABuf);
            zwp_linux_dmabuf_feedback_v1_add_listener(priv->dmabufFeedback, &linuxDMABufFeedbackListener, display);
        }
#endif
        zwp_linux_dmabuf_v1_add_listener(priv->linuxDMABuf, &linuxDMABufListener, display);
        wl_display_roundtrip(priv->wlDisplay);
    }

    if (priv->drmDevice.isNull())
        wpeDisplayWaylandInitializeDRMDeviceFromEGL(displayWayland);

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

static guint wpeDisplayWaylandGetNMonitors(WPEDisplay* display)
{
    return WPE_DISPLAY_WAYLAND(display)->priv->monitors.size();
}

static WPEMonitor* wpeDisplayWaylandGetMonitor(WPEDisplay* display, guint index)
{
    auto* priv = WPE_DISPLAY_WAYLAND(display)->priv;
    if (priv->monitors.isEmpty() || index >= priv->monitors.size())
        return nullptr;

    return priv->monitors[index].get();
}

static const char* wpeDisplayWaylandGetDRMDevice(WPEDisplay* display)
{
    return WPE_DISPLAY_WAYLAND(display)->priv->drmDevice.data();
}

static const char* wpeDisplayWaylandGetDRMRenderNode(WPEDisplay* display)
{
    auto* priv = WPE_DISPLAY_WAYLAND(display)->priv;
    if (!priv->drmRenderNode.isNull())
        return priv->drmRenderNode.data();
    return priv->drmDevice.data();
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

WPEMonitor* wpeDisplayWaylandFindMonitor(WPEDisplayWayland* display, struct wl_output* output)
{
    for (const auto& monitor : display->priv->monitors) {
        if (wpe_monitor_wayland_get_wl_output(WPE_MONITOR_WAYLAND(monitor.get())) == output)
            return monitor.get();
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
    displayClass->get_n_monitors = wpeDisplayWaylandGetNMonitors;
    displayClass->get_monitor = wpeDisplayWaylandGetMonitor;
    displayClass->get_drm_device = wpeDisplayWaylandGetDRMDevice;
    displayClass->get_drm_render_node = wpeDisplayWaylandGetDRMRenderNode;
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

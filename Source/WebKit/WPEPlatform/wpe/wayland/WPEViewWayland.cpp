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
#include "WPEViewWayland.h"

#include "WPEBufferDMABufFormats.h"
#include "WPEDisplayWaylandPrivate.h"
#include "WPEWaylandSHMPool.h"
#include "linux-dmabuf-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include <fcntl.h>
#include <gio/gio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <wtf/FastMalloc.h>
#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

// These includes need to be in this order because wayland-egl.h defines WL_EGL_PLATFORM
// and egl.h checks that to decide whether it's Wayland platform.
#include <wayland-client-protocol.h>
#include <wayland-egl.h>
#include <epoxy/egl.h>

#if USE(LIBDRM)
#include <xf86drm.h>
#endif

#ifndef EGL_WL_create_wayland_buffer_from_image
typedef struct wl_buffer *(EGLAPIENTRYP PFNEGLCREATEWAYLANDBUFFERFROMIMAGEWL)(EGLDisplay dpy, EGLImageKHR image);
#endif

struct DMABufFeedback {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    struct FormatTable {
        struct Data {
            uint32_t format { 0 };
            uint32_t padding { 0 };
            uint64_t modifier { 0 };
        };

        FormatTable(const FormatTable&) = delete;
        FormatTable& operator=(const FormatTable&) = delete;
        FormatTable(FormatTable&& other)
        {
            *this = WTFMove(other);
        }

        FormatTable(unsigned size, Data* data)
            : size(size)
            , data(data)
        {
        }

        explicit FormatTable() = default;
        explicit FormatTable(uint32_t size, int fd)
            : size(size)
            , data(static_cast<FormatTable::Data*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0)))
        {
            if (data == MAP_FAILED) {
                data = nullptr;
                size = 0;
            }
        }

        explicit operator bool() const
        {
            return size && data;
        }

        FormatTable& operator=(FormatTable&& other)
        {
            if (data != other.data) {
                if (data)
                    munmap(data, size);
                data = std::exchange(other.data, nullptr);
                size = std::exchange(other.size, 0);
            }
            return *this;
        }

        ~FormatTable()
        {
            if (data)
                munmap(data, size);
        }

        unsigned size { 0 };
        Data* data { nullptr };
    };

    DMABufFeedback()
    {
#if USE(LIBDRM)
        memset(&mainDevice, 0, sizeof(dev_t));
#endif
    }

    ~DMABufFeedback() = default;

    DMABufFeedback(const DMABufFeedback&) = delete;
    DMABufFeedback& operator=(const DMABufFeedback&) = delete;

    DMABufFeedback(DMABufFeedback&& other)
        : formatTable(WTFMove(other.formatTable))
        , pendingTranche(WTFMove(other.pendingTranche))
        , tranches(WTFMove(other.tranches))
    {
#if USE(LIBDRM)
        memcpy(&mainDevice, &other.mainDevice, sizeof(dev_t));
        memset(&other.mainDevice, 0, sizeof(dev_t));
#endif
    }

#if USE(LIBDRM)
    static CString drmDeviceForUsage(const dev_t* device, bool isScanout)
    {
        drmDevicePtr drmDevice;
        if (drmGetDeviceFromDevId(*device, 0, &drmDevice))
            return { };

        CString returnValue;
        if (isScanout) {
            if (drmDevice->available_nodes & (1 << DRM_NODE_PRIMARY))
                returnValue = drmDevice->nodes[DRM_NODE_PRIMARY];
        } else {
            if (drmDevice->available_nodes & (1 << DRM_NODE_RENDER))
                returnValue = drmDevice->nodes[DRM_NODE_RENDER];
            else if (drmDevice->available_nodes & (1 << DRM_NODE_PRIMARY))
                returnValue = drmDevice->nodes[DRM_NODE_PRIMARY];
        }

        drmFreeDevice(&drmDevice);

        return returnValue;
    }
#endif

    CString drmDevice() const
    {
#if USE(LIBDRM)
        return drmDeviceForUsage(&mainDevice, false);
#else
        return { };
#endif
    }

    struct Tranche {
        Tranche()
        {
#if USE(LIBDRM)
            memset(&targetDevice, 0, sizeof(dev_t));
#endif
        }
        ~Tranche() = default;
        Tranche(const Tranche&) = delete;
        Tranche& operator=(const Tranche&) = delete;
        Tranche(Tranche&& other)
            : flags(other.flags)
            , formats(WTFMove(other.formats))
        {
            other.flags = 0;
#if USE(LIBDRM)
            memcpy(&targetDevice, &other.targetDevice, sizeof(dev_t));
            memset(&other.targetDevice, 0, sizeof(dev_t));
#endif
        }

        CString drmDevice() const
        {
#if USE(LIBDRM)
            return drmDeviceForUsage(&targetDevice, flags & ZWP_LINUX_DMABUF_FEEDBACK_V1_TRANCHE_FLAGS_SCANOUT);
#else
            return { };
#endif
        }

        uint32_t flags { 0 };
        Vector<uint16_t> formats;
#if USE(LIBDRM)
        dev_t targetDevice;
#endif
    };

    std::pair<uint32_t, uint64_t> format(uint16_t index)
    {
        ASSERT(index < formatTable.size);
        if (UNLIKELY(index >= formatTable.size))
            return { 0, 0 };

        return { formatTable.data[index].format, formatTable.data[index].modifier };
    }

    FormatTable formatTable;
    Tranche pendingTranche;
    Vector<Tranche> tranches;
#if USE(LIBDRM)
    dev_t mainDevice;
#endif
};

/**
 * WPEViewWayland:
 *
 */
struct _WPEViewWaylandPrivate {
    struct wl_surface* wlSurface;
    struct xdg_surface* xdgSurface;
    struct xdg_toplevel* xdgToplevel;
    struct zwp_linux_dmabuf_feedback_v1* dmabufFeedback;
    std::unique_ptr<DMABufFeedback> pendingDMABufFeedback;
    std::unique_ptr<DMABufFeedback> committedDMABufFeedback;
    GRefPtr<WPEBufferDMABufFormats> preferredDMABufFormats;

    Vector<GRefPtr<WPEMonitor>, 1> monitors;
    GRefPtr<WPEMonitor> currentMonitor;

    GRefPtr<WPEBuffer> buffer;
    struct wl_callback* frameCallback;

    struct {
        std::optional<uint32_t> width;
        std::optional<uint32_t> height;
        WPEViewState state { WPE_VIEW_STATE_NONE };
    } pendingState;

    struct {
        std::optional<uint32_t> width;
        std::optional<uint32_t> height;
    } savedSize;

    struct {
        Vector<WPERectangle, 1> rects;
        bool dirty;
    } pendingOpaqueRegion;
};
WEBKIT_DEFINE_FINAL_TYPE(WPEViewWayland, wpe_view_wayland, WPE_TYPE_VIEW, WPEView)

static void wpeViewWaylandSaveSize(WPEView* view)
{
    auto state = wpe_view_get_state(view);
    if (state & (WPE_VIEW_STATE_FULLSCREEN | WPE_VIEW_STATE_MAXIMIZED))
        return;

    auto* priv = WPE_VIEW_WAYLAND(view)->priv;
    priv->savedSize.width = wpe_view_get_width(view);
    priv->savedSize.height = wpe_view_get_height(view);
}

const struct xdg_surface_listener xdgSurfaceListener = {
    // configure
    [](void* data, struct xdg_surface* surface, uint32_t serial)
    {
        auto* view = WPE_VIEW(data);
        auto* priv = WPE_VIEW_WAYLAND(view)->priv;

        bool isFixedSize = priv->pendingState.state & (WPE_VIEW_STATE_FULLSCREEN | WPE_VIEW_STATE_MAXIMIZED);
        bool wasFixedSize = wpe_view_get_state(view) & (WPE_VIEW_STATE_FULLSCREEN | WPE_VIEW_STATE_MAXIMIZED);
        auto width = priv->pendingState.width;
        auto height = priv->pendingState.height;
        bool useSavedSize = !width.has_value() && !height.has_value();
        if (useSavedSize && !isFixedSize && wasFixedSize) {
            width = priv->savedSize.width;
            height = priv->savedSize.height;
        }

        if (width.has_value() && height.has_value()) {
            if (!useSavedSize)
                wpeViewWaylandSaveSize(view);
            wpe_view_resized(view, width.value(), height.value());
        }

        wpe_view_state_changed(view, priv->pendingState.state);
        priv->pendingState = { };
        xdg_surface_ack_configure(surface, serial);
    },
};

const struct xdg_toplevel_listener xdgToplevelListener = {
    // configure
    [](void* data, struct xdg_toplevel*, int32_t width, int32_t height, struct wl_array* states)
    {
        auto* view = WPE_VIEW_WAYLAND(data);
        if (width && height) {
            view->priv->pendingState.width = width;
            view->priv->pendingState.height = height;
        }

        uint32_t pendingState = 0;
        const char* end = static_cast<const char*>(states->data) + states->size;
        for (uint32_t* state = static_cast<uint32_t*>(states->data); reinterpret_cast<const char*>(state) < end; ++state) {
            switch (*state) {
            case XDG_TOPLEVEL_STATE_FULLSCREEN:
                pendingState |= WPE_VIEW_STATE_FULLSCREEN;
                break;
            case XDG_TOPLEVEL_STATE_MAXIMIZED:
                pendingState |= WPE_VIEW_STATE_MAXIMIZED;
                break;
            default:
                break;
            }
        }

        view->priv->pendingState.state = static_cast<WPEViewState>(view->priv->pendingState.state | pendingState);
    },
    // close
    [](void* data, struct xdg_toplevel*)
    {
        wpe_view_closed(WPE_VIEW(data));
    },
#ifdef XDG_TOPLEVEL_CONFIGURE_BOUNDS_SINCE_VERSION
    // configure_bounds
    [](void*, struct xdg_toplevel*, int32_t, int32_t)
    {
    },
#endif
#ifdef XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION
    // wm_capabilities
    [](void*, struct xdg_toplevel*, struct wl_array*)
    {
    },
#endif
};

static void wpeViewWaylandUpdateScale(WPEViewWayland* view)
{
    if (view->priv->monitors.isEmpty())
        return;

    double scale = 1;
    for (const auto& monitor : view->priv->monitors)
        scale = std::max(scale, wpe_monitor_get_scale(monitor.get()));

    if (wl_surface_get_version(view->priv->wlSurface) >= WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION)
        wl_surface_set_buffer_scale(view->priv->wlSurface, scale);

    wpe_view_scale_changed(WPE_VIEW(view), scale);
}

static const struct wl_surface_listener surfaceListener = {
    // enter
    [](void* data, struct wl_surface*, struct wl_output* wlOutput)
    {
        auto* view = WPE_VIEW_WAYLAND(data);
        auto* monitor = wpeDisplayWaylandFindMonitor(WPE_DISPLAY_WAYLAND(wpe_view_get_display(WPE_VIEW(view))), wlOutput);
        if (!monitor)
            return;

        // For now we just use the last entered monitor as current, but we could do someting smarter.
        bool monitorChanged = false;
        if (view->priv->currentMonitor.get() != monitor) {
            view->priv->currentMonitor = monitor;
            monitorChanged = true;
        }
        view->priv->monitors.append(monitor);
        wpeViewWaylandUpdateScale(view);
        if (monitorChanged) {
            wpe_view_map(WPE_VIEW(view));
            g_object_notify(G_OBJECT(view), "monitor");
        }
        g_signal_connect_object(monitor, "notify::scale", G_CALLBACK(+[](WPEViewWayland* view) {
            wpeViewWaylandUpdateScale(view);
        }), view, G_CONNECT_SWAPPED);
    },
    // leave
    [](void* data, struct wl_surface*, struct wl_output* wlOutput)
    {
        auto* view = WPE_VIEW_WAYLAND(data);
        auto* monitor = wpeDisplayWaylandFindMonitor(WPE_DISPLAY_WAYLAND(wpe_view_get_display(WPE_VIEW(view))), wlOutput);
        if (!monitor)
            return;

        view->priv->monitors.removeLast(monitor);
        if (!view->priv->monitors.isEmpty())
            view->priv->currentMonitor = view->priv->monitors.last();
        else
            view->priv->currentMonitor = nullptr;
        wpeViewWaylandUpdateScale(view);
        if (!view->priv->currentMonitor)
            wpe_view_unmap(WPE_VIEW(view));
        g_object_notify(G_OBJECT(view), "monitor");
        g_signal_handlers_disconnect_by_data(monitor, view);
    },
#ifdef WL_SURFACE_PREFERRED_BUFFER_SCALE_SINCE_VERSION
    // preferred_buffer_scale
    [](void*, struct wl_surface*, int /* factor */)
    {
    },
#endif
#ifdef WL_SURFACE_PREFERRED_BUFFER_TRANSFORM_SINCE_VERSION
    // preferred_buffer_transform
    [](void*, struct wl_surface*, uint32_t) {
    },
#endif
};

static const struct zwp_linux_dmabuf_feedback_v1_listener linuxDMABufFeedbackListener = {
    // done
    [](void* data, struct zwp_linux_dmabuf_feedback_v1*)
    {
        auto* view = WPE_VIEW_WAYLAND(data);
        if (!view->priv->pendingDMABufFeedback)
            return;

        // The compositor might not have sent the formats table. In that case, try to reuse the previous
        // one. Return early and skip emitting the signal if there is no usable formats table in the end.
        if (!view->priv->pendingDMABufFeedback->formatTable) {
            if (view->priv->committedDMABufFeedback && view->priv->committedDMABufFeedback->formatTable)
                view->priv->pendingDMABufFeedback->formatTable = WTFMove(view->priv->committedDMABufFeedback->formatTable);
            else {
                view->priv->pendingDMABufFeedback.reset();
                return;
            }
        }

        view->priv->committedDMABufFeedback = WTFMove(view->priv->pendingDMABufFeedback);
        view->priv->preferredDMABufFormats = nullptr;
        g_signal_emit_by_name(view, "preferred-dma-buf-formats-changed");
    },
    // format_table
    [](void* data, struct zwp_linux_dmabuf_feedback_v1*, int32_t fd, uint32_t size)
    {
        // The protocol specification is not clear about the ordering of the format_table and main_device
        // events. Err on the safer side and allow any of them to create the pending feedback instance.
        auto* priv = WPE_VIEW_WAYLAND(data)->priv;
        if (!priv->pendingDMABufFeedback)
            priv->pendingDMABufFeedback = makeUnique<DMABufFeedback>();

        priv->pendingDMABufFeedback->formatTable = DMABufFeedback::FormatTable(size, fd);
        close(fd);
    },
    // main_device
    [](void* data, struct zwp_linux_dmabuf_feedback_v1*, struct wl_array* device)
    {
        auto* priv = WPE_VIEW_WAYLAND(data)->priv;
        if (!priv->pendingDMABufFeedback)
            priv->pendingDMABufFeedback = makeUnique<DMABufFeedback>();

#if USE(LIBDRM)
        memcpy(&priv->pendingDMABufFeedback->mainDevice, device->data, sizeof(dev_t));
#endif
    },
    // tranche_done
    [](void* data, struct zwp_linux_dmabuf_feedback_v1*)
    {
        auto* priv = WPE_VIEW_WAYLAND(data)->priv;
        if (!priv->pendingDMABufFeedback)
            return;

        priv->pendingDMABufFeedback->tranches.append(WTFMove(priv->pendingDMABufFeedback->pendingTranche));
    },
    // tranche_target_device
    [](void* data, struct zwp_linux_dmabuf_feedback_v1*, struct wl_array* device)
    {
#if USE(LIBDRM)
        auto* priv = WPE_VIEW_WAYLAND(data)->priv;
        if (!priv->pendingDMABufFeedback)
            return;

        memcpy(&priv->pendingDMABufFeedback->pendingTranche.targetDevice, device->data, sizeof(dev_t));
#endif
    },
    // tranche_formats
    [](void* data, struct zwp_linux_dmabuf_feedback_v1*, struct wl_array* indices)
    {
        auto* priv = WPE_VIEW_WAYLAND(data)->priv;
        if (!priv->pendingDMABufFeedback)
            return;

        const char* end = static_cast<const char*>(indices->data) + indices->size;
        for (uint16_t* index = static_cast<uint16_t*>(indices->data); reinterpret_cast<const char*>(index) < end; ++index)
            priv->pendingDMABufFeedback->pendingTranche.formats.append(*index);
    },
    // tranche_flags
    [](void* data, struct zwp_linux_dmabuf_feedback_v1*, uint32_t flags)
    {
        auto* priv = WPE_VIEW_WAYLAND(data)->priv;
        if (priv->pendingDMABufFeedback)
            priv->pendingDMABufFeedback->pendingTranche.flags |= flags;
    }
};

static void wpeViewWaylandConstructed(GObject* object)
{
    G_OBJECT_CLASS(wpe_view_wayland_parent_class)->constructed(object);

    auto* view = WPE_VIEW(object);
    auto* display = WPE_DISPLAY_WAYLAND(wpe_view_get_display(view));
    auto* priv = WPE_VIEW_WAYLAND(object)->priv;
    auto* wlCompositor = wpe_display_wayland_get_wl_compositor(display);
    priv->wlSurface = wl_compositor_create_surface(wlCompositor);
    wl_surface_add_listener(priv->wlSurface, &surfaceListener, object);
    if (auto* xdgWMBase = wpeDisplayWaylandGetXDGWMBase(display)) {
        priv->xdgSurface = xdg_wm_base_get_xdg_surface(xdgWMBase, priv->wlSurface);
        xdg_surface_add_listener(priv->xdgSurface, &xdgSurfaceListener, object);
        if (auto* xdgToplevel = xdg_surface_get_toplevel(priv->xdgSurface)) {
            priv->xdgToplevel = xdgToplevel;
            xdg_toplevel_add_listener(priv->xdgToplevel, &xdgToplevelListener, object);
            xdg_toplevel_set_title(priv->xdgToplevel, "WPEDMABuf"); // FIXME
            wl_surface_commit(priv->wlSurface);
        }
    }

    auto* dmabuf = wpeDisplayWaylandGetLinuxDMABuf(display);
    if (dmabuf && zwp_linux_dmabuf_v1_get_version(dmabuf) >= ZWP_LINUX_DMABUF_V1_GET_SURFACE_FEEDBACK_SINCE_VERSION) {
        priv->dmabufFeedback = zwp_linux_dmabuf_v1_get_surface_feedback(dmabuf, priv->wlSurface);
        zwp_linux_dmabuf_feedback_v1_add_listener(priv->dmabufFeedback, &linuxDMABufFeedbackListener, object);
    }

    wl_display_roundtrip(wpe_display_wayland_get_wl_display(display));

    // Set the first monitor as the default one until enter monitor is emitted.
    if (wpe_display_get_n_monitors(WPE_DISPLAY(display))) {
        priv->currentMonitor = wpe_display_get_monitor(WPE_DISPLAY(display), 0);
        wpe_view_map(view);
        auto scale = wpe_monitor_get_scale(priv->currentMonitor.get());
        if (wl_surface_get_version(priv->wlSurface) >= WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION)
            wl_surface_set_buffer_scale(priv->wlSurface, scale);
        wpe_view_scale_changed(view, scale);
    }

    // The web view default background color is opaque white, so set the whole view region as opaque initially.
    priv->pendingOpaqueRegion.rects.append({ 0, 0, wpe_view_get_width(view), wpe_view_get_height(view) });
    priv->pendingOpaqueRegion.dirty = true;
}

static void wpeViewWaylandDispose(GObject* object)
{
    auto* priv = WPE_VIEW_WAYLAND(object)->priv;
    priv->currentMonitor = nullptr;
    priv->monitors.clear();
    g_clear_pointer(&priv->xdgToplevel, xdg_toplevel_destroy);
    g_clear_pointer(&priv->dmabufFeedback, zwp_linux_dmabuf_feedback_v1_destroy);
    g_clear_pointer(&priv->xdgSurface, xdg_surface_destroy);
    g_clear_pointer(&priv->wlSurface, wl_surface_destroy);
    g_clear_pointer(&priv->frameCallback, wl_callback_destroy);

    G_OBJECT_CLASS(wpe_view_wayland_parent_class)->dispose(object);
}

static const struct wl_buffer_listener bufferListener = {
    // release
    [](void* userData, struct wl_buffer*)
    {
        auto* buffer = WPE_BUFFER(userData);
        wpe_view_buffer_released(wpe_buffer_get_view(buffer), buffer);
    }
};

static struct wl_buffer* createWaylandBufferFromEGLImage(WPEView* view, WPEBuffer* buffer, GError** error)
{
    GUniqueOutPtr<GError> bufferError;
    auto* eglDisplay = wpe_display_get_egl_display(wpe_view_get_display(view), &bufferError.outPtr());
    if (!eglDisplay) {
        g_set_error(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render buffer: can't create Wayland buffer because failed to get EGL display: %s", bufferError->message);
        return nullptr;
    }

    auto eglImage = wpe_buffer_import_to_egl_image(buffer, &bufferError.outPtr());
    if (!eglImage) {
        g_set_error(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render buffer: failed to import buffer to EGL image: %s", bufferError->message);
        return nullptr;
    }

    static PFNEGLCREATEWAYLANDBUFFERFROMIMAGEWL s_eglCreateWaylandBufferFromImageWL;
    if (!s_eglCreateWaylandBufferFromImageWL) {
        if (epoxy_has_egl_extension(eglDisplay, "EGL_WL_create_wayland_buffer_from_image"))
            s_eglCreateWaylandBufferFromImageWL = reinterpret_cast<PFNEGLCREATEWAYLANDBUFFERFROMIMAGEWL>(epoxy_eglGetProcAddress("eglCreateWaylandBufferFromImageWL"));
    }
    if (!s_eglCreateWaylandBufferFromImageWL) {
        g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render buffer: eglCreateWaylandBufferFromImageWL is not available");
        return nullptr;
    }

    if (auto* wlBuffer = s_eglCreateWaylandBufferFromImageWL(eglDisplay, eglImage))
        return wlBuffer;

    g_set_error(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render buffer: eglCreateWaylandBufferFromImageWL failed with error %#04x", eglGetError());
    return nullptr;
}

static struct wl_buffer* createWaylandBufferFromDMABuf(WPEView* view, WPEBuffer* buffer, GError** error)
{
    if (auto* wlBuffer = static_cast<struct wl_buffer*>(wpe_buffer_get_user_data(buffer)))
        return wlBuffer;

    struct wl_buffer* wlBuffer = nullptr;
    if (auto* dmabuf = wpeDisplayWaylandGetLinuxDMABuf(WPE_DISPLAY_WAYLAND(wpe_view_get_display(view)))) {
        auto* bufferDMABuf = WPE_BUFFER_DMA_BUF(buffer);
        auto modifier = wpe_buffer_dma_buf_get_modifier(bufferDMABuf);
        auto* params = zwp_linux_dmabuf_v1_create_params(dmabuf);
        auto planeCount = wpe_buffer_dma_buf_get_n_planes(bufferDMABuf);
        for (guint32 i = 0; i < planeCount; ++i) {
            zwp_linux_buffer_params_v1_add(params, wpe_buffer_dma_buf_get_fd(bufferDMABuf, i), i, wpe_buffer_dma_buf_get_offset(bufferDMABuf, i),
                wpe_buffer_dma_buf_get_stride(bufferDMABuf, i), modifier >> 32, modifier & 0xffffffff);
        }
        wlBuffer = zwp_linux_buffer_params_v1_create_immed(params, wpe_buffer_get_width(buffer), wpe_buffer_get_height(buffer),
            wpe_buffer_dma_buf_get_format(bufferDMABuf), 0);
        zwp_linux_buffer_params_v1_destroy(params);

        if (!wlBuffer) {
            g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render buffer: failed to create Wayland buffer from DMABuf");
            return nullptr;
        }
    } else {
        wlBuffer = createWaylandBufferFromEGLImage(view, buffer, error);
        if (!wlBuffer)
            return nullptr;
    }

    wl_buffer_add_listener(wlBuffer, &bufferListener, buffer);

    wpe_buffer_set_user_data(buffer, wlBuffer, reinterpret_cast<GDestroyNotify>(wl_buffer_destroy));
    return wlBuffer;
}

struct SharedMemoryBuffer {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    std::unique_ptr<WPE::WaylandSHMPool> wlPool;
    struct wl_buffer* wlBuffer;
};

static void sharedMemoryBufferDestroy(SharedMemoryBuffer* buffer)
{
    g_clear_pointer(&buffer->wlBuffer, wl_buffer_destroy);
    delete buffer;
}

static SharedMemoryBuffer* sharedMemoryBufferCreate(WPEDisplayWayland* display, GBytes* bytes, int width, int height, unsigned stride)
{
    auto size = g_bytes_get_size(bytes);
    auto wlPool = WPE::WaylandSHMPool::create(wpe_display_wayland_get_wl_shm(display), size);
    if (!wlPool)
        return nullptr;

    auto offset = wlPool->allocate(size);
    if (offset < 0)
        return nullptr;

    memcpy(reinterpret_cast<char*>(wlPool->data()) + offset, g_bytes_get_data(bytes, nullptr), size);

    auto* sharedMemoryBuffer = new SharedMemoryBuffer();
    sharedMemoryBuffer->wlPool = WTFMove(wlPool);
    sharedMemoryBuffer->wlBuffer = sharedMemoryBuffer->wlPool->createBuffer(offset, width, height, stride);
    return sharedMemoryBuffer;
}

static struct wl_buffer* createWaylandBufferSHM(WPEView* view, WPEBuffer* buffer, GError** error)
{
    if (auto* sharedMemoryBuffer = static_cast<SharedMemoryBuffer*>(wpe_buffer_get_user_data(buffer))) {
        GBytes* bytes = wpe_buffer_shm_get_data(WPE_BUFFER_SHM(buffer));
        memcpy(reinterpret_cast<char*>(sharedMemoryBuffer->wlPool->data()), g_bytes_get_data(bytes, nullptr), sharedMemoryBuffer->wlPool->size());
        return sharedMemoryBuffer->wlBuffer;
    }

    auto* bufferSHM = WPE_BUFFER_SHM(buffer);
    if (wpe_buffer_shm_get_format(bufferSHM) != WPE_PIXEL_FORMAT_ARGB8888) {
        g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render buffer: can't create Wayland buffer because format is unsupported");
        return nullptr;
    }

    auto* display = WPE_DISPLAY_WAYLAND(wpe_view_get_display(view));
    auto* sharedMemoryBuffer = sharedMemoryBufferCreate(display, wpe_buffer_shm_get_data(bufferSHM),
        wpe_buffer_get_width(buffer), wpe_buffer_get_height(buffer), wpe_buffer_shm_get_stride(bufferSHM));
    if (!sharedMemoryBuffer) {
        g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render buffer: can't create Wayland buffer because failed to create shared memory");
        return nullptr;
    }

    wl_buffer_add_listener(sharedMemoryBuffer->wlBuffer, &bufferListener, buffer);

    wpe_buffer_set_user_data(buffer, sharedMemoryBuffer, reinterpret_cast<GDestroyNotify>(sharedMemoryBufferDestroy));
    return sharedMemoryBuffer->wlBuffer;
}

static struct wl_buffer* createWaylandBuffer(WPEView* view, WPEBuffer* buffer, GError** error)
{
    struct wl_buffer* wlBuffer = nullptr;
    if (WPE_IS_BUFFER_DMA_BUF(buffer))
        wlBuffer = createWaylandBufferFromDMABuf(view, buffer, error);
    else if (WPE_IS_BUFFER_SHM(buffer))
        wlBuffer = createWaylandBufferSHM(view, buffer, error);
    else
        RELEASE_ASSERT_NOT_REACHED();

    return wlBuffer;
}

const struct wl_callback_listener frameListener = {
    // frame
    [](void* data, struct wl_callback* callback, uint32_t)
    {
        auto* view = WPE_VIEW(data);
        auto* priv = WPE_VIEW_WAYLAND(view)->priv;
        RELEASE_ASSERT(!priv->frameCallback || priv->frameCallback == callback);
        g_clear_pointer(&priv->frameCallback, wl_callback_destroy);

        wpe_view_buffer_rendered(view, priv->buffer.get());
        priv->buffer = nullptr;
    }
};

static gboolean wpeViewWaylandRenderBuffer(WPEView* view, WPEBuffer* buffer, const WPERectangle* damageRects, guint nDamageRects, GError** error)
{
    auto* wlBuffer = createWaylandBuffer(view, buffer, error);
    if (!wlBuffer)
        return FALSE;

    auto* priv = WPE_VIEW_WAYLAND(view)->priv;
    priv->buffer = buffer;

    auto* wlSurface = wpe_view_wayland_get_wl_surface(WPE_VIEW_WAYLAND(view));
    auto* wlCompositor = wpe_display_wayland_get_wl_compositor(WPE_DISPLAY_WAYLAND(wpe_view_get_display(view)));

    if (priv->pendingOpaqueRegion.dirty) {
        struct wl_region* region = nullptr;
        if (!priv->pendingOpaqueRegion.rects.isEmpty()) {
            region = wl_compositor_create_region(wlCompositor);
            if (region) {
                for (const auto& rect : priv->pendingOpaqueRegion.rects)
                    wl_region_add(region, rect.x, rect.y, rect.width, rect.height);
            }
        }

        wl_surface_set_opaque_region(wlSurface, region);
        if (region)
            wl_region_destroy(region);

        priv->pendingOpaqueRegion.rects.clear();
        priv->pendingOpaqueRegion.dirty = false;
    }

    wl_surface_attach(wlSurface, wlBuffer, 0, 0);
    if (nDamageRects && LIKELY(wl_compositor_get_version(wlCompositor) >= 4)) {
        ASSERT(damageRects);
        for (unsigned i = 0; i < nDamageRects; ++i)
            wl_surface_damage_buffer(wlSurface, damageRects[i].x, damageRects[i].y, damageRects[i].width, damageRects[i].height);
    } else
        wl_surface_damage(wlSurface, 0, 0, INT32_MAX, INT32_MAX);

    priv->frameCallback = wl_surface_frame(wlSurface);
    wl_callback_add_listener(priv->frameCallback, &frameListener, view);
    wl_surface_commit(wlSurface);

    return TRUE;
}

static WPEMonitor* wpeViewWaylandGetMonitor(WPEView* view)
{
    auto* priv = WPE_VIEW_WAYLAND(view)->priv;
    return priv->currentMonitor.get();
}

static gboolean wpeViewWaylandResize(WPEView* view, int width, int height)
{
    auto* priv = WPE_VIEW_WAYLAND(view)->priv;
    if (!priv->xdgToplevel)
        return FALSE;

    wpe_view_resized(view, width, height);
    return TRUE;
}

static gboolean wpeViewWaylandSetFullscreen(WPEView* view, gboolean fullscreen)
{
    auto* priv = WPE_VIEW_WAYLAND(view)->priv;
    if (!priv->xdgToplevel)
        return FALSE;

    if (fullscreen) {
        wpeViewWaylandSaveSize(view);
        xdg_toplevel_set_fullscreen(priv->xdgToplevel, nullptr);
        return TRUE;
    }

    xdg_toplevel_unset_fullscreen(priv->xdgToplevel);
    return TRUE;
}

static gboolean wpeViewWaylandSetMaximized(WPEView* view, gboolean maximized)
{
    auto* priv = WPE_VIEW_WAYLAND(view)->priv;
    if (!priv->xdgToplevel)
        return FALSE;

    if (maximized) {
        wpeViewWaylandSaveSize(view);
        xdg_toplevel_set_maximized(priv->xdgToplevel);
        return TRUE;
    }

    xdg_toplevel_unset_maximized(priv->xdgToplevel);
    return TRUE;
}

static WPEBufferDMABufFormats* wpeViewWaylandGetPreferredDMABufFormats(WPEView* view)
{
    auto* priv = WPE_VIEW_WAYLAND(view)->priv;
    if (priv->preferredDMABufFormats)
        return priv->preferredDMABufFormats.get();

    if (!priv->committedDMABufFeedback)
        return nullptr;

    auto mainDevice = priv->committedDMABufFeedback->drmDevice();
    auto* builder = wpe_buffer_dma_buf_formats_builder_new(mainDevice.data());
    for (const auto& tranche : priv->committedDMABufFeedback->tranches) {
        WPEBufferDMABufFormatUsage usage = tranche.flags & ZWP_LINUX_DMABUF_FEEDBACK_V1_TRANCHE_FLAGS_SCANOUT ? WPE_BUFFER_DMA_BUF_FORMAT_USAGE_SCANOUT : WPE_BUFFER_DMA_BUF_FORMAT_USAGE_RENDERING;
        auto targetDevice = tranche.drmDevice();
        wpe_buffer_dma_buf_formats_builder_append_group(builder, targetDevice.data(), usage);

        for (const auto& format : tranche.formats) {
            auto [fourcc, modifier] = priv->committedDMABufFeedback->format(format);
            if (LIKELY(fourcc))
                wpe_buffer_dma_buf_formats_builder_append_format(builder, fourcc, modifier);
        }
    }

    priv->preferredDMABufFormats = adoptGRef(wpe_buffer_dma_buf_formats_builder_end(builder));
    return priv->preferredDMABufFormats.get();
}

static void wpeViewWaylandSetCursorFromName(WPEView* view, const char* name)
{
    if (auto* cursor = wpeDisplayWaylandGetCursor(WPE_DISPLAY_WAYLAND(wpe_view_get_display(view))))
        cursor->setFromName(name, wpe_view_get_scale(view));
}

static const struct wl_buffer_listener cursorBufferListener = {
    // release
    [](void* userData, struct wl_buffer*)
    {
        sharedMemoryBufferDestroy(static_cast<SharedMemoryBuffer*>(userData));
    }
};

static void wpeViewWaylandSetCursorFromBytes(WPEView* view, GBytes* bytes, guint width, guint height, guint stride, guint hotspotX, guint hotspotY)
{
    auto* display = WPE_DISPLAY_WAYLAND(wpe_view_get_display(view));
    auto* cursor = wpeDisplayWaylandGetCursor(display);
    if (!cursor)
        return;

    auto* sharedMemoryBuffer = sharedMemoryBufferCreate(display, bytes, width, height, stride);
    if (!sharedMemoryBuffer)
        return;

    wl_buffer_add_listener(sharedMemoryBuffer->wlBuffer, &cursorBufferListener, sharedMemoryBuffer);
    cursor->setFromBuffer(sharedMemoryBuffer->wlBuffer, width, height, hotspotX, hotspotY);
}

static void wpeViewWaylandSetOpaqueRectangles(WPEView* view, WPERectangle* rects, guint rectsCount)
{
    auto* priv = WPE_VIEW_WAYLAND(view)->priv;
    priv->pendingOpaqueRegion.rects.clear();
    if (rects) {
        priv->pendingOpaqueRegion.rects.reserveInitialCapacity(rectsCount);
        for (unsigned i = 0; i < rectsCount; ++i)
            priv->pendingOpaqueRegion.rects.append(rects[i]);
    }
    priv->pendingOpaqueRegion.dirty = true;
}

static gboolean wpeViewWaylandCanBeMapped(WPEView* view)
{
    auto* priv = WPE_VIEW_WAYLAND(view)->priv;
    return !!priv->currentMonitor.get();
}

static void wpe_view_wayland_class_init(WPEViewWaylandClass* viewWaylandClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(viewWaylandClass);
    objectClass->constructed = wpeViewWaylandConstructed;
    objectClass->dispose = wpeViewWaylandDispose;

    WPEViewClass* viewClass = WPE_VIEW_CLASS(viewWaylandClass);
    viewClass->render_buffer = wpeViewWaylandRenderBuffer;
    viewClass->get_monitor = wpeViewWaylandGetMonitor;
    viewClass->resize = wpeViewWaylandResize;
    viewClass->set_fullscreen = wpeViewWaylandSetFullscreen;
    viewClass->set_maximized = wpeViewWaylandSetMaximized;
    viewClass->get_preferred_dma_buf_formats = wpeViewWaylandGetPreferredDMABufFormats;
    viewClass->set_cursor_from_name = wpeViewWaylandSetCursorFromName;
    viewClass->set_cursor_from_bytes = wpeViewWaylandSetCursorFromBytes;
    viewClass->set_opaque_rectangles = wpeViewWaylandSetOpaqueRectangles;
    viewClass->can_be_mapped = wpeViewWaylandCanBeMapped;
}

/**
 * wpe_view_wayland_new:
 * @display: a #WPEDisplayWayland
 *
 * Create a new #WPEViewWayland
 *
 * Returns: (transfer full): a #WPEView
 */
WPEView* wpe_view_wayland_new(WPEDisplayWayland* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY_WAYLAND(display), nullptr);

    return WPE_VIEW(g_object_new(WPE_TYPE_VIEW_WAYLAND, "display", display, nullptr));
}

/**
 * wpe_view_wayland_get_wl_surface: (skip)
 * @view: a #WPEViewWayland
 *
 * Get the native Wayland view of @view
 *
 * Returns: (transfer none) (nullable): a Wayland `wl_surface`
 */
struct wl_surface* wpe_view_wayland_get_wl_surface(WPEViewWayland* view)
{
    g_return_val_if_fail(WPE_IS_VIEW_WAYLAND(view), nullptr);

    return view->priv->wlSurface;
}

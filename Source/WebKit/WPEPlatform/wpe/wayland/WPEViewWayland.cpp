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

#include "WPEDisplayWaylandPrivate.h"
#include "WPEWaylandOutput.h"
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

#ifndef EGL_WL_create_wayland_buffer_from_image
typedef struct wl_buffer *(EGLAPIENTRYP PFNEGLCREATEWAYLANDBUFFERFROMIMAGEWL)(EGLDisplay dpy, EGLImageKHR image);
#endif

struct DMABufFeedback {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    DMABufFeedback(unsigned size, int fd)
    {
        formatTable.size = size;
        formatTable.data = static_cast<FormatTable::Data*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
    }

    ~DMABufFeedback()
    {
        if (formatTable.data && formatTable.data != MAP_FAILED)
            munmap(formatTable.data, formatTable.size);
    }

    struct FormatTable {
        unsigned size { 0 };
        struct Data {
            uint32_t format { 0 };
            uint32_t padding { 0 };
            uint64_t modifier { 0 };
        } *data { nullptr };
    };

    struct Tranche {
        uint32_t flags { 0 };
        Vector<uint16_t> formats;
    };

    std::pair<uint32_t, uint64_t> format(uint16_t index)
    {
        if (!formatTable.data || formatTable.data == MAP_FAILED)
            return { 0, 0 };
        return { formatTable.data[index].format, formatTable.data[index].modifier };
    }

    FormatTable formatTable;
    Tranche pendingTranche;
    Vector<Tranche> tranches;
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

    Vector<WPE::WaylandOutput*, 1> wlOutputs;

    GRefPtr<WPEBuffer> buffer;
    struct wl_callback* frameCallback;

    struct {
        std::optional<uint32_t> width;
        std::optional<uint32_t> height;
        WPEViewState state { WPE_VIEW_STATE_NONE };
    } pendingState;
};
WEBKIT_DEFINE_FINAL_TYPE(WPEViewWayland, wpe_view_wayland, WPE_TYPE_VIEW, WPEView)

const struct xdg_surface_listener xdgSurfaceListener = {
    // configure
    [](void* data, struct xdg_surface* surface, uint32_t serial)
    {
        auto* view = WPE_VIEW_WAYLAND(data);
        if (view->priv->pendingState.width && view->priv->pendingState.height)
            wpe_view_resize(WPE_VIEW(view), view->priv->pendingState.width.value(), view->priv->pendingState.height.value());
        wpe_view_set_state(WPE_VIEW(view), view->priv->pendingState.state);
        view->priv->pendingState = { };
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
        const auto* stateData = static_cast<uint32_t*>(states->data);
        for (size_t i = 0; i < states->size; i++) {
            uint32_t state = stateData[i];

            switch (state) {
            case XDG_TOPLEVEL_STATE_FULLSCREEN:
                pendingState |= WPE_VIEW_STATE_FULLSCREEN;
                break;
            default:
                break;
            }
        }

        view->priv->pendingState.state = static_cast<WPEViewState>(view->priv->pendingState.state | pendingState);
    },
    // close
    [](void*, struct xdg_toplevel*)
    {
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

static void wpe_view_wayland_update_scale(WPEViewWayland* view)
{
    double scale = 1;
    for (const auto* output : view->priv->wlOutputs)
        scale = std::max(scale, output->scale());

    if (wl_surface_get_version(view->priv->wlSurface) >= WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION)
        wl_surface_set_buffer_scale(view->priv->wlSurface, scale);

    wpe_view_set_scale(WPE_VIEW(view), scale);
}

static const struct wl_surface_listener surfaceListener = {
    // enter
    [](void* data, struct wl_surface*, struct wl_output* wlOutput)
    {
        auto* view = WPE_VIEW_WAYLAND(data);
        auto* output = wpeDisplayWaylandGetOutput(WPE_DISPLAY_WAYLAND(wpe_view_get_display(WPE_VIEW(view))), wlOutput);
        if (!output)
            return;

        view->priv->wlOutputs.append(output);
        wpe_view_wayland_update_scale(view);
        output->addScaleObserver(view, [](WPEViewWayland* view) {
            wpe_view_wayland_update_scale(view);
        });
    },
    // leave
    [](void* data, struct wl_surface*, struct wl_output* wlOutput)
    {
        auto* view = WPE_VIEW_WAYLAND(data);
        auto* output = wpeDisplayWaylandGetOutput(WPE_DISPLAY_WAYLAND(wpe_view_get_display(WPE_VIEW(view))), wlOutput);
        if (!output)
            return;

        view->priv->wlOutputs.removeLast(output);
        wpe_view_wayland_update_scale(view);
        output->removeScaleObserver(view);
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
        view->priv->committedDMABufFeedback = WTFMove(view->priv->pendingDMABufFeedback);
        g_signal_emit_by_name(view, "preferred-dma-buf-formats-changed");
    },
    // format_table
    [](void* data, struct zwp_linux_dmabuf_feedback_v1*, int32_t fd, uint32_t size)
    {
        auto* priv = WPE_VIEW_WAYLAND(data)->priv;
        priv->pendingDMABufFeedback = makeUnique<DMABufFeedback>(size, fd);
        close(fd);
    },
    // main_device
    [](void*, struct zwp_linux_dmabuf_feedback_v1*, struct wl_array*)
    {
        // FIXME: handle main device.
    },
    // tranche_done
    [](void* data, struct zwp_linux_dmabuf_feedback_v1*)
    {
        auto* priv = WPE_VIEW_WAYLAND(data)->priv;
        priv->pendingDMABufFeedback->tranches.append(WTFMove(priv->pendingDMABufFeedback->pendingTranche));
    },
    // tranche_target_device
    [](void*, struct zwp_linux_dmabuf_feedback_v1*, struct wl_array*)
    {
        // FIXME: handle target device.
    },
    // tranche_formats
    [](void* data, struct zwp_linux_dmabuf_feedback_v1*, struct wl_array* indices)
    {
        auto* priv = WPE_VIEW_WAYLAND(data)->priv;
        if (!priv->pendingDMABufFeedback->formatTable.data && priv->committedDMABufFeedback)
            priv->pendingDMABufFeedback->formatTable = WTFMove(priv->committedDMABufFeedback->formatTable);
        if (!priv->pendingDMABufFeedback->formatTable.data || priv->pendingDMABufFeedback->formatTable.data == MAP_FAILED)
            return;

        const char* end = static_cast<const char*>(indices->data) + indices->size;
        for (uint16_t* index = static_cast<uint16_t*>(indices->data); reinterpret_cast<const char*>(index) < end; ++index)
            priv->pendingDMABufFeedback->pendingTranche.formats.append(*index);
    },
    // tranche_flags
    [](void* data, struct zwp_linux_dmabuf_feedback_v1*, uint32_t flags)
    {
        auto* priv = WPE_VIEW_WAYLAND(data)->priv;
        priv->pendingDMABufFeedback->pendingTranche.flags |= flags;
    }
};

static void wpeViewWaylandConstructed(GObject* object)
{
    G_OBJECT_CLASS(wpe_view_wayland_parent_class)->constructed(object);

    auto* display = WPE_DISPLAY_WAYLAND(wpe_view_get_display(WPE_VIEW(object)));
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
}

static void wpeViewWaylandDispose(GObject* object)
{
    auto* priv = WPE_VIEW_WAYLAND(object)->priv;
    g_clear_pointer(&priv->xdgToplevel, xdg_toplevel_destroy);
    g_clear_pointer(&priv->dmabufFeedback, zwp_linux_dmabuf_feedback_v1_destroy);
    g_clear_pointer(&priv->xdgSurface, xdg_surface_destroy);
    g_clear_pointer(&priv->wlSurface, wl_surface_destroy);
    g_clear_pointer(&priv->frameCallback, wl_callback_destroy);

    G_OBJECT_CLASS(wpe_view_wayland_parent_class)->dispose(object);
}

static struct wl_buffer* createWaylandBufferFromEGLImage(WPEBuffer* buffer, GError** error)
{
    GUniqueOutPtr<GError> bufferError;
    auto* eglDisplay = wpe_display_get_egl_display(wpe_buffer_get_display(buffer), &bufferError.outPtr());
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

static struct wl_buffer* createWaylandBufferFromDMABuf(WPEBuffer* buffer, GError** error)
{
    if (auto* wlBuffer = static_cast<struct wl_buffer*>(g_object_get_data(G_OBJECT(buffer), "wpe-wayland-buffer")))
        return wlBuffer;

    struct wl_buffer* wlBuffer = nullptr;
    if (auto* dmabuf = wpeDisplayWaylandGetLinuxDMABuf(WPE_DISPLAY_WAYLAND(wpe_buffer_get_display(buffer)))) {
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
        wlBuffer = createWaylandBufferFromEGLImage(buffer, error);
        if (!wlBuffer)
            return nullptr;
    }

    g_object_set_data_full(G_OBJECT(buffer), "wpe-wayland-buffer", wlBuffer, reinterpret_cast<GDestroyNotify>(wl_buffer_destroy));
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

static SharedMemoryBuffer* sharedMemoryBufferCreate(WPEDisplayWayland* display, GBytes* bytes, int width, int height)
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
    sharedMemoryBuffer->wlBuffer = sharedMemoryBuffer->wlPool->createBuffer(offset, width, height, size / height);
    return sharedMemoryBuffer;
}

static struct wl_buffer* createWaylandBufferSHM(WPEBuffer* buffer, GError** error)
{
    if (auto* sharedMemoryBuffer = static_cast<SharedMemoryBuffer*>(g_object_get_data(G_OBJECT(buffer), "wpe-wayland-buffer"))) {
        GBytes* bytes = wpe_buffer_shm_get_data(WPE_BUFFER_SHM(buffer));
        memcpy(reinterpret_cast<char*>(sharedMemoryBuffer->wlPool->data()), g_bytes_get_data(bytes, nullptr), sharedMemoryBuffer->wlPool->size());
        return sharedMemoryBuffer->wlBuffer;
    }

    auto* bufferSHM = WPE_BUFFER_SHM(buffer);
    if (wpe_buffer_shm_get_format(bufferSHM) != WPE_PIXEL_FORMAT_ARGB8888) {
        g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render buffer: can't create Wayland buffer because format is unsupported");
        return nullptr;
    }

    auto* display = WPE_DISPLAY_WAYLAND(wpe_buffer_get_display(buffer));
    auto* sharedMemoryBuffer = sharedMemoryBufferCreate(display, wpe_buffer_shm_get_data(bufferSHM), wpe_buffer_get_width(buffer), wpe_buffer_get_height(buffer));
    if (!sharedMemoryBuffer) {
        g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render buffer: can't create Wayland buffer because failed to create shared memory");
        return nullptr;
    }

    g_object_set_data_full(G_OBJECT(buffer), "wpe-wayland-buffer", sharedMemoryBuffer, reinterpret_cast<GDestroyNotify>(sharedMemoryBufferDestroy));

    return sharedMemoryBuffer->wlBuffer;
}

static struct wl_buffer* createWaylandBuffer(WPEBuffer* buffer, GError** error)
{
    struct wl_buffer* wlBuffer = nullptr;
    if (WPE_IS_BUFFER_DMA_BUF(buffer))
        wlBuffer = createWaylandBufferFromDMABuf(buffer, error);
    else if (WPE_IS_BUFFER_SHM(buffer))
        wlBuffer = createWaylandBufferSHM(buffer, error);
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

static gboolean wpeViewWaylandRenderBuffer(WPEView* view, WPEBuffer* buffer, GError** error)
{
    auto* wlBuffer = createWaylandBuffer(buffer, error);
    if (!wlBuffer)
        return FALSE;

    auto* priv = WPE_VIEW_WAYLAND(view)->priv;
    priv->buffer = buffer;

    auto* wlSurface = wpe_view_wayland_get_wl_surface(WPE_VIEW_WAYLAND(view));
    wl_surface_attach(wlSurface, wlBuffer, 0, 0);
    wl_surface_damage(wlSurface, 0, 0, wpe_view_get_width(view), wpe_view_get_height(view));
    priv->frameCallback = wl_surface_frame(wlSurface);
    wl_callback_add_listener(priv->frameCallback, &frameListener, view);
    wl_surface_commit(wlSurface);

    return TRUE;
}

static gboolean wpeViewWaylandSetFullscreen(WPEView* view, gboolean fullscreen)
{
    auto* priv = WPE_VIEW_WAYLAND(view)->priv;
    if (!priv->xdgToplevel)
        return FALSE;

    if (fullscreen)
        xdg_toplevel_set_fullscreen(priv->xdgToplevel, nullptr);
    else
        xdg_toplevel_unset_fullscreen(priv->xdgToplevel);

    return TRUE;
}

static GList* wpeViewWaylandGetPreferredDMABufFormats(WPEView* view)
{
    auto* priv = WPE_VIEW_WAYLAND(view)->priv;
    if (!priv->committedDMABufFeedback)
        return nullptr;

    GList* preferredFormats = nullptr;
    for (const auto& tranche : priv->committedDMABufFeedback->tranches) {
        uint32_t previousFormat = 0;
        GRefPtr<GArray> modifiers = adoptGRef(g_array_new(FALSE, TRUE, sizeof(guint64)));
        WPEBufferDMABufFormatUsage usage = tranche.flags & ZWP_LINUX_DMABUF_FEEDBACK_V1_TRANCHE_FLAGS_SCANOUT ? WPE_BUFFER_DMA_BUF_FORMAT_USAGE_SCANOUT : WPE_BUFFER_DMA_BUF_FORMAT_USAGE_RENDERING;
        auto formatsLength = tranche.formats.size();
        for (size_t i = 0; i < formatsLength; ++i) {
            auto [format, modifier] = priv->committedDMABufFeedback->format(tranche.formats[i]);
            g_array_append_val(modifiers.get(), modifier);
            if (i && format != previousFormat) {
                preferredFormats = g_list_prepend(preferredFormats, wpe_buffer_dma_buf_format_new(usage, previousFormat, modifiers.get()));
                modifiers = adoptGRef(g_array_new(FALSE, TRUE, sizeof(guint64)));
            }
            previousFormat = format;
        }
        if (previousFormat)
            preferredFormats = g_list_prepend(preferredFormats, wpe_buffer_dma_buf_format_new(usage, previousFormat, modifiers.get()));
    }
    return g_list_reverse(preferredFormats);
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

static void wpeViewWaylandSetCursorFromBytes(WPEView* view, GBytes* bytes, guint width, guint height, guint hotspotX, guint hotspotY)
{
    auto* display = WPE_DISPLAY_WAYLAND(wpe_view_get_display(view));
    auto* cursor = wpeDisplayWaylandGetCursor(display);
    if (!cursor)
        return;

    auto* sharedMemoryBuffer = sharedMemoryBufferCreate(display, bytes, width, height);
    if (!sharedMemoryBuffer)
        return;

    wl_buffer_add_listener(sharedMemoryBuffer->wlBuffer, &cursorBufferListener, sharedMemoryBuffer);
    cursor->setFromBuffer(sharedMemoryBuffer->wlBuffer, width, height, hotspotX, hotspotY);
}

static void wpe_view_wayland_class_init(WPEViewWaylandClass* viewWaylandClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(viewWaylandClass);
    objectClass->constructed = wpeViewWaylandConstructed;
    objectClass->dispose = wpeViewWaylandDispose;

    WPEViewClass* viewClass = WPE_VIEW_CLASS(viewWaylandClass);
    viewClass->render_buffer = wpeViewWaylandRenderBuffer;
    viewClass->set_fullscreen = wpeViewWaylandSetFullscreen;
    viewClass->get_preferred_dma_buf_formats = wpeViewWaylandGetPreferredDMABufFormats;
    viewClass->set_cursor_from_name = wpeViewWaylandSetCursorFromName;
    viewClass->set_cursor_from_bytes = wpeViewWaylandSetCursorFromBytes;
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

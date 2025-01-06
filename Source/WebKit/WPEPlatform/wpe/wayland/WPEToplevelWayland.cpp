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
#include "WPEToplevelWayland.h"

#include "WPEBufferDMABufFormats.h"
#include "WPEDisplayWaylandPrivate.h"
#include "WPEToplevelWaylandPrivate.h"
#include "linux-dmabuf-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <wtf/Vector.h>
#include <wtf/glib/Application.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GWeakPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>
#include <wtf/unix/UnixFileDescriptor.h>

#if USE(LIBDRM)
#include <xf86drm.h>
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
 * WPEToplevelWayland:
 *
 */
struct _WPEToplevelWaylandPrivate {
    struct wl_surface* wlSurface;
    struct xdg_surface* xdgSurface;
    struct xdg_toplevel* xdgToplevel;

    struct zwp_linux_dmabuf_feedback_v1* dmabufFeedback;
    struct zwp_linux_surface_synchronization_v1* surfaceSync;
    std::unique_ptr<DMABufFeedback> pendingDMABufFeedback;
    std::unique_ptr<DMABufFeedback> committedDMABufFeedback;
    GRefPtr<WPEBufferDMABufFormats> preferredDMABufFormats;

    bool hasPointer;
    bool isFocused;
    bool isUnderTouch;
    GWeakPtr<WPEView> visibleView;

    Vector<GRefPtr<WPEScreen>, 1> screens;
    GRefPtr<WPEScreen> currentScreen;

    struct {
        std::optional<uint32_t> width;
        std::optional<uint32_t> height;
        WPEToplevelState state { WPE_TOPLEVEL_STATE_NONE };
    } pendingState;

    struct {
        std::optional<uint32_t> width;
        std::optional<uint32_t> height;
    } savedSize;

    struct {
        Vector<WPERectangle, 1> rects;
        bool dirty;
    } opaqueRegion;
};
WEBKIT_DEFINE_FINAL_TYPE(WPEToplevelWayland, wpe_toplevel_wayland, WPE_TYPE_TOPLEVEL, WPEToplevel)

static void wpeToplevelWaylandSaveSize(WPEToplevel* toplevel)
{
    auto state = wpe_toplevel_get_state(toplevel);
    if (state & (WPE_TOPLEVEL_STATE_FULLSCREEN | WPE_TOPLEVEL_STATE_MAXIMIZED))
        return;

    auto* priv = WPE_TOPLEVEL_WAYLAND(toplevel)->priv;
    int width, height;
    wpe_toplevel_get_size(toplevel, &width, &height);
    priv->savedSize.width = width;
    priv->savedSize.height = height;
}

static void wpeToplevelWaylandResized(WPEToplevel* toplevel, int width, int height)
{
    wpe_toplevel_resized(toplevel, width, height);
    wpe_toplevel_foreach_view(toplevel, [](WPEToplevel* toplevel, WPEView* view, gpointer) -> gboolean {
        int width, height;
        wpe_toplevel_get_size(toplevel, &width, &height);
        wpe_view_resized(view, width, height);
        return FALSE;
    }, nullptr);
}

const struct xdg_surface_listener xdgSurfaceListener = {
    // configure
    [](void* data, struct xdg_surface* surface, uint32_t serial)
    {
        auto* toplevel = WPE_TOPLEVEL(data);
        auto* priv = WPE_TOPLEVEL_WAYLAND(toplevel)->priv;

        bool isFixedSize = priv->pendingState.state & (WPE_TOPLEVEL_STATE_FULLSCREEN | WPE_TOPLEVEL_STATE_MAXIMIZED);
        bool wasFixedSize = wpe_toplevel_get_state(toplevel) & (WPE_TOPLEVEL_STATE_FULLSCREEN | WPE_TOPLEVEL_STATE_MAXIMIZED);
        auto width = priv->pendingState.width;
        auto height = priv->pendingState.height;
        bool useSavedSize = !width.has_value() && !height.has_value();
        if (useSavedSize && !isFixedSize && wasFixedSize) {
            width = priv->savedSize.width;
            height = priv->savedSize.height;
        }

        if (width.has_value() && height.has_value()) {
            if (!useSavedSize)
                wpeToplevelWaylandSaveSize(toplevel);
            wpeToplevelWaylandResized(toplevel, width.value(), height.value());
        }

        wpe_toplevel_state_changed(toplevel, priv->pendingState.state);
        priv->pendingState = { };
        xdg_surface_ack_configure(surface, serial);
    },
};

const struct xdg_toplevel_listener xdgToplevelListener = {
    // configure
    [](void* data, struct xdg_toplevel*, int32_t width, int32_t height, struct wl_array* states)
    {
        auto* toplevel = WPE_TOPLEVEL_WAYLAND(data);
        auto* priv = toplevel->priv;
        if (width && height) {
            priv->pendingState.width = width;
            priv->pendingState.height = height;
        }

        uint32_t pendingState = 0;
        const auto* stateData = static_cast<uint32_t*>(states->data);
        for (size_t i = 0; i < states->size; i++) {
            uint32_t state = stateData[i];

            switch (state) {
            case XDG_TOPLEVEL_STATE_FULLSCREEN:
                pendingState |= WPE_TOPLEVEL_STATE_FULLSCREEN;
                break;
            case XDG_TOPLEVEL_STATE_MAXIMIZED:
                pendingState |= WPE_TOPLEVEL_STATE_MAXIMIZED;
                break;
            case XDG_TOPLEVEL_STATE_ACTIVATED:
                pendingState |= WPE_TOPLEVEL_STATE_ACTIVE;
                break;
            default:
                break;
            }
        }

        priv->pendingState.state = static_cast<WPEToplevelState>(priv->pendingState.state | pendingState);
    },
    // close
    [](void* data, struct xdg_toplevel*)
    {
        wpe_toplevel_closed(WPE_TOPLEVEL(data));
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

static void wpeToplevelWaylandUpdateScale(WPEToplevelWayland* toplevel)
{
    auto* priv = toplevel->priv;
    if (priv->screens.isEmpty())
        return;

    double scale = 1;
    for (const auto& screen : priv->screens)
        scale = std::max(scale, wpe_screen_get_scale(screen.get()));

    if (wl_surface_get_version(priv->wlSurface) >= WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION)
        wl_surface_set_buffer_scale(priv->wlSurface, scale);

    wpe_toplevel_scale_changed(WPE_TOPLEVEL(toplevel), scale);
}

static const struct wl_surface_listener surfaceListener = {
    // enter
    [](void* data, struct wl_surface*, struct wl_output* wlOutput)
    {
        auto* toplevel = WPE_TOPLEVEL(data);
        auto* display = wpe_toplevel_get_display(toplevel);
        if (!display)
            return;

        auto* screen = wpeDisplayWaylandFindScreen(WPE_DISPLAY_WAYLAND(display), wlOutput);
        if (!screen)
            return;

        auto* priv = WPE_TOPLEVEL_WAYLAND(toplevel)->priv;
        // For now we just use the last entered screen as current, but we could do someting smarter.
        bool screenChanged = false;
        if (priv->currentScreen.get() != screen) {
            priv->currentScreen = screen;
            screenChanged = true;
        }
        priv->screens.append(screen);
        wpeToplevelWaylandUpdateScale(WPE_TOPLEVEL_WAYLAND(toplevel));
        if (screenChanged)
            wpe_toplevel_screen_changed(toplevel);
        g_signal_connect_object(screen, "notify::scale", G_CALLBACK(+[](WPEToplevelWayland* toplevel) {
            wpeToplevelWaylandUpdateScale(WPE_TOPLEVEL_WAYLAND(toplevel));
        }), toplevel, G_CONNECT_SWAPPED);
    },
    // leave
    [](void* data, struct wl_surface*, struct wl_output* wlOutput)
    {
        auto* toplevel = WPE_TOPLEVEL(data);
        auto* display = wpe_toplevel_get_display(toplevel);
        if (!display)
            return;

        auto* screen = wpeDisplayWaylandFindScreen(WPE_DISPLAY_WAYLAND(display), wlOutput);
        if (!screen)
            return;

        auto* priv = WPE_TOPLEVEL_WAYLAND(toplevel)->priv;
        priv->screens.removeLast(screen);
        if (!priv->screens.isEmpty())
            priv->currentScreen = priv->screens.last();
        else
            priv->currentScreen = nullptr;
        wpeToplevelWaylandUpdateScale(WPE_TOPLEVEL_WAYLAND(toplevel));
        wpe_toplevel_screen_changed(toplevel);
        g_signal_handlers_disconnect_by_data(screen, toplevel);
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
        auto* toplevel = WPE_TOPLEVEL(data);
        auto* priv = WPE_TOPLEVEL_WAYLAND(toplevel)->priv;
        if (!priv->pendingDMABufFeedback)
            return;

        // The compositor might not have sent the formats table. In that case, try to reuse the previous
        // one. Return early and skip emitting the signal if there is no usable formats table in the end.
        if (!priv->pendingDMABufFeedback->formatTable) {
            if (priv->committedDMABufFeedback && priv->committedDMABufFeedback->formatTable)
                priv->pendingDMABufFeedback->formatTable = WTFMove(priv->committedDMABufFeedback->formatTable);
            else {
                priv->pendingDMABufFeedback.reset();
                return;
            }
        }

        priv->committedDMABufFeedback = WTFMove(priv->pendingDMABufFeedback);
        priv->preferredDMABufFormats = nullptr;
        wpe_toplevel_preferred_dma_buf_formats_changed(toplevel);
    },
    // format_table
    [](void* data, struct zwp_linux_dmabuf_feedback_v1*, int32_t fd, uint32_t size)
    {
        // The protocol specification is not clear about the ordering of the format_table and main_device
        // events. Err on the safer side and allow any of them to create the pending feedback instance.
        auto* priv = WPE_TOPLEVEL_WAYLAND(data)->priv;
        if (!priv->pendingDMABufFeedback)
            priv->pendingDMABufFeedback = makeUnique<DMABufFeedback>();

        priv->pendingDMABufFeedback->formatTable = DMABufFeedback::FormatTable(size, fd);
        close(fd);
    },
    // main_device
    [](void* data, struct zwp_linux_dmabuf_feedback_v1*, struct wl_array* device)
    {
        auto* priv = WPE_TOPLEVEL_WAYLAND(data)->priv;
        if (!priv->pendingDMABufFeedback)
            priv->pendingDMABufFeedback = makeUnique<DMABufFeedback>();

#if USE(LIBDRM)
        memcpy(&priv->pendingDMABufFeedback->mainDevice, device->data, sizeof(dev_t));
#endif
    },
    // tranche_done
    [](void* data, struct zwp_linux_dmabuf_feedback_v1*)
    {
        auto* priv = WPE_TOPLEVEL_WAYLAND(data)->priv;
        if (!priv->pendingDMABufFeedback)
            return;

        priv->pendingDMABufFeedback->tranches.append(WTFMove(priv->pendingDMABufFeedback->pendingTranche));
    },
    // tranche_target_device
    [](void* data, struct zwp_linux_dmabuf_feedback_v1*, struct wl_array* device)
    {
#if USE(LIBDRM)
        auto* priv = WPE_TOPLEVEL_WAYLAND(data)->priv;
        if (!priv->pendingDMABufFeedback)
            return;

        memcpy(&priv->pendingDMABufFeedback->pendingTranche.targetDevice, device->data, sizeof(dev_t));
#endif
    },
    // tranche_formats
    [](void* data, struct zwp_linux_dmabuf_feedback_v1*, struct wl_array* indices)
    {
        auto* priv = WPE_TOPLEVEL_WAYLAND(data)->priv;
        if (!priv->pendingDMABufFeedback)
            return;

        const char* end = static_cast<const char*>(indices->data) + indices->size;
        for (uint16_t* index = static_cast<uint16_t*>(indices->data); reinterpret_cast<const char*>(index) < end; ++index)
            priv->pendingDMABufFeedback->pendingTranche.formats.append(*index);
    },
    // tranche_flags
    [](void* data, struct zwp_linux_dmabuf_feedback_v1*, uint32_t flags)
    {
        auto* priv = WPE_TOPLEVEL_WAYLAND(data)->priv;
        if (priv->pendingDMABufFeedback)
            priv->pendingDMABufFeedback->pendingTranche.flags |= flags;
    }
};

static const char* defaultTitle()
{
    if (const char* title = g_get_application_name())
        return title;

    if (const char* title = g_get_prgname())
        return title;

    return nullptr;
}

static void wpeToplevelWaylandConstructed(GObject *object)
{
    G_OBJECT_CLASS(wpe_toplevel_wayland_parent_class)->constructed(object);

    auto* toplevel = WPE_TOPLEVEL(object);
    auto* display = WPE_DISPLAY_WAYLAND(wpe_toplevel_get_display(toplevel));
    auto* priv = WPE_TOPLEVEL_WAYLAND(object)->priv;
    auto* wlCompositor = wpe_display_wayland_get_wl_compositor(display);
    priv->wlSurface = wl_compositor_create_surface(wlCompositor);
    wl_surface_add_listener(priv->wlSurface, &surfaceListener, object);
    if (auto* xdgWMBase = wpeDisplayWaylandGetXDGWMBase(display)) {
        priv->xdgSurface = xdg_wm_base_get_xdg_surface(xdgWMBase, priv->wlSurface);
        xdg_surface_add_listener(priv->xdgSurface, &xdgSurfaceListener, object);
        if (auto* xdgToplevel = xdg_surface_get_toplevel(priv->xdgSurface)) {
            priv->xdgToplevel = xdgToplevel;
            xdg_toplevel_add_listener(priv->xdgToplevel, &xdgToplevelListener, object);
            const char* title = defaultTitle();
            xdg_toplevel_set_title(priv->xdgToplevel, title ? title : "");
            xdg_toplevel_set_app_id(priv->xdgToplevel, WTF::applicationID().data());
            wl_surface_commit(priv->wlSurface);
        }
    }

    auto* dmabuf = wpeDisplayWaylandGetLinuxDMABuf(display);
    if (dmabuf && zwp_linux_dmabuf_v1_get_version(dmabuf) >= ZWP_LINUX_DMABUF_V1_GET_SURFACE_FEEDBACK_SINCE_VERSION) {
        priv->dmabufFeedback = zwp_linux_dmabuf_v1_get_surface_feedback(dmabuf, priv->wlSurface);
        zwp_linux_dmabuf_feedback_v1_add_listener(priv->dmabufFeedback, &linuxDMABufFeedbackListener, object);
    }

    wl_display_roundtrip(wpe_display_wayland_get_wl_display(display));

    // Set the first screen as the default one until enter screen is emitted.
    if (wpe_display_get_n_screens(WPE_DISPLAY(display))) {
        priv->currentScreen = wpe_display_get_screen(WPE_DISPLAY(display), 0);
        auto scale = wpe_screen_get_scale(priv->currentScreen.get());
        if (wl_surface_get_version(priv->wlSurface) >= WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION)
            wl_surface_set_buffer_scale(priv->wlSurface, scale);
        wpe_toplevel_scale_changed(toplevel, scale);
    }

    if (auto* explicitSync = wpeDisplayWaylandGetLinuxExplicitSync(display))
        priv->surfaceSync = zwp_linux_explicit_synchronization_v1_get_synchronization(explicitSync, priv->wlSurface);
}

static void wpeToplevelWaylandDispose(GObject* object)
{
    auto* priv = WPE_TOPLEVEL_WAYLAND(object)->priv;
    priv->currentScreen = nullptr;
    priv->screens.clear();
    g_clear_pointer(&priv->xdgToplevel, xdg_toplevel_destroy);
    g_clear_pointer(&priv->dmabufFeedback, zwp_linux_dmabuf_feedback_v1_destroy);
    g_clear_pointer(&priv->surfaceSync, zwp_linux_surface_synchronization_v1_destroy);
    g_clear_pointer(&priv->xdgSurface, xdg_surface_destroy);
    g_clear_pointer(&priv->wlSurface, wl_surface_destroy);

    G_OBJECT_CLASS(wpe_toplevel_wayland_parent_class)->dispose(object);
}

static WPEScreen* wpeToplevelWaylandGetScreen(WPEToplevel* toplevel)
{
    auto* priv = WPE_TOPLEVEL_WAYLAND(toplevel)->priv;
    return priv->currentScreen.get();
}

static gboolean wpeToplevelWaylandResize(WPEToplevel* toplevel, int width, int height)
{
    auto* priv = WPE_TOPLEVEL_WAYLAND(toplevel)->priv;
    if (!priv->xdgToplevel)
        return FALSE;

    wpeToplevelWaylandResized(toplevel, width, height);
    return TRUE;
}

static gboolean wpeToplevelWaylandSetFullscreen(WPEToplevel* toplevel, gboolean fullscreen)
{
    auto* priv = WPE_TOPLEVEL_WAYLAND(toplevel)->priv;
    if (!priv->xdgToplevel)
        return FALSE;

    if (fullscreen) {
        wpeToplevelWaylandSaveSize(toplevel);
        xdg_toplevel_set_fullscreen(priv->xdgToplevel, nullptr);
        return TRUE;
    }

    xdg_toplevel_unset_fullscreen(priv->xdgToplevel);
    return TRUE;
}

static gboolean wpeToplevelWaylandSetMaximized(WPEToplevel* toplevel, gboolean maximized)
{
    auto* priv = WPE_TOPLEVEL_WAYLAND(toplevel)->priv;
    if (!priv->xdgToplevel)
        return FALSE;

    if (maximized) {
        wpeToplevelWaylandSaveSize(toplevel);
        xdg_toplevel_set_maximized(priv->xdgToplevel);
        return TRUE;
    }

    xdg_toplevel_unset_maximized(priv->xdgToplevel);
    return TRUE;
}

static gboolean wpeToplevelWaylandSetMinimized(WPEToplevel* toplevel)
{
    auto* priv = WPE_TOPLEVEL_WAYLAND(toplevel)->priv;
    if (!priv->xdgToplevel)
        return FALSE;

    xdg_toplevel_set_minimized(priv->xdgToplevel);
    return TRUE;
}

static WPEBufferDMABufFormats* wpeToplevelWaylandGetPreferredDMABufFormats(WPEToplevel* toplevel)
{
    auto* priv = WPE_TOPLEVEL_WAYLAND(toplevel)->priv;
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

static void wpeToplevelWaylandSetTitle(WPEToplevel* toplevel, const char* title)
{
    auto* priv = WPE_TOPLEVEL_WAYLAND(toplevel)->priv;
    if (!priv->xdgToplevel)
        return;

    if (!title || !*title) {
        xdg_toplevel_set_title(priv->xdgToplevel, "");
        return;
    }

    static constexpr size_t wlMaxTitleSize = 4083; // 4096 minus header, string argument length and NUL byte.
    auto titleLength = strlen(title);
    auto minTitleLength = std::min<size_t>(titleLength, wlMaxTitleSize);
    const char* end = nullptr;
    GUniquePtr<char> validTitle;
    if (g_utf8_validate(title, minTitleLength, &end)) {
        if (end == title + titleLength) {
            xdg_toplevel_set_title(priv->xdgToplevel, title);
            return;
        }

        validTitle.reset(g_strndup(title, end - title));
    } else
        validTitle.reset(g_utf8_make_valid(title, minTitleLength));

    xdg_toplevel_set_title(priv->xdgToplevel, validTitle.get());
}

static void wpe_toplevel_wayland_class_init(WPEToplevelWaylandClass* toplevelWaylandClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(toplevelWaylandClass);
    objectClass->constructed = wpeToplevelWaylandConstructed;
    objectClass->dispose = wpeToplevelWaylandDispose;

    WPEToplevelClass* toplevelClass = WPE_TOPLEVEL_CLASS(toplevelWaylandClass);
    toplevelClass->get_screen = wpeToplevelWaylandGetScreen;
    toplevelClass->resize = wpeToplevelWaylandResize;
    toplevelClass->set_fullscreen = wpeToplevelWaylandSetFullscreen;
    toplevelClass->set_maximized = wpeToplevelWaylandSetMaximized;
    toplevelClass->set_minimized = wpeToplevelWaylandSetMinimized;
    toplevelClass->get_preferred_dma_buf_formats = wpeToplevelWaylandGetPreferredDMABufFormats;
    toplevelClass->set_title = wpeToplevelWaylandSetTitle;
}

static bool regionsEqual(WPERectangle* rectsA, unsigned rectsACount, WPERectangle* rectsB, unsigned rectsBCount)
{
    if (rectsACount != rectsBCount)
        return false;

    if (rectsA == rectsB)
        return true;

    if (!rectsA || !rectsB)
        return false;

    auto rectsEqual = [](const WPERectangle& rectA, const WPERectangle& rectB) -> bool {
        return rectA.x == rectB.x
            && rectA.y == rectB.y
            && rectA.width == rectB.width
            && rectA.height == rectB.height;
    };

    for (unsigned i = 0; i < rectsACount; ++i) {
        if (!rectsEqual(rectsA[i], rectsB[i]))
            return false;
    }

    return true;
}

void wpeToplevelWaylandSetOpaqueRectangles(WPEToplevelWayland* toplevel, WPERectangle* rects, unsigned rectsCount)
{
    auto* priv = toplevel->priv;
    if (regionsEqual(priv->opaqueRegion.rects.data(), priv->opaqueRegion.rects.size(), rects, rectsCount))
        return;

    priv->opaqueRegion.rects.clear();
    if (rects) {
        priv->opaqueRegion.rects.reserveInitialCapacity(rectsCount);
        for (unsigned i = 0; i < rectsCount; ++i)
            priv->opaqueRegion.rects.append(rects[i]);
    }
    priv->opaqueRegion.dirty = true;
}

void wpeToplevelWaylandUpdateOpaqueRegion(WPEToplevelWayland* toplevel)
{
    auto* priv = toplevel->priv;
    if (!priv->opaqueRegion.dirty)
        return;

    struct wl_region* region = nullptr;
    if (!priv->opaqueRegion.rects.isEmpty()) {
        auto* display = WPE_DISPLAY_WAYLAND(wpe_toplevel_get_display(WPE_TOPLEVEL(toplevel)));
        auto* wlCompositor = wpe_display_wayland_get_wl_compositor(display);
        region = wl_compositor_create_region(wlCompositor);
        if (region) {
            for (const auto& rect : priv->opaqueRegion.rects)
                wl_region_add(region, rect.x, rect.y, rect.width, rect.height);
        }
    }

    wl_surface_set_opaque_region(priv->wlSurface, region);
    if (region)
        wl_region_destroy(region);

    priv->opaqueRegion.dirty = false;
}

static WPEView* wpeToplevelWaylandFindVisibleView(WPEToplevelWayland* toplevel)
{
    WPEView* view = nullptr;
    wpe_toplevel_foreach_view(WPE_TOPLEVEL(toplevel), [](WPEToplevel*, WPEView* view, gpointer userData) -> gboolean {
        if (wpe_view_get_mapped(view)) {
            *static_cast<WPEView**>(userData) = view;
            return TRUE;
        }
        return FALSE;
    }, &view);
    return view;
}

void wpeToplevelWaylandSetHasPointer(WPEToplevelWayland* toplevel, bool hasPointer)
{
    auto* priv = toplevel->priv;
    priv->hasPointer = hasPointer;
    if (hasPointer && !priv->visibleView)
        priv->visibleView.reset(wpeToplevelWaylandFindVisibleView(toplevel));
}

WPEView* wpeToplevelWaylandGetVisibleViewUnderPointer(WPEToplevelWayland* toplevel)
{
    auto* priv = toplevel->priv;
    return priv->hasPointer ? toplevel->priv->visibleView.get() : nullptr;
}

void wpeToplevelWaylandSetIsFocused(WPEToplevelWayland* toplevel, bool isFocused)
{
    auto* priv = toplevel->priv;
    priv->isFocused = isFocused;
    if (isFocused && !priv->visibleView)
        priv->visibleView.reset(wpeToplevelWaylandFindVisibleView(toplevel));
}

WPEView* wpeToplevelWaylandGetVisibleFocusedView(WPEToplevelWayland* toplevel)
{
    auto* priv = toplevel->priv;
    return priv->isFocused ? toplevel->priv->visibleView.get() : nullptr;
}

void wpeToplevelWaylandSetIsUnderTouch(WPEToplevelWayland* toplevel, bool isUnderTouch)
{
    auto* priv = toplevel->priv;
    priv->isUnderTouch = isUnderTouch;
    if (isUnderTouch && !priv->visibleView)
        priv->visibleView.reset(wpeToplevelWaylandFindVisibleView(toplevel));
}

WPEView* wpeToplevelWaylandGetVisibleViewUnderTouch(WPEToplevelWayland* toplevel)
{
    auto* priv = toplevel->priv;
    return priv->isUnderTouch ? toplevel->priv->visibleView.get() : nullptr;
}

void wpeToplevelWaylandViewVisibilityChanged(WPEToplevelWayland* toplevel, WPEView* view)
{
    auto* priv = toplevel->priv;
    auto* visibleView = wpeToplevelWaylandFindVisibleView(toplevel);
    if (wpe_view_get_visible(view)) {
        priv->visibleView.reset(visibleView);
        if (priv->visibleView.get() != view)
            return;

        if (priv->hasPointer) {
            if (auto* seat = wpeDisplayWaylandGetSeat(WPE_DISPLAY_WAYLAND(wpe_toplevel_get_display(WPE_TOPLEVEL(toplevel)))))
                seat->emitPointerEnter(view);
        }
        if (priv->isFocused)
            wpe_view_focus_in(view);
    } else {
        if (priv->visibleView && priv->visibleView.get() == view) {
            if (priv->hasPointer) {
                if (auto* seat = wpeDisplayWaylandGetSeat(WPE_DISPLAY_WAYLAND(wpe_toplevel_get_display(WPE_TOPLEVEL(toplevel)))))
                    seat->emitPointerLeave(view);
            }
            if (priv->isFocused)
                wpe_view_focus_out(view);
        }
        priv->visibleView.reset(visibleView);
    }
}

struct zwp_linux_surface_synchronization_v1* wpeToplevelWaylandGetSurfaceSync(WPEToplevelWayland* toplevel)
{
    return toplevel->priv->surfaceSync;
}

/**
 * wpe_toplevel_wayland_new:
 * @display: a #WPEDisplayWayland
 *
 * Create a new #WPEToplevel on @display.
 *
 * Returns: (transfer full): a #WPEToplevel
 */
WPEToplevel* wpe_toplevel_wayland_new(WPEDisplayWayland* display)
{
    return WPE_TOPLEVEL(g_object_new(WPE_TYPE_TOPLEVEL_WAYLAND, "display", display, nullptr));
}

/**
 * wpe_toplevel_wayland_get_wl_surface: (skip)
 * @toplevel: a #WPEToplevelWayland
 *
 * Get the native Wayland surface of @toplevel
 *
 * Returns: (transfer none) (nullable): a Wayland `wl_surface`
 */
struct wl_surface* wpe_toplevel_wayland_get_wl_surface(WPEToplevelWayland* toplevel)
{
    g_return_val_if_fail(WPE_IS_TOPLEVEL_WAYLAND(toplevel), nullptr);

    return toplevel->priv->wlSurface;
}

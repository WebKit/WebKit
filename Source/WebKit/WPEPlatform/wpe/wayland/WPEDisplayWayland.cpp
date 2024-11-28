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
#include "WPEInputMethodContextWaylandV1.h"
#include "WPEInputMethodContextWaylandV3.h"
#include "WPEScreenWaylandPrivate.h"
#include "WPEToplevelWayland.h"
#include "WPEViewWayland.h"
#include "WPEWaylandCursor.h"
#include "WPEWaylandSeat.h"
#include "linux-dmabuf-unstable-v1-client-protocol.h"
#include "linux-explicit-synchronization-unstable-v1-client-protocol.h"
#include "pointer-constraints-unstable-v1-client-protocol.h"
#include "relative-pointer-unstable-v1-client-protocol.h"
#include "text-input-unstable-v1-client-protocol.h"
#include "text-input-unstable-v3-client-protocol.h"
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
    struct zwp_linux_explicit_synchronization_v1* linuxExplicitSync;
#if USE(LIBDRM)
    struct zwp_linux_dmabuf_feedback_v1* dmabufFeedback;
#endif
    struct zwp_text_input_manager_v1* textInputManagerV1;
    struct zwp_text_input_v1* textInputV1;
    struct zwp_text_input_manager_v3* textInputManagerV3;
    struct zwp_text_input_v3* textInputV3;
    struct zwp_pointer_constraints_v1* pointerConstraints;
    struct zwp_relative_pointer_manager_v1* relativePointerManager;
    Vector<std::pair<uint32_t, uint64_t>> linuxDMABufFormats;
    std::unique_ptr<WPE::WaylandSeat> wlSeat;
    std::unique_ptr<WPE::WaylandCursor> wlCursor;
    CString drmDevice;
    CString drmRenderNode;
    Vector<GRefPtr<WPEScreen>, 1> screens;
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
    while (!priv->screens.isEmpty()) {
        auto screen = priv->screens.takeLast();
        wpe_screen_invalidate(screen.get());
    }
    if (priv->textInputManagerV1) {
        g_clear_pointer(&priv->textInputV1, zwp_text_input_v1_destroy);
        g_clear_pointer(&priv->textInputManagerV1, zwp_text_input_manager_v1_destroy);
    }
    if (priv->textInputManagerV3) {
        g_clear_pointer(&priv->textInputV3, zwp_text_input_v3_destroy);
        g_clear_pointer(&priv->textInputManagerV3, zwp_text_input_manager_v3_destroy);
    }
    g_clear_pointer(&priv->pointerConstraints, zwp_pointer_constraints_v1_destroy);
    g_clear_pointer(&priv->relativePointerManager, zwp_relative_pointer_manager_v1_destroy);
#if USE(LIBDRM)
    g_clear_pointer(&priv->dmabufFeedback, zwp_linux_dmabuf_feedback_v1_destroy);
#endif
    g_clear_pointer(&priv->linuxDMABuf, zwp_linux_dmabuf_v1_destroy);
    g_clear_pointer(&priv->linuxExplicitSync, zwp_linux_explicit_synchronization_v1_destroy);
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
            GRefPtr<WPEScreen> screen = adoptGRef(wpeScreenWaylandCreate(name, static_cast<struct wl_output*>(wl_registry_bind(registry, name, &wl_output_interface, std::min<uint32_t>(version, 2)))));
            auto* screenPtr = screen.get();
            priv->screens.append(WTFMove(screen));
            wpe_display_screen_added(WPE_DISPLAY(display), screenPtr);
        } else if (!std::strcmp(interface, "wl_shm"))
            priv->wlSHM = static_cast<struct wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
        else if (!std::strcmp(interface, "zwp_linux_dmabuf_v1"))
            priv->linuxDMABuf = static_cast<struct zwp_linux_dmabuf_v1*>(wl_registry_bind(registry, name, &zwp_linux_dmabuf_v1_interface, std::min<uint32_t>(version, 4)));
        else if (!std::strcmp(interface, "zwp_linux_explicit_synchronization_v1"))
            priv->linuxExplicitSync = static_cast<struct zwp_linux_explicit_synchronization_v1*>(wl_registry_bind(registry, name, &zwp_linux_explicit_synchronization_v1_interface, 1));
        else if (!std::strcmp(interface, "zwp_text_input_manager_v1")) {
            priv->textInputManagerV1 = static_cast<struct zwp_text_input_manager_v1*>(wl_registry_bind(registry, name, &zwp_text_input_manager_v1_interface, 1));
            priv->textInputV1 = zwp_text_input_manager_v1_create_text_input(priv->textInputManagerV1);
        } else if (!std::strcmp(interface, "zwp_text_input_manager_v3")) {
            priv->textInputManagerV3 = static_cast<struct zwp_text_input_manager_v3*>(wl_registry_bind(registry, name, &zwp_text_input_manager_v3_interface, 1));
        } else if (!std::strcmp(interface, "zwp_pointer_constraints_v1")) {
            priv->pointerConstraints = static_cast<struct zwp_pointer_constraints_v1*>(wl_registry_bind(registry, name, &zwp_pointer_constraints_v1_interface, 1));
        } else if (!std::strcmp(interface, "zwp_relative_pointer_manager_v1")) {
            priv->relativePointerManager = static_cast<struct zwp_relative_pointer_manager_v1*>(wl_registry_bind(registry, name, &zwp_relative_pointer_manager_v1_interface, 1));
        }
    },
    // global_remove
    [](void* data, struct wl_registry*, uint32_t name)
    {
        auto* display = WPE_DISPLAY_WAYLAND(data);
        auto* priv = display->priv;
        auto index = priv->screens.findIf([name](const auto& screen) {
            return wpe_screen_get_id(screen.get()) == name;
        });
        if (index != notFound) {
            auto screen = priv->screens[index];
            priv->screens.remove(index);
            wpe_display_screen_removed(WPE_DISPLAY(display), screen.get());
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

static gboolean wpeDisplayWaylandSetup(WPEDisplayWayland* display, GError** error)
{
    auto* priv = display->priv;
    priv->eventSource = wpeDisplayWaylandCreateEventSource(display);

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
        priv->wlCursor = makeUnique<WPE::WaylandCursor>(display);
        priv->wlSeat->startListening();
    }

    if (priv->textInputManagerV3) {
        // Using this interface needs a valid seat. Do not keep around the object
        // without a seat, to give a chance for a different IM interface to be used.
        if (priv->wlSeat)
            priv->textInputV3 = zwp_text_input_manager_v3_get_text_input(priv->textInputManagerV3, priv->wlSeat->seat());
        else
            g_clear_pointer(&priv->textInputManagerV3, zwp_text_input_manager_v3_destroy);
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
        wpeDisplayWaylandInitializeDRMDeviceFromEGL(display);

    return TRUE;
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

    return wpeDisplayWaylandSetup(displayWayland, error);
}

static WPEView* wpeDisplayWaylandCreateView(WPEDisplay* display)
{
    auto* displayWayland = WPE_DISPLAY_WAYLAND(display);
    auto* view = wpe_view_wayland_new(displayWayland);

    // FIXME: create the toplevel conditionally.
    GRefPtr<WPEToplevel> toplevel = adoptGRef(wpe_toplevel_wayland_new(displayWayland));
    wpe_view_set_toplevel(view, toplevel.get());

    return view;
}

static WPEInputMethodContext* wpeDisplayWaylandCreateInputMethodContext(WPEDisplay* display)
{
    auto* priv = WPE_DISPLAY_WAYLAND(display)->priv;
    if (!priv->wlDisplay || !priv->wlCompositor)
        return nullptr;

    if (priv->textInputManagerV3)
        return wpe_im_context_wayland_v3_new(WPE_DISPLAY_WAYLAND(display));
    if (priv->textInputManagerV1)
        return wpe_im_context_wayland_v1_new(WPE_DISPLAY_WAYLAND(display));

    return nullptr;
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

static WPEBufferDMABufFormats* wpeDisplayWaylandGetPreferredDMABufFormats(WPEDisplay* display)
{
    auto* priv = WPE_DISPLAY_WAYLAND(display)->priv;
    if (!priv->linuxDMABuf)
        return nullptr;

    auto* builder = wpe_buffer_dma_buf_formats_builder_new(priv->drmDevice.data());
    wpe_buffer_dma_buf_formats_builder_append_group(builder, nullptr, WPE_BUFFER_DMA_BUF_FORMAT_USAGE_RENDERING);
    for (const auto& format : priv->linuxDMABufFormats)
        wpe_buffer_dma_buf_formats_builder_append_format(builder, format.first, format.second);

    return wpe_buffer_dma_buf_formats_builder_end(builder);
}

static guint wpeDisplayWaylandGetNScreens(WPEDisplay* display)
{
    return WPE_DISPLAY_WAYLAND(display)->priv->screens.size();
}

static WPEScreen* wpeDisplayWaylandGetScreen(WPEDisplay* display, guint index)
{
    auto* priv = WPE_DISPLAY_WAYLAND(display)->priv;
    if (priv->screens.isEmpty() || index >= priv->screens.size())
        return nullptr;

    return priv->screens[index].get();
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

static gboolean wpeDisplayWaylandUseExplicitSync(WPEDisplay* display)
{
    return !!WPE_DISPLAY_WAYLAND(display)->priv->linuxExplicitSync;
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

WPEScreen* wpeDisplayWaylandFindScreen(WPEDisplayWayland* display, struct wl_output* output)
{
    for (const auto& screen : display->priv->screens) {
        if (wpe_screen_wayland_get_wl_output(WPE_SCREEN_WAYLAND(screen.get())) == output)
            return screen.get();
    }

    return nullptr;
}

struct zwp_linux_dmabuf_v1* wpeDisplayWaylandGetLinuxDMABuf(WPEDisplayWayland* display)
{
    return display->priv->linuxDMABuf;
}

struct zwp_text_input_v1* wpeDisplayWaylandGetTextInputV1(WPEDisplayWayland* display)
{
    return display->priv->textInputV1;
}

struct zwp_text_input_v3* wpeDisplayWaylandGetTextInputV3(WPEDisplayWayland* display)
{
    return display->priv->textInputV3;
}

struct zwp_pointer_constraints_v1* wpeDisplayWaylandGetPointerConstraints(WPEDisplayWayland* display)
{
    return display->priv->pointerConstraints;
}

struct zwp_relative_pointer_manager_v1* wpeDisplayWaylandGetRelativePointerManager(WPEDisplayWayland* display)
{
    return display->priv->relativePointerManager;
}

struct zwp_linux_explicit_synchronization_v1* wpeDisplayWaylandGetLinuxExplicitSync(WPEDisplayWayland* display)
{
    return display->priv->linuxExplicitSync;
}

static void wpe_display_wayland_class_init(WPEDisplayWaylandClass* displayWaylandClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(displayWaylandClass);
    objectClass->dispose = wpeDisplayWaylandDispose;

    WPEDisplayClass* displayClass = WPE_DISPLAY_CLASS(displayWaylandClass);
    displayClass->connect = wpeDisplayWaylandConnect;
    displayClass->create_view = wpeDisplayWaylandCreateView;
    displayClass->create_input_method_context = wpeDisplayWaylandCreateInputMethodContext;
    displayClass->get_egl_display = wpeDisplayWaylandGetEGLDisplay;
    displayClass->get_keymap = wpeDisplayWaylandGetKeymap;
    displayClass->get_preferred_dma_buf_formats = wpeDisplayWaylandGetPreferredDMABufFormats;
    displayClass->get_n_screens = wpeDisplayWaylandGetNScreens;
    displayClass->get_screen = wpeDisplayWaylandGetScreen;
    displayClass->get_drm_device = wpeDisplayWaylandGetDRMDevice;
    displayClass->get_drm_render_node = wpeDisplayWaylandGetDRMRenderNode;
    displayClass->use_explicit_sync = wpeDisplayWaylandUseExplicitSync;
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

    return wpeDisplayWaylandSetup(display, error);
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

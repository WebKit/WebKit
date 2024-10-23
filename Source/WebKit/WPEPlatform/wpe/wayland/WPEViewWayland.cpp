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
#include "WPEToplevelWaylandPrivate.h"
#include "WPEWaylandSHMPool.h"
#include "linux-dmabuf-unstable-v1-client-protocol.h"
#include "pointer-constraints-unstable-v1-client-protocol.h"
#include "relative-pointer-unstable-v1-client-protocol.h"
#include <gio/gio.h>
#include <wtf/FastMalloc.h>
#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>

// These includes need to be in this order because wayland-egl.h defines WL_EGL_PLATFORM
// and egl.h checks that to decide whether it's Wayland platform.
#include <wayland-client-protocol.h>
#include <wayland-egl.h>
#include <epoxy/egl.h>

#ifndef EGL_WL_create_wayland_buffer_from_image
typedef struct wl_buffer *(EGLAPIENTRYP PFNEGLCREATEWAYLANDBUFFERFROMIMAGEWL)(EGLDisplay dpy, EGLImageKHR image);
#endif

/**
 * WPEViewWayland:
 *
 */
struct _WPEViewWaylandPrivate {
    GRefPtr<WPEBuffer> buffer;
    struct wl_callback* frameCallback;

    Vector<WPERectangle, 1> opaqueRegion;
    unsigned long resizedID;

    struct zwp_relative_pointer_v1* relativePointer;
    struct zwp_locked_pointer_v1* lockedPointer;
    uint32_t savedPointerModifiers { 0 };
    std::pair<double, double> savedPointerCoords { 0, 0 };
};
WEBKIT_DEFINE_FINAL_TYPE(WPEViewWayland, wpe_view_wayland, WPE_TYPE_VIEW, WPEView)

static void wpeViewWaylandConstructed(GObject* object)
{
    G_OBJECT_CLASS(wpe_view_wayland_parent_class)->constructed(object);

    auto* view = WPE_VIEW(object);
    auto* priv = WPE_VIEW_WAYLAND(view)->priv;
    // The web view default background color is opaque white, so set the whole view region as opaque initially.
    priv->opaqueRegion.append({ 0, 0, wpe_view_get_width(view), wpe_view_get_height(view) });

    priv->resizedID = g_signal_connect(view, "resized", G_CALLBACK(+[](WPEView* view, gpointer) {
        auto* priv = WPE_VIEW_WAYLAND(view)->priv;
        priv->opaqueRegion.clear();
        priv->opaqueRegion.append({ 0, 0, wpe_view_get_width(view), wpe_view_get_height(view) });
        if (auto* toplevel = wpe_view_get_toplevel(view))
            wpeToplevelWaylandSetOpaqueRectangles(WPE_TOPLEVEL_WAYLAND(toplevel), priv->opaqueRegion.data(), priv->opaqueRegion.size());
    }), nullptr);

    g_signal_connect(view, "notify::toplevel", G_CALLBACK(+[](WPEView* view, GParamSpec*, gpointer) {
        auto* toplevel = wpe_view_get_toplevel(view);
        if (!toplevel) {
            wpe_view_unmap(view);
            return;
        }

        int width;
        int height;
        wpe_toplevel_get_size(toplevel, &width, &height);
        if (width && height)
            wpe_view_resized(view, width, height);

        auto* priv = WPE_VIEW_WAYLAND(view)->priv;
        wpeToplevelWaylandSetOpaqueRectangles(WPE_TOPLEVEL_WAYLAND(toplevel), !priv->opaqueRegion.isEmpty() ? priv->opaqueRegion.data() : nullptr, priv->opaqueRegion.size());

        wpe_view_map(view);
    }), nullptr);

    g_signal_connect(view, "notify::screen", G_CALLBACK(+[](WPEView* view, GParamSpec*, gpointer) {
        if (wpe_view_get_screen(view))
            wpe_view_map(view);
        else
            wpe_view_unmap(view);
    }), nullptr);

    g_signal_connect(view, "notify::visible", G_CALLBACK(+[](WPEView* view, GParamSpec*, gpointer) {
        auto* toplevel = wpe_view_get_toplevel(view);
        if (!toplevel)
            return;

        wpeToplevelWaylandViewVisibilityChanged(WPE_TOPLEVEL_WAYLAND(toplevel), view);
    }), nullptr);
}

static void wpeViewWaylandDispose(GObject* object)
{
    auto* priv = WPE_VIEW_WAYLAND(object)->priv;
    g_clear_pointer(&priv->frameCallback, wl_callback_destroy);
    g_clear_pointer(&priv->lockedPointer, zwp_locked_pointer_v1_destroy);
    g_clear_pointer(&priv->relativePointer, zwp_relative_pointer_v1_destroy);

    G_OBJECT_CLASS(wpe_view_wayland_parent_class)->dispose(object);
}

struct DMABufBuffer {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    explicit DMABufBuffer(wl_buffer* buffer)
        : wlBuffer(buffer)
    {
    }

    ~DMABufBuffer()
    {
        if (wlBuffer)
            wl_buffer_destroy(wlBuffer);
        if (release)
            zwp_linux_buffer_release_v1_destroy(release);
    }

    struct wl_buffer* wlBuffer { nullptr };
    struct zwp_linux_buffer_release_v1* release { nullptr };
};

static void dmaBufBufferDestroy(DMABufBuffer* buffer)
{
    delete buffer;
}

static const struct wl_buffer_listener bufferListener = {
    // release
    [](void* userData, struct wl_buffer*)
    {
        auto* buffer = WPE_BUFFER(userData);
        wpe_view_buffer_released(wpe_buffer_get_view(buffer), buffer);
    }
};

static DMABufBuffer* createWaylandBufferFromEGLImage(WPEView* view, WPEBuffer* buffer, GError** error)
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
        return new DMABufBuffer(wlBuffer);

    g_set_error(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render buffer: eglCreateWaylandBufferFromImageWL failed with error %#04x", eglGetError());
    return nullptr;
}

static struct wl_buffer* createWaylandBufferFromDMABuf(WPEView* view, WPEBuffer* buffer, GError** error)
{
    if (auto* dmaBufBuffer = static_cast<DMABufBuffer*>(wpe_buffer_get_user_data(buffer)))
        return dmaBufBuffer->wlBuffer;

    auto* bufferDMABuf = WPE_BUFFER_DMA_BUF(buffer);
    DMABufBuffer* dmaBufBuffer = nullptr;
    if (auto* dmabuf = wpeDisplayWaylandGetLinuxDMABuf(WPE_DISPLAY_WAYLAND(wpe_view_get_display(view)))) {
        auto modifier = wpe_buffer_dma_buf_get_modifier(bufferDMABuf);
        auto* params = zwp_linux_dmabuf_v1_create_params(dmabuf);
        auto planeCount = wpe_buffer_dma_buf_get_n_planes(bufferDMABuf);
        for (guint32 i = 0; i < planeCount; ++i) {
            zwp_linux_buffer_params_v1_add(params, wpe_buffer_dma_buf_get_fd(bufferDMABuf, i), i, wpe_buffer_dma_buf_get_offset(bufferDMABuf, i),
                wpe_buffer_dma_buf_get_stride(bufferDMABuf, i), modifier >> 32, modifier & 0xffffffff);
        }
        auto* wlBuffer = zwp_linux_buffer_params_v1_create_immed(params, wpe_buffer_get_width(buffer), wpe_buffer_get_height(buffer),
            wpe_buffer_dma_buf_get_format(bufferDMABuf), 0);
        zwp_linux_buffer_params_v1_destroy(params);

        if (!wlBuffer) {
            g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render buffer: failed to create Wayland buffer from DMABuf");
            return nullptr;
        }

        dmaBufBuffer = new DMABufBuffer(wlBuffer);
    } else {
        dmaBufBuffer = createWaylandBufferFromEGLImage(view, buffer, error);
        if (!dmaBufBuffer)
            return nullptr;
    }

    auto* toplevel = wpe_view_get_toplevel(WPE_VIEW(view));
    if (!wpeToplevelWaylandGetSurfaceSync(WPE_TOPLEVEL_WAYLAND(toplevel)) || wpe_buffer_dma_buf_get_rendering_fence(bufferDMABuf) == -1)
        wl_buffer_add_listener(dmaBufBuffer->wlBuffer, &bufferListener, buffer);

    wpe_buffer_set_user_data(buffer, dmaBufBuffer, reinterpret_cast<GDestroyNotify>(dmaBufBufferDestroy));
    return dmaBufBuffer->wlBuffer;
}

struct SharedMemoryBuffer {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    SharedMemoryBuffer(std::unique_ptr<WPE::WaylandSHMPool>&& pool, uint32_t offset, uint32_t width, uint32_t height, uint32_t stride)
        : wlPool(WTFMove(pool))
        , wlBuffer(wlPool->createBuffer(offset, width, height, stride))
    {
    }

    ~SharedMemoryBuffer()
    {
        if (wlBuffer)
            wl_buffer_destroy(wlBuffer);
    }

    std::unique_ptr<WPE::WaylandSHMPool> wlPool;
    struct wl_buffer* wlBuffer;
};

static void sharedMemoryBufferDestroy(SharedMemoryBuffer* buffer)
{
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

    return new SharedMemoryBuffer(WTFMove(wlPool), offset, width, height, stride);
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

static void dmaBufBufferReleased(WPEBuffer* buffer)
{
    wpe_view_buffer_released(wpe_buffer_get_view(buffer), buffer);
    if (auto* dmaBufBuffer = static_cast<DMABufBuffer*>(wpe_buffer_get_user_data(buffer)))
        g_clear_pointer(&dmaBufBuffer->release, zwp_linux_buffer_release_v1_destroy);
}

const struct zwp_linux_buffer_release_v1_listener bufferReleaseListener = {
    // fenced_release
    [](void* userData, struct zwp_linux_buffer_release_v1*, int32_t fence)
    {
        auto* buffer = WPE_BUFFER(userData);
        wpe_buffer_dma_buf_set_release_fence(WPE_BUFFER_DMA_BUF(buffer), fence);
        dmaBufBufferReleased(buffer);
    },
    // immediate_release
    [](void* userData, struct zwp_linux_buffer_release_v1*)
    {
        dmaBufBufferReleased(WPE_BUFFER(userData));
    }
};

static gboolean wpeViewWaylandRenderBuffer(WPEView* view, WPEBuffer* buffer, const WPERectangle* damageRects, guint nDamageRects, GError** error)
{
    auto* wlBuffer = createWaylandBuffer(view, buffer, error);
    if (!wlBuffer)
        return FALSE;

    auto* priv = WPE_VIEW_WAYLAND(view)->priv;
    priv->buffer = buffer;

    wpeToplevelWaylandUpdateOpaqueRegion(WPE_TOPLEVEL_WAYLAND(wpe_view_get_toplevel(view)));

    auto* wlSurface = wpe_view_wayland_get_wl_surface(WPE_VIEW_WAYLAND(view));
    wl_surface_attach(wlSurface, wlBuffer, 0, 0);

    if (WPE_IS_BUFFER_DMA_BUF(buffer)) {
        auto renderingFence = UnixFileDescriptor { wpe_buffer_dma_buf_take_rendering_fence(WPE_BUFFER_DMA_BUF(buffer)), UnixFileDescriptor::Adopt };
        if (renderingFence) {
            auto* surfaceSync = wpeToplevelWaylandGetSurfaceSync(WPE_TOPLEVEL_WAYLAND(wpe_view_get_toplevel(view)));
            zwp_linux_surface_synchronization_v1_set_acquire_fence(surfaceSync, renderingFence.value());

            auto* dmaBufBuffer = static_cast<DMABufBuffer*>(wpe_buffer_get_user_data(buffer));
            dmaBufBuffer->release = zwp_linux_surface_synchronization_v1_get_release(surfaceSync);
            zwp_linux_buffer_release_v1_add_listener(dmaBufBuffer->release, &bufferReleaseListener, buffer);
        }
    }

    auto* display = WPE_DISPLAY_WAYLAND(wpe_view_get_display(view));
    auto* wlCompositor = wpe_display_wayland_get_wl_compositor(display);
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

static const struct zwp_relative_pointer_v1_listener relativePointerListener = {
    // relative_motion
    [](void* data, struct zwp_relative_pointer_v1*, uint32_t, uint32_t, wl_fixed_t, wl_fixed_t, wl_fixed_t deltaX, wl_fixed_t deltaY) {
        auto* view = reinterpret_cast<WPEView*>(data);
        auto pointerModifiers = WPE_VIEW_WAYLAND(view)->priv->savedPointerModifiers;
        auto pointerCoords = WPE_VIEW_WAYLAND(view)->priv->savedPointerCoords;
        double x = pointerCoords.first;
        double y = pointerCoords.second;
        double dX = wl_fixed_to_double(deltaX);
        double dY = wl_fixed_to_double(deltaY);

        auto* event = wpe_event_pointer_move_new(WPE_EVENT_POINTER_MOVE, view, WPE_INPUT_SOURCE_MOUSE, 0, static_cast<WPEModifiers>(pointerModifiers), x, y, dX, dY);
        wpe_view_event(view, event);
        wpe_event_unref(event);
    }
};

static gboolean wpeViewWaylandLockPointer(WPEView* view)
{
    auto* priv = WPE_VIEW_WAYLAND(view)->priv;
    if (priv->relativePointer || priv->lockedPointer)
        return FALSE;

    auto* display = WPE_DISPLAY_WAYLAND(wpe_view_get_display(view));
    auto* pointerConstraints = wpeDisplayWaylandGetPointerConstraints(display);
    auto* relativePointerManager = wpeDisplayWaylandGetRelativePointerManager(display);
    if (!pointerConstraints || !relativePointerManager)
        return FALSE;

    WPE::WaylandSeat* seat = wpeDisplayWaylandGetSeat(display);
    priv->savedPointerModifiers = seat->pointerModifiers();
    priv->savedPointerCoords = seat->pointerCoords();
    struct wl_pointer* wlPointer = wl_seat_get_pointer(seat->seat());

    priv->relativePointer = zwp_relative_pointer_manager_v1_get_relative_pointer(relativePointerManager, wlPointer);
    zwp_relative_pointer_v1_add_listener(priv->relativePointer, &relativePointerListener, view);
    auto* wlSurface = wpe_view_wayland_get_wl_surface(WPE_VIEW_WAYLAND(view));
    priv->lockedPointer = zwp_pointer_constraints_v1_lock_pointer(pointerConstraints, wlSurface, wlPointer, nullptr, ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);

    return TRUE;
}

static gboolean wpeViewWaylandUnlockPointer(WPEView* view)
{
    auto* priv = WPE_VIEW_WAYLAND(view)->priv;
    g_clear_pointer(&priv->lockedPointer, zwp_locked_pointer_v1_destroy);
    g_clear_pointer(&priv->relativePointer, zwp_relative_pointer_v1_destroy);

    return TRUE;
}

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
    if (priv->resizedID) {
        g_signal_handler_disconnect(view, priv->resizedID);
        priv->resizedID = 0;
    }

    priv->opaqueRegion.clear();
    if (rects) {
        priv->opaqueRegion.reserveInitialCapacity(rectsCount);
        for (unsigned i = 0; i < rectsCount; ++i)
            priv->opaqueRegion.append(rects[i]);
    }
    if (auto* toplevel = wpe_view_get_toplevel(view))
        wpeToplevelWaylandSetOpaqueRectangles(WPE_TOPLEVEL_WAYLAND(toplevel), !priv->opaqueRegion.isEmpty() ? priv->opaqueRegion.data() : nullptr, priv->opaqueRegion.size());
}

static gboolean wpeViewWaylandCanBeMapped(WPEView* view)
{
    if (auto* toplevel = wpe_view_get_toplevel(view))
        return !!wpe_toplevel_get_screen(toplevel);
    return FALSE;
}

static void wpe_view_wayland_class_init(WPEViewWaylandClass* viewWaylandClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(viewWaylandClass);
    objectClass->constructed = wpeViewWaylandConstructed;
    objectClass->dispose = wpeViewWaylandDispose;

    WPEViewClass* viewClass = WPE_VIEW_CLASS(viewWaylandClass);
    viewClass->render_buffer = wpeViewWaylandRenderBuffer;
    viewClass->lock_pointer = wpeViewWaylandLockPointer;
    viewClass->unlock_pointer = wpeViewWaylandUnlockPointer;
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

    if (auto* toplevel = wpe_view_get_toplevel(WPE_VIEW(view)))
        return wpe_toplevel_wayland_get_wl_surface(WPE_TOPLEVEL_WAYLAND(toplevel));
    return nullptr;
}

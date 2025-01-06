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
#include "WPEViewDRM.h"

#include "DRMUniquePtr.h"
#include "WPEDisplayDRMPrivate.h"
#include "WPEScreenDRMPrivate.h"
#include "WPEToplevelDRM.h"
#include "WPEViewDRMPrivate.h"
#include <drm_fourcc.h>
#include <glib-unix.h>
#include <optional>
#include <wtf/FastMalloc.h>
#include <wtf/OptionSet.h>
#include <wtf/RunLoop.h>
#include <wtf/SafeStrerror.h>
#include <wtf/Seconds.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>

enum class UpdateFlags : uint8_t {
    BufferUpdateRequested = 1 << 0,
    CursorUpdateRequested = 1 << 1,
    BufferUpdatePending = 1 << 2,
    CursorUpdatePending = 1 << 3
};

/**
 * WPEViewDRM:
 *
 */
struct _WPEViewDRMPrivate {
    Seconds refreshDuration;
    std::optional<uint32_t> modeBlob;
    GRefPtr<WPEBuffer> pendingBuffer;
    GRefPtr<WPEBuffer> committedBuffer;
    Vector<drm_mode_rect> damageRects;
    drmEventContext eventContext;
    GRefPtr<GSource> eventSource;
    OptionSet<UpdateFlags> updateFlags;
    std::unique_ptr<RunLoop::Timer> cursorUpdateTimer;
};
WEBKIT_DEFINE_FINAL_TYPE(WPEViewDRM, wpe_view_drm, WPE_TYPE_VIEW, WPEView)

static void wpeViewDRMDidPageFlip(WPEViewDRM*);

static void wpeViewDRMConstructed(GObject* object)
{
    G_OBJECT_CLASS(wpe_view_drm_parent_class)->constructed(object);

    auto* view = WPE_VIEW(object);
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

        wpe_view_map(view);
    }), nullptr);

    auto* display = WPE_DISPLAY_DRM(wpe_view_get_display(view));
    auto* priv = WPE_VIEW_DRM(view)->priv;
    priv->refreshDuration = Seconds(1 / (wpe_screen_get_refresh_rate(wpeDisplayDRMGetScreen(display)) / 1000.));

    int fd = gbm_device_get_fd(wpe_display_drm_get_device(display));
    priv->eventContext.version = DRM_EVENT_CONTEXT_VERSION;
    priv->eventContext.page_flip_handler = [](int, unsigned, unsigned, unsigned, void* userData) {
        wpeViewDRMDidPageFlip(WPE_VIEW_DRM(userData));
    };

    priv->eventSource = adoptGRef(g_unix_fd_source_new(fd, static_cast<GIOCondition>(G_IO_IN | G_IO_ERR | G_IO_HUP)));
    g_source_set_name(priv->eventSource.get(), "WPE DRM events");
    g_source_set_priority(priv->eventSource.get(), G_PRIORITY_DEFAULT);
    g_source_set_can_recurse(priv->eventSource.get(), TRUE);
    g_source_set_callback(priv->eventSource.get(), reinterpret_cast<GSourceFunc>(reinterpret_cast<GCallback>(+[](int fd, GIOCondition condition, gpointer userData) -> gboolean {
        if (condition & (G_IO_ERR | G_IO_HUP))
            return G_SOURCE_REMOVE;

        if (condition & G_IO_IN) {
            auto* priv = WPE_VIEW_DRM(userData)->priv;
            drmHandleEvent(fd, &priv->eventContext);
        }
        return G_SOURCE_CONTINUE;
    })), object, nullptr);
    g_source_attach(priv->eventSource.get(), g_main_context_get_thread_default());
}

static void wpeViewDRMDispose(GObject* object)
{
    auto* priv = WPE_VIEW_DRM(object)->priv;

    priv->cursorUpdateTimer = nullptr;

    if (priv->modeBlob) {
        auto fd = gbm_device_get_fd(wpe_display_drm_get_device(WPE_DISPLAY_DRM(wpe_view_get_display(WPE_VIEW(object)))));
        drmModeDestroyPropertyBlob(fd, priv->modeBlob.value());
        priv->modeBlob = std::nullopt;
    }

    if (priv->eventSource) {
        g_source_destroy(priv->eventSource.get());
        priv->eventSource = nullptr;
    }

    G_OBJECT_CLASS(wpe_view_drm_parent_class)->dispose(object);
}

static WPE::DRM::Buffer* drmBufferCreateDMABuf(WPEView* view, WPEBuffer* buffer, bool modifiersSupported, GError** error)
{
    auto* device = wpe_display_drm_get_device(WPE_DISPLAY_DRM(wpe_view_get_display(view)));
    auto* dmaBuffer = WPE_BUFFER_DMA_BUF(buffer);
    struct gbm_bo* bo;
    if (modifiersSupported) {
        auto planeCount = wpe_buffer_dma_buf_get_n_planes(dmaBuffer);
        struct gbm_import_fd_modifier_data fdModifierData = {
            static_cast<uint32_t>(wpe_buffer_get_width(buffer)),
            static_cast<uint32_t>(wpe_buffer_get_height(buffer)),
            wpe_buffer_dma_buf_get_format(dmaBuffer),
            planeCount, {
                wpe_buffer_dma_buf_get_fd(dmaBuffer, 0),
                planeCount > 1 ? wpe_buffer_dma_buf_get_fd(dmaBuffer, 1) : -1,
                planeCount > 2 ? wpe_buffer_dma_buf_get_fd(dmaBuffer, 2) : -1,
                planeCount > 3 ? wpe_buffer_dma_buf_get_fd(dmaBuffer, 3) : -1
            }, {
                static_cast<int>(wpe_buffer_dma_buf_get_stride(dmaBuffer, 0)),
                planeCount > 1 ? static_cast<int>(wpe_buffer_dma_buf_get_stride(dmaBuffer, 1)) : 0,
                planeCount > 2 ? static_cast<int>(wpe_buffer_dma_buf_get_stride(dmaBuffer, 2)) : 0,
                planeCount > 3 ? static_cast<int>(wpe_buffer_dma_buf_get_stride(dmaBuffer, 3)) : 0
            }, {
                static_cast<int>(wpe_buffer_dma_buf_get_offset(dmaBuffer, 0)),
                planeCount > 1 ? static_cast<int>(wpe_buffer_dma_buf_get_offset(dmaBuffer, 1)) : 0,
                planeCount > 2 ? static_cast<int>(wpe_buffer_dma_buf_get_offset(dmaBuffer, 2)) : 0,
                planeCount > 3 ? static_cast<int>(wpe_buffer_dma_buf_get_offset(dmaBuffer, 3)) : 0
            },
            wpe_buffer_dma_buf_get_modifier(dmaBuffer)
        };
        bo = gbm_bo_import(device, GBM_BO_IMPORT_FD_MODIFIER, &fdModifierData, GBM_BO_USE_SCANOUT);
    } else {
        struct gbm_import_fd_data fdData = {
            wpe_buffer_dma_buf_get_fd(dmaBuffer, 0),
            static_cast<uint32_t>(wpe_buffer_get_width(buffer)),
            static_cast<uint32_t>(wpe_buffer_get_height(buffer)),
            wpe_buffer_dma_buf_get_stride(dmaBuffer, 0),
            wpe_buffer_dma_buf_get_format(dmaBuffer)
        };
        bo = gbm_bo_import(device, GBM_BO_IMPORT_FD, &fdData, GBM_BO_USE_SCANOUT);
    }
    if (!bo) {
        g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render buffer: failed to import buffer for scanout");
        return nullptr;
    }

    auto drmBuffer = WPE::DRM::Buffer::create(bo);
    if (!drmBuffer) {
        g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render buffer: failed to create DRM frame buffer");
        gbm_bo_destroy(bo);
        return nullptr;
    }

    auto* drmBufferPtr = drmBuffer.get();
    wpe_buffer_set_user_data(buffer, drmBuffer.release(), reinterpret_cast<GDestroyNotify>(+[](void* userData) {
        delete static_cast<WPE::DRM::Buffer*>(userData);
    }));
    return drmBufferPtr;
}

static WPE::DRM::Buffer* drmBufferCreate(WPEView* view, WPEBuffer* buffer, bool modifiersSupported, GError** error)
{
    // FIXME: check bounds.
    if (WPE_IS_BUFFER_DMA_BUF(buffer))
        return drmBufferCreateDMABuf(view, buffer, modifiersSupported, error);

    // FIXME: implement.
    g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render buffer: unsupported buffer");
    return nullptr;
}

static bool drmAtomicAddProperty(drmModeAtomicReq* request, uint32_t id, const WPE::DRM::Property& property)
{
    if (!property.first)
        return false;

    return drmModeAtomicAddProperty(request, id, property.first, property.second) > 0;
}

static bool addCrtcProperties(drmModeAtomicReq* request, const WPE::DRM::Crtc& crtc, uint32_t modeID)
{
    auto properties = crtc.properties();
    properties.active.second = 1;
    properties.modeID.second = modeID;

    bool success = drmAtomicAddProperty(request, crtc.id(), properties.active);
    success &= drmAtomicAddProperty(request, crtc.id(), properties.modeID);
    return success;
}

static bool addConnectorProperties(drmModeAtomicReq* request, const WPE::DRM::Connector& connector, uint32_t crtcID)
{
    auto properties = connector.properties();
    properties.crtcID.second = crtcID;
    properties.linkStatus.second = DRM_MODE_LINK_STATUS_GOOD;

    bool success = drmAtomicAddProperty(request, connector.id(), properties.crtcID);
    success &= drmAtomicAddProperty(request, connector.id(), properties.linkStatus);
    return success;
}

WPE::DRM::Plane::Properties emptyPlaneProperties(const WPE::DRM::Plane& plane)
{
    auto properties = plane.properties();
    properties.crtcID.second = 0;
    properties.crtcX.second = 0;
    properties.crtcY.second = 0;
    properties.crtcW.second = 0;
    properties.crtcH.second = 0;
    properties.fbID.second = 0;
    properties.srcX.second = 0;
    properties.srcY.second = 0;
    properties.srcW.second = 0;
    properties.srcH.second = 0;
    properties.fbDamageClips.second = 0;
    return properties;
}

WPE::DRM::Plane::Properties primaryPlaneProperties(const WPE::DRM::Plane& plane, uint32_t crtcID, drmModeModeInfo* mode, const WPE::DRM::Buffer& buffer, std::optional<uint32_t> damageID)
{
    auto properties = plane.properties();
    properties.crtcID.second = crtcID;
    properties.crtcX.second = 0;
    properties.crtcY.second = 0;
    properties.crtcW.second = mode->hdisplay;
    properties.crtcH.second = mode->vdisplay;
    properties.fbID.second = buffer.frameBufferID();
    properties.srcX.second = 0;
    properties.srcY.second = 0;
    properties.srcW.second = (static_cast<uint64_t>(gbm_bo_get_width(buffer.bufferObject())) << 16);
    properties.srcH.second = (static_cast<uint64_t>(gbm_bo_get_height(buffer.bufferObject())) << 16);
    if (properties.fbDamageClips.first && damageID)
        properties.fbDamageClips.second = damageID.value();
    if (properties.inFenceFD.first) {
        if (const auto& inFenceFD = buffer.fenceFD())
            properties.inFenceFD.second = inFenceFD.value();
    }
    return properties;
}

WPE::DRM::Plane::Properties cursorPlaneProperties(uint32_t crtcID, const WPE::DRM::Cursor& cursor)
{
    auto properties = cursor.plane().properties();
    properties.crtcID.second = crtcID;
    properties.crtcX.second = cursor.x();
    properties.crtcY.second = cursor.y();
    properties.crtcW.second = gbm_bo_get_width(cursor.buffer()->bufferObject());
    properties.crtcH.second = gbm_bo_get_width(cursor.buffer()->bufferObject());
    properties.fbID.second = cursor.buffer()->frameBufferID();
    properties.srcX.second = 0;
    properties.srcY.second = 0;
    properties.srcW.second = (static_cast<uint64_t>(gbm_bo_get_width(cursor.buffer()->bufferObject())) << 16);
    properties.srcH.second = (static_cast<uint64_t>(gbm_bo_get_width(cursor.buffer()->bufferObject())) << 16);
    return properties;
}

static bool addPlaneProperties(drmModeAtomicReq* request, const WPE::DRM::Plane& plane, WPE::DRM::Plane::Properties&& properties)
{
    bool success = drmAtomicAddProperty(request, plane.id(), properties.crtcID);
    success &= drmAtomicAddProperty(request, plane.id(), properties.crtcX);
    success &= drmAtomicAddProperty(request, plane.id(), properties.crtcY);
    success &= drmAtomicAddProperty(request, plane.id(), properties.crtcW);
    success &= drmAtomicAddProperty(request, plane.id(), properties.crtcH);
    success &= drmAtomicAddProperty(request, plane.id(), properties.fbID);
    success &= drmAtomicAddProperty(request, plane.id(), properties.srcX);
    success &= drmAtomicAddProperty(request, plane.id(), properties.srcY);
    success &= drmAtomicAddProperty(request, plane.id(), properties.srcW);
    success &= drmAtomicAddProperty(request, plane.id(), properties.srcH);
    if (properties.fbDamageClips.first)
        success &= drmAtomicAddProperty(request, plane.id(), properties.fbDamageClips);
    return success;
}

static bool wpeViewDRMCommitAtomic(WPEViewDRM* view, WPE::DRM::Buffer* buffer, std::optional<uint32_t> damageID, GError** error)
{
    WPE::DRM::UniquePtr<drmModeAtomicReq> request(drmModeAtomicAlloc());
    uint32_t flags = DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK;

    auto* display = WPE_DISPLAY_DRM(wpe_view_get_display(WPE_VIEW(view)));
    auto* screen = WPE_SCREEN_DRM(wpeDisplayDRMGetScreen(display));
    const auto& crtc = wpeScreenDRMGetCrtc(screen);
    auto* mode = wpeScreenDRMGetMode(screen);
    auto fd = gbm_device_get_fd(wpe_display_drm_get_device(display));
    if (!crtc.modeIsCurrent(mode)) {
        flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;

        if (!view->priv->modeBlob) {
            uint32_t blobID;
            auto result = drmModeCreatePropertyBlob(fd, mode, sizeof(drmModeModeInfo), &blobID);
            if (result < 0) {
                g_set_error(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render buffer: failed to crate blob from DRM mode: %s", safeStrerror(-result).data());
                return false;
            }

            view->priv->modeBlob = blobID;
        }

        const auto& connector = wpeDisplayDRMGetConnector(display);
        bool success = addCrtcProperties(request.get(), crtc, view->priv->modeBlob.value());
        success &= addConnectorProperties(request.get(), connector, crtc.id());
        if (!success) {
            g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render buffer: failed to set DRM mode");
            return false;
        }
    }

    auto& plane = wpeDisplayDRMGetPrimaryPlane(display);
    if (!addPlaneProperties(request.get(), plane, buffer ? primaryPlaneProperties(plane, crtc.id(), mode, *buffer, damageID) : emptyPlaneProperties(plane))) {
        g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render buffer: failed to set plane properties");
        return false;
    }

    if (auto* cursor = wpeDisplayDRMGetCursor(display))
        addPlaneProperties(request.get(), cursor->plane(), cursor->buffer() ? cursorPlaneProperties(crtc.id(), *cursor) : emptyPlaneProperties(cursor->plane()));

    if (drmModeAtomicCommit(fd, request.get(), flags, view)) {
        g_set_error(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render buffer: failed to commit properties: %s", strerror(errno));
        return false;
    }

    return true;
}

static bool wpeViewDRMCommitLegacy(WPEViewDRM* view, const WPE::DRM::Buffer& buffer, GError** error)
{
    auto* display = WPE_DISPLAY_DRM(wpe_view_get_display(WPE_VIEW(view)));
    auto* screen = WPE_SCREEN_DRM(wpeDisplayDRMGetScreen(display));
    const auto& crtc = wpeScreenDRMGetCrtc(screen);
    auto* mode = wpeScreenDRMGetMode(screen);
    auto fd = gbm_device_get_fd(wpe_display_drm_get_device(display));
    if (!crtc.modeIsCurrent(mode)) {
        const auto& connector = wpeDisplayDRMGetConnector(display);
        auto connectorID = connector.id();
        if (drmModeSetCrtc(fd, crtc.id(), buffer.frameBufferID(), 0, 0, &connectorID, 1, mode)) {
            g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render buffer: failed to set CRTC");
            return false;
        }
    }

    // FIXME: support cursors in legacy mode.

    if (drmModePageFlip(fd, crtc.id(), buffer.frameBufferID(), DRM_MODE_PAGE_FLIP_EVENT, view)) {
        g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render buffer: failed to request page flip");
        return false;
    }

    return true;
}

static std::pair<uint32_t, uint64_t> wpeBufferFormat(WPEBuffer* buffer)
{
    if (WPE_IS_BUFFER_DMA_BUF(buffer)) {
        auto* dmaBuffer = WPE_BUFFER_DMA_BUF(buffer);
        return { wpe_buffer_dma_buf_get_format(dmaBuffer), wpe_buffer_dma_buf_get_modifier(dmaBuffer) };
    }

    return { DRM_FORMAT_INVALID, DRM_FORMAT_MOD_INVALID };
}

static std::optional<uint32_t> buildDamageBlob(WPEDisplayDRM* display, const Vector<drm_mode_rect>& damageRects, GError** error)
{
    if (damageRects.isEmpty())
        return std::nullopt;

    uint32_t blobID;
    int fd = gbm_device_get_fd(wpe_display_drm_get_device(display));
    auto result = drmModeCreatePropertyBlob(fd, damageRects.data(), damageRects.sizeInBytes(), &blobID);
    if (result < 0) {
        g_set_error(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render buffer: failed to crate damage blob: %s", safeStrerror(-result).data());
        return 0;
    }

    return blobID;
}

static void destroyDamageBlob(WPEDisplayDRM* display, uint32_t blobID)
{
    int fd = gbm_device_get_fd(wpe_display_drm_get_device(display));
    drmModeDestroyPropertyBlob(fd, blobID);
}

static gboolean wpeViewDRMRequestUpdate(WPEViewDRM* view, GError** error)
{
    auto* priv = view->priv;
    auto* buffer = priv->pendingBuffer ? priv->pendingBuffer.get() : priv->committedBuffer.get();
    auto* drmBuffer = buffer ? static_cast<WPE::DRM::Buffer*>(wpe_buffer_get_user_data(buffer)) : nullptr;
    auto* display = WPE_DISPLAY_DRM(wpe_view_get_display(WPE_VIEW(view)));
    if (wpe_display_drm_supports_atomic(display)) {
        auto damageID = drmBuffer ? buildDamageBlob(display, priv->damageRects, error) : std::nullopt;
        if (damageID.has_value() && !damageID.value())
            return FALSE;

        auto result = wpeViewDRMCommitAtomic(WPE_VIEW_DRM(view), drmBuffer, damageID, error);
        if (damageID)
            destroyDamageBlob(display, damageID.value());
        priv->damageRects.clear();
        return result;
    }

    return wpeViewDRMCommitLegacy(WPE_VIEW_DRM(view), *drmBuffer, error);
}

static gboolean wpeViewDRMRenderBuffer(WPEView* view, WPEBuffer* buffer, const WPERectangle* damageRects, guint nDamageRects, GError** error)
{
    auto* drmBuffer = static_cast<WPE::DRM::Buffer*>(wpe_buffer_get_user_data(buffer));
    if (!drmBuffer) {
        auto* display = WPE_DISPLAY_DRM(wpe_view_get_display(view));
        auto& plane = wpeDisplayDRMGetPrimaryPlane(display);
        auto format = wpeBufferFormat(buffer);
        if (!plane.supportsFormat(format.first, format.second)) {
            g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render buffer: buffer format is not supported by DRM plane");
            return FALSE;
        }

        drmBuffer = drmBufferCreate(view, buffer, wpe_display_drm_supports_modifiers(display), error);
        if (!drmBuffer)
            return FALSE;
    }

    if (WPE_IS_BUFFER_DMA_BUF(buffer))
        drmBuffer->setFenceFD(UnixFileDescriptor { wpe_buffer_dma_buf_take_rendering_fence(WPE_BUFFER_DMA_BUF(buffer)), UnixFileDescriptor::Adopt });

    auto* priv = WPE_VIEW_DRM(view)->priv;
    priv->pendingBuffer = buffer;
    priv->damageRects.clear();
    priv->damageRects.reserveInitialCapacity(nDamageRects);
    for (unsigned i = 0; i < nDamageRects; ++i)
        priv->damageRects.append({ damageRects[i].x, damageRects[i].y, damageRects[i].x + damageRects[i].width, damageRects[i].y + damageRects[i].height });

    if (priv->updateFlags.contains(UpdateFlags::CursorUpdateRequested)) {
        priv->updateFlags.add(UpdateFlags::BufferUpdatePending);
        return TRUE;
    }

    if (priv->cursorUpdateTimer)
        priv->cursorUpdateTimer->stop();
    if (wpeViewDRMRequestUpdate(WPE_VIEW_DRM(view), error)) {
        priv->updateFlags.add(UpdateFlags::BufferUpdateRequested);
        return TRUE;
    }

    return FALSE;
}

static void wpeViewDRMSetCursorFromName(WPEView* view, const char* name)
{
    if (auto* cursor = wpeDisplayDRMGetCursor(WPE_DISPLAY_DRM(wpe_view_get_display(view))))
        cursor->setFromName(name, wpe_view_get_scale(view));
}

static void wpeViewDRMSetCursorFromBytes(WPEView* view, GBytes* bytes, guint width, guint height, guint stride, guint hotspotX, guint hotspotY)
{
    if (auto* cursor = wpeDisplayDRMGetCursor(WPE_DISPLAY_DRM(wpe_view_get_display(view))))
        cursor->setFromBytes(bytes, width, height, stride, hotspotX, hotspotY);
}

static void wpeViewDRMScheduleCursorUpdate(WPEViewDRM* view)
{
    auto* priv = view->priv;
    if (priv->cursorUpdateTimer && priv->cursorUpdateTimer->isActive())
        return;

    if (!priv->cursorUpdateTimer) {
        priv->cursorUpdateTimer = makeUnique<RunLoop::Timer>(RunLoop::current(), [view] {
            if (wpeViewDRMRequestUpdate(view, nullptr))
                view->priv->updateFlags.add(UpdateFlags::CursorUpdateRequested);
        });
    }

    // Wait until the end of the frame to do the cursor update.
    auto* display = WPE_DISPLAY_DRM(wpe_view_get_display(WPE_VIEW(view)));
    auto* screen = WPE_SCREEN_DRM(wpeDisplayDRMGetScreen(display));
    auto crtcIndex = wpeScreenDRMGetCrtc(screen).index();
    int crtcBitmask = 0;
    if (crtcIndex > 1)
        crtcBitmask = ((crtcIndex << DRM_VBLANK_HIGH_CRTC_SHIFT) & DRM_VBLANK_HIGH_CRTC_MASK);
    else if (crtcIndex > 0)
        crtcBitmask = DRM_VBLANK_SECONDARY;

    drmVBlank vblank;
    vblank.request.type = static_cast<drmVBlankSeqType>(DRM_VBLANK_RELATIVE | crtcBitmask);
    vblank.request.sequence = 0;
    vblank.request.signal = 0;
    drmWaitVBlank(gbm_device_get_fd(wpe_display_drm_get_device(display)), &vblank);

    auto lastVBlank = Seconds::fromMicroseconds(vblank.reply.tval_sec * G_USEC_PER_SEC + vblank.reply.tval_usec);
    auto elapsed = MonotonicTime::now().secondsSinceEpoch() - lastVBlank;
    priv->cursorUpdateTimer->startOneShot(priv->refreshDuration - elapsed - 1_ms);
}

static void wpeViewDRMDidPageFlip(WPEViewDRM* view)
{
    auto* priv = view->priv;
    auto updateFlags = std::exchange(priv->updateFlags, OptionSet<UpdateFlags> { });
    if (updateFlags.contains(UpdateFlags::BufferUpdateRequested)) {
        if (priv->committedBuffer)
            wpe_view_buffer_released(WPE_VIEW(view), priv->committedBuffer.get());
        priv->committedBuffer = WTFMove(priv->pendingBuffer);
        wpe_view_buffer_rendered(WPE_VIEW(view), priv->committedBuffer.get());
    }

    if (updateFlags.contains(UpdateFlags::BufferUpdatePending)) {
        if (wpeViewDRMRequestUpdate(view, nullptr))
            priv->updateFlags.add(UpdateFlags::BufferUpdateRequested);
    } else if (updateFlags.contains(UpdateFlags::CursorUpdatePending))
        wpeViewDRMScheduleCursorUpdate(view);
}

void wpeViewDRMUpdateCursor(WPEViewDRM* view, double x, double y)
{
    auto* cursor = wpeDisplayDRMGetCursor(WPE_DISPLAY_DRM(wpe_view_get_display(WPE_VIEW(view))));
    if (!cursor)
        return;

    if (!cursor->setPosition(x, y))
        return;

    if (view->priv->updateFlags.containsAny({ UpdateFlags::CursorUpdateRequested, UpdateFlags::BufferUpdateRequested })) {
        view->priv->updateFlags.add(UpdateFlags::CursorUpdatePending);
        return;
    }

    wpeViewDRMScheduleCursorUpdate(view);
}

static void wpe_view_drm_class_init(WPEViewDRMClass* viewDRMClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(viewDRMClass);
    objectClass->constructed = wpeViewDRMConstructed;
    objectClass->dispose = wpeViewDRMDispose;

    WPEViewClass* viewClass = WPE_VIEW_CLASS(viewDRMClass);
    viewClass->render_buffer = wpeViewDRMRenderBuffer;
    viewClass->set_cursor_from_name = wpeViewDRMSetCursorFromName;
    viewClass->set_cursor_from_bytes = wpeViewDRMSetCursorFromBytes;
}

/**
 * wpe_view_drm_new:
 * @display: a #WPEDisplayDRM
 *
 * Create a new #WPEViewDRM
 *
 * Returns: (transfer full): a #WPEView
 */
WPEView* wpe_view_drm_new(WPEDisplayDRM* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY_DRM(display), nullptr);

    return WPE_VIEW(g_object_new(WPE_TYPE_VIEW_DRM, "display", display, nullptr));
}

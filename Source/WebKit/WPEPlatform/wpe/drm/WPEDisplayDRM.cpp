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
#include "WPEDisplayDRM.h"

#include "DRMUniquePtr.h"
#include "WPEDisplayDRMPrivate.h"
#include "WPEExtensions.h"
#include "WPEViewDRM.h"
#include <fcntl.h>
#include <gio/gio.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/unix/UnixFileDescriptor.h>

/**
 * WPEDisplayDRM:
 *
 */
struct _WPEDisplayDRMPrivate {
    UnixFileDescriptor fd;
    struct gbm_device* device;
    bool atomicSupported;
    bool modifiersSupported;
    uint32_t cursorWidth;
    uint32_t cursorHeight;
    std::unique_ptr<WPE::DRM::Connector> connector;
    std::unique_ptr<WPE::DRM::Crtc> crtc;
    std::unique_ptr<WPE::DRM::Plane> primaryPlane;
    std::unique_ptr<WPE::DRM::Cursor> cursor;
    std::unique_ptr<WPE::DRM::Seat> seat;
};
WEBKIT_DEFINE_FINAL_TYPE_WITH_CODE(WPEDisplayDRM, wpe_display_drm, WPE_TYPE_DISPLAY, WPEDisplay,
    wpeEnsureExtensionPointsRegistered();
    g_io_extension_point_implement(WPE_DISPLAY_EXTENSION_POINT_NAME, g_define_type_id, "wpe-display-drm", -100))

static void wpeDisplayDRMDispose(GObject* object)
{
    auto* priv = WPE_DISPLAY_DRM(object)->priv;

    g_clear_pointer(&priv->device, gbm_device_destroy);

    if (priv->fd) {
        drmDropMaster(priv->fd.value());
        priv->fd = { };
    }

    G_OBJECT_CLASS(wpe_display_drm_parent_class)->dispose(object);
}

static UnixFileDescriptor findDevice(GError** error)
{
    drmDevicePtr devices[64];
    memset(devices, 0, sizeof(devices));

    int numDevices = drmGetDevices2(0, devices, std::size(devices));
    if (numDevices <= 0) {
        g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "No DRM devices found");
        return { };
    }

    UnixFileDescriptor fd;
    for (int i = 0; i < numDevices; ++i) {
        drmDevice* device = devices[i];
        if (!(device->available_nodes & (1 << DRM_NODE_PRIMARY)))
            continue;

        fd = UnixFileDescriptor { open(device->nodes[DRM_NODE_PRIMARY], O_RDWR | O_CLOEXEC), UnixFileDescriptor::Adopt };
        if (fd)
            break;
    }

    drmFreeDevices(devices, numDevices);

    if (!fd) {
        g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "No suitable DRM device found");
        return { };
    }

    return fd;
}

static bool wpeDisplayDRMInitializeCapabilities(WPEDisplayDRM* display, int fd, GError** error)
{
    if (drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1)) {
        g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "DRM universal planes not supported");
        return false;
    }

    if (!getenv("WPE_DRM_DISABLE_ATOMIC"))
        display->priv->atomicSupported = !drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1);

    uint64_t capability;
    display->priv->modifiersSupported = !drmGetCap(fd, DRM_CAP_ADDFB2_MODIFIERS, &capability) && capability;

    if (!drmGetCap(fd, DRM_CAP_CURSOR_WIDTH, &capability))
        display->priv->cursorWidth = capability;
    else
        display->priv->cursorWidth = 64;

    if (!drmGetCap(fd, DRM_CAP_CURSOR_HEIGHT, &capability))
        display->priv->cursorHeight = capability;
    else
        display->priv->cursorHeight = 64;

    return true;
}

static std::unique_ptr<WPE::DRM::Connector> chooseConnector(int fd, drmModeRes* resources, GError** error)
{
    for (int i = 0; i < resources->count_connectors; ++i) {
        WPE::DRM::UniquePtr<drmModeConnector> connector(drmModeGetConnector(fd, resources->connectors[i]));
        if (!connector || connector->connection != DRM_MODE_CONNECTED || connector->connector_type == DRM_MODE_CONNECTOR_WRITEBACK || !connector->encoder_id)
            continue;

        if (auto conn = WPE::DRM::Connector::create(fd, connector.get()))
            return conn;
    }

    g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "Failed to find a DRM connector");
    return nullptr;
}

static std::unique_ptr<WPE::DRM::Crtc> chooseCrtcForConnector(int fd, drmModeRes* resources, const WPE::DRM::Connector& connector, GError** error)
{
    WPE::DRM::UniquePtr<drmModeEncoder> encoder(drmModeGetEncoder(fd, connector.encoderID()));
    if (!encoder) {
        g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "Failed to get the connector encoder");
        return nullptr;
    }

    for (int i = 0; i < resources->count_crtcs; ++i) {
        if (resources->crtcs[i] == encoder->crtc_id) {
            WPE::DRM::UniquePtr<drmModeCrtc> drmCrtc(drmModeGetCrtc(fd, resources->crtcs[i]));
            return WPE::DRM::Crtc::create(fd, drmCrtc.get(), i);
        }
    }

    g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "Failed to find a DRM crtc");
    return nullptr;
}

static std::unique_ptr<WPE::DRM::Plane> choosePlaneForCrtc(int fd, WPE::DRM::Plane::Type type, const WPE::DRM::Crtc& crtc, bool modifiersSupported, GError** error)
{
    WPE::DRM::UniquePtr<drmModePlaneRes> planeResources(drmModeGetPlaneResources(fd));
    if (!planeResources) {
        g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "No DRM plane resources found");
        return nullptr;
    }

    for (uint32_t i = 0; i < planeResources->count_planes; ++i) {
        WPE::DRM::UniquePtr<drmModePlane> drmPlane(drmModeGetPlane(fd, planeResources->planes[i]));
        auto plane = WPE::DRM::Plane::create(fd, type, drmPlane.get(), modifiersSupported);
        if (!plane)
            continue;

        if (plane->canBeUsedWithCrtc(crtc))
            return plane;
    }

    g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "No DRM planes found");
    return nullptr;
}

static gboolean wpeDisplayDRMConnect(WPEDisplay* display, GError** error)
{
    // FIXME: make it possible to pass a device.
    auto fd = findDevice(error);
    if (!fd)
        return FALSE;

    if (drmSetMaster(fd.value()) == -1) {
        g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "Failed to become DRM master");
        return FALSE;
    }

    auto displayDRM = WPE_DISPLAY_DRM(display);
    if (!wpeDisplayDRMInitializeCapabilities(displayDRM, fd.value(), error))
        return FALSE;

    WPE::DRM::UniquePtr<drmModeRes> resources(drmModeGetResources(fd.value()));
    if (!resources) {
        g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "Failed to get device resources");
        return FALSE;
    }

    auto connector = chooseConnector(fd.value(), resources.get(), error);
    if (!connector)
        return FALSE;

    auto crtc = chooseCrtcForConnector(fd.value(), resources.get(), *connector, error);
    if (!crtc)
        return FALSE;

    auto primaryPlane = choosePlaneForCrtc(fd.value(), WPE::DRM::Plane::Type::Primary, *crtc, displayDRM->priv->modifiersSupported, error);
    if (!primaryPlane)
        return FALSE;

    auto cursorPlane = choosePlaneForCrtc(fd.value(), WPE::DRM::Plane::Type::Cursor, *crtc, displayDRM->priv->modifiersSupported, nullptr);

    auto* device = gbm_create_device(fd.value());
    if (!device) {
        g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "Failed to create GBM device");
        return FALSE;
    }

    displayDRM->priv->fd = WTFMove(fd);
    displayDRM->priv->device = device;
    displayDRM->priv->connector = WTFMove(connector);
    displayDRM->priv->crtc = WTFMove(crtc);
    displayDRM->priv->primaryPlane = WTFMove(primaryPlane);
    displayDRM->priv->seat = WPE::DRM::Seat::create();
    if (cursorPlane)
        displayDRM->priv->cursor = makeUnique<WPE::DRM::Cursor>(WTFMove(cursorPlane), device, displayDRM->priv->cursorWidth, displayDRM->priv->cursorHeight);

    return TRUE;
}

static WPEView* wpeDisplayDRMCreateView(WPEDisplay* display)
{
    auto* displayDRM = WPE_DISPLAY_DRM(display);
    auto* view = wpe_view_drm_new(displayDRM);
    displayDRM->priv->seat->setView(view);
    return view;
}

static GList* wpeDisplayDRMGetPreferredDMABufFormats(WPEDisplay* display)
{
    auto* displayDRM = WPE_DISPLAY_DRM(display);
    GList* preferredFormats = nullptr;
    for (const auto& format : displayDRM->priv->primaryPlane->formats()) {
        GRefPtr<GArray> dmabufModifiers = adoptGRef(g_array_sized_new(FALSE, TRUE, sizeof(guint64), format.modifiers.size()));
        for (auto modifier : format.modifiers)
            g_array_append_val(dmabufModifiers.get(), modifier);
        preferredFormats = g_list_prepend(preferredFormats, wpe_buffer_dma_buf_format_new(WPE_BUFFER_DMA_BUF_FORMAT_USAGE_SCANOUT, format.format, dmabufModifiers.get()));
    }
    return g_list_reverse(preferredFormats);
}

static void wpe_display_drm_class_init(WPEDisplayDRMClass* displayDRMClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(displayDRMClass);
    objectClass->dispose = wpeDisplayDRMDispose;

    WPEDisplayClass* displayClass = WPE_DISPLAY_CLASS(displayDRMClass);
    displayClass->connect = wpeDisplayDRMConnect;
    displayClass->create_view = wpeDisplayDRMCreateView;
    displayClass->get_preferred_dma_buf_formats = wpeDisplayDRMGetPreferredDMABufFormats;
}

const WPE::DRM::Connector& wpeDisplayDRMGetConnector(WPEDisplayDRM* display)
{
    return *display->priv->connector;
}

const WPE::DRM::Crtc& wpeDisplayDRMGetCrtc(WPEDisplayDRM* display)
{
    return *display->priv->crtc;
}

const WPE::DRM::Plane& wpeDisplayDRMGetPrimaryPlane(WPEDisplayDRM* display)
{
    return *display->priv->primaryPlane;
}

WPE::DRM::Cursor* wpeDisplayDRMGetCursor(WPEDisplayDRM* display)
{
    return display->priv->cursor.get();
}

const WPE::DRM::Seat& wpeDisplayDRMGetSeat(WPEDisplayDRM* display)
{
    return *display->priv->seat;
}

/**
 * wpe_display_drm_new:
 *
 * Create a new #WPEDisplayDRM
 *
 * Returns: (transfer full): a #WPEDisplay
 */
WPEDisplay* wpe_display_drm_new(void)
{
    return WPE_DISPLAY(g_object_new(WPE_TYPE_DISPLAY_DRM, nullptr));
}

/**
 * wpe_display_drm_get_device: (skip)
 * @display: a #WPEDisplayDRM
 *
 * Get the GBM device for @display
 *
 * Returns: (transfer none) (nullable): a `struct gbm_display`,
     or %NULL if display is not connected
 */
struct gbm_device* wpe_display_drm_get_device(WPEDisplayDRM* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY_DRM(display), nullptr);

    return display->priv->device;
}

/**
 * wpe_display_drm_supports_atomic:
 * @display: a #WPEDisplayDRM
 *
 * Get whether the DRM driver supports atomic mode-setting
 *
 * Returns: %TRUE if atomic mode-setting is supported, or %FALSE otherwise
 */
gboolean wpe_display_drm_supports_atomic(WPEDisplayDRM* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY_DRM(display), FALSE);

    return display->priv->atomicSupported;
}

/**
 * wpe_display_drm_supports_modifiers:
 * @display: a #WPEDisplayDRM
 *
 * Get whether the DRM driver supports format modifiers
 *
 * Returns: %TRUE if format modifiers are supported, or %FALSE otherwise
 */
gboolean wpe_display_drm_supports_modifiers(WPEDisplayDRM* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY_DRM(display), FALSE);

    return display->priv->modifiersSupported;
}

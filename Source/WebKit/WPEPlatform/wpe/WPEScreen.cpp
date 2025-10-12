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
#include "WPEScreen.h"

#include <wtf/glib/WTFGType.h>

#if USE(LIBDRM)
#include "WPEScreenSyncObserverDRM.h"
#include <errno.h>
#include <fcntl.h>
#include <optional>
#include <wtf/glib/GRefPtr.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#endif

/**
 * WPEScreen:
 *
 */
struct _WPEScreenPrivate {
    guint32 id;
    int x { -1 };
    int y { -1 };
    int width { -1 };
    int height { -1 };
    int physicalWidth { -1 };
    int physicalHeight { -1 };
    gdouble scale { 1 };
    int refreshRate { -1 };
#if USE(LIBDRM)
    GRefPtr<WPEScreenSyncObserver> syncObserver;
#endif
};

WEBKIT_DEFINE_ABSTRACT_TYPE(WPEScreen, wpe_screen, G_TYPE_OBJECT)

enum {
    PROP_0,

    PROP_ID,
    PROP_X,
    PROP_Y,
    PROP_WIDTH,
    PROP_HEIGHT,
    PROP_PHYSICAL_WIDTH,
    PROP_PHYSICAL_HEIGHT,
    PROP_SCALE,
    PROP_REFRESH_RATE,

    N_PROPERTIES
};

static std::array<GParamSpec*, N_PROPERTIES> sObjProperties;

static void wpeScreenSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    auto* screen = WPE_SCREEN(object);

    switch (propId) {
    case PROP_ID:
        screen->priv->id = g_value_get_uint(value);
        break;
    case PROP_X:
        wpe_screen_set_position(screen, g_value_get_int(value), -1);
        break;
    case PROP_Y:
        wpe_screen_set_position(screen, -1, g_value_get_int(value));
        break;
    case PROP_WIDTH:
        wpe_screen_set_size(screen, g_value_get_int(value), -1);
        break;
    case PROP_HEIGHT:
        wpe_screen_set_size(screen, -1, g_value_get_int(value));
        break;
    case PROP_PHYSICAL_WIDTH:
        wpe_screen_set_physical_size(screen, g_value_get_int(value), -1);
        break;
    case PROP_PHYSICAL_HEIGHT:
        wpe_screen_set_physical_size(screen, -1, g_value_get_int(value));
        break;
    case PROP_SCALE:
        wpe_screen_set_scale(screen, g_value_get_double(value));
        break;
    case PROP_REFRESH_RATE:
        wpe_screen_set_refresh_rate(screen, g_value_get_int(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void wpeScreenGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    auto* screen = WPE_SCREEN(object);

    switch (propId) {
    case PROP_ID:
        g_value_set_uint(value, wpe_screen_get_id(screen));
        break;
    case PROP_X:
        g_value_set_int(value, wpe_screen_get_x(screen));
        break;
    case PROP_Y:
        g_value_set_int(value, wpe_screen_get_y(screen));
        break;
    case PROP_WIDTH:
        g_value_set_int(value, wpe_screen_get_width(screen));
        break;
    case PROP_HEIGHT:
        g_value_set_int(value, wpe_screen_get_height(screen));
        break;
    case PROP_PHYSICAL_WIDTH:
        g_value_set_int(value, wpe_screen_get_physical_width(screen));
        break;
    case PROP_PHYSICAL_HEIGHT:
        g_value_set_int(value, wpe_screen_get_physical_height(screen));
        break;
    case PROP_SCALE:
        g_value_set_double(value, wpe_screen_get_scale(screen));
        break;
    case PROP_REFRESH_RATE:
        g_value_set_int(value, wpe_screen_get_refresh_rate(screen));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void wpeScreenInvalidate(WPEScreen* screen)
{
#if USE(LIBDRM)
    screen->priv->syncObserver = nullptr;
#endif
}

#if USE(LIBDRM)
static std::optional<uint32_t> findCrtc(WPEScreen* screen, int fd)
{
    drmModeRes* resources = drmModeGetResources(fd);
    if (!resources)
        return std::nullopt;

    std::optional<uint32_t> crtcIndex;
    uint32_t widthMM = wpe_screen_get_physical_width(screen);
    uint32_t heightMM = wpe_screen_get_physical_height(screen);
    for (int i = 0; i < resources->count_connectors && !crtcIndex; ++i) {
        auto* connector = drmModeGetConnector(fd, resources->connectors[i]);
        if (!connector)
            continue;

        if (connector->connection != DRM_MODE_CONNECTED || !connector->encoder_id || !connector->count_modes) {
            drmModeFreeConnector(connector);
            continue;
        }

        if (widthMM != connector->mmWidth || heightMM != connector->mmHeight) {
            drmModeFreeConnector(connector);
            continue;
        }

        // FIXME: if there are multiple connectors matching the size, check other properties.
        if (drmModeEncoder* encoder = drmModeGetEncoder(fd, connector->encoder_id)) {
            for (int i = 0; i < resources->count_crtcs; ++i) {
                if (resources->crtcs[i] == encoder->crtc_id) {
                    crtcIndex = i;
                    break;
                }
            }
            drmModeFreeEncoder(encoder);
        }
        drmModeFreeConnector(connector);
    }
    drmModeFreeResources(resources);

    return crtcIndex;
}

static void wpeScreenTryEnsureSyncObserver(WPEScreen* screen)
{
    drmDevicePtr devices[64];
    const int devicesNum = drmGetDevices2(0, devices, std::size(devices));
    if (devicesNum <= 0)
        return;

    for (int i = 0; i < devicesNum; i++) {
        if (!(devices[i]->available_nodes & (1 << DRM_NODE_PRIMARY)))
            continue;
        auto fd = UnixFileDescriptor { open(devices[i]->nodes[DRM_NODE_PRIMARY], O_RDWR | O_CLOEXEC), UnixFileDescriptor::Adopt };
        if (!fd)
            continue;

        if (auto crtcIndex = findCrtc(WPE_SCREEN(screen), fd.value())) {
            screen->priv->syncObserver = adoptGRef(wpeScreenSyncObserverDRMCreate(WTFMove(fd), *crtcIndex));
            if (screen->priv->syncObserver) {
                g_debug("WPEScreen %u: Created WPEScreenSyncObserverDRM for device %s with CRTC index %u", screen->priv->id, devices[i]->nodes[DRM_NODE_PRIMARY], *crtcIndex);
                break;
            }
        } else
            g_debug("WPEScreen %u: Failed to find a CRTC for device %s", screen->priv->id, devices[i]->nodes[DRM_NODE_PRIMARY]);
    }
    drmFreeDevices(devices, devicesNum);

    if (!screen->priv->syncObserver)
        g_debug("WPEScreen %u: Could not create a WPEScreenSyncObserverDRM", screen->priv->id);
}

static WPEScreenSyncObserver* wpeScreenGetSyncObserver(WPEScreen* screen)
{
    auto* priv = screen->priv;
    if (!priv->syncObserver)
        wpeScreenTryEnsureSyncObserver(screen);
    return priv->syncObserver.get();
}
#endif

static void wpe_screen_class_init(WPEScreenClass* screenClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(screenClass);
    objectClass->set_property = wpeScreenSetProperty;
    objectClass->get_property = wpeScreenGetProperty;

    /**
     * WPEScreen:id:
     *
     * The identifier of the screen.
     */
    sObjProperties[PROP_ID] =
        g_param_spec_uint(
            "id",
            nullptr, nullptr,
            0, G_MAXUINT, 0,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WPEScreen:x:
     *
     * The x coordinate of the screen position in logical coordinates.
     */
    sObjProperties[PROP_X] =
        g_param_spec_int(
            "x",
            nullptr, nullptr,
            -1, G_MAXINT, -1,
            WEBKIT_PARAM_READWRITE);

    /**
     * WPEScreen:y:
     *
     * The y coordinate of the screen position in logical coordinates.
     */
    sObjProperties[PROP_Y] =
        g_param_spec_int(
            "y",
            nullptr, nullptr,
            -1, G_MAXINT, -1,
            WEBKIT_PARAM_READWRITE);

    /**
     * WPEScreen:width:
     *
     * The width of the screen in logical coordinates.
     */
    sObjProperties[PROP_WIDTH] =
        g_param_spec_int(
            "width",
            nullptr, nullptr,
            -1, G_MAXINT, -1,
            WEBKIT_PARAM_READWRITE);

    /**
     * WPEScreen:height:
     *
     * The height of the screen in logical coordinates.
     */
    sObjProperties[PROP_HEIGHT] =
        g_param_spec_int(
            "height",
            nullptr, nullptr,
            -1, G_MAXINT, -1,
            WEBKIT_PARAM_READWRITE);

    /**
     * WPEScreen:physical-width:
     *
     * The physical width of the screen in millimeters.
     */
    sObjProperties[PROP_PHYSICAL_WIDTH] =
        g_param_spec_int(
            "physical-width",
            nullptr, nullptr,
            -1, G_MAXINT, -1,
            WEBKIT_PARAM_READWRITE);

    /**
     * WPEScreen:physical-height:
     *
     * The physical height of the screen in millimeters.
     */
    sObjProperties[PROP_PHYSICAL_HEIGHT] =
        g_param_spec_int(
            "physical-height",
            nullptr, nullptr,
            -1, G_MAXINT, -1,
            WEBKIT_PARAM_READWRITE);

    /**
     * WPEScreen:scale:
     *
     * The scale factor for the screen.
     *
     * Scaling factor applied to views displayed on this screen. Those views
     * will have the value forwarded to their [property@View:scale] property.
     *
     * The accepted values are in the `0.05` to `20.0` range, meaning that
     * rendering would be done up to 20 times smaller or bigger.
     */
    sObjProperties[PROP_SCALE] =
        g_param_spec_double(
            "scale",
            nullptr, nullptr,
            0.05, 20., 1.,
            WEBKIT_PARAM_READWRITE);

    /**
     * WPEScreen:refresh-rate:
     *
     * The refresh rate of the screen in milli-Hertz.
     */
    sObjProperties[PROP_REFRESH_RATE] =
        g_param_spec_int(
            "refresh-rate",
            nullptr, nullptr,
            -1, G_MAXINT, -1,
            WEBKIT_PARAM_READWRITE);

    g_object_class_install_properties(objectClass, N_PROPERTIES, sObjProperties.data());

    screenClass->invalidate = wpeScreenInvalidate;
#if USE(LIBDRM)
    screenClass->get_sync_observer = wpeScreenGetSyncObserver;
#endif
}

/**
 * wpe_screen_get_id:
 * @screen: a #WPEScreen
 *
 * Get the @screen identifier.
 * The idenifier is a non-zero value to uniquely identify a #WPEScreen.
 *
 * Returns: the screen identifier
 */
guint32 wpe_screen_get_id(WPEScreen* screen)
{
    g_return_val_if_fail(WPE_IS_SCREEN(screen), 0);

    return screen->priv->id;
}

/**
 * wpe_screen_invalidate:
 * @screen: a #WPEScreen
 *
 * Invalidate @screen. This will release all the platform resources
 * associated with @screen. The properties cached will not be
 * modified so they are still available after invalidation.
 */
void wpe_screen_invalidate(WPEScreen* screen)
{
    g_return_if_fail(WPE_IS_SCREEN(screen));

    WPE_SCREEN_GET_CLASS(screen)->invalidate(screen);
}

/**
 * wpe_screen_get_x:
 * @screen: a #WPEScreen
 *
 * Get the x coordinate of the @screen position in logical coordinates.
 *
 * Returns: the x coordinate, or -1 if not available
 */
int wpe_screen_get_x(WPEScreen* screen)
{
    g_return_val_if_fail(WPE_IS_SCREEN(screen), -1);

    return screen->priv->x;
}

/**
 * wpe_screen_get_y:
 * @screen: a #WPEScreen
 *
 * Get the y coordinate of the @screen position in logical coordinates.
 *
 * Returns: the y coordinate, or -1 if not available
 */
int wpe_screen_get_y(WPEScreen* screen)
{
    g_return_val_if_fail(WPE_IS_SCREEN(screen), -1);

    return screen->priv->y;
}

/**
 * wpe_screen_set_position:
 * @screen: a #WPEScreen
 * @x: the x coordinate, or -1
 * @y: the y coordinate, or -1
 *
 * Set the position of @screen in logical coordinates.
 */
void wpe_screen_set_position(WPEScreen* screen, int x, int y)
{
    g_return_if_fail(WPE_IS_SCREEN(screen));
    g_return_if_fail(x == -1 || x >= 0);
    g_return_if_fail(y == -1 || y >= 0);

    if (x != -1 && x != screen->priv->x) {
        screen->priv->x = x;
        g_object_notify_by_pspec(G_OBJECT(screen), sObjProperties[PROP_X]);
    }

    if (y != -1 && y != screen->priv->y) {
        screen->priv->y = y;
        g_object_notify_by_pspec(G_OBJECT(screen), sObjProperties[PROP_Y]);
    }
}

/**
 * wpe_screen_get_width:
 * @screen: a #WPEScreen
 *
 * Get the width of @screen in logical coordinates.
 *
 * Returns: the width of @screen, or -1 if not available
 */
int wpe_screen_get_width(WPEScreen* screen)
{
    g_return_val_if_fail(WPE_IS_SCREEN(screen), -1);

    return screen->priv->width;
}

/**
 * wpe_screen_get_height:
 * @screen: a #WPEScreen
 *
 * Get the height of @screen in logical coordinates.
 *
 * Returns: the height of @screen, or -1 if not available
 */
int wpe_screen_get_height(WPEScreen* screen)
{
    g_return_val_if_fail(WPE_IS_SCREEN(screen), -1);

    return screen->priv->height;
}

/**
 * wpe_screen_set_size:
 * @screen: a #WPEScreen
 * @width: the width, or -1
 * @height: the height, or -1
 *
 * Set the size of @screen in logical coordinates.
 */
void wpe_screen_set_size(WPEScreen* screen, int width, int height)
{
    g_return_if_fail(WPE_IS_SCREEN(screen));
    g_return_if_fail(width == -1 || width >= 0);
    g_return_if_fail(height == -1 || height >= 0);

    g_object_freeze_notify(G_OBJECT(screen));
    if (width != -1 && width != screen->priv->width) {
        screen->priv->width = width;
        g_object_notify_by_pspec(G_OBJECT(screen), sObjProperties[PROP_WIDTH]);
    }

    if (height != -1 && height != screen->priv->height) {
        screen->priv->height = height;
        g_object_notify_by_pspec(G_OBJECT(screen), sObjProperties[PROP_HEIGHT]);
    }
    g_object_thaw_notify(G_OBJECT(screen));
}

/**
 * wpe_screen_get_physical_width:
 * @screen: a #WPEScreen
 *
 * Get the physical width of @screen in millimeters.
 *
 * Returns: the physical width of @screen, or -1 if not available
 */
int wpe_screen_get_physical_width(WPEScreen* screen)
{
    g_return_val_if_fail(WPE_IS_SCREEN(screen), -1);

    return screen->priv->physicalWidth;
}

/**
 * wpe_screen_get_physical_height:
 * @screen: a #WPEScreen
 *
 * Get the physical height of @screen in millimeters.
 *
 * Returns: the physical height of @screen, or -1 if not available
 */
int wpe_screen_get_physical_height(WPEScreen* screen)
{
    g_return_val_if_fail(WPE_IS_SCREEN(screen), -1);

    return screen->priv->physicalHeight;
}

/**
 * wpe_screen_set_physical_size:
 * @screen: a #WPEScreen
 * @width: the physical width, or -1
 * @height: the physical height, or -1
 *
 * Set the physical size of @screen in millimeters.
 */
void wpe_screen_set_physical_size(WPEScreen* screen, int width, int height)
{
    g_return_if_fail(WPE_IS_SCREEN(screen));
    g_return_if_fail(width == -1 || width >= 0);
    g_return_if_fail(height == -1 || height >= 0);

    if (width != -1 && width != screen->priv->physicalWidth) {
        screen->priv->physicalWidth = width;
        g_object_notify_by_pspec(G_OBJECT(screen), sObjProperties[PROP_PHYSICAL_WIDTH]);
    }

    if (height != -1 && height != screen->priv->physicalHeight) {
        screen->priv->physicalHeight = height;
        g_object_notify_by_pspec(G_OBJECT(screen), sObjProperties[PROP_PHYSICAL_HEIGHT]);
    }
}

/**
 * wpe_screen_get_scale:
 * @screen: a #WPEScreen
 *
 * Get the @screen scale factor
 *
 * Returns: the screen scale factor
 */
gdouble wpe_screen_get_scale(WPEScreen* screen)
{
    g_return_val_if_fail(WPE_IS_SCREEN(screen), 1);

    return screen->priv->scale;
}

/**
 * wpe_screen_set_scale:
 * @screen: a #WPEScreen
 * @scale: the scale factor to set
 *
 * Set the @screen scale factor
 */
void wpe_screen_set_scale(WPEScreen* screen, double scale)
{
    g_return_if_fail(WPE_IS_SCREEN(screen));
    g_return_if_fail(scale >= 0.05);
    g_return_if_fail(scale <= 20.0);

    if (screen->priv->scale == scale)
        return;

    screen->priv->scale = scale;
    g_object_notify_by_pspec(G_OBJECT(screen), sObjProperties[PROP_SCALE]);
}

/**
 * wpe_screen_get_refresh_rate:
 * @screen: a #WPEScreen
 *
 * Get the refresh rate of @screen in milli-Hertz.
 *
 * Returns: the refresh rate, or -1 if not available
 */
int wpe_screen_get_refresh_rate(WPEScreen* screen)
{
    g_return_val_if_fail(WPE_IS_SCREEN(screen), -1);

    return screen->priv->refreshRate;
}

/**
 * wpe_screen_set_refresh_rate:
 * @screen: a #WPEScreen
 * @refresh_rate: the refresh rate
 *
 * Set the refresh rate of @screen in milli-Hertz.
 *
 */
void wpe_screen_set_refresh_rate(WPEScreen* screen, int refreshRate)
{
    g_return_if_fail(WPE_IS_SCREEN(screen));
    g_return_if_fail(refreshRate == -1 || refreshRate >= 0);

    if (refreshRate == -1 || refreshRate == screen->priv->refreshRate)
        return;

    screen->priv->refreshRate = refreshRate;
    g_object_notify_by_pspec(G_OBJECT(screen), sObjProperties[PROP_REFRESH_RATE]);
}

/**
 * wpe_screen_get_sync_observer:
 * @screen: a #WPEScreen
 *
 * Get the #WPEScreenSyncObserver of @screen. If screen sync is not supported, %NULL is returned.
 *
 * Returns: (transfer none) (nullable): a #WPEScreenSyncObserver or %NULL
 */
WPEScreenSyncObserver* wpe_screen_get_sync_observer(WPEScreen* screen)
{
    g_return_val_if_fail(WPE_IS_SCREEN(screen), nullptr);

    auto* screenClass = WPE_SCREEN_GET_CLASS(screen);
    if (screenClass->get_sync_observer)
        return screenClass->get_sync_observer(screen);

    return nullptr;
}

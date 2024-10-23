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

static GParamSpec* sObjProperties[N_PROPERTIES] = { nullptr, };

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
     */
    sObjProperties[PROP_SCALE] =
        g_param_spec_double(
            "scale",
            nullptr, nullptr,
            0., G_MAXDOUBLE, 1.,
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

    g_object_class_install_properties(objectClass, N_PROPERTIES, sObjProperties);
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

    auto* screenClass = WPE_SCREEN_GET_CLASS(screen);
    if (screenClass->invalidate)
        screenClass->invalidate(screen);
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
    g_return_if_fail(scale > 0);

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

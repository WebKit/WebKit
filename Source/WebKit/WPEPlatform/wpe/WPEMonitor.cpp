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
#include "WPEMonitor.h"

#include <wtf/glib/WTFGType.h>

/**
 * WPEMonitor:
 *
 */
struct _WPEMonitorPrivate {
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

WEBKIT_DEFINE_ABSTRACT_TYPE(WPEMonitor, wpe_monitor, G_TYPE_OBJECT)

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

static void wpeMonitorSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    auto* monitor = WPE_MONITOR(object);

    switch (propId) {
    case PROP_ID:
        monitor->priv->id = g_value_get_uint(value);
        break;
    case PROP_X:
        wpe_monitor_set_position(monitor, g_value_get_int(value), -1);
        break;
    case PROP_Y:
        wpe_monitor_set_position(monitor, -1, g_value_get_int(value));
        break;
    case PROP_WIDTH:
        wpe_monitor_set_size(monitor, g_value_get_int(value), -1);
        break;
    case PROP_HEIGHT:
        wpe_monitor_set_size(monitor, -1, g_value_get_int(value));
        break;
    case PROP_PHYSICAL_WIDTH:
        wpe_monitor_set_physical_size(monitor, g_value_get_int(value), -1);
        break;
    case PROP_PHYSICAL_HEIGHT:
        wpe_monitor_set_physical_size(monitor, -1, g_value_get_int(value));
        break;
    case PROP_SCALE:
        wpe_monitor_set_scale(monitor, g_value_get_double(value));
        break;
    case PROP_REFRESH_RATE:
        wpe_monitor_set_refresh_rate(monitor, g_value_get_int(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void wpeMonitorGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    auto* monitor = WPE_MONITOR(object);

    switch (propId) {
    case PROP_ID:
        g_value_set_uint(value, wpe_monitor_get_id(monitor));
        break;
    case PROP_X:
        g_value_set_int(value, wpe_monitor_get_x(monitor));
        break;
    case PROP_Y:
        g_value_set_int(value, wpe_monitor_get_y(monitor));
        break;
    case PROP_WIDTH:
        g_value_set_int(value, wpe_monitor_get_width(monitor));
        break;
    case PROP_HEIGHT:
        g_value_set_int(value, wpe_monitor_get_height(monitor));
        break;
    case PROP_PHYSICAL_WIDTH:
        g_value_set_int(value, wpe_monitor_get_physical_width(monitor));
        break;
    case PROP_PHYSICAL_HEIGHT:
        g_value_set_int(value, wpe_monitor_get_physical_height(monitor));
        break;
    case PROP_SCALE:
        g_value_set_double(value, wpe_monitor_get_scale(monitor));
        break;
    case PROP_REFRESH_RATE:
        g_value_set_int(value, wpe_monitor_get_refresh_rate(monitor));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void wpe_monitor_class_init(WPEMonitorClass* monitorClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(monitorClass);
    objectClass->set_property = wpeMonitorSetProperty;
    objectClass->get_property = wpeMonitorGetProperty;

    /**
     * WPEMonitor:id:
     *
     * The identifier of the monitor.
     */
    sObjProperties[PROP_ID] =
        g_param_spec_uint(
            "id",
            nullptr, nullptr,
            0, G_MAXUINT, 0,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WPEMonitor:x:
     *
     * The x coordinate of the monitor position in pixels.
     * Note this is not device pixels, so not affected by #WPEMonitor:scale.
     */
    sObjProperties[PROP_X] =
        g_param_spec_int(
            "x",
            nullptr, nullptr,
            -1, G_MAXINT, -1,
            WEBKIT_PARAM_READWRITE);

    /**
     * WPEMonitor:y:
     *
     * The y coordinate of the monitor position in pixels.
     * Note this is not device pixels, so not affected by #WPEMonitor:scale.
     */
    sObjProperties[PROP_Y] =
        g_param_spec_int(
            "y",
            nullptr, nullptr,
            -1, G_MAXINT, -1,
            WEBKIT_PARAM_READWRITE);

    /**
     * WPEMonitor:width:
     *
     * The width of the monitor in pixels.
     * Note this is not device pixels, so not affected by #WPEMonitor:scale.
     */
    sObjProperties[PROP_WIDTH] =
        g_param_spec_int(
            "width",
            nullptr, nullptr,
            -1, G_MAXINT, -1,
            WEBKIT_PARAM_READWRITE);

    /**
     * WPEMonitor:height:
     *
     * The height of the monitor in pixels.
     * Note this is not device pixels, so not affected by #WPEMonitor:scale.
     */
    sObjProperties[PROP_HEIGHT] =
        g_param_spec_int(
            "height",
            nullptr, nullptr,
            -1, G_MAXINT, -1,
            WEBKIT_PARAM_READWRITE);

    /**
     * WPEMonitor:physical-width:
     *
     * The physical width of the monitor in millimeters.
     */
    sObjProperties[PROP_PHYSICAL_WIDTH] =
        g_param_spec_int(
            "physical-width",
            nullptr, nullptr,
            -1, G_MAXINT, -1,
            WEBKIT_PARAM_READWRITE);

    /**
     * WPEMonitor:physical-height:
     *
     * The physical height of the monitor in millimeters.
     */
    sObjProperties[PROP_PHYSICAL_HEIGHT] =
        g_param_spec_int(
            "physical-height",
            nullptr, nullptr,
            -1, G_MAXINT, -1,
            WEBKIT_PARAM_READWRITE);

    /**
     * WPEMonitor:scale:
     *
     * The scale factor for the monitor.
     */
    sObjProperties[PROP_SCALE] =
        g_param_spec_double(
            "scale",
            nullptr, nullptr,
            1, G_MAXDOUBLE, 1,
            WEBKIT_PARAM_READWRITE);

    /**
     * WPEMonitor:refresh-rate:
     *
     * The refresh rate of the monitor in milli-Hertz.
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
 * wpe_monitor_get_id:
 * @monitor: a #WPEMonitor
 *
 * Get the @monitor identifier.
 * The idenifier is a non-zero value to uniquely identify a #WPEMonitor.
 *
 * Returns: the monitor identifier
 */
guint32 wpe_monitor_get_id(WPEMonitor* monitor)
{
    g_return_val_if_fail(WPE_IS_MONITOR(monitor), 0);

    return monitor->priv->id;
}

/**
 * wpe_monitor_invalidate:
 * @monitor: a #WPEMonitor
 *
 * Invalidate @monitor. This will release all the platform resources
 * associated with @monitor. The properties cached will not be
 * modified so they are still available after invalidation.
 */
void wpe_monitor_invalidate(WPEMonitor* monitor)
{
    g_return_if_fail(WPE_IS_MONITOR(monitor));

    auto* monitorClass = WPE_MONITOR_GET_CLASS(monitor);
    if (monitorClass->invalidate)
        monitorClass->invalidate(monitor);
}

/**
 * wpe_monitor_get_x:
 * @monitor: a #WPEMonitor
 *
 * Get the x coordinate of the @monitor position.
 *
 * Returns: the x coordinate, or -1 if not available
 */
int wpe_monitor_get_x(WPEMonitor* monitor)
{
    g_return_val_if_fail(WPE_IS_MONITOR(monitor), -1);

    return monitor->priv->x;
}

/**
 * wpe_monitor_get_y:
 * @monitor: a #WPEMonitor
 *
 * Get the y coordinate of the @monitor position.
 *
 * Returns: the y coordinate, or -1 if not available
 */
int wpe_monitor_get_y(WPEMonitor* monitor)
{
    g_return_val_if_fail(WPE_IS_MONITOR(monitor), -1);

    return monitor->priv->y;
}

/**
 * wpe_monitor_set_position:
 * @monitor: a #WPEMonitor
 * @x: the x coordinate, or -1
 * @y: the y coordinate, or -1
 *
 * Set the position of @monitor
 */
void wpe_monitor_set_position(WPEMonitor* monitor, int x, int y)
{
    g_return_if_fail(WPE_IS_MONITOR(monitor));
    g_return_if_fail(x == -1 || x >= 0);
    g_return_if_fail(y == -1 || y >= 0);

    if (x != -1 && x != monitor->priv->x) {
        monitor->priv->x = x;
        g_object_notify_by_pspec(G_OBJECT(monitor), sObjProperties[PROP_X]);
    }

    if (y != -1 && y != monitor->priv->y) {
        monitor->priv->y = y;
        g_object_notify_by_pspec(G_OBJECT(monitor), sObjProperties[PROP_Y]);
    }
}

/**
 * wpe_monitor_get_width:
 * @monitor: a #WPEMonitor
 *
 * Get the width of @monitor.
 *
 * Returns: the width of @monitor, or -1 if not available
 */
int wpe_monitor_get_width(WPEMonitor* monitor)
{
    g_return_val_if_fail(WPE_IS_MONITOR(monitor), -1);

    return monitor->priv->width;
}

/**
 * wpe_monitor_get_height:
 * @monitor: a #WPEMonitor
 *
 * Get the height of @monitor.
 *
 * Returns: the height of @monitor, or -1 if not available
 */
int wpe_monitor_get_height(WPEMonitor* monitor)
{
    g_return_val_if_fail(WPE_IS_MONITOR(monitor), -1);

    return monitor->priv->height;
}

/**
 * wpe_monitor_set_size:
 * @monitor: a #WPEMonitor
 * @width: the width, or -1
 * @height: the height, o -1
 *
 * Set the size of @monitor.
 */
void wpe_monitor_set_size(WPEMonitor* monitor, int width, int height)
{
    g_return_if_fail(WPE_IS_MONITOR(monitor));
    g_return_if_fail(width == -1 || width >= 0);
    g_return_if_fail(height == -1 || height >= 0);

    if (width != -1 && width != monitor->priv->width) {
        monitor->priv->width = width;
        g_object_notify_by_pspec(G_OBJECT(monitor), sObjProperties[PROP_WIDTH]);
    }

    if (height != -1 && height != monitor->priv->height) {
        monitor->priv->height = height;
        g_object_notify_by_pspec(G_OBJECT(monitor), sObjProperties[PROP_HEIGHT]);
    }
}

/**
 * wpe_monitor_get_physical_width:
 * @monitor: a #WPEMonitor
 *
 * Get the physical width of @monitor in millimeters.
 *
 * Returns: the physical width of @monitor, or -1 if not available
 */
int wpe_monitor_get_physical_width(WPEMonitor* monitor)
{
    g_return_val_if_fail(WPE_IS_MONITOR(monitor), -1);

    return monitor->priv->physicalWidth;
}

/**
 * wpe_monitor_get_physical_height:
 * @monitor: a #WPEMonitor
 *
 * Get the physical height of @monitor in millimeters.
 *
 * Returns: the physical height of @monitor, or -1 if not available
 */
int wpe_monitor_get_physical_height(WPEMonitor* monitor)
{
    g_return_val_if_fail(WPE_IS_MONITOR(monitor), -1);

    return monitor->priv->physicalHeight;
}

/**
 * wpe_monitor_set_physical_size:
 * @monitor: a #WPEMonitor
 * @width: the physical width, or -1
 * @height: the physical height, or -1
 *
 * Set the physical size of @monitor in millimeters.
 */
void wpe_monitor_set_physical_size(WPEMonitor* monitor, int width, int height)
{
    g_return_if_fail(WPE_IS_MONITOR(monitor));
    g_return_if_fail(width == -1 || width >= 0);
    g_return_if_fail(height == -1 || height >= 0);

    if (width != -1 && width != monitor->priv->physicalWidth) {
        monitor->priv->physicalWidth = width;
        g_object_notify_by_pspec(G_OBJECT(monitor), sObjProperties[PROP_PHYSICAL_WIDTH]);
    }

    if (height != -1 && height != monitor->priv->physicalHeight) {
        monitor->priv->physicalHeight = height;
        g_object_notify_by_pspec(G_OBJECT(monitor), sObjProperties[PROP_PHYSICAL_HEIGHT]);
    }
}

/**
 * wpe_monitor_get_scale:
 * @monitor: a #WPEMonitor
 *
 * Get the @monitor scale factor
 *
 * Returns: the monitor scale factor
 */
gdouble wpe_monitor_get_scale(WPEMonitor* monitor)
{
    g_return_val_if_fail(WPE_IS_MONITOR(monitor), 1);

    return monitor->priv->scale;
}

/**
 * wpe_monitor_set_scale:
 * @monitor: a #WPEMonitor
 * @scale: the scale factor to set
 *
 * Set the @monitor scale factor
 */
void wpe_monitor_set_scale(WPEMonitor* monitor, double scale)
{
    g_return_if_fail(WPE_IS_MONITOR(monitor));
    g_return_if_fail(scale >= 1);

    if (monitor->priv->scale == scale)
        return;

    monitor->priv->scale = scale;
    g_object_notify_by_pspec(G_OBJECT(monitor), sObjProperties[PROP_SCALE]);
}

/**
 * wpe_monitor_get_refresh_rate:
 * @monitor: a #WPEMonitor
 *
 * Get the refresh rate of @monitor in milli-Hertz.
 *
 * Returns: the refresh rate, or -1 if not available
 */
int wpe_monitor_get_refresh_rate(WPEMonitor* monitor)
{
    g_return_val_if_fail(WPE_IS_MONITOR(monitor), -1);

    return monitor->priv->refreshRate;
}

/**
 * wpe_monitor_set_refresh_rate:
 * @monitor: a #WPEMonitor
 * @refresh_rate: the refresh rate
 *
 * Set the refresh rate of @monitor in milli-Hertz.
 *
 */
void wpe_monitor_set_refresh_rate(WPEMonitor* monitor, int refreshRate)
{
    g_return_if_fail(WPE_IS_MONITOR(monitor));
    g_return_if_fail(refreshRate == -1 || refreshRate >= 0);

    if (refreshRate == -1 || refreshRate == monitor->priv->refreshRate)
        return;

    monitor->priv->refreshRate = refreshRate;
    g_object_notify_by_pspec(G_OBJECT(monitor), sObjProperties[PROP_REFRESH_RATE]);
}

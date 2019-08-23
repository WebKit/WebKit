/*
 * Copyright (C) 2019 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitGeolocationManager.h"

#include "APIGeolocationProvider.h"
#include "GeoclueGeolocationProvider.h"
#include "WebGeolocationPosition.h"
#include "WebKitGeolocationManagerPrivate.h"
#include <glib/gi18n-lib.h>
#include <wtf/WallTime.h>
#include <wtf/glib/WTFGType.h>

using namespace WebKit;
using namespace WebCore;

/**
 * SECTION:WebKitGeolocationManager
 * @short_description: WebKitGeolocationManager
 * @title: Geolocation manager
 * @see_also: #WebKitGeolocationPermissionRequest, #WebKitWebContext
 *
 * WebKitGeolocationManager provides API to get the geographical position of the user.
 * Once a #WebKitGeolocationPermissionRequest is allowed, when WebKit needs to know the
 * user location #WebKitGeolocationManager::start signal is emitted. If the signal is handled
 * and returns %TRUE, the application is responsible for providing the position every time it's
 * updated by calling webkit_geolocation_manager_update_position(). The signal #WebKitGeolocationManager::stop
 * will be emitted when location updates are no longer needed.
 *
 * Since: 2.26
 */

enum {
    PROP_0,

    PROP_ENABLE_HIGH_ACCURACY
};

enum {
    START,
    STOP,
    LAST_SIGNAL
};

struct _WebKitGeolocationPosition {
    _WebKitGeolocationPosition() = default;

    _WebKitGeolocationPosition(double latitude, double longitude, double accuracy)
    {
        position.timestamp = WallTime::now().secondsSinceEpoch().value();
        position.latitude = latitude;
        position.longitude = longitude;
        position.accuracy = accuracy;
    }

    explicit _WebKitGeolocationPosition(GeolocationPositionData&& corePosition)
        : position(WTFMove(corePosition))
    {
    }

    explicit _WebKitGeolocationPosition(const GeolocationPositionData& other)
    {
        position = other;
    }

    GeolocationPositionData position;
};

/**
 * WebKitGeolocationPosition:
 *
 * WebKitGeolocationPosition is an opaque struct used to provide position updates to a
 * #WebKitGeolocationManager using webkit_geolocation_manager_update_position().
 *
 * Since: 2.26
 */

G_DEFINE_BOXED_TYPE(WebKitGeolocationPosition, webkit_geolocation_position, webkit_geolocation_position_copy, webkit_geolocation_position_free)

/**
 * webkit_geolocation_position_new:
 * @latitude: a valid latitude in degrees
 * @longitude: a valid longitude in degrees
 * @accuracy: accuracy of location in meters
 *
 * Create a new #WebKitGeolocationPosition
 *
 * Returns: (transfer full): a newly created #WebKitGeolocationPosition
 *
 * Since: 2.26
 */
WebKitGeolocationPosition* webkit_geolocation_position_new(double latitude, double longitude, double accuracy)
{
    auto* position = static_cast<WebKitGeolocationPosition*>(fastMalloc(sizeof(WebKitGeolocationPosition)));
    new (position) WebKitGeolocationPosition(latitude, longitude, accuracy);
    return position;
}

/**
 * webkit_geolocation_position_copy:
 * @position: a #WebKitGeolocationPosition
 *
 * Make a copy of the #WebKitGeolocationPosition
 *
 * Returns: (transfer full): a copy of @position
 *
 * Since: 2.26
 */
WebKitGeolocationPosition* webkit_geolocation_position_copy(WebKitGeolocationPosition* position)
{
    g_return_val_if_fail(position, nullptr);

    auto* copy = static_cast<WebKitGeolocationPosition*>(fastMalloc(sizeof(WebKitGeolocationPosition)));
    new (copy) WebKitGeolocationPosition(position->position);
    return copy;
}

/**
 * webkit_geolocation_position_free:
 * @position: a #WebKitGeolocationPosition
 *
 * Free the #WebKitGeolocationPosition
 *
 * Since: 2.26
 */
void webkit_geolocation_position_free(WebKitGeolocationPosition* position)
{
    g_return_if_fail(position);

    position->~WebKitGeolocationPosition();
    fastFree(position);
}

/**
 * webkit_geolocation_position_set_timestamp:
 * @position: a #WebKitGeolocationPosition
 * @timestamp: timestamp in seconds since the epoch, or 0 to use current time
 *
 * Set the @position timestamp. By default it's the time when the @position was created.
 *
 * Since: 2.26
 */
void webkit_geolocation_position_set_timestamp(WebKitGeolocationPosition* position, guint64 timestamp)
{
    g_return_if_fail(position);

    position->position.timestamp = timestamp ? static_cast<double>(timestamp) : WallTime::now().secondsSinceEpoch().value();
}

/**
 * webkit_geolocation_position_set_altitude:
 * @position: a #WebKitGeolocationPosition
 * @altitude: altitude in meters
 *
 * Set the @position altitude
 *
 * Since: 2.26
 */
void webkit_geolocation_position_set_altitude(WebKitGeolocationPosition* position, double altitude)
{
    g_return_if_fail(position);

    position->position.altitude = altitude;
}

/**
 * webkit_geolocation_position_set_altitude_accuracy:
 * @position: a #WebKitGeolocationPosition
 * @altitude_accuracy: accuracy of position altitude in meters
 *
 * Set the accuracy of @position altitude
 *
 * Since: 2.26
 */
void webkit_geolocation_position_set_altitude_accuracy(WebKitGeolocationPosition* position, double altitudeAccuracy)
{
    g_return_if_fail(position);

    position->position.altitudeAccuracy = altitudeAccuracy;
}

/**
 * webkit_geolocation_position_set_heading:
 * @position: a #WebKitGeolocationPosition
 * @heading: heading in degrees
 *
 * Set the @position heading, as a positive angle between the direction of movement and the North
 * direction, in clockwise direction.
 *
 * Since: 2.26
 */
void webkit_geolocation_position_set_heading(WebKitGeolocationPosition* position, double heading)
{
    g_return_if_fail(position);

    position->position.heading = heading;
}

/**
 * webkit_geolocation_position_set_speed:
 * @position: a #WebKitGeolocationPosition
 * @speed: speed in meters per second
 *
 * Set the @position speed
 *
 * Since: 2.26
 */
void webkit_geolocation_position_set_speed(WebKitGeolocationPosition* position, double speed)
{
    g_return_if_fail(position);

    position->position.speed = speed;
}

struct _WebKitGeolocationManagerPrivate {
    RefPtr<WebGeolocationManagerProxy> manager;
    bool highAccuracyEnabled;
    std::unique_ptr<GeoclueGeolocationProvider> geoclueProvider;
};

static guint signals[LAST_SIGNAL] = { 0, };

WEBKIT_DEFINE_TYPE(WebKitGeolocationManager, webkit_geolocation_manager, G_TYPE_OBJECT)

static void webkitGeolocationManagerStart(WebKitGeolocationManager* manager)
{
    gboolean returnValue;
    g_signal_emit(manager, signals[START], 0, &returnValue);
    if (returnValue) {
        manager->priv->geoclueProvider = nullptr;
        return;
    }

    if (!manager->priv->geoclueProvider) {
        manager->priv->geoclueProvider = makeUnique<GeoclueGeolocationProvider>();
        manager->priv->geoclueProvider->setEnableHighAccuracy(manager->priv->highAccuracyEnabled);
    }
    manager->priv->geoclueProvider->start([manager](GeolocationPositionData&& corePosition, Optional<CString> error) {
        if (error) {
            webkit_geolocation_manager_failed(manager, error->data());
            return;
        }

        WebKitGeolocationPosition position(WTFMove(corePosition));
        webkit_geolocation_manager_update_position(manager, &position);
    });
}

static void webkitGeolocationManagerStop(WebKitGeolocationManager* manager)
{
    g_signal_emit(manager, signals[STOP], 0, nullptr);

    if (manager->priv->geoclueProvider)
        manager->priv->geoclueProvider->stop();
}

static void webkitGeolocationManagerSetEnableHighAccuracy(WebKitGeolocationManager* manager, bool enabled)
{
    if (manager->priv->highAccuracyEnabled == enabled)
        return;

    manager->priv->highAccuracyEnabled = enabled;
    g_object_notify(G_OBJECT(manager), "enable-high-accuracy");
    if (manager->priv->geoclueProvider)
        manager->priv->geoclueProvider->setEnableHighAccuracy(enabled);
}

class GeolocationProvider final : public API::GeolocationProvider {
public:
    explicit GeolocationProvider(WebKitGeolocationManager* manager)
        : m_manager(manager)
    {
    }

private:
    void startUpdating(WebGeolocationManagerProxy&) override
    {
        webkitGeolocationManagerStart(m_manager);
    }

    void stopUpdating(WebGeolocationManagerProxy&) override
    {
        webkitGeolocationManagerStop(m_manager);
    }

    void setEnableHighAccuracy(WebGeolocationManagerProxy&, bool enabled) override
    {
        webkitGeolocationManagerSetEnableHighAccuracy(m_manager, enabled);
    }

    WebKitGeolocationManager* m_manager;
};

WebKitGeolocationManager* webkitGeolocationManagerCreate(WebGeolocationManagerProxy* proxy)
{
    auto* manager = WEBKIT_GEOLOCATION_MANAGER(g_object_new(WEBKIT_TYPE_GEOLOCATION_MANAGER, nullptr));
    manager->priv->manager = proxy;
    proxy->setProvider(makeUnique<GeolocationProvider>(manager));
    return manager;
}

static void webkitGeolocationManagerGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitGeolocationManager* manager = WEBKIT_GEOLOCATION_MANAGER(object);

    switch (propId) {
    case PROP_ENABLE_HIGH_ACCURACY:
        g_value_set_boolean(value, webkit_geolocation_manager_get_enable_high_accuracy(manager));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkit_geolocation_manager_class_init(WebKitGeolocationManagerClass* geolocationManagerClass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(geolocationManagerClass);
    gObjectClass->get_property = webkitGeolocationManagerGetProperty;

    /**
     * WebKitGeolocationManager:enable-high-accuracy:
     *
     * Whether high accuracy is enabled. This is a read-only property that will be
     * set to %TRUE when a #WebKitGeolocationManager needs to get accurate position updates.
     * You can connect to notify::enable-high-accuracy signal to monitor it.
     *
     * Since: 2.26
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_ENABLE_HIGH_ACCURACY,
        g_param_spec_boolean(
            "enable-high-accuracy",
            _("Enable high accuracy"),
            _("Whether high accuracy is enabled"),
            FALSE,
            WEBKIT_PARAM_READABLE));

    /**
     * WebKitGeolocationManager::start:
     * @manager: the #WebKitGeolocationManager on which the signal is emitted
     *
     * The signal is emitted to notify that @manager needs to start receiving
     * position updates. After this signal is emitted the user should provide
     * the updates using webkit_geolocation_manager_update_position() every time
     * the position changes, or use webkit_geolocation_manager_failed() in case
     * it isn't possible to determine the current position.
     *
     * If the signal is not handled, WebKit will try to determine the position
     * using GeoClue if available.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to propagate the event further.
     *
     * Since: 2.26
     */
    signals[START] = g_signal_new(
        "start",
        G_TYPE_FROM_CLASS(geolocationManagerClass),
        G_SIGNAL_RUN_LAST,
        0,
        g_signal_accumulator_true_handled, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN, 0);

    /**
     * WebKitGeolocationManager::stop:
     * @manager: the #WebKitGeolocationManager on which the signal is emitted
     *
     * The signal is emitted to notify that @manager doesn't need to receive
     * position updates anymore.
     *
     * Since: 2.26
     */
    signals[STOP] = g_signal_new(
        "stop",
        G_TYPE_FROM_CLASS(geolocationManagerClass),
        G_SIGNAL_RUN_LAST,
        0,
        nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 0);
}

/**
 * webkit_geolocation_manager_update_position:
 * @manager: a #WebKitGeolocationManager
 * @position: a #WebKitGeolocationPosition
 *
 * Notify @manager that position has been updated to @position.
 *
 * Since: 2.26
 */
void webkit_geolocation_manager_update_position(WebKitGeolocationManager* manager, WebKitGeolocationPosition* position)
{
    g_return_if_fail(WEBKIT_IS_GEOLOCATION_MANAGER(manager));
    g_return_if_fail(position);

    auto corePosition = position->position;
    auto wkPosition = WebGeolocationPosition::create(WTFMove(corePosition));
    manager->priv->manager->providerDidChangePosition(wkPosition.ptr());
}

/**
 * webkit_geolocation_manager_failed:
 * @manager: a #WebKitGeolocationManager
 * @error_message: the error message
 *
 * Notify @manager that determining the position failed.
 *
 * Since: 2.26
 */
void webkit_geolocation_manager_failed(WebKitGeolocationManager* manager, const char* errorMessage)
{
    g_return_if_fail(WEBKIT_IS_GEOLOCATION_MANAGER(manager));

    manager->priv->manager->providerDidFailToDeterminePosition(String::fromUTF8(errorMessage));
}

/**
 * webkit_geolocation_manager_get_enable_high_accuracy:
 * @manager: a #WebKitGeolocationManager
 *
 * Get whether high accuracy is enabled.
 *
 * Since: 2.26
 */
gboolean webkit_geolocation_manager_get_enable_high_accuracy(WebKitGeolocationManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_GEOLOCATION_MANAGER(manager), FALSE);

    return manager->priv->highAccuracyEnabled;
}

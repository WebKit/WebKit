/*
 * Copyright (C) 2008 Holger Hans Peter Freyther <zecke@selfish.org>
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
 *
 */

#include "config.h"
#include "GeolocationClientGtk.h"

#if ENABLE(GEOLOCATION)

#include "Chrome.h"
#include "ChromeClient.h"
#include "Geolocation.h"
#include "GeolocationController.h"
#include "GeolocationError.h"
#include "GeolocationPosition.h"
#include "webkitgeolocationpolicydecisionprivate.h"
#include "webkitwebframeprivate.h"
#include "webkitwebviewprivate.h"
#include <glib/gi18n-lib.h>

namespace WebKit {

void getPositionCallback(GeocluePosition* position, GeocluePositionFields fields, int timestamp, double latitude, double longitude, double altitude, GeoclueAccuracy* accuracy, GError* error, GeolocationClient* client)
{
    if (error) {
        client->errorOccured(error->message);
        g_error_free(error);
        return;
    }
    client->positionChanged(position, fields, timestamp, latitude, longitude, altitude, accuracy);
}

void positionChangedCallback(GeocluePosition* position, GeocluePositionFields fields, int timestamp, double latitude, double longitude, double altitude, GeoclueAccuracy* accuracy, GeolocationClient* client)
{
    client->positionChanged(position, fields, timestamp, latitude, longitude, altitude, accuracy);
}

GeolocationClient::GeolocationClient(WebKitWebView* webView)
    : m_webView(webView)
    , m_geoclueClient(0)
    , m_geocluePosition(0)
    , m_latitude(0)
    , m_longitude(0)
    , m_altitude(0)
    , m_accuracy(0)
    , m_altitudeAccuracy(0)
    , m_timestamp(0)
    , m_enableHighAccuracy(false)
    , m_isUpdating(false)
{
}

void GeolocationClient::geolocationDestroyed()
{
    delete this;
}

void GeolocationClient::startUpdating()
{
    ASSERT(!m_geoclueClient);

    GRefPtr<GeoclueMaster> master = adoptGRef(geoclue_master_get_default());
    GRefPtr<GeoclueMasterClient> client = adoptGRef(geoclue_master_create_client(master.get(), 0, 0));
    if (!client) {
        errorOccured(_("Could not connect to location provider."));
        return;
    }

    GOwnPtr<GError> error;
    GeoclueAccuracyLevel accuracyLevel = m_enableHighAccuracy ? GEOCLUE_ACCURACY_LEVEL_DETAILED : GEOCLUE_ACCURACY_LEVEL_LOCALITY;
    if (!geoclue_master_client_set_requirements(client.get(), accuracyLevel, 0,
                                                false, GEOCLUE_RESOURCE_ALL, &error.outPtr())) {
        errorOccured(error->message);
        return;
    }

    m_geocluePosition = adoptGRef(geoclue_master_client_create_position(client.get(), &error.outPtr()));
    if (!m_geocluePosition) {
        errorOccured(error->message);
        return;
    }

    m_geoclueClient = client;
    geoclue_position_get_position_async(m_geocluePosition.get(), reinterpret_cast<GeocluePositionCallback>(getPositionCallback), this);
    g_signal_connect(G_OBJECT(m_geocluePosition.get()), "position-changed",
                     G_CALLBACK(positionChangedCallback), this);

    m_isUpdating = true;
}

void GeolocationClient::stopUpdating()
{
    if (!m_geoclueClient)
        return;

    m_geocluePosition.clear();
    m_geoclueClient.clear();

    m_isUpdating = false;
}

void GeolocationClient::setEnableHighAccuracy(bool enable)
{
    m_enableHighAccuracy = enable;

    // If we're already updating we should report the new requirements in order
    // to change to a more suitable provider if needed. If not, return.
    if (!m_isUpdating)
        return;

    GOwnPtr<GError> error;
    GeoclueAccuracyLevel accuracyLevel = m_enableHighAccuracy ? GEOCLUE_ACCURACY_LEVEL_DETAILED : GEOCLUE_ACCURACY_LEVEL_LOCALITY;
    if (!geoclue_master_client_set_requirements(m_geoclueClient.get(), accuracyLevel, 0,
                                                false, GEOCLUE_RESOURCE_ALL, &error.outPtr())) {
        errorOccured(error->message);
        stopUpdating();
    }
}

WebCore::GeolocationPosition* GeolocationClient::lastPosition()
{
    return m_lastPosition.get();
}

void GeolocationClient::requestPermission(WebCore::Geolocation* geolocation)
{
    WebKitWebFrame* webFrame = kit(geolocation->frame());
    GRefPtr<WebKitGeolocationPolicyDecision> policyDecision(adoptGRef(webkit_geolocation_policy_decision_new(webFrame, geolocation)));

    gboolean isHandled = FALSE;
    g_signal_emit_by_name(m_webView, "geolocation-policy-decision-requested", webFrame, policyDecision.get(), &isHandled);
    if (!isHandled)
        webkit_geolocation_policy_deny(policyDecision.get());
}

void GeolocationClient::cancelPermissionRequest(WebCore::Geolocation* geolocation)
{
    g_signal_emit_by_name(m_webView, "geolocation-policy-decision-cancelled", kit(geolocation->frame()));
}

void GeolocationClient::positionChanged(GeocluePosition*, GeocluePositionFields fields, int timestamp, double latitude, double longitude, double altitude, GeoclueAccuracy* accuracy)
{
    if (!(fields & GEOCLUE_POSITION_FIELDS_LATITUDE && fields & GEOCLUE_POSITION_FIELDS_LONGITUDE)) {
        errorOccured(_("Position could not be determined."));
        return;
    }

    m_timestamp = timestamp;
    m_latitude = latitude;
    m_longitude = longitude;
    m_altitude = altitude;

    geoclue_accuracy_get_details(accuracy, 0, &m_accuracy, &m_altitudeAccuracy);

    updatePosition();
}

void GeolocationClient::updatePosition()
{
    m_lastPosition = WebCore::GeolocationPosition::create(static_cast<double>(m_timestamp), m_latitude, m_longitude, m_accuracy,
                                                          true, m_altitude, true, m_altitudeAccuracy, false, 0, false, 0);
    WebCore::GeolocationController::from(core(m_webView))->positionChanged(m_lastPosition.get());
}

void GeolocationClient::errorOccured(const char* message)
{
    RefPtr<WebCore::GeolocationError> error = WebCore::GeolocationError::create(WebCore::GeolocationError::PositionUnavailable, message);
    WebCore::GeolocationController::from(core(m_webView))->errorOccurred(error.get());
}

}

#endif // ENABLE(GEOLOCATION)

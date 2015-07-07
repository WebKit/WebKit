/*
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GeolocationProviderGeoclue.h"

#if ENABLE(GEOLOCATION) && USE(GEOCLUE2)

#include <wtf/gobject/GUniquePtr.h>
#include <wtf/gobject/GlibUtilities.h>
#include <wtf/text/CString.h>

const char* gGeoclueBusName = "org.freedesktop.GeoClue2";
const char* gGeoclueManagerPath = "/org/freedesktop/GeoClue2/Manager";

using namespace WebCore;

typedef enum {
    GeoclueAccuracyLevelCountry = 1,
    GeoclueAccuracyLevelCity = 4,
    GeoclueAccuracyLevelStreet = 6,
    GeoclueAccuracyLevelExact = 8,
} GeoclueAccuracyLevel;

GeolocationProviderGeoclue::GeolocationProviderGeoclue(GeolocationProviderGeoclueClient* client)
    : m_client(client)
    , m_latitude(0)
    , m_longitude(0)
    , m_altitude(0)
    , m_accuracy(0)
    , m_altitudeAccuracy(0)
    , m_timestamp(0)
    , m_enableHighAccuracy(false)
    , m_isUpdating(false)
{
    ASSERT(m_client);
}

GeolocationProviderGeoclue::~GeolocationProviderGeoclue()
{
    stopUpdating();
}

void GeolocationProviderGeoclue::startUpdating()
{
    m_isUpdating = true;

    if (!m_managerProxy) {
        geoclue_manager_proxy_new_for_bus(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, gGeoclueBusName, gGeoclueManagerPath, nullptr,
            reinterpret_cast<GAsyncReadyCallback>(createGeoclueManagerProxyCallback), this);
        return;
    }

    geoclue_manager_call_get_client(m_managerProxy.get(), nullptr, reinterpret_cast<GAsyncReadyCallback>(getGeoclueClientCallback), this);
}

void GeolocationProviderGeoclue::stopUpdating()
{
    if (m_clientProxy) {
        geoclue_client_call_stop(m_clientProxy.get(), nullptr, nullptr, nullptr);
        g_signal_handlers_disconnect_by_func(m_clientProxy.get(), reinterpret_cast<gpointer>(locationUpdatedCallback), this);
        m_clientProxy = nullptr;
    }
    m_isUpdating = false;
}

void GeolocationProviderGeoclue::setEnableHighAccuracy(bool enable)
{
    if (m_enableHighAccuracy == enable)
        return;

    m_enableHighAccuracy = enable;

    // If we're already updating we should report the new requirements in order
    // to change to a more suitable provider if needed. If not, return.
    if (!m_isUpdating)
        return;

    updateClientRequirements();
}

void GeolocationProviderGeoclue::createGeoclueManagerProxyCallback(GObject*, GAsyncResult* result, GeolocationProviderGeoclue* provider)
{
    GUniqueOutPtr<GError> error;
    provider->m_managerProxy = adoptGRef(geoclue_manager_proxy_new_for_bus_finish(result, &error.outPtr()));
    if (error) {
        provider->errorOccurred(error->message);
        return;
    }

    geoclue_manager_call_get_client(provider->m_managerProxy.get(), nullptr, reinterpret_cast<GAsyncReadyCallback>(getGeoclueClientCallback), provider);
}

void GeolocationProviderGeoclue::getGeoclueClientCallback(GObject* sourceObject, GAsyncResult* result, GeolocationProviderGeoclue* provider)
{
    GUniqueOutPtr<GError> error;
    GUniqueOutPtr<gchar> path;
    if (!geoclue_manager_call_get_client_finish(GEOCLUE_MANAGER(sourceObject), &path.outPtr(), result, &error.outPtr())) {
        provider->errorOccurred(error->message);
        return;
    }

    geoclue_client_proxy_new_for_bus(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, gGeoclueBusName, path.get(), nullptr,
        reinterpret_cast<GAsyncReadyCallback>(createGeoclueClientProxyCallback), provider);
}

void GeolocationProviderGeoclue::createGeoclueClientProxyCallback(GObject*, GAsyncResult* result, GeolocationProviderGeoclue* provider)
{
    GUniqueOutPtr<GError> error;
    provider->m_clientProxy = adoptGRef(geoclue_client_proxy_new_for_bus_finish(result, &error.outPtr()));
    if (error) {
        provider->errorOccurred(error->message);
        return;
    }

    // Geoclue2 requires the client to provide a desktop ID for security
    // reasons, which should identify the application requesting the location.
    // FIXME: We provide the program name as the desktop ID for now but, in an ideal world,
    // we should provide a proper "application ID" (normally the name of a .desktop file).
    // https://bugs.webkit.org/show_bug.cgi?id=129879
    geoclue_client_set_desktop_id(provider->m_clientProxy.get(), g_get_prgname());

    provider->startGeoclueClient();
}

void GeolocationProviderGeoclue::startClientCallback(GObject* sourceObject, GAsyncResult* result, GeolocationProviderGeoclue* provider)
{
    GUniqueOutPtr<GError> error;
    if (!geoclue_client_call_start_finish(GEOCLUE_CLIENT(sourceObject), result, &error.outPtr()))
        static_cast<GeolocationProviderGeoclue*>(provider)->errorOccurred(error->message);
}

void GeolocationProviderGeoclue::locationUpdatedCallback(GeoclueClient*, const gchar*, const gchar* newPath, GeolocationProviderGeoclue* provider)
{
    geoclue_location_proxy_new_for_bus(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, gGeoclueBusName, newPath, nullptr,
        reinterpret_cast<GAsyncReadyCallback>(createLocationProxyCallback), provider);
}

void GeolocationProviderGeoclue::createLocationProxyCallback(GObject*, GAsyncResult* result, GeolocationProviderGeoclue* provider)
{
    GUniqueOutPtr<GError> error;
    GRefPtr<GeoclueLocation> locationProxy = adoptGRef(geoclue_location_proxy_new_for_bus_finish(result, &error.outPtr()));
    if (error) {
        provider->errorOccurred(error->message);
        return;
    }
    provider->updateLocation(locationProxy.get());
}

void GeolocationProviderGeoclue::startGeoclueClient()
{
    // Set the requirement for the client.
    updateClientRequirements();

    g_signal_connect(m_clientProxy.get(), "location-updated", G_CALLBACK(locationUpdatedCallback), this);
    geoclue_client_call_start(m_clientProxy.get(), nullptr, reinterpret_cast<GAsyncReadyCallback>(startClientCallback), this);
}

void GeolocationProviderGeoclue::updateLocation(GeoclueLocation* locationProxy)
{
    GTimeVal timeValue;
    g_get_current_time(&timeValue);
    m_timestamp = timeValue.tv_sec;
    m_latitude = geoclue_location_get_latitude(locationProxy);
    m_longitude = geoclue_location_get_longitude(locationProxy);
    m_accuracy = geoclue_location_get_accuracy(locationProxy);
    m_client->notifyPositionChanged(m_timestamp, m_latitude, m_longitude, m_altitude, m_accuracy, m_altitudeAccuracy);
}

void GeolocationProviderGeoclue::errorOccurred(const char* message)
{
    m_isUpdating = false;
    m_client->notifyErrorOccurred(message);
}

void GeolocationProviderGeoclue::updateClientRequirements()
{
    if (!m_clientProxy)
        return;

    GeoclueAccuracyLevel accuracyLevel = m_enableHighAccuracy ? GeoclueAccuracyLevelExact : GeoclueAccuracyLevelCity;
    geoclue_client_set_requested_accuracy_level(m_clientProxy.get(), accuracyLevel);
}

#endif // ENABLE(GEOLOCATION) && USE(GEOCLUE2)

/*
 * Copyright (C) 2019 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GeoclueGeolocationProvider.h"

#include <WebCore/GeolocationPositionData.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <wtf/glib/GUniquePtr.h>

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace WebKit {

GeoclueGeolocationProvider::GeoclueGeolocationProvider()
    : m_destroyManagerLaterTimer(RunLoop::current(), this, &GeoclueGeolocationProvider::destroyManager)
{
#if USE(GLIB_EVENT_LOOP)
    m_destroyManagerLaterTimer.setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
#endif
}

GeoclueGeolocationProvider::~GeoclueGeolocationProvider()
{
    stop();
}

void GeoclueGeolocationProvider::start(UpdateNotifyFunction&& updateNotifyFunction)
{
    if (m_isRunning)
        return;

    m_destroyManagerLaterTimer.stop();
    m_updateNotifyFunction = WTFMove(updateNotifyFunction);
    m_isRunning = true;
    m_cancellable = adoptGRef(g_cancellable_new());
    if (!m_manager) {
        g_dbus_proxy_new_for_bus(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, nullptr,
            "org.freedesktop.GeoClue2", "/org/freedesktop/GeoClue2/Manager", "org.freedesktop.GeoClue2.Manager", m_cancellable.get(),
            [](GObject*, GAsyncResult* result, gpointer userData) {
                GUniqueOutPtr<GError> error;
                GRefPtr<GDBusProxy> proxy = adoptGRef(g_dbus_proxy_new_for_bus_finish(result, &error.outPtr()));
                if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                    return;

                auto& provider = *static_cast<GeoclueGeolocationProvider*>(userData);
                if (error) {
                    provider.didFail(_("Failed to connect to geolocation service"));
                    return;
                }
                provider.setupManager(WTFMove(proxy));
            }, this);
        return;
    }

    startClient();
}

void GeoclueGeolocationProvider::stop()
{
    if (!m_isRunning)
        return;

    m_isRunning = false;
    m_updateNotifyFunction = nullptr;
    g_cancellable_cancel(m_cancellable.get());
    m_cancellable = nullptr;
    stopClient();
    destroyManagerLater();
}

void GeoclueGeolocationProvider::setEnableHighAccuracy(bool enabled)
{
    if (m_isHighAccuracyEnabled == enabled)
        return;

    requestAccuracyLevel();
}

void GeoclueGeolocationProvider::destroyManagerLater()
{
    if (!m_manager)
        return;

    if (m_destroyManagerLaterTimer.isActive())
        return;

    m_destroyManagerLaterTimer.startOneShot(60_s);
}

void GeoclueGeolocationProvider::destroyManager()
{
    ASSERT(!m_isRunning);
    m_client = nullptr;
    m_manager = nullptr;
}

void GeoclueGeolocationProvider::setupManager(GRefPtr<GDBusProxy>&& proxy)
{
    m_manager = WTFMove(proxy);
    if (!m_isRunning) {
        destroyManagerLater();
        return;
    }

    g_dbus_proxy_call(m_manager.get(), "CreateClient", nullptr, G_DBUS_CALL_FLAGS_NONE, -1, m_cancellable.get(),
        [](GObject* manager, GAsyncResult* result, gpointer userData) {
            GUniqueOutPtr<GError> error;
            GRefPtr<GVariant> returnValue = adoptGRef(g_dbus_proxy_call_finish(G_DBUS_PROXY(manager), result, &error.outPtr()));
            if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                return;

            auto& provider = *static_cast<GeoclueGeolocationProvider*>(userData);
            if (error) {
                provider.didFail(_("Failed to connect to geolocation service"));
                return;
            }
            const char* clientPath;
            g_variant_get(returnValue.get(), "(&o)", &clientPath);
            provider.createClient(clientPath);
        }, this);
}

void GeoclueGeolocationProvider::createClient(const char* clientPath)
{
    if (!m_isRunning) {
        destroyManagerLater();
        return;
    }

    g_dbus_proxy_new_for_bus(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, nullptr,
        "org.freedesktop.GeoClue2", clientPath, "org.freedesktop.GeoClue2.Client", m_cancellable.get(),
        [](GObject*, GAsyncResult* result, gpointer userData) {
            GUniqueOutPtr<GError> error;
            GRefPtr<GDBusProxy> proxy = adoptGRef(g_dbus_proxy_new_for_bus_finish(result, &error.outPtr()));
            if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                return;

            auto& provider = *static_cast<GeoclueGeolocationProvider*>(userData);
            if (error) {
                provider.didFail(_("Failed to connect to geolocation service"));
                return;
            }
            provider.setupClient(WTFMove(proxy));
        }, this);
}

void GeoclueGeolocationProvider::setupClient(GRefPtr<GDBusProxy>&& proxy)
{
    m_client = WTFMove(proxy);
    if (!m_isRunning) {
        destroyManagerLater();
        return;
    }

    // Geoclue2 requires the client to provide a desktop ID for security
    // reasons, which should identify the application requesting the location.
    // We use the application ID configured for the default GApplication, and
    // also fallback to our old behavior of using g_get_prgname().
    const char* applicationID = nullptr;
    if (auto* defaultApplication = g_application_get_default())
        applicationID = g_application_get_application_id(defaultApplication);
    if (!applicationID)
        applicationID = g_get_prgname();
    g_dbus_proxy_call(m_client.get(), "org.freedesktop.DBus.Properties.Set",
        g_variant_new("(ssv)", "org.freedesktop.GeoClue2.Client", "DesktopId", g_variant_new_string(applicationID)),
        G_DBUS_CALL_FLAGS_NONE, -1, nullptr, nullptr, nullptr);

    requestAccuracyLevel();

    startClient();
}

void GeoclueGeolocationProvider::startClient()
{
    if (!m_client)
        return;

    g_signal_connect(m_client.get(), "g-signal", G_CALLBACK(clientLocationUpdatedCallback), this);

    g_dbus_proxy_call(m_client.get(), "Start", nullptr, G_DBUS_CALL_FLAGS_NONE, -1, m_cancellable.get(),
        [](GObject* client, GAsyncResult* result, gpointer userData) {
            GUniqueOutPtr<GError> error;
            GRefPtr<GVariant> returnValue = adoptGRef(g_dbus_proxy_call_finish(G_DBUS_PROXY(client), result, &error.outPtr()));
            if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                return;

            auto& provider = *static_cast<GeoclueGeolocationProvider*>(userData);
            if (error) {
                provider.didFail(_("Failed to determine position from geolocation service"));
                return;
            }
        }, this);
}

void GeoclueGeolocationProvider::stopClient()
{
    if (!m_client)
        return;

    g_signal_handlers_disconnect_matched(m_client.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
    g_dbus_proxy_call(m_client.get(), "Stop", nullptr, G_DBUS_CALL_FLAGS_NONE, -1, nullptr, nullptr, nullptr);
}

void GeoclueGeolocationProvider::requestAccuracyLevel()
{
    if (!m_client)
        return;

    // From https://bugs.webkit.org/show_bug.cgi?id=214566:
    //
    //   "Websites like OpenStreetMap or Google Maps do not use the
    //    enableHighAccuracy position options for simple location, which
    //    not very useful with only a city level of accuracy. They appear
    //    to assume that at least a street level of accuracy could be given
    //    without enabling a GPS on the device."
    //
    // GeoclueAccuracyLevelStreetLevel = 6, GeoclueAccuracyLevelExact = 8.
    unsigned accuracy = m_isHighAccuracyEnabled ? 8 : 6;
    g_dbus_proxy_call(m_client.get(), "org.freedesktop.DBus.Properties.Set",
        g_variant_new("(ssv)", "org.freedesktop.GeoClue2.Client", "RequestedAccuracyLevel", g_variant_new_uint32(accuracy)),
        G_DBUS_CALL_FLAGS_NONE, -1, nullptr, nullptr, nullptr);
}

void GeoclueGeolocationProvider::clientLocationUpdatedCallback(GDBusProxy* client, gchar*, gchar* signal, GVariant* parameters, gpointer userData)
{
    if (g_strcmp0(signal, "LocationUpdated"))
        return;

    const char* locationPath;
    g_variant_get(parameters, "(o&o)", nullptr, &locationPath);
    auto& provider = *static_cast<GeoclueGeolocationProvider*>(userData);
    provider.createLocation(locationPath);
}

void GeoclueGeolocationProvider::createLocation(const char* locationPath)
{
    g_dbus_proxy_new_for_bus(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, nullptr,
        "org.freedesktop.GeoClue2", locationPath, "org.freedesktop.GeoClue2.Location", m_cancellable.get(),
        [](GObject*, GAsyncResult* result, gpointer userData) {
            GUniqueOutPtr<GError> error;
            GRefPtr<GDBusProxy> proxy = adoptGRef(g_dbus_proxy_new_for_bus_finish(result, &error.outPtr()));
            if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                return;

            auto& provider = *static_cast<GeoclueGeolocationProvider*>(userData);
            if (error) {
                provider.didFail(_("Failed to determine position from geolocation service"));
                return;
            }
            provider.locationUpdated(WTFMove(proxy));
        }, this);
}

void GeoclueGeolocationProvider::locationUpdated(GRefPtr<GDBusProxy>&& proxy)
{
    WebCore::GeolocationPositionData position;
    GRefPtr<GVariant> property = adoptGRef(g_dbus_proxy_get_cached_property(proxy.get(), "Latitude"));
    position.latitude = g_variant_get_double(property.get());
    property = adoptGRef(g_dbus_proxy_get_cached_property(proxy.get(), "Longitude"));
    position.longitude = g_variant_get_double(property.get());
    property = adoptGRef(g_dbus_proxy_get_cached_property(proxy.get(), "Accuracy"));
    position.accuracy = g_variant_get_double(property.get());
    property = adoptGRef(g_dbus_proxy_get_cached_property(proxy.get(), "Altitude"));
    position.altitude = g_variant_get_double(property.get());
    property = adoptGRef(g_dbus_proxy_get_cached_property(proxy.get(), "Speed"));
    position.speed = g_variant_get_double(property.get());
    property = adoptGRef(g_dbus_proxy_get_cached_property(proxy.get(), "Heading"));
    position.heading = g_variant_get_double(property.get());
    property = adoptGRef(g_dbus_proxy_get_cached_property(proxy.get(), "Timestamp"));
    guint64 timestamp;
    g_variant_get(property.get(), "(tt)", &timestamp, nullptr);
    position.timestamp = static_cast<double>(timestamp);
    m_updateNotifyFunction(WTFMove(position), WTF::nullopt);
}

void GeoclueGeolocationProvider::didFail(CString errorMessage)
{
    if (m_updateNotifyFunction)
        m_updateNotifyFunction({ }, errorMessage);
}

} // namespace WebKit

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
#include <wtf/TZoneMallocInlines.h>
#include <wtf/glib/Application.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/Sandbox.h>
#include <wtf/text/MakeString.h>

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GeoclueGeolocationProvider);

GeoclueGeolocationProvider::GeoclueGeolocationProvider()
    : m_destroyLaterTimer(RunLoop::current(), this, &GeoclueGeolocationProvider::destroyState)
{
#if USE(GLIB_EVENT_LOOP)
    m_destroyLaterTimer.setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
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

    m_destroyLaterTimer.stop();
    m_updateNotifyFunction = WTFMove(updateNotifyFunction);
    m_isRunning = true;
    m_cancellable = adoptGRef(g_cancellable_new());

    switch (m_sourceType) {
    case LocationProviderSource::Unknown:
        acquirePortalProxy();
        break;
    case LocationProviderSource::Portal:
        ASSERT(m_portal.locationPortal);
        createPortalSession();
        break;
    case LocationProviderSource::Geoclue:
        ASSERT(m_geoclue.manager);
        startGeoclueClient();
        break;
    }
}

void GeoclueGeolocationProvider::stop()
{
    if (!m_isRunning)
        return;

    m_isRunning = false;
    m_updateNotifyFunction = nullptr;
    g_cancellable_cancel(m_cancellable.get());
    m_cancellable = nullptr;

    switch (m_sourceType) {
    case LocationProviderSource::Unknown:
        break;
    case LocationProviderSource::Portal:
        stopPortalSession();
        destroyStateLater();
        break;
    case LocationProviderSource::Geoclue:
        stopGeoclueClient();
        destroyStateLater();
        break;
    }

    m_sourceType = LocationProviderSource::Unknown;
}

void GeoclueGeolocationProvider::setEnableHighAccuracy(bool enabled)
{
    if (m_isHighAccuracyEnabled == enabled)
        return;

    m_isHighAccuracyEnabled = enabled;

    requestAccuracyLevel();
}

void GeoclueGeolocationProvider::destroyStateLater()
{
    if (!m_geoclue.manager)
        return;

    if (m_destroyLaterTimer.isActive())
        return;

    m_destroyLaterTimer.startOneShot(60_s);
}

void GeoclueGeolocationProvider::destroyState()
{
    ASSERT(!m_isRunning);

    m_portal.locationPortal = nullptr;
    m_portal.senderName = std::nullopt;
    m_portal.sessionId = std::nullopt;

    m_geoclue.client = nullptr;
    m_geoclue.manager = nullptr;
}

void GeoclueGeolocationProvider::acquirePortalProxy()
{
    ASSERT(!m_portal.locationPortal);

    if (!shouldUsePortal()) {
        createGeoclueManager();
        return;
    }

    g_dbus_proxy_new_for_bus(G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, nullptr,
        "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.Location", m_cancellable.get(),
        [](GObject*, GAsyncResult* result, gpointer userData) {
            GUniqueOutPtr<GError> error;
            GRefPtr<GDBusProxy> proxy = adoptGRef(g_dbus_proxy_new_for_bus_finish(result, &error.outPtr()));
            if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                return;

            auto& provider = *static_cast<GeoclueGeolocationProvider*>(userData);
            if (error) {
                provider.createGeoclueManager();
                return;
            }
            provider.setupPortalProxy(WTFMove(proxy));
        }, this);
}

void GeoclueGeolocationProvider::setupPortalProxy(GRefPtr<GDBusProxy>&& proxy)
{
    m_sourceType = LocationProviderSource::Portal;

    m_portal.locationPortal = WTFMove(proxy);

    auto* connection = g_dbus_proxy_get_connection(m_portal.locationPortal.get());
    auto uniqueName = String::fromUTF8(g_dbus_connection_get_unique_name(connection));

    // The portal sender name is the D-Bus unique name, without the initial ':',
    // and with all '.' replaced by '_'
    m_portal.senderName = makeStringByReplacingAll(uniqueName.substring(1), '.', '_');

    if (!m_isRunning) {
        destroyStateLater();
        return;
    }

    createPortalSession();
}

void GeoclueGeolocationProvider::createPortalSession()
{
    ASSERT(m_sourceType == LocationProviderSource::Portal);
    ASSERT(m_portal.locationPortal);
    ASSERT(m_portal.senderName);

    auto sessionToken = makeString("WebKit"_s, weakRandomNumber<uint32_t>());

    m_portal.sessionId = makeString("/org/freedesktop/portal/desktop/session/"_s, *m_portal.senderName, '/', sessionToken);

    // XDG Desktop Portal and Geoclue use different values for STREET and EXACT precision.
    // See: https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Location.html
    // The Location portal uses STREET: 4 and EXACT: 5
    unsigned accuracy = m_isHighAccuracyEnabled ? 5 : 4;

    GVariantBuilder options;
    g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&options, "{sv}", "session_handle_token", g_variant_new_string(sessionToken.ascii().data()));
    g_variant_builder_add(&options, "{sv}", "accuracy", g_variant_new_uint32(accuracy));

    g_dbus_proxy_call(m_portal.locationPortal.get(), "CreateSession", g_variant_new("(a{sv})", &options), G_DBUS_CALL_FLAGS_NONE, -1, m_cancellable.get(),
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
            provider.startPortalSession();
        }, this);
}

void GeoclueGeolocationProvider::startPortalSession()
{
    ASSERT(m_sourceType == LocationProviderSource::Portal);
    ASSERT(m_portal.locationPortal);
    ASSERT(m_portal.senderName);

    auto token = makeString("WebKit"_s, weakRandomNumber<uint32_t>());
    auto requestPath = makeString("/org/freedesktop/portal/desktop/request/"_s, *m_portal.senderName, '/', token);

    auto* connection = g_dbus_proxy_get_connection(m_portal.locationPortal.get());
    m_portal.responseSignalId = g_dbus_connection_signal_subscribe(connection, "org.freedesktop.portal.Desktop", "org.freedesktop.portal.Request",
        "Response", requestPath.ascii().data(), nullptr, G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
        [](GDBusConnection*, const char* /* senderName */, const char* /* objectPath */, const char* /* interfaceName */, const char* /* signalName */, GVariant* parameters, gpointer userData) {
            GRefPtr<GVariant> returnValue;
            uint32_t response;

            g_variant_get(parameters, "(u@a{sv})", &response, returnValue.outPtr());

            auto& provider = *static_cast<GeoclueGeolocationProvider*>(userData);
            if (response) {
                provider.didFail(_("Failed to connect to geolocation service"));
                provider.stopPortalSession();
            }
        }, this, nullptr);

    m_portal.locationUpdatedSignalId = g_dbus_connection_signal_subscribe(connection, "org.freedesktop.portal.Desktop", "org.freedesktop.portal.Location",
        "LocationUpdated", nullptr, nullptr, G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
        [](GDBusConnection*, const char* /* senderName */, const char* /* objectPath */, const char* /* interfaceName */, const char* /* signalName */, GVariant* parameters, gpointer userData) {
            GRefPtr<GVariant> locationData;

            g_variant_get(parameters, "(o@a{sv})", nullptr, &locationData.outPtr());

            auto& provider = *static_cast<GeoclueGeolocationProvider*>(userData);
            provider.portalLocationUpdated(locationData.get());
        }, this, nullptr);

    GVariantBuilder options;
    g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&options, "{sv}", "handle_token", g_variant_new_string(token.ascii().data()));

    g_dbus_proxy_call(m_portal.locationPortal.get(), "Start", g_variant_new("(osa{sv})", m_portal.sessionId->ascii().data(), "", &options), G_DBUS_CALL_FLAGS_NONE, -1, m_cancellable.get(),
        [](GObject* manager, GAsyncResult* result, gpointer userData) {
            GUniqueOutPtr<GError> error;
            GRefPtr<GVariant> returnValue = adoptGRef(g_dbus_proxy_call_finish(G_DBUS_PROXY(manager), result, &error.outPtr()));
            if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                return;

            auto& provider = *static_cast<GeoclueGeolocationProvider*>(userData);
            if (error)
                provider.didFail(_("Failed to connect to geolocation service"));
        }, this);
}

void GeoclueGeolocationProvider::portalLocationUpdated(GVariant* variant)
{
    WebCore::GeolocationPositionData position;

    g_variant_lookup(variant, "Accuracy", "d", &position.accuracy);
    g_variant_lookup(variant, "Altitude", "d", &position.altitude);
    g_variant_lookup(variant, "Heading", "d", &position.heading);
    g_variant_lookup(variant, "Latitude", "d", &position.latitude);
    g_variant_lookup(variant, "Longitude", "d", &position.longitude);
    g_variant_lookup(variant, "Speed", "d", &position.speed);

    guint64 timestamp;
    if (g_variant_lookup(variant, "Timestamp", "(tt)", &timestamp, nullptr))
        position.timestamp = static_cast<double>(timestamp);

    m_updateNotifyFunction(WTFMove(position), std::nullopt);
}

void GeoclueGeolocationProvider::stopPortalSession()
{
    if (!m_portal.sessionId)
        return;

    ASSERT(m_portal.locationPortal);

    auto* connection = g_dbus_proxy_get_connection(m_portal.locationPortal.get());
    ASSERT(connection);

    if (m_portal.locationUpdatedSignalId) {
        g_dbus_connection_signal_unsubscribe(connection, m_portal.locationUpdatedSignalId);
        m_portal.locationUpdatedSignalId = 0;
    }

    if (m_portal.responseSignalId) {
        g_dbus_connection_signal_unsubscribe(connection, m_portal.responseSignalId);
        m_portal.responseSignalId = 0;
    }

    m_portal.senderName = std::nullopt;

    g_dbus_connection_call(connection, "org.freedesktop.portal.Desktop", m_portal.sessionId->ascii().data(),
        "org.freedesktop.portal.Session", "Close", nullptr, nullptr, G_DBUS_CALL_FLAGS_NONE, -1, nullptr, nullptr, nullptr);
    m_portal.sessionId = std::nullopt;
}

void GeoclueGeolocationProvider::createGeoclueManager()
{
    ASSERT(!m_geoclue.manager);

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
            provider.setupGeoclueManager(WTFMove(proxy));
        }, this);
}

void GeoclueGeolocationProvider::setupGeoclueManager(GRefPtr<GDBusProxy>&& proxy)
{
    m_sourceType = LocationProviderSource::Geoclue;

    m_geoclue.manager = WTFMove(proxy);
    if (!m_isRunning) {
        destroyStateLater();
        return;
    }

    g_dbus_proxy_call(m_geoclue.manager.get(), "CreateClient", nullptr, G_DBUS_CALL_FLAGS_NONE, -1, m_cancellable.get(),
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
            provider.createGeoclueClient(clientPath);
        }, this);
}

void GeoclueGeolocationProvider::createGeoclueClient(const char* clientPath)
{
    if (!m_isRunning) {
        destroyStateLater();
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
            provider.setupGeoclueClient(WTFMove(proxy));
        }, this);
}

void GeoclueGeolocationProvider::setupGeoclueClient(GRefPtr<GDBusProxy>&& proxy)
{
    m_geoclue.client = WTFMove(proxy);
    if (!m_isRunning) {
        destroyStateLater();
        return;
    }

    // Geoclue2 requires the client to provide a desktop ID for security
    // reasons, which should identify the application requesting the location.
    g_dbus_proxy_call(m_geoclue.client.get(), "org.freedesktop.DBus.Properties.Set",
        g_variant_new("(ssv)", "org.freedesktop.GeoClue2.Client", "DesktopId", g_variant_new_string(WTF::applicationID().data())),
        G_DBUS_CALL_FLAGS_NONE, -1, nullptr, [](GObject* manager, GAsyncResult* result, gpointer userData) {
            GUniqueOutPtr<GError> error;
            GRefPtr<GVariant> returnValue = adoptGRef(g_dbus_proxy_call_finish(G_DBUS_PROXY(manager), result, &error.outPtr()));
            if (error && !g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                g_warning("Error setting Geoclue client desktop id: %s", error->message);
        }, nullptr);

    requestAccuracyLevel();

    startGeoclueClient();
}

void GeoclueGeolocationProvider::startGeoclueClient()
{
    if (!m_geoclue.client)
        return;

    g_signal_connect(m_geoclue.client.get(), "g-signal", G_CALLBACK(clientLocationUpdatedCallback), this);

    g_dbus_proxy_call(m_geoclue.client.get(), "Start", nullptr, G_DBUS_CALL_FLAGS_NONE, -1, m_cancellable.get(),
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

void GeoclueGeolocationProvider::stopGeoclueClient()
{
    if (!m_geoclue.client)
        return;

    g_signal_handlers_disconnect_matched(m_geoclue.client.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
    g_dbus_proxy_call(m_geoclue.client.get(), "Stop", nullptr, G_DBUS_CALL_FLAGS_NONE, -1, nullptr,
        [](GObject* manager, GAsyncResult* result, gpointer userData) {
            GUniqueOutPtr<GError> error;
            GRefPtr<GVariant> returnValue = adoptGRef(g_dbus_proxy_call_finish(G_DBUS_PROXY(manager), result, &error.outPtr()));
            if (error)
                g_warning("Error stopping Geoclue client: %s", error->message);
        }, nullptr);
}

void GeoclueGeolocationProvider::requestAccuracyLevel()
{
    switch (m_sourceType) {
    case LocationProviderSource::Unknown:
        break;
    case LocationProviderSource::Portal:
        // The Location portal does not offer any way to change the accuracy
        // level after the session is created, so we recreate the session.
        if (m_portal.sessionId) {
            stopPortalSession();
            createPortalSession();
        }
        break;
    case LocationProviderSource::Geoclue:
        if (m_geoclue.client) {
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
            g_dbus_proxy_call(m_geoclue.client.get(), "org.freedesktop.DBus.Properties.Set",
                g_variant_new("(ssv)", "org.freedesktop.GeoClue2.Client", "RequestedAccuracyLevel", g_variant_new_uint32(accuracy)),
                G_DBUS_CALL_FLAGS_NONE, -1, m_cancellable.get(), [](GObject* manager, GAsyncResult* result, gpointer userData) {
                    GUniqueOutPtr<GError> error;
                    GRefPtr<GVariant> returnValue = adoptGRef(g_dbus_proxy_call_finish(G_DBUS_PROXY(manager), result, &error.outPtr()));
                    if (error && !g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning("Error requesting accuracy level: %s", error->message);
                }, nullptr);
        }
        break;
    }
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
    m_updateNotifyFunction(WTFMove(position), std::nullopt);
}

void GeoclueGeolocationProvider::didFail(CString errorMessage)
{
    if (m_updateNotifyFunction)
        m_updateNotifyFunction({ }, errorMessage);
}

} // namespace WebKit

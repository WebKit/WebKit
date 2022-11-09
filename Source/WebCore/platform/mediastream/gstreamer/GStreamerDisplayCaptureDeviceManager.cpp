/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Author: Thibault Saunier <tsaunier@igalia.com>
 * Author: Alejandro G. Castro <alex@igalia.com>
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
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
#include "GStreamerCaptureDeviceManager.h"

#include "GStreamerCommon.h"
#include "GStreamerVideoCaptureSource.h"
#include <gio/gunixfdlist.h>
#include <wtf/UUID.h>

namespace WebCore {

static const Seconds s_dbusCallTimeout = 10_ms;

GStreamerDisplayCaptureDeviceManager& GStreamerDisplayCaptureDeviceManager::singleton()
{
    static NeverDestroyed<GStreamerDisplayCaptureDeviceManager> manager;
    return manager;
}

GStreamerDisplayCaptureDeviceManager::GStreamerDisplayCaptureDeviceManager()
{
}

GStreamerDisplayCaptureDeviceManager::~GStreamerDisplayCaptureDeviceManager()
{
    for (auto& sourceId : m_sessions.keys())
        stopSource(sourceId);
}

void GStreamerDisplayCaptureDeviceManager::computeCaptureDevices(CompletionHandler<void()>&& callback)
{
    m_devices.clear();

    CaptureDevice screenCaptureDevice(createVersion4UUIDString(), CaptureDevice::DeviceType::Screen, makeString("Capture Screen"));
    screenCaptureDevice.setEnabled(true);
    m_devices.append(WTFMove(screenCaptureDevice));
    callback();
}

CaptureSourceOrError GStreamerDisplayCaptureDeviceManager::createDisplayCaptureSource(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints)
{
    const auto it = m_sessions.find(device.persistentId());
    if (it != m_sessions.end()) {
        return GStreamerVideoCaptureSource::createPipewireSource(device.persistentId().isolatedCopy(),
            it->value->nodeAndFd, WTFMove(hashSalts), constraints, device.type());
    }

    GUniqueOutPtr<GError> error;
    m_proxy = adoptGRef(g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
        static_cast<GDBusProxyFlags>(G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS | G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES), nullptr,
        "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.ScreenCast", nullptr, &error.outPtr()));
    if (error) {
        WTFLogAlways("Unable to connect to the Deskop portal: %s", error->message);
        return { };
    }

    auto token = makeString("WebKit", weakRandomNumber<uint32_t>());
    auto sessionToken = makeString("WebKit", weakRandomNumber<uint32_t>());
    GVariantBuilder options;
    g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&options, "{sv}", "handle_token", g_variant_new_string(token.ascii().data()));
    g_variant_builder_add(&options, "{sv}", "session_handle_token", g_variant_new_string(sessionToken.ascii().data()));

    auto result = adoptGRef(g_dbus_proxy_call_sync(m_proxy.get(), "CreateSession", g_variant_new("(a{sv})", &options),
        G_DBUS_CALL_FLAGS_NONE, s_dbusCallTimeout.millisecondsAs<int>(), nullptr, &error.outPtr()));
    if (error) {
        WTFLogAlways("Unable to create a Deskop portal session: %s", error->message);
        return { };
    }

    GUniqueOutPtr<char> objectPath;
    g_variant_get(result.get(), "(o)", &objectPath.outPtr());
    waitResponseSignal(objectPath.get());

    auto requestPath = String::fromLatin1(objectPath.get());
    auto sessionPath = makeStringByReplacingAll(requestPath, "/request/"_s, "/session/"_s);
    sessionPath = makeStringByReplacingAll(sessionPath, token, sessionToken);

    // FIXME: Maybe check this depending on device.type().
    auto outputType = GStreamerDisplayCaptureDeviceManager::PipeWireOutputType::Monitor | GStreamerDisplayCaptureDeviceManager::PipeWireOutputType::Window;

    token = makeString("WebKit", weakRandomNumber<uint32_t>());
    g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&options, "{sv}", "handle_token", g_variant_new_string(token.ascii().data()));
    g_variant_builder_add(&options, "{sv}", "types", g_variant_new_uint32(static_cast<uint32_t>(outputType)));
    g_variant_builder_add(&options, "{sv}", "multiple", g_variant_new_boolean(false));

    auto propertiesResult = adoptGRef(g_dbus_proxy_call_sync(m_proxy.get(), "org.freedesktop.DBus.Properties.Get",
        g_variant_new("(ss)", "org.freedesktop.portal.ScreenCast", "version"), G_DBUS_CALL_FLAGS_NONE,
        s_dbusCallTimeout.millisecondsAs<int>(), nullptr, nullptr));
    if (propertiesResult) {
        GRefPtr<GVariant> property;
        g_variant_get(propertiesResult.get(), "(v)", &property.outPtr());
        if (g_variant_get_uint32(property.get()) >= 2) {
            // Enable embedded cursor. FIXME: Should be checked in the constraints.
            g_variant_builder_add(&options, "{sv}", "cursor_mode", g_variant_new_uint32(2));
        }
    }

    result = adoptGRef(g_dbus_proxy_call_sync(m_proxy.get(), "SelectSources",
        g_variant_new("(oa{sv})", sessionPath.ascii().data(), &options), G_DBUS_CALL_FLAGS_NONE, s_dbusCallTimeout.millisecondsAs<int>(), nullptr, &error.outPtr()));
    if (error) {
        WTFLogAlways("SelectSources error: %s", error->message);
        return { };
    }
    g_variant_get(result.get(), "(o)", &objectPath.outPtr());
    waitResponseSignal(objectPath.get());

    token = makeString("WebKit", weakRandomNumber<uint32_t>());
    g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&options, "{sv}", "handle_token", g_variant_new_string(token.ascii().data()));
    result = adoptGRef(g_dbus_proxy_call_sync(m_proxy.get(), "Start",
        g_variant_new("(osa{sv})", sessionPath.ascii().data(), "", &options), G_DBUS_CALL_FLAGS_NONE, s_dbusCallTimeout.millisecondsAs<int>(), nullptr, &error.outPtr()));
    if (error) {
        WTFLogAlways("Start error: %s", error->message);
        return { };
    }

    std::optional<uint32_t> nodeId;
    g_variant_get(result.get(), "(o)", &objectPath.outPtr());
    waitResponseSignal(objectPath.get(), [&nodeId](GVariant* parameters) mutable {
        uint32_t portalResponse;
        GRefPtr<GVariant> responseData;
        g_variant_get(parameters, "(u@a{sv})", &portalResponse, &responseData.outPtr());

        if (portalResponse) {
            WTFLogAlways("User cancelled the Start request or an unknown error happened");
            return;
        }

        // The portal interface allows multiple streams but we care only about the first one.
        GUniqueOutPtr<GVariantIter> iter;
        if (g_variant_lookup(responseData.get(), "streams", "a(ua{sv})", &iter.outPtr())) {
            auto variant = adoptGRef(g_variant_iter_next_value(iter.get()));
            if (!variant) {
                WTFLogAlways("Stream list is empty");
                return;
            }

            uint32_t streamId;
            GRefPtr<GVariant> options;
            g_variant_get(variant.get(), "(u@a{sv})", &streamId, &options.outPtr());
            nodeId = streamId;
        }
    });

    if (!nodeId) {
        WTFLogAlways("Unable to retrieve display capture session data");
        return { };
    }

    GRefPtr<GUnixFDList> fdList;
    int fd = -1;
    g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
    result = adoptGRef(g_dbus_proxy_call_with_unix_fd_list_sync(m_proxy.get(), "OpenPipeWireRemote",
        g_variant_new("(oa{sv})", sessionPath.ascii().data(), &options), G_DBUS_CALL_FLAGS_NONE, s_dbusCallTimeout.millisecondsAs<int>(), nullptr, &fdList.outPtr(), nullptr, &error.outPtr()));
    if (error) {
        WTFLogAlways("Unable to request display capture. Error: %s", error->message);
        return { };
    }

    int fdOut;
    g_variant_get(result.get(), "(h)", &fdOut);
    fd = g_unix_fd_list_get(fdList.get(), fdOut, nullptr);

    NodeAndFD nodeAndFd = { *nodeId, fd };
    auto session = makeUnique<GStreamerDisplayCaptureDeviceManager::Session>(nodeAndFd, WTFMove(sessionPath));
    m_sessions.add(device.persistentId(), WTFMove(session));
    return GStreamerVideoCaptureSource::createPipewireSource(device.persistentId().isolatedCopy(), nodeAndFd, WTFMove(hashSalts), constraints, device.type());
}

void GStreamerDisplayCaptureDeviceManager::stopSource(const String& persistentID)
{
    auto session = m_sessions.take(persistentID);
    GUniqueOutPtr<GError> error;
    auto proxy = adoptGRef(g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
        static_cast<GDBusProxyFlags>(G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS | G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES), nullptr,
        "org.freedesktop.portal.Desktop", session->path.ascii().data(), "org.freedesktop.portal.Session", nullptr, &error.outPtr()));
    if (error) {
        WTFLogAlways("Unable to connect to the Deskop portal: %s", error->message);
        return;
    }
    auto dbusCallTimeout = 100_ms;
    auto result = adoptGRef(g_dbus_proxy_call_sync(proxy.get(), "Close", nullptr, G_DBUS_CALL_FLAGS_NONE,
        dbusCallTimeout.millisecondsAs<int>(), nullptr, &error.outPtr()));
    if (error)
        WTFLogAlways("Portal session could not be closed: %s", error->message);
}

void GStreamerDisplayCaptureDeviceManager::waitResponseSignal(const char* objectPath, ResponseCallback&& callback)
{
    RELEASE_ASSERT(!m_currentResponseCallback);
    m_currentResponseCallback = WTFMove(callback);
    auto* connection = g_dbus_proxy_get_connection(m_proxy.get());
    auto signalId = g_dbus_connection_signal_subscribe(connection, "org.freedesktop.portal.Desktop", "org.freedesktop.portal.Request",
        "Response", objectPath, nullptr, G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE, reinterpret_cast<GDBusSignalCallback>(+[](GDBusConnection*, const char* /* senderName */, const char* /* objectPath */, const char* /* interfaceName */, const char* /* signalName */, GVariant* parameters, gpointer userData) {
            auto& manager = *reinterpret_cast<GStreamerDisplayCaptureDeviceManager*>(userData);
            manager.notifyResponse(parameters);
        }), this, nullptr);

    while (m_currentResponseCallback)
        g_main_context_iteration(nullptr, false);

    g_dbus_connection_signal_unsubscribe(connection, signalId);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)

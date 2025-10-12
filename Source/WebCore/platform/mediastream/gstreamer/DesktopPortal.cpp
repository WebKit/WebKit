/*
 * Copyright (C) 2025 Igalia S.L.
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
#include "DesktopPortal.h"

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)

#include "GRefPtrGStreamer.h"
#include <gio/gunixfdlist.h>
#include <gst/video/video-format.h>
#include <optional>
#include <unistd.h>
#include <wtf/Scope.h>
#include <wtf/UUID.h>
#include <wtf/WeakRandomNumber.h>
#include <wtf/glib/GUniquePtr.h>

#if GST_CHECK_VERSION(1, 24, 0)
#include <gst/video/video-info-dma.h>
#endif

namespace WebCore {

static const Seconds s_dbusCallTimeout = 10_ms;

static GRefPtr<GDBusProxy> createDBusProxy(ASCIILiteral interfaceName)
{
    GUniqueOutPtr<GError> error;
    auto proxy = adoptGRef(g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
        static_cast<GDBusProxyFlags>(G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS | G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES), nullptr,
        "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", interfaceName.characters(), nullptr, &error.outPtr()));
    if (error) {
        gst_printerrln("Unable to connect to the Deskop portal: %s", error->message);
        return nullptr;
    }
    return proxy;
}

RefPtr<DesktopPortalCamera> DesktopPortalCamera::create()
{
    auto interfaceName = "org.freedesktop.portal.Camera"_s;
    auto proxy = createDBusProxy(interfaceName);
    if (!proxy)
        return nullptr;
    return adoptRef(*new DesktopPortalCamera(interfaceName, WTFMove(proxy)));
}

DesktopPortalCamera::DesktopPortalCamera(ASCIILiteral interfaceName, GRefPtr<GDBusProxy>&& proxy)
    : DesktopPortal(interfaceName, WTFMove(proxy))
{
}

RefPtr<DesktopPortalScreenCast> DesktopPortalScreenCast::create()
{
    auto interfaceName = "org.freedesktop.portal.ScreenCast"_s;
    auto proxy = createDBusProxy(interfaceName);
    if (!proxy)
        return nullptr;
    return adoptRef(*new DesktopPortalScreenCast(interfaceName, WTFMove(proxy)));
}

DesktopPortalScreenCast::DesktopPortalScreenCast(ASCIILiteral interfaceName, GRefPtr<GDBusProxy>&& proxy)
    : DesktopPortal(interfaceName, WTFMove(proxy))
{
}

DesktopPortal::DesktopPortal(ASCIILiteral interfaceName, GRefPtr<GDBusProxy>&& proxy)
    : m_interfaceName(interfaceName)
    , m_proxy(WTFMove(proxy))
{
}

GRefPtr<GVariant> DesktopPortal::getProperty(ASCIILiteral name)
{
    auto propertiesResult = adoptGRef(g_dbus_proxy_call_sync(m_proxy.get(), "org.freedesktop.DBus.Properties.Get",
        g_variant_new("(ss)", m_interfaceName.characters(), name.characters()), G_DBUS_CALL_FLAGS_NONE,
        s_dbusCallTimeout.millisecondsAs<int>(), nullptr, nullptr));
    if (propertiesResult) {
        GRefPtr<GVariant> property;
        g_variant_get(propertiesResult.get(), "(v)", &property.outPtr());
        return property;
    }
    return nullptr;
}

void DesktopPortal::waitResponseSignal(ASCIILiteral objectPath, ResponseCallback&& callback)
{
    RELEASE_ASSERT(!m_currentResponseCallback);
    m_currentResponseCallback = WTFMove(callback);
    auto* connection = g_dbus_proxy_get_connection(m_proxy.get());
    auto signalId = g_dbus_connection_signal_subscribe(connection, "org.freedesktop.portal.Desktop", "org.freedesktop.portal.Request",
        "Response", objectPath.characters(), nullptr, G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE, reinterpret_cast<GDBusSignalCallback>(+[](GDBusConnection*, const char* /* senderName */, const char* /* objectPath */, const char* /* interfaceName */, const char* /* signalName */, GVariant* parameters, gpointer userData) {
            auto& self = *reinterpret_cast<DesktopPortal*>(userData);
            self.notifyResponse(parameters);
        }), this, nullptr);

    while (m_currentResponseCallback)
        g_main_context_iteration(nullptr, false);

    g_dbus_connection_signal_unsubscribe(connection, signalId);
}

bool DesktopPortalCamera::isCameraPresent()
{
    auto isCameraPresent = getProperty("IsCameraPresent"_s);
    if (!isCameraPresent)
        return false;

    return g_variant_get_boolean(isCameraPresent.get());
}

void DesktopPortalCamera::accessCamera(Function<void(std::optional<int>)>&& callback)
{
    auto token = makeString("WebKit"_s, weakRandomNumber<uint32_t>());
    GVariantBuilder options;
    g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&options, "{sv}", "handle_token", g_variant_new_string(token.utf8().data()));

    auto connection = g_dbus_proxy_get_connection(m_proxy.get());
    auto connectionString = StringView::fromLatin1(g_dbus_connection_get_unique_name(connection));
    auto sender = makeStringByReplacingAll(connectionString.substring(1), '.', '_');
    auto objectPath = makeString("/org/freedesktop/portal/desktop/request/"_s, sender, '/', token);

    m_currentResponseCallback = [callback = WTFMove(callback), this](auto* variant) mutable {
        if (!variant) {
            callback(std::nullopt);
            return;
        }

        uint32_t response;
        g_variant_get(variant, "(u@a{sv})", &response, nullptr);
        // 0 response value means the user allowed device access.
        if (!response) {
            callback(openCameraPipewireRemote());
            return;
        }
        callback(std::nullopt);
    };

    auto signalId = g_dbus_connection_signal_subscribe(connection, "org.freedesktop.portal.Desktop", "org.freedesktop.portal.Request",
        "Response", objectPath.utf8().data(), nullptr, G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE, reinterpret_cast<GDBusSignalCallback>(+[](GDBusConnection*, const char* /* senderName */, const char* /* objectPath */, const char* /* interfaceName */, const char* /* signalName */, GVariant* parameters, gpointer userData) {
            auto& self = *reinterpret_cast<DesktopPortal*>(userData);
            self.notifyResponse(parameters);
        }), this, nullptr);

    g_dbus_proxy_call(m_proxy.get(), "AccessCamera", g_variant_new("(a{sv})", &options), G_DBUS_CALL_FLAGS_NONE, -1, nullptr, reinterpret_cast<GAsyncReadyCallback>(+[](GDBusProxy* proxy, GAsyncResult* result, gpointer) {
        auto finalResult = adoptGRef(g_dbus_proxy_call_finish(proxy, result, nullptr));
    }), nullptr);

    while (m_currentResponseCallback)
        g_main_context_iteration(nullptr, false);
    g_dbus_connection_signal_unsubscribe(connection, signalId);
}

std::optional<int> DesktopPortalCamera::openCameraPipewireRemote()
{
    GRefPtr<GUnixFDList> fdList;
    int fd = -1;
    GVariantBuilder options;
    g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
    GUniqueOutPtr<GError> error;
    auto result = adoptGRef(g_dbus_proxy_call_with_unix_fd_list_sync(m_proxy.get(), "OpenPipeWireRemote",
        g_variant_new("(a{sv})", &options), G_DBUS_CALL_FLAGS_NONE, s_dbusCallTimeout.millisecondsAs<int>(), nullptr, &fdList.outPtr(), nullptr, &error.outPtr()));
    if (error) {
        gst_printerrln("Unable to open pipewire remote. Error: %s", error->message);
        return { };
    }

    int index;
    g_variant_get(result.get(), "(h)", &index);
    fd = g_unix_fd_list_get(fdList.get(), index, &error.outPtr());
    if (fd == -1) {
        gst_printerrln("Unable to open pipewire remote. Error: %s", error->message);
        return { };
    }
    return fd;
}

std::optional<DesktopPortalScreenCast::ScreencastSession> DesktopPortalScreenCast::createScreencastSession()
{
    auto token = makeString("WebKit"_s, weakRandomNumber<uint32_t>());
    auto sessionToken = makeString("WebKit"_s, weakRandomNumber<uint32_t>());
    GVariantBuilder options;
    g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&options, "{sv}", "handle_token", g_variant_new_string(token.ascii().data()));
    g_variant_builder_add(&options, "{sv}", "session_handle_token", g_variant_new_string(sessionToken.ascii().data()));

    GUniqueOutPtr<GError> error;
    auto result = adoptGRef(g_dbus_proxy_call_sync(m_proxy.get(), "CreateSession", g_variant_new("(a{sv})", &options),
        G_DBUS_CALL_FLAGS_NONE, s_dbusCallTimeout.millisecondsAs<int>(), nullptr, &error.outPtr()));
    if (error) {
        gst_printerrln("Unable to create a Deskop portal session: %s", error->message);
        return { };
    }

    GUniqueOutPtr<char> objectPath;
    g_variant_get(result.get(), "(o)", &objectPath.outPtr());
    waitResponseSignal(ASCIILiteral::fromLiteralUnsafe(objectPath.get()));

    auto requestPath = StringView::fromLatin1(objectPath.get());
    auto sessionPath = makeStringByReplacingAll(requestPath.toStringWithoutCopying(), "/request/"_s, "/session/"_s);
    sessionPath = makeStringByReplacingAll(sessionPath, token, sessionToken);
    return { ScreencastSession { WTFMove(sessionPath), m_proxy } };
}

void DesktopPortalScreenCast::closeSession(const String& path)
{
    GUniqueOutPtr<GError> error;
    auto proxy = adoptGRef(g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
        static_cast<GDBusProxyFlags>(G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS | G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES), nullptr,
        "org.freedesktop.portal.Desktop", path.ascii().data(), "org.freedesktop.portal.Session", nullptr, &error.outPtr()));
    if (error) {
        gst_printerrln("Unable to connect to the Desktop portal: %s", error->message);
        return;
    }
    auto dbusCallTimeout = 100_ms;
    auto result = adoptGRef(g_dbus_proxy_call_sync(proxy.get(), "Close", nullptr, G_DBUS_CALL_FLAGS_NONE,
        dbusCallTimeout.millisecondsAs<int>(), nullptr, &error.outPtr()));
    if (error)
        gst_printerrln("Portal session could not be closed: %s", error->message);
}

GRefPtr<GVariant> DesktopPortalScreenCast::ScreencastSession::selectSources(GVariantBuilder& options)
{
    auto token = makeString("WebKit"_s, weakRandomNumber<uint32_t>());
    g_variant_builder_add(&options, "{sv}", "handle_token", g_variant_new_string(token.ascii().data()));

    GUniqueOutPtr<GError> error;
    auto result = adoptGRef(g_dbus_proxy_call_sync(m_proxy.get(), "SelectSources",
        g_variant_new("(oa{sv})", m_path.ascii().data(), &options), G_DBUS_CALL_FLAGS_NONE, s_dbusCallTimeout.millisecondsAs<int>(), nullptr, &error.outPtr()));
    if (error) {
        gst_printerrln("SelectSources error: %s", error->message);
        return nullptr;
    }

    return result;
}

GRefPtr<GVariant> DesktopPortalScreenCast::ScreencastSession::start()
{
    auto token = makeString("WebKit"_s, weakRandomNumber<uint32_t>());
    GVariantBuilder options;
    g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&options, "{sv}", "handle_token", g_variant_new_string(token.ascii().data()));

    GUniqueOutPtr<GError> error;
    auto result = adoptGRef(g_dbus_proxy_call_sync(m_proxy.get(), "Start",
        g_variant_new("(osa{sv})", m_path.ascii().data(), "", &options), G_DBUS_CALL_FLAGS_NONE, s_dbusCallTimeout.millisecondsAs<int>(), nullptr, &error.outPtr()));
    if (error) {
        gst_printerrln("Start error: %s", error->message);
        return nullptr;
    }
    return result;
}

std::optional<PipeWireNodeData> DesktopPortalScreenCast::ScreencastSession::openPipewireRemote()
{
    GRefPtr<GUnixFDList> fdList;
    GVariantBuilder options;
    g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
    GUniqueOutPtr<GError> error;
    auto result = adoptGRef(g_dbus_proxy_call_with_unix_fd_list_sync(m_proxy.get(), "OpenPipeWireRemote",
        g_variant_new("(oa{sv})", m_path.ascii().data(), &options), G_DBUS_CALL_FLAGS_NONE, s_dbusCallTimeout.millisecondsAs<int>(), nullptr, &fdList.outPtr(), nullptr, &error.outPtr()));
    if (error) {
        gst_printerrln("Unable to open pipewire remote. Error: %s", error->message);
        return { };
    }

    int index;
    g_variant_get(result.get(), "(h)", &index);
    int fd = g_unix_fd_list_get(fdList.get(), index, &error.outPtr());
    if (fd == -1) {
        gst_printerrln("Unable to open pipewire remote. Error: %s", error->message);
        return { };
    }

    auto nodeData = PipeWireNodeData(0);
    nodeData.fd = fd;
    nodeData.path = path();
    nodeData.caps = adoptGRef(gst_caps_new_empty_simple("video/x-raw"));
    gst_caps_set_features_simple(nodeData.caps.get(), gst_caps_features_new("memory:DMABuf", nullptr));

#if GST_CHECK_VERSION(1, 24, 0)
    auto fourcc = gst_video_dma_drm_fourcc_from_format(GST_VIDEO_FORMAT_BGRA);
    GUniquePtr<char> drmFormat(gst_video_dma_drm_fourcc_to_string(fourcc, 0));
    gst_caps_set_simple(nodeData.caps.get(), "format", G_TYPE_STRING, "DMA_DRM", "drm-format", G_TYPE_STRING, drmFormat.get(), nullptr);
#endif
    return nodeData;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)

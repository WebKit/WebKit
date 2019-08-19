/*
 * Copyright (C) 2017 Igalia S.L.
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
#include "SessionHost.h"

#include "WebDriverService.h"
#include <gio/gio.h>
#include <wtf/RunLoop.h>
#include <wtf/UUID.h>
#include <wtf/glib/GUniquePtr.h>

#define REMOTE_INSPECTOR_CLIENT_DBUS_INTERFACE "org.webkit.RemoteInspectorClient"
#define REMOTE_INSPECTOR_CLIENT_OBJECT_PATH "/org/webkit/RemoteInspectorClient"
#define INSPECTOR_DBUS_INTERFACE "org.webkit.Inspector"
#define INSPECTOR_DBUS_OBJECT_PATH "/org/webkit/Inspector"

namespace WebDriver {

SessionHost::~SessionHost()
{
    if (m_dbusConnection)
        g_signal_handlers_disconnect_matched(m_dbusConnection.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
    g_cancellable_cancel(m_cancellable.get());
    if (m_browser)
        g_subprocess_force_exit(m_browser.get());
}

static const char introspectionXML[] =
    "<node>"
    "  <interface name='" REMOTE_INSPECTOR_CLIENT_DBUS_INTERFACE "'>"
    "    <method name='SetTargetList'>"
    "      <arg type='t' name='connectionID' direction='in'/>"
    "      <arg type='a(tsssb)' name='list' direction='in'/>"
    "    </method>"
    "    <method name='SendMessageToFrontend'>"
    "      <arg type='t' name='connectionID' direction='in'/>"
    "      <arg type='t' name='target' direction='in'/>"
    "      <arg type='s' name='message' direction='in'/>"
    "    </method>"
    "  </interface>"
    "</node>";

const GDBusInterfaceVTable SessionHost::s_interfaceVTable = {
    // method_call
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData) {
        auto* sessionHost = static_cast<SessionHost*>(userData);
        if (!g_strcmp0(methodName, "SetTargetList")) {
            guint64 connectionID;
            GUniqueOutPtr<GVariantIter> iter;
            g_variant_get(parameters, "(ta(tsssb))", &connectionID, &iter.outPtr());
            size_t targetCount = g_variant_iter_n_children(iter.get());
            Vector<SessionHost::Target> targetList;
            targetList.reserveInitialCapacity(targetCount);
            guint64 targetID;
            const char* type;
            const char* name;
            const char* dummy;
            gboolean isPaired;
            while (g_variant_iter_loop(iter.get(), "(t&s&s&sb)", &targetID, &type, &name, &dummy, &isPaired)) {
                if (!g_strcmp0(type, "Automation"))
                    targetList.uncheckedAppend({ targetID, name, static_cast<bool>(isPaired) });
            }
            sessionHost->setTargetList(connectionID, WTFMove(targetList));
            g_dbus_method_invocation_return_value(invocation, nullptr);
        } else if (!g_strcmp0(methodName, "SendMessageToFrontend")) {
            guint64 connectionID, targetID;
            const char* message;
            g_variant_get(parameters, "(tt&s)", &connectionID, &targetID, &message);
            sessionHost->sendMessageToFrontend(connectionID, targetID, message);
            g_dbus_method_invocation_return_value(invocation, nullptr);
        }
    },
    // get_property
    nullptr,
    // set_property
    nullptr,
    // padding
    { 0 }
};

void SessionHost::connectToBrowser(Function<void (Optional<String> error)>&& completionHandler)
{
    launchBrowser(WTFMove(completionHandler));
}

bool SessionHost::isConnected() const
{
    // Session is connected when launching or when dbus connection hasn't been closed.
    return m_browser && (!m_dbusConnection || !g_dbus_connection_is_closed(m_dbusConnection.get()));
}

struct ConnectToBrowserAsyncData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    ConnectToBrowserAsyncData(SessionHost* sessionHost, GUniquePtr<char>&& dbusAddress, GCancellable* cancellable, Function<void (Optional<String> error)>&& completionHandler)
        : sessionHost(sessionHost)
        , dbusAddress(WTFMove(dbusAddress))
        , cancellable(cancellable)
        , completionHandler(WTFMove(completionHandler))
    {
    }

    SessionHost* sessionHost;
    GUniquePtr<char> dbusAddress;
    GRefPtr<GCancellable> cancellable;
    Function<void (Optional<String> error)> completionHandler;
};

static guint16 freePort()
{
    GRefPtr<GSocket> socket = adoptGRef(g_socket_new(G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT, nullptr));
    GRefPtr<GInetAddress> loopbackAdress = adoptGRef(g_inet_address_new_loopback(G_SOCKET_FAMILY_IPV4));
    GRefPtr<GSocketAddress> address = adoptGRef(g_inet_socket_address_new(loopbackAdress.get(), 0));
    g_socket_bind(socket.get(), address.get(), FALSE, nullptr);
    g_socket_listen(socket.get(), nullptr);
    address = adoptGRef(g_socket_get_local_address(socket.get(), nullptr));
    g_socket_close(socket.get(), nullptr);
    return g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(address.get()));
}

void SessionHost::launchBrowser(Function<void (Optional<String> error)>&& completionHandler)
{
    m_cancellable = adoptGRef(g_cancellable_new());
    GRefPtr<GSubprocessLauncher> launcher = adoptGRef(g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_NONE));
    guint16 port = freePort();
    GUniquePtr<char> inspectorAddress(g_strdup_printf("127.0.0.1:%u", port));
    g_subprocess_launcher_setenv(launcher.get(), "WEBKIT_INSPECTOR_SERVER", inspectorAddress.get(), TRUE);
#if PLATFORM(GTK)
    g_subprocess_launcher_setenv(launcher.get(), "GTK_OVERLAY_SCROLLING", m_capabilities.useOverlayScrollbars.value() ? "1" : "0", TRUE);
#endif

    size_t browserArgumentsSize = m_capabilities.browserArguments ? m_capabilities.browserArguments->size() : 0;
    GUniquePtr<char*> args(g_new0(char*, browserArgumentsSize + 2));
    args.get()[0] = g_strdup(m_capabilities.browserBinary.value().utf8().data());
    for (unsigned i = 0; i < browserArgumentsSize; ++i)
        args.get()[i + 1] = g_strdup(m_capabilities.browserArguments.value()[i].utf8().data());

    GUniqueOutPtr<GError> error;
    m_browser = adoptGRef(g_subprocess_launcher_spawnv(launcher.get(), args.get(), &error.outPtr()));
    if (error) {
        completionHandler(String::fromUTF8(error->message));
        return;
    }

    g_subprocess_wait_async(m_browser.get(), m_cancellable.get(), [](GObject* browser, GAsyncResult* result, gpointer userData) {
        GUniqueOutPtr<GError> error;
        g_subprocess_wait_finish(G_SUBPROCESS(browser), result, &error.outPtr());
        if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
            return;
        auto* sessionHost = static_cast<SessionHost*>(userData);
        sessionHost->m_browser = nullptr;
    }, this);

    GUniquePtr<char> dbusAddress(g_strdup_printf("tcp:host=%s,port=%u", "127.0.0.1", port));
    connectToBrowser(makeUnique<ConnectToBrowserAsyncData>(this, WTFMove(dbusAddress), m_cancellable.get(), WTFMove(completionHandler)));
}

void SessionHost::connectToBrowser(std::unique_ptr<ConnectToBrowserAsyncData>&& data)
{
    if (!m_browser)
        return;

    RunLoop::main().dispatchAfter(100_ms, [connectToBrowserData = WTFMove(data)]() mutable {
        auto* data = connectToBrowserData.release();
        if (g_cancellable_is_cancelled(data->cancellable.get()))
            return;

        g_dbus_connection_new_for_address(data->dbusAddress.get(), G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT, nullptr, data->cancellable.get(),
            [](GObject*, GAsyncResult* result, gpointer userData) {
                auto data = std::unique_ptr<ConnectToBrowserAsyncData>(static_cast<ConnectToBrowserAsyncData*>(userData));
                GUniqueOutPtr<GError> error;
                GRefPtr<GDBusConnection> connection = adoptGRef(g_dbus_connection_new_for_address_finish(result, &error.outPtr()));
                if (!connection) {
                    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        return;

                    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CONNECTION_REFUSED)) {
                        data->sessionHost->connectToBrowser(WTFMove(data));
                        return;
                    }

                    data->completionHandler(String::fromUTF8(error->message));
                    return;
                }
                data->sessionHost->setupConnection(WTFMove(connection));
                data->completionHandler(WTF::nullopt);
        }, data);
    });
}

void SessionHost::dbusConnectionClosedCallback(SessionHost* sessionHost)
{
    sessionHost->m_browser = nullptr;
    sessionHost->inspectorDisconnected();
}

static void dbusConnectionCallAsyncReadyCallback(GObject* source, GAsyncResult* result, gpointer)
{
    GUniqueOutPtr<GError> error;
    GRefPtr<GVariant> resultVariant = adoptGRef(g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, &error.outPtr()));
    if (!resultVariant && !g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
        WTFLogAlways("RemoteInspectorServer failed to send DBus message: %s", error->message);
}

void SessionHost::setupConnection(GRefPtr<GDBusConnection>&& connection)
{
    ASSERT(!m_dbusConnection);
    ASSERT(connection);
    m_dbusConnection = WTFMove(connection);

    g_signal_connect_swapped(m_dbusConnection.get(), "closed", G_CALLBACK(dbusConnectionClosedCallback), this);

    static GDBusNodeInfo* introspectionData = nullptr;
    if (!introspectionData)
        introspectionData = g_dbus_node_info_new_for_xml(introspectionXML, nullptr);

    g_dbus_connection_register_object(m_dbusConnection.get(), REMOTE_INSPECTOR_CLIENT_OBJECT_PATH, introspectionData->interfaces[0], &s_interfaceVTable, this, nullptr, nullptr);
}

static bool matchBrowserOptions(const String& browserName, const String& browserVersion, const Capabilities& capabilities)
{
    if (capabilities.browserName && capabilities.browserName.value() != browserName)
        return false;

    if (capabilities.browserVersion && !WebDriverService::platformCompareBrowserVersions(capabilities.browserVersion.value(), browserVersion))
        return false;

    return true;
}

bool SessionHost::matchCapabilities(GVariant* capabilities)
{
    const char* name;
    const char* version;
    g_variant_get(capabilities, "(&s&s)", &name, &version);

    auto browserName = String::fromUTF8(name);
    auto browserVersion = String::fromUTF8(version);
    bool didMatch = matchBrowserOptions(browserName, browserVersion, m_capabilities);
    m_capabilities.browserName = browserName;
    m_capabilities.browserVersion = browserVersion;

    return didMatch;
}

bool SessionHost::buildSessionCapabilities(GVariantBuilder* builder) const
{
    if (!m_capabilities.acceptInsecureCerts && !m_capabilities.certificates)
        return false;

    g_variant_builder_init(builder, G_VARIANT_TYPE("a{sv}"));
    if (m_capabilities.acceptInsecureCerts)
        g_variant_builder_add(builder, "{sv}", "acceptInsecureCerts", g_variant_new_boolean(m_capabilities.acceptInsecureCerts.value()));

    if (m_capabilities.certificates) {
        GVariantBuilder arrayBuilder;
        g_variant_builder_init(&arrayBuilder, G_VARIANT_TYPE("a(ss)"));
        for (auto& certificate : *m_capabilities.certificates) {
            g_variant_builder_add_value(&arrayBuilder, g_variant_new("(ss)",
                certificate.first.utf8().data(), certificate.second.utf8().data()));
        }
        g_variant_builder_add(builder, "{sv}", "certificates", g_variant_builder_end(&arrayBuilder));
    }

    return true;
}

void SessionHost::startAutomationSession(Function<void (bool, Optional<String>)>&& completionHandler)
{
    ASSERT(m_dbusConnection);
    ASSERT(!m_startSessionCompletionHandler);
    m_startSessionCompletionHandler = WTFMove(completionHandler);
    m_sessionID = createCanonicalUUIDString();
    GVariantBuilder builder;
    g_dbus_connection_call(m_dbusConnection.get(), nullptr,
        INSPECTOR_DBUS_OBJECT_PATH,
        INSPECTOR_DBUS_INTERFACE,
        "StartAutomationSession",
        g_variant_new("(sa{sv})", m_sessionID.utf8().data(), buildSessionCapabilities(&builder) ? &builder : nullptr),
        nullptr, G_DBUS_CALL_FLAGS_NO_AUTO_START,
        -1, m_cancellable.get(), [](GObject* source, GAsyncResult* result, gpointer userData) {
            GUniqueOutPtr<GError> error;
            GRefPtr<GVariant> resultVariant = adoptGRef(g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, &error.outPtr()));
            if (!resultVariant && g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                return;

            auto sessionHost = static_cast<SessionHost*>(userData);
            if (!resultVariant) {
                auto completionHandler = std::exchange(sessionHost->m_startSessionCompletionHandler, nullptr);
                completionHandler(false, makeString("Failed to start automation session: ", String::fromUTF8(error->message)));
                return;
            }

            if (!sessionHost->matchCapabilities(resultVariant.get())) {
                auto completionHandler = std::exchange(sessionHost->m_startSessionCompletionHandler, nullptr);
                completionHandler(false, WTF::nullopt);
                return;
            }
        }, this
    );
}

void SessionHost::setTargetList(uint64_t connectionID, Vector<Target>&& targetList)
{
    // The server notifies all its clients when connection is lost by sending an empty target list.
    // We only care about automation connection.
    if (m_connectionID && m_connectionID != connectionID)
        return;

    ASSERT(targetList.size() <= 1);
    if (targetList.isEmpty()) {
        m_target = Target();
        if (m_connectionID) {
            if (m_dbusConnection)
                g_dbus_connection_close(m_dbusConnection.get(), nullptr, nullptr, nullptr);
            m_connectionID = 0;
        }
        return;
    }

    m_target = targetList[0];
    if (m_connectionID) {
        ASSERT(m_connectionID == connectionID);
        return;
    }

    if (!m_startSessionCompletionHandler) {
        // Session creation was already rejected.
        return;
    }

    m_connectionID = connectionID;
    g_dbus_connection_call(m_dbusConnection.get(), nullptr,
        INSPECTOR_DBUS_OBJECT_PATH,
        INSPECTOR_DBUS_INTERFACE,
        "Setup",
        g_variant_new("(tt)", m_connectionID, m_target.id),
        nullptr, G_DBUS_CALL_FLAGS_NO_AUTO_START,
        -1, m_cancellable.get(), dbusConnectionCallAsyncReadyCallback, nullptr);

    auto startSessionCompletionHandler = std::exchange(m_startSessionCompletionHandler, nullptr);
    startSessionCompletionHandler(true, WTF::nullopt);
}

void SessionHost::sendMessageToFrontend(uint64_t connectionID, uint64_t targetID, const char* message)
{
    if (connectionID != m_connectionID || targetID != m_target.id)
        return;
    dispatchMessage(String::fromUTF8(message));
}

struct MessageContext {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    long messageID;
    SessionHost* host;
};

void SessionHost::sendMessageToBackend(long messageID, const String& message)
{
    ASSERT(m_dbusConnection);
    ASSERT(m_connectionID);
    ASSERT(m_target.id);

    auto messageContext = makeUnique<MessageContext>(MessageContext { messageID, this });
    g_dbus_connection_call(m_dbusConnection.get(), nullptr,
        INSPECTOR_DBUS_OBJECT_PATH,
        INSPECTOR_DBUS_INTERFACE,
        "SendMessageToBackend",
        g_variant_new("(tts)", m_connectionID, m_target.id, message.utf8().data()),
        nullptr, G_DBUS_CALL_FLAGS_NO_AUTO_START,
        -1, m_cancellable.get(), [](GObject* source, GAsyncResult* result, gpointer userData) {
            auto messageContext = std::unique_ptr<MessageContext>(static_cast<MessageContext*>(userData));
            GUniqueOutPtr<GError> error;
            GRefPtr<GVariant> resultVariant = adoptGRef(g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, &error.outPtr()));
            if (!resultVariant && !g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
                auto responseHandler = messageContext->host->m_commandRequests.take(messageContext->messageID);
                if (responseHandler) {
                    auto errorObject = JSON::Object::create();
                    errorObject->setInteger("code"_s, -32603);
                    errorObject->setString("message"_s, String::fromUTF8(error->message));
                    responseHandler({ WTFMove(errorObject), true });
                }
            }
        }, messageContext.release());
}

} // namespace WebDriver

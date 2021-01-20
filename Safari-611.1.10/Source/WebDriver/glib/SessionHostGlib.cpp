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
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/UUID.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebDriver {

SessionHost::~SessionHost()
{
    g_cancellable_cancel(m_cancellable.get());
    if (m_socketConnection)
        m_socketConnection->close();
    if (m_browser)
        g_subprocess_force_exit(m_browser.get());
}

const SocketConnection::MessageHandlers& SessionHost::messageHandlers()
{
    static NeverDestroyed<const SocketConnection::MessageHandlers> messageHandlers = SocketConnection::MessageHandlers({
    { "DidClose", std::pair<CString, SocketConnection::MessageCallback> { { },
        [](SocketConnection&, GVariant*, gpointer userData) {
            auto& sessionHost = *static_cast<SessionHost*>(userData);
            sessionHost.connectionDidClose();
        }}
    },
    { "DidStartAutomationSession", std::pair<CString, SocketConnection::MessageCallback> { "(ss)",
        [](SocketConnection&, GVariant* parameters, gpointer userData) {
            auto& sessionHost = *static_cast<SessionHost*>(userData);
            sessionHost.didStartAutomationSession(parameters);
        }}
    },
    { "SetTargetList", std::pair<CString, SocketConnection::MessageCallback> { "(ta(tsssb))",
        [](SocketConnection&, GVariant* parameters, gpointer userData) {
            auto& sessionHost = *static_cast<SessionHost*>(userData);
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
            sessionHost.setTargetList(connectionID, WTFMove(targetList));
        }}
    },
    { "SendMessageToFrontend", std::pair<CString, SocketConnection::MessageCallback> { "(tts)",
        [](SocketConnection&, GVariant* parameters, gpointer userData) {
            auto& sessionHost = *static_cast<SessionHost*>(userData);
            guint64 connectionID, targetID;
            const char* message;
            g_variant_get(parameters, "(tt&s)", &connectionID, &targetID, &message);
            sessionHost.sendMessageToFrontend(connectionID, targetID, message);
        }}
    }
    });
    return messageHandlers;
}

void SessionHost::connectToBrowser(Function<void (Optional<String> error)>&& completionHandler)
{
    launchBrowser(WTFMove(completionHandler));
}

bool SessionHost::isConnected() const
{
    // Session is connected when launching or when socket connection hasn't been closed.
    return m_browser && (!m_socketConnection || !m_socketConnection->isClosed());
}

struct ConnectToBrowserAsyncData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    ConnectToBrowserAsyncData(SessionHost* sessionHost, GUniquePtr<char>&& inspectorAddress, GCancellable* cancellable, Function<void(Optional<String>)>&& completionHandler)
        : sessionHost(sessionHost)
        , inspectorAddress(WTFMove(inspectorAddress))
        , cancellable(cancellable)
        , completionHandler(WTFMove(completionHandler))
    {
    }

    SessionHost* sessionHost;
    GUniquePtr<char> inspectorAddress;
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

    connectToBrowser(makeUnique<ConnectToBrowserAsyncData>(this, WTFMove(inspectorAddress), m_cancellable.get(), WTFMove(completionHandler)));
}

void SessionHost::connectToBrowser(std::unique_ptr<ConnectToBrowserAsyncData>&& data)
{
    if (!m_browser)
        return;

    RunLoop::main().dispatchAfter(100_ms, [connectToBrowserData = WTFMove(data)]() mutable {
        auto* data = connectToBrowserData.release();
        if (g_cancellable_is_cancelled(data->cancellable.get()))
            return;

        GRefPtr<GSocketClient> socketClient = adoptGRef(g_socket_client_new());
        g_socket_client_connect_to_host_async(socketClient.get(), data->inspectorAddress.get(), 0, data->cancellable.get(),
            [](GObject* client, GAsyncResult* result, gpointer userData) {
                auto data = std::unique_ptr<ConnectToBrowserAsyncData>(static_cast<ConnectToBrowserAsyncData*>(userData));
                GUniqueOutPtr<GError> error;
                GRefPtr<GSocketConnection> connection = adoptGRef(g_socket_client_connect_to_host_finish(G_SOCKET_CLIENT(client), result, &error.outPtr()));
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
                data->sessionHost->setupConnection(SocketConnection::create(WTFMove(connection), messageHandlers(), data->sessionHost));
                data->completionHandler(WTF::nullopt);
        }, data);
    });
}

void SessionHost::connectionDidClose()
{
    m_browser = nullptr;
    inspectorDisconnected();
    m_socketConnection = nullptr;
}

void SessionHost::setupConnection(Ref<SocketConnection>&& connection)
{
    ASSERT(!m_socketConnection);
    m_socketConnection = WTFMove(connection);
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
    if (!m_capabilities.acceptInsecureCerts && !m_capabilities.certificates && !m_capabilities.proxy)
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

    if (m_capabilities.proxy) {
        GVariantBuilder dictBuilder;
        g_variant_builder_init(&dictBuilder, G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add(&dictBuilder, "{sv}", "type", g_variant_new_string(m_capabilities.proxy->type.utf8().data()));
        if (m_capabilities.proxy->ftpURL)
            g_variant_builder_add(&dictBuilder, "{sv}", "ftpURL", g_variant_new_string(m_capabilities.proxy->ftpURL->string().utf8().data()));
        if (m_capabilities.proxy->httpURL)
            g_variant_builder_add(&dictBuilder, "{sv}", "httpURL", g_variant_new_string(m_capabilities.proxy->httpURL->string().utf8().data()));
        if (m_capabilities.proxy->httpsURL)
            g_variant_builder_add(&dictBuilder, "{sv}", "httpsURL", g_variant_new_string(m_capabilities.proxy->httpsURL->string().utf8().data()));
        if (m_capabilities.proxy->socksURL) {
            URL socksURL = m_capabilities.proxy->socksURL.value();
            ASSERT(m_capabilities.proxy->socksVersion);
            switch (m_capabilities.proxy->socksVersion.value()) {
            case 4:
                if (URL::hostIsIPAddress(socksURL.host()))
                    socksURL.setProtocol("socks4");
                else
                    socksURL.setProtocol("socks4a");
                break;
            case 5:
                socksURL.setProtocol("socks5");
                break;
            default:
                break;
            }
            g_variant_builder_add(&dictBuilder, "{sv}", "socksURL", g_variant_new_string(socksURL.string().utf8().data()));
        }
        if (!m_capabilities.proxy->ignoreAddressList.isEmpty()) {
            GUniquePtr<char*> ignoreAddressList(static_cast<char**>(g_new0(char*, m_capabilities.proxy->ignoreAddressList.size() + 1)));
            unsigned i = 0;
            for (const auto& ignoreAddress : m_capabilities.proxy->ignoreAddressList)
                ignoreAddressList.get()[i++] = g_strdup(ignoreAddress.utf8().data());
            g_variant_builder_add(&dictBuilder, "{sv}", "ignoreAddressList", g_variant_new_strv(ignoreAddressList.get(), -1));
        }
        g_variant_builder_add(builder, "{sv}", "proxy", g_variant_builder_end(&dictBuilder));
    }

    return true;
}

void SessionHost::startAutomationSession(Function<void (bool, Optional<String>)>&& completionHandler)
{
    ASSERT(m_socketConnection);
    ASSERT(!m_startSessionCompletionHandler);
    m_startSessionCompletionHandler = WTFMove(completionHandler);
    m_sessionID = createCanonicalUUIDString();
    GVariantBuilder builder;
    m_socketConnection->sendMessage("StartAutomationSession", g_variant_new("(sa{sv})", m_sessionID.utf8().data(), buildSessionCapabilities(&builder) ? &builder : nullptr));
}

void SessionHost::didStartAutomationSession(GVariant* parameters)
{
    if (matchCapabilities(parameters))
        return;

    auto completionHandler = std::exchange(m_startSessionCompletionHandler, nullptr);
    completionHandler(false, WTF::nullopt);
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
            if (m_socketConnection)
                m_socketConnection->close();
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
    m_socketConnection->sendMessage("Setup", g_variant_new("(tt)", m_connectionID, m_target.id));

    auto startSessionCompletionHandler = std::exchange(m_startSessionCompletionHandler, nullptr);
    startSessionCompletionHandler(true, WTF::nullopt);
}

void SessionHost::sendMessageToFrontend(uint64_t connectionID, uint64_t targetID, const char* message)
{
    if (connectionID != m_connectionID || targetID != m_target.id)
        return;
    dispatchMessage(String::fromUTF8(message));
}

void SessionHost::sendMessageToBackend(const String& message)
{
    ASSERT(m_socketConnection);
    ASSERT(m_connectionID);
    ASSERT(m_target.id);
    m_socketConnection->sendMessage("SendMessageToBackend", g_variant_new("(tts)", m_connectionID, m_target.id, message.utf8().data()));
}

} // namespace WebDriver

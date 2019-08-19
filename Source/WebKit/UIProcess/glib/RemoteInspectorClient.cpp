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
#include "RemoteInspectorClient.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "RemoteWebInspectorProxy.h"
#include <JavaScriptCore/RemoteInspectorUtils.h>
#include <gio/gio.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/Base64.h>

#define REMOTE_INSPECTOR_CLIENT_DBUS_INTERFACE "org.webkit.RemoteInspectorClient"
#define REMOTE_INSPECTOR_CLIENT_OBJECT_PATH "/org/webkit/RemoteInspectorClient"
#define INSPECTOR_DBUS_INTERFACE "org.webkit.Inspector"
#define INSPECTOR_DBUS_OBJECT_PATH "/org/webkit/Inspector"

namespace WebKit {

class RemoteInspectorProxy final : public RemoteWebInspectorProxyClient {
    WTF_MAKE_FAST_ALLOCATED();
public:
    RemoteInspectorProxy(RemoteInspectorClient& inspectorClient, uint64_t connectionID, uint64_t targetID)
        : m_proxy(RemoteWebInspectorProxy::create())
        , m_inspectorClient(inspectorClient)
        , m_connectionID(connectionID)
        , m_targetID(targetID)
    {
        m_proxy->setClient(this);
    }

    ~RemoteInspectorProxy()
    {
        m_proxy->setClient(nullptr);
        m_proxy->invalidate();
    }

    void load()
    {
        m_proxy->load("web", m_inspectorClient.backendCommandsURL());
    }

    void show()
    {
        m_proxy->show();
    }

    void setTargetName(const CString& name)
    {
#if PLATFORM(GTK)
        m_proxy->updateWindowTitle(name);
#endif
    }

    void sendMessageToFrontend(const String& message)
    {
        m_proxy->sendMessageToFrontend(message);
    }

    void sendMessageToBackend(const String& message) override
    {
        m_inspectorClient.sendMessageToBackend(m_connectionID, m_targetID, message);
    }

    void closeFromFrontend() override
    {
        m_inspectorClient.closeFromFrontend(m_connectionID, m_targetID);
    }

private:
    Ref<RemoteWebInspectorProxy> m_proxy;
    RemoteInspectorClient& m_inspectorClient;
    uint64_t m_connectionID;
    uint64_t m_targetID;
};

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

const GDBusInterfaceVTable RemoteInspectorClient::s_interfaceVTable = {
    // method_call
    [](GDBusConnection* connection, const gchar* sender, const gchar* objectPath, const gchar* interfaceName, const gchar* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData) {
        auto* client = static_cast<RemoteInspectorClient*>(userData);
        if (!g_strcmp0(methodName, "SetTargetList")) {
            guint64 connectionID;
            GUniqueOutPtr<GVariantIter> iter;
            g_variant_get(parameters, "(ta(tsssb))", &connectionID, &iter.outPtr());
            size_t targetCount = g_variant_iter_n_children(iter.get());
            Vector<RemoteInspectorClient::Target> targetList;
            targetList.reserveInitialCapacity(targetCount);
            guint64 targetID;
            const char* type;
            const char* name;
            const char* url;
            gboolean hasLocalDebugger;
            while (g_variant_iter_loop(iter.get(), "(t&s&s&sb)", &targetID, &type, &name, &url, &hasLocalDebugger)) {
                if (!g_strcmp0(type, "Web") || !g_strcmp0(type, "JavaScript"))
                    targetList.uncheckedAppend({ targetID, type, name, url });
            }
            client->setTargetList(connectionID, WTFMove(targetList));
            g_dbus_method_invocation_return_value(invocation, nullptr);
        } else if (!g_strcmp0(methodName, "SendMessageToFrontend")) {
            guint64 connectionID, targetID;
            const char* message;
            g_variant_get(parameters, "(tt&s)", &connectionID, &targetID, &message);
            client->sendMessageToFrontend(connectionID, targetID, message);
            g_dbus_method_invocation_return_value(invocation, nullptr);
        } else
            g_dbus_method_invocation_return_value(invocation, nullptr);
    },
    // get_property
    nullptr,
    // set_property
    nullptr,
    // padding
    { 0 }
};

RemoteInspectorClient::RemoteInspectorClient(const char* address, unsigned port, RemoteInspectorObserver& observer)
    : m_hostAndPort(String::fromUTF8(address) + ':' + String::number(port))
    , m_observer(observer)
    , m_cancellable(adoptGRef(g_cancellable_new()))
{
    GUniquePtr<char> dbusAddress(g_strdup_printf("tcp:host=%s,port=%u", address, port));
    g_dbus_connection_new_for_address(dbusAddress.get(), G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT, nullptr, m_cancellable.get(),
        [](GObject*, GAsyncResult* result, gpointer userData) {
            auto* client = static_cast<RemoteInspectorClient*>(userData);
            GUniqueOutPtr<GError> error;
            GRefPtr<GDBusConnection> connection = adoptGRef(g_dbus_connection_new_for_address_finish(result, &error.outPtr()));
            if (!connection && !g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                WTFLogAlways("RemoteInspectorClient failed to connect to inspector server: %s", error->message);
            client->setupConnection(WTFMove(connection));
    }, this);
}

RemoteInspectorClient::~RemoteInspectorClient()
{
}

static void dbusConnectionCallAsyncReadyCallback(GObject* source, GAsyncResult* result, gpointer)
{
    GUniqueOutPtr<GError> error;
    GRefPtr<GVariant> resultVariant = adoptGRef(g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, &error.outPtr()));
    if (!resultVariant && !g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
        WTFLogAlways("RemoteInspectorClient failed to send DBus message: %s", error->message);
}

void RemoteInspectorClient::connectionClosedCallback(GDBusConnection* connection, gboolean /*remotePeerVanished*/, GError*, RemoteInspectorClient* client)
{
    ASSERT_UNUSED(connection, client->m_dbusConnection.get() == connection);
    client->connectionClosed();
}

void RemoteInspectorClient::setupConnection(GRefPtr<GDBusConnection>&& connection)
{
    m_dbusConnection = WTFMove(connection);
    if (!m_dbusConnection) {
        m_observer.connectionClosed(*this);
        return;
    }
    g_signal_connect(m_dbusConnection.get(), "closed", G_CALLBACK(connectionClosedCallback), this);

    static GDBusNodeInfo* introspectionData = nullptr;
    if (!introspectionData)
        introspectionData = g_dbus_node_info_new_for_xml(introspectionXML, nullptr);

    g_dbus_connection_register_object(m_dbusConnection.get(), REMOTE_INSPECTOR_CLIENT_OBJECT_PATH, introspectionData->interfaces[0], &s_interfaceVTable, this, nullptr, nullptr);

    g_dbus_connection_call(m_dbusConnection.get(), nullptr,
        INSPECTOR_DBUS_OBJECT_PATH,
        INSPECTOR_DBUS_INTERFACE,
        "SetupInspectorClient",
        g_variant_new("(@ay)", g_variant_new_bytestring(Inspector::backendCommandsHash().data())),
        nullptr, G_DBUS_CALL_FLAGS_NO_AUTO_START,
        -1, m_cancellable.get(), [](GObject* source, GAsyncResult* result, gpointer userData) {
            GUniqueOutPtr<GError> error;
            GRefPtr<GVariant> resultVariant = adoptGRef(g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, &error.outPtr()));
            if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                return;
            if (!resultVariant) {
                WTFLogAlways("RemoteInspectorClient failed to send DBus message: %s", error->message);
                return;
            }

            auto* client = static_cast<RemoteInspectorClient*>(userData);
            GRefPtr<GVariant> backendCommandsVariant;
            g_variant_get(resultVariant.get(), "(@ay)", &backendCommandsVariant.outPtr());
            client->setBackendCommands(g_variant_get_bytestring(backendCommandsVariant.get()));
        }, this);
}

void RemoteInspectorClient::setBackendCommands(const char* backendCommands)
{
    if (!backendCommands || !backendCommands[0]) {
        m_backendCommandsURL = String();
        return;
    }

    Vector<char> base64Data;
    base64Encode(backendCommands, strlen(backendCommands), base64Data);
    m_backendCommandsURL = "data:text/javascript;base64," + base64Data;
}

void RemoteInspectorClient::connectionClosed()
{
    g_cancellable_cancel(m_cancellable.get());
    m_targets.clear();
    m_inspectorProxyMap.clear();
    m_dbusConnection = nullptr;
    m_observer.connectionClosed(*this);
}

void RemoteInspectorClient::inspect(uint64_t connectionID, uint64_t targetID)
{
    auto addResult = m_inspectorProxyMap.ensure(std::make_pair(connectionID, targetID), [this, connectionID, targetID] {
        return makeUnique<RemoteInspectorProxy>(*this, connectionID, targetID);
    });
    if (!addResult.isNewEntry) {
        addResult.iterator->value->show();
        return;
    }

    g_dbus_connection_call(m_dbusConnection.get(), nullptr,
        INSPECTOR_DBUS_OBJECT_PATH,
        INSPECTOR_DBUS_INTERFACE,
        "Setup",
        g_variant_new("(tt)", connectionID, targetID),
        nullptr, G_DBUS_CALL_FLAGS_NO_AUTO_START,
        -1, m_cancellable.get(), dbusConnectionCallAsyncReadyCallback, nullptr);

    addResult.iterator->value->load();
}

void RemoteInspectorClient::sendMessageToBackend(uint64_t connectionID, uint64_t targetID, const String& message)
{
    g_dbus_connection_call(m_dbusConnection.get(), nullptr,
        INSPECTOR_DBUS_OBJECT_PATH,
        INSPECTOR_DBUS_INTERFACE,
        "SendMessageToBackend",
        g_variant_new("(tts)", connectionID, targetID, message.utf8().data()),
        nullptr, G_DBUS_CALL_FLAGS_NO_AUTO_START,
        -1, m_cancellable.get(), dbusConnectionCallAsyncReadyCallback, nullptr);
}

void RemoteInspectorClient::closeFromFrontend(uint64_t connectionID, uint64_t targetID)
{
    ASSERT(m_inspectorProxyMap.contains(std::make_pair(connectionID, targetID)));
    g_dbus_connection_call(m_dbusConnection.get(), nullptr,
        INSPECTOR_DBUS_OBJECT_PATH,
        INSPECTOR_DBUS_INTERFACE,
        "FrontendDidClose",
        g_variant_new("(tt)", connectionID, targetID),
        nullptr, G_DBUS_CALL_FLAGS_NO_AUTO_START,
        -1, m_cancellable.get(), dbusConnectionCallAsyncReadyCallback, nullptr);
    m_inspectorProxyMap.remove(std::make_pair(connectionID, targetID));
}

void RemoteInspectorClient::setTargetList(uint64_t connectionID, Vector<Target>&& targetList)
{
    // Find closed targets to remove them.
    Vector<uint64_t, 4> targetsToRemove;
    for (auto& connectionTargetPair : m_inspectorProxyMap.keys()) {
        if (connectionTargetPair.first != connectionID)
            continue;
        bool found = false;
        for (const auto& target : targetList) {
            if (target.id == connectionTargetPair.second) {
                m_inspectorProxyMap.get(connectionTargetPair)->setTargetName(target.name);
                found = true;
                break;
            }
        }
        if (!found)
            targetsToRemove.append(connectionTargetPair.second);
    }
    for (auto targetID : targetsToRemove)
        m_inspectorProxyMap.remove(std::make_pair(connectionID, targetID));

    m_targets.set(connectionID, WTFMove(targetList));
    m_observer.targetListChanged(*this);
}

void RemoteInspectorClient::sendMessageToFrontend(uint64_t connectionID, uint64_t targetID, const char* message)
{
    auto proxy = m_inspectorProxyMap.get(std::make_pair(connectionID, targetID));
    ASSERT(proxy);
    if (!proxy)
        return;
    proxy->sendMessageToFrontend(String::fromUTF8(message));
}

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)

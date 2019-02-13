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
#include "RemoteInspectorServer.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "RemoteInspectorUtils.h"
#include <gio/gio.h>
#include <wtf/Vector.h>
#include <wtf/glib/GUniquePtr.h>

#define INSPECTOR_DBUS_INTERFACE "org.webkit.Inspector"
#define INSPECTOR_DBUS_OBJECT_PATH "/org/webkit/Inspector"
#define REMOTE_INSPECTOR_DBUS_INTERFACE "org.webkit.RemoteInspector"
#define REMOTE_INSPECTOR_DBUS_OBJECT_PATH "/org/webkit/RemoteInspector"
#define REMOTE_INSPECTOR_CLIENT_DBUS_INTERFACE "org.webkit.RemoteInspectorClient"
#define REMOTE_INSPECTOR_CLIENT_OBJECT_PATH "/org/webkit/RemoteInspectorClient"

namespace Inspector {

static uint64_t generateConnectionID()
{
    static uint64_t connectionID = 0;
    return ++connectionID;
}

namespace RemoteInspectorServerInternal {
static const char introspectionXML[] =
    "<node>"
    "  <interface name='" INSPECTOR_DBUS_INTERFACE "'>"
    "    <method name='SetTargetList'>"
    "      <arg type='a(tsssb)' name='list' direction='in'/>"
    "      <arg type='b' name='remoteAutomationEnabled' direction='in'/>"
    "    </method>"
    "    <method name='SetupInspectorClient'>"
    "      <arg type='ay' name='backendCommandsHash' direction='in'/>"
    "      <arg type='ay' name='backendCommands' direction='out'/>"
    "    </method>"
    "    <method name='Setup'>"
    "      <arg type='t' name='connection' direction='in'/>"
    "      <arg type='t' name='target' direction='in'/>"
    "    </method>"
    "    <method name='FrontendDidClose'>"
    "      <arg type='t' name='connection' direction='in'/>"
    "      <arg type='t' name='target' direction='in'/>"
    "    </method>"
    "    <method name='SendMessageToFrontend'>"
    "      <arg type='t' name='target' direction='in'/>"
    "      <arg type='s' name='message' direction='in'/>"
    "    </method>"
    "    <method name='SendMessageToBackend'>"
    "      <arg type='t' name='connection' direction='in'/>"
    "      <arg type='t' name='target' direction='in'/>"
    "      <arg type='s' name='message' direction='in'/>"
    "    </method>"
    "    <method name='StartAutomationSession'>"
    "      <arg type='s' name='sessionID' direction='in'/>"
    "      <arg type='a{sv}' name='capabilities' direction='in'/>"
    "      <arg type='s' name='browserName' direction='out'/>"
    "      <arg type='s' name='browserVersion' direction='out'/>"
    "    </method>"
    "  </interface>"
    "</node>";
}

static RemoteInspector::Client::SessionCapabilities processSessionCapabilities(GVariant* sessionCapabilities)
{
    RemoteInspector::Client::SessionCapabilities capabilities;

    gboolean acceptInsecureCerts;
    if (g_variant_lookup(sessionCapabilities, "acceptInsecureCerts", "b", &acceptInsecureCerts))
        capabilities.acceptInsecureCertificates = acceptInsecureCerts;

    if (GRefPtr<GVariant> certificates = g_variant_lookup_value(sessionCapabilities, "certificates",  G_VARIANT_TYPE("a(ss)"))) {
        GVariantIter iter;
        auto childCount = g_variant_iter_init(&iter, certificates.get());
        capabilities.certificates.reserveCapacity(childCount);
        const char* host;
        const char* certificateFile;
        while (g_variant_iter_loop(&iter, "(&s&s)", &host, &certificateFile))
            capabilities.certificates.uncheckedAppend({ String::fromUTF8(host), String::fromUTF8(certificateFile) });
    }

    return capabilities;
}
const GDBusInterfaceVTable RemoteInspectorServer::s_interfaceVTable = {
    // method_call
    [] (GDBusConnection* connection, const gchar* /*sender*/, const gchar* /*objectPath*/, const gchar* /*interfaceName*/, const gchar* methodName, GVariant* parameters, GDBusMethodInvocation* invocation, gpointer userData) {
        auto* inspectorServer = static_cast<RemoteInspectorServer*>(userData);
        if (!g_strcmp0(methodName, "SetTargetList")) {
            inspectorServer->setTargetList(connection, parameters);
            g_dbus_method_invocation_return_value(invocation, nullptr);
        } else if (!g_strcmp0(methodName, "SetupInspectorClient")) {
            GRefPtr<GVariant> backendCommandsHash;
            g_variant_get(parameters, "(@ay)", &backendCommandsHash.outPtr());
            auto* backendCommands = inspectorServer->setupInspectorClient(connection, g_variant_get_bytestring(backendCommandsHash.get()));
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(@ay)", backendCommands));
        } else if (!g_strcmp0(methodName, "Setup")) {
            guint64 connectionID, targetID;
            g_variant_get(parameters, "(tt)", &connectionID, &targetID);
            inspectorServer->setup(connection, connectionID, targetID);
            g_dbus_method_invocation_return_value(invocation, nullptr);
        } else if (!g_strcmp0(methodName, "FrontendDidClose")) {
            guint64 connectionID, targetID;
            g_variant_get(parameters, "(tt)", &connectionID, &targetID);
            inspectorServer->close(connection, connectionID, targetID);
            g_dbus_method_invocation_return_value(invocation, nullptr);
        } else if (!g_strcmp0(methodName, "SendMessageToFrontend")) {
            guint64 targetID;
            const char* message;
            g_variant_get(parameters, "(t&s)", &targetID, &message);
            inspectorServer->sendMessageToFrontend(connection, targetID, message);
            g_dbus_method_invocation_return_value(invocation, nullptr);
        } else if (!g_strcmp0(methodName, "SendMessageToBackend")) {
            guint64 connectionID, targetID;
            const char* message;
            g_variant_get(parameters, "(tt&s)", &connectionID, &targetID, &message);
            inspectorServer->sendMessageToBackend(connection, connectionID, targetID, message);
            g_dbus_method_invocation_return_value(invocation, nullptr);
        } else if (!g_strcmp0(methodName, "StartAutomationSession")) {
            const char* sessionID;
            GRefPtr<GVariant> sessionCapabilities;
            g_variant_get(parameters, "(&s@a{sv})", &sessionID, &sessionCapabilities.outPtr());
            auto capabilities = processSessionCapabilities(sessionCapabilities.get());
            inspectorServer->startAutomationSession(connection, sessionID, capabilities);
            auto clientCapabilities = RemoteInspector::singleton().clientCapabilities();
            g_dbus_method_invocation_return_value(invocation, g_variant_new("(ss)",
                clientCapabilities ? clientCapabilities->browserName.utf8().data() : "",
                clientCapabilities ? clientCapabilities->browserVersion.utf8().data() : ""));
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

RemoteInspectorServer& RemoteInspectorServer::singleton()
{
    static RemoteInspectorServer server;
    return server;
}

RemoteInspectorServer::~RemoteInspectorServer()
{
    g_cancellable_cancel(m_cancellable.get());
    if (m_dbusServer)
        g_dbus_server_stop(m_dbusServer.get());
    if (m_introspectionData)
        g_dbus_node_info_unref(m_introspectionData);
}

GDBusInterfaceInfo* RemoteInspectorServer::interfaceInfo()
{
    if (!m_introspectionData) {
        m_introspectionData = g_dbus_node_info_new_for_xml(RemoteInspectorServerInternal::introspectionXML, nullptr);
        ASSERT(m_introspectionData);
    }
    return m_introspectionData->interfaces[0];
}

bool RemoteInspectorServer::start(const char* address, unsigned port)
{
    GUniquePtr<char> dbusAddress(g_strdup_printf("tcp:host=%s,port=%u", address, port));
    GUniquePtr<char> uid(g_dbus_generate_guid());

    m_cancellable = adoptGRef(g_cancellable_new());

    GUniqueOutPtr<GError> error;
    m_dbusServer = adoptGRef(g_dbus_server_new_sync(dbusAddress.get(), G_DBUS_SERVER_FLAGS_AUTHENTICATION_ALLOW_ANONYMOUS, uid.get(), nullptr, m_cancellable.get(), &error.outPtr()));
    if (!m_dbusServer) {
        if (!g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_warning("Failed to start remote inspector server on %s: %s\n", dbusAddress.get(), error->message);
        return false;
    }

    g_signal_connect(m_dbusServer.get(), "new-connection", G_CALLBACK(newConnectionCallback), this);
    g_dbus_server_start(m_dbusServer.get());

    return true;
}

gboolean RemoteInspectorServer::newConnectionCallback(GDBusServer*, GDBusConnection* connection, RemoteInspectorServer* inspectorServer)
{
    inspectorServer->newConnection(connection);
    return TRUE;
}

void RemoteInspectorServer::connectionClosedCallback(GDBusConnection* connection, gboolean /*remotePeerVanished*/, GError*, RemoteInspectorServer* server)
{
    server->connectionClosed(connection);
}

void RemoteInspectorServer::newConnection(GDBusConnection* connection)
{
    ASSERT(!m_connections.contains(connection));
    m_connections.add(connection);
    g_signal_connect(connection, "closed", G_CALLBACK(connectionClosedCallback), this);

    g_dbus_connection_register_object(connection, INSPECTOR_DBUS_OBJECT_PATH, interfaceInfo(), &s_interfaceVTable, this, nullptr, nullptr);
}

namespace RemoteInspectorServerInternal {
static void dbusConnectionCallAsyncReadyCallback(GObject* source, GAsyncResult* result, gpointer)
{
    GUniqueOutPtr<GError> error;
    GRefPtr<GVariant> resultVariant = adoptGRef(g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, &error.outPtr()));
    if (!resultVariant && !g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
        WTFLogAlways("RemoteInspectorServer failed to send DBus message: %s", error->message);
}
}

void RemoteInspectorServer::setTargetList(GDBusConnection* remoteInspectorConnection, GVariant* parameters)
{
    ASSERT(m_connections.contains(remoteInspectorConnection));
    auto addResult = m_remoteInspectorConnectionToIDMap.add(remoteInspectorConnection, 0);
    if (addResult.isNewEntry) {
        addResult.iterator->value = generateConnectionID();
        m_idToRemoteInspectorConnectionMap.add(addResult.iterator->value, remoteInspectorConnection);
    }

    gboolean remoteAutomationEnabled;
    GRefPtr<GVariant> targetList;
    g_variant_get(parameters, "(@a(tsssb)b)", &targetList.outPtr(), &remoteAutomationEnabled);
    GDBusConnection* clientConnection = remoteAutomationEnabled && m_automationConnection ? m_automationConnection.get() : m_clientConnection.get();
    if (!clientConnection)
        return;

    g_dbus_connection_call(clientConnection, nullptr,
        REMOTE_INSPECTOR_CLIENT_OBJECT_PATH,
        REMOTE_INSPECTOR_CLIENT_DBUS_INTERFACE,
        "SetTargetList",
        g_variant_new("(t@a(tsssb))", m_remoteInspectorConnectionToIDMap.get(remoteInspectorConnection), targetList.get()),
        nullptr, G_DBUS_CALL_FLAGS_NO_AUTO_START,
        -1, m_cancellable.get(), RemoteInspectorServerInternal::dbusConnectionCallAsyncReadyCallback, nullptr);
}

void RemoteInspectorServer::clientConnectionClosedCallback(GDBusConnection* connection, gboolean /*remotePeerVanished*/, GError*, RemoteInspectorServer* server)
{
    server->clientConnectionClosed(connection);
}

GVariant* RemoteInspectorServer::setupInspectorClient(GDBusConnection* clientConnection, const char* clientBackendCommandsHash)
{
    ASSERT(!m_clientConnection);
    m_clientConnection = clientConnection;
    g_signal_connect(m_clientConnection.get(), "closed", G_CALLBACK(clientConnectionClosedCallback), this);

    GVariant* backendCommands;
    if (strcmp(clientBackendCommandsHash, backendCommandsHash().data())) {
        auto bytes = Inspector::backendCommands();
        backendCommands = g_variant_new_bytestring(static_cast<const char*>(g_bytes_get_data(bytes.get(), nullptr)));
    } else
        backendCommands = g_variant_new_bytestring("");

    // Ask all remote inspectors to push their target lists to notify the new client.
    for (auto* remoteInspectorConnection : m_remoteInspectorConnectionToIDMap.keys()) {
        g_dbus_connection_call(remoteInspectorConnection, nullptr,
            REMOTE_INSPECTOR_DBUS_OBJECT_PATH,
            REMOTE_INSPECTOR_DBUS_INTERFACE,
            "GetTargetList",
            nullptr,
            nullptr, G_DBUS_CALL_FLAGS_NO_AUTO_START,
            -1, m_cancellable.get(), RemoteInspectorServerInternal::dbusConnectionCallAsyncReadyCallback, nullptr);
    }

    return backendCommands;
}

void RemoteInspectorServer::setup(GDBusConnection* clientConnection, uint64_t connectionID, uint64_t targetID)
{
    ASSERT(m_clientConnection.get() == clientConnection || m_automationConnection.get() == clientConnection);
    ASSERT(m_idToRemoteInspectorConnectionMap.contains(connectionID));
    if (clientConnection == m_automationConnection.get()) {
        m_automationTargets.add(std::make_pair(connectionID, targetID));
        RemoteInspector::singleton().setup(targetID);
        return;
    }

    m_inspectionTargets.add(std::make_pair(connectionID, targetID));
    auto* remoteInspectorConnection = m_idToRemoteInspectorConnectionMap.get(connectionID);
    g_dbus_connection_call(remoteInspectorConnection, nullptr,
        REMOTE_INSPECTOR_DBUS_OBJECT_PATH,
        REMOTE_INSPECTOR_DBUS_INTERFACE,
        "Setup",
        g_variant_new("(t)", targetID),
        nullptr, G_DBUS_CALL_FLAGS_NO_AUTO_START,
        -1, m_cancellable.get(), RemoteInspectorServerInternal::dbusConnectionCallAsyncReadyCallback, nullptr);
}

void RemoteInspectorServer::close(GDBusConnection* clientConnection, uint64_t connectionID, uint64_t targetID)
{
    ASSERT(m_clientConnection.get() == clientConnection || m_automationConnection.get() == clientConnection);
    ASSERT(m_idToRemoteInspectorConnectionMap.contains(connectionID));
    if (clientConnection == m_automationConnection.get()) {
        // FIXME: automation.
        return;
    }

    ASSERT(m_inspectionTargets.contains(std::make_pair(connectionID, targetID)));
    auto* remoteInspectorConnection = m_idToRemoteInspectorConnectionMap.get(connectionID);
    g_dbus_connection_call(remoteInspectorConnection, nullptr,
        REMOTE_INSPECTOR_DBUS_OBJECT_PATH,
        REMOTE_INSPECTOR_DBUS_INTERFACE,
        "FrontendDidClose",
        g_variant_new("(t)", targetID),
        nullptr, G_DBUS_CALL_FLAGS_NO_AUTO_START,
        -1, m_cancellable.get(), RemoteInspectorServerInternal::dbusConnectionCallAsyncReadyCallback, nullptr);
    m_inspectionTargets.remove(std::make_pair(connectionID, targetID));
}

void RemoteInspectorServer::clientConnectionClosed(GDBusConnection* clientConnection)
{
    ASSERT(m_clientConnection.get() == clientConnection || m_automationConnection.get() == clientConnection);
    if (clientConnection == m_automationConnection.get()) {
        for (auto connectionTargetPair : m_automationTargets)
            close(clientConnection, connectionTargetPair.first, connectionTargetPair.second);
        m_automationConnection = nullptr;
        return;
    }

    for (auto connectionTargetPair : m_inspectionTargets)
        close(clientConnection, connectionTargetPair.first, connectionTargetPair.second);
    m_clientConnection = nullptr;
}

void RemoteInspectorServer::connectionClosed(GDBusConnection* remoteInspectorConnection)
{
    ASSERT(m_connections.contains(remoteInspectorConnection));
    if (m_remoteInspectorConnectionToIDMap.contains(remoteInspectorConnection)) {
        uint64_t connectionID = m_remoteInspectorConnectionToIDMap.take(remoteInspectorConnection);
        m_idToRemoteInspectorConnectionMap.remove(connectionID);
        // Send an empty target list to the clients.
        Vector<GRefPtr<GDBusConnection>> clientConnections = { m_automationConnection, m_clientConnection };
        for (auto& clientConnection : clientConnections) {
            if (!clientConnection)
                continue;
            g_dbus_connection_call(clientConnection.get(), nullptr,
                REMOTE_INSPECTOR_CLIENT_OBJECT_PATH,
                REMOTE_INSPECTOR_CLIENT_DBUS_INTERFACE,
                "SetTargetList",
                g_variant_new("(t@a(tsssb))", connectionID, g_variant_new_array(G_VARIANT_TYPE("(tsssb)"), nullptr, 0)),
                nullptr, G_DBUS_CALL_FLAGS_NO_AUTO_START,
                -1, m_cancellable.get(), RemoteInspectorServerInternal::dbusConnectionCallAsyncReadyCallback, nullptr);
        }
    }
    m_connections.remove(remoteInspectorConnection);
}

void RemoteInspectorServer::sendMessageToBackend(GDBusConnection* clientConnection, uint64_t connectionID, uint64_t targetID, const char* message)
{
    ASSERT(m_clientConnection.get() == clientConnection || m_automationConnection.get() == clientConnection);
    ASSERT(m_idToRemoteInspectorConnectionMap.contains(connectionID));
    if (clientConnection == m_automationConnection.get()) {
        RemoteInspector::singleton().sendMessageToTarget(targetID, message);
        return;
    }

    auto* remoteInspectorConnection = m_idToRemoteInspectorConnectionMap.get(connectionID);
    g_dbus_connection_call(remoteInspectorConnection, nullptr,
        REMOTE_INSPECTOR_DBUS_OBJECT_PATH,
        REMOTE_INSPECTOR_DBUS_INTERFACE,
        "SendMessageToTarget",
        g_variant_new("(t&s)", targetID, message),
        nullptr, G_DBUS_CALL_FLAGS_NO_AUTO_START,
        -1, m_cancellable.get(), RemoteInspectorServerInternal::dbusConnectionCallAsyncReadyCallback, nullptr);
}

void RemoteInspectorServer::sendMessageToFrontend(GDBusConnection* remoteInspectorConnection, uint64_t targetID, const char* message)
{
    ASSERT(m_connections.contains(remoteInspectorConnection));
    ASSERT(m_remoteInspectorConnectionToIDMap.contains(remoteInspectorConnection));

    uint64_t connectionID = m_remoteInspectorConnectionToIDMap.get(remoteInspectorConnection);
    auto connectionTargetPair = std::make_pair(connectionID, targetID);
    ASSERT(m_automationTargets.contains(connectionTargetPair) || m_inspectionTargets.contains(connectionTargetPair));
    GDBusConnection* clientConnection = m_inspectionTargets.contains(connectionTargetPair) ? m_clientConnection.get() : m_automationConnection.get();
    ASSERT(clientConnection);

    g_dbus_connection_call(clientConnection, nullptr,
        REMOTE_INSPECTOR_CLIENT_OBJECT_PATH,
        REMOTE_INSPECTOR_CLIENT_DBUS_INTERFACE,
        "SendMessageToFrontend",
        g_variant_new("(tt&s)", m_remoteInspectorConnectionToIDMap.get(remoteInspectorConnection), targetID, message),
        nullptr, G_DBUS_CALL_FLAGS_NO_AUTO_START,
        -1, m_cancellable.get(), RemoteInspectorServerInternal::dbusConnectionCallAsyncReadyCallback, nullptr);
}

void RemoteInspectorServer::startAutomationSession(GDBusConnection* automationConnection, const char* sessionID, const RemoteInspector::Client::SessionCapabilities& capabilities)
{
    if (!m_automationConnection)
        m_automationConnection = automationConnection;
    ASSERT(m_automationConnection.get() == automationConnection);

    RemoteInspector::singleton().requestAutomationSession(sessionID, capabilities);
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)

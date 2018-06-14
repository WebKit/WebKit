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

#pragma once

#if ENABLE(REMOTE_INSPECTOR)

#include "RemoteInspector.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/glib/GRefPtr.h>

typedef struct _GCancellable GCancellable;
typedef struct _GDBusConnection GDBusConnection;
typedef struct _GDBusInterfaceInfo GDBusInterfaceInfo;
typedef struct _GDBusInterfaceVTable GDBusInterfaceVTable;
typedef struct _GDBusNodeInfo GDBusNodeInfo;
typedef struct _GDBusServer GDBusServer;
typedef struct _GVariant GVariant;

namespace Inspector {

class RemoteInspectorServer {
public:
    static RemoteInspectorServer& singleton();
    ~RemoteInspectorServer();

    bool start(const char* address, unsigned port);
    bool isRunning() const { return !!m_dbusServer; }

private:
    GDBusInterfaceInfo* interfaceInfo();

    static gboolean newConnectionCallback(GDBusServer*, GDBusConnection*, RemoteInspectorServer*);
    static void connectionClosedCallback(GDBusConnection*, gboolean remotePeerVanished, GError*, RemoteInspectorServer*);
    void newConnection(GDBusConnection*);
    void connectionClosed(GDBusConnection*);

    static const GDBusInterfaceVTable s_interfaceVTable;
    void setTargetList(GDBusConnection*, GVariant*);
    GVariant* setupInspectorClient(GDBusConnection*, const char* clientBackendCommandsHash);
    void setup(GDBusConnection*, uint64_t connectionID, uint64_t targetID);
    void close(GDBusConnection*, uint64_t connectionID, uint64_t targetID);
    void clientConnectionClosed(GDBusConnection*);
    void sendMessageToFrontend(GDBusConnection*, uint64_t target, const char*);
    void sendMessageToBackend(GDBusConnection*, uint64_t connectionID, uint64_t targetID, const char*);
    void startAutomationSession(GDBusConnection*, const char* sessionID, const RemoteInspector::Client::SessionCapabilities&);

    static void clientConnectionClosedCallback(GDBusConnection*, gboolean remotePeerVanished, GError*, RemoteInspectorServer*);

    GDBusNodeInfo* m_introspectionData { nullptr };
    GRefPtr<GDBusServer> m_dbusServer;
    GRefPtr<GCancellable> m_cancellable;
    HashSet<GRefPtr<GDBusConnection>> m_connections;
    HashMap<GDBusConnection*, uint64_t> m_remoteInspectorConnectionToIDMap;
    HashMap<uint64_t, GDBusConnection*> m_idToRemoteInspectorConnectionMap;
    GRefPtr<GDBusConnection> m_clientConnection;
    HashSet<std::pair<uint64_t, uint64_t>> m_inspectionTargets;
    GRefPtr<GDBusConnection> m_automationConnection;
    HashSet<std::pair<uint64_t, uint64_t>> m_automationTargets;
};

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)

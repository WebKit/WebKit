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
#include <wtf/glib/SocketConnection.h>

typedef struct _GSocketConnection GSocketConnection;
typedef struct _GSocketService GSocketService;

namespace Inspector {

class RemoteInspectorServer {
public:
    static RemoteInspectorServer& singleton();
    ~RemoteInspectorServer();

    bool start(const char* address, unsigned port);
    bool isRunning() const { return !!m_service; }

private:
    static gboolean incomingConnectionCallback(GSocketService*, GSocketConnection*, GObject*, RemoteInspectorServer*);
    void incomingConnection(Ref<SocketConnection>&&);

    static const SocketConnection::MessageHandlers& messageHandlers();
    void connectionDidClose(SocketConnection&);
    void setTargetList(SocketConnection&, GVariant*);
    GVariant* setupInspectorClient(SocketConnection&, const char* clientBackendCommandsHash);
    void setup(SocketConnection&, uint64_t connectionID, uint64_t targetID);
    void close(SocketConnection&, uint64_t connectionID, uint64_t targetID);
    void sendMessageToFrontend(SocketConnection&, uint64_t target, const char*);
    void sendMessageToBackend(SocketConnection&, uint64_t connectionID, uint64_t targetID, const char*);
    void startAutomationSession(SocketConnection&, const char* sessionID, const RemoteInspector::Client::SessionCapabilities&);

    GRefPtr<GSocketService> m_service;
    HashSet<RefPtr<SocketConnection>> m_connections;
    HashMap<SocketConnection*, uint64_t> m_remoteInspectorConnectionToIDMap;
    HashMap<uint64_t, SocketConnection*> m_idToRemoteInspectorConnectionMap;
    SocketConnection* m_clientConnection { nullptr };
    SocketConnection* m_automationConnection { nullptr };
    HashSet<std::pair<uint64_t, uint64_t>> m_inspectionTargets;
    HashSet<std::pair<uint64_t, uint64_t>> m_automationTargets;
};

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)

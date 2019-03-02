/*
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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

#pragma once

#if ENABLE(REMOTE_INSPECTOR)

#include "RemoteInspector.h"

#include "RemoteInspectorConnectionClient.h"
#include "RemoteInspectorSocketServer.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>

namespace Inspector {

class RemoteInspectorServer : public RemoteInspectorConnectionClient {
public:
    JS_EXPORT_PRIVATE static RemoteInspectorServer& singleton();

    JS_EXPORT_PRIVATE bool start(uint16_t);
    bool isRunning() const { return !!m_server; }

    JS_EXPORT_PRIVATE void addServerConnection(PlatformSocketType);

private:
    void connectionClosed(uint64_t connectionID);

    void setTargetList(const struct Event&);
    void setupInspectorClient(const struct Event&);
    void setup(const struct Event&);
    void close(const struct Event&);
    void sendMessageToFrontend(const struct Event&);
    void sendMessageToBackend(const struct Event&);

    void sendCloseEvent(uint64_t connectionID, uint64_t targetID);
    void clientConnectionClosed();

    void didAccept(ClientID, RemoteInspectorSocket::DomainType) override;
    void didClose(ClientID) override;

    void sendWebInspectorEvent(ClientID, const String&);

    HashMap<String, CallHandler>& dispatchMap();

    HashSet<std::pair<uint64_t, uint64_t>> m_inspectionTargets;
    std::unique_ptr<RemoteInspectorSocketServer> m_server;

    // Connections to the WebProcess.
    Vector<ClientID> m_inspectorConnections;
    Lock m_connectionsLock;

    // Connections from RemoteInspectorClient.
    Optional<ClientID> m_clientConnection;
};

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)

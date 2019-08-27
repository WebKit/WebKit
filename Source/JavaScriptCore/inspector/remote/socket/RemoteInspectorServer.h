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
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>

namespace Inspector {

class RemoteInspectorServer : public RemoteInspectorConnectionClient {
public:
    JS_EXPORT_PRIVATE static RemoteInspectorServer& singleton();

    JS_EXPORT_PRIVATE bool start(const char* address, uint16_t port);
    bool isRunning() const { return !!m_server; }

    JS_EXPORT_PRIVATE Optional<uint16_t> listenForTargets();
    JS_EXPORT_PRIVATE Optional<PlatformSocketType> connect();

private:
    friend class NeverDestroyed<RemoteInspectorServer>;
    RemoteInspectorServer() { Socket::init(); }

    void connectionClosed(ConnectionID);

    void setTargetList(const Event&);
    void setupInspectorClient(const Event&);
    void setup(const Event&);
    void close(const Event&);
    void sendMessageToFrontend(const Event&);
    void sendMessageToBackend(const Event&);

    void sendCloseEvent(ConnectionID, TargetID);
    void clientConnectionClosed();

    void didAccept(ConnectionID acceptedID, ConnectionID listenerID, Socket::Domain) override;
    void didClose(ConnectionID) override;

    void sendWebInspectorEvent(ConnectionID, const String&);

    HashMap<String, CallHandler>& dispatchMap() override;

    HashSet<std::pair<ConnectionID, TargetID>> m_inspectionTargets;

    Optional<ConnectionID> m_server;

    // Connections to the WebProcess.
    Vector<ConnectionID> m_inspectorConnections;
    Lock m_connectionsLock;

    // Listener for targets.
    Optional<ConnectionID> m_inspectorListener;

    // Connection from RemoteInspectorClient.
    Optional<ConnectionID> m_clientConnection;
};

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)

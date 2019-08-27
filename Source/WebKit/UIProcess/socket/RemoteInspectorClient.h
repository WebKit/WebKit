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

#include <JavaScriptCore/RemoteControllableTarget.h>
#include <JavaScriptCore/RemoteInspectorConnectionClient.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class RemoteInspectorClient;
class RemoteInspectorProxy;

class RemoteInspectorObserver {
public:
    virtual ~RemoteInspectorObserver() { }
    virtual void targetListChanged(RemoteInspectorClient&) = 0;
    virtual void connectionClosed(RemoteInspectorClient&) = 0;
};

using ConnectionID = Inspector::ConnectionID;
using TargetID = Inspector::TargetID;

class RemoteInspectorClient : public Inspector::RemoteInspectorConnectionClient {
    WTF_MAKE_FAST_ALLOCATED();
public:
    RemoteInspectorClient(const char* address, unsigned port, RemoteInspectorObserver&);
    ~RemoteInspectorClient();

    struct Target {
        TargetID id;
        String type;
        String name;
        String url;
    };

    const HashMap<ConnectionID, Vector<Target>>& targets() const { return m_targets; }

    void inspect(ConnectionID, TargetID, const String&);
    void sendMessageToBackend(ConnectionID, TargetID, const String&);
    void closeFromFrontend(ConnectionID, TargetID);

private:
    friend class NeverDestroyed<RemoteInspectorClient>;

    void startInitialCommunication();
    void connectionClosed();

    void setTargetList(const Event&);
    void sendMessageToFrontend(const Event&);
    void setBackendCommands(const Event&);

    void didClose(ConnectionID) final;
    HashMap<String, CallHandler>& dispatchMap() final;

    void sendWebInspectorEvent(const String&);

    RemoteInspectorObserver& m_observer;
    Optional<ConnectionID> m_connectionID;
    HashMap<ConnectionID, Vector<Target>> m_targets;
    HashMap<std::pair<ConnectionID, TargetID>, std::unique_ptr<RemoteInspectorProxy>> m_inspectorProxyMap;
};

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)

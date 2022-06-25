/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "MessageReceiver.h"
#include "MessageSender.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/RegistrableDomain.h>
#include <WebCore/SharedWorkerIdentifier.h>
#include <WebCore/SharedWorkerObjectIdentifier.h>
#include <WebCore/Timer.h>
#include <WebCore/TransferredMessagePort.h>

namespace WebCore {
class RegistrableDomain;
class ScriptBuffer;
struct ClientOrigin;
struct WorkerFetchResult;
struct WorkerOptions;
}

namespace WebKit {

class NetworkConnectionToWebProcess;
class WebSharedWorker;
class WebSharedWorkerServer;

class WebSharedWorkerServerToContextConnection final : public IPC::MessageSender, public IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WebSharedWorkerServerToContextConnection(NetworkConnectionToWebProcess&, const WebCore::RegistrableDomain&, WebSharedWorkerServer&);
    ~WebSharedWorkerServerToContextConnection();

    WebCore::ProcessIdentifier webProcessIdentifier() const;
    const WebCore::RegistrableDomain& registrableDomain() const { return m_registrableDomain; }
    IPC::Connection& ipcConnection() const;

    void terminateWhenPossible() { m_shouldTerminateWhenPossible = true; }

    void launchSharedWorker(WebSharedWorker&);
    void postConnectEvent(const WebSharedWorker&, const WebCore::TransferredMessagePort&);
    void terminateSharedWorker(const WebSharedWorker&);

    void suspendSharedWorker(WebCore::SharedWorkerIdentifier);
    void resumeSharedWorker(WebCore::SharedWorkerIdentifier);

    const HashMap<WebCore::ProcessIdentifier, HashSet<WebCore::SharedWorkerObjectIdentifier>>& sharedWorkerObjects() const { return m_sharedWorkerObjects; }

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    void addSharedWorkerObject(WebCore::SharedWorkerObjectIdentifier);
    void removeSharedWorkerObject(WebCore::SharedWorkerObjectIdentifier);

private:
    void idleTerminationTimerFired();
    void connectionIsNoLongerNeeded();

    // IPC messages.
    void postExceptionToWorkerObject(WebCore::SharedWorkerIdentifier, const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL);

    // IPC::MessageSender.
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final;

    NetworkConnectionToWebProcess& m_connection;
    WeakPtr<WebSharedWorkerServer> m_server;
    WebCore::RegistrableDomain m_registrableDomain;
    HashMap<WebCore::ProcessIdentifier, HashSet<WebCore::SharedWorkerObjectIdentifier>> m_sharedWorkerObjects;
    WebCore::Timer m_idleTerminationTimer;
    bool m_shouldTerminateWhenPossible { false };
};

} // namespace WebKit

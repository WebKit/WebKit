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
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/SharedWorkerObjectIdentifier.h>
#include <WebCore/TransferredMessagePort.h>
#include <WebCore/WorkerInitializationData.h>
#include <pal/SessionID.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit {
class WebSharedWorkerServerConnection;
}

namespace WebCore {
class ResourceError;
struct SharedWorkerKey;
struct WorkerFetchResult;
struct WorkerOptions;
}

namespace WebKit {

class NetworkProcess;
class NetworkSession;
class WebSharedWorker;
class WebSharedWorkerServer;
class WebSharedWorkerServerToContextConnection;
class NetworkSession;

class WebSharedWorkerServerConnection : public IPC::MessageSender, public IPC::MessageReceiver, public RefCounted<WebSharedWorkerServerConnection> {
    WTF_MAKE_TZONE_ALLOCATED(WebSharedWorkerServerConnection);
public:
    static Ref<WebSharedWorkerServerConnection> create(NetworkProcess&, WebSharedWorkerServer&, IPC::Connection&, WebCore::ProcessIdentifier);

    ~WebSharedWorkerServerConnection();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    WebSharedWorkerServer* server();
    const WebSharedWorkerServer* server() const;

    NetworkSession* session();
    WebCore::ProcessIdentifier webProcessIdentifier() const { return m_webProcessIdentifier; }

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    void fetchScriptInClient(const WebSharedWorker&, WebCore::SharedWorkerObjectIdentifier, CompletionHandler<void(WebCore::WorkerFetchResult&&, WebCore::WorkerInitializationData&&)>&&);
    void notifyWorkerObjectOfLoadCompletion(WebCore::SharedWorkerObjectIdentifier, const WebCore::ResourceError&);
    void postErrorToWorkerObject(WebCore::SharedWorkerObjectIdentifier, const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL, bool isErrorEvent);

private:
    WebSharedWorkerServerConnection(NetworkProcess&, WebSharedWorkerServer&, IPC::Connection&, WebCore::ProcessIdentifier);
    Ref<NetworkProcess> protectedNetworkProcess();

    // IPC::MessageSender.
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final { return 0; }

    // IPC messages.
    void requestSharedWorker(WebCore::SharedWorkerKey&&, WebCore::SharedWorkerObjectIdentifier, WebCore::TransferredMessagePort&&, WebCore::WorkerOptions&&);
    void sharedWorkerObjectIsGoingAway(WebCore::SharedWorkerKey&&, WebCore::SharedWorkerObjectIdentifier);
    void suspendForBackForwardCache(WebCore::SharedWorkerKey&&, WebCore::SharedWorkerObjectIdentifier);
    void resumeForBackForwardCache(WebCore::SharedWorkerKey&&, WebCore::SharedWorkerObjectIdentifier);

    Ref<IPC::Connection> m_contentConnection;
    Ref<NetworkProcess> m_networkProcess;
    WeakPtr<WebSharedWorkerServer> m_server;
    WebCore::ProcessIdentifier m_webProcessIdentifier;
};

} // namespace WebKit

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

#include <WebCore/ProcessIdentifier.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <WebCore/SharedWorkerIdentifier.h>
#include <WebCore/SharedWorkerKey.h>
#include <WebCore/SharedWorkerObjectIdentifier.h>
#include <WebCore/TransferredMessagePort.h>
#include <wtf/CheckedPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebKit {
class WebSharedWorkerServer;
}

namespace PAL {
class SessionID;
}

namespace WebCore {
class Site;
struct ClientOrigin;
struct WorkerFetchResult;
struct WorkerInitializationData;
struct WorkerOptions;
}

namespace WebKit {

class NetworkSession;
class WebSharedWorker;
class WebSharedWorkerServerConnection;
class WebSharedWorkerServerToContextConnection;

class WebSharedWorkerServer : public CanMakeWeakPtr<WebSharedWorkerServer>, public CanMakeCheckedPtr<WebSharedWorkerServer> {
    WTF_MAKE_TZONE_ALLOCATED(WebSharedWorkerServer);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WebSharedWorkerServer);
public:
    explicit WebSharedWorkerServer(NetworkSession&);
    ~WebSharedWorkerServer();

    PAL::SessionID sessionID();
    WebSharedWorkerServerToContextConnection* contextConnectionForRegistrableDomain(const WebCore::RegistrableDomain&) const;

    void requestSharedWorker(WebCore::SharedWorkerKey&&, WebCore::SharedWorkerObjectIdentifier, WebCore::TransferredMessagePort&&, WebCore::WorkerOptions&&);
    void sharedWorkerObjectIsGoingAway(const WebCore::SharedWorkerKey&, WebCore::SharedWorkerObjectIdentifier);
    void suspendForBackForwardCache(const WebCore::SharedWorkerKey&, WebCore::SharedWorkerObjectIdentifier);
    void resumeForBackForwardCache(const WebCore::SharedWorkerKey&, WebCore::SharedWorkerObjectIdentifier);
    void postErrorToWorkerObject(WebCore::SharedWorkerIdentifier, const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL, bool isErrorEvent);
    void sharedWorkerTerminated(WebCore::SharedWorkerIdentifier);

    void terminateContextConnectionWhenPossible(const WebCore::RegistrableDomain&, WebCore::ProcessIdentifier);
    void addContextConnection(WebSharedWorkerServerToContextConnection&);
    void removeContextConnection(WebSharedWorkerServerToContextConnection&);
    void addConnection(Ref<WebSharedWorkerServerConnection>&&);
    void removeConnection(WebCore::ProcessIdentifier);

private:
    void createContextConnection(const WebCore::Site&, std::optional<WebCore::ProcessIdentifier> requestingProcessIdentifier);
    bool needsContextConnectionForRegistrableDomain(const WebCore::RegistrableDomain&) const;
    void contextConnectionCreated(WebSharedWorkerServerToContextConnection&);
    void didFinishFetchingSharedWorkerScript(WebSharedWorker&, WebCore::WorkerFetchResult&&, WebCore::WorkerInitializationData&&);
    void shutDownSharedWorker(const WebCore::SharedWorkerKey&);

    CheckedRef<NetworkSession> m_session;
    HashMap<WebCore::ProcessIdentifier, Ref<WebSharedWorkerServerConnection>> m_connections;
    HashMap<WebCore::RegistrableDomain, WeakRef<WebSharedWorkerServerToContextConnection>> m_contextConnections;
    HashSet<WebCore::RegistrableDomain> m_pendingContextConnectionDomains;
    HashMap<WebCore::SharedWorkerKey, Ref<WebSharedWorker>> m_sharedWorkers;
};

} // namespace WebKit

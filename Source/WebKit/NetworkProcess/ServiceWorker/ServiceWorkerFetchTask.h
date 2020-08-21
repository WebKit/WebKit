/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(SERVICE_WORKER)

#include <WebCore/FetchIdentifier.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ServiceWorkerClientIdentifier.h>
#include <WebCore/ServiceWorkerTypes.h>
#include <WebCore/Timer.h>
#include <pal/SessionID.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class ResourceError;
class ResourceRequest;
class ResourceResponse;
}

namespace IPC {
class Connection;
class DataReference;
class Decoder;
class FormDataReference;
}

namespace WebKit {

class NetworkResourceLoader;
class WebSWServerConnection;
class WebSWServerToContextConnection;

class ServiceWorkerFetchTask : public CanMakeWeakPtr<ServiceWorkerFetchTask> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ServiceWorkerFetchTask(WebSWServerConnection&, NetworkResourceLoader&, WebCore::ResourceRequest&&, WebCore::SWServerConnectionIdentifier, WebCore::ServiceWorkerIdentifier, WebCore::ServiceWorkerRegistrationIdentifier, bool shouldSoftUpdate);
    ~ServiceWorkerFetchTask();

    void start(WebSWServerToContextConnection&);
    void cancelFromClient();
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

    void continueDidReceiveFetchResponse();
    void continueFetchTaskWith(WebCore::ResourceRequest&&);

    WebCore::FetchIdentifier fetchIdentifier() const { return m_fetchIdentifier; }
    WebCore::ServiceWorkerIdentifier serviceWorkerIdentifier() const { return m_serviceWorkerIdentifier; }

    WebCore::ResourceRequest takeRequest() { return WTFMove(m_currentRequest); }

    void cannotHandle();
    void contextClosed();

private:
    void didReceiveRedirectResponse(WebCore::ResourceResponse&&);
    void didReceiveResponse(WebCore::ResourceResponse&&, bool needsContinueDidReceiveResponseMessage);
    void didReceiveData(const IPC::DataReference&, int64_t encodedDataLength);
    void didReceiveFormData(const IPC::FormDataReference&);
    void didFinish();
    void didFail(const WebCore::ResourceError&);
    void didNotHandle();

    void startFetch();

    void timeoutTimerFired();
    void softUpdateIfNeeded();

    template<typename Message> bool sendToServiceWorker(Message&&);
    template<typename Message> bool sendToClient(Message&&);

    WeakPtr<WebSWServerConnection> m_swServerConnection;
    NetworkResourceLoader& m_loader;
    WeakPtr<WebSWServerToContextConnection> m_serviceWorkerConnection;
    WebCore::FetchIdentifier m_fetchIdentifier;
    WebCore::SWServerConnectionIdentifier m_serverConnectionIdentifier;
    WebCore::ServiceWorkerIdentifier m_serviceWorkerIdentifier;
    WebCore::ResourceRequest m_currentRequest;
    WebCore::Timer m_timeoutTimer;
    bool m_wasHandled { false };
    bool m_isDone { false };
    WebCore::ServiceWorkerRegistrationIdentifier m_serviceWorkerRegistrationIdentifier;
    bool m_shouldSoftUpdate { false };
};

}

#endif // ENABLE(SERVICE_WORKER)

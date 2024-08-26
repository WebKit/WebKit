/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "Connection.h"
#include <WebCore/FetchEvent.h>
#include <WebCore/FetchLoader.h>
#include <WebCore/FetchLoaderClient.h>
#include <WebCore/NetworkLoadMetrics.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/ServiceWorkerFetch.h>
#include <WebCore/ServiceWorkerTypes.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/UniqueRef.h>

namespace WebKit {

class WebServiceWorkerFetchTaskClient final : public WebCore::ServiceWorkerFetch::Client {
public:
    static Ref<WebServiceWorkerFetchTaskClient> create(Ref<IPC::Connection>&& connection, WebCore::ServiceWorkerIdentifier serviceWorkerIdentifier,  WebCore::SWServerConnectionIdentifier serverConnectionIdentifier, WebCore::FetchIdentifier fetchTaskIdentifier, bool needsContinueDidReceiveResponseMessage)
    {
        return adoptRef(*new WebServiceWorkerFetchTaskClient(WTFMove(connection), serviceWorkerIdentifier, serverConnectionIdentifier, fetchTaskIdentifier, needsContinueDidReceiveResponseMessage));
    }

    virtual ~WebServiceWorkerFetchTaskClient();

private:
    WebServiceWorkerFetchTaskClient(Ref<IPC::Connection>&&, WebCore::ServiceWorkerIdentifier, WebCore::SWServerConnectionIdentifier, WebCore::FetchIdentifier, bool needsContinueDidReceiveResponseMessage);

    void didReceiveResponse(const WebCore::ResourceResponse&) final;
    void didReceiveRedirection(const WebCore::ResourceResponse&) final;
    void didReceiveData(const WebCore::SharedBuffer&) final;
    void didReceiveFormDataAndFinish(Ref<WebCore::FormData>&&) final;
    void didFail(const WebCore::ResourceError&) final;
    void didFinish(const WebCore::NetworkLoadMetrics&) final;
    void didNotHandle() final;
    void cancel() final;
    void continueDidReceiveResponse() final;
    void convertFetchToDownload() final;
    void setCancelledCallback(Function<void()>&&) final;
    void setFetchEvent(Ref<WebCore::FetchEvent>&&);
    void navigationPreloadIsReady(WebCore::ResourceResponse::CrossThreadData&&) final;
    void navigationPreloadFailed(WebCore::ResourceError&&) final;
    void usePreload() final;

    void cleanup();

    void didReceiveBlobChunk(const WebCore::SharedBuffer&);
    void didFinishBlobLoading();

    struct BlobLoader final : WebCore::FetchLoaderClient {
        explicit BlobLoader(WebServiceWorkerFetchTaskClient& client) : client(client) { }

        // FetchLoaderClient API
        void didReceiveResponse(const WebCore::ResourceResponse&) final { }
        void didReceiveData(const WebCore::SharedBuffer& data) final { client->didReceiveBlobChunk(data); }
        void didFail(const WebCore::ResourceError& error) final { client->didFail(error); }
        void didSucceed(const WebCore::NetworkLoadMetrics&) final { client->didFinishBlobLoading(); }

        Ref<WebServiceWorkerFetchTaskClient> client;
        std::unique_ptr<WebCore::FetchLoader> loader;
    };

    RefPtr<IPC::Connection> m_connection;
    WebCore::SWServerConnectionIdentifier m_serverConnectionIdentifier;
    WebCore::ServiceWorkerIdentifier m_serviceWorkerIdentifier;
    WebCore::FetchIdentifier m_fetchIdentifier;
    std::optional<BlobLoader> m_blobLoader;
    bool m_needsContinueDidReceiveResponseMessage { false };
    bool m_waitingForContinueDidReceiveResponseMessage { false };
    std::variant<std::nullptr_t, WebCore::SharedBufferBuilder, Ref<WebCore::FormData>, UniqueRef<WebCore::ResourceError>> m_responseData;
    WebCore::NetworkLoadMetrics m_networkLoadMetrics;
    bool m_didFinish { false };
    bool m_isDownload { false };
    RefPtr<WebCore::FetchEvent> m_event;
    Function<void()> m_cancelledCallback;
    std::optional<WebCore::ResourceResponse::CrossThreadData> m_preloadResponse;
    WebCore::ResourceError m_preloadError;
};

} // namespace WebKit

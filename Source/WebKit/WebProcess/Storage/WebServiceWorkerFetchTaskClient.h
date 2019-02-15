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

#if ENABLE(SERVICE_WORKER)

#include "Connection.h"
#include <WebCore/FetchIdentifier.h>
#include <WebCore/FetchLoader.h>
#include <WebCore/FetchLoaderClient.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ServiceWorkerFetch.h>
#include <WebCore/ServiceWorkerTypes.h>

namespace WebKit {

class WebServiceWorkerFetchTaskClient final : public WebCore::ServiceWorkerFetch::Client {
public:
    static Ref<WebServiceWorkerFetchTaskClient> create(Ref<IPC::Connection>&& connection, WebCore::ServiceWorkerIdentifier serviceWorkerIdentifier,  WebCore::SWServerConnectionIdentifier serverConnectionIdentifier, WebCore::FetchIdentifier fetchTaskIdentifier, bool needsContinueDidReceiveResponseMessage)
    {
        return adoptRef(*new WebServiceWorkerFetchTaskClient(WTFMove(connection), serviceWorkerIdentifier, serverConnectionIdentifier, fetchTaskIdentifier, needsContinueDidReceiveResponseMessage));
    }

private:
    WebServiceWorkerFetchTaskClient(Ref<IPC::Connection>&&, WebCore::ServiceWorkerIdentifier, WebCore::SWServerConnectionIdentifier, WebCore::FetchIdentifier, bool needsContinueDidReceiveResponseMessage);

    void didReceiveResponse(const WebCore::ResourceResponse&) final;
    void didReceiveRedirection(const WebCore::ResourceResponse&) final;
    void didReceiveData(Ref<WebCore::SharedBuffer>&&) final;
    void didReceiveFormDataAndFinish(Ref<WebCore::FormData>&&) final;
    void didFail(const WebCore::ResourceError&) final;
    void didFinish() final;
    void didNotHandle() final;
    void cancel() final;
    void continueDidReceiveResponse() final;

    void cleanup();
    
    void didReceiveBlobChunk(const char* data, size_t size);
    void didFinishBlobLoading();

    struct BlobLoader final : WebCore::FetchLoaderClient {
        explicit BlobLoader(WebServiceWorkerFetchTaskClient& client) : client(client) { }

        // FetchLoaderClient API
        void didReceiveResponse(const WebCore::ResourceResponse&) final { }
        void didReceiveData(const char* data, size_t size) final { client->didReceiveBlobChunk(data, size); }
        void didFail(const WebCore::ResourceError& error) final { client->didFail(error); }
        void didSucceed() final { client->didFinishBlobLoading(); }

        Ref<WebServiceWorkerFetchTaskClient> client;
        std::unique_ptr<WebCore::FetchLoader> loader;
    };

    RefPtr<IPC::Connection> m_connection;
    WebCore::SWServerConnectionIdentifier m_serverConnectionIdentifier;
    WebCore::ServiceWorkerIdentifier m_serviceWorkerIdentifier;
    WebCore::FetchIdentifier m_fetchIdentifier;
    Optional<BlobLoader> m_blobLoader;
    bool m_needsContinueDidReceiveResponseMessage { false };
    bool m_waitingForContinueDidReceiveResponseMessage { false };
    Variant<std::nullptr_t, Ref<WebCore::SharedBuffer>, Ref<FormData>, UniqueRef<ResourceError>> m_responseData;
    bool m_didFinish { false };
};

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)

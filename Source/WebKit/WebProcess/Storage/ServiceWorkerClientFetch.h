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

#include "DataReference.h"
#include "FormDataReference.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include <WebCore/FetchIdentifier.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceLoader.h>
#include <wtf/CompletionHandler.h>

namespace WebKit {

class WebSWClientConnection;
class WebServiceWorkerProvider;

class ServiceWorkerClientFetch final : public RefCounted<ServiceWorkerClientFetch>, public IPC::MessageReceiver {
public:
    enum class Result { Succeeded, Cancelled, Unhandled };
    using Callback = WTF::CompletionHandler<void(Result)>;

    static Ref<ServiceWorkerClientFetch> create(WebServiceWorkerProvider&, Ref<WebCore::ResourceLoader>&&, WebCore::FetchIdentifier, Ref<WebSWClientConnection>&&, bool shouldClearReferrerOnHTTPSToHTTPRedirect, Callback&&);
    ~ServiceWorkerClientFetch();

    void start();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void cancel();

    bool isOngoing() const { return !!m_callback; }

private:
    ServiceWorkerClientFetch(WebServiceWorkerProvider&, Ref<WebCore::ResourceLoader>&&, WebCore::FetchIdentifier, Ref<WebSWClientConnection>&&, bool shouldClearReferrerOnHTTPSToHTTPRedirect, Callback&&);

    Optional<WebCore::ResourceError> validateResponse(const WebCore::ResourceResponse&);

    void didReceiveResponse(WebCore::ResourceResponse&&);
    void didReceiveData(const IPC::DataReference&, int64_t encodedDataLength);
    void didReceiveFormData(const IPC::FormDataReference&);
    void didFinish();
    void didFail(WebCore::ResourceError&&);
    void didNotHandle();

    void continueLoadingAfterCheckingResponse();

    WebServiceWorkerProvider& m_serviceWorkerProvider;
    RefPtr<WebCore::ResourceLoader> m_loader;
    WebCore::FetchIdentifier m_identifier;
    Ref<WebSWClientConnection> m_connection;
    Callback m_callback;
    enum class RedirectionStatus { None, Receiving, Following, Received };
    RedirectionStatus m_redirectionStatus { RedirectionStatus::None };
    bool m_shouldClearReferrerOnHTTPSToHTTPRedirect { true };
    RefPtr<WebCore::SharedBuffer> m_buffer;
    int64_t m_encodedDataLength { 0 };
    bool m_isCheckingResponse { false };
    bool m_didFinish { false };
    bool m_didFail { false };
    WebCore::ResourceError m_error;

    WebCore::ServiceWorkerRegistrationIdentifier m_serviceWorkerRegistrationIdentifier;
};

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)

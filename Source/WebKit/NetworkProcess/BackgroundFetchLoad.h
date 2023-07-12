/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "NetworkDataTask.h"
#include "NetworkResourceLoadParameters.h"
#include <WebCore/BackgroundFetchRecordLoader.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <wtf/CompletionHandler.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class ResourceRequest;
struct BackgroundFetchRequest;
struct ClientOrigin;
struct FetchOptions;
}

namespace WebKit {

class NetworkLoadChecker;
class NetworkProcess;

class BackgroundFetchLoad final : public WebCore::BackgroundFetchRecordLoader, public NetworkDataTaskClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    BackgroundFetchLoad(NetworkProcess&, PAL::SessionID, Client&, const WebCore::BackgroundFetchRequest&, size_t responseDataSize, const WebCore::ClientOrigin&);
    ~BackgroundFetchLoad();

private:
    const URL& currentURL() const;

    // NetworkDataTaskClient
    void willPerformHTTPRedirection(WebCore::ResourceResponse&&, WebCore::ResourceRequest&&, RedirectCompletionHandler&&) final;
    void didReceiveChallenge(WebCore::AuthenticationChallenge&&, NegotiatedLegacyTLS, ChallengeCompletionHandler&&) final;
    void didReceiveResponse(WebCore::ResourceResponse&&, NegotiatedLegacyTLS, PrivateRelayed, ResponseCompletionHandler&&) final;
    void didReceiveData(const WebCore::SharedBuffer&) final;
    void didCompleteWithError(const WebCore::ResourceError&, const WebCore::NetworkLoadMetrics&) final;
    void didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend) final;
    void wasBlocked() final;
    void cannotShowURL() final;
    void wasBlockedByRestrictions() final;
    void wasBlockedByDisabledFTP() final;

    // WebCore::BackgroundFetchRecordLoader
    void abort() final;

    void loadRequest(NetworkProcess&, WebCore::ResourceRequest&&);

    void didFinish(const WebCore::ResourceError& = { }, const WebCore::ResourceResponse& response = { });

    PAL::SessionID m_sessionID;
    WeakPtr<Client> m_client;
    WebCore::ResourceRequest m_request;
    NetworkResourceLoadParameters m_parameters;
    RefPtr<NetworkDataTask> m_task;
    UniqueRef<NetworkLoadChecker> m_networkLoadChecker;
    Vector<RefPtr<WebCore::BlobDataFileReference>> m_blobFiles;
};

}

#endif

/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "DownloadID.h"
#include "NetworkCacheEntry.h"
#include "NetworkLoadClient.h"
#include "NetworkLoadParameters.h"
#include <WebCore/NavigationPreloadState.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class NetworkLoadMetrics;
class FragmentedSharedBuffer;
}

namespace WebKit {

class DownloadManager;
class NetworkLoad;
class NetworkSession;

class ServiceWorkerNavigationPreloader final : public NetworkLoadClient, public CanMakeWeakPtr<ServiceWorkerNavigationPreloader> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ServiceWorkerNavigationPreloader(NetworkSession&, NetworkLoadParameters&&, const WebCore::NavigationPreloadState&, bool shouldCaptureExtraNetworkLoadMetrics);
    ~ServiceWorkerNavigationPreloader();

    void cancel();

    using ResponseCallback = Function<void()>;
    void waitForResponse(ResponseCallback&&);
    using BodyCallback = Function<void(RefPtr<const WebCore::FragmentedSharedBuffer>&&, uint64_t reportedEncodedDataLength)>;
    void waitForBody(BodyCallback&&);

    const WebCore::ResourceError& error() const { return m_error; }
    const WebCore::ResourceResponse& response() const { return m_response; }
    const WebCore::NetworkLoadMetrics& networkLoadMetrics() const { return m_networkLoadMetrics; }
    bool isServiceWorkerNavigationPreloadEnabled() const { return m_state.enabled; }

    bool convertToDownload(DownloadManager&, DownloadID, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&);

    MonotonicTime startTime() const { return m_startTime; }

private:
    // NetworkLoadClient.
    void didSendData(uint64_t bytesSent, uint64_t totalBytesToBeSent) final { }
    bool isSynchronous() const final { return false; }
    bool isAllowedToAskUserForCredentials() const final { return false; }
    void willSendRedirectedRequest(WebCore::ResourceRequest&&, WebCore::ResourceRequest&& redirectRequest, WebCore::ResourceResponse&& redirectResponse) final;
    void didReceiveResponse(WebCore::ResourceResponse&&, PrivateRelayed, ResponseCompletionHandler&&) final;
    void didReceiveBuffer(const WebCore::FragmentedSharedBuffer&, uint64_t reportedEncodedDataLength) final;
    void didFinishLoading(const WebCore::NetworkLoadMetrics&) final;
    void didFailLoading(const WebCore::ResourceError&) final;
    bool shouldCaptureExtraNetworkLoadMetrics() const final { return m_shouldCaptureExtraNetworkLoadMetrics; }

    void start();
    void loadWithCacheEntry(NetworkCache::Entry&);
    void loadFromNetwork();
    void didComplete();

    std::unique_ptr<NetworkLoad> m_networkLoad;
    WeakPtr<NetworkSession> m_session;

    NetworkLoadParameters m_parameters;
    WebCore::NavigationPreloadState m_state;

    std::unique_ptr<NetworkCache::Entry> m_cacheEntry;
    WebCore::NetworkLoadMetrics m_networkLoadMetrics;
    WebCore::ResourceResponse m_response;
    ResponseCompletionHandler m_responseCompletionHandler;
    WebCore::ResourceError m_error;

    ResponseCallback m_responseCallback;
    BodyCallback m_bodyCallback;

    bool m_isStarted { false };
    bool m_isCancelled { false };
    bool m_shouldCaptureExtraNetworkLoadMetrics { false };
    MonotonicTime m_startTime;
};

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)


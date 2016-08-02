/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef NetworkLoad_h
#define NetworkLoad_h

#include "NetworkLoadClient.h"
#include "NetworkLoadParameters.h"
#include "RemoteNetworkingContext.h"
#include <WebCore/ResourceHandleClient.h>
#include <wtf/Optional.h>

#if USE(NETWORK_SESSION)
#include "DownloadID.h"
#include "NetworkSession.h"
#include <WebCore/AuthenticationChallenge.h>
#endif

namespace WebKit {

class NetworkLoad : public WebCore::ResourceHandleClient
#if USE(NETWORK_SESSION)
    , public NetworkDataTaskClient
#endif
{
    WTF_MAKE_FAST_ALLOCATED;
public:
#if USE(NETWORK_SESSION)
    NetworkLoad(NetworkLoadClient&, NetworkLoadParameters&&, NetworkSession&);
#else
    NetworkLoad(NetworkLoadClient&, NetworkLoadParameters&&);
#endif
    ~NetworkLoad();

    void setDefersLoading(bool);
    void cancel();

    const WebCore::ResourceRequest& currentRequest() const { return m_currentRequest; }
    void clearCurrentRequest() { m_currentRequest = WebCore::ResourceRequest(); }

    void continueWillSendRequest(WebCore::ResourceRequest&&);
    void continueDidReceiveResponse();

#if USE(NETWORK_SESSION)
    void convertTaskToDownload(DownloadID, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&);
    void setPendingDownloadID(DownloadID);
    void setPendingDownload(PendingDownload&);
    DownloadID pendingDownloadID() { return m_task->pendingDownloadID(); }
    
    // NetworkDataTaskClient
    void willPerformHTTPRedirection(WebCore::ResourceResponse&&, WebCore::ResourceRequest&&, RedirectCompletionHandler&&) final;
    void didReceiveChallenge(const WebCore::AuthenticationChallenge&, ChallengeCompletionHandler&&) final;
    void didReceiveResponseNetworkSession(WebCore::ResourceResponse&&, ResponseCompletionHandler&&) final;
    void didReceiveData(Ref<WebCore::SharedBuffer>&&) final;
    void didCompleteWithError(const WebCore::ResourceError&) final;
    void didBecomeDownload() final;
    void didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend) override;
    void wasBlocked() override;
    void cannotShowURL() override;
#endif
    // ResourceHandleClient
    void willSendRequestAsync(WebCore::ResourceHandle*, WebCore::ResourceRequest&&, WebCore::ResourceResponse&& redirectResponse) override;
    void didSendData(WebCore::ResourceHandle*, unsigned long long bytesSent, unsigned long long totalBytesToBeSent) override;
    void didReceiveResponseAsync(WebCore::ResourceHandle*, WebCore::ResourceResponse&&) override;
    void didReceiveData(WebCore::ResourceHandle*, const char*, unsigned, int encodedDataLength) override;
    void didReceiveBuffer(WebCore::ResourceHandle*, Ref<WebCore::SharedBuffer>&&, int reportedEncodedDataLength) override;
    void didFinishLoading(WebCore::ResourceHandle*, double finishTime) override;
    void didFail(WebCore::ResourceHandle*, const WebCore::ResourceError&) override;
    void wasBlocked(WebCore::ResourceHandle*) override;
    void cannotShowURL(WebCore::ResourceHandle*) override;
    bool shouldUseCredentialStorage(WebCore::ResourceHandle*) override;
    void didReceiveAuthenticationChallenge(WebCore::ResourceHandle*, const WebCore::AuthenticationChallenge&) override;
    void receivedCancellation(WebCore::ResourceHandle*, const WebCore::AuthenticationChallenge&) override;
    bool usesAsyncCallbacks() override { return true; }
    bool loadingSynchronousXHR() override { return m_client.isSynchronous(); }

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    void canAuthenticateAgainstProtectionSpaceAsync(WebCore::ResourceHandle*, const WebCore::ProtectionSpace&) override;
#endif
#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
    bool supportsDataArray() override;
    void didReceiveDataArray(WebCore::ResourceHandle*, CFArrayRef) override;
#endif
#if PLATFORM(COCOA)
#if USE(CFNETWORK)
    void willCacheResponseAsync(WebCore::ResourceHandle*, CFCachedURLResponseRef) override;
#else
    void willCacheResponseAsync(WebCore::ResourceHandle*, NSCachedURLResponse *) override;
#endif
#endif

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    void continueCanAuthenticateAgainstProtectionSpace(bool);
#endif

#if !USE(NETWORK_SESSION)
    WebCore::ResourceHandle* handle() const { return m_handle.get(); }
#endif

private:
    NetworkLoadClient::ShouldContinueDidReceiveResponse sharedDidReceiveResponse(WebCore::ResourceResponse&&);
    void sharedWillSendRedirectedRequest(WebCore::ResourceRequest&&, WebCore::ResourceResponse&&);

    NetworkLoadClient& m_client;
    const NetworkLoadParameters m_parameters;
#if USE(NETWORK_SESSION)
    RefPtr<NetworkDataTask> m_task;
    Optional<WebCore::AuthenticationChallenge> m_challenge;
    ChallengeCompletionHandler m_challengeCompletionHandler;
    ResponseCompletionHandler m_responseCompletionHandler;
    RedirectCompletionHandler m_redirectCompletionHandler;
#else
    bool m_waitingForContinueCanAuthenticateAgainstProtectionSpace { false };
    RefPtr<RemoteNetworkingContext> m_networkingContext;
#endif
    RefPtr<WebCore::ResourceHandle> m_handle;

    WebCore::ResourceRequest m_currentRequest; // Updated on redirects.
};

} // namespace WebKit

#endif // NetworkLoad_h

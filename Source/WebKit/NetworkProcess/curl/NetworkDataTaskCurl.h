/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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

#include "NetworkDataTask.h"
#include <WebCore/CurlRequestClient.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/ProtectionSpace.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/ShouldRelaxThirdPartyCookieBlocking.h>
#include <wtf/MonotonicTime.h>

namespace WebCore {
class CurlRequest;
}

namespace WebKit {

class NetworkDataTaskCurl final : public NetworkDataTask, public WebCore::CurlRequestClient {
public:
    static Ref<NetworkDataTask> create(NetworkSession& session, NetworkDataTaskClient& client, const WebCore::ResourceRequest& request, WebCore::FrameIdentifier frameID, WebCore::PageIdentifier pageID, WebCore::StoredCredentialsPolicy storedCredentialsPolicy, WebCore::ContentSniffingPolicy shouldContentSniff, WebCore::ContentEncodingSniffingPolicy shouldContentEncodingSniff, bool shouldClearReferrerOnHTTPSToHTTPRedirect, bool dataTaskIsForMainFrameNavigation, WebCore::ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking)
    {
        return adoptRef(*new NetworkDataTaskCurl(session, client, request, frameID, pageID, storedCredentialsPolicy, shouldContentSniff, shouldContentEncodingSniff, shouldClearReferrerOnHTTPSToHTTPRedirect, dataTaskIsForMainFrameNavigation, shouldRelaxThirdPartyCookieBlocking));
    }

    ~NetworkDataTaskCurl();

    void ref() override { NetworkDataTask::ref(); }
    void deref() override { NetworkDataTask::deref(); }

private:
    enum class RequestStatus {
        NewRequest,
        ReusedRequest
    };

    NetworkDataTaskCurl(NetworkSession&, NetworkDataTaskClient&,  const WebCore::ResourceRequest&, WebCore::FrameIdentifier, WebCore::PageIdentifier&, WebCore::StoredCredentialsPolicy, WebCore::ContentSniffingPolicy, WebCore::ContentEncodingSniffingPolicy, bool shouldClearReferrerOnHTTPSToHTTPRedirect, bool dataTaskIsForMainFrameNavigation, WebCore::ShouldRelaxThirdPartyCookieBlocking);

    void cancel() override;
    void resume() override;
    void invalidateAndCancel() override;
    NetworkDataTask::State state() const override;

    Ref<WebCore::CurlRequest> createCurlRequest(WebCore::ResourceRequest&&, RequestStatus = RequestStatus::NewRequest);
    void curlDidSendData(WebCore::CurlRequest&, unsigned long long, unsigned long long) override;
    void curlDidReceiveResponse(WebCore::CurlRequest&, WebCore::CurlResponse&&) override;
    void curlDidReceiveBuffer(WebCore::CurlRequest&, Ref<WebCore::SharedBuffer>&&) override;
    void curlDidComplete(WebCore::CurlRequest&, WebCore::NetworkLoadMetrics&&) override;
    void curlDidFailWithError(WebCore::CurlRequest&, WebCore::ResourceError&&, WebCore::CertificateInfo&&) override;

    void invokeDidReceiveResponse();

    bool shouldRedirectAsGET(const WebCore::ResourceRequest&, bool crossOrigin);
    void willPerformHTTPRedirection();

    void tryHttpAuthentication(WebCore::AuthenticationChallenge&&);
    void tryProxyAuthentication(WebCore::AuthenticationChallenge&&);
    void restartWithCredential(const WebCore::ProtectionSpace&, const WebCore::Credential&);

    void tryServerTrustEvaluation(WebCore::AuthenticationChallenge&&);

    void appendCookieHeader(WebCore::ResourceRequest&);
    void handleCookieHeaders(const WebCore::ResourceRequest&, const WebCore::CurlResponse&);

    bool isThirdPartyRequest(const WebCore::ResourceRequest&);
    bool shouldBlockCookies(const WebCore::ResourceRequest&);
    void blockCookies();
    void unblockCookies();

    State m_state { State::Suspended };

    RefPtr<WebCore::CurlRequest> m_curlRequest;
    WebCore::ResourceResponse m_response;
    unsigned m_redirectCount { 0 };
    unsigned m_authFailureCount { 0 };
    MonotonicTime m_startTime;

    WebCore::FrameIdentifier m_frameID;
    WebCore::PageIdentifier m_pageID;

    bool m_blockingCookies { false };

    WebCore::ShouldRelaxThirdPartyCookieBlocking m_shouldRelaxThirdPartyCookieBlocking { WebCore::ShouldRelaxThirdPartyCookieBlocking::No };
};

} // namespace WebKit

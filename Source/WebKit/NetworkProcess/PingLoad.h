/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if USE(NETWORK_SESSION)

#include "NetworkDataTask.h"
#include "NetworkResourceLoadParameters.h"
#include <WebCore/ContentExtensionsBackend.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceResponse.h>
#include <wtf/CompletionHandler.h>

namespace WebCore {
class ContentSecurityPolicy;
class HTTPHeaderMap;
class URL;
}

namespace WebKit {

class NetworkCORSPreflightChecker;

class PingLoad final : private NetworkDataTaskClient {
public:
    PingLoad(NetworkResourceLoadParameters&&, WebCore::HTTPHeaderMap&& originalRequestHeaders, WTF::CompletionHandler<void(const WebCore::ResourceError&, const WebCore::ResourceResponse&)>&&);
    
private:
    ~PingLoad();

    WebCore::ContentSecurityPolicy* contentSecurityPolicy() const;

    void willPerformHTTPRedirection(WebCore::ResourceResponse&&, WebCore::ResourceRequest&&, RedirectCompletionHandler&&) final;
    void didReceiveChallenge(const WebCore::AuthenticationChallenge&, ChallengeCompletionHandler&&) final;
    void didReceiveResponseNetworkSession(WebCore::ResourceResponse&&, ResponseCompletionHandler&&) final;
    void didReceiveData(Ref<WebCore::SharedBuffer>&&) final;
    void didCompleteWithError(const WebCore::ResourceError&, const WebCore::NetworkLoadMetrics&) final;
    void didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend) final;
    void wasBlocked() final;
    void cannotShowURL() final;
    void timeoutTimerFired();

    void loadRequest(WebCore::ResourceRequest&&);
    bool isAllowedRedirect(const WebCore::URL&) const;
    void makeCrossOriginAccessRequest(WebCore::ResourceRequest&&);
    void makeSimpleCrossOriginAccessRequest(WebCore::ResourceRequest&&);
    void makeCrossOriginAccessRequestWithPreflight(WebCore::ResourceRequest&&);
    void preflightSuccess(WebCore::ResourceRequest&&);

#if ENABLE(CONTENT_EXTENSIONS)
    WebCore::ContentExtensions::ContentExtensionsBackend& contentExtensionsBackend();
    WebCore::ContentExtensions::BlockedStatus processContentExtensionRulesForLoad(WebCore::ResourceRequest&);
#endif

    WebCore::SecurityOrigin& securityOrigin() const;

    const WebCore::ResourceRequest& currentRequest() const;
    void didFinish(const WebCore::ResourceError& = { }, const WebCore::ResourceResponse& response = { });
    
    NetworkResourceLoadParameters m_parameters;
    WebCore::HTTPHeaderMap m_originalRequestHeaders; // Needed for CORS checks.
    WTF::CompletionHandler<void(const WebCore::ResourceError&, const WebCore::ResourceResponse&)> m_completionHandler;
    RefPtr<NetworkDataTask> m_task;
    WebCore::Timer m_timeoutTimer;
    std::unique_ptr<NetworkCORSPreflightChecker> m_corsPreflightChecker;
    RefPtr<WebCore::SecurityOrigin> m_origin;
    bool m_isSameOriginRequest;
    bool m_isSimpleRequest { true };
    RedirectCompletionHandler m_redirectHandler;
    mutable std::unique_ptr<WebCore::ContentSecurityPolicy> m_contentSecurityPolicy;
#if ENABLE(CONTENT_EXTENSIONS)
    std::unique_ptr<WebCore::ContentExtensions::ContentExtensionsBackend> m_contentExtensionsBackend;
#endif
    std::optional<WebCore::ResourceRequest> m_lastRedirectionRequest;
};

}

#endif // USE(NETWORK_SESSION)

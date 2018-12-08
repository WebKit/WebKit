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

#include "NetworkDataTask.h"
#include <WebCore/NetworkLoadInformation.h>
#include <WebCore/StoredCredentialsPolicy.h>
#include <pal/SessionID.h>
#include <wtf/CompletionHandler.h>

namespace WebCore {
class ResourceError;
class SecurityOrigin;
}

namespace WebKit {

class NetworkCORSPreflightChecker final : private NetworkDataTaskClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    struct Parameters {
        WebCore::ResourceRequest originalRequest;
        Ref<WebCore::SecurityOrigin> sourceOrigin;
        String referrer;
        String userAgent;
        PAL::SessionID sessionID;
        uint64_t pageID;
        uint64_t frameID;
        WebCore::StoredCredentialsPolicy storedCredentialsPolicy;
    };
    using CompletionCallback = CompletionHandler<void(WebCore::ResourceError&&)>;

    NetworkCORSPreflightChecker(Parameters&&, bool shouldCaptureExtraNetworkLoadMetrics, CompletionCallback&&);
    ~NetworkCORSPreflightChecker();
    const WebCore::ResourceRequest& originalRequest() const { return m_parameters.originalRequest; }

    void startPreflight();

    WebCore::NetworkTransactionInformation takeInformation();

private:
    void willPerformHTTPRedirection(WebCore::ResourceResponse&&, WebCore::ResourceRequest&&, RedirectCompletionHandler&&) final;
    void didReceiveChallenge(WebCore::AuthenticationChallenge&&, ChallengeCompletionHandler&&) final;
    void didReceiveResponse(WebCore::ResourceResponse&&, ResponseCompletionHandler&&) final;
    void didReceiveData(Ref<WebCore::SharedBuffer>&&) final;
    void didCompleteWithError(const WebCore::ResourceError&, const WebCore::NetworkLoadMetrics&) final;
    void didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend) final;
    void wasBlocked() final;
    void cannotShowURL() final;

    Parameters m_parameters;
    WebCore::ResourceResponse m_response;
    CompletionCallback m_completionCallback;
    RefPtr<NetworkDataTask> m_task;
    bool m_shouldCaptureExtraNetworkLoadMetrics { false };
    WebCore::NetworkTransactionInformation m_loadInformation;
};

} // namespace WebKit

/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

#include "NetworkActivityTracker.h"
#include "NetworkDataTask.h"
#include "NetworkLoadParameters.h"
#include "NetworkProximityAssertion.h"
#include <WebCore/NetworkLoadMetrics.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS NSHTTPCookieStorage;
OBJC_CLASS NSURLSessionDataTask;

namespace WebKit {

class Download;
class NetworkSessionCocoa;

class NetworkDataTaskCocoa final : public NetworkDataTask {
    friend class NetworkSessionCocoa;
public:
    static Ref<NetworkDataTask> create(NetworkSession& session, NetworkDataTaskClient& client, const WebCore::ResourceRequest& request, uint64_t frameID, uint64_t pageID, WebCore::StoredCredentialsPolicy storedCredentialsPolicy, WebCore::ContentSniffingPolicy shouldContentSniff, WebCore::ContentEncodingSniffingPolicy shouldContentEncodingSniff, bool shouldClearReferrerOnHTTPSToHTTPRedirect, PreconnectOnly shouldPreconnectOnly, bool dataTaskIsForMainFrameNavigation, Optional<NetworkActivityTracker> networkActivityTracker)
    {
        return adoptRef(*new NetworkDataTaskCocoa(session, client, request, frameID, pageID, storedCredentialsPolicy, shouldContentSniff, shouldContentEncodingSniff, shouldClearReferrerOnHTTPSToHTTPRedirect, shouldPreconnectOnly, dataTaskIsForMainFrameNavigation, networkActivityTracker));
    }

    ~NetworkDataTaskCocoa();

    typedef uint64_t TaskIdentifier;

    void didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend);
    void didReceiveChallenge(WebCore::AuthenticationChallenge&&, ChallengeCompletionHandler&&);
    void didCompleteWithError(const WebCore::ResourceError&, const WebCore::NetworkLoadMetrics&);
    void didReceiveData(Ref<WebCore::SharedBuffer>&&);

    void willPerformHTTPRedirection(WebCore::ResourceResponse&&, WebCore::ResourceRequest&&, RedirectCompletionHandler&&);
    void transferSandboxExtensionToDownload(Download&);

    void cancel() override;
    void resume() override;
    void invalidateAndCancel() override { }
    NetworkDataTask::State state() const override;

    void setPendingDownloadLocation(const String&, SandboxExtension::Handle&&, bool /*allowOverwrite*/) override;
    String suggestedFilename() const override;

    WebCore::NetworkLoadMetrics& networkLoadMetrics() { return m_networkLoadMetrics; }

    uint64_t frameID() const { return m_frameID; };
    uint64_t pageID() const { return m_pageID; };

#if ENABLE(PROXIMITY_NETWORKING)
    void holdProximityAssertion(NetworkProximityAssertion& assertion)
    {
        ASSERT(!m_proximityAssertionToken);
        m_proximityAssertionToken.emplace(assertion);
    }
#endif

    String description() const override;

    void setShouldExtendTaskLifetime(bool shouldExtendTaskLifetime) { m_shouldExtendTaskLifetime = shouldExtendTaskLifetime; }

private:
    NetworkDataTaskCocoa(NetworkSession&, NetworkDataTaskClient&, const WebCore::ResourceRequest&, uint64_t frameID, uint64_t pageID, WebCore::StoredCredentialsPolicy, WebCore::ContentSniffingPolicy, WebCore::ContentEncodingSniffingPolicy, bool shouldClearReferrerOnHTTPSToHTTPRedirect, PreconnectOnly, bool dataTaskIsForMainFrameNavigation, Optional<NetworkActivityTracker>);

    bool tryPasswordBasedAuthentication(const WebCore::AuthenticationChallenge&, ChallengeCompletionHandler&);
    void applySniffingPoliciesAndBindRequestToInferfaceIfNeeded(__strong NSURLRequest*&, bool shouldContentSniff, bool shouldContentEncodingSniff);

    void restrictRequestReferrerToOriginIfNeeded(WebCore::ResourceRequest&, bool shouldBlockCookies);

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    static NSHTTPCookieStorage *statelessCookieStorage();
    void applyCookieBlockingPolicy(bool shouldBlock);
#endif
    bool isThirdPartyRequest(const WebCore::ResourceRequest&);
    bool isAlwaysOnLoggingAllowed() const;

    RefPtr<SandboxExtension> m_sandboxExtension;
    RetainPtr<NSURLSessionDataTask> m_task;
    WebCore::NetworkLoadMetrics m_networkLoadMetrics;
    uint64_t m_frameID;
    uint64_t m_pageID;

    // FIXME: This is only needed until rdar://problem/28233746 is fixed.
    bool m_shouldExtendTaskLifetime { false };

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    bool m_hasBeenSetToUseStatelessCookieStorage { false };
#endif

#if ENABLE(PROXIMITY_NETWORKING)
    Optional<NetworkProximityAssertion::Token> m_proximityAssertionToken;
#endif
};

WebCore::Credential serverTrustCredential(const WebCore::AuthenticationChallenge&);

} // namespace WebKit

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
#include "NetworkTaskCocoa.h"
#include <WebCore/NetworkLoadMetrics.h>
#include <WebCore/PrivateClickMeasurement.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS NSHTTPCookieStorage;
OBJC_CLASS NSURLSessionDataTask;
OBJC_CLASS NSMutableURLRequest;

namespace WebCore {
class RegistrableDomain;
class SharedBuffer;
enum class AdvancedPrivacyProtections : uint16_t;
}

namespace WebKit {

class Download;
class NetworkSessionCocoa;
struct SessionWrapper;

class NetworkDataTaskCocoa final : public NetworkDataTask, public NetworkTaskCocoa {
public:
    static Ref<NetworkDataTask> create(NetworkSession& session, NetworkDataTaskClient& client, const NetworkLoadParameters& parameters)
    {
        return adoptRef(*new NetworkDataTaskCocoa(session, client, parameters));
    }

    ~NetworkDataTaskCocoa();

    using TaskIdentifier = uint64_t;

    void didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend);
    void didReceiveChallenge(WebCore::AuthenticationChallenge&&, NegotiatedLegacyTLS, ChallengeCompletionHandler&&);
    void didNegotiateModernTLS(const URL&);
    void didCompleteWithError(const WebCore::ResourceError&, const WebCore::NetworkLoadMetrics&);
    void didReceiveResponse(WebCore::ResourceResponse&&, NegotiatedLegacyTLS, PrivateRelayed, ResponseCompletionHandler&&);
    void didReceiveData(const WebCore::SharedBuffer&);

    void willPerformHTTPRedirection(WebCore::ResourceResponse&&, WebCore::ResourceRequest&&, RedirectCompletionHandler&&);
    void transferSandboxExtensionToDownload(Download&);

    void cancel() override;
    void resume() override;
    void invalidateAndCancel() override { }
    NetworkDataTask::State state() const override;

    void setPendingDownloadLocation(const String&, SandboxExtension::Handle&&, bool /*allowOverwrite*/) override;
    String suggestedFilename() const override;

    WebCore::NetworkLoadMetrics& networkLoadMetrics() { return m_networkLoadMetrics; }

    std::optional<WebCore::FrameIdentifier> frameID() const final { return m_frameID; };
    std::optional<WebCore::PageIdentifier> pageID() const final { return m_pageID; };

    String description() const override;

    void setH2PingCallback(const URL&, CompletionHandler<void(Expected<WTF::Seconds, WebCore::ResourceError>&&)>&&) override;
    void setPriority(WebCore::ResourceLoadPriority) override;
#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    void setEmulatedConditions(const std::optional<int64_t>& bytesPerSecondLimit) override;
#endif

    void checkTAO(const WebCore::ResourceResponse&);

private:
    NetworkDataTaskCocoa(NetworkSession&, NetworkDataTaskClient&, const NetworkLoadParameters&);

    bool tryPasswordBasedAuthentication(const WebCore::AuthenticationChallenge&, ChallengeCompletionHandler&);
    void applySniffingPoliciesAndBindRequestToInferfaceIfNeeded(RetainPtr<NSURLRequest>&, bool shouldContentSniff, WebCore::ContentEncodingSniffingPolicy);

    void updateFirstPartyInfoForSession(const URL&);

    NSURLSessionTask* task() const final;
    WebCore::StoredCredentialsPolicy storedCredentialsPolicy() const final { return m_storedCredentialsPolicy; }

    WeakPtr<SessionWrapper> m_sessionWrapper;
    RefPtr<SandboxExtension> m_sandboxExtension;
    RetainPtr<NSURLSessionDataTask> m_task;
    WebCore::NetworkLoadMetrics m_networkLoadMetrics;
    WebCore::FrameIdentifier m_frameID;
    WebCore::PageIdentifier m_pageID;
    WebPageProxyIdentifier m_webPageProxyID;

    bool m_isForMainResourceNavigationForAnyFrame { false };
    RefPtr<WebCore::SecurityOrigin> m_sourceOrigin;
};

WebCore::Credential serverTrustCredential(const WebCore::AuthenticationChallenge&);
void setPCMDataCarriedOnRequest(WebCore::PrivateClickMeasurement::PcmDataCarried, NSMutableURLRequest *);

void enableAdvancedPrivacyProtections(NSMutableURLRequest *, OptionSet<WebCore::AdvancedPrivacyProtections>);

} // namespace WebKit

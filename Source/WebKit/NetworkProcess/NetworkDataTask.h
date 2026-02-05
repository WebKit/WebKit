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

#include "DownloadID.h"
#include "NetworkSession.h"
#include "SandboxExtension.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/Credential.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/NetworkLoadMetrics.h>
#include <WebCore/ResourceLoaderOptions.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/StoredCredentialsPolicy.h>
#include <pal/SessionID.h>
#include <wtf/AbstractRefCountedAndCanMakeWeakPtr.h>
#include <wtf/CheckedPtr.h>
#include <wtf/CompletionHandler.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class AuthenticationChallenge;
class IPAddress;
class ResourceError;
class ResourceResponse;
class SharedBuffer;
}

namespace WebKit {

class NetworkSession;
class PendingDownload;

enum class AuthenticationChallengeDisposition : uint8_t;
enum class NegotiatedLegacyTLS : bool;
enum class PrivateRelayed : bool;

struct NetworkLoadParameters;

using RedirectCompletionHandler = CompletionHandler<void(WebCore::ResourceRequest&&)>;
using ChallengeCompletionHandler = CompletionHandler<void(AuthenticationChallengeDisposition, const WebCore::Credential&)>;
using ResponseCompletionHandler = CompletionHandler<void(WebCore::PolicyAction)>;

class NetworkDataTaskClient : public AbstractRefCountedAndCanMakeWeakPtr<NetworkDataTaskClient> {
public:
    virtual void willPerformHTTPRedirection(WebCore::ResourceResponse&&, WebCore::ResourceRequest&&, RedirectCompletionHandler&&) = 0;
    virtual void didReceiveChallenge(WebCore::AuthenticationChallenge&&, NegotiatedLegacyTLS, ChallengeCompletionHandler&&) = 0;
    virtual void didReceiveInformationalResponse(WebCore::ResourceResponse&&) { };
    virtual void didReceiveResponse(WebCore::ResourceResponse&&, NegotiatedLegacyTLS, PrivateRelayed, ResponseCompletionHandler&&) = 0;
    virtual void didReceiveData(const WebCore::SharedBuffer&) = 0;
    virtual void didCompleteWithError(const WebCore::ResourceError&, const WebCore::NetworkLoadMetrics&) = 0;
    virtual void didSendData(uint64_t totalBytesSent, uint64_t totalBytesExpectedToSend) = 0;
    virtual void wasBlocked() = 0;
    virtual void cannotShowURL() = 0;
    virtual void wasBlockedByRestrictions() = 0;
    virtual void wasBlockedByDisabledFTP() = 0;

    virtual bool shouldCaptureExtraNetworkLoadMetrics() const { return false; }

    virtual void didNegotiateModernTLS(const URL&) { }

    void didCompleteWithError(const WebCore::ResourceError& error)
    {
        WebCore::NetworkLoadMetrics emptyMetrics;
        didCompleteWithError(error, emptyMetrics);
    }

    virtual ~NetworkDataTaskClient() { }
};

class NetworkDataTask : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<NetworkDataTask, WTF::DestructionThread::Main> {
public:
    static Ref<NetworkDataTask> create(NetworkSession&, NetworkDataTaskClient&, const NetworkLoadParameters&);

    virtual ~NetworkDataTask();

    virtual void cancel() = 0;
    virtual void resume() = 0;
    virtual void invalidateAndCancel() = 0;

    void didReceiveInformationalResponse(WebCore::ResourceResponse&&);
    void didReceiveResponse(WebCore::ResourceResponse&&, NegotiatedLegacyTLS, PrivateRelayed, std::optional<WebCore::IPAddress>, ResponseCompletionHandler&&);
    bool shouldCaptureExtraNetworkLoadMetrics() const;

    enum class State {
        Running,
        Suspended,
        Canceling,
        Completed
    };
    virtual State state() const = 0;

    NetworkDataTaskClient* client() const { return m_client.get(); }
    void clearClient() { m_client = nullptr; }

    std::optional<DownloadID> pendingDownloadID() const { return m_pendingDownloadID.asOptional(); }
    PendingDownload* pendingDownload() const;
    void setPendingDownloadID(DownloadID downloadID)
    {
        ASSERT(!m_pendingDownloadID);
        m_pendingDownloadID = downloadID;
    }
    void setPendingDownload(PendingDownload&);

    virtual void setPendingDownloadLocation(const String& filename, SandboxExtension::Handle&&, bool /*allowOverwrite*/) { m_pendingDownloadLocation = filename; }
    const String& pendingDownloadLocation() const { return m_pendingDownloadLocation; }
    bool isDownload() const { return !!m_pendingDownloadID; }

    const WebCore::ResourceRequest& firstRequest() const { return m_firstRequest; }
    virtual String suggestedFilename() const { return String(); }
    void setSuggestedFilename(const String& suggestedName) { m_suggestedFilename = suggestedName; }
    const String& partition() { return m_partition; }

    bool isTopLevelNavigation() const { return m_dataTaskIsForMainFrameNavigation; }

    bool isInitiatedByDedicatedWorker() const { return m_isInitiatedByDedicatedWorker; }

    virtual String description() const;
    virtual void setH2PingCallback(const URL&, CompletionHandler<void(Expected<WTF::Seconds, WebCore::ResourceError>&&)>&&);

    virtual void setPriority(WebCore::ResourceLoadPriority) { }
    String attributedBundleIdentifier(WebPageProxyIdentifier);

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    virtual void setEmulatedConditions(const std::optional<int64_t>& /* bytesPerSecondLimit */) { }
#endif

    PAL::SessionID sessionID() const { return m_session->sessionID(); }
    const NetworkSession* networkSession() const { return m_session.get(); }
    NetworkSession* networkSession() { return m_session.get(); }

    CheckedPtr<NetworkSession> checkedNetworkSession()
    {
        ASSERT(m_session);
        return m_session.get();
    }

    virtual void setTimingAllowFailedFlag() { }

    size_t bytesTransferredOverNetwork() const { return m_bytesTransferredOverNetwork; }

protected:
    NetworkDataTask(NetworkSession&, NetworkDataTaskClient&, const WebCore::ResourceRequest&, WebCore::StoredCredentialsPolicy, bool shouldClearReferrerOnHTTPSToHTTPRedirect, bool dataTaskIsForMainFrameNavigation, bool isInitiatedByDedicatedWorker);

    enum class FailureType : uint8_t {
        Blocked,
        InvalidURL,
        RestrictedURL,
        FTPDisabled
    };
    void scheduleFailure(FailureType);

    void restrictRequestReferrerToOriginIfNeeded(WebCore::ResourceRequest&);
    void setBytesTransferredOverNetwork(size_t bytes) { m_bytesTransferredOverNetwork = bytes; }

    const WeakPtr<NetworkSession> m_session;
    WeakPtr<NetworkDataTaskClient> m_client;
    WeakPtr<PendingDownload> m_pendingDownload;
    Markable<DownloadID> m_pendingDownloadID;
    String m_user;
    String m_password;
    String m_partition;
    WebCore::Credential m_initialCredential;
    WebCore::StoredCredentialsPolicy m_storedCredentialsPolicy { WebCore::StoredCredentialsPolicy::DoNotUse };
    String m_lastHTTPMethod;
    String m_pendingDownloadLocation;
    WebCore::ResourceRequest m_firstRequest;
    WebCore::ResourceRequest m_previousRequest;
    String m_suggestedFilename;
    size_t m_bytesTransferredOverNetwork { 0 };
    bool m_shouldClearReferrerOnHTTPSToHTTPRedirect { true };
    bool m_dataTaskIsForMainFrameNavigation { false };
    bool m_failureScheduled { false };
    bool m_isInitiatedByDedicatedWorker { false };
};

} // namespace WebKit

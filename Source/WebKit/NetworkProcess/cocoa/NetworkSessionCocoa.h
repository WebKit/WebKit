/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

OBJC_CLASS DMFWebsitePolicyMonitor;
OBJC_CLASS NSData;
OBJC_CLASS NSURLSession;
OBJC_CLASS NSURLSessionConfiguration;
OBJC_CLASS NSURLSessionDownloadTask;
OBJC_CLASS NSOperationQueue;
OBJC_CLASS WKNetworkSessionDelegate;
OBJC_CLASS WKNetworkSessionWebSocketDelegate;

#include "DownloadID.h"
#include "NetworkDataTaskCocoa.h"
#include "NetworkSession.h"
#include "WebSocketTask.h"
#include <WebCore/NetworkLoadMetrics.h>
#include <WebCore/RegistrableDomain.h>
#include <wtf/HashMap.h>
#include <wtf/Seconds.h>

namespace WebKit {

enum class NegotiatedLegacyTLS : bool;
class LegacyCustomProtocolManager;
class NetworkSessionCocoa;
using HostAndPort = std::pair<String, uint16_t>;

struct SessionWrapper : public CanMakeWeakPtr<SessionWrapper> {
    void initialize(NSURLSessionConfiguration *, NetworkSessionCocoa&, WebCore::StoredCredentialsPolicy, NavigatingToAppBoundDomain);

    RetainPtr<NSURLSession> session;
    RetainPtr<WKNetworkSessionDelegate> delegate;
    HashMap<NetworkDataTaskCocoa::TaskIdentifier, NetworkDataTaskCocoa*> dataTaskMap;
    HashMap<NetworkDataTaskCocoa::TaskIdentifier, DownloadID> downloadMap;
#if HAVE(NSURLSESSION_WEBSOCKET)
    HashMap<NetworkDataTaskCocoa::TaskIdentifier, WebSocketTask*> webSocketDataTaskMap;
#endif
};

class NetworkSessionCocoa final : public NetworkSession {
public:
    static std::unique_ptr<NetworkSession> create(NetworkProcess&, NetworkSessionCreationParameters&&);

    NetworkSessionCocoa(NetworkProcess&, NetworkSessionCreationParameters&&);
    ~NetworkSessionCocoa();

    void initializeEphemeralStatelessSession(NavigatingToAppBoundDomain);

    const String& boundInterfaceIdentifier() const;
    const String& sourceApplicationBundleIdentifier() const;
    const String& sourceApplicationSecondaryIdentifier() const;
#if PLATFORM(IOS_FAMILY)
    const String& dataConnectionServiceType() const;
#endif

    static bool allowsSpecificHTTPSCertificateForHost(const WebCore::AuthenticationChallenge&);

    void continueDidReceiveChallenge(SessionWrapper&, const WebCore::AuthenticationChallenge&, NegotiatedLegacyTLS, NetworkDataTaskCocoa::TaskIdentifier, NetworkDataTaskCocoa*, CompletionHandler<void(WebKit::AuthenticationChallengeDisposition, const WebCore::Credential&)>&&);

    SessionWrapper& sessionWrapperForDownloads() { return m_sessionWithCredentialStorage; }

    bool fastServerTrustEvaluationEnabled() const { return m_fastServerTrustEvaluationEnabled; }
    bool deviceManagementRestrictionsEnabled() const { return m_deviceManagementRestrictionsEnabled; }
    bool allLoadsBlockedByDeviceManagementRestrictionsForTesting() const { return m_allLoadsBlockedByDeviceManagementRestrictionsForTesting; }
    DMFWebsitePolicyMonitor *deviceManagementPolicyMonitor();

    CFDictionaryRef proxyConfiguration() const { return m_proxyConfiguration.get(); }

    bool hasIsolatedSession(const WebCore::RegistrableDomain) const override;
    void clearIsolatedSessions() override;

    bool hasAppBoundSession() const override { return !!m_appBoundSession; }

    SessionWrapper& sessionWrapperForTask(const WebCore::ResourceRequest&, WebCore::StoredCredentialsPolicy, Optional<NavigatingToAppBoundDomain>);
    void clearAppBoundSession() override;
    bool isInAppBrowserPrivacyEnabled() const { return m_isInAppBrowserPrivacyEnabled; }
    bool preventsSystemHTTPProxyAuthentication() const { return m_preventsSystemHTTPProxyAuthentication; }
    
    void clientCertificateSuggestedForHost(NetworkDataTaskCocoa::TaskIdentifier, NSURLCredential *, const String& host, uint16_t port);
    void taskServerConnectionSucceeded(NetworkDataTaskCocoa::TaskIdentifier);
    void taskFailed(NetworkDataTaskCocoa::TaskIdentifier);
    NSURLCredential *successfulClientCertificateForHost(const String& host, uint16_t port) const;

private:
    void invalidateAndCancel() override;
    void clearCredentials() override;
    bool shouldLogCookieInformation() const override { return m_shouldLogCookieInformation; }
    Seconds loadThrottleLatency() const override { return m_loadThrottleLatency; }
    SessionWrapper& isolatedSession(WebCore::StoredCredentialsPolicy, const WebCore::RegistrableDomain, NavigatingToAppBoundDomain);
    SessionWrapper& appBoundSession(WebCore::StoredCredentialsPolicy);

    Vector<WebCore::SecurityOriginData> hostNamesWithAlternativeServices() const override;
    void deleteAlternativeServicesForHostNames(const Vector<String>&) override;
    void clearAlternativeServices(WallTime) override;

#if HAVE(NSURLSESSION_WEBSOCKET)
    std::unique_ptr<WebSocketTask> createWebSocketTask(NetworkSocketChannel&, const WebCore::ResourceRequest&, const String& protocol) final;
    void addWebSocketTask(WebSocketTask&) final;
    void removeWebSocketTask(WebSocketTask&) final;
#endif

    struct IsolatedSession {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        SessionWrapper sessionWithCredentialStorage;
        SessionWrapper sessionWithoutCredentialStorage;
        WallTime lastUsed;
    };

    HashMap<WebCore::RegistrableDomain, std::unique_ptr<IsolatedSession>> m_isolatedSessions;
    std::unique_ptr<IsolatedSession> m_appBoundSession;

    SessionWrapper m_sessionWithCredentialStorage;
    SessionWrapper m_sessionWithoutCredentialStorage;
    SessionWrapper m_ephemeralStatelessSession;

    String m_boundInterfaceIdentifier;
    String m_sourceApplicationBundleIdentifier;
    String m_sourceApplicationSecondaryIdentifier;
    RetainPtr<CFDictionaryRef> m_proxyConfiguration;
    RetainPtr<DMFWebsitePolicyMonitor> m_deviceManagementPolicyMonitor;
    bool m_deviceManagementRestrictionsEnabled { false };
    bool m_allLoadsBlockedByDeviceManagementRestrictionsForTesting { false };
    bool m_shouldLogCookieInformation { false };
    Seconds m_loadThrottleLatency;
    bool m_fastServerTrustEvaluationEnabled { false };
    String m_dataConnectionServiceType;
    bool m_isInAppBrowserPrivacyEnabled { false };
    bool m_preventsSystemHTTPProxyAuthentication { false };

    struct SuggestedClientCertificate {
        String host;
        uint16_t port { 0 };
        RetainPtr<NSURLCredential> credential;
    };
    HashMap<NetworkDataTaskCocoa::TaskIdentifier, SuggestedClientCertificate> m_suggestedClientCertificates;
    HashMap<HostAndPort, RetainPtr<NSURLCredential>> m_successfulClientCertificates;
};

} // namespace WebKit

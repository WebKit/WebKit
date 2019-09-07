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

class LegacyCustomProtocolManager;

class NetworkSessionCocoa final : public NetworkSession {
    friend class NetworkDataTaskCocoa;
public:
    static std::unique_ptr<NetworkSession> create(NetworkProcess&, NetworkSessionCreationParameters&&);

    NetworkSessionCocoa(NetworkProcess&, NetworkSessionCreationParameters&&);
    ~NetworkSessionCocoa();

    void initializeEphemeralStatelessCookielessSession();

    const String& boundInterfaceIdentifier() const;
    const String& sourceApplicationBundleIdentifier() const;
    const String& sourceApplicationSecondaryIdentifier() const;
    // Must be called before any NetworkSession has been created.
    // FIXME: Move this to NetworkSessionCreationParameters.
#if PLATFORM(IOS_FAMILY)
    const String& ctDataConnectionServiceType() const;
    static void setCTDataConnectionServiceType(const String&);
#endif

    NetworkDataTaskCocoa* dataTaskForIdentifier(NetworkDataTaskCocoa::TaskIdentifier, WebCore::StoredCredentialsPolicy);
    NSURLSessionDownloadTask* downloadTaskWithResumeData(NSData*);

    WebSocketTask* webSocketDataTaskForIdentifier(WebSocketTask::TaskIdentifier);

    void addDownloadID(NetworkDataTaskCocoa::TaskIdentifier, DownloadID);
    DownloadID downloadID(NetworkDataTaskCocoa::TaskIdentifier);
    DownloadID takeDownloadID(NetworkDataTaskCocoa::TaskIdentifier);

    static bool allowsSpecificHTTPSCertificateForHost(const WebCore::AuthenticationChallenge&);

    void continueDidReceiveChallenge(const WebCore::AuthenticationChallenge&, NetworkDataTaskCocoa::TaskIdentifier, NetworkDataTaskCocoa*, CompletionHandler<void(WebKit::AuthenticationChallengeDisposition, const WebCore::Credential&)>&&);

    bool deviceManagementRestrictionsEnabled() const { return m_deviceManagementRestrictionsEnabled; }
    bool allLoadsBlockedByDeviceManagementRestrictionsForTesting() const { return m_allLoadsBlockedByDeviceManagementRestrictionsForTesting; }
    DMFWebsitePolicyMonitor *deviceManagementPolicyMonitor();

    CFDictionaryRef proxyConfiguration() const { return m_proxyConfiguration.get(); }

    NSURLSession* session(WebCore::StoredCredentialsPolicy);
    NSURLSession* isolatedSession(WebCore::StoredCredentialsPolicy, const WebCore::RegistrableDomain);
    bool hasIsolatedSession(const WebCore::RegistrableDomain) const override;
    void clearIsolatedSessions() override;

private:
    void invalidateAndCancel() override;
    void clearCredentials() override;
    bool shouldLogCookieInformation() const override { return m_shouldLogCookieInformation; }
    Seconds loadThrottleLatency() const override { return m_loadThrottleLatency; }

#if HAVE(NSURLSESSION_WEBSOCKET)
    std::unique_ptr<WebSocketTask> createWebSocketTask(NetworkSocketChannel&, const WebCore::ResourceRequest&, const String& protocol) final;
    void addWebSocketTask(WebSocketTask&) final;
    void removeWebSocketTask(WebSocketTask&) final;
#endif

    HashMap<NetworkDataTaskCocoa::TaskIdentifier, NetworkDataTaskCocoa*> m_dataTaskMapWithCredentials;
    HashMap<NetworkDataTaskCocoa::TaskIdentifier, NetworkDataTaskCocoa*> m_dataTaskMapWithoutState;
    HashMap<NetworkDataTaskCocoa::TaskIdentifier, NetworkDataTaskCocoa*> m_dataTaskMapEphemeralStatelessCookieless;
    HashMap<NetworkDataTaskCocoa::TaskIdentifier, DownloadID> m_downloadMap;
#if HAVE(NSURLSESSION_WEBSOCKET)
    HashMap<NetworkDataTaskCocoa::TaskIdentifier, WebSocketTask*> m_webSocketDataTaskMap;
#endif

    struct IsolatedSession {
        RetainPtr<NSURLSession> sessionWithCredentialStorage;
        RetainPtr<WKNetworkSessionDelegate> sessionWithCredentialStorageDelegate;
        RetainPtr<NSURLSession> statelessSession;
        RetainPtr<WKNetworkSessionDelegate> statelessSessionDelegate;
        WallTime lastUsed;
    };

    HashMap<WebCore::RegistrableDomain, IsolatedSession> m_isolatedSessions;

    RetainPtr<NSURLSession> m_sessionWithCredentialStorage;
    RetainPtr<WKNetworkSessionDelegate> m_sessionWithCredentialStorageDelegate;
    RetainPtr<NSURLSession> m_statelessSession;
    RetainPtr<WKNetworkSessionDelegate> m_statelessSessionDelegate;
    RetainPtr<NSURLSession> m_ephemeralStatelessCookielessSession;
    RetainPtr<WKNetworkSessionDelegate> m_ephemeralStatelessCookielessSessionDelegate;

    String m_boundInterfaceIdentifier;
    String m_sourceApplicationBundleIdentifier;
    String m_sourceApplicationSecondaryIdentifier;
    RetainPtr<CFDictionaryRef> m_proxyConfiguration;
    RetainPtr<DMFWebsitePolicyMonitor> m_deviceManagementPolicyMonitor;
    bool m_deviceManagementRestrictionsEnabled { false };
    bool m_allLoadsBlockedByDeviceManagementRestrictionsForTesting { false };
    bool m_shouldLogCookieInformation { false };
    Seconds m_loadThrottleLatency;
};

} // namespace WebKit

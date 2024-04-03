/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
OBJC_CLASS _NSHSTSStorage;
OBJC_CLASS NSURLCredentialStorage;

#include "DownloadID.h"
#include "NetworkDataTaskCocoa.h"
#include "NetworkSession.h"
#include "WebPageNetworkParameters.h"
#include "WebPageProxyIdentifier.h"
#include "WebSocketTask.h"
#include <WebCore/NetworkLoadMetrics.h>
#include <WebCore/RegistrableDomain.h>
#include <wtf/HashMap.h>
#include <wtf/Seconds.h>

namespace WebCore {
enum class AdvancedPrivacyProtections : uint16_t;
}

namespace WebKit {

enum class NegotiatedLegacyTLS : bool;
class LegacyCustomProtocolManager;
class NetworkSessionCocoa;

struct SessionWrapper : public CanMakeWeakPtr<SessionWrapper> {
    void initialize(NSURLSessionConfiguration *, NetworkSessionCocoa&, WebCore::StoredCredentialsPolicy, NavigatingToAppBoundDomain);

    void recreateSessionWithUpdatedProxyConfigurations(NetworkSessionCocoa&);

    RetainPtr<NSURLSession> session;
    RetainPtr<WKNetworkSessionDelegate> delegate;
    HashMap<NetworkDataTaskCocoa::TaskIdentifier, ThreadSafeWeakPtr<NetworkDataTaskCocoa>> dataTaskMap;
    HashMap<NetworkDataTaskCocoa::TaskIdentifier, DownloadID> downloadMap;
    HashMap<NetworkDataTaskCocoa::TaskIdentifier, WeakPtr<WebSocketTask>> webSocketDataTaskMap;
};

struct IsolatedSession {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SessionWrapper sessionWithCredentialStorage;
    WallTime lastUsed;
};

struct SessionSet : public RefCounted<SessionSet>, public CanMakeWeakPtr<SessionSet> {
public:
    static Ref<SessionSet> create()
    {
        return adoptRef(*new SessionSet);
    }

    SessionWrapper& initializeEphemeralStatelessSessionIfNeeded(NavigatingToAppBoundDomain, NetworkSessionCocoa&);

    SessionWrapper& isolatedSession(WebCore::StoredCredentialsPolicy, const WebCore::RegistrableDomain&, NavigatingToAppBoundDomain, NetworkSessionCocoa&);
    HashMap<WebCore::RegistrableDomain, std::unique_ptr<IsolatedSession>> isolatedSessions;

    std::unique_ptr<IsolatedSession> appBoundSession;

    SessionWrapper sessionWithCredentialStorage;
    SessionWrapper ephemeralStatelessSession;

private:

    SessionSet() = default;
};

class NetworkSessionCocoa final : public NetworkSession {
public:
    static std::unique_ptr<NetworkSession> create(NetworkProcess&, const NetworkSessionCreationParameters&);

    NetworkSessionCocoa(NetworkProcess&, const NetworkSessionCreationParameters&);
    ~NetworkSessionCocoa();

    SessionWrapper& initializeEphemeralStatelessSessionIfNeeded(WebPageProxyIdentifier, NavigatingToAppBoundDomain);

    const String& boundInterfaceIdentifier() const;
    const String& sourceApplicationBundleIdentifier() const;
    const String& sourceApplicationSecondaryIdentifier() const;
#if PLATFORM(IOS_FAMILY)
    const String& dataConnectionServiceType() const;
#endif

    void setClientAuditToken(const WebCore::AuthenticationChallenge&);

    void continueDidReceiveChallenge(SessionWrapper&, const WebCore::AuthenticationChallenge&, NegotiatedLegacyTLS, NetworkDataTaskCocoa::TaskIdentifier, RefPtr<NetworkDataTaskCocoa>, CompletionHandler<void(WebKit::AuthenticationChallengeDisposition, const WebCore::Credential&)>&&);

    SessionWrapper& sessionWrapperForDownloadResume() { return m_defaultSessionSet->sessionWithCredentialStorage; }

    bool fastServerTrustEvaluationEnabled() const { return m_fastServerTrustEvaluationEnabled; }
    bool deviceManagementRestrictionsEnabled() const { return m_deviceManagementRestrictionsEnabled; }
    bool allLoadsBlockedByDeviceManagementRestrictionsForTesting() const { return m_allLoadsBlockedByDeviceManagementRestrictionsForTesting; }

    DMFWebsitePolicyMonitor *deviceManagementPolicyMonitor();

    CFDictionaryRef proxyConfiguration() const { return m_proxyConfiguration.get(); }

    bool hasIsolatedSession(const WebCore::RegistrableDomain&) const override;
    void clearIsolatedSessions() override;

#if ENABLE(APP_BOUND_DOMAINS)
    bool hasAppBoundSession() const override;
    void clearAppBoundSession() override;
#endif

    SessionWrapper& sessionWrapperForTask(WebPageProxyIdentifier, const WebCore::ResourceRequest&, WebCore::StoredCredentialsPolicy, std::optional<NavigatingToAppBoundDomain>);
    bool preventsSystemHTTPProxyAuthentication() const { return m_preventsSystemHTTPProxyAuthentication; }
    
    _NSHSTSStorage *hstsStorage() const;
    NSURLCredentialStorage *nsCredentialStorage() const;

    void removeNetworkWebsiteData(std::optional<WallTime>, std::optional<HashSet<WebCore::RegistrableDomain>>&&, CompletionHandler<void()>&&) override;

    void removeDataTask(DataTaskIdentifier);
    void removeBlobDataTask(DataTaskIdentifier);

#if HAVE(NW_PROXY_CONFIG)
    const Vector<RetainPtr<nw_proxy_config_t>> proxyConfigs() const { return m_nwProxyConfigs; }

    void clearProxyConfigData() final;
    void setProxyConfigData(const Vector<std::pair<Vector<uint8_t>, WTF::UUID>>&) final;

    void applyProxyConfigurationToSessionConfiguration(NSURLSessionConfiguration *);
#endif

private:
    void invalidateAndCancel() override;
    HashSet<WebCore::SecurityOriginData> originsWithCredentials() final;
    void removeCredentialsForOrigins(const Vector<WebCore::SecurityOriginData>&) final;
    void clearCredentials(WallTime) final;

    bool shouldLogCookieInformation() const override { return m_shouldLogCookieInformation; }
    SessionWrapper& isolatedSession(WebPageProxyIdentifier, WebCore::StoredCredentialsPolicy, const WebCore::RegistrableDomain&, NavigatingToAppBoundDomain);

#if ENABLE(APP_BOUND_DOMAINS)
    SessionWrapper& appBoundSession(WebPageProxyIdentifier, WebCore::StoredCredentialsPolicy);
#endif

    void donateToSKAdNetwork(WebCore::PrivateClickMeasurement&&) final;

    Vector<WebCore::SecurityOriginData> hostNamesWithAlternativeServices() const override;
    void deleteAlternativeServicesForHostNames(const Vector<String>&) override;
    void clearAlternativeServices(WallTime) override;

    std::unique_ptr<WebSocketTask> createWebSocketTask(WebPageProxyIdentifier, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, NetworkSocketChannel&, const WebCore::ResourceRequest&, const String& protocol, const WebCore::ClientOrigin&, bool hadMainFrameMainResourcePrivateRelayed, bool allowPrivacyProxy, OptionSet<WebCore::AdvancedPrivacyProtections>, WebCore::ShouldRelaxThirdPartyCookieBlocking, WebCore::StoredCredentialsPolicy) final;
    void addWebSocketTask(WebPageProxyIdentifier, WebSocketTask&) final;
    void removeWebSocketTask(SessionSet&, WebSocketTask&) final;

    void dataTaskWithRequest(WebPageProxyIdentifier, WebCore::ResourceRequest&&, const std::optional<WebCore::SecurityOriginData>& topOrigin, CompletionHandler<void(DataTaskIdentifier)>&&) final;
    void cancelDataTask(DataTaskIdentifier) final;
    void addWebPageNetworkParameters(WebPageProxyIdentifier, WebPageNetworkParameters&&) final;
    void removeWebPageNetworkParameters(WebPageProxyIdentifier) final;
    size_t countNonDefaultSessionSets() const final;

    void forEachSessionWrapper(Function<void(SessionWrapper&)>&&);

    Ref<SessionSet> m_defaultSessionSet;
    HashMap<WebPageProxyIdentifier, Ref<SessionSet>> m_perPageSessionSets;
    HashMap<WebPageNetworkParameters, WeakPtr<SessionSet>> m_perParametersSessionSets;

    void initializeNSURLSessionsInSet(SessionSet&, NSURLSessionConfiguration *);
    SessionSet& sessionSetForPage(WebPageProxyIdentifier);
    const SessionSet& sessionSetForPage(WebPageProxyIdentifier) const;
    void invalidateAndCancelSessionSet(SessionSet&);
    
    String m_boundInterfaceIdentifier;
    String m_sourceApplicationBundleIdentifier;
    String m_sourceApplicationSecondaryIdentifier;
    RetainPtr<CFDictionaryRef> m_proxyConfiguration;
#if HAVE(NW_PROXY_CONFIG)
    Vector<RetainPtr<nw_proxy_config_t>> m_nwProxyConfigs;
#endif
    RetainPtr<DMFWebsitePolicyMonitor> m_deviceManagementPolicyMonitor;
    bool m_deviceManagementRestrictionsEnabled { false };
    bool m_allLoadsBlockedByDeviceManagementRestrictionsForTesting { false };
    bool m_shouldLogCookieInformation { false };
    bool m_fastServerTrustEvaluationEnabled { false };
    String m_dataConnectionServiceType;
    bool m_preventsSystemHTTPProxyAuthentication { false };

    class BlobDataTaskClient;
    HashMap<DataTaskIdentifier, UniqueRef<BlobDataTaskClient>> m_blobDataTasksForAPI;
    HashMap<DataTaskIdentifier, RetainPtr<NSURLSessionDataTask>> m_dataTasksForAPI;
};

} // namespace WebKit

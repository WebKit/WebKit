/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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

#include "Connection.h"
#include "DownloadID.h"
#include "NetworkActivityTracker.h"
#include "NetworkMDNSRegister.h"
#include "NetworkRTCProvider.h"
#include "NetworkResourceLoadIdentifier.h"
#include "NetworkResourceLoadMap.h"
#include "PolicyDecision.h"
#include "SandboxExtension.h"
#include "WebPageProxyIdentifier.h"
#include "WebPaymentCoordinatorProxy.h"
#include "WebResourceLoadObserver.h"
#include "WebSWServerToContextConnection.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <WebCore/ExceptionData.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/LayoutMilestone.h>
#include <WebCore/LoadSchedulingMode.h>
#include <WebCore/MessagePortChannelProvider.h>
#include <WebCore/MessagePortIdentifier.h>
#include <WebCore/NetworkLoadInformation.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/PushSubscriptionData.h>
#include <WebCore/PushSubscriptionIdentifier.h>
#include <WebCore/RTCDataChannelIdentifier.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/WebSocketIdentifier.h>
#include <optional>
#include <wtf/HashCountedSet.h>
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/URLHash.h>

#if ENABLE(IPC_TESTING_API)
#include "IPCTester.h"
#endif

namespace PAL {
class SessionID;
}

namespace WebCore {
class BlobDataFileReference;
class BlobPart;
class BlobRegistryImpl;
class MockContentFilterSettings;
class ResourceError;
class ResourceRequest;
enum class AdvancedPrivacyProtections : uint16_t;
enum class ApplyTrackingPrevention : bool;
enum class StorageAccessScope : bool;
struct ClientOrigin;
struct Cookie;
struct CookieStoreGetOptions;
struct PolicyContainer;
struct RequestStorageAccessResult;
struct SameSiteInfo;

enum class HTTPCookieAcceptPolicy : uint8_t;
enum class IncludeSecureCookies : bool;
}

namespace WebKit {

class NetworkOriginAccessPatterns;
class NetworkSchemeRegistry;
class NetworkProcess;
class NetworkResourceLoader;
class NetworkResourceLoadParameters;
class NetworkSession;
class NetworkSocketChannel;
class NetworkTransportSession;
class ServiceWorkerFetchTask;
class WebSWServerConnection;
class WebSWServerToContextConnection;
class WebSharedWorkerServerConnection;
class WebSharedWorkerServerToContextConnection;

struct CoreIPCAuditToken;
struct NetworkProcessConnectionParameters;
struct WebTransportSessionIdentifierType;

using WebTransportSessionIdentifier = ObjectIdentifier<WebTransportSessionIdentifierType>;

enum class PrivateRelayed : bool;

namespace NetworkCache {
struct DataKey;
}

class NetworkConnectionToWebProcess
    : public RefCounted<NetworkConnectionToWebProcess>
#if ENABLE(APPLE_PAY_REMOTE_UI)
    , public WebPaymentCoordinatorProxy::Client
#endif
#if HAVE(COOKIE_CHANGE_LISTENER_API)
    , public WebCore::CookieChangeObserver
#endif
    , public WebCore::CookiesEnabledStateObserver
    , public IPC::Connection::Client {
public:
    using MessageReceiver::weakPtrFactory;
    using MessageReceiver::WeakValueType;
    using MessageReceiver::WeakPtrImplType;
    using RegistrableDomain = WebCore::RegistrableDomain;

    static Ref<NetworkConnectionToWebProcess> create(NetworkProcess&, WebCore::ProcessIdentifier, PAL::SessionID, NetworkProcessConnectionParameters&&, IPC::Connection::Identifier);
    virtual ~NetworkConnectionToWebProcess();
    
    PAL::SessionID sessionID() const { return m_sessionID; }
    NetworkSession* networkSession();

    IPC::Connection& connection() { return m_connection.get(); }
    NetworkProcess& networkProcess() { return m_networkProcess.get(); }

    void didCleanupResourceLoader(NetworkResourceLoader&);
    void transferKeptAliveLoad(NetworkResourceLoader&);
    void setOnLineState(bool);

    bool captureExtraNetworkLoadMetricsEnabled() const { return m_captureExtraNetworkLoadMetricsEnabled; }

    RefPtr<WebCore::BlobDataFileReference> getBlobDataFileReferenceForPath(const String& path);

    void endSuspension();

    void getNetworkLoadInformationResponse(WebCore::ResourceLoaderIdentifier identifier, CompletionHandler<void(const WebCore::ResourceResponse&)>&& completionHandler)
    {
        if (auto* info = m_networkLoadInformationByID.get(identifier))
            return completionHandler(info->response);
        completionHandler({ });
    }

    void getNetworkLoadIntermediateInformation(WebCore::ResourceLoaderIdentifier identifier, CompletionHandler<void(const Vector<WebCore::NetworkTransactionInformation>&)>&& completionHandler)
    {
        if (auto* info = m_networkLoadInformationByID.get(identifier))
            return completionHandler(info->transactions);
        completionHandler({ });
    }

    void takeNetworkLoadInformationMetrics(WebCore::ResourceLoaderIdentifier identifier, CompletionHandler<void(const WebCore::NetworkLoadMetrics&)>&& completionHandler)
    {
        if (auto info = m_networkLoadInformationByID.take(identifier))
            return completionHandler(info->metrics);
        completionHandler({ });
    }

    void addNetworkLoadInformation(WebCore::ResourceLoaderIdentifier identifier, WebCore::NetworkLoadInformation&& information)
    {
        ASSERT(!m_networkLoadInformationByID.contains(identifier));
        m_networkLoadInformationByID.add(identifier, makeUnique<WebCore::NetworkLoadInformation>(WTFMove(information)));
    }

    void addNetworkLoadInformationMetrics(WebCore::ResourceLoaderIdentifier identifier, const WebCore::NetworkLoadMetrics& metrics)
    {
        ASSERT(m_networkLoadInformationByID.contains(identifier));
        m_networkLoadInformationByID.ensure(identifier, [] {
            return makeUnique<WebCore::NetworkLoadInformation>();
        }).iterator->value->metrics = metrics;
    }

    void removeNetworkLoadInformation(WebCore::ResourceLoaderIdentifier identifier)
    {
        m_networkLoadInformationByID.remove(identifier);
    }

    std::optional<NetworkActivityTracker> startTrackingResourceLoad(WebCore::PageIdentifier, WebCore::ResourceLoaderIdentifier resourceID, bool isTopResource);
    void stopTrackingResourceLoad(WebCore::ResourceLoaderIdentifier resourceID, NetworkActivityTracker::CompletionCode);

    Vector<RefPtr<WebCore::BlobDataFileReference>> resolveBlobReferences(const NetworkResourceLoadParameters&);

    void removeSocketChannel(WebCore::WebSocketIdentifier);

    WebCore::ProcessIdentifier webProcessIdentifier() const { return m_webProcessIdentifier; }

    void serviceWorkerServerToContextConnectionNoLongerNeeded();
    WebSWServerConnection* swConnection();
    RefPtr<ServiceWorkerFetchTask> createFetchTask(NetworkResourceLoader&, const WebCore::ResourceRequest&);
    void sharedWorkerServerToContextConnectionIsNoLongerNeeded();

    WebSharedWorkerServerConnection* sharedWorkerConnection();

    NetworkSchemeRegistry& schemeRegistry() { return m_schemeRegistry.get(); }

    void cookieAcceptPolicyChanged(WebCore::HTTPCookieAcceptPolicy);

    void broadcastConsoleMessage(JSC::MessageSource, JSC::MessageLevel, const String& message);
    RefPtr<NetworkResourceLoader> takeNetworkResourceLoader(WebCore::ResourceLoaderIdentifier);

    NetworkOriginAccessPatterns& originAccessPatterns() { return m_originAccessPatterns.get(); }

#if ENABLE(CONTENT_FILTERING)
    void installMockContentFilter(WebCore::MockContentFilterSettings&&);
#endif
#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)
    void logOnBehalfOfWebContent(IPC::DataReference&& logChannel, IPC::DataReference&& logCategory, IPC::DataReference&& logString, uint8_t logType, int32_t pid);
#endif

    void addAllowedFirstPartyForCookies(const RegistrableDomain&);
    void useRedirectionForCurrentNavigation(WebCore::ResourceLoaderIdentifier, WebCore::ResourceResponse&&);

#if ENABLE(WEB_RTC)
    NetworkMDNSRegister& mdnsRegister() { return m_mdnsRegister; }
#endif

    WebSWServerToContextConnection* swContextConnection() { return m_swContextConnection.get(); }

private:
    NetworkConnectionToWebProcess(NetworkProcess&, WebCore::ProcessIdentifier, PAL::SessionID, NetworkProcessConnectionParameters&&, IPC::Connection::Identifier);

    void didFinishPreconnection(WebCore::ResourceLoaderIdentifier preconnectionIdentifier, const WebCore::ResourceError&);
    WebCore::NetworkStorageSession* storageSession();

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) override;
    void didClose(IPC::Connection&) override;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName) override;

    // Message handlers.
    void didReceiveNetworkConnectionToWebProcessMessage(IPC::Connection&, IPC::Decoder&);
    bool didReceiveSyncNetworkConnectionToWebProcessMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&);

    void scheduleResourceLoad(NetworkResourceLoadParameters&&, std::optional<NetworkResourceLoadIdentifier> existingLoaderToResume);
    void performSynchronousLoad(NetworkResourceLoadParameters&&, CompletionHandler<void(const WebCore::ResourceError&, const WebCore::ResourceResponse, Vector<uint8_t>&&)>&&);
    void testProcessIncomingSyncMessagesWhenWaitingForSyncReply(WebPageProxyIdentifier, CompletionHandler<void(bool)>&&);
    void loadPing(NetworkResourceLoadParameters&&);
    void prefetchDNS(const String&);
    void sendH2Ping(NetworkResourceLoadParameters&&, CompletionHandler<void(Expected<WTF::Seconds, WebCore::ResourceError>&&)>&&);
    void preconnectTo(std::optional<WebCore::ResourceLoaderIdentifier> preconnectionIdentifier, NetworkResourceLoadParameters&&);
    void isResourceLoadFinished(WebCore::ResourceLoaderIdentifier, CompletionHandler<void(bool)>&&);

    void removeLoadIdentifier(WebCore::ResourceLoaderIdentifier);
    void pageLoadCompleted(WebCore::PageIdentifier);
    void browsingContextRemoved(WebPageProxyIdentifier, WebCore::PageIdentifier, WebCore::FrameIdentifier);
    void crossOriginRedirectReceived(WebCore::ResourceLoaderIdentifier, const URL& redirectURL);
    void startDownload(DownloadID, const WebCore::ResourceRequest&, const std::optional<WebCore::SecurityOriginData>& topOrigin, std::optional<NavigatingToAppBoundDomain>, const String& suggestedName = { });
    void convertMainResourceLoadToDownload(std::optional<WebCore::ResourceLoaderIdentifier> mainResourceLoadIdentifier, DownloadID, const WebCore::ResourceRequest&, const std::optional<WebCore::SecurityOriginData>& topOrigin, const WebCore::ResourceResponse&, std::optional<NavigatingToAppBoundDomain>);

    void registerURLSchemesAsCORSEnabled(Vector<String>&& schemes);

    void cookiesForDOM(const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, WebCore::FrameIdentifier, WebCore::PageIdentifier, WebCore::IncludeSecureCookies, WebCore::ApplyTrackingPrevention, WebCore::ShouldRelaxThirdPartyCookieBlocking, CompletionHandler<void(String cookieString, bool secureCookiesAccessed)>&&);
    void setCookiesFromDOM(const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, WebCore::FrameIdentifier, WebCore::PageIdentifier, WebCore::ApplyTrackingPrevention, const String& cookieString, WebCore::ShouldRelaxThirdPartyCookieBlocking);
    void cookieRequestHeaderFieldValue(const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, WebCore::IncludeSecureCookies, WebCore::ApplyTrackingPrevention, WebCore::ShouldRelaxThirdPartyCookieBlocking, CompletionHandler<void(String cookieString, bool secureCookiesAccessed)>&&);
    void getRawCookies(const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, WebCore::ApplyTrackingPrevention, WebCore::ShouldRelaxThirdPartyCookieBlocking, CompletionHandler<void(Vector<WebCore::Cookie>&&)>&&);
    void setRawCookie(const WebCore::Cookie&);
    void deleteCookie(const URL&, const String& cookieName, CompletionHandler<void()>&&);
    void cookiesEnabledSync(const URL& firstParty, const URL&, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, WebCore::ShouldRelaxThirdPartyCookieBlocking, CompletionHandler<void(bool enabled)>&&);
    void cookiesEnabled(const URL& firstParty, const URL&, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, WebCore::ShouldRelaxThirdPartyCookieBlocking, CompletionHandler<void(bool enabled)>&&);

    void cookiesForDOMAsync(const URL&, const WebCore::SameSiteInfo&, const URL&, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, WebCore::IncludeSecureCookies, WebCore::ApplyTrackingPrevention, WebCore::ShouldRelaxThirdPartyCookieBlocking, WebCore::CookieStoreGetOptions&&, CompletionHandler<void(std::optional<Vector<WebCore::Cookie>>&&)>&&);
    void setCookieFromDOMAsync(const URL&, const WebCore::SameSiteInfo&, const URL&, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, WebCore::ApplyTrackingPrevention, WebCore::Cookie&&, WebCore::ShouldRelaxThirdPartyCookieBlocking, CompletionHandler<void(bool)>&&);

    void registerInternalFileBlobURL(const URL&, const String& path, const String& replacementPath, SandboxExtension::Handle&&, const String& contentType);
    void registerInternalBlobURL(const URL&, Vector<WebCore::BlobPart>&&, const String& contentType);
    void registerBlobURL(const URL&, const URL& srcURL, WebCore::PolicyContainer&&, const std::optional<WebCore::SecurityOriginData>& topOrigin);
    void registerInternalBlobURLOptionallyFileBacked(const URL&, const URL& srcURL, const String& fileBackedPath, const String& contentType);
    void registerInternalBlobURLForSlice(const URL&, const URL& srcURL, int64_t start, int64_t end, const String& contentType);
    void blobSize(const URL&, CompletionHandler<void(uint64_t)>&&);
    void unregisterBlobURL(const URL&, const std::optional<WebCore::SecurityOriginData>& topOrigin);
    void writeBlobsToTemporaryFilesForIndexedDB(const Vector<String>& blobURLs, CompletionHandler<void(Vector<String>&&)>&&);

    void registerBlobURLHandle(const URL&, const std::optional<WebCore::SecurityOriginData>& topOrigin);
    void unregisterBlobURLHandle(const URL&, const std::optional<WebCore::SecurityOriginData>& topOrigin);

    void setCaptureExtraNetworkLoadMetricsEnabled(bool);

    void createSocketChannel(const WebCore::ResourceRequest&, const String& protocol, WebCore::WebSocketIdentifier, WebPageProxyIdentifier, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, const WebCore::ClientOrigin&, bool hadMainFrameMainResourcePrivateRelayed, bool allowPrivacyProxy, OptionSet<WebCore::AdvancedPrivacyProtections>, WebCore::ShouldRelaxThirdPartyCookieBlocking, WebCore::StoredCredentialsPolicy);

    void establishSharedWorkerServerConnection();
    void unregisterSharedWorkerConnection();

    void establishSWServerConnection();
    void establishSWContextConnection(WebPageProxyIdentifier, WebCore::RegistrableDomain&&, std::optional<WebCore::ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier, CompletionHandler<void()>&&);
    void closeSWContextConnection();
    void unregisterSWConnection();

    void establishSharedWorkerContextConnection(WebPageProxyIdentifier, WebCore::RegistrableDomain&&, CompletionHandler<void()>&&);
    void closeSharedWorkerContextConnection();

    void createRTCProvider(CompletionHandler<void()>&&);
#if ENABLE(WEB_RTC)
    void connectToRTCDataChannelRemoteSource(WebCore::RTCDataChannelIdentifier source, WebCore::RTCDataChannelIdentifier handler, CompletionHandler<void(std::optional<bool>)>&&);
#endif

    void createNewMessagePortChannel(const WebCore::MessagePortIdentifier& port1, const WebCore::MessagePortIdentifier& port2);
    void entangleLocalPortInThisProcessToRemote(const WebCore::MessagePortIdentifier& local, const WebCore::MessagePortIdentifier& remote);
    void messagePortDisentangled(const WebCore::MessagePortIdentifier&);
    void messagePortClosed(const WebCore::MessagePortIdentifier&);
    void takeAllMessagesForPort(const WebCore::MessagePortIdentifier&, CompletionHandler<void(Vector<WebCore::MessageWithMessagePorts>&&, uint64_t)>&&);
    void postMessageToRemote(WebCore::MessageWithMessagePorts&&, const WebCore::MessagePortIdentifier&);
    void didDeliverMessagePortMessages(uint64_t messageBatchIdentifier);

    void setCORSDisablingPatterns(WebCore::PageIdentifier, Vector<String>&&);

#if PLATFORM(MAC)
    void updateActivePages(const String& name, const Vector<String>& activePagesOrigins, CoreIPCAuditToken&&);
    void getProcessDisplayName(CoreIPCAuditToken&&, CompletionHandler<void(const String&)>&&);
#endif

#if USE(LIBWEBRTC)
    NetworkRTCProvider& rtcProvider();
#endif
#if ENABLE(WEB_RTC)
    void registerToRTCDataChannelProxy();
    void unregisterToRTCDataChannelProxy();
#endif
        
    bool allowTestOnlyIPC() const { return m_allowTestOnlyIPC; }

    void clearPageSpecificData(WebCore::PageIdentifier);

    void removeStorageAccessForFrame(WebCore::FrameIdentifier, WebCore::PageIdentifier);

    void logUserInteraction(RegistrableDomain&&);
    void resourceLoadStatisticsUpdated(Vector<WebCore::ResourceLoadStatistics>&&, CompletionHandler<void()>&&);
    void hasStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, WebCore::FrameIdentifier, WebCore::PageIdentifier, CompletionHandler<void(bool)>&&);
    void requestStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, WebCore::FrameIdentifier, WebCore::PageIdentifier, WebPageProxyIdentifier, WebCore::StorageAccessScope, CompletionHandler<void(WebCore::RequestStorageAccessResult)>&&);
    void storageAccessQuirkForTopFrameDomain(WebCore::RegistrableDomain&&, CompletionHandler<void(Vector<RegistrableDomain>)>&&);
    void requestStorageAccessUnderOpener(WebCore::RegistrableDomain&& domainInNeedOfStorageAccess, WebCore::PageIdentifier openerPageID, WebCore::RegistrableDomain&& openerDomain);

    void addOriginAccessAllowListEntry(const String& sourceOrigin, const String& destinationProtocol, const String& destinationHost, bool allowDestinationSubdomains);
    void removeOriginAccessAllowListEntry(const String& sourceOrigin, const String& destinationProtocol, const String& destinationHost, bool allowDestinationSubdomains);
    void resetOriginAccessAllowLists();

    uint64_t nextMessageBatchIdentifier(CompletionHandler<void()>&&);

    void domCookiesForHost(const URL& host, CompletionHandler<void(const Vector<WebCore::Cookie>&)>&&);

#if HAVE(COOKIE_CHANGE_LISTENER_API)
    void subscribeToCookieChangeNotifications(const String& host);
    void unsubscribeFromCookieChangeNotifications(const String& host);

    // WebCore::CookieChangeObserver.
    void cookiesAdded(const String& host, const Vector<WebCore::Cookie>&) final;
    void cookiesDeleted(const String& host, const Vector<WebCore::Cookie>&) final;
    void allCookiesDeleted() final;
#endif
    // WebCore::CookiesEnabledStateObserver
    void cookieEnabledStateMayHaveChanged() final;

#if ENABLE(DECLARATIVE_WEB_PUSH)
    void navigatorSubscribeToPushService(URL&& scopeURL, Vector<uint8_t>&& applicationServerKey, CompletionHandler<void(Expected<WebCore::PushSubscriptionData, WebCore::ExceptionData>&&)>&&);
    void navigatorUnsubscribeFromPushService(URL&& scopeURL, const WebCore::PushSubscriptionIdentifier&, CompletionHandler<void(Expected<bool, WebCore::ExceptionData>&&)>&&);
    void navigatorGetPushSubscription(URL&& scopeURL, CompletionHandler<void(Expected<std::optional<WebCore::PushSubscriptionData>, WebCore::ExceptionData>&&)>&&);
    void navigatorGetPushPermissionState(URL&& scopeURL, CompletionHandler<void(Expected<uint8_t, WebCore::ExceptionData>&&)>&&);
#endif

    void initializeWebTransportSession(URL&&, CompletionHandler<void(std::optional<WebTransportSessionIdentifier>)>&&);
    void destroyWebTransportSession(WebTransportSessionIdentifier);

    struct ResourceNetworkActivityTracker {
        ResourceNetworkActivityTracker() = default;
        ResourceNetworkActivityTracker(const ResourceNetworkActivityTracker&) = default;
        ResourceNetworkActivityTracker(ResourceNetworkActivityTracker&&) = default;
        ResourceNetworkActivityTracker(WebCore::PageIdentifier pageID)
            : pageID { pageID }
            , isRootActivity { true }
            , networkActivity { NetworkActivityTracker::Label::LoadPage }
        {
        }

        ResourceNetworkActivityTracker(WebCore::PageIdentifier pageID, WebCore::ResourceLoaderIdentifier resourceID)
            : pageID { pageID }
            , resourceID { resourceID }
            , networkActivity { NetworkActivityTracker::Label::LoadResource }
        {
        }

        WebCore::PageIdentifier pageID;
        WebCore::ResourceLoaderIdentifier resourceID;
        bool isRootActivity { false };
        NetworkActivityTracker networkActivity;
    };

    void stopAllNetworkActivityTracking();
    void stopAllNetworkActivityTrackingForPage(WebCore::PageIdentifier);
    size_t findRootNetworkActivity(WebCore::PageIdentifier);
    size_t findNetworkActivityTracker(WebCore::ResourceLoaderIdentifier resourceID);

    void hasUploadStateChanged(bool);

    void setResourceLoadSchedulingMode(WebCore::PageIdentifier, WebCore::LoadSchedulingMode);
    void prioritizeResourceLoads(const Vector<WebCore::ResourceLoaderIdentifier>&);

#if ENABLE(APPLE_PAY_REMOTE_UI)
    WebPaymentCoordinatorProxy& paymentCoordinator();

    // WebPaymentCoordinatorProxy::Client
    IPC::Connection* paymentCoordinatorConnection(const WebPaymentCoordinatorProxy&) final;
    UIViewController *paymentCoordinatorPresentingViewController(const WebPaymentCoordinatorProxy&) final;
#if ENABLE(APPLE_PAY_REMOTE_UI_USES_SCENE)
    void getWindowSceneAndBundleIdentifierForPaymentPresentation(WebPageProxyIdentifier, CompletionHandler<void(const String&, const String&)>&&) final;
#endif
    const String& paymentCoordinatorBoundInterfaceIdentifier(const WebPaymentCoordinatorProxy&) final;
    const String& paymentCoordinatorCTDataConnectionServiceType(const WebPaymentCoordinatorProxy&) final;
    const String& paymentCoordinatorSourceApplicationBundleIdentifier(const WebPaymentCoordinatorProxy&) final;
    const String& paymentCoordinatorSourceApplicationSecondaryIdentifier(const WebPaymentCoordinatorProxy&) final;
    std::unique_ptr<PaymentAuthorizationPresenter> paymentCoordinatorAuthorizationPresenter(WebPaymentCoordinatorProxy&, PKPaymentRequest *) final;
    void paymentCoordinatorAddMessageReceiver(WebPaymentCoordinatorProxy&, IPC::ReceiverName, IPC::MessageReceiver&) final;
    void paymentCoordinatorRemoveMessageReceiver(WebPaymentCoordinatorProxy&, IPC::ReceiverName) final;
#endif
    Ref<IPC::Connection> protectedConnection() { return m_connection; }

    Ref<IPC::Connection> m_connection;
    Ref<NetworkProcess> m_networkProcess;
    PAL::SessionID m_sessionID;

    HashMap<WebCore::WebSocketIdentifier, std::unique_ptr<NetworkSocketChannel>> m_networkSocketChannels;
    NetworkResourceLoadMap m_networkResourceLoaders;
    HashMap<String, RefPtr<WebCore::BlobDataFileReference>> m_blobDataFileReferences;
    Vector<ResourceNetworkActivityTracker> m_networkActivityTrackers;

    HashMap<WebCore::ResourceLoaderIdentifier, std::unique_ptr<WebCore::NetworkLoadInformation>> m_networkLoadInformationByID;


#if USE(LIBWEBRTC)
    RefPtr<NetworkRTCProvider> m_rtcProvider;
#endif
#if ENABLE(WEB_RTC)
    NetworkMDNSRegister m_mdnsRegister;
#endif
#if HAVE(COOKIE_CHANGE_LISTENER_API)
    HashSet<String> m_hostsWithCookieListeners;
#endif

    bool m_captureExtraNetworkLoadMetricsEnabled { false };

    WeakPtr<WebSWServerConnection> m_swConnection;
    std::unique_ptr<WebSWServerToContextConnection> m_swContextConnection;
    WeakPtr<WebSharedWorkerServerConnection> m_sharedWorkerConnection;
    std::unique_ptr<WebSharedWorkerServerToContextConnection> m_sharedWorkerContextConnection;

#if ENABLE(WEB_RTC)
    bool m_isRegisteredToRTCDataChannelProxy { false };
#endif

#if ENABLE(APPLE_PAY_REMOTE_UI)
    std::unique_ptr<WebPaymentCoordinatorProxy> m_paymentCoordinator;
#endif
    const WebCore::ProcessIdentifier m_webProcessIdentifier;

    HashSet<WebCore::MessagePortIdentifier> m_processEntangledPorts;
    HashMap<uint64_t, CompletionHandler<void()>> m_messageBatchDeliveryCompletionHandlers;
    Ref<NetworkSchemeRegistry> m_schemeRegistry;
    UniqueRef<NetworkOriginAccessPatterns> m_originAccessPatterns;
        
    using BlobURLKey = std::pair<URL, std::optional<WebCore::SecurityOriginData>>;
    HashSet<BlobURLKey> m_blobURLs;
    HashCountedSet<BlobURLKey> m_blobURLHandles;
#if ENABLE(IPC_TESTING_API)
    IPCTester m_ipcTester;
#endif
    bool m_allowTestOnlyIPC { false };

    HashMap<WebTransportSessionIdentifier, UniqueRef<NetworkTransportSession>> m_networkTransportSessions;
};

} // namespace WebKit

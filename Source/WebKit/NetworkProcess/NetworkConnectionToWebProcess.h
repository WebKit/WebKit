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

#include "CacheStorageEngineConnection.h"
#include "Connection.h"
#include "DownloadID.h"
#include "NetworkActivityTracker.h"
#include "NetworkConnectionToWebProcessMessagesReplies.h"
#include "NetworkMDNSRegister.h"
#include "NetworkRTCProvider.h"
#include "NetworkResourceLoadIdentifier.h"
#include "NetworkResourceLoadMap.h"
#include "PolicyDecision.h"
#include "SandboxExtension.h"
#include "WebPageProxyIdentifier.h"
#include "WebPaymentCoordinatorProxy.h"
#include "WebResourceLoadObserver.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/LayoutMilestone.h>
#include <WebCore/LoadSchedulingMode.h>
#include <WebCore/MessagePortChannelProvider.h>
#include <WebCore/MessagePortIdentifier.h>
#include <WebCore/NetworkLoadInformation.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/RTCDataChannelIdentifier.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/WebSocketIdentifier.h>
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
enum class ApplyTrackingPrevention : bool;
enum class StorageAccessScope : bool;
struct PolicyContainer;
struct RequestStorageAccessResult;
struct SameSiteInfo;

enum class HTTPCookieAcceptPolicy : uint8_t;
enum class IncludeSecureCookies : bool;
}

namespace WebKit {

class NetworkSchemeRegistry;
class NetworkProcess;
class NetworkResourceLoader;
class NetworkResourceLoadParameters;
class NetworkSession;
class NetworkSocketChannel;
class NetworkSocketStream;
class ServiceWorkerFetchTask;
class WebSWServerConnection;
class WebSWServerToContextConnection;
class WebSharedWorkerServerConnection;
class WebSharedWorkerServerToContextConnection;

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
    , IPC::Connection::Client {
public:
    using RegistrableDomain = WebCore::RegistrableDomain;

    static Ref<NetworkConnectionToWebProcess> create(NetworkProcess&, WebCore::ProcessIdentifier, PAL::SessionID, IPC::Connection::Identifier);
    virtual ~NetworkConnectionToWebProcess();
    
    PAL::SessionID sessionID() const { return m_sessionID; }
    NetworkSession* networkSession();

    IPC::Connection& connection() { return m_connection.get(); }
    NetworkProcess& networkProcess() { return m_networkProcess.get(); }

    void didCleanupResourceLoader(NetworkResourceLoader&);
    void transferKeptAliveLoad(NetworkResourceLoader&);
    void setOnLineState(bool);

    void deleteWebsiteDataForOrigins(OptionSet<WebsiteDataType>, const Vector<WebCore::SecurityOriginData>&, CompletionHandler<void()>&&);

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

    void checkProcessLocalPortForActivity(const WebCore::MessagePortIdentifier&, CompletionHandler<void(WebCore::MessagePortChannelProvider::HasActivity)>&&);

#if ENABLE(SERVICE_WORKER)
    void serviceWorkerServerToContextConnectionNoLongerNeeded();
    WebSWServerConnection* swConnection();
    std::unique_ptr<ServiceWorkerFetchTask> createFetchTask(NetworkResourceLoader&, const WebCore::ResourceRequest&);
#endif
    void sharedWorkerServerToContextConnectionIsNoLongerNeeded();

    WebSharedWorkerServerConnection* sharedWorkerConnection();

    NetworkSchemeRegistry& schemeRegistry() { return m_schemeRegistry.get(); }

    void cookieAcceptPolicyChanged(WebCore::HTTPCookieAcceptPolicy);

    void broadcastConsoleMessage(JSC::MessageSource, JSC::MessageLevel, const String& message);
    RefPtr<NetworkResourceLoader> takeNetworkResourceLoader(WebCore::ResourceLoaderIdentifier);

#if ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    void installMockContentFilter(WebCore::MockContentFilterSettings&&);
#endif

private:
    NetworkConnectionToWebProcess(NetworkProcess&, WebCore::ProcessIdentifier, PAL::SessionID, IPC::Connection::Identifier);

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
    void performSynchronousLoad(NetworkResourceLoadParameters&&, Messages::NetworkConnectionToWebProcess::PerformSynchronousLoadDelayedReply&&);
    void testProcessIncomingSyncMessagesWhenWaitingForSyncReply(WebPageProxyIdentifier, Messages::NetworkConnectionToWebProcess::TestProcessIncomingSyncMessagesWhenWaitingForSyncReplyDelayedReply&&);
    void loadPing(NetworkResourceLoadParameters&&);
    void prefetchDNS(const String&);
    void sendH2Ping(NetworkResourceLoadParameters&&, CompletionHandler<void(Expected<WTF::Seconds, WebCore::ResourceError>&&)>&&);
    void preconnectTo(std::optional<WebCore::ResourceLoaderIdentifier> preconnectionIdentifier, NetworkResourceLoadParameters&&);
    void isResourceLoadFinished(WebCore::ResourceLoaderIdentifier, CompletionHandler<void(bool)>&&);

    void removeLoadIdentifier(WebCore::ResourceLoaderIdentifier);
    void pageLoadCompleted(WebCore::PageIdentifier);
    void browsingContextRemoved(WebPageProxyIdentifier, WebCore::PageIdentifier, WebCore::FrameIdentifier);
    void crossOriginRedirectReceived(WebCore::ResourceLoaderIdentifier, const URL& redirectURL);
    void startDownload(DownloadID, const WebCore::ResourceRequest&, std::optional<NavigatingToAppBoundDomain>, const String& suggestedName = { });
    void convertMainResourceLoadToDownload(std::optional<WebCore::ResourceLoaderIdentifier> mainResourceLoadIdentifier, DownloadID, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, std::optional<NavigatingToAppBoundDomain>);

    void registerURLSchemesAsCORSEnabled(Vector<String>&& schemes);

    void cookiesForDOM(const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, WebCore::FrameIdentifier, WebCore::PageIdentifier, WebCore::IncludeSecureCookies, WebCore::ApplyTrackingPrevention, WebCore::ShouldRelaxThirdPartyCookieBlocking, CompletionHandler<void(String cookieString, bool secureCookiesAccessed)>&&);
    void setCookiesFromDOM(const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, WebCore::FrameIdentifier, WebCore::PageIdentifier, WebCore::ApplyTrackingPrevention, const String&, WebCore::ShouldRelaxThirdPartyCookieBlocking);
    void cookieRequestHeaderFieldValue(const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, WebCore::IncludeSecureCookies, WebCore::ApplyTrackingPrevention, WebCore::ShouldRelaxThirdPartyCookieBlocking, CompletionHandler<void(String cookieString, bool secureCookiesAccessed)>&&);
    void getRawCookies(const URL& firstParty, const WebCore::SameSiteInfo&, const URL&, std::optional<WebCore::FrameIdentifier>, std::optional<WebCore::PageIdentifier>, WebCore::ApplyTrackingPrevention, WebCore::ShouldRelaxThirdPartyCookieBlocking, CompletionHandler<void(Vector<WebCore::Cookie>&&)>&&);
    void setRawCookie(const WebCore::Cookie&);
    void deleteCookie(const URL&, const String& cookieName, CompletionHandler<void()>&&);

    void registerFileBlobURL(const URL&, const String& path, const String& replacementPath, SandboxExtension::Handle&&, const String& contentType);
    void registerBlobURL(const URL&, Vector<WebCore::BlobPart>&&, const String& contentType);
    void registerBlobURLFromURL(const URL&, const URL& srcURL, WebCore::PolicyContainer&&);
    void registerBlobURLOptionallyFileBacked(const URL&, const URL& srcURL, const String& fileBackedPath, const String& contentType);
    void registerBlobURLForSlice(const URL&, const URL& srcURL, int64_t start, int64_t end, const String& contentType);
    void blobSize(const URL&, CompletionHandler<void(uint64_t)>&&);
    void unregisterBlobURL(const URL&);
    void writeBlobsToTemporaryFilesForIndexedDB(const Vector<String>& blobURLs, CompletionHandler<void(Vector<String>&&)>&&);

    void registerBlobURLHandle(const URL&);
    void unregisterBlobURLHandle(const URL&);

    void setCaptureExtraNetworkLoadMetricsEnabled(bool);

    void createSocketStream(URL&&, String cachePartition, WebCore::WebSocketIdentifier);

    void createSocketChannel(const WebCore::ResourceRequest&, const String& protocol, WebCore::WebSocketIdentifier, WebPageProxyIdentifier, const WebCore::ClientOrigin&, bool hadMainFrameMainResourcePrivateRelayed, bool allowPrivacyProxy, bool networkConnectionIntegrityEnabled);
    void updateQuotaBasedOnSpaceUsageForTesting(WebCore::ClientOrigin&&);

    void establishSharedWorkerServerConnection();
    void unregisterSharedWorkerConnection();

#if ENABLE(SERVICE_WORKER)
    void establishSWServerConnection();
    void establishSWContextConnection(WebPageProxyIdentifier, WebCore::RegistrableDomain&&, std::optional<WebCore::ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier, CompletionHandler<void()>&&);
    void closeSWContextConnection();
    void unregisterSWConnection();
#endif

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
    void checkRemotePortForActivity(const WebCore::MessagePortIdentifier, CompletionHandler<void(bool)>&&);
    void didDeliverMessagePortMessages(uint64_t messageBatchIdentifier);

    void setCORSDisablingPatterns(WebCore::PageIdentifier, Vector<String>&&);

#if PLATFORM(MAC)
    void updateActivePages(const String& name, const Vector<String>& activePagesOrigins, audit_token_t);
    void getProcessDisplayName(audit_token_t, CompletionHandler<void(const String&)>&&);
#endif

#if USE(LIBWEBRTC)
    NetworkRTCProvider& rtcProvider();
#endif
#if ENABLE(WEB_RTC)
    NetworkMDNSRegister& mdnsRegister() { return m_mdnsRegister; }
    void registerToRTCDataChannelProxy();
    void unregisterToRTCDataChannelProxy();
#endif

    CacheStorageEngineConnection& cacheStorageConnection();

    void clearPageSpecificData(WebCore::PageIdentifier);

#if ENABLE(TRACKING_PREVENTION)
    void removeStorageAccessForFrame(WebCore::FrameIdentifier, WebCore::PageIdentifier);

    void logUserInteraction(RegistrableDomain&&);
    void resourceLoadStatisticsUpdated(Vector<WebCore::ResourceLoadStatistics>&&, CompletionHandler<void()>&&);
    void hasStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, WebCore::FrameIdentifier, WebCore::PageIdentifier, CompletionHandler<void(bool)>&&);
    void requestStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, WebCore::FrameIdentifier, WebCore::PageIdentifier, WebPageProxyIdentifier, WebCore::StorageAccessScope, CompletionHandler<void(WebCore::RequestStorageAccessResult)>&&);
    void requestStorageAccessUnderOpener(WebCore::RegistrableDomain&& domainInNeedOfStorageAccess, WebCore::PageIdentifier openerPageID, WebCore::RegistrableDomain&& openerDomain);
#endif

    void addOriginAccessAllowListEntry(const String& sourceOrigin, const String& destinationProtocol, const String& destinationHost, bool allowDestinationSubdomains);
    void removeOriginAccessAllowListEntry(const String& sourceOrigin, const String& destinationProtocol, const String& destinationHost, bool allowDestinationSubdomains);
    void resetOriginAccessAllowLists();

    uint64_t nextMessageBatchIdentifier(CompletionHandler<void()>&&);

    void domCookiesForHost(const String& host, bool subscribeToCookieChangeNotifications, CompletionHandler<void(const Vector<WebCore::Cookie>&)>&&);

#if HAVE(COOKIE_CHANGE_LISTENER_API)
    void unsubscribeFromCookieChangeNotifications(const HashSet<String>& hosts);

    // WebCore::CookieChangeObserver.
    void cookiesAdded(const String& host, const Vector<WebCore::Cookie>&) final;
    void cookiesDeleted(const String& host, const Vector<WebCore::Cookie>&) final;
    void allCookiesDeleted() final;
#endif

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
    void getWindowSceneIdentifierForPaymentPresentation(WebPageProxyIdentifier, CompletionHandler<void(const String&)>&&) final;
#endif
    const String& paymentCoordinatorBoundInterfaceIdentifier(const WebPaymentCoordinatorProxy&) final;
    const String& paymentCoordinatorCTDataConnectionServiceType(const WebPaymentCoordinatorProxy&) final;
    const String& paymentCoordinatorSourceApplicationBundleIdentifier(const WebPaymentCoordinatorProxy&) final;
    const String& paymentCoordinatorSourceApplicationSecondaryIdentifier(const WebPaymentCoordinatorProxy&) final;
    std::unique_ptr<PaymentAuthorizationPresenter> paymentCoordinatorAuthorizationPresenter(WebPaymentCoordinatorProxy&, PKPaymentRequest *) final;
    void paymentCoordinatorAddMessageReceiver(WebPaymentCoordinatorProxy&, IPC::ReceiverName, IPC::MessageReceiver&) final;
    void paymentCoordinatorRemoveMessageReceiver(WebPaymentCoordinatorProxy&, IPC::ReceiverName) final;
#endif

    Ref<IPC::Connection> m_connection;
    Ref<NetworkProcess> m_networkProcess;
    PAL::SessionID m_sessionID;

    HashMap<WebCore::WebSocketIdentifier, RefPtr<NetworkSocketStream>> m_networkSocketStreams;
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

    RefPtr<CacheStorageEngineConnection> m_cacheStorageConnection;

#if ENABLE(SERVICE_WORKER)
    WeakPtr<WebSWServerConnection> m_swConnection;
    std::unique_ptr<WebSWServerToContextConnection> m_swContextConnection;
#endif
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
        
    HashSet<URL> m_blobURLs;
    HashCountedSet<URL> m_blobURLHandles;
#if ENABLE(IPC_TESTING_API)
    IPCTester m_ipcTester;
#endif
};

} // namespace WebKit

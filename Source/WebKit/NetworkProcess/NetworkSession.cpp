/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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

#include "config.h"
#include "NetworkSession.h"

#include "CacheStorageEngine.h"
#include "Logging.h"
#include "NetworkBroadcastChannelRegistry.h"
#include "NetworkLoadScheduler.h"
#include "NetworkProcess.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkResourceLoadParameters.h"
#include "NetworkResourceLoader.h"
#include "NetworkSessionCreationParameters.h"
#include "NetworkStorageManager.h"
#include "NotificationManagerMessageHandlerMessages.h"
#include "PingLoad.h"
#include "PrivateClickMeasurementClientImpl.h"
#include "PrivateClickMeasurementManager.h"
#include "PrivateClickMeasurementManagerProxy.h"
#include "RemoteWorkerType.h"
#include "ServiceWorkerFetchTask.h"
#include "WebPageProxy.h"
#include "WebPageProxyMessages.h"
#include "WebProcessProxy.h"
#include "WebSWOriginStore.h"
#include "WebSharedWorkerServer.h"
#include "WebSocketTask.h"
#include <WebCore/CookieJar.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <WebCore/SWServer.h>

#if PLATFORM(COCOA)
#include "DefaultWebBrowserChecks.h"
#include "NetworkSessionCocoa.h"
#endif
#if USE(SOUP)
#include "NetworkSessionSoup.h"
#endif
#if USE(CURL)
#include "NetworkSessionCurl.h"
#endif

namespace WebKit {
using namespace WebCore;

constexpr Seconds cachedNetworkResourceLoaderLifetime { 30_s };

std::unique_ptr<NetworkSession> NetworkSession::create(NetworkProcess& networkProcess, const NetworkSessionCreationParameters& parameters)
{
#if PLATFORM(COCOA)
    return NetworkSessionCocoa::create(networkProcess, parameters);
#endif
#if USE(SOUP)
    return NetworkSessionSoup::create(networkProcess, parameters);
#endif
#if USE(CURL)
    return NetworkSessionCurl::create(networkProcess, parameters);
#endif
}

NetworkStorageSession* NetworkSession::networkStorageSession() const
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=194926 NetworkSession should own NetworkStorageSession
    // instead of having separate maps with the same key and different management.
    auto* storageSession = m_networkProcess->storageSession(m_sessionID);
    ASSERT(storageSession);
    return storageSession;
}

static UniqueRef<PCM::ManagerInterface> managerOrProxy(NetworkSession& networkSession, NetworkProcess& networkProcess, const NetworkSessionCreationParameters& parameters)
{
    if (!parameters.pcmMachServiceName.isEmpty())
        return makeUniqueRef<PCM::ManagerProxy>(parameters.pcmMachServiceName, networkSession);
    return makeUniqueRef<PrivateClickMeasurementManager>(makeUniqueRef<PCM::ClientImpl>(networkSession, networkProcess), parameters.resourceLoadStatisticsParameters.directory);
}

static Ref<NetworkStorageManager> createNetworkStorageManager(IPC::Connection* connection, const NetworkSessionCreationParameters& parameters)
{
    SandboxExtension::consumePermanently(parameters.localStorageDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.indexedDBDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.cacheStorageDirectoryExtensionHandle);
    SandboxExtension::consumePermanently(parameters.generalStorageDirectoryHandle);
    IPC::Connection::UniqueID connectionID;
    if (connection)
        connectionID = connection->uniqueID();
    return NetworkStorageManager::create(parameters.sessionID, connectionID, parameters.generalStorageDirectory, parameters.localStorageDirectory, parameters.indexedDBDirectory, parameters.cacheStorageDirectory, parameters.perOriginStorageQuota, parameters.perThirdPartyOriginStorageQuota, parameters.shouldUseCustomStoragePaths);
}

NetworkSession::NetworkSession(NetworkProcess& networkProcess, const NetworkSessionCreationParameters& parameters)
    : m_sessionID(parameters.sessionID)
    , m_dataStoreIdentifier(parameters.dataStoreIdentifier)
    , m_networkProcess(networkProcess)
#if ENABLE(TRACKING_PREVENTION)
    , m_resourceLoadStatisticsDirectory(parameters.resourceLoadStatisticsParameters.directory)
    , m_shouldIncludeLocalhostInResourceLoadStatistics(parameters.resourceLoadStatisticsParameters.shouldIncludeLocalhost ? ShouldIncludeLocalhost::Yes : ShouldIncludeLocalhost::No)
    , m_enableResourceLoadStatisticsDebugMode(parameters.resourceLoadStatisticsParameters.enableDebugMode ? EnableResourceLoadStatisticsDebugMode::Yes : EnableResourceLoadStatisticsDebugMode::No)
    , m_resourceLoadStatisticsManualPrevalentResource(parameters.resourceLoadStatisticsParameters.manualPrevalentResource)
    , m_enableResourceLoadStatisticsLogTestingEvent(parameters.resourceLoadStatisticsParameters.enableLogTestingEvent)
    , m_thirdPartyCookieBlockingMode(parameters.resourceLoadStatisticsParameters.thirdPartyCookieBlockingMode)
    , m_sameSiteStrictEnforcementEnabled(parameters.resourceLoadStatisticsParameters.sameSiteStrictEnforcementEnabled)
    , m_firstPartyWebsiteDataRemovalMode(parameters.resourceLoadStatisticsParameters.firstPartyWebsiteDataRemovalMode)
    , m_standaloneApplicationDomain(parameters.resourceLoadStatisticsParameters.standaloneApplicationDomain)
#endif
    , m_privateClickMeasurement(managerOrProxy(*this, networkProcess, parameters))
    , m_privateClickMeasurementDebugModeEnabled(parameters.enablePrivateClickMeasurementDebugMode)
    , m_broadcastChannelRegistry(makeUniqueRef<NetworkBroadcastChannelRegistry>(networkProcess))
    , m_testSpeedMultiplier(parameters.testSpeedMultiplier)
    , m_allowsServerPreconnect(parameters.allowsServerPreconnect)
    , m_shouldRunServiceWorkersOnMainThreadForTesting(parameters.shouldRunServiceWorkersOnMainThreadForTesting)
    , m_overrideServiceWorkerRegistrationCountTestingValue(parameters.overrideServiceWorkerRegistrationCountTestingValue)
    , m_storageManager(createNetworkStorageManager(networkProcess.parentProcessConnection(), parameters))
#if ENABLE(BUILT_IN_NOTIFICATIONS)
    , m_notificationManager(*this, parameters.webPushMachServiceName, WebPushD::WebPushDaemonConnectionConfiguration { parameters.webPushDaemonConnectionConfiguration })
#endif
#if !HAVE(NSURLSESSION_WEBSOCKET)
    , m_shouldAcceptInsecureCertificatesForWebSockets(parameters.shouldAcceptInsecureCertificatesForWebSockets)
#endif
{
    if (!m_sessionID.isEphemeral()) {
        String networkCacheDirectory = parameters.networkCacheDirectory;
        if (!networkCacheDirectory.isNull()) {
            SandboxExtension::consumePermanently(parameters.networkCacheDirectoryExtensionHandle);

            auto cacheOptions = networkProcess.cacheOptions();
#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
            if (parameters.networkCacheSpeculativeValidationEnabled)
                cacheOptions.add(NetworkCache::CacheOption::SpeculativeRevalidation);
#endif
            if (parameters.shouldUseTestingNetworkSession)
                cacheOptions.add(NetworkCache::CacheOption::TestingMode);

            m_cache = NetworkCache::Cache::open(networkProcess, networkCacheDirectory, cacheOptions, m_sessionID);

            if (!m_cache)
                RELEASE_LOG_ERROR(NetworkCache, "Failed to initialize the WebKit network disk cache");
        }

        if (!parameters.resourceLoadStatisticsParameters.directory.isEmpty())
            SandboxExtension::consumePermanently(parameters.resourceLoadStatisticsParameters.directoryExtensionHandle);
        if (!parameters.cacheStorageDirectory.isEmpty()) {
            m_cacheStorageDirectory = parameters.cacheStorageDirectory;
            SandboxExtension::consumePermanently(parameters.cacheStorageDirectoryExtensionHandle);
        }
    }

    m_isStaleWhileRevalidateEnabled = parameters.staleWhileRevalidateEnabled;

#if ENABLE(TRACKING_PREVENTION)
    setTrackingPreventionEnabled(parameters.resourceLoadStatisticsParameters.enabled);
#endif

#if ENABLE(SERVICE_WORKER)
    SandboxExtension::consumePermanently(parameters.serviceWorkerRegistrationDirectoryExtensionHandle);
    m_serviceWorkerInfo = ServiceWorkerInfo {
        parameters.serviceWorkerRegistrationDirectory,
        parameters.serviceWorkerProcessTerminationDelayEnabled
    };
#endif

#if ENABLE(BUILT_IN_NOTIFICATIONS)
    m_networkProcess->addMessageReceiver(Messages::NotificationManagerMessageHandler::messageReceiverName(), m_sessionID.toUInt64(), m_notificationManager);
#endif
}

NetworkSession::~NetworkSession()
{
#if ENABLE(TRACKING_PREVENTION)
    destroyResourceLoadStatistics([] { });
#endif
    for (auto& loader : std::exchange(m_keptAliveLoads, { }))
        loader->abort();

#if ENABLE(BUILT_IN_NOTIFICATIONS)
    m_networkProcess->removeMessageReceiver(Messages::NotificationManagerMessageHandler::messageReceiverName(), m_sessionID.toUInt64());
#endif
}

#if ENABLE(TRACKING_PREVENTION)
void NetworkSession::destroyResourceLoadStatistics(CompletionHandler<void()>&& completionHandler)
{
    if (!m_resourceLoadStatistics)
        return completionHandler();

    m_resourceLoadStatistics->didDestroyNetworkSession(WTFMove(completionHandler));
    m_resourceLoadStatistics = nullptr;
}
#endif

void NetworkSession::invalidateAndCancel()
{
    m_dataTaskSet.forEach([] (auto& task) {
        task.invalidateAndCancel();
    });
#if ENABLE(TRACKING_PREVENTION)
    if (m_resourceLoadStatistics)
        m_resourceLoadStatistics->invalidateAndCancel();
#endif
    m_cacheEngine = nullptr;
#if ASSERT_ENABLED
    m_isInvalidated = true;
#endif

    if (m_cache) {
        auto networkCacheDirectory = m_cache->storageDirectory();
        m_cache = nullptr;
        FileSystem::markPurgeable(networkCacheDirectory);
    }
}

void NetworkSession::destroyPrivateClickMeasurementStore(CompletionHandler<void()>&& completionHandler)
{
    privateClickMeasurement().destroyStoreForTesting(WTFMove(completionHandler));
}

#if ENABLE(TRACKING_PREVENTION)
void NetworkSession::setTrackingPreventionEnabled(bool enable)
{
    ASSERT(!m_isInvalidated);
    if (auto* storageSession = networkStorageSession())
        storageSession->setTrackingPreventionEnabled(enable);
    if (!enable) {
        destroyResourceLoadStatistics([] { });
        return;
    }

    if (m_resourceLoadStatistics)
        return;

    m_resourceLoadStatistics = WebResourceLoadStatisticsStore::create(*this, m_resourceLoadStatisticsDirectory, m_shouldIncludeLocalhostInResourceLoadStatistics, (m_sessionID.isEphemeral() ? ResourceLoadStatistics::IsEphemeral::Yes : ResourceLoadStatistics::IsEphemeral::No));
    if (!m_sessionID.isEphemeral())
        m_resourceLoadStatistics->populateMemoryStoreFromDisk([] { });

    if (m_enableResourceLoadStatisticsDebugMode == EnableResourceLoadStatisticsDebugMode::Yes)
        m_resourceLoadStatistics->setResourceLoadStatisticsDebugMode(true, [] { });
    // This should always be forwarded since debug mode may be enabled at runtime.
    if (!m_resourceLoadStatisticsManualPrevalentResource.isEmpty())
        m_resourceLoadStatistics->setPrevalentResourceForDebugMode(RegistrableDomain { m_resourceLoadStatisticsManualPrevalentResource }, [] { });
    forwardResourceLoadStatisticsSettings();
}

void NetworkSession::forwardResourceLoadStatisticsSettings()
{
    m_resourceLoadStatistics->setThirdPartyCookieBlockingMode(m_thirdPartyCookieBlockingMode);
    m_resourceLoadStatistics->setSameSiteStrictEnforcementEnabled(m_sameSiteStrictEnforcementEnabled);
    m_resourceLoadStatistics->setFirstPartyWebsiteDataRemovalMode(m_firstPartyWebsiteDataRemovalMode, [] { });
    m_resourceLoadStatistics->setStandaloneApplicationDomain(m_standaloneApplicationDomain, [] { });
}

bool NetworkSession::isTrackingPreventionEnabled() const
{
    return !!m_resourceLoadStatistics;
}

void NetworkSession::notifyResourceLoadStatisticsProcessed()
{
    m_networkProcess->parentProcessConnection()->send(Messages::NetworkProcessProxy::NotifyResourceLoadStatisticsProcessed(), 0);
}

void NetworkSession::logDiagnosticMessageWithValue(const String& message, const String& description, unsigned value, unsigned significantFigures, WebCore::ShouldSample shouldSample)
{
    m_networkProcess->parentProcessConnection()->send(Messages::WebPageProxy::LogDiagnosticMessageWithValueFromWebProcess(message, description, value, significantFigures, shouldSample), 0);
}

void NetworkSession::deleteAndRestrictWebsiteDataForRegistrableDomains(OptionSet<WebsiteDataType> dataTypes, RegistrableDomainsToDeleteOrRestrictWebsiteDataFor&& domains, bool shouldNotifyPage, CompletionHandler<void(HashSet<RegistrableDomain>&&)>&& completionHandler)
{
    if (auto* storageSession = networkStorageSession()) {
        for (auto& domain : domains.domainsToEnforceSameSiteStrictFor)
            storageSession->setAllCookiesToSameSiteStrict(domain, [] { });
    }
    domains.domainsToEnforceSameSiteStrictFor.clear();

    m_networkProcess->deleteAndRestrictWebsiteDataForRegistrableDomains(m_sessionID, dataTypes, WTFMove(domains), shouldNotifyPage, WTFMove(completionHandler));
}

void NetworkSession::registrableDomainsWithWebsiteData(OptionSet<WebsiteDataType> dataTypes, bool shouldNotifyPage, CompletionHandler<void(HashSet<RegistrableDomain>&&)>&& completionHandler)
{
    m_networkProcess->registrableDomainsWithWebsiteData(m_sessionID, dataTypes, shouldNotifyPage, WTFMove(completionHandler));
}

void NetworkSession::setShouldDowngradeReferrerForTesting(bool enabled)
{
    m_downgradeReferrer = enabled;
}

bool NetworkSession::shouldDowngradeReferrer() const
{
    return m_downgradeReferrer;
}

void NetworkSession::setThirdPartyCookieBlockingMode(ThirdPartyCookieBlockingMode blockingMode)
{
    ASSERT(m_resourceLoadStatistics);
    m_thirdPartyCookieBlockingMode = blockingMode;
    if (m_resourceLoadStatistics)
        m_resourceLoadStatistics->setThirdPartyCookieBlockingMode(blockingMode);
}

void NetworkSession::setShouldEnbleSameSiteStrictEnforcement(WebCore::SameSiteStrictEnforcementEnabled enabled)
{
    ASSERT(m_resourceLoadStatistics);
    m_sameSiteStrictEnforcementEnabled = enabled;
    if (m_resourceLoadStatistics)
        m_resourceLoadStatistics->setSameSiteStrictEnforcementEnabled(enabled);
}

void NetworkSession::setFirstPartyHostCNAMEDomain(String&& firstPartyHost, WebCore::RegistrableDomain&& cnameDomain)
{
    ASSERT(!firstPartyHost.isEmpty() && !cnameDomain.isEmpty() && firstPartyHost != cnameDomain.string());
    if (firstPartyHost.isEmpty() || cnameDomain.isEmpty() || firstPartyHost == cnameDomain.string())
        return;
    m_firstPartyHostCNAMEDomains.add(WTFMove(firstPartyHost), WTFMove(cnameDomain));
}

std::optional<WebCore::RegistrableDomain> NetworkSession::firstPartyHostCNAMEDomain(const String& firstPartyHost)
{
    if (!decltype(m_firstPartyHostCNAMEDomains)::isValidKey(firstPartyHost))
        return std::nullopt;

    auto iterator = m_firstPartyHostCNAMEDomains.find(firstPartyHost);
    if (iterator == m_firstPartyHostCNAMEDomains.end())
        return std::nullopt;
    return iterator->value;
}

void NetworkSession::resetFirstPartyDNSData()
{
    m_firstPartyHostCNAMEDomains.clear();
    m_firstPartyHostIPAddresses.clear();
    m_thirdPartyCNAMEDomainForTesting = std::nullopt;
}

void NetworkSession::setFirstPartyHostIPAddress(const String& firstPartyHost, const String& addressString)
{
    if (firstPartyHost.isEmpty() || addressString.isEmpty())
        return;

    if (auto address = WebCore::IPAddress::fromString(addressString))
        m_firstPartyHostIPAddresses.set(firstPartyHost, WTFMove(*address));
}

std::optional<WebCore::IPAddress> NetworkSession::firstPartyHostIPAddress(const String& firstPartyHost)
{
    if (firstPartyHost.isEmpty())
        return std::nullopt;

    auto iterator = m_firstPartyHostIPAddresses.find(firstPartyHost);
    if (iterator == m_firstPartyHostIPAddresses.end())
        return std::nullopt;

    return { iterator->value };
}
#endif // ENABLE(TRACKING_PREVENTION)

void NetworkSession::storePrivateClickMeasurement(WebCore::PrivateClickMeasurement&& unattributedPrivateClickMeasurement)
{
    if (m_isRunningEphemeralMeasurementTest)
        unattributedPrivateClickMeasurement.setEphemeral(WebCore::PCM::AttributionEphemeral::Yes);
    if (unattributedPrivateClickMeasurement.isEphemeral() == WebCore::PCM::AttributionEphemeral::Yes) {
        m_ephemeralMeasurement = WTFMove(unattributedPrivateClickMeasurement);
        return;
    }

    if (unattributedPrivateClickMeasurement.isSKAdNetworkAttribution())
        return donateToSKAdNetwork(WTFMove(unattributedPrivateClickMeasurement));

    privateClickMeasurement().storeUnattributed(WTFMove(unattributedPrivateClickMeasurement), [] { });
}

void NetworkSession::handlePrivateClickMeasurementConversion(WebCore::PCM::AttributionTriggerData&& attributionTriggerData, const URL& requestURL, const WebCore::ResourceRequest& redirectRequest, String&& attributedBundleIdentifier)
{
    String appBundleID = WTFMove(attributedBundleIdentifier);
#if PLATFORM(COCOA)
    if (appBundleID.isEmpty())
        appBundleID = WebCore::applicationBundleIdentifier();
#endif

    if (m_ephemeralMeasurement) {
        auto ephemeralMeasurement = *std::exchange(m_ephemeralMeasurement, std::nullopt);

        auto redirectDomain = RegistrableDomain(redirectRequest.url());
        auto firstPartyForCookies = redirectRequest.firstPartyForCookies();

        // Ephemeral measurement can only have one pending click.
        if (ephemeralMeasurement.isNeitherSameSiteNorCrossSiteTriggeringEvent(redirectDomain, firstPartyForCookies, attributionTriggerData))
            return;
        if (ephemeralMeasurement.destinationSite().registrableDomain != RegistrableDomain(firstPartyForCookies))
            return;

        // Insert ephemeral measurement right before attribution.
        privateClickMeasurement().storeUnattributed(WTFMove(ephemeralMeasurement), [this, weakThis = WeakPtr { *this }, attributionTriggerData = WTFMove(attributionTriggerData), requestURL, redirectDomain = WTFMove(redirectDomain), firstPartyForCookies = WTFMove(firstPartyForCookies), appBundleID = WTFMove(appBundleID)] () mutable {
            if (!weakThis)
                return;
            privateClickMeasurement().handleAttribution(WTFMove(attributionTriggerData), requestURL, WTFMove(redirectDomain), firstPartyForCookies, appBundleID);
        });
        return;
    }

    privateClickMeasurement().handleAttribution(WTFMove(attributionTriggerData), requestURL, RegistrableDomain(redirectRequest.url()), redirectRequest.firstPartyForCookies(), appBundleID);
}

void NetworkSession::dumpPrivateClickMeasurement(CompletionHandler<void(String)>&& completionHandler)
{
    privateClickMeasurement().toStringForTesting(WTFMove(completionHandler));
}

void NetworkSession::clearPrivateClickMeasurement(CompletionHandler<void()>&& completionHandler)
{
    privateClickMeasurement().clear(WTFMove(completionHandler));
    m_ephemeralMeasurement = std::nullopt;
    m_isRunningEphemeralMeasurementTest = false;
}

void NetworkSession::clearPrivateClickMeasurementForRegistrableDomain(WebCore::RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    privateClickMeasurement().clearForRegistrableDomain(WTFMove(domain), WTFMove(completionHandler));
}

void NetworkSession::setPrivateClickMeasurementOverrideTimerForTesting(bool value)
{
    privateClickMeasurement().setOverrideTimerForTesting(value);
}

void NetworkSession::markAttributedPrivateClickMeasurementsAsExpiredForTesting(CompletionHandler<void()>&& completionHandler)
{
    privateClickMeasurement().markAttributedPrivateClickMeasurementsAsExpiredForTesting(WTFMove(completionHandler));
}

void NetworkSession::setPrivateClickMeasurementTokenPublicKeyURLForTesting(URL&& url)
{
    privateClickMeasurement().setTokenPublicKeyURLForTesting(WTFMove(url));
}

void NetworkSession::setPrivateClickMeasurementTokenSignatureURLForTesting(URL&& url)
{
    privateClickMeasurement().setTokenSignatureURLForTesting(WTFMove(url));
}

void NetworkSession::setPrivateClickMeasurementAttributionReportURLsForTesting(URL&& sourceURL, URL&& destinationURL)
{
    privateClickMeasurement().setAttributionReportURLsForTesting(WTFMove(sourceURL), WTFMove(destinationURL));
}

void NetworkSession::markPrivateClickMeasurementsAsExpiredForTesting()
{
    privateClickMeasurement().markAllUnattributedAsExpiredForTesting();
}

void NetworkSession::setPrivateClickMeasurementEphemeralMeasurementForTesting(bool value)
{
    m_isRunningEphemeralMeasurementTest = value;
}

// FIXME: Switch to non-mocked test data once the right cryptography library is available in open source.
void NetworkSession::setPCMFraudPreventionValuesForTesting(String&& unlinkableToken, String&& secretToken, String&& signature, String&& keyID)
{
    privateClickMeasurement().setPCMFraudPreventionValuesForTesting(WTFMove(unlinkableToken), WTFMove(secretToken), WTFMove(signature), WTFMove(keyID));
}

void NetworkSession::setPrivateClickMeasurementDebugMode(bool enabled)
{
    if (m_privateClickMeasurementDebugModeEnabled == enabled)
        return;

    m_privateClickMeasurementDebugModeEnabled = enabled;
    m_privateClickMeasurement->setDebugModeIsEnabled(enabled);
}

void NetworkSession::firePrivateClickMeasurementTimerImmediatelyForTesting()
{
    privateClickMeasurement().startTimerImmediatelyForTesting();
}

void NetworkSession::allowTLSCertificateChainForLocalPCMTesting(const WebCore::CertificateInfo& certificateInfo)
{
    privateClickMeasurement().allowTLSCertificateChainForLocalPCMTesting(certificateInfo);
}

void NetworkSession::setPrivateClickMeasurementAppBundleIDForTesting(String&& appBundleIDForTesting)
{
#if PLATFORM(COCOA)
    auto appBundleID = WebCore::applicationBundleIdentifier();
    if (!isRunningTest(appBundleID))
        WTFLogAlways("isRunningTest() returned false. appBundleID is %s.", appBundleID.isEmpty() ? "empty" : appBundleID.utf8().data());
    RELEASE_ASSERT(isRunningTest(WebCore::applicationBundleIdentifier()));
#endif
    privateClickMeasurement().setPrivateClickMeasurementAppBundleIDForTesting(WTFMove(appBundleIDForTesting));
}

void NetworkSession::addKeptAliveLoad(Ref<NetworkResourceLoader>&& loader)
{
    ASSERT(m_sessionID == loader->sessionID());
    ASSERT(!m_keptAliveLoads.contains(loader));
    m_keptAliveLoads.add(WTFMove(loader));
}

void NetworkSession::removeKeptAliveLoad(NetworkResourceLoader& loader)
{
    ASSERT(m_sessionID == loader.sessionID());
    ASSERT(m_keptAliveLoads.contains(loader));
    m_keptAliveLoads.remove(loader);
}

NetworkSession::CachedNetworkResourceLoader::CachedNetworkResourceLoader(Ref<NetworkResourceLoader>&& loader)
    : m_expirationTimer(*this, &CachedNetworkResourceLoader::expirationTimerFired)
    , m_loader(WTFMove(loader))
{
    m_expirationTimer.startOneShot(cachedNetworkResourceLoaderLifetime);
}

RefPtr<NetworkResourceLoader> NetworkSession::CachedNetworkResourceLoader::takeLoader()
{
    return std::exchange(m_loader, nullptr);
}

void NetworkSession::CachedNetworkResourceLoader::expirationTimerFired()
{
    auto session = m_loader->connectionToWebProcess().networkSession();
    ASSERT(session);
    if (!session)
        return;

    session->removeLoaderWaitingWebProcessTransfer(m_loader->identifier());
}

void NetworkSession::addLoaderAwaitingWebProcessTransfer(Ref<NetworkResourceLoader>&& loader)
{
    ASSERT(m_sessionID == loader->sessionID());
    auto identifier = loader->identifier();
    ASSERT(!m_loadersAwaitingWebProcessTransfer.contains(identifier));
    m_loadersAwaitingWebProcessTransfer.add(identifier, makeUnique<CachedNetworkResourceLoader>(WTFMove(loader)));
}

RefPtr<NetworkResourceLoader> NetworkSession::takeLoaderAwaitingWebProcessTransfer(NetworkResourceLoadIdentifier identifier)
{
    auto cachedResourceLoader = m_loadersAwaitingWebProcessTransfer.take(identifier);
    return cachedResourceLoader ? cachedResourceLoader->takeLoader() : nullptr;
}

void NetworkSession::removeLoaderWaitingWebProcessTransfer(NetworkResourceLoadIdentifier identifier)
{
    if (auto cachedResourceLoader = m_loadersAwaitingWebProcessTransfer.take(identifier))
        cachedResourceLoader->takeLoader()->abort();
}

std::unique_ptr<WebSocketTask> NetworkSession::createWebSocketTask(WebPageProxyIdentifier, NetworkSocketChannel&, const WebCore::ResourceRequest&, const String& protocol, const WebCore::ClientOrigin&, bool, bool, OptionSet<WebCore::NetworkConnectionIntegrity>)
{
    return nullptr;
}

void NetworkSession::registerNetworkDataTask(NetworkDataTask& task)
{
    // Unregistration happens automatically in ThreadSafeWeakHashSet::amortizedCleanupIfNeeded.
    m_dataTaskSet.add(task);

    // FIXME: This is not in a good place. It should probably be in the NetworkDataTask constructor.
#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    task.setEmulatedConditions(m_bytesPerSecondLimit);
#endif
}

NetworkLoadScheduler& NetworkSession::networkLoadScheduler()
{
    if (!m_networkLoadScheduler)
        m_networkLoadScheduler = makeUnique<NetworkLoadScheduler>();
    return *m_networkLoadScheduler;
}

String NetworkSession::attributedBundleIdentifierFromPageIdentifier(WebPageProxyIdentifier identifier) const
{
    return m_attributedBundleIdentifierFromPageIdentifiers.get(identifier);
}

void NetworkSession::lowMemoryHandler(Critical)
{
    clearPrefetchCache();

#if ENABLE(SERVICE_WORKER)
    if (m_swServer)
        m_swServer->handleLowMemoryWarning();
#endif
    m_storageManager->handleLowMemoryWarning();
}

#if ENABLE(SERVICE_WORKER)
void NetworkSession::addNavigationPreloaderTask(ServiceWorkerFetchTask& task)
{
    m_navigationPreloaders.add(task.fetchIdentifier(), task);
}

void NetworkSession::removeNavigationPreloaderTask(ServiceWorkerFetchTask& task)
{
    m_navigationPreloaders.remove(task.fetchIdentifier());
}

ServiceWorkerFetchTask* NetworkSession::navigationPreloaderTaskFromFetchIdentifier(FetchIdentifier identifier)
{
    return m_navigationPreloaders.get(identifier).get();
}

WebSWOriginStore* NetworkSession::swOriginStore() const
{
    return m_swServer ? &static_cast<WebSWOriginStore&>(m_swServer->originStore()) : nullptr;
}

void NetworkSession::registerSWServerConnection(WebSWServerConnection& connection)
{
    auto* store = swOriginStore();
    ASSERT(store);
    if (store)
        store->registerSWServerConnection(connection);
}

void NetworkSession::unregisterSWServerConnection(WebSWServerConnection& connection)
{
    if (auto* store = swOriginStore())
        store->unregisterSWServerConnection(connection);
}

SWServer& NetworkSession::ensureSWServer()
{
    if (!m_swServer) {
        auto info = valueOrDefault(m_serviceWorkerInfo);
        auto path = info.databasePath;
        // There should already be a registered path for this PAL::SessionID.
        // If there's not, then where did this PAL::SessionID come from?
        ASSERT(m_sessionID.isEphemeral() || !path.isEmpty());

#if ENABLE(APP_BOUND_DOMAINS)
        auto appBoundDomainsCallback = [this](auto&& completionHandler) {
            m_networkProcess->parentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::GetAppBoundDomains { m_sessionID }, WTFMove(completionHandler), 0);
        };
#else
        auto appBoundDomainsCallback = [] (auto&& completionHandler) {
            completionHandler({ });
        };
#endif
        m_swServer = makeUnique<SWServer>(makeUniqueRef<WebSWOriginStore>(), info.processTerminationDelayEnabled, WTFMove(path), m_sessionID, shouldRunServiceWorkersOnMainThreadForTesting(), m_networkProcess->parentProcessHasServiceWorkerEntitlement(), overrideServiceWorkerRegistrationCountTestingValue(), [this](auto&& jobData, bool shouldRefreshCache, auto&& request, auto&& completionHandler) mutable {
            ServiceWorkerSoftUpdateLoader::start(this, WTFMove(jobData), shouldRefreshCache, WTFMove(request), WTFMove(completionHandler));
        }, [this](auto& registrableDomain, std::optional<ProcessIdentifier> requestingProcessIdentifier, std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier, auto&& completionHandler) {
            ASSERT(!registrableDomain.isEmpty());
            m_networkProcess->parentProcessConnection()->sendWithAsyncReply(Messages::NetworkProcessProxy::EstablishRemoteWorkerContextConnectionToNetworkProcess { RemoteWorkerType::ServiceWorker, registrableDomain, requestingProcessIdentifier, serviceWorkerPageIdentifier, m_sessionID }, [completionHandler = WTFMove(completionHandler)] (auto) mutable {
                completionHandler();
            }, 0);
        }, WTFMove(appBoundDomainsCallback), [this](auto webProcessIdentifier, auto requestingProcessIdentifier, auto&& firstPartyForCookies) {
            if (requestingProcessIdentifier && (requestingProcessIdentifier != webProcessIdentifier) && !m_networkProcess->allowsFirstPartyForCookies(requestingProcessIdentifier.value(), firstPartyForCookies)) {
                ASSERT_NOT_REACHED();
                return;
            }
            m_networkProcess->addAllowedFirstPartyForCookies(webProcessIdentifier, WTFMove(firstPartyForCookies), [] { });
        });
    }
    return *m_swServer;
}

bool NetworkSession::hasServiceWorkerDatabasePath() const
{
    return m_serviceWorkerInfo && !m_serviceWorkerInfo->databasePath.isEmpty();
}

#endif // ENABLE(SERVICE_WORKER)

WebSharedWorkerServer& NetworkSession::ensureSharedWorkerServer()
{
    if (!m_sharedWorkerServer)
        m_sharedWorkerServer = makeUnique<WebSharedWorkerServer>(*this);
    return *m_sharedWorkerServer;
}

CacheStorage::Engine& NetworkSession::ensureCacheEngine()
{
    if (!m_cacheEngine)
        m_cacheEngine = CacheStorage::Engine::create(*this, m_cacheStorageDirectory);

    return *m_cacheEngine;
}

void NetworkSession::clearCacheEngine()
{
    m_cacheEngine = nullptr;
}

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)

void NetworkSession::setEmulatedConditions(std::optional<int64_t>&& bytesPerSecondLimit)
{
    m_bytesPerSecondLimit = WTFMove(bytesPerSecondLimit);

    m_dataTaskSet.forEach([&] (auto& task) {
        task.setEmulatedConditions(m_bytesPerSecondLimit);
    });
}

#endif // ENABLE(INSPECTOR_NETWORK_THROTTLING)

bool NetworkSession::needsAdditionalNetworkConnectionIntegritySettings(const ResourceRequest& request)
{
    if (request.isThirdParty())
        return false;

    if (request.url().host() == request.firstPartyForCookies().host())
        return false;

    return true;
}

} // namespace WebKit

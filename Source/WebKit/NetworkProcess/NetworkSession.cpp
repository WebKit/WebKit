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

#include "AdClickAttributionManager.h"
#include "Logging.h"
#include "NetworkProcess.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkResourceLoadParameters.h"
#include "NetworkResourceLoader.h"
#include "NetworkSessionCreationParameters.h"
#include "PingLoad.h"
#include "WebPageProxy.h"
#include "WebPageProxyMessages.h"
#include "WebProcessProxy.h"
#include "WebSocketTask.h"
#include <WebCore/AdClickAttribution.h>
#include <WebCore/CookieJar.h>
#include <WebCore/ResourceRequest.h>

#if PLATFORM(COCOA)
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

std::unique_ptr<NetworkSession> NetworkSession::create(NetworkProcess& networkProcess, NetworkSessionCreationParameters&& parameters)
{
#if PLATFORM(COCOA)
    return NetworkSessionCocoa::create(networkProcess, WTFMove(parameters));
#endif
#if USE(SOUP)
    return NetworkSessionSoup::create(networkProcess, WTFMove(parameters));
#endif
#if USE(CURL)
    return NetworkSessionCurl::create(networkProcess, WTFMove(parameters));
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

NetworkSession::NetworkSession(NetworkProcess& networkProcess, const NetworkSessionCreationParameters& parameters)
    : m_sessionID(parameters.sessionID)
    , m_networkProcess(networkProcess)
#if ENABLE(RESOURCE_LOAD_STATISTICS)
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
    , m_adClickAttribution(makeUniqueRef<AdClickAttributionManager>(networkProcess, parameters.sessionID))
    , m_testSpeedMultiplier(parameters.testSpeedMultiplier)
    , m_allowsServerPreconnect(parameters.allowsServerPreconnect)
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
    }

    m_isStaleWhileRevalidateEnabled = parameters.staleWhileRevalidateEnabled;

    m_adClickAttribution->setPingLoadFunction([this, weakThis = makeWeakPtr(this)](NetworkResourceLoadParameters&& loadParameters, CompletionHandler<void(const WebCore::ResourceError&, const WebCore::ResourceResponse&)>&& completionHandler) {
        if (!weakThis)
            return;
        // PingLoad manages its own lifetime, deleting itself when its purpose has been fulfilled.
        new PingLoad(m_networkProcess, m_sessionID, WTFMove(loadParameters), WTFMove(completionHandler));
    });

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    setResourceLoadStatisticsEnabled(parameters.resourceLoadStatisticsParameters.enabled);
#endif
}

NetworkSession::~NetworkSession()
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    destroyResourceLoadStatistics([] { });
#endif
}

#if ENABLE(RESOURCE_LOAD_STATISTICS)
void NetworkSession::destroyResourceLoadStatistics(CompletionHandler<void()>&& completionHandler)
{
    if (!m_resourceLoadStatistics)
        return completionHandler();

    m_resourceLoadStatistics->didDestroyNetworkSession(WTFMove(completionHandler));
    m_resourceLoadStatistics = nullptr;
}

void NetworkSession::flushAndDestroyPersistentStore(CompletionHandler<void()>&& completionHandler)
{
    if (!m_resourceLoadStatistics)
        return completionHandler();

    m_resourceLoadStatistics->flushAndDestroyPersistentStore(WTFMove(completionHandler));
}
#endif

void NetworkSession::invalidateAndCancel()
{
    for (auto* task : m_dataTaskSet)
        task->invalidateAndCancel();
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (m_resourceLoadStatistics)
        m_resourceLoadStatistics->invalidateAndCancel();
#endif
#if ASSERT_ENABLED
    m_isInvalidated = true;
#endif
}

#if ENABLE(RESOURCE_LOAD_STATISTICS)
void NetworkSession::setResourceLoadStatisticsEnabled(bool enable)
{
    ASSERT(!m_isInvalidated);
    if (auto* storageSession = networkStorageSession())
        storageSession->setResourceLoadStatisticsEnabled(enable);
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
        m_resourceLoadStatistics->setPrevalentResourceForDebugMode(m_resourceLoadStatisticsManualPrevalentResource, [] { });
    forwardResourceLoadStatisticsSettings();
}

void NetworkSession::recreateResourceLoadStatisticStore(CompletionHandler<void()>&& completionHandler)
{
    destroyResourceLoadStatistics([this, weakThis = makeWeakPtr(*this), completionHandler = WTFMove(completionHandler)] () mutable {
        if (!weakThis)
            return completionHandler();
        m_resourceLoadStatistics = WebResourceLoadStatisticsStore::create(*this, m_resourceLoadStatisticsDirectory, m_shouldIncludeLocalhostInResourceLoadStatistics, (m_sessionID.isEphemeral() ? ResourceLoadStatistics::IsEphemeral::Yes : ResourceLoadStatistics::IsEphemeral::No));
        forwardResourceLoadStatisticsSettings();
        if (!m_sessionID.isEphemeral())
            m_resourceLoadStatistics->populateMemoryStoreFromDisk(WTFMove(completionHandler));
        else
            completionHandler();
    });
}

void NetworkSession::forwardResourceLoadStatisticsSettings()
{
    m_resourceLoadStatistics->setThirdPartyCookieBlockingMode(m_thirdPartyCookieBlockingMode);
    m_resourceLoadStatistics->setSameSiteStrictEnforcementEnabled(m_sameSiteStrictEnforcementEnabled);
    m_resourceLoadStatistics->setFirstPartyWebsiteDataRemovalMode(m_firstPartyWebsiteDataRemovalMode, [] { });
    m_resourceLoadStatistics->setStandaloneApplicationDomain(m_standaloneApplicationDomain, [] { });
}

bool NetworkSession::isResourceLoadStatisticsEnabled() const
{
    return !!m_resourceLoadStatistics;
}

void NetworkSession::notifyResourceLoadStatisticsProcessed()
{
    m_networkProcess->parentProcessConnection()->send(Messages::NetworkProcessProxy::NotifyResourceLoadStatisticsProcessed(), 0);
}

void NetworkSession::logDiagnosticMessageWithValue(const String& message, const String& description, unsigned value, unsigned significantFigures, WebCore::ShouldSample shouldSample)
{
    m_networkProcess->parentProcessConnection()->send(Messages::WebPageProxy::LogDiagnosticMessageWithValue(message, description, value, significantFigures, shouldSample), 0);
}

void NetworkSession::notifyPageStatisticsTelemetryFinished(unsigned numberOfPrevalentResources, unsigned numberOfPrevalentResourcesWithUserInteraction, unsigned numberOfPrevalentResourcesWithoutUserInteraction, unsigned topPrevalentResourceWithUserInteractionDaysSinceUserInteraction, unsigned medianDaysSinceUserInteractionPrevalentResourceWithUserInteraction, unsigned top3NumberOfPrevalentResourcesWithUI, unsigned top3MedianSubFrameWithoutUI, unsigned top3MedianSubResourceWithoutUI, unsigned top3MedianUniqueRedirectsWithoutUI, unsigned top3MedianDataRecordsRemovedWithoutUI)
{
    m_networkProcess->parentProcessConnection()->send(Messages::NetworkProcessProxy::NotifyResourceLoadStatisticsTelemetryFinished(numberOfPrevalentResources, numberOfPrevalentResourcesWithUserInteraction, numberOfPrevalentResourcesWithoutUserInteraction, topPrevalentResourceWithUserInteractionDaysSinceUserInteraction, medianDaysSinceUserInteractionPrevalentResourceWithUserInteraction, top3NumberOfPrevalentResourcesWithUI, top3MedianSubFrameWithoutUI, top3MedianSubResourceWithoutUI, top3MedianUniqueRedirectsWithoutUI, top3MedianDataRecordsRemovedWithoutUI), 0);
}

void NetworkSession::deleteAndRestrictWebsiteDataForRegistrableDomains(OptionSet<WebsiteDataType> dataTypes, RegistrableDomainsToDeleteOrRestrictWebsiteDataFor&& domains, bool shouldNotifyPage, CompletionHandler<void(const HashSet<RegistrableDomain>&)>&& completionHandler)
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
#endif // ENABLE(RESOURCE_LOAD_STATISTICS)

void NetworkSession::storeAdClickAttribution(WebCore::AdClickAttribution&& adClickAttribution)
{
    m_adClickAttribution->storeUnconverted(WTFMove(adClickAttribution));
}

void NetworkSession::handleAdClickAttributionConversion(AdClickAttribution::Conversion&& conversion, const URL& requestURL, const WebCore::ResourceRequest& redirectRequest)
{
    m_adClickAttribution->handleConversion(WTFMove(conversion), requestURL, redirectRequest);
}

void NetworkSession::dumpAdClickAttribution(CompletionHandler<void(String)>&& completionHandler)
{
    m_adClickAttribution->toString(WTFMove(completionHandler));
}

void NetworkSession::clearAdClickAttribution()
{
    m_adClickAttribution->clear();
}

void NetworkSession::clearAdClickAttributionForRegistrableDomain(WebCore::RegistrableDomain&& domain)
{
    m_adClickAttribution->clearForRegistrableDomain(WTFMove(domain));
}

void NetworkSession::setAdClickAttributionOverrideTimerForTesting(bool value)
{
    m_adClickAttribution->setOverrideTimerForTesting(value);
}

void NetworkSession::setAdClickAttributionConversionURLForTesting(URL&& url)
{
    m_adClickAttribution->setConversionURLForTesting(WTFMove(url));
}

void NetworkSession::markAdClickAttributionsAsExpiredForTesting()
{
    m_adClickAttribution->markAllUnconvertedAsExpiredForTesting();
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

std::unique_ptr<WebSocketTask> NetworkSession::createWebSocketTask(NetworkSocketChannel&, const WebCore::ResourceRequest&, const String& protocol)
{
    return nullptr;
}

} // namespace WebKit

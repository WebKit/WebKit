/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "ResourceLoadStatisticsStore.h"

#if ENABLE(RESOURCE_LOAD_STATISTICS)

#include "Logging.h"
#include "NetworkProcess.h"
#include "NetworkSession.h"
#include "PluginProcessManager.h"
#include "PluginProcessProxy.h"
#include "ResourceLoadStatisticsPersistentStorage.h"
#include "StorageAccessStatus.h"
#include "WebProcessProxy.h"
#include "WebResourceLoadStatisticsTelemetry.h"
#include "WebsiteDataStore.h"
#include <WebCore/CookieJar.h>
#include <WebCore/KeyedCoding.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/DateMath.h>
#include <wtf/MathExtras.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {
using namespace WebCore;

constexpr Seconds minimumStatisticsProcessingInterval { 5_s };

static String domainsToString(const Vector<RegistrableDomain>& domains)
{
    StringBuilder builder;
    for (auto& domain : domains) {
        if (!builder.isEmpty())
            builder.appendLiteral(", ");
        builder.append(domain.string());
    }
    return builder.toString();
}

static String domainsToString(const RegistrableDomainsToDeleteOrRestrictWebsiteDataFor& domainsToRemoveOrRestrictWebsiteDataFor)
{
    StringBuilder builder;
    for (auto& domain : domainsToRemoveOrRestrictWebsiteDataFor.domainsToDeleteAllCookiesFor) {
        if (!builder.isEmpty())
            builder.appendLiteral(", ");
        builder.append(domain.string());
        builder.appendLiteral("(all data)");
    }
    for (auto& domain : domainsToRemoveOrRestrictWebsiteDataFor.domainsToDeleteAllButHttpOnlyCookiesFor) {
        if (!builder.isEmpty())
            builder.appendLiteral(", ");
        builder.append(domain.string());
        builder.appendLiteral("(all but HttpOnly cookies)");
    }
    for (auto& domain : domainsToRemoveOrRestrictWebsiteDataFor.domainsToDeleteAllNonCookieWebsiteDataFor) {
        if (!builder.isEmpty())
            builder.appendLiteral(", ");
        builder.append(domain.string());
        builder.appendLiteral("(all but cookies)");
    }
    return builder.toString();
}

OperatingDate OperatingDate::fromWallTime(WallTime time)
{
    double ms = time.secondsSinceEpoch().milliseconds();
    int year = msToYear(ms);
    int yearDay = dayInYear(ms, year);
    int month = monthFromDayInYear(yearDay, isLeapYear(year));
    int monthDay = dayInMonthFromDayInYear(yearDay, isLeapYear(year));

    return OperatingDate { year, month, monthDay };
}

OperatingDate OperatingDate::today()
{
    return OperatingDate::fromWallTime(WallTime::now());
}

Seconds OperatingDate::secondsSinceEpoch() const
{
    return Seconds { dateToDaysFrom1970(m_year, m_month, m_monthDay) * secondsPerDay };
}

bool OperatingDate::operator==(const OperatingDate& other) const
{
    return m_monthDay == other.m_monthDay && m_month == other.m_month && m_year == other.m_year;
}

bool OperatingDate::operator<(const OperatingDate& other) const
{
    return secondsSinceEpoch() < other.secondsSinceEpoch();
}

bool OperatingDate::operator<=(const OperatingDate& other) const
{
    return secondsSinceEpoch() <= other.secondsSinceEpoch();
}

ResourceLoadStatisticsStore::ResourceLoadStatisticsStore(WebResourceLoadStatisticsStore& store, WorkQueue& workQueue, ShouldIncludeLocalhost shouldIncludeLocalhost)
    : m_store(store)
    , m_workQueue(workQueue)
    , m_shouldIncludeLocalhost(shouldIncludeLocalhost)
{
    ASSERT(!RunLoop::isMain());
}

ResourceLoadStatisticsStore::~ResourceLoadStatisticsStore()
{
    ASSERT(!RunLoop::isMain());
}

unsigned ResourceLoadStatisticsStore::computeImportance(const ResourceLoadStatistics& resourceStatistic)
{
    unsigned importance = ResourceLoadStatisticsStore::maxImportance;
    if (!resourceStatistic.isPrevalentResource)
        importance -= 1;
    if (!resourceStatistic.hadUserInteraction)
        importance -= 2;
    return importance;
}

void ResourceLoadStatisticsStore::setNotifyPagesWhenDataRecordsWereScanned(bool value)
{
    ASSERT(!RunLoop::isMain());
    m_parameters.shouldNotifyPagesWhenDataRecordsWereScanned = value;
}

bool ResourceLoadStatisticsStore::shouldSkip(const RegistrableDomain& domain) const
{
    ASSERT(!RunLoop::isMain());
    return !(parameters().isRunningTest)
    && m_shouldIncludeLocalhost == ShouldIncludeLocalhost::No && domain.string() == "localhost";
}

void ResourceLoadStatisticsStore::setIsRunningTest(bool value)
{
    ASSERT(!RunLoop::isMain());
    m_parameters.isRunningTest = value;
}

void ResourceLoadStatisticsStore::setShouldClassifyResourcesBeforeDataRecordsRemoval(bool value)
{
    ASSERT(!RunLoop::isMain());
    m_parameters.shouldClassifyResourcesBeforeDataRecordsRemoval = value;
}

void ResourceLoadStatisticsStore::setShouldSubmitTelemetry(bool value)
{
    ASSERT(!RunLoop::isMain());
    m_parameters.shouldSubmitTelemetry = value;
}

void ResourceLoadStatisticsStore::removeDataRecords(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (!shouldRemoveDataRecords()) {
        completionHandler();
        return;
    }

#if ENABLE(NETSCAPE_PLUGIN_API)
    m_activePluginTokens.clear();
    for (const auto& plugin : PluginProcessManager::singleton().pluginProcesses())
        m_activePluginTokens.add(plugin->pluginProcessToken());
#endif

    auto domainsToDeleteOrRestrictWebsiteDataFor = registrableDomainsToDeleteOrRestrictWebsiteDataFor();
    if (domainsToDeleteOrRestrictWebsiteDataFor.isEmpty()) {
        completionHandler();
        return;
    }

    if (UNLIKELY(m_debugLoggingEnabled)) {
        RELEASE_LOG_INFO(ITPDebug, "About to remove data records for %" PUBLIC_LOG_STRING ".", domainsToString(domainsToDeleteOrRestrictWebsiteDataFor).utf8().data());
        debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("[ITP] About to remove data records for: ["_s, domainsToString(domainsToDeleteOrRestrictWebsiteDataFor), "]."_s));
    }

    setDataRecordsBeingRemoved(true);

    RunLoop::main().dispatch([store = makeRef(m_store), domainsToDeleteOrRestrictWebsiteDataFor = crossThreadCopy(domainsToDeleteOrRestrictWebsiteDataFor), completionHandler = WTFMove(completionHandler), weakThis = makeWeakPtr(*this), shouldNotifyPagesWhenDataRecordsWereScanned = m_parameters.shouldNotifyPagesWhenDataRecordsWereScanned, workQueue = m_workQueue] () mutable {
        store->deleteAndRestrictWebsiteDataForRegistrableDomains(WebResourceLoadStatisticsStore::monitoredDataTypes(), WTFMove(domainsToDeleteOrRestrictWebsiteDataFor), shouldNotifyPagesWhenDataRecordsWereScanned, [completionHandler = WTFMove(completionHandler), weakThis = WTFMove(weakThis), workQueue](const HashSet<RegistrableDomain>& domainsWithDeletedWebsiteData) mutable {
            workQueue->dispatch([domainsWithDeletedWebsiteData = crossThreadCopy(domainsWithDeletedWebsiteData), completionHandler = WTFMove(completionHandler), weakThis = WTFMove(weakThis)] () mutable {
                if (!weakThis) {
                    completionHandler();
                    return;
                }

                weakThis->incrementRecordsDeletedCountForDomains(WTFMove(domainsWithDeletedWebsiteData));
                weakThis->setDataRecordsBeingRemoved(false);

                auto dataRecordRemovalCompletionHandlers = WTFMove(weakThis->m_dataRecordRemovalCompletionHandlers);
                completionHandler();

                for (auto& dataRecordRemovalCompletionHandler : dataRecordRemovalCompletionHandlers)
                    dataRecordRemovalCompletionHandler();

                if (UNLIKELY(weakThis->m_debugLoggingEnabled)) {
                    RELEASE_LOG_INFO(ITPDebug, "Done removing data records.");
                    weakThis->debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, "[ITP] Done removing data records"_s);
                }
            });
        });
    });
}

void ResourceLoadStatisticsStore::processStatisticsAndDataRecords()
{
    ASSERT(!RunLoop::isMain());

    if (parameters().isRunningTest && !m_parameters.shouldNotifyPagesWhenDataRecordsWereScanned)
        return;

    if (m_parameters.shouldClassifyResourcesBeforeDataRecordsRemoval)
        classifyPrevalentResources();
    
    removeDataRecords([this, weakThis = makeWeakPtr(*this)] () mutable {
        ASSERT(!RunLoop::isMain());
        if (!weakThis)
            return;

        pruneStatisticsIfNeeded();
        syncStorageIfNeeded();

        logTestingEvent("Storage Synced"_s);

        if (!m_parameters.shouldNotifyPagesWhenDataRecordsWereScanned)
            return;

        RunLoop::main().dispatch([store = makeRef(m_store)] {
            store->notifyResourceLoadStatisticsProcessed();
        });
    });
}

void ResourceLoadStatisticsStore::grandfatherExistingWebsiteData(CompletionHandler<void()>&& callback)
{
    ASSERT(!RunLoop::isMain());

    RunLoop::main().dispatch([weakThis = makeWeakPtr(*this), callback = WTFMove(callback), shouldNotifyPagesWhenDataRecordsWereScanned = m_parameters.shouldNotifyPagesWhenDataRecordsWereScanned, workQueue = m_workQueue, store = makeRef(m_store)] () mutable {
        store->registrableDomainsWithWebsiteData(WebResourceLoadStatisticsStore::monitoredDataTypes(), shouldNotifyPagesWhenDataRecordsWereScanned, [weakThis = WTFMove(weakThis), callback = WTFMove(callback), workQueue] (HashSet<RegistrableDomain>&& domainsWithWebsiteData) mutable {
            workQueue->dispatch([weakThis = WTFMove(weakThis), domainsWithWebsiteData = crossThreadCopy(domainsWithWebsiteData), callback = WTFMove(callback)] () mutable {
                if (!weakThis) {
                    callback();
                    return;
                }

                weakThis->grandfatherDataForDomains(domainsWithWebsiteData);
                weakThis->m_endOfGrandfatheringTimestamp = WallTime::now() + weakThis->m_parameters.grandfatheringTime;
                weakThis->syncStorageImmediately();
                callback();
                weakThis->logTestingEvent("Grandfathered"_s);
            });
        });
    });
}

void ResourceLoadStatisticsStore::setResourceLoadStatisticsDebugMode(bool enable)
{
    ASSERT(!RunLoop::isMain());

    if (m_debugModeEnabled == enable)
        return;

    m_debugModeEnabled = enable;
    m_debugLoggingEnabled = enable;

    if (m_debugLoggingEnabled) {
        RELEASE_LOG_INFO(ITPDebug, "Turned ITP Debug Mode on.");
        debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, "[ITP] Turned Debug Mode on."_s);
    } else {
        RELEASE_LOG_INFO(ITPDebug, "Turned ITP Debug Mode off.");
        debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, "[ITP] Turned Debug Mode off."_s);
    }

    ensurePrevalentResourcesForDebugMode();
    // This will log the current cookie blocking state.
    if (enable)
        updateCookieBlocking([]() { });
}

void ResourceLoadStatisticsStore::setPrevalentResourceForDebugMode(const RegistrableDomain& domain)
{
    m_debugManualPrevalentResource = domain;
}

#if ENABLE(APP_BOUND_DOMAINS)
void ResourceLoadStatisticsStore::setAppBoundDomains(HashSet<RegistrableDomain>&& domains)
{
    m_appBoundDomains = WTFMove(domains);
}
#endif

void ResourceLoadStatisticsStore::scheduleStatisticsProcessingRequestIfNecessary()
{
    ASSERT(!RunLoop::isMain());

    m_pendingStatisticsProcessingRequestIdentifier = ++m_lastStatisticsProcessingRequestIdentifier;
    m_workQueue->dispatchAfter(minimumStatisticsProcessingInterval, [this, weakThis = makeWeakPtr(*this), statisticsProcessingRequestIdentifier = *m_pendingStatisticsProcessingRequestIdentifier] {
        if (!weakThis)
            return;

        if (!m_pendingStatisticsProcessingRequestIdentifier || *m_pendingStatisticsProcessingRequestIdentifier != statisticsProcessingRequestIdentifier) {
            // This request has been canceled.
            return;
        }

        updateCookieBlocking([]() { });
        processStatisticsAndDataRecords();
    });
}

void ResourceLoadStatisticsStore::cancelPendingStatisticsProcessingRequest()
{
    ASSERT(!RunLoop::isMain());

    m_pendingStatisticsProcessingRequestIdentifier = WTF::nullopt;
}

void ResourceLoadStatisticsStore::setTimeToLiveUserInteraction(Seconds seconds)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(seconds >= 0_s);

    m_parameters.timeToLiveUserInteraction = seconds;
}

void ResourceLoadStatisticsStore::setMinimumTimeBetweenDataRecordsRemoval(Seconds seconds)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(seconds >= 0_s);

    m_parameters.minimumTimeBetweenDataRecordsRemoval = seconds;
}

void ResourceLoadStatisticsStore::setGrandfatheringTime(Seconds seconds)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(seconds >= 0_s);

    m_parameters.grandfatheringTime = seconds;
}

void ResourceLoadStatisticsStore::setCacheMaxAgeCap(Seconds seconds)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(seconds >= 0_s);

    m_parameters.cacheMaxAgeCapTime = seconds;
    updateCacheMaxAgeCap();
}

void ResourceLoadStatisticsStore::updateCacheMaxAgeCap()
{
    ASSERT(!RunLoop::isMain());
    
    RunLoop::main().dispatch([store = makeRef(m_store), seconds = m_parameters.cacheMaxAgeCapTime] () {
        store->setCacheMaxAgeCap(seconds, [] { });
    });
}

void ResourceLoadStatisticsStore::setAgeCapForClientSideCookies(Seconds seconds)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(seconds >= 0_s);
    
    m_parameters.clientSideCookiesAgeCapTime = seconds;
    updateClientSideCookiesAgeCap();
}

void ResourceLoadStatisticsStore::updateClientSideCookiesAgeCap()
{
    ASSERT(!RunLoop::isMain());

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    RunLoop::main().dispatch([store = makeRef(m_store), seconds = m_parameters.clientSideCookiesAgeCapTime] () {
        if (auto* networkSession = store->networkSession()) {
            if (auto* storageSession = networkSession->networkStorageSession())
                storageSession->setAgeCapForClientSideCookies(seconds);
        }
    });
#endif
}

bool ResourceLoadStatisticsStore::shouldRemoveDataRecords() const
{
    ASSERT(!RunLoop::isMain());

    if (m_dataRecordsBeingRemoved)
        return false;

#if ENABLE(NETSCAPE_PLUGIN_API)
    for (const auto& plugin : PluginProcessManager::singleton().pluginProcesses()) {
        if (!m_activePluginTokens.contains(plugin->pluginProcessToken()))
            return true;
    }
#endif

    return !m_lastTimeDataRecordsWereRemoved || MonotonicTime::now() >= (m_lastTimeDataRecordsWereRemoved + m_parameters.minimumTimeBetweenDataRecordsRemoval) || parameters().isRunningTest;
}

void ResourceLoadStatisticsStore::setDataRecordsBeingRemoved(bool value)
{
    ASSERT(!RunLoop::isMain());

    m_dataRecordsBeingRemoved = value;
    if (m_dataRecordsBeingRemoved)
        m_lastTimeDataRecordsWereRemoved = MonotonicTime::now();
}

void ResourceLoadStatisticsStore::updateCookieBlockingForDomains(const RegistrableDomainsToBlockCookiesFor& domainsToBlock, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());
    
    RunLoop::main().dispatch([store = makeRef(m_store), domainsToBlock = crossThreadCopy(domainsToBlock), completionHandler = WTFMove(completionHandler)] () mutable {
        store->callUpdatePrevalentDomainsToBlockCookiesForHandler(domainsToBlock, [store, completionHandler = WTFMove(completionHandler)]() mutable {
            store->statisticsQueue().dispatch([completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler();
            });
        });
    });
}

bool ResourceLoadStatisticsStore::shouldEnforceSameSiteStrictForSpecificDomain(const RegistrableDomain& domain) const
{
    // We currently know of no domains that need this protection.
    UNUSED_PARAM(domain);
    return false;
}

void ResourceLoadStatisticsStore::setMaxStatisticsEntries(size_t maximumEntryCount)
{
    ASSERT(!RunLoop::isMain());

    m_parameters.maxStatisticsEntries = maximumEntryCount;
}

void ResourceLoadStatisticsStore::setPruneEntriesDownTo(size_t pruneTargetCount)
{
    ASSERT(!RunLoop::isMain());

    m_parameters.pruneEntriesDownTo = pruneTargetCount;
}

void ResourceLoadStatisticsStore::resetParametersToDefaultValues()
{
    ASSERT(!RunLoop::isMain());

    m_parameters = { };
    m_appBoundDomains.clear();
}

void ResourceLoadStatisticsStore::logTestingEvent(const String& event)
{
    ASSERT(!RunLoop::isMain());

    RunLoop::main().dispatch([store = makeRef(m_store), event = event.isolatedCopy()] {
        store->logTestingEvent(event);
    });
}

void ResourceLoadStatisticsStore::removeAllStorageAccess(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());
    RunLoop::main().dispatch([store = makeRef(m_store), completionHandler = WTFMove(completionHandler)]() mutable {
        store->removeAllStorageAccess([store, completionHandler = WTFMove(completionHandler)]() mutable {
            store->statisticsQueue().dispatch([completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler();
            });
        });
    });
}

void ResourceLoadStatisticsStore::didCreateNetworkProcess()
{
    ASSERT(!RunLoop::isMain());

    updateCookieBlocking([]() { });
    updateCacheMaxAgeCap();
    updateClientSideCookiesAgeCap();
}

void ResourceLoadStatisticsStore::debugBroadcastConsoleMessage(MessageSource source, MessageLevel level, const String& message)
{
    if (!RunLoop::isMain()) {
        RunLoop::main().dispatch([&, weakThis = makeWeakPtr(*this), source = crossThreadCopy(source), level = crossThreadCopy(level), message = crossThreadCopy(message)]() {
            if (!weakThis)
                return;

            debugBroadcastConsoleMessage(source, level, message);
        });
        return;
    }

    if (auto* networkSession = m_store.networkSession())
        networkSession->networkProcess().broadcastConsoleMessage(networkSession->sessionID(), source, level, message);
}

void ResourceLoadStatisticsStore::debugLogDomainsInBatches(const char* action, const RegistrableDomainsToBlockCookiesFor& domainsToBlock)
{
    ASSERT(debugLoggingEnabled());

    Vector<RegistrableDomain> domains;
    domains.appendVector(domainsToBlock.domainsToBlockAndDeleteCookiesFor);
    domains.appendVector(domainsToBlock.domainsToBlockButKeepCookiesFor);
    if (domains.isEmpty())
        return;

    debugBroadcastConsoleMessage(MessageSource::ITPDebug, MessageLevel::Info, makeString("[ITP] "_s, action, " to: ["_s, domainsToString(domains), "]."_s));

    static const auto maxNumberOfDomainsInOneLogStatement = 50;

    if (domains.size() <= maxNumberOfDomainsInOneLogStatement) {
        RELEASE_LOG_INFO(ITPDebug, "%" PUBLIC_LOG_STRING " to: %" PUBLIC_LOG_STRING ".", action, domainsToString(domains).utf8().data());
        return;
    }

    Vector<RegistrableDomain> batch;
    batch.reserveInitialCapacity(maxNumberOfDomainsInOneLogStatement);
    auto batchNumber = 1;
    unsigned numberOfBatches = std::ceil(domains.size() / static_cast<float>(maxNumberOfDomainsInOneLogStatement));

    for (auto& domain : domains) {
        if (batch.size() == maxNumberOfDomainsInOneLogStatement) {
            RELEASE_LOG_INFO(ITPDebug, "%" PUBLIC_LOG_STRING " to (%{public}d of %u): %" PUBLIC_LOG_STRING ".", action, batchNumber, numberOfBatches, domainsToString(batch).utf8().data());
            batch.shrink(0);
            ++batchNumber;
        }
        batch.append(domain);
    }
    if (!batch.isEmpty())
        RELEASE_LOG_INFO(ITPDebug, "%" PUBLIC_LOG_STRING " to (%{public}d of %u): %" PUBLIC_LOG_STRING ".", action, batchNumber, numberOfBatches, domainsToString(batch).utf8().data());
}

bool ResourceLoadStatisticsStore::shouldExemptFromWebsiteDataDeletion(const RegistrableDomain& domain) const
{
    return !domain.isEmpty() && (domain == m_standaloneApplicationDomain || m_appBoundDomains.contains(domain));
}

} // namespace WebKit

#endif

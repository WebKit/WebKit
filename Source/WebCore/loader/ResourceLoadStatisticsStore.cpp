/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#include "KeyedCoding.h"
#include "Logging.h"
#include "NetworkStorageSession.h"
#include "PlatformStrategies.h"
#include "ResourceLoadStatistics.h"
#include "SharedBuffer.h"
#include "URL.h"
#include <wtf/CrossThreadCopier.h>
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>

namespace WebCore {

static const auto statisticsModelVersion = 4;
static const auto secondsPerHour = 3600;
static const auto secondsPerDay = 24 * secondsPerHour;
static auto timeToLiveUserInteraction = 30 * secondsPerDay;
static auto timeToLiveCookiePartitionFree = 1 * secondsPerDay;
static auto grandfatheringTime = 1 * secondsPerHour;
static auto minimumTimeBetweeenDataRecordsRemoval = 60;

Ref<ResourceLoadStatisticsStore> ResourceLoadStatisticsStore::create()
{
    return adoptRef(*new ResourceLoadStatisticsStore());
}
    
bool ResourceLoadStatisticsStore::isPrevalentResource(const String& primaryDomain) const
{
    auto locker = holdLock(m_statisticsLock);
    auto mapEntry = m_resourceStatisticsMap.find(primaryDomain);
    if (mapEntry == m_resourceStatisticsMap.end())
        return false;

    return mapEntry->value.isPrevalentResource;
}
    
ResourceLoadStatistics& ResourceLoadStatisticsStore::ensureResourceStatisticsForPrimaryDomain(const String& primaryDomain)
{
    ASSERT(m_statisticsLock.isLocked());
    auto addResult = m_resourceStatisticsMap.ensure(primaryDomain, [&primaryDomain] {
        return ResourceLoadStatistics(primaryDomain);
    });

    return addResult.iterator->value;
}

void ResourceLoadStatisticsStore::setResourceStatisticsForPrimaryDomain(const String& primaryDomain, ResourceLoadStatistics&& statistics)
{
    ASSERT(!isMainThread());
    auto locker = holdLock(m_statisticsLock);
    m_resourceStatisticsMap.set(primaryDomain, WTFMove(statistics));
}

typedef HashMap<String, ResourceLoadStatistics>::KeyValuePairType StatisticsValue;

std::unique_ptr<KeyedEncoder> ResourceLoadStatisticsStore::createEncoderFromData()
{
    ASSERT(!isMainThread());
    auto encoder = KeyedEncoder::encoder();

    auto locker = holdLock(m_statisticsLock);
    encoder->encodeUInt32("version", statisticsModelVersion);
    encoder->encodeDouble("endOfGrandfatheringTimestamp", m_endOfGrandfatheringTimestamp);
    
    encoder->encodeObjects("browsingStatistics", m_resourceStatisticsMap.begin(), m_resourceStatisticsMap.end(), [](KeyedEncoder& encoderInner, const StatisticsValue& origin) {
        origin.value.encode(encoderInner);
    });

    return encoder;
}

void ResourceLoadStatisticsStore::readDataFromDecoder(KeyedDecoder& decoder)
{
    ASSERT(!isMainThread());
    ASSERT(m_statisticsLock.isLocked());
    if (m_resourceStatisticsMap.size())
        return;

    unsigned version;
    if (!decoder.decodeUInt32("version", version))
        version = 1;

    static const auto minimumVersionWithGrandfathering = 3;
    if (version > minimumVersionWithGrandfathering) {
        double endOfGrandfatheringTimestamp;
        if (decoder.decodeDouble("endOfGrandfatheringTimestamp", endOfGrandfatheringTimestamp))
            m_endOfGrandfatheringTimestamp = endOfGrandfatheringTimestamp;
        else
            m_endOfGrandfatheringTimestamp = 0;
    }

    Vector<ResourceLoadStatistics> loadedStatistics;
    bool succeeded = decoder.decodeObjects("browsingStatistics", loadedStatistics, [version](KeyedDecoder& decoderInner, ResourceLoadStatistics& statistics) {
        return statistics.decode(decoderInner, version);
    });

    if (!succeeded)
        return;

    Vector<String> prevalentResourceDomainsWithoutUserInteraction;
    prevalentResourceDomainsWithoutUserInteraction.reserveInitialCapacity(loadedStatistics.size());
    
    {
    auto locker = holdLock(m_statisticsLock);
    for (auto& statistics : loadedStatistics) {
        if (statistics.isPrevalentResource && !statistics.hadUserInteraction) {
            prevalentResourceDomainsWithoutUserInteraction.uncheckedAppend(statistics.highLevelDomain);
            statistics.isMarkedForCookiePartitioning = true;
        }
        m_resourceStatisticsMap.set(statistics.highLevelDomain, statistics);
    }
    }
    
    fireShouldPartitionCookiesHandler({ }, prevalentResourceDomainsWithoutUserInteraction, true);
}

void ResourceLoadStatisticsStore::clearInMemory()
{
    ASSERT(!isMainThread());
    {
    auto locker = holdLock(m_statisticsLock);
    m_resourceStatisticsMap.clear();
    }
    
    fireShouldPartitionCookiesHandler({ }, { }, true);
}

void ResourceLoadStatisticsStore::clearInMemoryAndPersistent()
{
    ASSERT(!isMainThread());
    clearInMemory();
    if (m_writePersistentStoreHandler)
        m_writePersistentStoreHandler();
    if (m_grandfatherExistingWebsiteDataHandler)
        m_grandfatherExistingWebsiteDataHandler();
}

String ResourceLoadStatisticsStore::statisticsForOrigin(const String& origin)
{
    auto locker = holdLock(m_statisticsLock);
    auto iter = m_resourceStatisticsMap.find(origin);
    if (iter == m_resourceStatisticsMap.end())
        return emptyString();
    
    return "Statistics for " + origin + ":\n" + iter->value.toString();
}

Vector<ResourceLoadStatistics> ResourceLoadStatisticsStore::takeStatistics()
{
    Vector<ResourceLoadStatistics> statistics;
    
    {
    auto locker = holdLock(m_statisticsLock);
    statistics.reserveInitialCapacity(m_resourceStatisticsMap.size());
    for (auto& statistic : m_resourceStatisticsMap.values())
        statistics.uncheckedAppend(WTFMove(statistic));

    m_resourceStatisticsMap.clear();
    }
    
    return statistics;
}

void ResourceLoadStatisticsStore::mergeStatistics(const Vector<ResourceLoadStatistics>& statistics)
{
    auto locker = holdLock(m_statisticsLock);
    for (auto& statistic : statistics) {
        auto result = m_resourceStatisticsMap.ensure(statistic.highLevelDomain, [&statistic] {
            return ResourceLoadStatistics(statistic.highLevelDomain);
        });
        
        result.iterator->value.merge(statistic);
    }
}

void ResourceLoadStatisticsStore::setNotificationCallback(WTF::Function<void()>&& handler)
{
    m_dataAddedHandler = WTFMove(handler);
}

void ResourceLoadStatisticsStore::setShouldPartitionCookiesCallback(WTF::Function<void(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, bool clearFirst)>&& handler)
{
    m_shouldPartitionCookiesForDomainsHandler = WTFMove(handler);
}
    
void ResourceLoadStatisticsStore::setWritePersistentStoreCallback(WTF::Function<void()>&& handler)
{
    m_writePersistentStoreHandler = WTFMove(handler);
}

void ResourceLoadStatisticsStore::setGrandfatherExistingWebsiteDataCallback(WTF::Function<void()>&& handler)
{
    m_grandfatherExistingWebsiteDataHandler = WTFMove(handler);
}

void ResourceLoadStatisticsStore::setFireTelemetryCallback(WTF::Function<void()>&& handler)
{
    m_fireTelemetryHandler = WTFMove(handler);
}
    
void ResourceLoadStatisticsStore::fireDataModificationHandler()
{
    ASSERT(!isMainThread());
    RunLoop::main().dispatch([this, protectedThis = makeRef(*this)] () {
        if (m_dataAddedHandler)
            m_dataAddedHandler();
    });
}

void ResourceLoadStatisticsStore::fireTelemetryHandler()
{
    ASSERT(isMainThread());
    if (m_fireTelemetryHandler)
        m_fireTelemetryHandler();
}
    
static inline bool shouldPartitionCookies(const ResourceLoadStatistics& statistic)
{
    return statistic.isPrevalentResource
        && (!statistic.hadUserInteraction || currentTime() > statistic.mostRecentUserInteraction + timeToLiveCookiePartitionFree);
}

void ResourceLoadStatisticsStore::fireShouldPartitionCookiesHandler()
{
    ASSERT(!isMainThread());
    Vector<String> domainsToRemove;
    Vector<String> domainsToAdd;
    
    auto locker = holdLock(m_statisticsLock);
    for (auto& resourceStatistic : m_resourceStatisticsMap.values()) {
        bool shouldPartition = shouldPartitionCookies(resourceStatistic);
        if (resourceStatistic.isMarkedForCookiePartitioning && !shouldPartition) {
            resourceStatistic.isMarkedForCookiePartitioning = false;
            domainsToRemove.append(resourceStatistic.highLevelDomain);
        } else if (!resourceStatistic.isMarkedForCookiePartitioning && shouldPartition) {
            resourceStatistic.isMarkedForCookiePartitioning = true;
            domainsToAdd.append(resourceStatistic.highLevelDomain);
        }
    }

    if (domainsToRemove.isEmpty() && domainsToAdd.isEmpty())
        return;

    RunLoop::main().dispatch([this, protectedThis = makeRef(*this), domainsToRemove = CrossThreadCopier<Vector<String>>::copy(domainsToRemove), domainsToAdd = CrossThreadCopier<Vector<String>>::copy(domainsToAdd)] () {
        if (m_shouldPartitionCookiesForDomainsHandler)
            m_shouldPartitionCookiesForDomainsHandler(domainsToRemove, domainsToAdd, false);
    });
}

void ResourceLoadStatisticsStore::fireShouldPartitionCookiesHandler(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, bool clearFirst)
{
    ASSERT(!isMainThread());
    if (domainsToRemove.isEmpty() && domainsToAdd.isEmpty())
        return;
    
    RunLoop::main().dispatch([this, clearFirst, protectedThis = makeRef(*this), domainsToRemove = CrossThreadCopier<Vector<String>>::copy(domainsToRemove), domainsToAdd = CrossThreadCopier<Vector<String>>::copy(domainsToAdd)] () {
        if (m_shouldPartitionCookiesForDomainsHandler)
            m_shouldPartitionCookiesForDomainsHandler(domainsToRemove, domainsToAdd, clearFirst);
    });
    
    auto locker = holdLock(m_statisticsLock);
    if (clearFirst) {
        for (auto& resourceStatistic : m_resourceStatisticsMap.values())
            resourceStatistic.isMarkedForCookiePartitioning = false;
    } else {
        for (auto& domain : domainsToRemove)
            ensureResourceStatisticsForPrimaryDomain(domain).isMarkedForCookiePartitioning = false;
    }

    for (auto& domain : domainsToAdd)
        ensureResourceStatisticsForPrimaryDomain(domain).isMarkedForCookiePartitioning = true;
}

void ResourceLoadStatisticsStore::setTimeToLiveUserInteraction(double seconds)
{
    if (seconds >= 0)
        timeToLiveUserInteraction = seconds;
}

void ResourceLoadStatisticsStore::setTimeToLiveCookiePartitionFree(double seconds)
{
    if (seconds >= 0)
        timeToLiveCookiePartitionFree = seconds;
}

void ResourceLoadStatisticsStore::setMinimumTimeBetweeenDataRecordsRemoval(double seconds)
{
    if (seconds >= 0)
        minimumTimeBetweeenDataRecordsRemoval = seconds;
}

void ResourceLoadStatisticsStore::setGrandfatheringTime(double seconds)
{
    if (seconds >= 0)
        grandfatheringTime = seconds;
}

void ResourceLoadStatisticsStore::processStatistics(WTF::Function<void(ResourceLoadStatistics&)>&& processFunction)
{
    ASSERT(!isMainThread());
    auto locker = holdLock(m_statisticsLock);
    for (auto& resourceStatistic : m_resourceStatisticsMap.values())
        processFunction(resourceStatistic);
}

bool ResourceLoadStatisticsStore::hasHadRecentUserInteraction(ResourceLoadStatistics& resourceStatistic) const
{
    if (!resourceStatistic.hadUserInteraction)
        return false;

    if (currentTime() > resourceStatistic.mostRecentUserInteraction + timeToLiveUserInteraction) {
        // Drop privacy sensitive data because we no longer need it.
        // Set timestamp to 0.0 so that statistics merge will know
        // it has been reset as opposed to its default -1.
        resourceStatistic.mostRecentUserInteraction = 0;
        resourceStatistic.hadUserInteraction = false;

        return false;
    }

    return true;
}

Vector<String> ResourceLoadStatisticsStore::topPrivatelyControlledDomainsToRemoveWebsiteDataFor()
{
    bool shouldCheckForGrandfathering = m_endOfGrandfatheringTimestamp > currentTime();
    bool shouldClearGrandfathering = !shouldCheckForGrandfathering && m_endOfGrandfatheringTimestamp;

    if (shouldClearGrandfathering)
        m_endOfGrandfatheringTimestamp = 0;

    Vector<String> prevalentResources;
    auto locker = holdLock(m_statisticsLock);
    for (auto& statistic : m_resourceStatisticsMap.values()) {
        if (statistic.isPrevalentResource
            && !hasHadRecentUserInteraction(statistic)
            && (!shouldCheckForGrandfathering || !statistic.grandfathered))
            prevalentResources.append(statistic.highLevelDomain);

        if (shouldClearGrandfathering && statistic.grandfathered)
            statistic.grandfathered = false;
    }

    return prevalentResources;
}
    
Vector<PrevalentResourceTelemetry> ResourceLoadStatisticsStore::sortedPrevalentResourceTelemetry() const
{
    auto locker = holdLock(m_statisticsLock);
    Vector<PrevalentResourceTelemetry> sorted;
    
    for (auto& statistic : m_resourceStatisticsMap.values()) {
        if (statistic.isPrevalentResource) {
            unsigned daysSinceUserInteraction = statistic.mostRecentUserInteraction <= 0 ? 0 :
                std::floor((currentTime() - statistic.mostRecentUserInteraction) / secondsPerDay);
            sorted.append(PrevalentResourceTelemetry(
                statistic.dataRecordsRemoved,
                statistic.hadUserInteraction,
                daysSinceUserInteraction,
                statistic.subframeUnderTopFrameOrigins.size(),
                statistic.subresourceUnderTopFrameOrigins.size(),
                statistic.subresourceUniqueRedirectsTo.size()
            ));
        }
    }
    
    if (sorted.size() < minimumPrevalentResourcesForTelemetry)
        return Vector<PrevalentResourceTelemetry>();

    std::sort(sorted.begin(), sorted.end(), [](const PrevalentResourceTelemetry& a, const PrevalentResourceTelemetry& b) {
        return a.subframeUnderTopFrameOrigins + a.subresourceUnderTopFrameOrigins + a.subresourceUniqueRedirectsTo >
        b.subframeUnderTopFrameOrigins + b.subresourceUnderTopFrameOrigins + b.subresourceUniqueRedirectsTo;
    });
    
    return sorted;
}

void ResourceLoadStatisticsStore::updateStatisticsForRemovedDataRecords(const HashSet<String>& prevalentResourceDomains)
{
    auto locker = holdLock(m_statisticsLock);
    for (auto& prevalentResourceDomain : prevalentResourceDomains) {
        ResourceLoadStatistics& statistic = ensureResourceStatisticsForPrimaryDomain(prevalentResourceDomain);
        ++statistic.dataRecordsRemoved;
    }
}

void ResourceLoadStatisticsStore::handleFreshStartWithEmptyOrNoStore(HashSet<String>&& topPrivatelyControlledDomainsToGrandfather)
{
    auto locker = holdLock(m_statisticsLock);
    for (auto& topPrivatelyControlledDomain : topPrivatelyControlledDomainsToGrandfather) {
        ResourceLoadStatistics& statistic = ensureResourceStatisticsForPrimaryDomain(topPrivatelyControlledDomain);
        statistic.grandfathered = true;
    }
    m_endOfGrandfatheringTimestamp = std::floor(currentTime()) + grandfatheringTime;
}

bool ResourceLoadStatisticsStore::shouldRemoveDataRecords() const
{
    ASSERT(!isMainThread());
    if (m_dataRecordsRemovalPending)
        return false;

    if (m_lastTimeDataRecordsWereRemoved && currentTime() < m_lastTimeDataRecordsWereRemoved + minimumTimeBetweeenDataRecordsRemoval)
        return false;

    return true;
}

void ResourceLoadStatisticsStore::dataRecordsBeingRemoved()
{
    ASSERT(!isMainThread());
    m_lastTimeDataRecordsWereRemoved = currentTime();
    m_dataRecordsRemovalPending = true;
}

void ResourceLoadStatisticsStore::dataRecordsWereRemoved()
{
    ASSERT(!isMainThread());
    m_dataRecordsRemovalPending = false;
}
    
WTF::RecursiveLockAdapter<Lock>& ResourceLoadStatisticsStore::statisticsLock()
{
    return m_statisticsLock;
}
    
}

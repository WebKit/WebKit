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

#include <WebCore/KeyedCoding.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/RunLoop.h>

namespace WebKit {

using namespace WebCore;

const unsigned minimumPrevalentResourcesForTelemetry = 3;
const unsigned operatingDatesWindow { 30 };
const unsigned statisticsModelVersion { 6 };

Ref<ResourceLoadStatisticsStore> ResourceLoadStatisticsStore::create()
{
    return adoptRef(*new ResourceLoadStatisticsStore);
}

bool ResourceLoadStatisticsStore::isPrevalentResource(const String& primaryDomain) const
{
    ASSERT(!RunLoop::isMain());
    auto mapEntry = m_resourceStatisticsMap.find(primaryDomain);
    if (mapEntry == m_resourceStatisticsMap.end())
        return false;

    return mapEntry->value.isPrevalentResource;
}

bool ResourceLoadStatisticsStore::isGrandFathered(const String& primaryDomain) const
{
    ASSERT(!RunLoop::isMain());
    auto mapEntry = m_resourceStatisticsMap.find(primaryDomain);
    if (mapEntry == m_resourceStatisticsMap.end())
        return false;

    return mapEntry->value.grandfathered;
}
    
ResourceLoadStatistics& ResourceLoadStatisticsStore::ensureResourceStatisticsForPrimaryDomain(const String& primaryDomain)
{
    ASSERT(!RunLoop::isMain());
    return m_resourceStatisticsMap.ensure(primaryDomain, [&primaryDomain] {
        return ResourceLoadStatistics(primaryDomain);
    }).iterator->value;
}

typedef HashMap<String, ResourceLoadStatistics>::KeyValuePairType StatisticsValue;

std::unique_ptr<KeyedEncoder> ResourceLoadStatisticsStore::createEncoderFromData() const
{
    ASSERT(!RunLoop::isMain());
    auto encoder = KeyedEncoder::encoder();
    encoder->encodeUInt32("version", statisticsModelVersion);
    encoder->encodeDouble("endOfGrandfatheringTimestamp", m_endOfGrandfatheringTimestamp.secondsSinceEpoch().value());
    
    encoder->encodeObjects("browsingStatistics", m_resourceStatisticsMap.begin(), m_resourceStatisticsMap.end(), [](KeyedEncoder& encoderInner, const StatisticsValue& origin) {
        origin.value.encode(encoderInner);
    });

    encoder->encodeObjects("operatingDates", m_operatingDates.begin(), m_operatingDates.end(), [](KeyedEncoder& encoderInner, WallTime date) {
        encoderInner.encodeDouble("date", date.secondsSinceEpoch().value());
    });
    
    return encoder;
}

void ResourceLoadStatisticsStore::readDataFromDecoder(KeyedDecoder& decoder)
{
    ASSERT(!RunLoop::isMain());
    if (!m_resourceStatisticsMap.isEmpty())
        return;

    unsigned versionOnDisk;
    if (!decoder.decodeUInt32("version", versionOnDisk))
        return;

    if (versionOnDisk != statisticsModelVersion)
        return;

    double endOfGrandfatheringTimestamp;
    if (decoder.decodeDouble("endOfGrandfatheringTimestamp", endOfGrandfatheringTimestamp))
        m_endOfGrandfatheringTimestamp = WallTime::fromRawSeconds(endOfGrandfatheringTimestamp);
    else
        m_endOfGrandfatheringTimestamp = { };

    Vector<ResourceLoadStatistics> loadedStatistics;
    bool succeeded = decoder.decodeObjects("browsingStatistics", loadedStatistics, [](KeyedDecoder& decoderInner, ResourceLoadStatistics& statistics) {
        return statistics.decode(decoderInner);
    });

    if (!succeeded)
        return;

    Vector<String> prevalentResourceDomainsWithoutUserInteraction;
    prevalentResourceDomainsWithoutUserInteraction.reserveInitialCapacity(loadedStatistics.size());
    for (auto& statistics : loadedStatistics) {
        if (statistics.isPrevalentResource && !statistics.hadUserInteraction) {
            prevalentResourceDomainsWithoutUserInteraction.uncheckedAppend(statistics.highLevelDomain);
            statistics.isMarkedForCookiePartitioning = true;
        }
        m_resourceStatisticsMap.add(statistics.highLevelDomain, WTFMove(statistics));
    }

    succeeded = decoder.decodeObjects("operatingDates", m_operatingDates, [](KeyedDecoder& decoder, WallTime& wallTime) {
        double value;
        if (!decoder.decodeDouble("date", value))
            return false;
        
        wallTime = WallTime::fromRawSeconds(value);
        return true;
    });

    if (!succeeded)
        return;
    
    fireShouldPartitionCookiesHandler({ }, prevalentResourceDomainsWithoutUserInteraction, true);
}

void ResourceLoadStatisticsStore::clearInMemory()
{
    ASSERT(!RunLoop::isMain());
    m_resourceStatisticsMap.clear();
    m_operatingDates.clear();

    fireShouldPartitionCookiesHandler({ }, { }, true);
}

void ResourceLoadStatisticsStore::mergeStatistics(Vector<ResourceLoadStatistics>&& statistics)
{
    ASSERT(!RunLoop::isMain());
    for (auto& statistic : statistics) {
        // FIXME: In the case where the statistics does not already exist, it seems inefficient to create
        // an empty one just to merge the new one into it.
        auto result = m_resourceStatisticsMap.ensure(statistic.highLevelDomain, [&statistic] {
            return ResourceLoadStatistics(statistic.highLevelDomain);
        });
        
        result.iterator->value.merge(statistic);
    }
}

void ResourceLoadStatisticsStore::setShouldPartitionCookiesCallback(WTF::Function<void(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, bool clearFirst)>&& handler)
{
    m_shouldPartitionCookiesForDomainsHandler = WTFMove(handler);
}

inline bool ResourceLoadStatisticsStore::shouldPartitionCookies(const ResourceLoadStatistics& statistic) const
{
    return statistic.isPrevalentResource && (!statistic.hadUserInteraction || WallTime::now() > statistic.mostRecentUserInteractionTime + m_timeToLiveCookiePartitionFree);
}

void ResourceLoadStatisticsStore::fireShouldPartitionCookiesHandler()
{
    ASSERT(!RunLoop::isMain());
    Vector<String> domainsToRemove;
    Vector<String> domainsToAdd;
    
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
    ASSERT(!RunLoop::isMain());
    if (domainsToRemove.isEmpty() && domainsToAdd.isEmpty())
        return;
    
    RunLoop::main().dispatch([this, clearFirst, protectedThis = makeRef(*this), domainsToRemove = CrossThreadCopier<Vector<String>>::copy(domainsToRemove), domainsToAdd = CrossThreadCopier<Vector<String>>::copy(domainsToAdd)] () {
        if (m_shouldPartitionCookiesForDomainsHandler)
            m_shouldPartitionCookiesForDomainsHandler(domainsToRemove, domainsToAdd, clearFirst);
    });
    
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

void ResourceLoadStatisticsStore::setTimeToLiveUserInteraction(std::optional<Seconds> seconds)
{
    ASSERT(!seconds || seconds.value() >= 0_s);
    m_timeToLiveUserInteraction = seconds;
}

void ResourceLoadStatisticsStore::setTimeToLiveCookiePartitionFree(Seconds seconds)
{
    ASSERT(seconds >= 0_s);
    m_timeToLiveCookiePartitionFree = seconds;
}

void ResourceLoadStatisticsStore::setGrandfatheringTime(Seconds seconds)
{
    ASSERT(seconds >= 0_s);
    m_grandfatheringTime = seconds;
}

void ResourceLoadStatisticsStore::processStatistics(const WTF::Function<void(ResourceLoadStatistics&)>& processFunction)
{
    ASSERT(!RunLoop::isMain());
    for (auto& resourceStatistic : m_resourceStatisticsMap.values())
        processFunction(resourceStatistic);
}

bool ResourceLoadStatisticsStore::hasHadRecentUserInteraction(ResourceLoadStatistics& resourceStatistic) const
{
    if (!resourceStatistic.hadUserInteraction)
        return false;

    if (hasStatisticsExpired(resourceStatistic)) {
        // Drop privacy sensitive data because we no longer need it.
        // Set timestamp to 0 so that statistics merge will know
        // it has been reset as opposed to its default -1.
        resourceStatistic.mostRecentUserInteractionTime = { };
        resourceStatistic.hadUserInteraction = false;

        return false;
    }

    return true;
}

Vector<String> ResourceLoadStatisticsStore::topPrivatelyControlledDomainsToRemoveWebsiteDataFor()
{
    ASSERT(!RunLoop::isMain());

    bool shouldCheckForGrandfathering = m_endOfGrandfatheringTimestamp > WallTime::now();
    bool shouldClearGrandfathering = !shouldCheckForGrandfathering && m_endOfGrandfatheringTimestamp;

    if (shouldClearGrandfathering)
        m_endOfGrandfatheringTimestamp = { };

    Vector<String> prevalentResources;
    for (auto& statistic : m_resourceStatisticsMap.values()) {
        if (statistic.isPrevalentResource && !hasHadRecentUserInteraction(statistic) && (!shouldCheckForGrandfathering || !statistic.grandfathered))
            prevalentResources.append(statistic.highLevelDomain);

        if (shouldClearGrandfathering && statistic.grandfathered)
            statistic.grandfathered = false;
    }

    return prevalentResources;
}
    
Vector<PrevalentResourceTelemetry> ResourceLoadStatisticsStore::sortedPrevalentResourceTelemetry() const
{
    ASSERT(!RunLoop::isMain());
    Vector<PrevalentResourceTelemetry> sorted;
    for (auto& statistic : m_resourceStatisticsMap.values()) {
        if (!statistic.isPrevalentResource)
            continue;

        unsigned daysSinceUserInteraction = statistic.mostRecentUserInteractionTime <= WallTime() ? 0 : std::floor((WallTime::now() - statistic.mostRecentUserInteractionTime) / 24_h);
        sorted.append(PrevalentResourceTelemetry {
            statistic.dataRecordsRemoved,
            statistic.hadUserInteraction,
            daysSinceUserInteraction,
            statistic.subframeUnderTopFrameOrigins.size(),
            statistic.subresourceUnderTopFrameOrigins.size(),
            statistic.subresourceUniqueRedirectsTo.size()
        });
    }
    
    if (sorted.size() < minimumPrevalentResourcesForTelemetry)
        return { };

    std::sort(sorted.begin(), sorted.end(), [](const PrevalentResourceTelemetry& a, const PrevalentResourceTelemetry& b) {
        return a.subframeUnderTopFrameOrigins + a.subresourceUnderTopFrameOrigins + a.subresourceUniqueRedirectsTo >
        b.subframeUnderTopFrameOrigins + b.subresourceUnderTopFrameOrigins + b.subresourceUniqueRedirectsTo;
    });
    
    return sorted;
}

void ResourceLoadStatisticsStore::updateStatisticsForRemovedDataRecords(const HashSet<String>& prevalentResourceDomains)
{
    ASSERT(!RunLoop::isMain());
    for (auto& prevalentResourceDomain : prevalentResourceDomains) {
        auto& statistic = ensureResourceStatisticsForPrimaryDomain(prevalentResourceDomain);
        ++statistic.dataRecordsRemoved;
    }
}

void ResourceLoadStatisticsStore::handleFreshStartWithEmptyOrNoStore(HashSet<String>&& topPrivatelyControlledDomainsToGrandfather)
{
    ASSERT(!RunLoop::isMain());
    for (auto& topPrivatelyControlledDomain : topPrivatelyControlledDomainsToGrandfather) {
        auto& statistic = ensureResourceStatisticsForPrimaryDomain(topPrivatelyControlledDomain);
        statistic.grandfathered = true;
    }
    m_endOfGrandfatheringTimestamp = WallTime::now() + m_grandfatheringTime;
}

void ResourceLoadStatisticsStore::includeTodayAsOperatingDateIfNecessary()
{
    if (!m_operatingDates.isEmpty() && (WallTime::now() - m_operatingDates.last() < 24_h))
        return;

    while (m_operatingDates.size() >= operatingDatesWindow)
        m_operatingDates.removeFirst();

    m_operatingDates.append(WallTime::now());
}
    
bool ResourceLoadStatisticsStore::hasStatisticsExpired(const ResourceLoadStatistics& resourceStatistic) const
{
    if (m_operatingDates.size() >= operatingDatesWindow) {
        if (resourceStatistic.mostRecentUserInteractionTime < m_operatingDates.first())
            return true;
    }

    // If we don't meet the real criteria for an expired statistic, check the user
    // setting for a tighter restriction (mainly for testing).
    if (m_timeToLiveUserInteraction) {
        if (WallTime::now() > resourceStatistic.mostRecentUserInteractionTime + m_timeToLiveUserInteraction.value())
            return true;
    }
    
    return false;
}
    
}

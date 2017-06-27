/*
 * Copyright (C) 2016-2017 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ResourceLoadStatistics.h"
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/RecursiveLockAdapter.h>

namespace WebCore {

class KeyedDecoder;
class KeyedEncoder;
class URL;

static const auto minimumPrevalentResourcesForTelemetry = 3;
    
struct PrevalentResourceTelemetry {
    unsigned numberOfTimesDataRecordsRemoved;
    bool hasHadUserInteraction;
    unsigned daysSinceUserInteraction;
    unsigned subframeUnderTopFrameOrigins;
    unsigned subresourceUnderTopFrameOrigins;
    unsigned subresourceUniqueRedirectsTo;
    PrevalentResourceTelemetry()
        : numberOfTimesDataRecordsRemoved(0), hasHadUserInteraction(0), daysSinceUserInteraction(0), subframeUnderTopFrameOrigins(0), subresourceUnderTopFrameOrigins(0), subresourceUniqueRedirectsTo(0)
    {
    }
    PrevalentResourceTelemetry(unsigned recordsRemoved, bool userInteraction, unsigned daysSince, unsigned subframe, unsigned subresource, unsigned uniqueRedirects)
        : numberOfTimesDataRecordsRemoved(recordsRemoved), hasHadUserInteraction(userInteraction), daysSinceUserInteraction(daysSince), subframeUnderTopFrameOrigins(subframe), subresourceUnderTopFrameOrigins(subresource), subresourceUniqueRedirectsTo(uniqueRedirects)
    {
    }
};
    
struct ResourceLoadStatistics;

class ResourceLoadStatisticsStore : public ThreadSafeRefCounted<ResourceLoadStatisticsStore> {
public:
    WEBCORE_EXPORT static Ref<ResourceLoadStatisticsStore> create();

    WEBCORE_EXPORT std::unique_ptr<KeyedEncoder> createEncoderFromData();
    WEBCORE_EXPORT void readDataFromDecoder(KeyedDecoder&);

    WEBCORE_EXPORT String statisticsForOrigin(const String&);

    bool isEmpty() const { return m_resourceStatisticsMap.isEmpty(); }
    size_t size() const { return m_resourceStatisticsMap.size(); }
    WEBCORE_EXPORT void clearInMemory();
    void clearInMemoryAndPersistent();

    ResourceLoadStatistics& ensureResourceStatisticsForPrimaryDomain(const String&);
    void setResourceStatisticsForPrimaryDomain(const String&, ResourceLoadStatistics&&);

    bool isPrevalentResource(const String&) const;
    
    WEBCORE_EXPORT void mergeStatistics(const Vector<ResourceLoadStatistics>&);
    WEBCORE_EXPORT Vector<ResourceLoadStatistics> takeStatistics();

    WEBCORE_EXPORT void setNotificationCallback(WTF::Function<void()>&&);
    WEBCORE_EXPORT void setShouldPartitionCookiesCallback(WTF::Function<void(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, bool clearFirst)>&&);
    WEBCORE_EXPORT void setWritePersistentStoreCallback(WTF::Function<void()>&&);
    WEBCORE_EXPORT void setGrandfatherExistingWebsiteDataCallback(WTF::Function<void()>&&);
    WEBCORE_EXPORT void setFireTelemetryCallback(WTF::Function<void()>&& handler);

    void fireDataModificationHandler();
    void fireTelemetryHandler();
    void setTimeToLiveUserInteraction(double seconds);
    void setTimeToLiveCookiePartitionFree(double seconds);
    void setMinimumTimeBetweeenDataRecordsRemoval(double seconds);
    void setGrandfatheringTime(double seconds);    
    WEBCORE_EXPORT void fireShouldPartitionCookiesHandler();
    void fireShouldPartitionCookiesHandler(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, bool clearFirst);

    WEBCORE_EXPORT void processStatistics(WTF::Function<void(ResourceLoadStatistics&)>&&);

    WEBCORE_EXPORT bool hasHadRecentUserInteraction(ResourceLoadStatistics&) const;
    WEBCORE_EXPORT Vector<String> topPrivatelyControlledDomainsToRemoveWebsiteDataFor();
    WEBCORE_EXPORT Vector<PrevalentResourceTelemetry> sortedPrevalentResourceTelemetry() const;
    WEBCORE_EXPORT void updateStatisticsForRemovedDataRecords(const HashSet<String>& prevalentResourceDomains);

    WEBCORE_EXPORT void handleFreshStartWithEmptyOrNoStore(HashSet<String>&& topPrivatelyControlledDomainsToGrandfather);
    WEBCORE_EXPORT bool shouldRemoveDataRecords() const;
    WEBCORE_EXPORT void dataRecordsBeingRemoved();
    WEBCORE_EXPORT void dataRecordsWereRemoved();
    
    WEBCORE_EXPORT WTF::RecursiveLockAdapter<Lock>& statisticsLock();
private:
    ResourceLoadStatisticsStore() = default;

    HashMap<String, ResourceLoadStatistics> m_resourceStatisticsMap;
    mutable WTF::RecursiveLockAdapter<Lock> m_statisticsLock;

    WTF::Function<void()> m_dataAddedHandler;
    WTF::Function<void(const Vector<String>&, const Vector<String>&, bool clearFirst)> m_shouldPartitionCookiesForDomainsHandler;
    WTF::Function<void()> m_writePersistentStoreHandler;
    WTF::Function<void()> m_grandfatherExistingWebsiteDataHandler;
    WTF::Function<void()> m_fireTelemetryHandler;

    double m_endOfGrandfatheringTimestamp { 0 };
    double m_lastTimeDataRecordsWereRemoved { 0 };
    bool m_dataRecordsRemovalPending { false };
};
    
} // namespace WebCore

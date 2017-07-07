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

#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/MonotonicTime.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WallTime.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class KeyedDecoder;
class KeyedEncoder;
struct ResourceLoadStatistics;
}

namespace WebKit {

static const auto minimumPrevalentResourcesForTelemetry = 3;

struct PrevalentResourceTelemetry {
    unsigned numberOfTimesDataRecordsRemoved;
    bool hasHadUserInteraction;
    unsigned daysSinceUserInteraction;
    unsigned subframeUnderTopFrameOrigins;
    unsigned subresourceUnderTopFrameOrigins;
    unsigned subresourceUniqueRedirectsTo;
};

// FIXME: We should probably consider merging this with WebResourceLoadStatisticsStore.
class ResourceLoadStatisticsStore : public ThreadSafeRefCounted<ResourceLoadStatisticsStore> {
public:
    static Ref<ResourceLoadStatisticsStore> create();

    std::unique_ptr<WebCore::KeyedEncoder> createEncoderFromData();
    void readDataFromDecoder(WebCore::KeyedDecoder&);

    bool isEmpty() const { return m_resourceStatisticsMap.isEmpty(); }
    size_t size() const { return m_resourceStatisticsMap.size(); }
    void clearInMemory();
    void clearInMemoryAndPersistent();

    WebCore::ResourceLoadStatistics& ensureResourceStatisticsForPrimaryDomain(const String&);
    void setResourceStatisticsForPrimaryDomain(const String&, WebCore::ResourceLoadStatistics&&);

    bool isPrevalentResource(const String&) const;
    bool isGrandFathered(const String&) const;
    
    void mergeStatistics(const Vector<WebCore::ResourceLoadStatistics>&);

    void setNotificationCallback(WTF::Function<void()>&&);
    void setShouldPartitionCookiesCallback(WTF::Function<void(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, bool clearFirst)>&&);
    void setDeletePersistentStoreCallback(WTF::Function<void()>&&);
    void setGrandfatherExistingWebsiteDataCallback(WTF::Function<void()>&&);
    void setFireTelemetryCallback(WTF::Function<void()>&& handler);

    void fireDataModificationHandler();
    void fireTelemetryHandler();
    void setTimeToLiveUserInteraction(Seconds);
    void setTimeToLiveCookiePartitionFree(Seconds);
    void setMinimumTimeBetweenDataRecordsRemoval(Seconds);
    void setGrandfatheringTime(Seconds);
    void fireShouldPartitionCookiesHandler();
    void fireShouldPartitionCookiesHandler(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, bool clearFirst);

    void processStatistics(WTF::Function<void (WebCore::ResourceLoadStatistics&)>&&);

    bool hasHadRecentUserInteraction(WebCore::ResourceLoadStatistics&) const;
    Vector<String> topPrivatelyControlledDomainsToRemoveWebsiteDataFor();
    Vector<PrevalentResourceTelemetry> sortedPrevalentResourceTelemetry() const;
    void updateStatisticsForRemovedDataRecords(const HashSet<String>& prevalentResourceDomains);

    void handleFreshStartWithEmptyOrNoStore(HashSet<String>&& topPrivatelyControlledDomainsToGrandfather);
    bool shouldRemoveDataRecords() const;
    void dataRecordsBeingRemoved();
    void dataRecordsWereRemoved();

private:
    ResourceLoadStatisticsStore() = default;

    bool shouldPartitionCookies(const WebCore::ResourceLoadStatistics&) const;

    HashMap<String, WebCore::ResourceLoadStatistics> m_resourceStatisticsMap;

    WTF::Function<void()> m_dataAddedHandler;
    WTF::Function<void(const Vector<String>&, const Vector<String>&, bool clearFirst)> m_shouldPartitionCookiesForDomainsHandler;
    WTF::Function<void()> m_grandfatherExistingWebsiteDataHandler;
    WTF::Function<void()> m_deletePersistentStoreHandler;
    WTF::Function<void()> m_fireTelemetryHandler;

    Seconds m_timeToLiveUserInteraction { 24_h * 30. };
    Seconds m_timeToLiveCookiePartitionFree { 24_h };
    Seconds m_grandfatheringTime { 1_h };
    Seconds m_minimumTimeBetweenDataRecordsRemoval { 1_h };

    WallTime m_endOfGrandfatheringTimestamp;
    MonotonicTime m_lastTimeDataRecordsWereRemoved;
    bool m_dataRecordsRemovalPending { false };
};
    
} // namespace WebKit

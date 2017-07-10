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

#include <wtf/Deque.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/MonotonicTime.h>
#include <wtf/WallTime.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class KeyedDecoder;
class KeyedEncoder;
struct ResourceLoadStatistics;
}

namespace WebKit {

enum class ShouldClearFirst;

// FIXME: We should probably consider merging this with WebResourceLoadStatisticsStore.
class ResourceLoadStatisticsStore : public ThreadSafeRefCounted<ResourceLoadStatisticsStore> {
public:
    using UpdateCookiePartitioningForDomainsHandler = WTF::Function<void(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, ShouldClearFirst)>;
    static Ref<ResourceLoadStatisticsStore> create(UpdateCookiePartitioningForDomainsHandler&&);

    std::unique_ptr<WebCore::KeyedEncoder> createEncoderFromData() const;
    void populateFromDecoder(WebCore::KeyedDecoder&);

    bool isEmpty() const { return m_resourceStatisticsMap.isEmpty(); }
    void clearInMemory();

    WebCore::ResourceLoadStatistics& ensureResourceStatisticsForPrimaryDomain(const String&);

    bool isPrevalentResource(const String&) const;
    bool isGrandFathered(const String&) const;
    bool hasHadRecentUserInteraction(const String&);
    
    void mergeStatistics(Vector<WebCore::ResourceLoadStatistics>&&);

    void setTimeToLiveUserInteraction(std::optional<Seconds>);
    void setTimeToLiveCookiePartitionFree(Seconds);
    void setGrandfatheringTime(Seconds);

    void updateCookiePartitioning();
    void updateCookiePartitioningForDomains(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, ShouldClearFirst);

    void processStatistics(const WTF::Function<void (WebCore::ResourceLoadStatistics&)>&);
    void processStatistics(const WTF::Function<void (const WebCore::ResourceLoadStatistics&)>&) const;

    Vector<String> topPrivatelyControlledDomainsToRemoveWebsiteDataFor();
    void updateStatisticsForRemovedDataRecords(const HashSet<String>& prevalentResourceDomains);

    void handleFreshStartWithEmptyOrNoStore(HashSet<String>&& topPrivatelyControlledDomainsToGrandfather);
    void includeTodayAsOperatingDateIfNecessary();

private:
    explicit ResourceLoadStatisticsStore(UpdateCookiePartitioningForDomainsHandler&&);

    bool shouldPartitionCookies(const WebCore::ResourceLoadStatistics&) const;
    bool hasStatisticsExpired(const WebCore::ResourceLoadStatistics&) const;
    bool hasHadUnexpiredRecentUserInteraction(WebCore::ResourceLoadStatistics&) const;

    HashMap<String, WebCore::ResourceLoadStatistics> m_resourceStatisticsMap;
    Deque<WTF::WallTime> m_operatingDates;

    UpdateCookiePartitioningForDomainsHandler m_updateCookiePartitioningForDomainsHandler;

    std::optional<Seconds> m_timeToLiveUserInteraction;
    Seconds m_timeToLiveCookiePartitionFree { 24_h };
    Seconds m_grandfatheringTime { 1_h };

    WallTime m_endOfGrandfatheringTimestamp;
};
    
} // namespace WebKit

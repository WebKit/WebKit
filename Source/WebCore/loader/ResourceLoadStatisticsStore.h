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
#include <wtf/HashSet.h>

namespace WebCore {

class KeyedDecoder;
class KeyedEncoder;
class URL;

struct ResourceLoadStatistics;

class ResourceLoadStatisticsStore : public RefCounted<ResourceLoadStatisticsStore> {
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

    WEBCORE_EXPORT void setNotificationCallback(std::function<void()>);
    WEBCORE_EXPORT void setShouldPartitionCookiesCallback(std::function<void(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, bool clearFirst)>&&);
    WEBCORE_EXPORT void setWritePersistentStoreCallback(std::function<void()>&&);
    WEBCORE_EXPORT void setGrandfatherExistingWebsiteDataCallback(std::function<void()>&&);

    void fireDataModificationHandler();
    void setTimeToLiveUserInteraction(double seconds);
    void setTimeToLiveCookiePartitionFree(double seconds);
    void setMinimumTimeBetweeenDataRecordsRemoval(double seconds);
    void setGrandfatheringTime(double seconds);    
    WEBCORE_EXPORT void fireShouldPartitionCookiesHandler();
    void fireShouldPartitionCookiesHandler(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, bool clearFirst);

    WEBCORE_EXPORT void processStatistics(std::function<void(ResourceLoadStatistics&)>&&);

    WEBCORE_EXPORT bool hasHadRecentUserInteraction(ResourceLoadStatistics&);
    WEBCORE_EXPORT Vector<String> topPrivatelyControlledDomainsToRemoveWebsiteDataFor();
    WEBCORE_EXPORT void updateStatisticsForRemovedDataRecords(const Vector<String>& prevalentResourceDomains);

    WEBCORE_EXPORT void handleFreshStartWithEmptyOrNoStore(HashSet<String>&& topPrivatelyControlledDomainsToGrandfather);
    WEBCORE_EXPORT bool shouldRemoveDataRecords();
    WEBCORE_EXPORT void dataRecordsBeingRemoved();
    WEBCORE_EXPORT void dataRecordsWereRemoved();
private:
    ResourceLoadStatisticsStore() = default;

    HashMap<String, ResourceLoadStatistics> m_resourceStatisticsMap;
    std::function<void()> m_dataAddedHandler;
    std::function<void(const Vector<String>&, const Vector<String>&, bool clearFirst)> m_shouldPartitionCookiesForDomainsHandler;
    std::function<void()> m_writePersistentStoreHandler;
    std::function<void()> m_grandfatherExistingWebsiteDataHandler;

    double m_endOfGrandfatheringTimestamp { 0 };
    double m_lastTimeDataRecordsWereRemoved { 0 };
    bool m_dataRecordsRemovalPending { false };
};
    
} // namespace WebCore

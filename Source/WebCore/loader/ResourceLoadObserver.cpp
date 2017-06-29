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
#include "ResourceLoadObserver.h"

#include "Document.h"
#include "Frame.h"
#include "Logging.h"
#include "MainFrame.h"
#include "NetworkStorageSession.h"
#include "Page.h"
#include "PlatformStrategies.h"
#include "PublicSuffix.h"
#include "ResourceLoadStatistics.h"
#include "ResourceLoadStatisticsStore.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "URL.h"
#include <wtf/CrossThreadCopier.h>
#include <wtf/CurrentTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static Seconds timestampResolution { 1_h };

ResourceLoadObserver& ResourceLoadObserver::sharedObserver()
{
    static NeverDestroyed<ResourceLoadObserver> resourceLoadObserver;
    return resourceLoadObserver;
}

void ResourceLoadObserver::setStatisticsStore(Ref<ResourceLoadStatisticsStore>&& store)
{
    if (m_store && m_queue)
        m_queue = nullptr;
    m_store = WTFMove(store);
}
    
void ResourceLoadObserver::setStatisticsQueue(Ref<WTF::WorkQueue>&& queue)
{
    ASSERT(!m_queue);
    m_queue = WTFMove(queue);
}
    
void ResourceLoadObserver::clearInMemoryStore()
{
    if (!m_store)
        return;
    
    ASSERT(m_queue);
    m_queue->dispatch([this] {
        m_store->clearInMemory();
    });
}
    
void ResourceLoadObserver::clearInMemoryAndPersistentStore()
{
    if (!m_store)
        return;
    
    ASSERT(m_queue);
    m_queue->dispatch([this] {
        m_store->clearInMemoryAndPersistent();
    });
}

void ResourceLoadObserver::clearInMemoryAndPersistentStore(std::chrono::system_clock::time_point modifiedSince)
{
    // For now, be conservative and clear everything regardless of modifiedSince
    UNUSED_PARAM(modifiedSince);
    clearInMemoryAndPersistentStore();
}

static inline bool is3xxRedirect(const ResourceResponse& response)
{
    return response.httpStatusCode() >= 300 && response.httpStatusCode() <= 399;
}

bool ResourceLoadObserver::shouldLog(Page* page)
{
    // FIXME: Err on the safe side until we have sorted out what to do in worker contexts
    if (!page)
        return false;

    return Settings::resourceLoadStatisticsEnabled() && !page->usesEphemeralSession() && m_store;
}

void ResourceLoadObserver::logFrameNavigation(const Frame& frame, const Frame& topFrame, const ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    ASSERT(frame.document());
    ASSERT(topFrame.document());
    ASSERT(topFrame.page());
    
    if (!shouldLog(topFrame.page()))
        return;

    bool isRedirect = is3xxRedirect(redirectResponse);
    bool isMainFrame = frame.isMainFrame();
    auto& sourceURL = frame.document()->url();
    auto& targetURL = newRequest.url();
    auto& mainFrameURL = topFrame.document()->url();
    
    if (!targetURL.isValid() || !mainFrameURL.isValid())
        return;

    auto targetHost = targetURL.host();
    auto mainFrameHost = mainFrameURL.host();

    if (targetHost.isEmpty() || mainFrameHost.isEmpty() || targetHost == mainFrameHost || targetHost == sourceURL.host())
        return;

    auto targetPrimaryDomain = primaryDomain(targetURL);
    auto mainFramePrimaryDomain = primaryDomain(mainFrameURL);
    auto sourcePrimaryDomain = primaryDomain(sourceURL);
    
    if (targetPrimaryDomain == mainFramePrimaryDomain || targetPrimaryDomain == sourcePrimaryDomain)
        return;
    
    ASSERT(m_queue);
    m_queue->dispatch([this, isMainFrame, isRedirect, sourcePrimaryDomain = sourcePrimaryDomain.isolatedCopy(), mainFramePrimaryDomain = mainFramePrimaryDomain.isolatedCopy(), targetPrimaryDomain = targetPrimaryDomain.isolatedCopy()] {
        bool shouldFireDataModificationHandler = false;
        
        {
            auto locker = holdLock(m_store->statisticsLock());
            ResourceLoadStatistics targetStatistics(targetPrimaryDomain);

            // Always fire if we have previously removed data records for this domain
            shouldFireDataModificationHandler = targetStatistics.dataRecordsRemoved > 0;

            if (isMainFrame)
                targetStatistics.topFrameHasBeenNavigatedToBefore = true;
            else {
                targetStatistics.subframeHasBeenLoadedBefore = true;

                auto subframeUnderTopFrameOriginsResult = targetStatistics.subframeUnderTopFrameOrigins.add(mainFramePrimaryDomain);
                if (subframeUnderTopFrameOriginsResult.isNewEntry)
                    shouldFireDataModificationHandler = true;
            }

            if (isRedirect) {
                auto& redirectingOriginResourceStatistics = m_store->ensureResourceStatisticsForPrimaryDomain(sourcePrimaryDomain);

                if (m_store->isPrevalentResource(targetPrimaryDomain))
                    redirectingOriginResourceStatistics.redirectedToOtherPrevalentResourceOrigins.add(targetPrimaryDomain);

                if (isMainFrame) {
                    ++targetStatistics.topFrameHasBeenRedirectedTo;
                    ++redirectingOriginResourceStatistics.topFrameHasBeenRedirectedFrom;
                } else {
                    ++targetStatistics.subframeHasBeenRedirectedTo;
                    ++redirectingOriginResourceStatistics.subframeHasBeenRedirectedFrom;
                    redirectingOriginResourceStatistics.subframeUniqueRedirectsTo.add(targetPrimaryDomain);

                    ++targetStatistics.subframeSubResourceCount;
                }
            } else {
                if (sourcePrimaryDomain.isNull() || sourcePrimaryDomain.isEmpty() || sourcePrimaryDomain == "nullOrigin") {
                    if (isMainFrame)
                        ++targetStatistics.topFrameInitialLoadCount;
                    else
                        ++targetStatistics.subframeSubResourceCount;
                } else {
                    auto& sourceOriginResourceStatistics = m_store->ensureResourceStatisticsForPrimaryDomain(sourcePrimaryDomain);

                    if (isMainFrame) {
                        ++sourceOriginResourceStatistics.topFrameHasBeenNavigatedFrom;
                        ++targetStatistics.topFrameHasBeenNavigatedTo;
                    } else {
                        ++sourceOriginResourceStatistics.subframeHasBeenNavigatedFrom;
                        ++targetStatistics.subframeHasBeenNavigatedTo;
                    }
                }
            }

            m_store->setResourceStatisticsForPrimaryDomain(targetPrimaryDomain, WTFMove(targetStatistics));
        } // Release lock
        
        if (shouldFireDataModificationHandler)
            m_store->fireDataModificationHandler();
    });
}
    
void ResourceLoadObserver::logSubresourceLoading(const Frame* frame, const ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    ASSERT(frame->page());

    if (!shouldLog(frame->page()))
        return;

    bool isRedirect = is3xxRedirect(redirectResponse);
    const URL& sourceURL = redirectResponse.url();
    const URL& targetURL = newRequest.url();
    const URL& mainFrameURL = frame ? frame->mainFrame().document()->url() : URL();
    
    auto targetHost = targetURL.host();
    auto mainFrameHost = mainFrameURL.host();

    if (targetHost.isEmpty() || mainFrameHost.isEmpty() || targetHost == mainFrameHost || (isRedirect && targetHost == sourceURL.host()))
        return;

    auto targetPrimaryDomain = primaryDomain(targetURL);
    auto mainFramePrimaryDomain = primaryDomain(mainFrameURL);
    auto sourcePrimaryDomain = primaryDomain(sourceURL);
    
    if (targetPrimaryDomain == mainFramePrimaryDomain || (isRedirect && targetPrimaryDomain == sourcePrimaryDomain))
        return;
    
    ASSERT(m_queue);
    m_queue->dispatch([this, isRedirect, sourcePrimaryDomain = sourcePrimaryDomain.isolatedCopy(), mainFramePrimaryDomain = mainFramePrimaryDomain.isolatedCopy(), targetPrimaryDomain = targetPrimaryDomain.isolatedCopy()] {
        bool shouldFireDataModificationHandler = false;
        
        {
        auto locker = holdLock(m_store->statisticsLock());
        auto& targetStatistics = m_store->ensureResourceStatisticsForPrimaryDomain(targetPrimaryDomain);

        // Always fire if we have previously removed data records for this domain
        shouldFireDataModificationHandler = targetStatistics.dataRecordsRemoved > 0;

        auto subresourceUnderTopFrameOriginsResult = targetStatistics.subresourceUnderTopFrameOrigins.add(mainFramePrimaryDomain);
        if (subresourceUnderTopFrameOriginsResult.isNewEntry)
            shouldFireDataModificationHandler = true;

        if (isRedirect) {
            auto& redirectingOriginStatistics = m_store->ensureResourceStatisticsForPrimaryDomain(sourcePrimaryDomain);
            
            // We just inserted to the store, so we need to reget 'targetStatistics'
            auto& updatedTargetStatistics = m_store->ensureResourceStatisticsForPrimaryDomain(targetPrimaryDomain);

            if (m_store->isPrevalentResource(targetPrimaryDomain))
                redirectingOriginStatistics.redirectedToOtherPrevalentResourceOrigins.add(targetPrimaryDomain);
            
            ++redirectingOriginStatistics.subresourceHasBeenRedirectedFrom;
            ++updatedTargetStatistics.subresourceHasBeenRedirectedTo;

            auto subresourceUniqueRedirectsToResult = redirectingOriginStatistics.subresourceUniqueRedirectsTo.add(targetPrimaryDomain);
            if (subresourceUniqueRedirectsToResult.isNewEntry)
                shouldFireDataModificationHandler = true;

            ++updatedTargetStatistics.subresourceHasBeenSubresourceCount;

            auto totalVisited = std::max(m_originsVisitedMap.size(), 1U);
            
            updatedTargetStatistics.subresourceHasBeenSubresourceCountDividedByTotalNumberOfOriginsVisited = static_cast<double>(updatedTargetStatistics.subresourceHasBeenSubresourceCount) / totalVisited;
        } else {
            ++targetStatistics.subresourceHasBeenSubresourceCount;

            auto totalVisited = std::max(m_originsVisitedMap.size(), 1U);
            
            targetStatistics.subresourceHasBeenSubresourceCountDividedByTotalNumberOfOriginsVisited = static_cast<double>(targetStatistics.subresourceHasBeenSubresourceCount) / totalVisited;
        }
        } // Release lock
        
        if (shouldFireDataModificationHandler)
            m_store->fireDataModificationHandler();
    });
}

void ResourceLoadObserver::logWebSocketLoading(const Frame* frame, const URL& targetURL)
{
    // FIXME: Web sockets can run in detached frames. Decide how to count such connections.
    // See LayoutTests/http/tests/websocket/construct-in-detached-frame.html
    if (!frame)
        return;

    if (!shouldLog(frame->page()))
        return;

    auto& mainFrameURL = frame->mainFrame().document()->url();

    auto targetHost = targetURL.host();
    auto mainFrameHost = mainFrameURL.host();
    
    if (targetHost.isEmpty() || mainFrameHost.isEmpty() || targetHost == mainFrameHost)
        return;
    
    auto targetPrimaryDomain = primaryDomain(targetURL);
    auto mainFramePrimaryDomain = primaryDomain(mainFrameURL);
    
    if (targetPrimaryDomain == mainFramePrimaryDomain)
        return;

    ASSERT(m_queue);
    m_queue->dispatch([this, targetPrimaryDomain = targetPrimaryDomain.isolatedCopy(), mainFramePrimaryDomain = mainFramePrimaryDomain.isolatedCopy()] {
        bool shouldFireDataModificationHandler = false;
        
        {
        auto locker = holdLock(m_store->statisticsLock());
        auto& targetStatistics = m_store->ensureResourceStatisticsForPrimaryDomain(targetPrimaryDomain);

        // Always fire if we have previously removed data records for this domain
        shouldFireDataModificationHandler = targetStatistics.dataRecordsRemoved > 0;
        
        auto subresourceUnderTopFrameOriginsResult = targetStatistics.subresourceUnderTopFrameOrigins.add(mainFramePrimaryDomain);
        if (subresourceUnderTopFrameOriginsResult.isNewEntry)
            shouldFireDataModificationHandler = true;

        ++targetStatistics.subresourceHasBeenSubresourceCount;
        
        auto totalVisited = std::max(m_originsVisitedMap.size(), 1U);
        
        targetStatistics.subresourceHasBeenSubresourceCountDividedByTotalNumberOfOriginsVisited = static_cast<double>(targetStatistics.subresourceHasBeenSubresourceCount) / totalVisited;
        } // Release lock
        
        if (shouldFireDataModificationHandler)
            m_store->fireDataModificationHandler();
    });
}

static WallTime reduceTimeResolution(WallTime time)
{
    return WallTime::fromRawSeconds(std::floor(time.secondsSinceEpoch() / timestampResolution) * timestampResolution.seconds());
}

void ResourceLoadObserver::logUserInteractionWithReducedTimeResolution(const Document& document)
{
    ASSERT(document.page());

    if (!shouldLog(document.page()))
        return;

    auto& url = document.url();
    if (url.isBlankURL() || url.isEmpty())
        return;

    ASSERT(m_queue);
    m_queue->dispatch([this, primaryDomainString = primaryDomain(url).isolatedCopy()] {
        {
        auto locker = holdLock(m_store->statisticsLock());
        auto& statistics = m_store->ensureResourceStatisticsForPrimaryDomain(primaryDomainString);
        WallTime newTime = reduceTimeResolution(WallTime::now());
        if (newTime == statistics.mostRecentUserInteractionTime())
            return;

        statistics.hadUserInteraction = true;
        statistics.mostRecentUserInteraction = newTime.secondsSinceEpoch().value();
        }
        
        m_store->fireDataModificationHandler();
    });
}

void ResourceLoadObserver::logUserInteraction(const URL& url)
{
    if (url.isBlankURL() || url.isEmpty())
        return;

    ASSERT(m_queue);
    m_queue->dispatch([this, primaryDomainString = primaryDomain(url).isolatedCopy()] {
        {
        auto locker = holdLock(m_store->statisticsLock());
        auto& statistics = m_store->ensureResourceStatisticsForPrimaryDomain(primaryDomainString);
        statistics.hadUserInteraction = true;
        statistics.mostRecentUserInteraction = WTF::currentTime();
        }
        
        m_store->fireShouldPartitionCookiesHandler({ primaryDomainString }, { }, false);
    });
}

void ResourceLoadObserver::clearUserInteraction(const URL& url)
{
    if (url.isBlankURL() || url.isEmpty())
        return;

    auto locker = holdLock(m_store->statisticsLock());
    auto& statistics = m_store->ensureResourceStatisticsForPrimaryDomain(primaryDomain(url));
    
    statistics.hadUserInteraction = false;
    statistics.mostRecentUserInteraction = 0;
}

bool ResourceLoadObserver::hasHadUserInteraction(const URL& url)
{
    if (url.isBlankURL() || url.isEmpty())
        return false;

    auto locker = holdLock(m_store->statisticsLock());
    auto& statistics = m_store->ensureResourceStatisticsForPrimaryDomain(primaryDomain(url));
    
    return m_store->hasHadRecentUserInteraction(statistics);
}

void ResourceLoadObserver::setPrevalentResource(const URL& url)
{
    if (url.isBlankURL() || url.isEmpty())
        return;

    auto locker = holdLock(m_store->statisticsLock());
    auto& statistics = m_store->ensureResourceStatisticsForPrimaryDomain(primaryDomain(url));
    
    statistics.isPrevalentResource = true;
}

bool ResourceLoadObserver::isPrevalentResource(const URL& url)
{
    if (url.isBlankURL() || url.isEmpty())
        return false;

    auto locker = holdLock(m_store->statisticsLock());
    auto& statistics = m_store->ensureResourceStatisticsForPrimaryDomain(primaryDomain(url));
    
    return statistics.isPrevalentResource;
}
    
void ResourceLoadObserver::clearPrevalentResource(const URL& url)
{
    if (url.isBlankURL() || url.isEmpty())
        return;

    auto locker = holdLock(m_store->statisticsLock());
    auto& statistics = m_store->ensureResourceStatisticsForPrimaryDomain(primaryDomain(url));
    
    statistics.isPrevalentResource = false;
}
    
void ResourceLoadObserver::setGrandfathered(const URL& url, bool value)
{
    if (url.isBlankURL() || url.isEmpty())
        return;
    
    auto locker = holdLock(m_store->statisticsLock());
    auto& statistics = m_store->ensureResourceStatisticsForPrimaryDomain(primaryDomain(url));
    
    statistics.grandfathered = value;
}
    
bool ResourceLoadObserver::isGrandfathered(const URL& url)
{
    if (url.isBlankURL() || url.isEmpty())
        return false;
    
    auto locker = holdLock(m_store->statisticsLock());
    auto& statistics = m_store->ensureResourceStatisticsForPrimaryDomain(primaryDomain(url));
    
    return statistics.grandfathered;
}

void ResourceLoadObserver::setSubframeUnderTopFrameOrigin(const URL& subframe, const URL& topFrame)
{
    if (subframe.isBlankURL() || subframe.isEmpty() || topFrame.isBlankURL() || topFrame.isEmpty())
        return;
    
    ASSERT(m_queue);
    m_queue->dispatch([this, primaryTopFrameDomainString = primaryDomain(topFrame).isolatedCopy(), primarySubFrameDomainString = primaryDomain(subframe).isolatedCopy()] {
        auto locker = holdLock(m_store->statisticsLock());
        auto& statistics = m_store->ensureResourceStatisticsForPrimaryDomain(primarySubFrameDomainString);
        statistics.subframeUnderTopFrameOrigins.add(primaryTopFrameDomainString);
    });
}

void ResourceLoadObserver::setSubresourceUnderTopFrameOrigin(const URL& subresource, const URL& topFrame)
{
    if (subresource.isBlankURL() || subresource.isEmpty() || topFrame.isBlankURL() || topFrame.isEmpty())
        return;
    
    ASSERT(m_queue);
    m_queue->dispatch([this, primaryTopFrameDomainString = primaryDomain(topFrame).isolatedCopy(), primarySubresourceDomainString = primaryDomain(subresource).isolatedCopy()] {
        auto locker = holdLock(m_store->statisticsLock());
        auto& statistics = m_store->ensureResourceStatisticsForPrimaryDomain(primarySubresourceDomainString);
        statistics.subresourceUnderTopFrameOrigins.add(primaryTopFrameDomainString);
    });
}

void ResourceLoadObserver::setSubresourceUniqueRedirectTo(const URL& subresource, const URL& hostNameRedirectedTo)
{
    if (subresource.isBlankURL() || subresource.isEmpty() || hostNameRedirectedTo.isBlankURL() || hostNameRedirectedTo.isEmpty())
        return;
    
    ASSERT(m_queue);
    m_queue->dispatch([this, primaryRedirectDomainString = primaryDomain(hostNameRedirectedTo).isolatedCopy(), primarySubresourceDomainString = primaryDomain(subresource).isolatedCopy()] {
        auto locker = holdLock(m_store->statisticsLock());
        auto& statistics = m_store->ensureResourceStatisticsForPrimaryDomain(primarySubresourceDomainString);
        statistics.subresourceUniqueRedirectsTo.add(primaryRedirectDomainString);
    });
}

void ResourceLoadObserver::setTimeToLiveUserInteraction(Seconds seconds)
{
    m_store->setTimeToLiveUserInteraction(seconds);
}

void ResourceLoadObserver::setTimeToLiveCookiePartitionFree(Seconds seconds)
{
    m_store->setTimeToLiveCookiePartitionFree(seconds);
}

void ResourceLoadObserver::setMinimumTimeBetweeenDataRecordsRemoval(Seconds seconds)
{
    m_store->setMinimumTimeBetweeenDataRecordsRemoval(seconds);
}
    
void ResourceLoadObserver::setReducedTimestampResolution(Seconds seconds)
{
    if (seconds > 0_s)
        timestampResolution = seconds;
}

void ResourceLoadObserver::setGrandfatheringTime(Seconds seconds)
{
    m_store->setMinimumTimeBetweeenDataRecordsRemoval(seconds);
}
    
void ResourceLoadObserver::fireDataModificationHandler()
{
    // Helper function used by testing system. Should only be called from the main thread.
    ASSERT(isMainThread());
    m_queue->dispatch([this] {
        m_store->fireDataModificationHandler();
    });
}

void ResourceLoadObserver::fireShouldPartitionCookiesHandler()
{
    // Helper function used by testing system. Should only be called from the main thread.
    ASSERT(isMainThread());
    m_queue->dispatch([this] {
        m_store->fireShouldPartitionCookiesHandler();
    });
}

void ResourceLoadObserver::fireShouldPartitionCookiesHandler(const Vector<String>& domainsToRemove, const Vector<String>& domainsToAdd, bool clearFirst)
{
    // Helper function used by testing system. Should only be called from the main thread.
    ASSERT(isMainThread());
    m_queue->dispatch([this, domainsToRemove = CrossThreadCopier<Vector<String>>::copy(domainsToRemove), domainsToAdd = CrossThreadCopier<Vector<String>>::copy(domainsToAdd), clearFirst] {
        m_store->fireShouldPartitionCookiesHandler(domainsToRemove, domainsToAdd, clearFirst);
    });
}

void ResourceLoadObserver::fireTelemetryHandler()
{
    // Helper function used by testing system. Should only be called from the main thread.
    ASSERT(isMainThread());
    m_store->fireTelemetryHandler();
}
    
String ResourceLoadObserver::primaryDomain(const URL& url)
{
    return primaryDomain(url.host());
}

String ResourceLoadObserver::primaryDomain(const String& host)
{
    if (host.isNull() || host.isEmpty())
        return ASCIILiteral("nullOrigin");

#if ENABLE(PUBLIC_SUFFIX_LIST)
    String primaryDomain = topPrivatelyControlledDomain(host);
    // We will have an empty string here if there is no TLD. Use the host as a fallback.
    if (!primaryDomain.isEmpty())
        return primaryDomain;
#endif

    return host;
}

String ResourceLoadObserver::statisticsForOrigin(const String& origin)
{
    return m_store ? m_store->statisticsForOrigin(origin) : emptyString();
}

}

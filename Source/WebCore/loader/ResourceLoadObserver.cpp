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
#include "Page.h"
#include "ResourceLoadStatisticsStore.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "URL.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/WorkQueue.h>

namespace WebCore {

template<typename T> static inline String primaryDomain(const T& value)
{
    return ResourceLoadStatisticsStore::primaryDomain(value);
}

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

static inline bool is3xxRedirect(const ResourceResponse& response)
{
    return response.httpStatusCode() >= 300 && response.httpStatusCode() <= 399;
}

bool ResourceLoadObserver::shouldLog(Page* page) const
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
            auto targetStatistics = m_store->takeResourceStatisticsForPrimaryDomain(targetPrimaryDomain);

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
        if (newTime == statistics.mostRecentUserInteractionTime)
            return;

        statistics.hadUserInteraction = true;
        statistics.mostRecentUserInteractionTime = newTime;
        }
        
        m_store->fireDataModificationHandler();
    });
}

String ResourceLoadObserver::statisticsForOrigin(const String& origin)
{
    return m_store ? m_store->statisticsForOrigin(origin) : emptyString();
}

} // namespace WebCore

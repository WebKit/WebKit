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
#include "ResourceLoadStatistics.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "URL.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

template<typename T> static inline String primaryDomain(const T& value)
{
    return ResourceLoadStatistics::primaryDomain(value);
}

static Seconds timestampResolution { 1_h };

ResourceLoadObserver& ResourceLoadObserver::shared()
{
    static NeverDestroyed<ResourceLoadObserver> resourceLoadObserver;
    return resourceLoadObserver;
}

void ResourceLoadObserver::setNotificationCallback(WTF::Function<void()>&& notificationCallback)
{
    ASSERT(!m_notificationCallback);
    m_notificationCallback = WTFMove(notificationCallback);
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

    return Settings::resourceLoadStatisticsEnabled() && !page->usesEphemeralSession() && m_notificationCallback;
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

    auto targetStatistics = takeResourceStatisticsForPrimaryDomain(targetPrimaryDomain);

    // Always fire if we have previously removed data records for this domain
    bool shouldCallNotificationCallback = targetStatistics.dataRecordsRemoved > 0;

    if (isMainFrame)
        targetStatistics.topFrameHasBeenNavigatedToBefore = true;
    else {
        targetStatistics.subframeHasBeenLoadedBefore = true;

        auto subframeUnderTopFrameOriginsResult = targetStatistics.subframeUnderTopFrameOrigins.add(mainFramePrimaryDomain);
        if (subframeUnderTopFrameOriginsResult.isNewEntry)
            shouldCallNotificationCallback = true;
    }

    if (isRedirect) {
        auto& redirectingOriginResourceStatistics = ensureResourceStatisticsForPrimaryDomain(sourcePrimaryDomain);

        if (isPrevalentResource(targetPrimaryDomain))
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
            auto& sourceOriginResourceStatistics = ensureResourceStatisticsForPrimaryDomain(sourcePrimaryDomain);

            if (isMainFrame) {
                ++sourceOriginResourceStatistics.topFrameHasBeenNavigatedFrom;
                ++targetStatistics.topFrameHasBeenNavigatedTo;
            } else {
                ++sourceOriginResourceStatistics.subframeHasBeenNavigatedFrom;
                ++targetStatistics.subframeHasBeenNavigatedTo;
            }
        }
    }

    m_resourceStatisticsMap.set(targetPrimaryDomain, WTFMove(targetStatistics));

    if (shouldCallNotificationCallback)
        m_notificationCallback();
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

    auto targetStatistics = takeResourceStatisticsForPrimaryDomain(targetPrimaryDomain);

    // Always fire if we have previously removed data records for this domain
    bool shouldCallNotificationCallback = targetStatistics.dataRecordsRemoved > 0;

    auto subresourceUnderTopFrameOriginsResult = targetStatistics.subresourceUnderTopFrameOrigins.add(mainFramePrimaryDomain);
    if (subresourceUnderTopFrameOriginsResult.isNewEntry)
        shouldCallNotificationCallback = true;

    if (isRedirect) {
        auto& redirectingOriginStatistics = ensureResourceStatisticsForPrimaryDomain(sourcePrimaryDomain);

        if (isPrevalentResource(targetPrimaryDomain))
            redirectingOriginStatistics.redirectedToOtherPrevalentResourceOrigins.add(targetPrimaryDomain);

        ++redirectingOriginStatistics.subresourceHasBeenRedirectedFrom;
        ++targetStatistics.subresourceHasBeenRedirectedTo;

        auto subresourceUniqueRedirectsToResult = redirectingOriginStatistics.subresourceUniqueRedirectsTo.add(targetPrimaryDomain);
        if (subresourceUniqueRedirectsToResult.isNewEntry)
            shouldCallNotificationCallback = true;

        ++targetStatistics.subresourceHasBeenSubresourceCount;

        auto totalVisited = std::max(m_originsVisitedMap.size(), 1U);

        targetStatistics.subresourceHasBeenSubresourceCountDividedByTotalNumberOfOriginsVisited = static_cast<double>(targetStatistics.subresourceHasBeenSubresourceCount) / totalVisited;
    } else {
        ++targetStatistics.subresourceHasBeenSubresourceCount;

        auto totalVisited = std::max(m_originsVisitedMap.size(), 1U);

        targetStatistics.subresourceHasBeenSubresourceCountDividedByTotalNumberOfOriginsVisited = static_cast<double>(targetStatistics.subresourceHasBeenSubresourceCount) / totalVisited;
    }

    m_resourceStatisticsMap.set(targetPrimaryDomain, WTFMove(targetStatistics));

    if (shouldCallNotificationCallback)
        m_notificationCallback();
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

    auto& targetStatistics = ensureResourceStatisticsForPrimaryDomain(targetPrimaryDomain);

    // Always fire if we have previously removed data records for this domain
    bool shouldCallNotificationCallback = targetStatistics.dataRecordsRemoved > 0;

    auto subresourceUnderTopFrameOriginsResult = targetStatistics.subresourceUnderTopFrameOrigins.add(mainFramePrimaryDomain);
    if (subresourceUnderTopFrameOriginsResult.isNewEntry)
        shouldCallNotificationCallback = true;

    ++targetStatistics.subresourceHasBeenSubresourceCount;

    auto totalVisited = std::max(m_originsVisitedMap.size(), 1U);

    targetStatistics.subresourceHasBeenSubresourceCountDividedByTotalNumberOfOriginsVisited = static_cast<double>(targetStatistics.subresourceHasBeenSubresourceCount) / totalVisited;

    if (shouldCallNotificationCallback)
        m_notificationCallback();
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

    auto& statistics = ensureResourceStatisticsForPrimaryDomain(primaryDomain(url));
    auto newTime = reduceTimeResolution(WallTime::now());
    if (newTime == statistics.mostRecentUserInteractionTime)
        return;

    statistics.hadUserInteraction = true;
    statistics.mostRecentUserInteractionTime = newTime;

    m_notificationCallback();
}

ResourceLoadStatistics& ResourceLoadObserver::ensureResourceStatisticsForPrimaryDomain(const String& primaryDomain)
{
    auto addResult = m_resourceStatisticsMap.ensure(primaryDomain, [&primaryDomain] {
        return ResourceLoadStatistics(primaryDomain);
    });
    return addResult.iterator->value;
}

ResourceLoadStatistics ResourceLoadObserver::takeResourceStatisticsForPrimaryDomain(const String& primaryDomain)
{
    auto statististics = m_resourceStatisticsMap.take(primaryDomain);
    if (statististics.highLevelDomain.isNull())
        statististics.highLevelDomain = primaryDomain;
    ASSERT(statististics.highLevelDomain == primaryDomain);
    return statististics;
}

bool ResourceLoadObserver::isPrevalentResource(const String& primaryDomain) const
{
    auto mapEntry = m_resourceStatisticsMap.find(primaryDomain);
    if (mapEntry == m_resourceStatisticsMap.end())
        return false;
    return mapEntry->value.isPrevalentResource;
}

String ResourceLoadObserver::statisticsForOrigin(const String& origin)
{
    auto iter = m_resourceStatisticsMap.find(origin);
    if (iter == m_resourceStatisticsMap.end())
        return emptyString();

    return "Statistics for " + origin + ":\n" + iter->value.toString();
}

Vector<ResourceLoadStatistics> ResourceLoadObserver::takeStatistics()
{
    Vector<ResourceLoadStatistics> statistics;
    statistics.reserveInitialCapacity(m_resourceStatisticsMap.size());
    for (auto& statistic : m_resourceStatisticsMap.values())
        statistics.uncheckedAppend(WTFMove(statistic));

    m_resourceStatisticsMap.clear();

    return statistics;
}

} // namespace WebCore

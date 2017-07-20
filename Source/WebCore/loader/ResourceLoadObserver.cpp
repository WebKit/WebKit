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

namespace WebCore {

template<typename T> static inline String primaryDomain(const T& value)
{
    return ResourceLoadStatistics::primaryDomain(value);
}

static Seconds timestampResolution { 1_h };
static const Seconds minimumNotificationInterval { 5_s };

ResourceLoadObserver& ResourceLoadObserver::shared()
{
    static NeverDestroyed<ResourceLoadObserver> resourceLoadObserver;
    return resourceLoadObserver;
}

void ResourceLoadObserver::setShouldThrottleObserverNotifications(bool shouldThrottle)
{
    m_shouldThrottleNotifications = shouldThrottle;

    if (!m_notificationTimer.isActive())
        return;

    // If we change the notification state, we need to restart any notifications
    // so they will be on the right schedule.
    m_notificationTimer.stop();
    scheduleNotificationIfNeeded();
}

void ResourceLoadObserver::setNotificationCallback(WTF::Function<void (Vector<ResourceLoadStatistics>&&)>&& notificationCallback)
{
    ASSERT(!m_notificationCallback);
    m_notificationCallback = WTFMove(notificationCallback);
}

ResourceLoadObserver::ResourceLoadObserver()
    : m_notificationTimer(*this, &ResourceLoadObserver::notificationTimerFired)
{
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

static WallTime reduceToHourlyTimeResolution(WallTime time)
{
    return WallTime::fromRawSeconds(std::floor(time.secondsSinceEpoch() / timestampResolution) * timestampResolution.seconds());
}

void ResourceLoadObserver::logFrameNavigation(const Frame& frame, const Frame& topFrame, const ResourceRequest& newRequest)
{
    ASSERT(frame.document());
    ASSERT(topFrame.document());
    ASSERT(topFrame.page());

    if (frame.isMainFrame())
        return;
    
    if (!shouldLog(topFrame.page()))
        return;

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

    auto& targetStatistics = ensureResourceStatisticsForPrimaryDomain(targetPrimaryDomain);
    targetStatistics.lastSeen = reduceToHourlyTimeResolution(WallTime::now());
    auto subframeUnderTopFrameOriginsResult = targetStatistics.subframeUnderTopFrameOrigins.add(mainFramePrimaryDomain);
    if (subframeUnderTopFrameOriginsResult.isNewEntry)
        scheduleNotificationIfNeeded();
}

// FIXME: This quirk was added to address <rdar://problem/33325881> and should be removed once content is fixed.
static bool resourceNeedsSSOQuirk(const URL& url)
{
    static const auto ssoOriginsHash = makeNeverDestroyed(HashSet<String, ASCIICaseInsensitiveHash> {
        "sp.auth.adobe.com"
    });
    return ssoOriginsHash.get().contains(url.host());
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

    if (resourceNeedsSSOQuirk(targetURL))
        return;

    bool shouldCallNotificationCallback = false;
    {
        auto& targetStatistics = ensureResourceStatisticsForPrimaryDomain(targetPrimaryDomain);
        targetStatistics.lastSeen = reduceToHourlyTimeResolution(WallTime::now());
        if (targetStatistics.subresourceUnderTopFrameOrigins.add(mainFramePrimaryDomain).isNewEntry)
            shouldCallNotificationCallback = true;
    }

    if (isRedirect) {
        auto& redirectingOriginStatistics = ensureResourceStatisticsForPrimaryDomain(sourcePrimaryDomain);
        if (redirectingOriginStatistics.subresourceUniqueRedirectsTo.add(targetPrimaryDomain).isNewEntry)
            shouldCallNotificationCallback = true;
    }

    if (shouldCallNotificationCallback)
        scheduleNotificationIfNeeded();
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
    targetStatistics.lastSeen = reduceToHourlyTimeResolution(WallTime::now());
    if (targetStatistics.subresourceUnderTopFrameOrigins.add(mainFramePrimaryDomain).isNewEntry)
        scheduleNotificationIfNeeded();
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
    auto newTime = reduceToHourlyTimeResolution(WallTime::now());
    if (newTime == statistics.mostRecentUserInteractionTime)
        return;

    statistics.hadUserInteraction = true;
    statistics.lastSeen = newTime;
    statistics.mostRecentUserInteractionTime = newTime;

    scheduleNotificationIfNeeded();
}

ResourceLoadStatistics& ResourceLoadObserver::ensureResourceStatisticsForPrimaryDomain(const String& primaryDomain)
{
    auto addResult = m_resourceStatisticsMap.ensure(primaryDomain, [&primaryDomain] {
        return ResourceLoadStatistics(primaryDomain);
    });
    return addResult.iterator->value;
}

void ResourceLoadObserver::scheduleNotificationIfNeeded()
{
    ASSERT(m_notificationCallback);
    if (m_resourceStatisticsMap.isEmpty()) {
        m_notificationTimer.stop();
        return;
    }

    if (!m_notificationTimer.isActive())
        m_notificationTimer.startOneShot(m_shouldThrottleNotifications ? minimumNotificationInterval : 0_s);
}

void ResourceLoadObserver::notificationTimerFired()
{
    ASSERT(m_notificationCallback);
    m_notificationCallback(takeStatistics());
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

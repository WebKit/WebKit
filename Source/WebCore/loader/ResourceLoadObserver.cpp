/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

#include "DeprecatedGlobalSettings.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLFrameOwnerElement.h"
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

static bool shouldEnableSiteSpecificQuirks(Page* page)
{
#if PLATFORM(IOS)
    UNUSED_PARAM(page);

    // There is currently no way to toggle the needsSiteSpecificQuirks setting on iOS so we always enable
    // the site-specific quirks on iOS.
    return true;
#else
    return page && page->settings().needsSiteSpecificQuirks();
#endif
}

// FIXME: Temporary fix for <rdar://problem/32343256> until content can be updated.
static bool areDomainsAssociated(Page* page, const String& firstDomain, const String& secondDomain)
{
    static NeverDestroyed<HashMap<String, unsigned>> metaDomainIdentifiers = [] {
        HashMap<String, unsigned> map;

        // Domains owned by Dow Jones & Company, Inc.
        const unsigned dowJonesIdentifier = 1;
        map.add(ASCIILiteral("dowjones.com"), dowJonesIdentifier);
        map.add(ASCIILiteral("wsj.com"), dowJonesIdentifier);
        map.add(ASCIILiteral("barrons.com"), dowJonesIdentifier);
        map.add(ASCIILiteral("marketwatch.com"), dowJonesIdentifier);
        map.add(ASCIILiteral("wsjplus.com"), dowJonesIdentifier);

        return map;
    }();

    if (firstDomain == secondDomain)
        return true;

    ASSERT(!equalIgnoringASCIICase(firstDomain, secondDomain));

    if (!shouldEnableSiteSpecificQuirks(page))
        return false;

    unsigned firstMetaDomainIdentifier = metaDomainIdentifiers.get().get(firstDomain);
    if (!firstMetaDomainIdentifier)
        return false;

    return firstMetaDomainIdentifier == metaDomainIdentifiers.get().get(secondDomain);
}

void ResourceLoadObserver::setNotificationCallback(WTF::Function<void (Vector<ResourceLoadStatistics>&&)>&& notificationCallback)
{
    ASSERT(!m_notificationCallback);
    m_notificationCallback = WTFMove(notificationCallback);
}

ResourceLoadObserver::ResourceLoadObserver()
    : m_notificationTimer(*this, &ResourceLoadObserver::notifyObserver)
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

    return DeprecatedGlobalSettings::resourceLoadStatisticsEnabled() && !page->usesEphemeralSession() && m_notificationCallback;
}

static WallTime reduceToHourlyTimeResolution(WallTime time)
{
    return WallTime::fromRawSeconds(std::floor(time.secondsSinceEpoch() / timestampResolution) * timestampResolution.seconds());
}

void ResourceLoadObserver::logFrameNavigation(const Frame& frame, const Frame& topFrame, const ResourceRequest& newRequest, const URL& redirectUrl)
{
    ASSERT(frame.document());
    ASSERT(topFrame.document());
    ASSERT(topFrame.page());

    if (frame.isMainFrame())
        return;
    
    auto* page = topFrame.page();
    if (!shouldLog(page))
        return;

    auto sourceURL = redirectUrl;
    bool isRedirect = !redirectUrl.isNull();
    if (!isRedirect)
        sourceURL = nonNullOwnerURL(*frame.document());

    auto& targetURL = newRequest.url();
    auto& mainFrameURL = topFrame.document()->url();
    
    if (!targetURL.isValid() || !mainFrameURL.isValid())
        return;

    auto targetHost = targetURL.host();
    auto mainFrameHost = mainFrameURL.host();

    if (targetHost.isEmpty() || mainFrameHost.isEmpty() || targetHost == sourceURL.host())
        return;

    auto targetPrimaryDomain = primaryDomain(targetURL);
    auto mainFramePrimaryDomain = primaryDomain(mainFrameURL);
    auto sourcePrimaryDomain = primaryDomain(sourceURL);
    bool shouldCallNotificationCallback = false;

    if (targetHost != mainFrameHost
        && !(areDomainsAssociated(page, targetPrimaryDomain, mainFramePrimaryDomain) || areDomainsAssociated(page, targetPrimaryDomain, sourcePrimaryDomain))) {
        auto& targetStatistics = ensureResourceStatisticsForPrimaryDomain(targetPrimaryDomain);
        targetStatistics.lastSeen = reduceToHourlyTimeResolution(WallTime::now());
        if (targetStatistics.subframeUnderTopFrameOrigins.add(mainFramePrimaryDomain).isNewEntry)
            shouldCallNotificationCallback = true;
    }

    if (isRedirect
        && !areDomainsAssociated(page, sourcePrimaryDomain, targetPrimaryDomain)) {
        auto& redirectingOriginStatistics = ensureResourceStatisticsForPrimaryDomain(sourcePrimaryDomain);
        if (redirectingOriginStatistics.subresourceUniqueRedirectsTo.add(targetPrimaryDomain).isNewEntry)
            shouldCallNotificationCallback = true;
    }

    if (shouldCallNotificationCallback)
        scheduleNotificationIfNeeded();
}

// FIXME: This quirk was added to address <rdar://problem/33325881> and should be removed once content is fixed.
static bool resourceNeedsSSOQuirk(Page* page, const URL& url)
{
    if (!shouldEnableSiteSpecificQuirks(page))
        return false;

    return equalIgnoringASCIICase(url.host(), "sp.auth.adobe.com");
}

void ResourceLoadObserver::logSubresourceLoading(const Frame* frame, const ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    ASSERT(frame->page());

    auto* page = frame->page();
    if (!shouldLog(page))
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
    
    if (areDomainsAssociated(page, targetPrimaryDomain, mainFramePrimaryDomain) || (isRedirect && areDomainsAssociated(page, targetPrimaryDomain, sourcePrimaryDomain)))
        return;

    if (resourceNeedsSSOQuirk(page, targetURL))
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

    auto* page = frame->page();
    if (!shouldLog(page))
        return;

    auto& mainFrameURL = frame->mainFrame().document()->url();

    auto targetHost = targetURL.host();
    auto mainFrameHost = mainFrameURL.host();
    
    if (targetHost.isEmpty() || mainFrameHost.isEmpty() || targetHost == mainFrameHost)
        return;
    
    auto targetPrimaryDomain = primaryDomain(targetURL);
    auto mainFramePrimaryDomain = primaryDomain(mainFrameURL);
    
    if (areDomainsAssociated(page, targetPrimaryDomain, mainFramePrimaryDomain))
        return;

    auto& targetStatistics = ensureResourceStatisticsForPrimaryDomain(targetPrimaryDomain);
    targetStatistics.lastSeen = reduceToHourlyTimeResolution(WallTime::now());
    if (targetStatistics.subresourceUnderTopFrameOrigins.add(mainFramePrimaryDomain).isNewEntry)
        scheduleNotificationIfNeeded();
}

void ResourceLoadObserver::logUserInteractionWithReducedTimeResolution(const Document& document)
{
    if (!shouldLog(document.page()))
        return;

    ASSERT(document.page());

    auto& url = document.url();
    if (url.isBlankURL() || url.isEmpty())
        return;

    auto domain = primaryDomain(url);
    auto newTime = reduceToHourlyTimeResolution(WallTime::now());
    auto lastReportedUserInteraction = m_lastReportedUserInteractionMap.get(domain);
    if (newTime == lastReportedUserInteraction)
        return;

    m_lastReportedUserInteractionMap.set(domain, newTime);

    auto& statistics = ensureResourceStatisticsForPrimaryDomain(domain);
    statistics.hadUserInteraction = true;
    statistics.lastSeen = newTime;
    statistics.mostRecentUserInteractionTime = newTime;

    m_notificationTimer.stop();
    notifyObserver();
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
        m_notificationTimer.startOneShot(minimumNotificationInterval);
}

void ResourceLoadObserver::notifyObserver()
{
    ASSERT(m_notificationCallback);
    m_notificationTimer.stop();
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

void ResourceLoadObserver::clearState()
{
    m_notificationTimer.stop();
    m_resourceStatisticsMap.clear();
    m_lastReportedUserInteractionMap.clear();
}

URL ResourceLoadObserver::nonNullOwnerURL(const Document& document) const
{
    auto url = document.url();
    auto* frame = document.frame();
    auto host = document.url().host();

    while ((host.isNull() || host.isEmpty()) && frame && !frame->isMainFrame()) {
        auto* ownerElement = frame->ownerElement();

        ASSERT(ownerElement != nullptr);
        
        auto& doc = ownerElement->document();
        frame = doc.frame();
        url = doc.url();
        host = url.host();
    }

    return url;
}

} // namespace WebCore

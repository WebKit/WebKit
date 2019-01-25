/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include "Page.h"
#include "ResourceLoadStatistics.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "RuntimeEnabledFeatures.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include <wtf/URL.h>

namespace WebCore {

template<typename T> static inline String primaryDomain(const T& value)
{
    return ResourceLoadStatistics::primaryDomain(value);
}

static const Seconds minimumNotificationInterval { 5_s };

ResourceLoadObserver& ResourceLoadObserver::shared()
{
    static NeverDestroyed<ResourceLoadObserver> resourceLoadObserver;
    return resourceLoadObserver;
}

void ResourceLoadObserver::setNotificationCallback(WTF::Function<void (Vector<ResourceLoadStatistics>&&)>&& notificationCallback)
{
    ASSERT(!m_notificationCallback);
    m_notificationCallback = WTFMove(notificationCallback);
}

void ResourceLoadObserver::setRequestStorageAccessUnderOpenerCallback(WTF::Function<void(const String& domainInNeedOfStorageAccess, uint64_t openerPageID, const String& openerDomain)>&& callback)
{
    ASSERT(!m_requestStorageAccessUnderOpenerCallback);
    m_requestStorageAccessUnderOpenerCallback = WTFMove(callback);
}

void ResourceLoadObserver::setLogUserInteractionNotificationCallback(Function<void(PAL::SessionID, const String&)>&& callback)
{
    ASSERT(!m_logUserInteractionNotificationCallback);
    m_logUserInteractionNotificationCallback = WTFMove(callback);
}

void ResourceLoadObserver::setLogWebSocketLoadingNotificationCallback(Function<void(PAL::SessionID, const String&, const String&, WallTime)>&& callback)
{
    ASSERT(!m_logWebSocketLoadingNotificationCallback);
    m_logWebSocketLoadingNotificationCallback = WTFMove(callback);
}

void ResourceLoadObserver::setLogSubresourceLoadingNotificationCallback(Function<void(PAL::SessionID, const String&, const String&, WallTime)>&& callback)
{
    ASSERT(!m_logSubresourceLoadingNotificationCallback);
    m_logSubresourceLoadingNotificationCallback = WTFMove(callback);
}

void ResourceLoadObserver::setLogSubresourceRedirectNotificationCallback(Function<void(PAL::SessionID, const String&, const String&)>&& callback)
{
    ASSERT(!m_logSubresourceRedirectNotificationCallback);
    m_logSubresourceRedirectNotificationCallback = WTFMove(callback);
}
    
ResourceLoadObserver::ResourceLoadObserver()
    : m_notificationTimer(*this, &ResourceLoadObserver::notifyObserver)
{
}

static inline bool is3xxRedirect(const ResourceResponse& response)
{
    return response.httpStatusCode() >= 300 && response.httpStatusCode() <= 399;
}

bool ResourceLoadObserver::shouldLog(bool usesEphemeralSession) const
{
    return DeprecatedGlobalSettings::resourceLoadStatisticsEnabled() && !usesEphemeralSession && m_notificationCallback;
}

void ResourceLoadObserver::logSubresourceLoading(const Frame* frame, const ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    ASSERT(frame->page());

    if (!frame)
        return;

    auto* page = frame->page();
    if (!page || !shouldLog(page->usesEphemeralSession()))
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

    bool shouldCallNotificationCallback = false;
    {
        auto& targetStatistics = ensureResourceStatisticsForPrimaryDomain(targetPrimaryDomain);
        auto lastSeen = ResourceLoadStatistics::reduceTimeResolution(WallTime::now());
        targetStatistics.lastSeen = lastSeen;
        if (targetStatistics.subresourceUnderTopFrameOrigins.add(mainFramePrimaryDomain).isNewEntry)
            shouldCallNotificationCallback = true;

        m_logSubresourceLoadingNotificationCallback(page->sessionID(), targetPrimaryDomain, mainFramePrimaryDomain, lastSeen);
    }

    if (isRedirect) {
        auto& redirectingOriginStatistics = ensureResourceStatisticsForPrimaryDomain(sourcePrimaryDomain);
        bool isNewRedirectToEntry = redirectingOriginStatistics.subresourceUniqueRedirectsTo.add(targetPrimaryDomain).isNewEntry;
        auto& targetStatistics = ensureResourceStatisticsForPrimaryDomain(targetPrimaryDomain);
        bool isNewRedirectFromEntry = targetStatistics.subresourceUniqueRedirectsFrom.add(sourcePrimaryDomain).isNewEntry;

        if (isNewRedirectToEntry || isNewRedirectFromEntry)
            shouldCallNotificationCallback = true;

        m_logSubresourceRedirectNotificationCallback(page->sessionID(), sourcePrimaryDomain, targetPrimaryDomain);
    }

    if (shouldCallNotificationCallback)
        scheduleNotificationIfNeeded();
}

void ResourceLoadObserver::logWebSocketLoading(const URL& targetURL, const URL& mainFrameURL, PAL::SessionID sessionID)
{
    if (!shouldLog(sessionID.isEphemeral()))
        return;

    auto targetHost = targetURL.host();
    auto mainFrameHost = mainFrameURL.host();
    
    if (targetHost.isEmpty() || mainFrameHost.isEmpty() || targetHost == mainFrameHost)
        return;
    
    auto targetPrimaryDomain = primaryDomain(targetURL);
    auto mainFramePrimaryDomain = primaryDomain(mainFrameURL);

    if (targetPrimaryDomain == mainFramePrimaryDomain)
        return;

    auto lastSeen = ResourceLoadStatistics::reduceTimeResolution(WallTime::now());

    auto& targetStatistics = ensureResourceStatisticsForPrimaryDomain(targetPrimaryDomain);
    targetStatistics.lastSeen = lastSeen;
    if (targetStatistics.subresourceUnderTopFrameOrigins.add(mainFramePrimaryDomain).isNewEntry)
        scheduleNotificationIfNeeded();

    m_logWebSocketLoadingNotificationCallback(sessionID, targetPrimaryDomain, mainFramePrimaryDomain, lastSeen);
}

void ResourceLoadObserver::logUserInteractionWithReducedTimeResolution(const Document& document)
{
    if (!shouldLog(document.sessionID().isEphemeral()))
        return;

    auto& url = document.url();
    if (url.protocolIsAbout() || url.isEmpty())
        return;

    auto domain = primaryDomain(url);
    auto newTime = ResourceLoadStatistics::reduceTimeResolution(WallTime::now());
    auto lastReportedUserInteraction = m_lastReportedUserInteractionMap.get(domain);
    if (newTime == lastReportedUserInteraction)
        return;

    m_lastReportedUserInteractionMap.set(domain, newTime);

    auto& statistics = ensureResourceStatisticsForPrimaryDomain(domain);
    statistics.hadUserInteraction = true;
    statistics.lastSeen = newTime;
    statistics.mostRecentUserInteractionTime = newTime;

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (auto* opener = document.frame()->loader().opener()) {
        if (auto* openerDocument = opener->document()) {
            if (auto* openerFrame = openerDocument->frame()) {
                if (auto openerPageID = openerFrame->loader().client().pageID()) {
                    requestStorageAccessUnderOpener(domain, openerPageID.value(), *openerDocument);
                }
            }
        }
    }

    m_logUserInteractionNotificationCallback(document.sessionID(), domain);
#endif

    m_notificationTimer.stop();
    notifyObserver();

#if ENABLE(RESOURCE_LOAD_STATISTICS) && !RELEASE_LOG_DISABLED
    if (shouldLogUserInteraction()) {
        auto counter = ++m_loggingCounter;
#define LOCAL_LOG(str, ...) \
        RELEASE_LOG(ResourceLoadStatistics, "ResourceLoadObserver::logUserInteraction: counter = %" PRIu64 ": " str, counter, ##__VA_ARGS__)

        auto escapeForJSON = [](String s) {
            s.replace('\\', "\\\\").replace('"', "\\\"");
            return s;
        };
        auto escapedURL = escapeForJSON(url.string());
        auto escapedDomain = escapeForJSON(domain);

        LOCAL_LOG(R"({ "url": "%{public}s",)", escapedURL.utf8().data());
        LOCAL_LOG(R"(  "domain" : "%{public}s",)", escapedDomain.utf8().data());
        LOCAL_LOG(R"(  "until" : %f })", newTime.secondsSinceEpoch().seconds());

#undef LOCAL_LOG
    }
#endif
}

#if ENABLE(RESOURCE_LOAD_STATISTICS)
void ResourceLoadObserver::requestStorageAccessUnderOpener(const String& domainInNeedOfStorageAccess, uint64_t openerPageID, Document& openerDocument)
{
    auto openerUrl = openerDocument.url();
    auto openerPrimaryDomain = primaryDomain(openerUrl);
    if (domainInNeedOfStorageAccess != openerPrimaryDomain
        && !openerDocument.hasRequestedPageSpecificStorageAccessWithUserInteraction(domainInNeedOfStorageAccess)
        && !equalIgnoringASCIICase(openerUrl.string(), WTF::blankURL())) {
        m_requestStorageAccessUnderOpenerCallback(domainInNeedOfStorageAccess, openerPageID, openerPrimaryDomain);
        // Remember user interaction-based requests since they don't need to be repeated.
        openerDocument.setHasRequestedPageSpecificStorageAccessWithUserInteraction(domainInNeedOfStorageAccess);
    }
}
#endif

void ResourceLoadObserver::logFontLoad(const Document& document, const String& familyName, bool loadStatus)
{
#if ENABLE(WEB_API_STATISTICS)
    if (!shouldLog(document.sessionID().isEphemeral()))
        return;
    auto registrableDomain = primaryDomain(document.url());
    auto& statistics = ensureResourceStatisticsForPrimaryDomain(registrableDomain);
    bool shouldCallNotificationCallback = false;
    if (!loadStatus) {
        if (statistics.fontsFailedToLoad.add(familyName).isNewEntry)
            shouldCallNotificationCallback = true;
    } else {
        if (statistics.fontsSuccessfullyLoaded.add(familyName).isNewEntry)
            shouldCallNotificationCallback = true;
    }
    auto mainFrameRegistrableDomain = primaryDomain(document.topDocument().url());
    if (statistics.topFrameRegistrableDomainsWhichAccessedWebAPIs.add(mainFrameRegistrableDomain).isNewEntry)
        shouldCallNotificationCallback = true;
    if (shouldCallNotificationCallback)
        scheduleNotificationIfNeeded();
#else
    UNUSED_PARAM(document);
    UNUSED_PARAM(familyName);
    UNUSED_PARAM(loadStatus);
#endif
}
    
void ResourceLoadObserver::logCanvasRead(const Document& document)
{
#if ENABLE(WEB_API_STATISTICS)
    if (!shouldLog(document.sessionID().isEphemeral()))
        return;
    auto registrableDomain = primaryDomain(document.url());
    auto& statistics = ensureResourceStatisticsForPrimaryDomain(registrableDomain);
    auto mainFrameRegistrableDomain = primaryDomain(document.topDocument().url());
    statistics.canvasActivityRecord.wasDataRead = true;
    if (statistics.topFrameRegistrableDomainsWhichAccessedWebAPIs.add(mainFrameRegistrableDomain).isNewEntry)
        scheduleNotificationIfNeeded();
#else
    UNUSED_PARAM(document);
#endif
}

void ResourceLoadObserver::logCanvasWriteOrMeasure(const Document& document, const String& textWritten)
{
#if ENABLE(WEB_API_STATISTICS)
    if (!shouldLog(document.sessionID().isEphemeral()))
        return;
    auto registrableDomain = primaryDomain(document.url());
    auto& statistics = ensureResourceStatisticsForPrimaryDomain(registrableDomain);
    bool shouldCallNotificationCallback = false;
    auto mainFrameRegistrableDomain = primaryDomain(document.topDocument().url());
    if (statistics.canvasActivityRecord.recordWrittenOrMeasuredText(textWritten))
        shouldCallNotificationCallback = true;
    if (statistics.topFrameRegistrableDomainsWhichAccessedWebAPIs.add(mainFrameRegistrableDomain).isNewEntry)
        shouldCallNotificationCallback = true;
    if (shouldCallNotificationCallback)
        scheduleNotificationIfNeeded();
#else
    UNUSED_PARAM(document);
    UNUSED_PARAM(textWritten);
#endif
}
    
void ResourceLoadObserver::logNavigatorAPIAccessed(const Document& document, const ResourceLoadStatistics::NavigatorAPI functionName)
{
#if ENABLE(WEB_API_STATISTICS)
    if (!shouldLog(document.sessionID().isEphemeral()))
        return;
    auto registrableDomain = primaryDomain(document.url());
    auto& statistics = ensureResourceStatisticsForPrimaryDomain(registrableDomain);
    bool shouldCallNotificationCallback = false;
    if (!statistics.navigatorFunctionsAccessed.contains(functionName)) {
        statistics.navigatorFunctionsAccessed.add(functionName);
        shouldCallNotificationCallback = true;
    }
    auto mainFrameRegistrableDomain = primaryDomain(document.topDocument().url());
    if (statistics.topFrameRegistrableDomainsWhichAccessedWebAPIs.add(mainFrameRegistrableDomain).isNewEntry)
        shouldCallNotificationCallback = true;
    if (shouldCallNotificationCallback)
        scheduleNotificationIfNeeded();
#else
    UNUSED_PARAM(document);
    UNUSED_PARAM(functionName);
#endif
}
    
void ResourceLoadObserver::logScreenAPIAccessed(const Document& document, const ResourceLoadStatistics::ScreenAPI functionName)
{
#if ENABLE(WEB_API_STATISTICS)
    if (!shouldLog(document.sessionID().isEphemeral()))
        return;
    auto registrableDomain = primaryDomain(document.url());
    auto& statistics = ensureResourceStatisticsForPrimaryDomain(registrableDomain);
    bool shouldCallNotificationCallback = false;
    if (!statistics.screenFunctionsAccessed.contains(functionName)) {
        statistics.screenFunctionsAccessed.add(functionName);
        shouldCallNotificationCallback = true;
    }
    auto mainFrameRegistrableDomain = primaryDomain(document.topDocument().url());
    if (statistics.topFrameRegistrableDomainsWhichAccessedWebAPIs.add(mainFrameRegistrableDomain).isNewEntry)
        shouldCallNotificationCallback = true;
    if (shouldCallNotificationCallback)
        scheduleNotificationIfNeeded();
#else
    UNUSED_PARAM(document);
    UNUSED_PARAM(functionName);
#endif
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

/*
* Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebResourceLoadObserver.h"

#if ENABLE(RESOURCE_LOAD_STATISTICS)

#include "Logging.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameLoaderClient.h>
#include <WebCore/HTMLFrameOwnerElement.h>
#include <WebCore/Page.h>

namespace WebKit {

using namespace WebCore;

static const Seconds minimumNotificationInterval { 5_s };

#if !RELEASE_LOG_DISABLED
bool WebResourceLoadObserver::shouldLogUserInteraction { false };

void WebResourceLoadObserver::setShouldLogUserInteraction(bool value)
{
    shouldLogUserInteraction = value;
}
#endif

static bool is3xxRedirect(const ResourceResponse& response)
{
    return response.httpStatusCode() >= 300 && response.httpStatusCode() <= 399;
}

WebResourceLoadObserver::WebResourceLoadObserver(ResourceLoadStatistics::IsEphemeral isEphemeral)
    : m_isEphemeral(isEphemeral)
    , m_notificationTimer(*this, &WebResourceLoadObserver::updateCentralStatisticsStore)
{
}

WebResourceLoadObserver::~WebResourceLoadObserver()
{
    if (hasStatistics())
        updateCentralStatisticsStore();
}

void WebResourceLoadObserver::requestStorageAccessUnderOpener(const RegistrableDomain& domainInNeedOfStorageAccess, WebPage& openerPage, Document& openerDocument)
{
    auto openerUrl = openerDocument.url();
    RegistrableDomain openerDomain { openerUrl };
    if (domainInNeedOfStorageAccess != openerDomain
        && !openerDocument.hasRequestedPageSpecificStorageAccessWithUserInteraction(domainInNeedOfStorageAccess)
        && !openerUrl.isAboutBlank()) {
        WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::RequestStorageAccessUnderOpener(domainInNeedOfStorageAccess, openerPage.identifier(), openerDomain), 0);
        
        openerPage.addDomainWithPageLevelStorageAccess(openerDomain, domainInNeedOfStorageAccess);

        // Remember user interaction-based requests since they don't need to be repeated.
        openerDocument.setHasRequestedPageSpecificStorageAccessWithUserInteraction(domainInNeedOfStorageAccess);
    }
}
    
ResourceLoadStatistics& WebResourceLoadObserver::ensureResourceStatisticsForRegistrableDomain(const RegistrableDomain& domain)
{
    RELEASE_ASSERT(!isEphemeral());

    return m_resourceStatisticsMap.ensure(domain, [&domain] {
        return ResourceLoadStatistics(domain);
    }).iterator->value;
}

void WebResourceLoadObserver::scheduleNotificationIfNeeded()
{
    if (m_resourceStatisticsMap.isEmpty()) {
        m_notificationTimer.stop();
        return;
    }

    if (!m_notificationTimer.isActive())
        m_notificationTimer.startOneShot(minimumNotificationInterval);
}

void WebResourceLoadObserver::updateCentralStatisticsStore()
{
    m_notificationTimer.stop();
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::ResourceLoadStatisticsUpdated(takeStatistics()), 0);
}

String WebResourceLoadObserver::statisticsForURL(const URL& url)
{
    auto iter = m_resourceStatisticsMap.find(RegistrableDomain { url });
    if (iter == m_resourceStatisticsMap.end())
        return emptyString();

    return makeString("Statistics for ", url.host().toString(), ":\n", iter->value.toString());
}

Vector<ResourceLoadStatistics> WebResourceLoadObserver::takeStatistics()
{
    Vector<ResourceLoadStatistics> statistics;
    statistics.reserveInitialCapacity(m_resourceStatisticsMap.size());
    for (auto& statistic : m_resourceStatisticsMap.values())
        statistics.uncheckedAppend(WTFMove(statistic));
    m_resourceStatisticsMap.clear();
    return statistics;
}

void WebResourceLoadObserver::clearState()
{
    m_notificationTimer.stop();
    m_resourceStatisticsMap.clear();
    m_lastReportedUserInteractionMap.clear();
}

bool WebResourceLoadObserver::hasHadUserInteraction(const RegistrableDomain& domain) const
{
    return m_domainsWithUserInteraction.contains(domain);
}

void WebResourceLoadObserver::logFontLoad(const Document& document, const String& familyName, bool loadStatus)
{
    if (isEphemeral())
        return;

#if ENABLE(WEB_API_STATISTICS)
    RegistrableDomain registrableDomain { document.url() };
    auto& statistics = ensureResourceStatisticsForRegistrableDomain(registrableDomain);
    bool shouldCallNotificationCallback = false;
    if (!loadStatus) {
        if (statistics.fontsFailedToLoad.add(familyName).isNewEntry)
            shouldCallNotificationCallback = true;
    } else {
        if (statistics.fontsSuccessfullyLoaded.add(familyName).isNewEntry)
            shouldCallNotificationCallback = true;
    }
    RegistrableDomain mainFrameRegistrableDomain { document.topDocument().url() };
    if (statistics.topFrameRegistrableDomainsWhichAccessedWebAPIs.add(mainFrameRegistrableDomain.string()).isNewEntry)
        shouldCallNotificationCallback = true;
    if (shouldCallNotificationCallback)
        scheduleNotificationIfNeeded();
#else
    UNUSED_PARAM(document);
    UNUSED_PARAM(familyName);
    UNUSED_PARAM(loadStatus);
#endif
}
    
void WebResourceLoadObserver::logCanvasRead(const Document& document)
{
    if (isEphemeral())
        return;

#if ENABLE(WEB_API_STATISTICS)
    RegistrableDomain registrableDomain { document.url() };
    auto& statistics = ensureResourceStatisticsForRegistrableDomain(registrableDomain);
    RegistrableDomain mainFrameRegistrableDomain { document.topDocument().url() };
    statistics.canvasActivityRecord.wasDataRead = true;
    if (statistics.topFrameRegistrableDomainsWhichAccessedWebAPIs.add(mainFrameRegistrableDomain.string()).isNewEntry)
        scheduleNotificationIfNeeded();
#else
    UNUSED_PARAM(document);
#endif
}

void WebResourceLoadObserver::logCanvasWriteOrMeasure(const Document& document, const String& textWritten)
{
    if (isEphemeral())
        return;

#if ENABLE(WEB_API_STATISTICS)
    RegistrableDomain registrableDomain { document.url() };
    auto& statistics = ensureResourceStatisticsForRegistrableDomain(registrableDomain);
    bool shouldCallNotificationCallback = false;
    RegistrableDomain mainFrameRegistrableDomain { document.topDocument().url() };
    if (statistics.canvasActivityRecord.recordWrittenOrMeasuredText(textWritten))
        shouldCallNotificationCallback = true;
    if (statistics.topFrameRegistrableDomainsWhichAccessedWebAPIs.add(mainFrameRegistrableDomain.string()).isNewEntry)
        shouldCallNotificationCallback = true;
    if (shouldCallNotificationCallback)
        scheduleNotificationIfNeeded();
#else
    UNUSED_PARAM(document);
    UNUSED_PARAM(textWritten);
#endif
}
    
void WebResourceLoadObserver::logNavigatorAPIAccessed(const Document& document, const ResourceLoadStatistics::NavigatorAPI functionName)
{
    if (isEphemeral())
        return;

#if ENABLE(WEB_API_STATISTICS)
    RegistrableDomain registrableDomain { document.url() };
    auto& statistics = ensureResourceStatisticsForRegistrableDomain(registrableDomain);
    bool shouldCallNotificationCallback = false;
    if (!statistics.navigatorFunctionsAccessed.contains(functionName)) {
        statistics.navigatorFunctionsAccessed.add(functionName);
        shouldCallNotificationCallback = true;
    }
    RegistrableDomain mainFrameRegistrableDomain { document.topDocument().url() };
    if (statistics.topFrameRegistrableDomainsWhichAccessedWebAPIs.add(mainFrameRegistrableDomain.string()).isNewEntry)
        shouldCallNotificationCallback = true;
    if (shouldCallNotificationCallback)
        scheduleNotificationIfNeeded();
#else
    UNUSED_PARAM(document);
    UNUSED_PARAM(functionName);
#endif
}
    
void WebResourceLoadObserver::logScreenAPIAccessed(const Document& document, const ResourceLoadStatistics::ScreenAPI functionName)
{
    if (isEphemeral())
        return;

#if ENABLE(WEB_API_STATISTICS)
    RegistrableDomain registrableDomain { document.url() };
    auto& statistics = ensureResourceStatisticsForRegistrableDomain(registrableDomain);
    bool shouldCallNotificationCallback = false;
    if (!statistics.screenFunctionsAccessed.contains(functionName)) {
        statistics.screenFunctionsAccessed.add(functionName);
        shouldCallNotificationCallback = true;
    }
    RegistrableDomain mainFrameRegistrableDomain { document.topDocument().url() };
    if (statistics.topFrameRegistrableDomainsWhichAccessedWebAPIs.add(mainFrameRegistrableDomain.string()).isNewEntry)
        shouldCallNotificationCallback = true;
    if (shouldCallNotificationCallback)
        scheduleNotificationIfNeeded();
#else
    UNUSED_PARAM(document);
    UNUSED_PARAM(functionName);
#endif
}

void WebResourceLoadObserver::logSubresourceLoading(const Frame* frame, const ResourceRequest& newRequest, const ResourceResponse& redirectResponse, FetchDestinationIsScriptLike isScriptLike)
{
    if (isEphemeral())
        return;

    ASSERT(frame->page());

    if (!frame)
        return;

    auto* page = frame->page();
    if (!page)
        return;

    bool isRedirect = is3xxRedirect(redirectResponse);
    const URL& redirectedFromURL = redirectResponse.url();
    const URL& targetURL = newRequest.url();
    const URL& topFrameURL = frame ? frame->mainFrame().document()->url() : URL();
    
    auto targetHost = targetURL.host();
    auto topFrameHost = topFrameURL.host();

    if (targetHost.isEmpty() || topFrameHost.isEmpty() || targetHost == topFrameHost || (isRedirect && targetHost == redirectedFromURL.host()))
        return;

    RegistrableDomain targetDomain { targetURL };
    RegistrableDomain topFrameDomain { topFrameURL };
    RegistrableDomain redirectedFromDomain { redirectedFromURL };

    if (targetDomain == topFrameDomain || (isRedirect && targetDomain == redirectedFromDomain))
        return;

    {
        auto& targetStatistics = ensureResourceStatisticsForRegistrableDomain(targetDomain);
        auto lastSeen = ResourceLoadStatistics::reduceTimeResolution(WallTime::now());
        targetStatistics.lastSeen = lastSeen;
        targetStatistics.subresourceUnderTopFrameDomains.add(topFrameDomain);

        scheduleNotificationIfNeeded();
    }

    if (frame->isMainFrame() && isScriptLike == FetchDestinationIsScriptLike::Yes) {
        auto& topFrameStatistics = ensureResourceStatisticsForRegistrableDomain(topFrameDomain);
        topFrameStatistics.topFrameLoadedThirdPartyScripts.add(targetDomain);

        scheduleNotificationIfNeeded();
    }

    if (isRedirect) {
        auto& redirectingOriginStatistics = ensureResourceStatisticsForRegistrableDomain(redirectedFromDomain);
        redirectingOriginStatistics.subresourceUniqueRedirectsTo.add(targetDomain);
        auto& targetStatistics = ensureResourceStatisticsForRegistrableDomain(targetDomain);
        targetStatistics.subresourceUniqueRedirectsFrom.add(redirectedFromDomain);

        scheduleNotificationIfNeeded();
    }
}

void WebResourceLoadObserver::logWebSocketLoading(const URL& targetURL, const URL& mainFrameURL)
{
    if (isEphemeral())
        return;

    auto targetHost = targetURL.host();
    auto mainFrameHost = mainFrameURL.host();
    
    if (targetHost.isEmpty() || mainFrameHost.isEmpty() || targetHost == mainFrameHost)
        return;
    
    RegistrableDomain targetDomain { targetURL };
    RegistrableDomain topFrameDomain { mainFrameURL };

    if (targetDomain == topFrameDomain)
        return;

    auto lastSeen = ResourceLoadStatistics::reduceTimeResolution(WallTime::now());

    auto& targetStatistics = ensureResourceStatisticsForRegistrableDomain(targetDomain);
    targetStatistics.lastSeen = lastSeen;
    targetStatistics.subresourceUnderTopFrameDomains.add(topFrameDomain);

    scheduleNotificationIfNeeded();
}

void WebResourceLoadObserver::logUserInteractionWithReducedTimeResolution(const Document& document)
{
    auto& url = document.url();
    if (url.protocolIsAbout() || url.isLocalFile() || url.isEmpty())
        return;

    RegistrableDomain topFrameDomain { url };
    auto newTime = ResourceLoadStatistics::reduceTimeResolution(WallTime::now());
    auto lastReportedUserInteraction = m_lastReportedUserInteractionMap.get(topFrameDomain);
    if (newTime == lastReportedUserInteraction)
        return;

    m_lastReportedUserInteractionMap.set(topFrameDomain, newTime);

    if (!isEphemeral()) {
        auto& statistics = ensureResourceStatisticsForRegistrableDomain(topFrameDomain);
        statistics.hadUserInteraction = true;
        statistics.lastSeen = newTime;
        statistics.mostRecentUserInteractionTime = newTime;
    }

    if (auto* frame = document.frame()) {
        if (auto* opener = frame->loader().opener()) {
            if (auto* openerDocument = opener->document()) {
                if (auto* openerPage = openerDocument->page())
                    requestStorageAccessUnderOpener(topFrameDomain, WebPage::fromCorePage(*openerPage), *openerDocument);
            }
        }
    }

    // We notify right away in case of a user interaction instead of waiting the usual 5 seconds because we want
    // to update cookie blocking state as quickly as possible.
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::LogUserInteraction(topFrameDomain), 0);

#if !RELEASE_LOG_DISABLED
    if (shouldLogUserInteraction) {
        auto counter = ++m_loggingCounter;
#define LOCAL_LOG(str, ...) \
        RELEASE_LOG(ResourceLoadStatistics, "ResourceLoadObserver::logUserInteraction: counter = %" PRIu64 ": " str, counter, ##__VA_ARGS__)

        auto escapeForJSON = [](String s) {
            s.replace('\\', "\\\\").replace('"', "\\\"");
            return s;
        };
        auto escapedURL = escapeForJSON(url.string());
        auto escapedDomain = escapeForJSON(topFrameDomain.string());

        LOCAL_LOG(R"({ "url": "%{public}s",)", escapedURL.utf8().data());
        LOCAL_LOG(R"(  "domain" : "%{public}s",)", escapedDomain.utf8().data());
        LOCAL_LOG(R"(  "until" : %f })", newTime.secondsSinceEpoch().seconds());

#undef LOCAL_LOG
    }
#endif
}

void WebResourceLoadObserver::logSubresourceLoadingForTesting(const RegistrableDomain& firstPartyDomain, const RegistrableDomain& thirdPartyDomain, bool shouldScheduleNotification)
{
    if (isEphemeral())
        return;

    auto& targetStatistics = ensureResourceStatisticsForRegistrableDomain(thirdPartyDomain);
    auto lastSeen = ResourceLoadStatistics::reduceTimeResolution(WallTime::now());
    targetStatistics.lastSeen = lastSeen;
    targetStatistics.subresourceUnderTopFrameDomains.add(firstPartyDomain);

    if (shouldScheduleNotification)
        scheduleNotificationIfNeeded();
    else
        m_notificationTimer.stop();
}

} // namespace WebKit

#endif // ENABLE(RESOURCE_LOAD_STATISTICS)

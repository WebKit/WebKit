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

#pragma once

#if ENABLE(RESOURCE_LOAD_STATISTICS)

#include <WebCore/PageIdentifier.h>
#include <WebCore/ResourceLoadObserver.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <WebCore/Timer.h>
#include <wtf/Forward.h>

namespace WebKit {

class WebPage;

class WebResourceLoadObserver final : public WebCore::ResourceLoadObserver {
public:
    WebResourceLoadObserver(WebCore::ResourceLoadStatistics::IsEphemeral);
    ~WebResourceLoadObserver();

    void logSubresourceLoading(const WebCore::Frame*, const WebCore::ResourceRequest& newRequest, const WebCore::ResourceResponse& redirectResponse, FetchDestinationIsScriptLike) final;
    void logWebSocketLoading(const URL& targetURL, const URL& mainFrameURL) final;
    void logUserInteractionWithReducedTimeResolution(const WebCore::Document&) final;
    void logFontLoad(const WebCore::Document&, const String& familyName, bool loadStatus) final;
    void logCanvasRead(const WebCore::Document&) final;
    void logCanvasWriteOrMeasure(const WebCore::Document&, const String& textWritten) final;
    void logNavigatorAPIAccessed(const WebCore::Document&, const WebCore::ResourceLoadStatistics::NavigatorAPI) final;
    void logScreenAPIAccessed(const WebCore::Document&, const WebCore::ResourceLoadStatistics::ScreenAPI) final;
    void logSubresourceLoadingForTesting(const WebCore::RegistrableDomain& firstPartyDomain, const WebCore::RegistrableDomain& thirdPartyDomain, bool shouldScheduleNotification);

#if !RELEASE_LOG_DISABLED
    static void setShouldLogUserInteraction(bool);
#endif

    String statisticsForURL(const URL&) final;
    void updateCentralStatisticsStore(CompletionHandler<void()>&&) final;
    void clearState() final;
    
    bool hasStatistics() const final { return !m_resourceStatisticsMap.isEmpty(); }

    void setDomainsWithUserInteraction(HashSet<WebCore::RegistrableDomain>&& domains) final { m_domainsWithUserInteraction = WTFMove(domains); }
    bool hasHadUserInteraction(const WebCore::RegistrableDomain&) const final;
private:
    WebCore::ResourceLoadStatistics& ensureResourceStatisticsForRegistrableDomain(const WebCore::RegistrableDomain&);
    void scheduleNotificationIfNeeded();

    Vector<WebCore::ResourceLoadStatistics> takeStatistics();
    void requestStorageAccessUnderOpener(const WebCore::RegistrableDomain& domainInNeedOfStorageAccess, WebPage& openerPage, WebCore::Document& openerDocument);

    bool isEphemeral() const { return m_isEphemeral == WebCore::ResourceLoadStatistics::IsEphemeral::Yes; }

    WebCore::ResourceLoadStatistics::IsEphemeral m_isEphemeral { WebCore::ResourceLoadStatistics::IsEphemeral::No };

    HashMap<WebCore::RegistrableDomain, WebCore::ResourceLoadStatistics> m_resourceStatisticsMap;
    HashMap<WebCore::RegistrableDomain, WTF::WallTime> m_lastReportedUserInteractionMap;

    WebCore::Timer m_notificationTimer;

    HashSet<WebCore::RegistrableDomain> m_domainsWithUserInteraction;
#if !RELEASE_LOG_DISABLED
    uint64_t m_loggingCounter { 0 };
    static bool shouldLogUserInteraction;
#endif
};

} // namespace WebKit

#endif // ENABLE(RESOURCE_LOAD_STATISTICS)

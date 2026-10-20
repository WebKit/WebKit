/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "BrowsingContextGroup.h"

#include "APIPageConfiguration.h"
#include "APIWebsitePolicies.h"
#include "EnhancedSecurity.h"
#include "FrameProcess.h"
#include "Logging.h"
#include "PageLoadState.h"
#include "ProvisionalPageProxy.h"
#include "RemotePageProxy.h"
#include "WebFrameProxy.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include <WebCore/IPAddressSpace.h>
#include <WebCore/SecurityOrigin.h>

#define BROWSINGCONTEXTGROUP_RELEASE_LOG(fmt, ...) RELEASE_LOG(SiteIsolation, "%p - BrowsingContextGroup::" fmt, this, ##__VA_ARGS__)

namespace WebKit {

using namespace WebCore;

BrowsingContextGroup::BrowsingContextGroup(CrossOriginMode crossOriginMode)
    : m_crossOriginMode(crossOriginMode)
{
}

BrowsingContextGroup::~BrowsingContextGroup() = default;

static bool isLoopbackOrLocalNetworkSite(const Site& site, bool localNetworkAccessEnabled)
{
    if (localNetworkAccessEnabled && determineIPAddressSpace(site) != IPAddressSpace::Public)
        return true;
    return SecurityOrigin::isLocalHostOrLoopbackIPAddress(site.domain().string());
}

void BrowsingContextGroup::sharedProcessForSite(WebsiteDataStore& websiteDataStore, API::WebsitePolicies* websitePolicies, const WebPreferences& preferences, const WebCore::Site& site, const WebCore::Site& mainFrameSite,
    WebProcessProxy::LockdownMode lockdownMode, EnhancedSecurity enhancedSecurity, API::PageConfiguration& pageConfiguration, IsMainFrame isMainFrame, CompletionHandler<void(FrameProcess*)>&& completionHandler)
{
    if (!preferences.siteIsolationEnabled() || !preferences.siteIsolationSharedProcessEnabled())
        return completionHandler(nullptr);

    if (site.isEmpty() || hasProcessForSite(site) || m_crossOriginMode == CrossOriginMode::Isolated)
        return completionHandler(nullptr);

    if (isLoopbackOrLocalNetworkSite(site, preferences.localNetworkAccessEnabled()))
        return completionHandler(nullptr);

    RefPtr existingSharedProcess = liveSharedProcess();

    if (!m_sharedProcessSites.contains(site)) {
        if (isMainFrame == IsMainFrame::Yes)
            return completionHandler(nullptr);
        if (websitePolicies && websitePolicies->prefersIsolatedProcess())
            return completionHandler(nullptr);

        // Placement is not revisited, so deciding early would strand a site promoted in an earlier
        // session in the shared process for the rest of this one.
        Ref isolatedSiteStore = websiteDataStore.isolatedSiteStore();
        if (!isolatedSiteStore->isReady()) {
            return isolatedSiteStore->whenReady([
                protectedThis = Ref { *this },
                websiteDataStore = protect(websiteDataStore),
                websitePolicies = RefPtr { websitePolicies },
                preferences = protect(preferences),
                site = Site { site },
                mainFrameSite = Site { mainFrameSite },
                lockdownMode,
                enhancedSecurity,
                pageConfiguration = protect(pageConfiguration),
                isMainFrame,
                completionHandler = WTF::move(completionHandler)
            ] mutable {
                protectedThis->sharedProcessForSite(websiteDataStore, websitePolicies.get(), preferences, site, mainFrameSite, lockdownMode, enhancedSecurity, pageConfiguration, isMainFrame, WTF::move(completionHandler));
            });
        }

        if (isolatedSiteStore->contains(site))
            return completionHandler(nullptr);
    }

    m_sharedProcessSites.add(site);

    if (existingSharedProcess) {
        ASSERT(existingSharedProcess->isSharedProcess());
        RELEASE_ASSERT(!existingSharedProcess->process().isInProcessCache());
        existingSharedProcess->process().addSharedProcessDomain(site.domain());
        BROWSINGCONTEXTGROUP_RELEASE_LOG("sharedProcessForSite: site %" SENSITIVE_LOG_STRING " joined shared process %d, which now hosts %u sites", site.loggingString().utf8(), existingSharedProcess->process().processID(), m_sharedProcessSites.size());
        return completionHandler(existingSharedProcess.get());
    }

    Ref process = protect(pageConfiguration.processPool())->processForSite(websiteDataStore, WebProcessProxy::IsolatedProcessType::Shared, site, mainFrameSite, lockdownMode, enhancedSecurity, pageConfiguration, ProcessSwapDisposition::Other, processCrossOriginIsolationForBrowsingContextGroup(m_crossOriginMode));
    ASSERT(!process->isInProcessCache());
    Ref frameProcess = FrameProcess::create(process, *this, std::nullopt, mainFrameSite, preferences, LoadedWebArchive::No, BrowsingContextGroupUpdate::AddProcessAndInjectBrowsingContext);
    ASSERT(frameProcess->isSharedProcess());
    ASSERT(frameProcess->process().isSharedProcess());
    frameProcess->process().addSharedProcessDomain(site.domain());
    m_sharedProcess = frameProcess.ptr();
    BROWSINGCONTEXTGROUP_RELEASE_LOG("sharedProcessForSite: created shared process %d for site %" SENSITIVE_LOG_STRING, process->processID(), site.loggingString().utf8());
    completionHandler(frameProcess.ptr());
}

Ref<FrameProcess> BrowsingContextGroup::ensureProcessForSite(const Site& site, const Site& mainFrameSite, WebProcessProxy& process, const WebPreferences& preferences, LoadedWebArchive loadedWebArchive, BrowsingContextGroupUpdate browsingContextGroupUpdate)
{
    ASSERT(process.isDummyProcessProxy() || process.crossOriginMode() == m_crossOriginMode || (process.isDocumentIsolationPolicyProcess() && m_crossOriginMode == CrossOriginMode::Shared));

    if (preferences.siteIsolationEnabled()) {
        RefPtr sharedProcess = liveSharedProcess();
        if (sharedProcess && !process.isDocumentIsolationPolicyProcess() && (m_sharedProcessSites.contains(site) || process.isSharedProcess())) {
            ASSERT(&sharedProcess->process() == &process);
            if (m_sharedProcessSites.add(site).isNewEntry) {
                process.addSharedProcessDomain(site.domain());
                BROWSINGCONTEXTGROUP_RELEASE_LOG("ensureProcessForSite: site %" SENSITIVE_LOG_STRING " joined shared process %d, which now hosts %u sites", site.loggingString().utf8(), process.processID(), m_sharedProcessSites.size());
            }
            return sharedProcess.releaseNonNull();
        }
        if (RefPtr existingProcess = processForSite(site, process.crossOriginIsolation())) {
            if (existingProcess->process().coreProcessIdentifier() == process.coreProcessIdentifier())
                return existingProcess.releaseNonNull();
        }
    }

    return FrameProcess::create(process, *this, site, mainFrameSite, preferences, loadedWebArchive, browsingContextGroupUpdate);
}

RefPtr<FrameProcess> BrowsingContextGroup::liveSharedProcess()
{
    RefPtr sharedProcess = m_sharedProcess.get();
    if (!sharedProcess)
        return nullptr;
    if (sharedProcess->process().state() != WebProcessProxy::State::Terminated)
        return sharedProcess;
    clearSharedProcess();
    return nullptr;
}

void BrowsingContextGroup::clearSharedProcess()
{
    m_sharedProcess = nullptr;
    m_sharedProcessSites.clear();
    m_pagesInSharedProcess.clear();
}

auto BrowsingContextGroup::processMapKey(const FrameProcess& process) const -> ProcessMapKey
{
    bool isDocumentIsolationPolicyProcess = process.process().isDocumentIsolationPolicyProcess();
    ASSERT(!isDocumentIsolationPolicyProcess || m_crossOriginMode == CrossOriginMode::Shared);
    return { *process.site(), isDocumentIsolationPolicyProcess ? IsDocumentIsolationPolicyProcess::Yes : IsDocumentIsolationPolicyProcess::No };
}

bool BrowsingContextGroup::hasProcessForSite(const Site& site) const
{
    return m_processMap.contains({ site, IsDocumentIsolationPolicyProcess::No }) || m_processMap.contains({ site, IsDocumentIsolationPolicyProcess::Yes });
}

RefPtr<FrameProcess> BrowsingContextGroup::processForSite(const Site& site)
{
    return processForSite(site, processCrossOriginIsolationForBrowsingContextGroup(m_crossOriginMode));
}

RefPtr<FrameProcess> BrowsingContextGroup::processForSite(const Site& site, ProcessCrossOriginIsolation crossOriginIsolation)
{
    auto isDocumentIsolationPolicyProcess = crossOriginIsolation == ProcessCrossOriginIsolation::DocumentIsolationPolicy ? IsDocumentIsolationPolicyProcess::Yes : IsDocumentIsolationPolicyProcess::No;
    RefPtr<FrameProcess> process;
    if (isDocumentIsolationPolicyProcess == IsDocumentIsolationPolicyProcess::No && m_sharedProcessSites.contains(site))
        process = liveSharedProcess();
    else
        process = m_processMap.get({ site, isDocumentIsolationPolicyProcess });
    if (!process)
        return nullptr;
    if (process->process().state() == WebProcessProxy::State::Terminated)
        return nullptr;
    return process;
}

void BrowsingContextGroup::processDidTerminate(WebPageProxy& page, WebProcessProxy& process)
{
    if (&page.siteIsolatedProcess() != &process)
        return;
    m_pages.remove(page);
    closeRemotePagesForPage(page);
}

void BrowsingContextGroup::addFrameProcess(FrameProcess& process)
{
    addFrameProcessAndInjectPageContextIf(process, [](auto&) {
        return true;
    });
}

void BrowsingContextGroup::addFrameProcessAndInjectPageContextIf(FrameProcess& process, Function<bool(WebPageProxy&)> functor)
{
    Ref processProxy = process.process();
    auto createRemotePageIfNeeded = [&](WebPageProxy& page, const Site& site) {
        if (!functor(page))
            return;

        // The process is hosting main frame so it does not need remote page.
        if (page.legacyMainFrameProcess().coreProcessIdentifier() == processProxy->coreProcessIdentifier())
            return;

        auto& set = m_remotePages.ensure(page, [] {
            return HashSet<Ref<RemotePageProxy>> { };
        }).iterator->value;
        Ref newRemotePage = RemotePageProxy::create(page, processProxy, site);
#if ASSERT_ENABLED
        for (auto& existingPage : set) {
            ASSERT(existingPage->process().coreProcessIdentifier() != newRemotePage->process().coreProcessIdentifier() || existingPage->site() != newRemotePage->site());
            ASSERT(existingPage->page() == newRemotePage->page());
        }
#endif

        // Register before injecting, so creation parameters can resolve this page's identifier in the
        // new process via webPageIDInProcess().
        set.add(newRemotePage.copyRef());
        newRemotePage->injectPageIntoNewProcess();
    };

    if (process.isSharedProcess()) {
        // RemotePageProxy currently requires a site, but it has no meaning under shared process mode since
        // one process/RemotePageProxy can represent multiple sites, so we just pick the first site as a
        // placeholder value.
        auto& representativeSite = *m_sharedProcessSites.begin();
        for (Ref page : m_pages) {
            if (!m_pagesInSharedProcess.add(page).isNewEntry)
                continue;
            createRemotePageIfNeeded(page, representativeSite);
        }
        return;
    }
    if (!addFrameProcessWithoutInjectingPageContext(process))
        return;
    auto& site = *process.site();
    for (Ref page : m_pages) {
        // Under site isolation, a same-site page should be hosted in the same process and
        // shouldn't need a remote page. Empty sites are the exception, as they can compare equal
        // for pages that are not actually co-located in the same process, and so are documents
        // with a Document-Isolation-Policy, which have their own process.
        bool sameSite = site == Site(URL(page->currentURL()));
        RefPtr mainFrame = page->mainFrame();
        if (sameSite && !site.isEmpty() && mainFrame && mainFrame->process().isDocumentIsolationPolicyProcess() == processProxy->isDocumentIsolationPolicyProcess())
            continue;
        createRemotePageIfNeeded(page, site);
    }
}

#if ASSERT_ENABLED
// A Site maps to exactly one FrameProcess at a time, so the entry registered for a site may only be
// replaced when the previous FrameProcess is on its way out.
// In addition to the obvious terminated case, sites with an empty registrable domain
// (e.g. data:, blob:, file:) all collapse to the same key in m_processMap, so they
// never represented a unique site-to-process binding; replacing them is benign.
// Otherwise the previous FrameProcess must be used by at most the one frame the navigation causing
// this replacement is replacing, so it is destroyed when that navigation commits. Navigations that
// would move a site away from a FrameProcess other frames still need are declined before they get
// this far (see canMoveSiteToEnhancedSecurityProcess in WebPageProxy.cpp).
static bool canReplaceFrameProcessInProcessMap(const WebCore::Site& site, const FrameProcess& existing)
{
    if (existing.process().state() == WebProcessProxy::State::Terminated)
        return true;
    if (site.isEmpty())
        return true;
    return existing.frameCount() <= 1;
}
#endif

bool BrowsingContextGroup::addFrameProcessWithoutInjectingPageContext(FrameProcess& process)
{
    auto key = processMapKey(process);
    if (m_processMap.get(key) == &process)
        return false;
    ASSERT(!m_processMap.get(key) || canReplaceFrameProcessInProcessMap(key.first, *m_processMap.get(key)));
    m_processMap.set(WTF::move(key), process);
    return true;
}

void BrowsingContextGroup::removeFrameProcess(FrameProcess& process)
{
    if (process.isSharedProcess()) {
        if (m_sharedProcess.get() == &process)
            clearSharedProcess();
    } else {
        // A later navigation may already have replaced this entry (see
        // canReplaceFrameProcessInProcessMap), in which case the site now belongs to a different
        // FrameProcess and this teardown must leave it alone.
        auto key = processMapKey(process);
        if (m_processMap.get(key) == &process)
            m_processMap.remove(key);
    }
    m_remotePages.removeIf([&] (auto& pair) {
        auto& set = pair.value;
        set.removeIf([&] (auto& remotePage) {
            if (remotePage->process().coreProcessIdentifier() != process.process().coreProcessIdentifier())
                return false;
            remotePage->disconnect();
            return true;
        });
        return set.isEmpty();
    });
}

void BrowsingContextGroup::addPage(WebPageProxy& page)
{
    if (m_pages.contains(page)) {
        // This only happens when restoring a page from a suspended BCG, which holds exactly this one page.
        ASSERT(!hasMultiplePages());
        return;
    }
    m_pages.add(page);
    auto& set = m_remotePages.ensure(page, [] {
        return HashSet<Ref<RemotePageProxy>> { };
    }).iterator->value;

    auto createRemotePageIfNeeded = [&, page = Ref { page }](WebProcessProxy& processProxy, const Site& site) {
        // A page whose main frame is already hosted directly by this process doesn't need a
        // RemotePageProxy to reach itself.
        if (page->legacyMainFrameProcess().coreProcessIdentifier() == processProxy.coreProcessIdentifier())
            return;
        Ref newRemotePage = RemotePageProxy::create(page, processProxy, site);
#if ASSERT_ENABLED
        for (auto& existingPage : set) {
            ASSERT(existingPage->process().coreProcessIdentifier() != newRemotePage->process().coreProcessIdentifier() || existingPage->site() != newRemotePage->site());
            ASSERT(existingPage->page() == newRemotePage->page());
        }
#endif
        set.add(newRemotePage.copyRef());
        newRemotePage->injectPageIntoNewProcess();
    };

    if (RefPtr sharedProcess = liveSharedProcess(); sharedProcess && m_pagesInSharedProcess.add(page).isNewEntry)
        createRemotePageIfNeeded(sharedProcess->process(), *m_sharedProcessSites.begin());

    m_processMap.removeIf([&](auto& pair) {
        auto& site = pair.key.first;
        auto& process = pair.value;
        if (!process) {
            ASSERT_NOT_REACHED_WITH_MESSAGE("FrameProcess should remove itself in the destructor so we should never find a null WeakPtr");
            return true;
        }

        createRemotePageIfNeeded(protect(process->process()), site);
        return false;
    });
}

void BrowsingContextGroup::addRemotePage(WebPageProxy& page, Ref<RemotePageProxy>&& remotePage)
{
    m_remotePages.ensure(page, [] {
        return HashSet<Ref<RemotePageProxy>> { };
    }).iterator->value.add(WTF::move(remotePage));
}

void BrowsingContextGroup::removePage(WebPageProxy& page, ShouldSendCloseMessage shouldSendCloseMessage)
{
    m_pages.remove(page);
    closeRemotePagesForPage(page, shouldSendCloseMessage);
}

void BrowsingContextGroup::closeRemotePagesForPage(WebPageProxy& page, ShouldSendCloseMessage shouldSendCloseMessage)
{
    for (auto& remotePage : m_remotePages.take(page))
        protect(remotePage)->disconnect(shouldSendCloseMessage);
}

bool BrowsingContextGroup::hasMultiplePages() const
{
    return m_pages.computeSize() > 1;
}

bool BrowsingContextGroup::hasVisiblePage() const
{
    for (Ref page : m_pages) {
        if (page->isViewVisible())
            return true;
    }
    return false;
}

void BrowsingContextGroup::forEachRemotePage(const WebPageProxy& page, NOESCAPE const Function<void(RemotePageProxy&)>& function)
{
    auto it = m_remotePages.find(page);
    if (it == m_remotePages.end())
        return;
    for (Ref remotePage : it->value)
        function(remotePage);
}

RefPtr<RemotePageProxy> BrowsingContextGroup::remotePageInProcess(const WebPageProxy& page, const WebProcessProxy& process)
{
    auto it = m_remotePages.find(page);
    if (it == m_remotePages.end())
        return nullptr;
    for (auto& remotePage : it->value) {
        if (remotePage->process().coreProcessIdentifier() == process.coreProcessIdentifier())
            return remotePage.ptr();
    }
    return nullptr;
}

RefPtr<RemotePageProxy> BrowsingContextGroup::takeRemotePageInProcessForProvisionalPage(const WebPageProxy& page, const WebProcessProxy& process)
{
    auto it = m_remotePages.find(page);
    if (it == m_remotePages.end())
        return nullptr;
    RefPtr remotePage = remotePageInProcess(page, process);
    if (!remotePage)
        return nullptr;
    return it->value.take(remotePage.get());
}

void BrowsingContextGroup::transitionPageToRemotePage(WebPageProxy& page, const Site& openerSite)
{
    auto& set = m_remotePages.ensure(page, [] {
        return HashSet<Ref<RemotePageProxy>> { };
    }).iterator->value;

    Ref newRemotePage = RemotePageProxy::create(page, protect(page.legacyMainFrameProcess()), openerSite, nullptr, page.webPageIDInMainFrameProcess());
#if ASSERT_ENABLED
    for (auto& existingPage : set) {
        ASSERT(existingPage->process().coreProcessIdentifier() != newRemotePage->process().coreProcessIdentifier() || existingPage->site() != newRemotePage->site());
        ASSERT(existingPage->page() == newRemotePage->page());
    }
#endif
    set.add(WTF::move(newRemotePage));
}

void BrowsingContextGroup::transitionProvisionalPageToRemotePage(ProvisionalPageProxy& page, const Site& provisionalNavigationFailureSite)
{
    auto& set = m_remotePages.ensure(*protect(page.page()), [] {
        return HashSet<Ref<RemotePageProxy>> { };
    }).iterator->value;

    Ref newRemotePage = RemotePageProxy::create(*protect(page.page()), protect(page.process()), provisionalNavigationFailureSite, &page.messageReceiverRegistration(), page.webPageID());
#if ASSERT_ENABLED
    for (auto& existingPage : set) {
        ASSERT(existingPage->process().coreProcessIdentifier() != newRemotePage->process().coreProcessIdentifier() || existingPage->site() != newRemotePage->site());
        ASSERT(existingPage->page() == newRemotePage->page());
    }
#endif
    set.add(WTF::move(newRemotePage));
}

bool BrowsingContextGroup::hasRemotePages(const WebPageProxy& page)
{
    auto it = m_remotePages.find(page);
    return it != m_remotePages.end() && !it->value.isEmpty();
}

// https://html.spec.whatwg.org/multipage/origin.html#historical-agent-cluster-key-map
WebCore::OriginKeyed BrowsingContextGroup::resolveAgentClusterKeying(const WebCore::SecurityOriginData& origin, WebCore::OriginKeyed requested)
{
    return m_historicalAgentClusterKeyMap.ensure(origin, [requested] {
        return requested;
    }).iterator->value;
}

WebCore::OriginKeyed BrowsingContextGroup::knownAgentClusterKeying(const WebCore::SecurityOriginData& origin) const
{
    if (auto keying = m_historicalAgentClusterKeyMap.getOptional(origin))
        return *keying;
    if (RefPtr reservation = m_agentClusterKeyingReservations.get(origin))
        return reservation->keying();
    return WebCore::OriginKeyed::No;
}

RefPtr<AgentClusterKeyingReservation> BrowsingContextGroup::reserveAgentClusterKeying(const WebCore::SecurityOriginData& origin, WebCore::OriginKeyed requested)
{
    if (m_historicalAgentClusterKeyMap.contains(origin))
        return nullptr;
    auto& existingReservation = m_agentClusterKeyingReservations.add(origin, nullptr).iterator->value;
    if (RefPtr reservation = existingReservation)
        return reservation;
    Ref reservation = AgentClusterKeyingReservation::create(*this, origin, requested);
    existingReservation = reservation;
    return reservation;
}

void BrowsingContextGroup::removeAgentClusterKeyingReservation(const AgentClusterKeyingReservation& reservation)
{
    auto it = m_agentClusterKeyingReservations.find(reservation.origin());
    if (it != m_agentClusterKeyingReservations.end() && it->value == &reservation)
        m_agentClusterKeyingReservations.remove(it);
}

// https://html.spec.whatwg.org/multipage/webappapis.html#obtain-similar-origin-window-agent
WebCore::AgentClusterIdentifier BrowsingContextGroup::agentClusterIdentifier(const WebCore::SecurityOriginData& origin, WebCore::OriginKeyed originKeyed, const std::optional<WebCore::SecurityOriginData>& crossOriginIsolationKey)
{
    // The second element is null for a site-keyed agent cluster and the third without a cross-origin isolation key.
    std::tuple<WebCore::Site, WebCore::SecurityOriginData, WebCore::SecurityOriginData> key { WebCore::Site(origin), originKeyed == WebCore::OriginKeyed::Yes || crossOriginIsolationKey ? origin : WebCore::SecurityOriginData { }, crossOriginIsolationKey.value_or(WebCore::SecurityOriginData { }) };
    return m_agentClusterIdentifiers.ensure(WTF::move(key), [] {
        return WebCore::AgentClusterIdentifier::generate();
    }).iterator->value;
}

std::pair<WebCore::AgentClusterAssignment, RefPtr<AgentClusterKeyingReservation>> BrowsingContextGroup::assignAgentCluster(const AgentClusterRequest& request, AgentClusterKeyingRecording recording)
{
    auto& origin = request.origin;
    WebCore::AgentClusterAssignment assignment { std::nullopt, WebCore::OriginKeyed::No, request.documentIsolationPolicy };
    std::optional<WebCore::OriginKeyed> keyingToSettle;
    // https://html.spec.whatwg.org/multipage/origin.html#origin-keyed-agent-clusters
    // A document with a Document-Isolation-Policy is origin-keyed and does not use the historical agent cluster key map.
    if (assignment.documentIsolationPolicy != WebCore::DocumentIsolationPolicy::None)
        assignment.isOriginKeyed = WebCore::OriginKeyed::Yes;
    else {
        switch (request.source) {
        case AgentClusterRequest::Source::NavigationAction:
            assignment.isOriginKeyed = knownAgentClusterKeying(origin);
            break;
        case AgentClusterRequest::Source::SubstituteData:
            // Substitute data gets no response decision, so its keying is settled now.
            if (!origin.isOpaque())
                keyingToSettle = WebCore::OriginKeyed::No;
            break;
        case AgentClusterRequest::Source::NavigationResponse:
            keyingToSettle = request.requestedKeying;
            break;
        }
    }

    RefPtr<AgentClusterKeyingReservation> reservation;
    if (keyingToSettle) {
        if (recording == AgentClusterKeyingRecording::Now)
            assignment.isOriginKeyed = resolveAgentClusterKeying(origin, *keyingToSettle);
        else {
            reservation = reserveAgentClusterKeying(origin, *keyingToSettle);
            assignment.isOriginKeyed = knownAgentClusterKeying(origin);
        }
    }

    // FIXME: Assign opaque origins an agent cluster too. Their responses can then no longer skip the policy decision through the injected bundle.
    // A blob URL created by an opaque origin belongs to its creator's agent cluster, which needs the blob URL entry rather than the URL.
    if (origin.isOpaque())
        return { assignment, WTF::move(reservation) };

    // https://wicg.github.io/document-isolation-policy/#coi-agent-cluster-key
    std::optional<WebCore::SecurityOriginData> crossOriginIsolationKey;
    if (assignment.documentIsolationPolicy != WebCore::DocumentIsolationPolicy::None)
        crossOriginIsolationKey = origin;
    else if (m_crossOriginMode == WebCore::CrossOriginMode::Isolated)
        crossOriginIsolationKey = WebCore::SecurityOriginData::fromURL(request.topLevelCreationURL);
    assignment.identifier = agentClusterIdentifier(origin, assignment.isOriginKeyed, crossOriginIsolationKey);
    return { assignment, WTF::move(reservation) };
}

void BrowsingContextGroup::clearBrowsingContextGroupForTesting()
{
    m_identifier = WebCore::BrowsingContextGroupIdentifier::generate();
    m_historicalAgentClusterKeyMap.clear();
    m_agentClusterKeyingReservations.clear();
    m_agentClusterIdentifiers.clear();
}

} // namespace WebKit

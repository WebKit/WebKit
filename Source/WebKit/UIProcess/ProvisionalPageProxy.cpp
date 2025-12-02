/*
 * Copyright (C) 2019-2025 Apple Inc. All rights reserved.
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
#include "ProvisionalPageProxy.h"

#include "APINavigation.h"
#include "APIWebsitePolicies.h"
#include "BrowsingContextGroup.h"
#include "DrawingAreaProxy.h"
#include "FormDataReference.h"
#include "FrameProcess.h"
#include "GoToBackForwardItemParameters.h"
#include "HandleMessage.h"
#include "LoadedWebArchive.h"
#include "Logging.h"
#include "MessageSenderInlines.h"
#include "NavigationActionData.h"
#include "PageClient.h"
#include "ProvisionalFrameCreationParameters.h"
#include "RemotePageProxy.h"
#include "SuspendedPageProxy.h"
#include "URLSchemeTaskParameters.h"
#include "WebBackForwardCacheEntry.h"
#include "WebBackForwardList.h"
#include "WebBackForwardListCounts.h"
#include "WebBackForwardListFrameItem.h"
#include "WebBackForwardListItem.h"
#include "WebBackForwardListMessages.h"
#include "WebErrors.h"
#include "WebFrameProxy.h"
#include "WebNavigationDataStore.h"
#include "WebNavigationState.h"
#include "WebPageInspectorController.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebPageProxyMessages.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include <WebCore/ShouldTreatAsContinuingLoad.h>
#include <wtf/TZoneMallocInlines.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, process().connection())

namespace WebKit {

using namespace WebCore;

#define PROVISIONALPAGEPROXY_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", PID=%i, navigationID=%" PRIu64 "] ProvisionalPageProxy::" fmt, this, m_page ? m_page->identifier().toUInt64() : 0, m_webPageID.toUInt64(), process().processID(), m_navigationID.toUInt64(), ##__VA_ARGS__)
#define PROVISIONALPAGEPROXY_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%p - [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", PID=%i, navigationID=%" PRIu64 "] ProvisionalPageProxy::" fmt, this, m_page ? m_page->identifier().toUInt64() : 0, m_webPageID.toUInt64(), process().processID(), m_navigationID.toUInt64(), ##__VA_ARGS__)

WTF_MAKE_TZONE_ALLOCATED_IMPL(ProvisionalPageProxy);

ProvisionalPageProxy::ProvisionalPageProxy(WebPageProxy& page, Ref<FrameProcess>&& frameProcess, BrowsingContextGroup& group, RefPtr<SuspendedPageProxy>&& suspendedPage, API::Navigation& navigation, bool isServerRedirect, const WebCore::ResourceRequest& request, ProcessSwapRequestedByClient processSwapRequestedByClient, bool isProcessSwappingOnNavigationResponse, API::WebsitePolicies* websitePolicies, WebsiteDataStore* replacedDataStoreForWebArchiveLoad)
    : m_page(page)
    , m_webPageID(suspendedPage ? suspendedPage->webPageID() : PageIdentifier::generate())
    , m_frameProcess(WTFMove(frameProcess))
    , m_browsingContextGroup(group)
    , m_replacedDataStoreForWebArchiveLoad(replacedDataStoreForWebArchiveLoad)
    , m_navigationID(navigation.navigationID())
    , m_isServerRedirect(isServerRedirect)
    , m_request(request)
    , m_processSwapRequestedByClient(processSwapRequestedByClient)
    , m_isProcessSwappingOnNavigationResponse(isProcessSwappingOnNavigationResponse)
    , m_isProcessSwappingForNewWindow(page.protectedPreferences()->siteIsolationEnabled() && page.openedByDOM())
    , m_provisionalLoadURL(isProcessSwappingOnNavigationResponse ? request.url() : URL())
#if USE(RUNNINGBOARD)
    , m_provisionalLoadActivity(m_frameProcess->process().protectedThrottler()->foregroundActivity("Provisional Load"_s))
#endif
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    , m_contextIDForVisibilityPropagationInWebProcess(suspendedPage ? suspendedPage->contextIDForVisibilityPropagationInWebProcess() : 0)
#if ENABLE(GPU_PROCESS)
    , m_contextIDForVisibilityPropagationInGPUProcess(suspendedPage ? suspendedPage->contextIDForVisibilityPropagationInGPUProcess() : 0)
#endif
#endif
{
    relaxAdoptionRequirement();
    PROVISIONALPAGEPROXY_RELEASE_LOG(ProcessSwapping, "ProvisionalPageProxy: suspendedPage=%p", suspendedPage.get());

    Ref process = this->process();
    m_messageReceiverRegistration.startReceivingMessages(process, m_webPageID, *this, *this);
    process->addProvisionalPageProxy(*this);
    ASSERT(!page.preferences().siteIsolationEnabled() || navigation.processID() == process->coreProcessIdentifier());

    RefPtr dataStore = process->websiteDataStore();
    m_websiteDataStore = dataStore.copyRef();
    ASSERT(dataStore);
    if (dataStore && dataStore != &page.websiteDataStore())
        process->protectedProcessPool()->pageBeginUsingWebsiteDataStore(page, *dataStore);

    RefPtr previousMainFrame = page.mainFrame();

    // If we are reattaching to a SuspendedPage, then the WebProcess' WebPage already exists and
    // we need to initialize m_mainFrame to reflect the fact the the WebProcess' WebPage
    // already exists and already has a main frame.
    if (suspendedPage) {
        ASSERT(&suspendedPage->process() == process.ptr());
        suspendedPage->unsuspend();
        m_mainFrame = suspendedPage->mainFrame();
        m_needsMainFrameObserver = true;
    } else if (m_isProcessSwappingForNewWindow)
        m_mainFrame = page.mainFrame();
    else {
        Ref mainFrame = WebFrameProxy::create(page, m_frameProcess, generateFrameIdentifier(), previousMainFrame->effectiveSandboxFlags(), previousMainFrame->effectiveReferrerPolicy(), previousMainFrame->scrollingMode(), nullptr, IsMainFrame::Yes);
        m_mainFrame = mainFrame.copyRef();

        m_needsMainFrameObserver = true;
        // Restore the main frame's committed URL as some clients may rely on it until the next load is committed.
        mainFrame->frameLoadState().setURL(URL { previousMainFrame->url() });
        previousMainFrame->transferNavigationCallbackToFrame(mainFrame);
    }

    // Normally, notification of a server redirect comes from the WebContent process.
    // If we are process swapping in response to a server redirect then that notification will not come from the new WebContent process.
    // In this case we have the UIProcess synthesize the redirect notification at the appropriate time.
    if (m_isServerRedirect) {
        // FIXME: When <rdar://116203552> is fixed we should not need this case here
        // because main frame redirect messages won't come from the web content process.
        if (page.protectedPreferences()->siteIsolationEnabled() && !m_mainFrame->frameLoadState().provisionalURL().isEmpty())
            m_mainFrame->frameLoadState().didReceiveServerRedirectForProvisionalLoad(URL { m_request.url() });
        else
            m_mainFrame->frameLoadState().didStartProvisionalLoad(URL { m_request.url() });
        page.didReceiveServerRedirectForProvisionalLoadForFrameShared(WTFMove(process), m_mainFrame->frameID(), m_navigationID, WTFMove(m_request), { });
    } else if (previousMainFrame && !previousMainFrame->provisionalURL().isEmpty() && !m_isProcessSwappingForNewWindow) {
        // In case of a process swap after response policy, the didStartProvisionalLoad already happened but the new main frame doesn't know about it
        // so we need to tell it so it can update its provisional URL.
        protectedMainFrame()->didStartProvisionalLoad(URL { previousMainFrame->provisionalURL() });
    }

    initializeWebPage(websitePolicies);
}

ProvisionalPageProxy::~ProvisionalPageProxy()
{
#if PLATFORM(GTK) || PLATFORM(WPE)
    if (m_accessibilityBindCompletionHandler)
        m_accessibilityBindCompletionHandler({ });
#endif

    Ref process = this->process();
    if (RefPtr drawingArea = m_drawingArea)
        drawingArea->stopReceivingMessages(process);

    if (!m_wasCommitted && m_page) {
        Ref page = *m_page;
        page->inspectorController().willDestroyProvisionalPage(*this);

        RefPtr dataStore = process->websiteDataStore();
        if (dataStore && dataStore!= &page->websiteDataStore())
            process->protectedProcessPool()->pageEndUsingWebsiteDataStore(page, *dataStore);

        if (process->hasConnection() && m_shouldClosePage)
            send(Messages::WebPage::Close());
        process->removeVisitedLinkStoreUser(page->visitedLinkStore(), page->identifier());
    }

    process->removeProvisionalPageProxy(*this);
}

WebProcessProxy& ProvisionalPageProxy::process()
{
    return m_frameProcess->process();
}

Ref<WebProcessProxy> ProvisionalPageProxy::protectedProcess()
{
    return process();
}

RefPtr<WebPageProxy> ProvisionalPageProxy::protectedPage() const
{
    return m_page.get();
}

RefPtr<WebFrameProxy> ProvisionalPageProxy::protectedMainFrame() const
{
    return m_mainFrame.copyRef();
}

void ProvisionalPageProxy::processDidTerminate()
{
    PROVISIONALPAGEPROXY_RELEASE_LOG_ERROR(ProcessSwapping, "processDidTerminate:");
    if (RefPtr page = m_page.get())
        page->provisionalProcessDidTerminate();
}

RefPtr<DrawingAreaProxy> ProvisionalPageProxy::takeDrawingArea()
{
    if (RefPtr drawingArea = m_drawingArea)
        drawingArea->stopReceivingMessages(protectedProcess());
    return WTFMove(m_drawingArea);
}

void ProvisionalPageProxy::setNavigation(API::Navigation& navigation)
{
    if (m_navigationID == navigation.navigationID())
        return;

    m_navigationID = navigation.navigationID();
    navigation.setProcessID(process().coreProcessIdentifier());
}

void ProvisionalPageProxy::cancel()
{
    // If the provisional load started, then indicate that it failed due to cancellation by calling didFailProvisionalLoadForFrame().
    RefPtr mainFrame = m_mainFrame;
    if (m_provisionalLoadURL.isEmpty() || !mainFrame)
        return;

    ASSERT(process().state() == WebProcessProxy::State::Running);

    PROVISIONALPAGEPROXY_RELEASE_LOG(ProcessSwapping, "cancel: Simulating a didFailProvisionalLoadForFrame");
    ASSERT(mainFrame);
    auto error = WebKit::cancelledError(m_request);
    error.setType(WebCore::ResourceError::Type::Cancellation);
    FrameInfoData frameInfo {
        true, // isMainFrame
        FrameType::Local,
        m_request,
        SecurityOriginData::fromURLWithoutStrictOpaqueness(m_request.url()),
        { },
        mainFrame->frameID(),
        m_page ? std::optional { m_page->identifier() } : std::nullopt,
        std::nullopt,
        std::nullopt,
        { },
        mainFrame->processID(),
        mainFrame->isFocused(),
    };
    didFailProvisionalLoadForFrame(WTFMove(frameInfo), ResourceRequest { m_request }, m_navigationID, String { m_provisionalLoadURL.string() }, WTFMove(error), WebCore::WillContinueLoading::No, UserData { }, WebCore::WillInternallyHandleFailure::No); // Will delete |this|.
}

void ProvisionalPageProxy::initializeWebPage(RefPtr<API::WebsitePolicies>&& websitePolicies)
{
    Ref page = *m_page;
    Ref process = this->process();
    Ref preferences = page->preferences();

    RefPtr drawingArea = page->protectedPageClient()->createDrawingAreaProxy(process);
    drawingArea->startReceivingMessages(process);
    m_drawingArea = drawingArea.copyRef();

    bool registerWithInspectorController { true };
    if (websitePolicies)
        m_mainFrameWebsitePolicies = websitePolicies->copy();

    if (preferences->siteIsolationEnabled()) {
        if (RefPtr existingRemotePageProxy = m_browsingContextGroup->takeRemotePageInProcessForProvisionalPage(page, process)) {
            if (m_isProcessSwappingForNewWindow) {
                m_webPageID = existingRemotePageProxy->pageID();
                m_mainFrame = existingRemotePageProxy->page()->mainFrame();
                m_needsMainFrameObserver = false;
                m_messageReceiverRegistration.stopReceivingMessages();
                m_messageReceiverRegistration.transferMessageReceivingFrom(existingRemotePageProxy->messageReceiverRegistration(), *this, *this);
                existingRemotePageProxy->setDrawingArea(nullptr);
                m_needsDidStartProvisionalLoad = false;
                m_needsCookieAccessAddedInNetworkProcess = true;
                registerWithInspectorController = false; // FIXME: <rdar://121240770> This is a hack. There seems to be a bug in our interaction with WebPageInspectorController.
            } else
                m_takenRemotePage = WTFMove(existingRemotePageProxy);
        }
        m_browsingContextGroup->addFrameProcessAndInjectPageContextIf(m_frameProcess, [m_page = m_page](WebPageProxy& page) {
            return m_page != &page;
        });
    }

    RefPtr mainFrame = m_mainFrame;
    auto creationParameters = page->creationParametersForProvisionalPage(process, *drawingArea, mainFrame->frameID());
    if (preferences->siteIsolationEnabled()) {
        creationParameters.remotePageParameters = RemotePageParameters {
            m_request.url(),
            mainFrame->frameTreeCreationParameters(),
            websitePolicies ? std::optional(websitePolicies->dataForProcess(process)) : std::nullopt
        };
        creationParameters.provisionalFrameCreationParameters = mainFrame->provisionalFrameCreationParameters(
            page->mainFrame() && !m_isProcessSwappingForNewWindow ? std::optional(page->mainFrame()->frameID()) : std::nullopt,
            CommitTiming::WaitForLoad
        );
    }
    process->send(Messages::WebProcess::CreateWebPage(m_webPageID, WTFMove(creationParameters)), 0);
    if (!preferences->siteIsolationEnabled())
        process->addVisitedLinkStoreUser(page->visitedLinkStore(), page->identifier());

    if (page->isLayerTreeFrozenDueToSwipeAnimation())
        send(Messages::WebPage::SwipeAnimationDidStart());

    if (registerWithInspectorController)
        page->inspectorController().didCreateProvisionalPage(*this);
}

void ProvisionalPageProxy::loadData(API::Navigation& navigation, Ref<WebCore::SharedBuffer>&& data, const String& mimeType, const String& encoding, const String& baseURL, API::Object* userData, WebCore::ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, RefPtr<API::WebsitePolicies>&& websitePolicies, SubstituteData::SessionHistoryVisibility sessionHistoryVisibility)
{
    PROVISIONALPAGEPROXY_RELEASE_LOG(ProcessSwapping, "loadData:");
    ASSERT(shouldTreatAsContinuingLoad != WebCore::ShouldTreatAsContinuingLoad::No);

    if (RefPtr page = m_page.get())
        page->loadDataWithNavigationShared(protectedProcess(), m_webPageID, navigation, WTFMove(data), mimeType, encoding, baseURL, userData, shouldTreatAsContinuingLoad, isNavigatingToAppBoundDomain, WTFMove(websitePolicies), navigation.shouldOpenExternalURLsPolicy(), sessionHistoryVisibility);
}

void ProvisionalPageProxy::loadRequest(API::Navigation& navigation, WebCore::ResourceRequest&& request, API::Object* userData, WebCore::ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, RefPtr<API::WebsitePolicies>&& websitePolicies, std::optional<NetworkResourceLoadIdentifier> existingNetworkResourceLoadIdentifierToResume, IsPerformingHTTPFallback isPerformingHTTPFallback)
{
    PROVISIONALPAGEPROXY_RELEASE_LOG(ProcessSwapping, "loadRequest: existingNetworkResourceLoadIdentifierToResume=%" PRIu64, existingNetworkResourceLoadIdentifierToResume ? existingNetworkResourceLoadIdentifierToResume->toUInt64() : 0);
    ASSERT(shouldTreatAsContinuingLoad != WebCore::ShouldTreatAsContinuingLoad::No);

    // If this is a client-side redirect continuing in a new process, then the new process will overwrite the fromItem's URL. In this case,
    // we need to make sure we update fromItem's processIdentifier as we want future navigations to this BackForward item to happen in the
    // new process.
    if (navigation.fromItem() && navigation.lockBackForwardList() == WebCore::LockBackForwardList::Yes)
        navigation.fromItem()->setLastProcessIdentifier(process().coreProcessIdentifier());

    if (RefPtr page = m_page.get())
        page->loadRequestWithNavigationShared(protectedProcess(), m_webPageID, navigation, WTFMove(request), navigation.shouldOpenExternalURLsPolicy(), isPerformingHTTPFallback, userData, shouldTreatAsContinuingLoad, isNavigatingToAppBoundDomain, WTFMove(websitePolicies), existingNetworkResourceLoadIdentifierToResume);
}

void ProvisionalPageProxy::goToBackForwardItem(API::Navigation& navigation, WebBackForwardListItem& item, RefPtr<API::WebsitePolicies>&& websitePolicies, WebCore::ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad, std::optional<NetworkResourceLoadIdentifier> existingNetworkResourceLoadIdentifierToResume, WebCore::ProcessSwapDisposition processSwapDisposition)
{
    PROVISIONALPAGEPROXY_RELEASE_LOG(ProcessSwapping, "goToBackForwardItem: existingNetworkResourceLoadIdentifierToResume=%" PRIu64, existingNetworkResourceLoadIdentifierToResume ? existingNetworkResourceLoadIdentifierToResume->toUInt64() : 0);

    RefPtr page = m_page.get();
    if (!page)
        return;

    // FIXME: This is a static analysis false positive. The lamda passed to `setItemsAsRestoredFromSessionIf()` is marked as NOESCAPE so capturing
    // `this` is actually safe.
    Ref backForwardList = page->backForwardList();
    SUPPRESS_UNCOUNTED_LAMBDA_CAPTURE backForwardList->setItemsAsRestoredFromSessionIf([this, targetItem = Ref { item }](auto& item) {
        if (auto* backForwardCacheEntry = item.backForwardCacheEntry()) {
            if (backForwardCacheEntry->processIdentifier() == process().coreProcessIdentifier())
                return false;
        }
        return &item != targetItem.ptr();
    });

    Ref process { this->process() };
    std::optional<WebsitePoliciesData> websitePoliciesData;
    if (websitePolicies)
        websitePoliciesData = websitePolicies->dataForProcess(process);

    SandboxExtension::Handle sandboxExtensionHandle;
    URL itemURL { item.url() };
    Ref frameState = navigation.targetFrameItem() ? navigation.targetFrameItem()->copyFrameStateWithChildren() : item.mainFrameState();
    page->maybeInitializeSandboxExtensionHandle(process, itemURL, item.resourceDirectoryURL(), true, [
        weakThis = WeakPtr { *this },
        itemURL,
        frameState = WTFMove(frameState),
        navigationLoadType = *navigation.backForwardFrameLoadType(),
        shouldTreatAsContinuingLoad,
        websitePoliciesData = WTFMove(websitePoliciesData),
        existingNetworkResourceLoadIdentifierToResume = WTFMove(existingNetworkResourceLoadIdentifierToResume),
        navigation = Ref { navigation },
        sandboxExtensionHandle = WTFMove(sandboxExtensionHandle),
        processSwapDisposition
    ] (std::optional<SandboxExtension::Handle> sandboxExtension) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        auto publicSuffix = WebCore::PublicSuffixStore::singleton().publicSuffix(itemURL);
        if (sandboxExtension)
            sandboxExtensionHandle = WTFMove(*sandboxExtension);
        GoToBackForwardItemParameters parameters { navigation->navigationID(), WTFMove(frameState), navigationLoadType, shouldTreatAsContinuingLoad, WTFMove(websitePoliciesData), weakThis->m_page->lastNavigationWasAppInitiated(), existingNetworkResourceLoadIdentifierToResume, WTFMove(publicSuffix), WTFMove(sandboxExtensionHandle), processSwapDisposition };
        if (!protectedThis->process().isLaunching() || !itemURL.protocolIsFile())
            protectedThis->send(Messages::WebPage::GoToBackForwardItem(WTFMove(parameters)));
        else
            protectedThis->send(Messages::WebPage::GoToBackForwardItemWaitingForProcessLaunch(WTFMove(parameters), weakThis->m_page->identifier()));

        protectedThis->protectedProcess()->startResponsivenessTimer();
    });
}

inline bool ProvisionalPageProxy::validateInput(FrameIdentifier frameID, const std::optional<WebCore::NavigationIdentifier>& navigationID)
{
    // If the previous provisional load used an existing process, we may receive leftover IPC for a previous navigation, which we need to ignore.
    if (!m_mainFrame || m_mainFrame->frameID() != frameID)
        return false;

    return !navigationID || *navigationID == m_navigationID;
}

void ProvisionalPageProxy::didPerformClientRedirect(String&& sourceURLString, String&& destinationURLString, FrameIdentifier frameID)
{
    if (!validateInput(frameID))
        return;

    if (RefPtr page = m_page.get())
        page->didPerformClientRedirectShared(protectedProcess(), WTFMove(sourceURLString), WTFMove(destinationURLString), frameID);
}

void ProvisionalPageProxy::didStartProvisionalLoadForFrame(FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, std::optional<WebCore::NavigationIdentifier> navigationID, URL&& url, URL&& unreachableURL, const UserData& userData, WallTime timestamp)
{
    if (!validateInput(frameID, navigationID))
        return;

    PROVISIONALPAGEPROXY_RELEASE_LOG(ProcessSwapping, "didStartProvisionalLoadForFrame: frameID=%" PRIu64, frameID.toUInt64());
    ASSERT(m_provisionalLoadURL.isNull());
    m_provisionalLoadURL = url;

    // Merely following a server side redirect so there is no need to send a didStartProvisionalLoad again.
    if (m_isServerRedirect)
        return;

    RefPtr page = m_page.get();
    if (!page)
        return;

    // Clients expect the Page's main frame's expectedURL to be the provisional one when a provisional load is started.
    if (RefPtr pageMainFrame = page->mainFrame(); pageMainFrame && m_needsDidStartProvisionalLoad)
        pageMainFrame->didStartProvisionalLoad(URL { url });

    page->didStartProvisionalLoadForFrameShared(protectedProcess(), frameID, WTFMove(frameInfo), WTFMove(request), navigationID, WTFMove(url), WTFMove(unreachableURL), userData, timestamp);
}

void ProvisionalPageProxy::didFailProvisionalLoadForFrame(FrameInfoData&& frameInfo, ResourceRequest&& request, std::optional<WebCore::NavigationIdentifier> navigationID, String&& provisionalURL, WebCore::ResourceError&& error, WebCore::WillContinueLoading willContinueLoading, const UserData& userData, WebCore::WillInternallyHandleFailure willInternallyHandleFailure)
{
    if (!validateInput(frameInfo.frameID, navigationID))
        return;

    PROVISIONALPAGEPROXY_RELEASE_LOG_ERROR(ProcessSwapping, "didFailProvisionalLoadForFrame: frameID=%" PRIu64, frameInfo.frameID.toUInt64());
    ASSERT(!m_provisionalLoadURL.isNull());
    m_provisionalLoadURL = { };

    RefPtr page = m_page.get();
    if (!page)
        return;

    // Make sure the Page's main frame's expectedURL gets cleared since we updated it in didStartProvisionalLoad.
    // When site isolation is enabled, we use the same WebFrameProxy so we don't need this duplicate call.
    // didFailProvisionalLoadForFrameShared will call didFailProvisionalLoad on the same main frame.
    if (page->protectedPreferences()->siteIsolationEnabled()) {
        if (m_isProcessSwappingForNewWindow)
            m_browsingContextGroup->transitionProvisionalPageToRemotePage(*this, Site(request.url()));
        else if (m_takenRemotePage)
            m_browsingContextGroup->addRemotePage(*page, m_takenRemotePage.releaseNonNull());
        m_shouldClosePage = false;
    } else if (RefPtr pageMainFrame = page->mainFrame())
        pageMainFrame->didFailProvisionalLoad();

    RefPtr frame = WebFrameProxy::webFrame(frameInfo.frameID);
    MESSAGE_CHECK(frame);
    page->didFailProvisionalLoadForFrameShared(protectedProcess(), *frame, WTFMove(frameInfo), WTFMove(request), navigationID, WTFMove(provisionalURL), WTFMove(error), willContinueLoading, userData, willInternallyHandleFailure); // May delete |this|.
}

void ProvisionalPageProxy::didCommitLoadForFrame(IPC::Connection& connection, FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, std::optional<WebCore::NavigationIdentifier> navigationID, String&& mimeType, bool frameHasCustomContentProvider, FrameLoadType frameLoadType, const CertificateInfo& certificateInfo, bool usedLegacyTLS, bool privateRelayed, String&& proxyName, WebCore::ResourceResponseSource source, bool containsPluginDocument, HasInsecureContent hasInsecureContent, MouseEventPolicy mouseEventPolicy, const UserData& userData)
{
    if (!validateInput(frameID, navigationID))
        return;

    PROVISIONALPAGEPROXY_RELEASE_LOG(ProcessSwapping, "didCommitLoadForFrame: frameID=%" PRIu64, frameID.toUInt64());
    RefPtr page = m_page.get();
    if (page && page->protectedPreferences()->siteIsolationEnabled()) {
        RefPtr mainFrame = page->mainFrame();
        RefPtr openerFrame = mainFrame->opener();
        if (m_frameProcess.ptr() != &mainFrame->frameProcess())
            mainFrame->setProcess(m_frameProcess);
        if (RefPtr openerPage = openerFrame ? openerFrame->page() : nullptr) {
            Site openerSite(openerFrame->url());
            Site openedSite(request.url());
            if (openerSite != openedSite && m_browsingContextGroup.ptr() == &page->browsingContextGroup()) {
                page->protectedLegacyMainFrameProcess()->send(Messages::WebPage::LoadDidCommitInAnotherProcess(page->mainFrame()->frameID(), std::nullopt), page->webPageIDInMainFrameProcess());
                m_browsingContextGroup->transitionPageToRemotePage(*page, openerSite);
            }
        }
    }
    m_provisionalLoadURL = { };
    m_messageReceiverRegistration.stopReceivingMessages();

    m_wasCommitted = true;
    page->commitProvisionalPage(connection, frameID, WTFMove(frameInfo), WTFMove(request), navigationID, WTFMove(mimeType), frameHasCustomContentProvider, frameLoadType, certificateInfo, usedLegacyTLS, privateRelayed, WTFMove(proxyName), source, containsPluginDocument, hasInsecureContent, mouseEventPolicy, userData); // Will delete |this|.
}

void ProvisionalPageProxy::didNavigateWithNavigationData(const WebNavigationDataStore& store, FrameIdentifier frameID)
{
    if (!validateInput(frameID))
        return;

    if (RefPtr page = m_page.get())
        page->didNavigateWithNavigationDataShared(protectedProcess(), store, frameID);
}

void ProvisionalPageProxy::didChangeProvisionalURLForFrame(FrameIdentifier frameID, std::optional<WebCore::NavigationIdentifier> navigationID, URL&& url)
{
    if (!validateInput(frameID, navigationID))
        return;

    if (RefPtr page = m_page.get())
        page->didChangeProvisionalURLForFrameShared(protectedProcess(), frameID, navigationID, WTFMove(url));
}

void ProvisionalPageProxy::decidePolicyForNavigationActionAsync(IPC::Connection& connection, NavigationActionData&& data, CompletionHandler<void(PolicyDecision&&)>&& completionHandler)
{
    if (!validateInput(data.frameInfo.frameID, data.navigationID))
        return completionHandler({ });

    if (RefPtr page = m_page.get())
        page->decidePolicyForNavigationActionAsync(connection, WTFMove(data), WTFMove(completionHandler));
    else
        completionHandler({ });
}

void ProvisionalPageProxy::decidePolicyForResponse(FrameInfoData&& frameInfo, std::optional<WebCore::NavigationIdentifier> navigationID, const WebCore::ResourceResponse& response, const WebCore::ResourceRequest& request, bool canShowMIMEType, String&& downloadAttribute, bool isShowingInitialAboutBlank, WebCore::CrossOriginOpenerPolicyValue activeDocumentCOOPValue, CompletionHandler<void(PolicyDecision&&)>&& completionHandler)
{
    if (!validateInput(frameInfo.frameID, navigationID))
        return completionHandler({ });

    if (RefPtr page = m_page.get())
        page->decidePolicyForResponseShared(protectedProcess(), m_webPageID, WTFMove(frameInfo), navigationID, response, request, canShowMIMEType, WTFMove(downloadAttribute), isShowingInitialAboutBlank, activeDocumentCOOPValue, WTFMove(completionHandler));
    else
        completionHandler({ });
}

void ProvisionalPageProxy::didPerformServerRedirect(String&& sourceURLString, String&& destinationURLString, FrameIdentifier frameID)
{
    if (!validateInput(frameID))
        return;

    if (RefPtr page = m_page.get())
        page->didPerformServerRedirectShared(protectedProcess(), WTFMove(sourceURLString), WTFMove(destinationURLString), frameID);
}

void ProvisionalPageProxy::didReceiveServerRedirectForProvisionalLoadForFrame(FrameIdentifier frameID, std::optional<WebCore::NavigationIdentifier> navigationID, WebCore::ResourceRequest&& request, const UserData& userData)
{
    if (!validateInput(frameID, navigationID))
        return;

    if (RefPtr page = m_page.get())
        page->didReceiveServerRedirectForProvisionalLoadForFrameShared(protectedProcess(), frameID, navigationID, WTFMove(request), userData);
}

void ProvisionalPageProxy::startURLSchemeTask(IPC::Connection& connection, URLSchemeTaskParameters&& parameters)
{
    if (RefPtr page = m_page.get())
        page->startURLSchemeTaskShared(connection, protectedProcess(), m_webPageID, WTFMove(parameters));
}

void ProvisionalPageProxy::backForwardGoToItem(WebCore::BackForwardItemIdentifier identifier, CompletionHandler<void(const WebBackForwardListCounts&)>&& completionHandler)
{
    if (RefPtr page = m_page.get())
        page->backForwardGoToItemShared(identifier, WTFMove(completionHandler));
    else
        completionHandler({ });
}

void ProvisionalPageProxy::decidePolicyForNavigationActionSync(IPC::Connection& connection, NavigationActionData&& data, CompletionHandler<void(PolicyDecision&&)>&& reply)
{
    auto& frameInfo = data.frameInfo;
    auto navigationID = data.navigationID;
    if (!frameInfo.isMainFrame || (m_mainFrame && m_mainFrame->frameID() != frameInfo.frameID) || navigationID != m_navigationID) {
        reply(PolicyDecision { std::nullopt, WebCore::PolicyAction::Ignore, navigationID });
        return;
    }

    ASSERT(m_mainFrame);

    if (RefPtr page = m_page.get())
        page->decidePolicyForNavigationActionSync(connection, WTFMove(data), WTFMove(reply));
    else
        reply({ });
}

void ProvisionalPageProxy::logDiagnosticMessageFromWebProcess(const String& message, const String& description, WebCore::ShouldSample shouldSample)
{
    MESSAGE_CHECK(message.containsOnlyASCII());

    if (RefPtr page = m_page.get())
        page->logDiagnosticMessage(message, description, shouldSample);
}

void ProvisionalPageProxy::logDiagnosticMessageWithEnhancedPrivacyFromWebProcess(const String& message, const String& description, WebCore::ShouldSample shouldSample)
{
    MESSAGE_CHECK(message.containsOnlyASCII());

    if (RefPtr page = m_page.get())
        page->logDiagnosticMessageWithEnhancedPrivacy(message, description, shouldSample);
}

void ProvisionalPageProxy::logDiagnosticMessageWithValueDictionaryFromWebProcess(const String& message, const String& description, const WebCore::DiagnosticLoggingClient::ValueDictionary& valueDictionary, WebCore::ShouldSample shouldSample)
{
    MESSAGE_CHECK(message.containsOnlyASCII());

    if (RefPtr page = m_page.get())
        page->logDiagnosticMessageWithValueDictionary(message, description, valueDictionary, shouldSample);
}

void ProvisionalPageProxy::backForwardAddItem(IPC::Connection& connection, Ref<FrameState>&& navigatedFrameState)
{
    if (RefPtr page = m_page.get())
        page->backForwardAddItemShared(connection, WTFMove(navigatedFrameState), m_replacedDataStoreForWebArchiveLoad ? LoadedWebArchive::Yes : LoadedWebArchive::No);
}

void ProvisionalPageProxy::didDestroyNavigation(WebCore::NavigationIdentifier navigationID)
{
    if (RefPtr page = m_page.get())
        page->didDestroyNavigationShared(protectedProcess(), navigationID);
}

#if USE(QUICK_LOOK)
void ProvisionalPageProxy::requestPasswordForQuickLookDocumentInMainFrame(const String& fileName, CompletionHandler<void(const String&)>&& completionHandler)
{
    if (RefPtr page = m_page.get())
        page->requestPasswordForQuickLookDocumentInMainFrameShared(fileName, WTFMove(completionHandler));
    else
        completionHandler({ });
}
#endif

#if PLATFORM(COCOA)
void ProvisionalPageProxy::registerWebProcessAccessibilityToken(std::span<const uint8_t> data)
{
    m_accessibilityToken = Vector(data);
}
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
void ProvisionalPageProxy::bindAccessibilityTree(const String& plugID)
{
    m_accessibilityPlugID = plugID;
}
#endif

#if ENABLE(CONTENT_FILTERING)
void ProvisionalPageProxy::contentFilterDidBlockLoadForFrame(const WebCore::ContentFilterUnblockHandler& unblockHandler, FrameIdentifier frameID)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    page->contentFilterDidBlockLoadForFrameShared(unblockHandler, frameID);
}
#endif

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
void ProvisionalPageProxy::didCreateContextInWebProcessForVisibilityPropagation(LayerHostingContextID contextID)
{
    PROVISIONALPAGEPROXY_RELEASE_LOG(ProcessSwapping, "didCreateContextInWebProcessForVisibilityPropagation: contextID=%u", contextID);
    m_contextIDForVisibilityPropagationInWebProcess = contextID;
}

#if ENABLE(GPU_PROCESS)
void ProvisionalPageProxy::didCreateContextInGPUProcessForVisibilityPropagation(LayerHostingContextID contextID)
{
    PROVISIONALPAGEPROXY_RELEASE_LOG(ProcessSwapping, "didCreateContextInGPUProcessForVisibilityPropagation: contextID=%u", contextID);
    m_contextIDForVisibilityPropagationInGPUProcess = contextID;
}
#endif // ENABLE(GPU_PROCESS)

#if ENABLE(MODEL_PROCESS)
void ProvisionalPageProxy::didCreateContextInModelProcessForVisibilityPropagation(LayerHostingContextID contextID)
{
    PROVISIONALPAGEPROXY_RELEASE_LOG(ProcessSwapping, "didCreateContextInModelProcessForVisibilityPropagation: contextID=%u", contextID);
    m_contextIDForVisibilityPropagationInModelProcess = contextID;
}
#endif // ENABLE(MODEL_PROCESS)
#endif // HAVE(VISIBILITY_PROPAGATION_VIEW)

void ProvisionalPageProxy::swipeAnimationDidEnd()
{
    send(Messages::WebPage::SwipeAnimationDidEnd());
}

void ProvisionalPageProxy::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    ASSERT(decoder.messageReceiverName() == Messages::WebPageProxy::messageReceiverName() || decoder.messageReceiverName() == Messages::WebBackForwardList::messageReceiverName());

    if (decoder.messageName() == Messages::WebPageProxy::DidStartProgress::name()
        || decoder.messageName() == Messages::WebPageProxy::DidChangeProgress::name()
        || decoder.messageName() == Messages::WebPageProxy::DidFinishProgress::name()
        || decoder.messageName() == Messages::WebPageProxy::SetNetworkRequestsInProgress::name()
        || decoder.messageName() == Messages::WebPageProxy::ShouldGoToBackForwardListItem::name()
        || decoder.messageName() == Messages::WebPageProxy::ShouldGoToBackForwardListItemSync::name()
#if USE(QUICK_LOOK)
        || decoder.messageName() == Messages::WebPageProxy::DidStartLoadForQuickLookDocumentInMainFrame::name()
        || decoder.messageName() == Messages::WebPageProxy::DidFinishLoadForQuickLookDocumentInMainFrame::name()
#endif
        || decoder.messageName() == Messages::WebPageProxy::CreateInspectorTarget::name()
        || decoder.messageName() == Messages::WebPageProxy::DestroyInspectorTarget::name()
        || decoder.messageName() == Messages::WebPageProxy::SendMessageToInspectorFrontend::name()
#if PLATFORM(GTK) || PLATFORM(WPE)
        || decoder.messageName() == Messages::WebPageProxy::DidInitiateLoadForResource::name()
        || decoder.messageName() == Messages::WebPageProxy::DidSendRequestForResource::name()
        || decoder.messageName() == Messages::WebPageProxy::DidReceiveResponseForResource::name()
#endif
        )
    {
        if (RefPtr page = m_page.get())
            page->didReceiveMessage(connection, decoder);
        return;
    }

    if (decoder.messageName() == Messages::WebBackForwardList::BackForwardUpdateItem::name()) {
        if (RefPtr page = m_page.get())
            page->backForwardList().didReceiveMessage(connection, decoder);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DidDestroyNavigation::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidDestroyNavigation>(connection, decoder, this, &ProvisionalPageProxy::didDestroyNavigation);
        return;
    }

#if PLATFORM(COCOA)
    if (decoder.messageName() == Messages::WebPageProxy::RegisterWebProcessAccessibilityToken::name()) {
        IPC::handleMessage<Messages::WebPageProxy::RegisterWebProcessAccessibilityToken>(connection, decoder, this, &ProvisionalPageProxy::registerWebProcessAccessibilityToken);
        return;
    }
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
    if (decoder.messageName() == Messages::WebPageProxy::BindAccessibilityTree::name()) {
        IPC::handleMessage<Messages::WebPageProxy::BindAccessibilityTree>(connection, decoder, this, &ProvisionalPageProxy::bindAccessibilityTree);
        return;
    }
#endif

    if (decoder.messageName() == Messages::WebBackForwardList::BackForwardAddItem::name()) {
        IPC::handleMessage<Messages::WebBackForwardList::BackForwardAddItem>(connection, decoder, this, &ProvisionalPageProxy::backForwardAddItem);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::LogDiagnosticMessageFromWebProcess::name()) {
        IPC::handleMessage<Messages::WebPageProxy::LogDiagnosticMessageFromWebProcess>(connection, decoder, this, &ProvisionalPageProxy::logDiagnosticMessageFromWebProcess);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::LogDiagnosticMessageWithEnhancedPrivacyFromWebProcess::name()) {
        IPC::handleMessage<Messages::WebPageProxy::LogDiagnosticMessageWithEnhancedPrivacyFromWebProcess>(connection, decoder, this, &ProvisionalPageProxy::logDiagnosticMessageWithEnhancedPrivacyFromWebProcess);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::LogDiagnosticMessageWithValueDictionaryFromWebProcess::name()) {
        IPC::handleMessage<Messages::WebPageProxy::LogDiagnosticMessageWithValueDictionaryFromWebProcess>(connection, decoder, this, &ProvisionalPageProxy::logDiagnosticMessageWithValueDictionaryFromWebProcess);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::StartURLSchemeTask::name()) {
        IPC::handleMessage<Messages::WebPageProxy::StartURLSchemeTask>(connection, decoder, this, &ProvisionalPageProxy::startURLSchemeTask);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DecidePolicyForNavigationActionAsync::name()) {
        IPC::handleMessageAsync<Messages::WebPageProxy::DecidePolicyForNavigationActionAsync>(connection, decoder, this, &ProvisionalPageProxy::decidePolicyForNavigationActionAsync);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DecidePolicyForResponse::name()) {
        IPC::handleMessageAsync<Messages::WebPageProxy::DecidePolicyForResponse>(connection, decoder, this, &ProvisionalPageProxy::decidePolicyForResponse);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DidChangeProvisionalURLForFrame::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidChangeProvisionalURLForFrame>(connection, decoder, this, &ProvisionalPageProxy::didChangeProvisionalURLForFrame);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DidNavigateWithNavigationData::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidNavigateWithNavigationData>(connection, decoder, this, &ProvisionalPageProxy::didNavigateWithNavigationData);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DidPerformClientRedirect::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidPerformClientRedirect>(connection, decoder, this, &ProvisionalPageProxy::didPerformClientRedirect);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DidStartProvisionalLoadForFrame::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidStartProvisionalLoadForFrame>(connection, decoder, this, &ProvisionalPageProxy::didStartProvisionalLoadForFrame);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DidFailProvisionalLoadForFrame::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidFailProvisionalLoadForFrame>(connection, decoder, this, &ProvisionalPageProxy::didFailProvisionalLoadForFrame);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DidCommitLoadForFrame::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidCommitLoadForFrame>(connection, decoder, this, &ProvisionalPageProxy::didCommitLoadForFrame);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DidReceiveServerRedirectForProvisionalLoadForFrame::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidReceiveServerRedirectForProvisionalLoadForFrame>(connection, decoder, this, &ProvisionalPageProxy::didReceiveServerRedirectForProvisionalLoadForFrame);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DidPerformServerRedirect::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidPerformServerRedirect>(connection, decoder, this, &ProvisionalPageProxy::didPerformServerRedirect);
        return;
    }

#if USE(QUICK_LOOK)
    if (decoder.messageName() == Messages::WebPageProxy::RequestPasswordForQuickLookDocumentInMainFrame::name()) {
        IPC::handleMessageAsync<Messages::WebPageProxy::RequestPasswordForQuickLookDocumentInMainFrame>(connection, decoder, this, &ProvisionalPageProxy::requestPasswordForQuickLookDocumentInMainFrame);
        return;
    }
#endif

#if ENABLE(CONTENT_FILTERING)
    if (decoder.messageName() == Messages::WebPageProxy::ContentFilterDidBlockLoadForFrame::name()) {
        IPC::handleMessage<Messages::WebPageProxy::ContentFilterDidBlockLoadForFrame>(connection, decoder, this, &ProvisionalPageProxy::contentFilterDidBlockLoadForFrame);
        return;
    }
#endif

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    if (decoder.messageName() == Messages::WebPageProxy::DidCreateContextInWebProcessForVisibilityPropagation::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidCreateContextInWebProcessForVisibilityPropagation>(connection, decoder, this, &ProvisionalPageProxy::didCreateContextInWebProcessForVisibilityPropagation);
        return;
    }
#endif

    LOG(ProcessSwapping, "Unhandled message %s from provisional process", description(decoder.messageName()).characters());
}

void ProvisionalPageProxy::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
{
    if (decoder.messageName() == Messages::WebBackForwardList::BackForwardGoToItem::name()) {
        IPC::handleMessageSynchronous<Messages::WebBackForwardList::BackForwardGoToItem>(connection, decoder, replyEncoder, this, &ProvisionalPageProxy::backForwardGoToItem);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DecidePolicyForNavigationActionSync::name()) {
        IPC::handleMessageSynchronous<Messages::WebPageProxy::DecidePolicyForNavigationActionSync>(connection, decoder, replyEncoder, this, &ProvisionalPageProxy::decidePolicyForNavigationActionSync);
        return;
    }

    RefPtr page = m_page.get();
    if (page) {
        if (decoder.messageReceiverName() == Messages::WebBackForwardList::messageReceiverName())
            page->backForwardList().didReceiveSyncMessage(connection, decoder, replyEncoder);
        else
            page->didReceiveSyncMessage(connection, decoder, replyEncoder);
    }
}

IPC::Connection* ProvisionalPageProxy::messageSenderConnection() const
{
    return &m_frameProcess->process().connection();
}

uint64_t ProvisionalPageProxy::messageSenderDestinationID() const
{
    return m_webPageID.toUInt64();
}

bool ProvisionalPageProxy::sendMessage(UniqueRef<IPC::Encoder>&& encoder, OptionSet<IPC::SendOption> sendOptions)
{
    // Send messages via the WebProcessProxy instead of the IPC::Connection since AuxiliaryProcessProxy implements queueing of messages
    // while the process is still launching.
    return protectedProcess()->sendMessage(WTFMove(encoder), sendOptions);
}

bool ProvisionalPageProxy::sendMessageWithAsyncReply(UniqueRef<IPC::Encoder>&& encoder, AsyncReplyHandler handler, OptionSet<IPC::SendOption> sendOptions)
{
    return protectedProcess()->sendMessage(WTFMove(encoder), sendOptions, WTFMove(handler));
}

} // namespace WebKit

#undef MESSAGE_CHECK

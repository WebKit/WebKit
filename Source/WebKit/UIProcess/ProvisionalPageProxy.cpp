/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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
#include "GoToBackForwardItemParameters.h"
#include "HandleMessage.h"
#include "LocalFrameCreationParameters.h"
#include "Logging.h"
#include "MessageSenderInlines.h"
#include "NavigationActionData.h"
#include "PageClient.h"
#include "RemotePageProxy.h"
#include "SuspendedPageProxy.h"
#include "URLSchemeTaskParameters.h"
#include "WebBackForwardCacheEntry.h"
#include "WebBackForwardList.h"
#include "WebBackForwardListCounts.h"
#include "WebBackForwardListItem.h"
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

#define MESSAGE_CHECK(process, assertion) MESSAGE_CHECK_BASE(assertion, process->connection())

namespace WebKit {

using namespace WebCore;

#define PROVISIONALPAGEPROXY_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", PID=%i, navigationID=%" PRIu64 "] ProvisionalPageProxy::" fmt, this, m_page->identifier().toUInt64(), m_webPageID.toUInt64(), m_process->processID(), m_navigationID, ##__VA_ARGS__)
#define PROVISIONALPAGEPROXY_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%p - [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", PID=%i, navigationID=%" PRIu64 "] ProvisionalPageProxy::" fmt, this, m_page->identifier().toUInt64(), m_webPageID.toUInt64(), m_process->processID(), m_navigationID, ##__VA_ARGS__)

ProvisionalPageProxy::ProvisionalPageProxy(WebPageProxy& page, Ref<WebProcessProxy>&& process, std::unique_ptr<SuspendedPageProxy> suspendedPage, API::Navigation& navigation, bool isServerRedirect, const WebCore::ResourceRequest& request, ProcessSwapRequestedByClient processSwapRequestedByClient, bool isProcessSwappingOnNavigationResponse, API::WebsitePolicies* websitePolicies)
    : m_page(page)
    , m_webPageID(suspendedPage ? suspendedPage->webPageID() : PageIdentifier::generate())
    , m_process(WTFMove(process))
    , m_navigationID(navigation.navigationID())
    , m_isServerRedirect(isServerRedirect)
    , m_request(request)
    , m_processSwapRequestedByClient(processSwapRequestedByClient)
    , m_isProcessSwappingOnNavigationResponse(isProcessSwappingOnNavigationResponse)
    , m_provisionalLoadURL(isProcessSwappingOnNavigationResponse ? request.url() : URL())
#if USE(RUNNINGBOARD)
    , m_provisionalLoadActivity(m_process->throttler().foregroundActivity("Provisional Load"_s))
#endif
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    , m_contextIDForVisibilityPropagationInWebProcess(suspendedPage ? suspendedPage->contextIDForVisibilityPropagationInWebProcess() : 0)
#if ENABLE(GPU_PROCESS)
    , m_contextIDForVisibilityPropagationInGPUProcess(suspendedPage ? suspendedPage->contextIDForVisibilityPropagationInGPUProcess() : 0)
#endif
#endif
{
    PROVISIONALPAGEPROXY_RELEASE_LOG(ProcessSwapping, "ProvisionalPageProxy: suspendedPage=%p", suspendedPage.get());

    m_messageReceiverRegistration.startReceivingMessages(m_process, m_webPageID, *this);
    m_process->addProvisionalPageProxy(*this);
    ASSERT(navigation.processID() == m_process->coreProcessIdentifier());

    m_websiteDataStore = m_process->websiteDataStore();
    ASSERT(m_websiteDataStore);
    if (m_websiteDataStore && m_websiteDataStore != &m_page->websiteDataStore())
        m_process->processPool().pageBeginUsingWebsiteDataStore(protectedPage(), *m_websiteDataStore);

    // If we are reattaching to a SuspendedPage, then the WebProcess' WebPage already exists and
    // WebPageProxy::didCreateMainFrame() will not be called to initialize m_mainFrame. In such
    // case, we need to initialize m_mainFrame to reflect the fact the the WebProcess' WebPage
    // already exists and already has a main frame.
    if (suspendedPage) {
        ASSERT(&suspendedPage->process() == m_process.ptr());
        suspendedPage->unsuspend();
        m_mainFrame = &suspendedPage->mainFrame();
        m_browsingContextGroup = &suspendedPage->browsingContextGroup();
        m_remotePageProxyState = suspendedPage->takeRemotePageProxyState();
    }

    initializeWebPage(websitePolicies);
}

ProvisionalPageProxy::~ProvisionalPageProxy()
{
#if PLATFORM(GTK) || PLATFORM(WPE)
    if (m_accessibilityBindCompletionHandler)
        m_accessibilityBindCompletionHandler({ });
#endif

    if (!m_wasCommitted) {
        m_page->inspectorController().willDestroyProvisionalPage(*this);

        auto dataStore = m_process->websiteDataStore();
        if (dataStore && dataStore!= &m_page->websiteDataStore())
            m_process->processPool().pageEndUsingWebsiteDataStore(Ref { m_page.get() }, *dataStore);

        if (m_process->hasConnection())
            send(Messages::WebPage::Close());
        m_process->removeVisitedLinkStoreUser(m_page->visitedLinkStore(), m_page->identifier());
    }

    m_process->removeProvisionalPageProxy(*this);
}

Ref<WebPageProxy> ProvisionalPageProxy::protectedPage() const
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
    m_page->provisionalProcessDidTerminate();
}

std::unique_ptr<DrawingAreaProxy> ProvisionalPageProxy::takeDrawingArea()
{
    return WTFMove(m_drawingArea);
}

void ProvisionalPageProxy::setNavigation(API::Navigation& navigation)
{
    if (m_navigationID == navigation.navigationID())
        return;

    m_navigationID = navigation.navigationID();
    navigation.setProcessID(m_process->coreProcessIdentifier());
}

void ProvisionalPageProxy::cancel()
{
    // If the provisional load started, then indicate that it failed due to cancellation by calling didFailProvisionalLoadForFrame().
    if (m_provisionalLoadURL.isEmpty() || !m_mainFrame)
        return;

    ASSERT(m_process->state() == WebProcessProxy::State::Running);

    PROVISIONALPAGEPROXY_RELEASE_LOG(ProcessSwapping, "cancel: Simulating a didFailProvisionalLoadForFrame");
    ASSERT(m_mainFrame);
    auto error = WebKit::cancelledError(m_request);
    error.setType(WebCore::ResourceError::Type::Cancellation);
    FrameInfoData frameInfo {
        true, // isMainFrame
        FrameType::Local,
        m_request,
        SecurityOriginData::fromURLWithoutStrictOpaqueness(m_request.url()),
        { },
        m_mainFrame->frameID(),
        std::nullopt,
        m_mainFrame->processID(),
        m_mainFrame->isFocused()
    };
    didFailProvisionalLoadForFrame(WTFMove(frameInfo), ResourceRequest { m_request }, m_navigationID, m_provisionalLoadURL.string(), error, WebCore::WillContinueLoading::No, UserData { }, WebCore::WillInternallyHandleFailure::No); // Will delete |this|.
}

void ProvisionalPageProxy::initializeWebPage(RefPtr<API::WebsitePolicies>&& websitePolicies)
{
    m_drawingArea = m_page->pageClient().createDrawingAreaProxy(m_process.copyRef());

    bool sendPageCreationParameters { true };
    bool registerWithInspectorController { true };
    auto parameters = m_page->creationParameters(m_process, *m_drawingArea, WTFMove(websitePolicies));

    if (page().preferences().processSwapOnCrossSiteWindowOpenEnabled() || page().preferences().siteIsolationEnabled()) {
        // FIXME: <rdar://121241026> This is not elegant and not robust. It needs to be.
        RegistrableDomain navigationDomain(m_request.url());
        RefPtr openerFrame = m_page->openerFrame();
        RefPtr openerPage = openerFrame ? openerFrame->page() : nullptr;
        if (openerFrame) {
            parameters.openerFrameIdentifier = openerFrame->frameID();
            parameters.mainFrameIdentifier = m_page->mainFrame()->frameID();
        }
        auto existingRemotePageProxy = m_page->takeRemotePageProxyInOpenerProcessIfDomainEquals(navigationDomain);
        if (!existingRemotePageProxy) {
            if ((existingRemotePageProxy = m_page->takeOpenedRemotePageProxyIfDomainEquals(navigationDomain)))
                registerWithInspectorController = false; // FIXME: <rdar://121240770> This is a hack. There seems to be a bug in our interaction with WebPageInspectorController.
        }
        if (existingRemotePageProxy) {
            ASSERT(existingRemotePageProxy->process().processID() == m_process->processID());
            m_webPageID = existingRemotePageProxy->pageID();
            m_mainFrame = existingRemotePageProxy->page()->mainFrame();
            m_messageReceiverRegistration.stopReceivingMessages();
            m_messageReceiverRegistration.transferMessageReceivingFrom(existingRemotePageProxy->messageReceiverRegistration(), *this);
            LocalFrameCreationParameters localFrameCreationParameters {
                std::nullopt
            };
            m_process->send(Messages::WebPage::TransitionFrameToLocal(localFrameCreationParameters, m_page->mainFrame()->frameID()), m_webPageID);
            sendPageCreationParameters = false;
        } else if (RefPtr existingRemotePageProxy = openerPage ? openerPage->remotePageProxyForRegistrableDomain(navigationDomain) : nullptr) {
            ASSERT(existingRemotePageProxy->process().processID() == m_process->processID());
            openerPage->addOpenedRemotePageProxy(m_page->identifier(), existingRemotePageProxy.releaseNonNull());
            m_needsCookieAccessAddedInNetworkProcess = true;
        } else if (openerPage) {
            auto remotePageProxy = RemotePageProxy::create(*openerPage, m_process, navigationDomain);
            remotePageProxy->injectPageIntoNewProcess();
            openerPage->addOpenedRemotePageProxy(m_page->identifier(), WTFMove(remotePageProxy));
        }
        m_needsDidStartProvisionalLoad = false;
    }

    parameters.isProcessSwap = true;
    if (sendPageCreationParameters) {
        m_process->send(Messages::WebProcess::CreateWebPage(m_webPageID, WTFMove(parameters)), 0);
        m_process->addVisitedLinkStoreUser(m_page->visitedLinkStore(), m_page->identifier());
    }

    if (m_page->isLayerTreeFrozenDueToSwipeAnimation())
        send(Messages::WebPage::FreezeLayerTreeDueToSwipeAnimation());

    if (registerWithInspectorController)
        m_page->inspectorController().didCreateProvisionalPage(*this);
}

void ProvisionalPageProxy::loadData(API::Navigation& navigation, const IPC::DataReference& data, const String& mimeType, const String& encoding, const String& baseURL, API::Object* userData, WebCore::ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, std::optional<WebsitePoliciesData>&& websitePolicies, SubstituteData::SessionHistoryVisibility sessionHistoryVisibility)
{
    PROVISIONALPAGEPROXY_RELEASE_LOG(ProcessSwapping, "loadData:");
    ASSERT(shouldTreatAsContinuingLoad != WebCore::ShouldTreatAsContinuingLoad::No);

    m_page->loadDataWithNavigationShared(m_process.copyRef(), m_webPageID, navigation, data, mimeType, encoding, baseURL, userData, shouldTreatAsContinuingLoad, isNavigatingToAppBoundDomain, WTFMove(websitePolicies), navigation.lastNavigationAction().shouldOpenExternalURLsPolicy, sessionHistoryVisibility);
}

void ProvisionalPageProxy::loadRequest(API::Navigation& navigation, WebCore::ResourceRequest&& request, API::Object* userData, WebCore::ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, std::optional<WebsitePoliciesData>&& websitePolicies, std::optional<NetworkResourceLoadIdentifier> existingNetworkResourceLoadIdentifierToResume)
{
    PROVISIONALPAGEPROXY_RELEASE_LOG(ProcessSwapping, "loadRequest: existingNetworkResourceLoadIdentifierToResume=%" PRIu64, valueOrDefault(existingNetworkResourceLoadIdentifierToResume).toUInt64());
    ASSERT(shouldTreatAsContinuingLoad != WebCore::ShouldTreatAsContinuingLoad::No);

    // If this is a client-side redirect continuing in a new process, then the new process will overwrite the fromItem's URL. In this case,
    // we need to make sure we update fromItem's processIdentifier as we want future navigations to this BackForward item to happen in the
    // new process.
    if (navigation.fromItem() && navigation.lockBackForwardList() == WebCore::LockBackForwardList::Yes)
        navigation.fromItem()->setLastProcessIdentifier(m_process->coreProcessIdentifier());

    m_page->loadRequestWithNavigationShared(m_process.copyRef(), m_webPageID, navigation, WTFMove(request), navigation.lastNavigationAction().shouldOpenExternalURLsPolicy, userData, shouldTreatAsContinuingLoad, isNavigatingToAppBoundDomain, WTFMove(websitePolicies), existingNetworkResourceLoadIdentifierToResume);
}

void ProvisionalPageProxy::goToBackForwardItem(API::Navigation& navigation, WebBackForwardListItem& item, RefPtr<API::WebsitePolicies>&& websitePolicies, WebCore::ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad, std::optional<NetworkResourceLoadIdentifier> existingNetworkResourceLoadIdentifierToResume)
{
    PROVISIONALPAGEPROXY_RELEASE_LOG(ProcessSwapping, "goToBackForwardItem: existingNetworkResourceLoadIdentifierToResume=%" PRIu64, valueOrDefault(existingNetworkResourceLoadIdentifierToResume).toUInt64());

    auto itemStates = m_page->backForwardList().filteredItemStates([this, targetItem = Ref { item }](auto& item) {
        if (auto* backForwardCacheEntry = item.backForwardCacheEntry()) {
            if (backForwardCacheEntry->processIdentifier() == m_process->coreProcessIdentifier())
                return false;
        }
        return &item != targetItem.ptr();
    });
    
    std::optional<WebsitePoliciesData> websitePoliciesData;
    if (websitePolicies)
        websitePoliciesData = websitePolicies->data();
    
    std::optional<String> topPrivatelyControlledDomain;
#if ENABLE(PUBLIC_SUFFIX_LIST)
    topPrivatelyControlledDomain = WebCore::topPrivatelyControlledDomain(URL(item.url()).host().toString());
#endif

    send(Messages::WebPage::UpdateBackForwardListForReattach(WTFMove(itemStates)));

    SandboxExtension::Handle sandboxExtensionHandle;
    URL itemURL { item.url() };
    m_page->maybeInitializeSandboxExtensionHandle(m_process.get(), itemURL, item.resourceDirectoryURL(), sandboxExtensionHandle);

    GoToBackForwardItemParameters parameters { navigation.navigationID(), item.itemID(), *navigation.backForwardFrameLoadType(), shouldTreatAsContinuingLoad, WTFMove(websitePoliciesData), m_page->lastNavigationWasAppInitiated(), existingNetworkResourceLoadIdentifierToResume, topPrivatelyControlledDomain, WTFMove(sandboxExtensionHandle) };
    if (!m_process->isLaunching() || !itemURL.protocolIsFile())
        send(Messages::WebPage::GoToBackForwardItem(WTFMove(parameters)));
    else
        send(Messages::WebPage::GoToBackForwardItemWaitingForProcessLaunch(WTFMove(parameters), m_page->identifier()));

    m_process->startResponsivenessTimer();
}

inline bool ProvisionalPageProxy::validateInput(FrameIdentifier frameID, const std::optional<uint64_t>& navigationID)
{
    // If the previous provisional load used an existing process, we may receive leftover IPC for a previous navigation, which we need to ignore.
    if (!m_mainFrame || m_mainFrame->frameID() != frameID)
        return false;

    return !navigationID || !*navigationID || *navigationID == m_navigationID;
}

void ProvisionalPageProxy::didCreateMainFrame(FrameIdentifier frameID)
{
    PROVISIONALPAGEPROXY_RELEASE_LOG(ProcessSwapping, "didCreateMainFrame: frameID=%" PRIu64, frameID.object().toUInt64());
    ASSERT(!m_mainFrame);

    RefPtr<WebFrameProxy> previousMainFrame = m_page->mainFrame();
    if (m_page->openerFrame() && (page().preferences().processSwapOnCrossSiteWindowOpenEnabled() || page().preferences().siteIsolationEnabled())) {
        ASSERT(m_page->mainFrame()->frameID() == frameID);
        m_mainFrame = m_page->mainFrame();
    } else
        m_mainFrame = WebFrameProxy::create(protectedPage(), m_process, frameID);

    // This navigation was destroyed so no need to notify of redirect.
    if (!m_page->navigationState().hasNavigation(m_navigationID))
        return;

    // Restore the main frame's committed URL as some clients may rely on it until the next load is committed.
    if (auto mainFrame = protectedMainFrame()) {
        mainFrame->frameLoadState().setURL(previousMainFrame->url());
        previousMainFrame->transferNavigationCallbackToFrame(*mainFrame);
    }

    // Normally, notification of a server redirect comes from the WebContent process.
    // If we are process swapping in response to a server redirect then that notification will not come from the new WebContent process.
    // In this case we have the UIProcess synthesize the redirect notification at the appropriate time.
    if (m_isServerRedirect) {
        // FIXME: When <rdar://116203552> is fixed we should not need this case here
        // because main frame redirect messages won't come from the web content process.
        if (m_page->preferences().siteIsolationEnabled() && !m_mainFrame->frameLoadState().provisionalURL().isEmpty())
            m_mainFrame->frameLoadState().didReceiveServerRedirectForProvisionalLoad(m_request.url());
        else
            m_mainFrame->frameLoadState().didStartProvisionalLoad(m_request.url());
        m_page->didReceiveServerRedirectForProvisionalLoadForFrameShared(m_process.copyRef(), m_mainFrame->frameID(), m_navigationID, WTFMove(m_request), { });
    } else if (previousMainFrame && !previousMainFrame->provisionalURL().isEmpty()) {
        // In case of a process swap after response policy, the didStartProvisionalLoad already happened but the new main frame doesn't know about it
        // so we need to tell it so it can update its provisional URL.
        m_mainFrame->didStartProvisionalLoad(previousMainFrame->provisionalURL());
    }
}

void ProvisionalPageProxy::didPerformClientRedirect(const String& sourceURLString, const String& destinationURLString, FrameIdentifier frameID)
{
    if (!validateInput(frameID))
        return;

    m_page->didPerformClientRedirectShared(m_process.copyRef(), sourceURLString, destinationURLString, frameID);
}

void ProvisionalPageProxy::didStartProvisionalLoadForFrame(FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, uint64_t navigationID, URL&& url, URL&& unreachableURL, const UserData& userData)
{
    if (!validateInput(frameID, navigationID))
        return;

    PROVISIONALPAGEPROXY_RELEASE_LOG(ProcessSwapping, "didStartProvisionalLoadForFrame: frameID=%" PRIu64, frameID.object().toUInt64());
    ASSERT(m_provisionalLoadURL.isNull());
    m_provisionalLoadURL = url;

    // Merely following a server side redirect so there is no need to send a didStartProvisionalLoad again.
    if (m_isServerRedirect)
        return;

    // Clients expect the Page's main frame's expectedURL to be the provisional one when a provisional load is started.
    if (auto* pageMainFrame = m_page->mainFrame(); pageMainFrame && m_needsDidStartProvisionalLoad)
        pageMainFrame->didStartProvisionalLoad(url);

    m_page->didStartProvisionalLoadForFrameShared(m_process.copyRef(), frameID, WTFMove(frameInfo), WTFMove(request), navigationID, WTFMove(url), WTFMove(unreachableURL), userData);
}

void ProvisionalPageProxy::didFailProvisionalLoadForFrame(FrameInfoData&& frameInfo, ResourceRequest&& request, uint64_t navigationID, const String& provisionalURL, const WebCore::ResourceError& error, WebCore::WillContinueLoading willContinueLoading, const UserData& userData, WebCore::WillInternallyHandleFailure willInternallyHandleFailure)
{
    if (!validateInput(frameInfo.frameID, navigationID))
        return;

    PROVISIONALPAGEPROXY_RELEASE_LOG_ERROR(ProcessSwapping, "didFailProvisionalLoadForFrame: frameID=%" PRIu64, frameInfo.frameID.object().toUInt64());
    ASSERT(!m_provisionalLoadURL.isNull());
    m_provisionalLoadURL = { };

    // Make sure the Page's main frame's expectedURL gets cleared since we updated it in didStartProvisionalLoad.
    if (auto* pageMainFrame = m_page->mainFrame())
        pageMainFrame->didFailProvisionalLoad();

    RefPtr frame = WebFrameProxy::webFrame(frameInfo.frameID);
    MESSAGE_CHECK(m_process, frame);
    m_page->didFailProvisionalLoadForFrameShared(m_process.copyRef(), *frame, WTFMove(frameInfo), WTFMove(request), navigationID, provisionalURL, error, willContinueLoading, userData, willInternallyHandleFailure); // May delete |this|.
}

void ProvisionalPageProxy::didCommitLoadForFrame(FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, uint64_t navigationID, const String& mimeType, bool frameHasCustomContentProvider, WebCore::FrameLoadType frameLoadType, const WebCore::CertificateInfo& certificateInfo, bool usedLegacyTLS, bool privateRelayed, bool containsPluginDocument, WebCore::HasInsecureContent hasInsecureContent, WebCore::MouseEventPolicy mouseEventPolicy, const UserData& userData)
{
    if (!validateInput(frameID, navigationID))
        return;

    PROVISIONALPAGEPROXY_RELEASE_LOG(ProcessSwapping, "didCommitLoadForFrame: frameID=%" PRIu64, frameID.object().toUInt64());
    auto page = protectedPage();
    if (page->preferences().processSwapOnCrossSiteWindowOpenEnabled() || page->preferences().siteIsolationEnabled()) {
        RefPtr openerFrame = m_page->openerFrame();
        if (RefPtr openerPage = openerFrame ? openerFrame->page() : nullptr) {
            RegistrableDomain openerDomain(openerFrame->url());
            RegistrableDomain openedDomain(request.url());
            if (openerDomain == openedDomain)
                openerPage->removeOpenedRemotePageProxy(page->identifier());
            else {
                page->send(Messages::WebPage::DidCommitLoadInAnotherProcess(page->mainFrame()->frameID(), std::nullopt));
                page->setRemotePageProxyInOpenerProcess(RemotePageProxy::create(page, openerPage->protectedProcess(), openerDomain, &page->messageReceiverRegistration()));
                page->mainFrame()->setProcess(m_process.get());
            }
        }
    }
    m_provisionalLoadURL = { };
    m_messageReceiverRegistration.stopReceivingMessages();

    m_wasCommitted = true;
    page->commitProvisionalPage(frameID, WTFMove(frameInfo), WTFMove(request), navigationID, mimeType, frameHasCustomContentProvider, frameLoadType, certificateInfo, usedLegacyTLS, privateRelayed, containsPluginDocument, hasInsecureContent, mouseEventPolicy, userData); // Will delete |this|.
}

void ProvisionalPageProxy::didNavigateWithNavigationData(const WebNavigationDataStore& store, FrameIdentifier frameID)
{
    if (!validateInput(frameID))
        return;

    m_page->didNavigateWithNavigationDataShared(m_process.copyRef(), store, frameID);
}

void ProvisionalPageProxy::didChangeProvisionalURLForFrame(FrameIdentifier frameID, uint64_t navigationID, URL&& url)
{
    if (!validateInput(frameID, navigationID))
        return;

    m_page->didChangeProvisionalURLForFrameShared(m_process.copyRef(), frameID, navigationID, WTFMove(url));
}

void ProvisionalPageProxy::decidePolicyForNavigationActionAsync(NavigationActionData&& data, CompletionHandler<void(PolicyDecision&&)>&& completionHandler)
{
    if (!validateInput(data.frameInfo.frameID, data.navigationID))
        return completionHandler({ });

    m_page->decidePolicyForNavigationActionAsyncShared(m_process.copyRef(), WTFMove(data), WTFMove(completionHandler));
}

void ProvisionalPageProxy::decidePolicyForResponse(FrameInfoData&& frameInfo, uint64_t navigationID, const WebCore::ResourceResponse& response, const WebCore::ResourceRequest& request, bool canShowMIMEType, const String& downloadAttribute, CompletionHandler<void(PolicyDecision&&)>&& completionHandler)
{
    if (!validateInput(frameInfo.frameID, navigationID))
        return completionHandler({ });

    m_page->decidePolicyForResponseShared(m_process.copyRef(), m_webPageID, WTFMove(frameInfo), navigationID, response, request, canShowMIMEType, downloadAttribute, WTFMove(completionHandler));
}

void ProvisionalPageProxy::didPerformServerRedirect(const String& sourceURLString, const String& destinationURLString, FrameIdentifier frameID)
{
    if (!validateInput(frameID))
        return;

    m_page->didPerformServerRedirectShared(m_process.copyRef(), sourceURLString, destinationURLString, frameID);
}

void ProvisionalPageProxy::didReceiveServerRedirectForProvisionalLoadForFrame(FrameIdentifier frameID, uint64_t navigationID, WebCore::ResourceRequest&& request, const UserData& userData)
{
    if (!validateInput(frameID, navigationID))
        return;

    m_page->didReceiveServerRedirectForProvisionalLoadForFrameShared(m_process.copyRef(), frameID, navigationID, WTFMove(request), userData);
}

void ProvisionalPageProxy::startURLSchemeTask(URLSchemeTaskParameters&& parameters)
{
    m_page->startURLSchemeTaskShared(m_process.copyRef(), m_webPageID, WTFMove(parameters));
}

void ProvisionalPageProxy::backForwardGoToItem(const WebCore::BackForwardItemIdentifier& identifier, CompletionHandler<void(const WebBackForwardListCounts&)>&& completionHandler)
{
    m_page->backForwardGoToItemShared(identifier, WTFMove(completionHandler));
}

void ProvisionalPageProxy::decidePolicyForNavigationActionSync(NavigationActionData&& data, CompletionHandler<void(PolicyDecision&&)>&& reply)
{
    auto& frameInfo = data.frameInfo;
    auto navigationID = data.navigationID;
    if (!frameInfo.isMainFrame || (m_mainFrame && m_mainFrame->frameID() != frameInfo.frameID) || navigationID != m_navigationID) {
        reply(PolicyDecision { std::nullopt, WebCore::PolicyAction::Ignore, navigationID });
        return;
    }

    if (!m_mainFrame) {
        // This synchronous IPC message was processed before the asynchronous DidCreateMainFrame one so we do not know about this frameID yet.
        didCreateMainFrame(frameInfo.frameID);
    }
    ASSERT(m_mainFrame);

    m_page->decidePolicyForNavigationActionSyncShared(m_process.copyRef(), WTFMove(data), WTFMove(reply));
}

void ProvisionalPageProxy::logDiagnosticMessageFromWebProcess(const String& message, const String& description, WebCore::ShouldSample shouldSample)
{
    MESSAGE_CHECK(m_process, message.containsOnlyASCII());

    m_page->logDiagnosticMessage(message, description, shouldSample);
}

void ProvisionalPageProxy::logDiagnosticMessageWithEnhancedPrivacyFromWebProcess(const String& message, const String& description, WebCore::ShouldSample shouldSample)
{
    MESSAGE_CHECK(m_process, message.containsOnlyASCII());

    m_page->logDiagnosticMessageWithEnhancedPrivacy(message, description, shouldSample);
}

void ProvisionalPageProxy::logDiagnosticMessageWithValueDictionaryFromWebProcess(const String& message, const String& description, const WebCore::DiagnosticLoggingClient::ValueDictionary& valueDictionary, WebCore::ShouldSample shouldSample)
{
    MESSAGE_CHECK(m_process, message.containsOnlyASCII());

    m_page->logDiagnosticMessageWithValueDictionary(message, description, valueDictionary, shouldSample);
}

void ProvisionalPageProxy::backForwardAddItem(BackForwardListItemState&& itemState)
{
    m_page->backForwardAddItemShared(m_process.copyRef(), WTFMove(itemState));
}

void ProvisionalPageProxy::didDestroyNavigation(uint64_t navigationID)
{
    m_page->didDestroyNavigationShared(m_process.copyRef(), navigationID);
}

#if USE(QUICK_LOOK)
void ProvisionalPageProxy::requestPasswordForQuickLookDocumentInMainFrame(const String& fileName, CompletionHandler<void(const String&)>&& completionHandler)
{
    m_page->requestPasswordForQuickLookDocumentInMainFrameShared(fileName, WTFMove(completionHandler));
}
#endif

#if PLATFORM(COCOA)
void ProvisionalPageProxy::registerWebProcessAccessibilityToken(const IPC::DataReference& data)
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
    m_page->contentFilterDidBlockLoadForFrameShared(m_process.copyRef(), unblockHandler, frameID);
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

void ProvisionalPageProxy::unfreezeLayerTreeDueToSwipeAnimation()
{
    send(Messages::WebPage::UnfreezeLayerTreeDueToSwipeAnimation());
}

void ProvisionalPageProxy::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    ASSERT(decoder.messageReceiverName() == Messages::WebPageProxy::messageReceiverName());

    if (decoder.messageName() == Messages::WebPageProxy::DidStartProgress::name()
        || decoder.messageName() == Messages::WebPageProxy::DidChangeProgress::name()
        || decoder.messageName() == Messages::WebPageProxy::DidFinishProgress::name()
        || decoder.messageName() == Messages::WebPageProxy::SetNetworkRequestsInProgress::name()
        || decoder.messageName() == Messages::WebPageProxy::WillGoToBackForwardListItem::name()
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
        m_page->didReceiveMessage(connection, decoder);
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

    if (decoder.messageName() == Messages::WebPageProxy::BackForwardAddItem::name()) {
        IPC::handleMessage<Messages::WebPageProxy::BackForwardAddItem>(connection, decoder, this, &ProvisionalPageProxy::backForwardAddItem);
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

    if (decoder.messageName() == Messages::WebPageProxy::DidCreateMainFrame::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidCreateMainFrame>(connection, decoder, this, &ProvisionalPageProxy::didCreateMainFrame);
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

    LOG(ProcessSwapping, "Unhandled message %s from provisional process", description(decoder.messageName()));
}

bool ProvisionalPageProxy::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
{
    if (decoder.messageName() == Messages::WebPageProxy::BackForwardGoToItem::name())
        return IPC::handleMessageSynchronous<Messages::WebPageProxy::BackForwardGoToItem>(connection, decoder, replyEncoder, this, &ProvisionalPageProxy::backForwardGoToItem);

    if (decoder.messageName() == Messages::WebPageProxy::DecidePolicyForNavigationActionSync::name())
        return IPC::handleMessageSynchronous<Messages::WebPageProxy::DecidePolicyForNavigationActionSync>(connection, decoder, replyEncoder, this, &ProvisionalPageProxy::decidePolicyForNavigationActionSync);

    return m_page->didReceiveSyncMessage(connection, decoder, replyEncoder);
}

IPC::Connection* ProvisionalPageProxy::messageSenderConnection() const
{
    return m_process->connection();
}

uint64_t ProvisionalPageProxy::messageSenderDestinationID() const
{
    return m_webPageID.toUInt64();
}

bool ProvisionalPageProxy::sendMessage(UniqueRef<IPC::Encoder>&& encoder, OptionSet<IPC::SendOption> sendOptions)
{
    // Send messages via the WebProcessProxy instead of the IPC::Connection since AuxiliaryProcessProxy implements queueing of messages
    // while the process is still launching.
    return m_process->sendMessage(WTFMove(encoder), sendOptions);
}

bool ProvisionalPageProxy::sendMessageWithAsyncReply(UniqueRef<IPC::Encoder>&& encoder, AsyncReplyHandler handler, OptionSet<IPC::SendOption> sendOptions)
{
    return m_process->sendMessage(WTFMove(encoder), sendOptions, WTFMove(handler));
}

} // namespace WebKit

#undef MESSAGE_CHECK

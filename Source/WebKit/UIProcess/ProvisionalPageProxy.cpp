/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "DrawingAreaProxy.h"
#include "FormDataReference.h"
#include "HandleMessage.h"
#include "Logging.h"
#include "PageClient.h"
#include "URLSchemeTaskParameters.h"
#include "WebBackForwardCacheEntry.h"
#include "WebBackForwardList.h"
#include "WebBackForwardListItem.h"
#include "WebErrors.h"
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

namespace WebKit {

using namespace WebCore;

#define RELEASE_LOG_IF_ALLOWED(channel, fmt, ...) RELEASE_LOG_IF(m_page.isAlwaysOnLoggingAllowed(), channel, "%p - [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", PID=%i, navigationID=%" PRIu64 "] ProvisionalPageProxy::" fmt, this, m_page.identifier().toUInt64(), m_webPageID.toUInt64(), m_process->processIdentifier(), m_navigationID, ##__VA_ARGS__)
#define RELEASE_LOG_ERROR_IF_ALLOWED(channel, fmt, ...) RELEASE_LOG_ERROR_IF(m_page.isAlwaysOnLoggingAllowed(), channel, "%p - [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", PID=%i, navigationID=%" PRIu64 "] ProvisionalPageProxy::" fmt, this, m_page.identifier().toUInt64(), m_webPageID.toUInt64(), m_process->processIdentifier(), m_navigationID, ##__VA_ARGS__)

ProvisionalPageProxy::ProvisionalPageProxy(WebPageProxy& page, Ref<WebProcessProxy>&& process, std::unique_ptr<SuspendedPageProxy> suspendedPage, uint64_t navigationID, bool isServerRedirect, const WebCore::ResourceRequest& request, ProcessSwapRequestedByClient processSwapRequestedByClient, API::WebsitePolicies* websitePolicies)
    : m_page(page)
    , m_webPageID(suspendedPage ? suspendedPage->webPageID() : PageIdentifier::generate())
    , m_process(WTFMove(process))
    , m_navigationID(navigationID)
    , m_isServerRedirect(isServerRedirect)
    , m_request(request)
    , m_processSwapRequestedByClient(processSwapRequestedByClient)
#if PLATFORM(IOS_FAMILY)
    , m_provisionalLoadActivity(m_process->throttler().foregroundActivity("Provisional Load"_s))
#endif
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    , m_contextIDForVisibilityPropagation(suspendedPage ? suspendedPage->contextIDForVisibilityPropagation() : 0)
#endif
{
    RELEASE_LOG_IF_ALLOWED(ProcessSwapping, "ProvisionalPageProxy: suspendedPage=%p", suspendedPage.get());

    m_process->addMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_webPageID, *this);
    m_process->addProvisionalPageProxy(*this);

    if (&m_process->websiteDataStore() != &m_page.websiteDataStore())
        m_process->processPool().pageBeginUsingWebsiteDataStore(m_page.identifier(), m_process->websiteDataStore());

    // If we are reattaching to a SuspendedPage, then the WebProcess' WebPage already exists and
    // WebPageProxy::didCreateMainFrame() will not be called to initialize m_mainFrame. In such
    // case, we need to initialize m_mainFrame to reflect the fact the the WebProcess' WebPage
    // already exists and already has a main frame.
    if (suspendedPage) {
        ASSERT(&suspendedPage->process() == m_process.ptr());
        suspendedPage->unsuspend();
        m_mainFrame = WebFrameProxy::create(m_page, suspendedPage->mainFrameID());
        m_process->frameCreated(suspendedPage->mainFrameID(), *m_mainFrame);
    }

    initializeWebPage(websitePolicies);

    m_page.inspectorController().didCreateProvisionalPage(*this);
}

ProvisionalPageProxy::~ProvisionalPageProxy()
{
    if (!m_wasCommitted) {
        m_page.inspectorController().willDestroyProvisionalPage(*this);

        if (&m_process->websiteDataStore() != &m_page.websiteDataStore())
            m_process->processPool().pageEndUsingWebsiteDataStore(m_page.identifier(), m_process->websiteDataStore());

        m_process->removeMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_webPageID);
        send(Messages::WebPage::Close());
        m_process->removeVisitedLinkStoreUser(m_page.visitedLinkStore(), m_page.identifier());
    }

    m_process->removeProvisionalPageProxy(*this);
}

void ProvisionalPageProxy::processDidTerminate()
{
    RELEASE_LOG_ERROR_IF_ALLOWED(ProcessSwapping, "processDidTerminate:");
    m_page.provisionalProcessDidTerminate();
}

std::unique_ptr<DrawingAreaProxy> ProvisionalPageProxy::takeDrawingArea()
{
    return WTFMove(m_drawingArea);
}

void ProvisionalPageProxy::cancel()
{
    // If the provisional load started, then indicate that it failed due to cancellation by calling didFailProvisionalLoadForFrame().
    if (m_provisionalLoadURL.isEmpty())
        return;
        
    ASSERT(m_process->state() == WebProcessProxy::State::Running);

    RELEASE_LOG_IF_ALLOWED(ProcessSwapping, "cancel: Simulating a didFailProvisionalLoadForFrame");
    ASSERT(m_mainFrame);
    auto error = WebKit::cancelledError(m_request);
    error.setType(WebCore::ResourceError::Type::Cancellation);
    FrameInfoData frameInfo {
        true, // isMainFrame
        m_request,
        SecurityOriginData::fromURL(m_request.url()),
        m_mainFrame->frameID(),
        WTF::nullopt,
    };
    didFailProvisionalLoadForFrame(m_mainFrame->frameID(), WTFMove(frameInfo), ResourceRequest { m_request }, m_navigationID, m_provisionalLoadURL.string(), error, WebCore::WillContinueLoading::No, UserData { }); // Will delete |this|.
}

void ProvisionalPageProxy::initializeWebPage(RefPtr<API::WebsitePolicies>&& websitePolicies)
{
    m_drawingArea = m_page.pageClient().createDrawingAreaProxy(m_process);

    auto parameters = m_page.creationParameters(m_process, *m_drawingArea, WTFMove(websitePolicies));
    parameters.isProcessSwap = true;
    m_process->send(Messages::WebProcess::CreateWebPage(m_webPageID, parameters), 0);
    m_process->addVisitedLinkStoreUser(m_page.visitedLinkStore(), m_page.identifier());

    if (m_page.isLayerTreeFrozenDueToSwipeAnimation())
        send(Messages::WebPage::FreezeLayerTreeDueToSwipeAnimation());
}

void ProvisionalPageProxy::loadData(API::Navigation& navigation, const IPC::DataReference& data, const String& MIMEType, const String& encoding, const String& baseURL, API::Object* userData, Optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, Optional<WebsitePoliciesData>&& websitePolicies)
{
    RELEASE_LOG_IF_ALLOWED(ProcessSwapping, "loadData:");

    m_page.loadDataWithNavigationShared(m_process.copyRef(), m_webPageID, navigation, data, MIMEType, encoding, baseURL, userData, WebCore::ShouldTreatAsContinuingLoad::Yes, isNavigatingToAppBoundDomain, WTFMove(websitePolicies), navigation.lastNavigationAction().shouldOpenExternalURLsPolicy);
}

void ProvisionalPageProxy::loadRequest(API::Navigation& navigation, WebCore::ResourceRequest&& request, API::Object* userData, Optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, Optional<WebsitePoliciesData>&& websitePolicies)
{
    RELEASE_LOG_IF_ALLOWED(ProcessSwapping, "loadRequest:");

    // If this is a client-side redirect continuing in a new process, then the new process will overwrite the fromItem's URL. In this case,
    // we need to make sure we update fromItem's processIdentifier as we want future navigations to this BackForward item to happen in the
    // new process.
    if (navigation.fromItem() && navigation.lockBackForwardList() == WebCore::LockBackForwardList::Yes)
        navigation.fromItem()->setLastProcessIdentifier(m_process->coreProcessIdentifier());

    m_page.loadRequestWithNavigationShared(m_process.copyRef(), m_webPageID, navigation, WTFMove(request), navigation.lastNavigationAction().shouldOpenExternalURLsPolicy, userData, WebCore::ShouldTreatAsContinuingLoad::Yes, isNavigatingToAppBoundDomain, WTFMove(websitePolicies));
}

void ProvisionalPageProxy::goToBackForwardItem(API::Navigation& navigation, WebBackForwardListItem& item, RefPtr<API::WebsitePolicies>&& websitePolicies)
{
    RELEASE_LOG_IF_ALLOWED(ProcessSwapping, "goToBackForwardItem:");

    auto itemStates = m_page.backForwardList().filteredItemStates([this, targetItem = &item](auto& item) {
        if (auto* backForwardCacheEntry = item.backForwardCacheEntry()) {
            if (backForwardCacheEntry->processIdentifier() == m_process->coreProcessIdentifier())
                return false;
        }
        return &item != targetItem;
    });
    
    Optional<WebsitePoliciesData> websitePoliciesData;
    if (websitePolicies)
        websitePoliciesData = websitePolicies->data();
    
    send(Messages::WebPage::UpdateBackForwardListForReattach(WTFMove(itemStates)));
    send(Messages::WebPage::GoToBackForwardItem(navigation.navigationID(), item.itemID(), *navigation.backForwardFrameLoadType(), WebCore::ShouldTreatAsContinuingLoad::Yes, WTFMove(websitePoliciesData)));
    m_process->startResponsivenessTimer();
}

inline bool ProvisionalPageProxy::validateInput(FrameIdentifier frameID, const Optional<uint64_t>& navigationID)
{
    // If the previous provisional load used an existing process, we may receive leftover IPC for a previous navigation, which we need to ignore.
    if (!m_mainFrame || m_mainFrame->frameID() != frameID)
        return false;

    return !navigationID || !*navigationID || *navigationID == m_navigationID;
}

void ProvisionalPageProxy::didCreateMainFrame(FrameIdentifier frameID)
{
    RELEASE_LOG_IF_ALLOWED(ProcessSwapping, "didCreateMainFrame: frameID=%" PRIu64, frameID.toUInt64());
    ASSERT(!m_mainFrame);

    m_mainFrame = WebFrameProxy::create(m_page, frameID);

    // Add the frame to the process wide map.
    m_process->frameCreated(frameID, *m_mainFrame);

    // This navigation was destroyed so no need to notify of redirect.
    if (!m_page.navigationState().hasNavigation(m_navigationID))
        return;

    // Restore the main frame's committed URL as some clients may rely on it until the next load is committed.
    if (auto* mainFrame = m_page.mainFrame())
        m_mainFrame->frameLoadState().setURL(mainFrame->frameLoadState().url());

    // Normally, notification of a server redirect comes from the WebContent process.
    // If we are process swapping in response to a server redirect then that notification will not come from the new WebContent process.
    // In this case we have the UIProcess synthesize the redirect notification at the appropriate time.
    if (m_isServerRedirect) {
        m_mainFrame->frameLoadState().didStartProvisionalLoad(m_request.url());
        m_page.didReceiveServerRedirectForProvisionalLoadForFrameShared(m_process.copyRef(), m_mainFrame->frameID(), m_navigationID, WTFMove(m_request), { });
    }
}

void ProvisionalPageProxy::didPerformClientRedirect(const String& sourceURLString, const String& destinationURLString, FrameIdentifier frameID)
{
    if (!validateInput(frameID))
        return;

    m_page.didPerformClientRedirectShared(m_process.copyRef(), sourceURLString, destinationURLString, frameID);
}

void ProvisionalPageProxy::didStartProvisionalLoadForFrame(FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, uint64_t navigationID, URL&& url, URL&& unreachableURL, const UserData& userData)
{
    if (!validateInput(frameID, navigationID))
        return;

    RELEASE_LOG_IF_ALLOWED(ProcessSwapping, "didStartProvisionalLoadForFrame: frameID=%" PRIu64, frameID.toUInt64());
    ASSERT(m_provisionalLoadURL.isNull());
    m_provisionalLoadURL = url;

    // Merely following a server side redirect so there is no need to send a didStartProvisionalLoad again.
    if (m_isServerRedirect)
        return;

    // Clients expect the Page's main frame's expectedURL to be the provisional one when a provisional load is started.
    if (auto* pageMainFrame = m_page.mainFrame())
        pageMainFrame->didStartProvisionalLoad(url);

    m_page.didStartProvisionalLoadForFrameShared(m_process.copyRef(), frameID, WTFMove(frameInfo), WTFMove(request), navigationID, WTFMove(url), WTFMove(unreachableURL), userData);
}

void ProvisionalPageProxy::didFailProvisionalLoadForFrame(FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, uint64_t navigationID, const String& provisionalURL, const WebCore::ResourceError& error, WebCore::WillContinueLoading willContinueLoading, const UserData& userData)
{
    if (!validateInput(frameID, navigationID))
        return;

    RELEASE_LOG_ERROR_IF_ALLOWED(ProcessSwapping, "didFailProvisionalLoadForFrame: frameID=%" PRIu64, frameID.toUInt64());
    ASSERT(!m_provisionalLoadURL.isNull());
    m_provisionalLoadURL = { };

    // Make sure the Page's main frame's expectedURL gets cleared since we updated it in didStartProvisionalLoad.
    if (auto* pageMainFrame = m_page.mainFrame())
        pageMainFrame->didFailProvisionalLoad();

    m_page.didFailProvisionalLoadForFrameShared(m_process.copyRef(), frameID, WTFMove(frameInfo), WTFMove(request), navigationID, provisionalURL, error, willContinueLoading, userData); // May delete |this|.
}

void ProvisionalPageProxy::didCommitLoadForFrame(FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, uint64_t navigationID, const String& mimeType, bool frameHasCustomContentProvider, WebCore::FrameLoadType frameLoadType, const WebCore::CertificateInfo& certificateInfo, bool usedLegacyTLS, bool containsPluginDocument, Optional<WebCore::HasInsecureContent> forcedHasInsecureContent, WebCore::MouseEventPolicy mouseEventPolicy, const UserData& userData)
{
    if (!validateInput(frameID, navigationID))
        return;

    RELEASE_LOG_IF_ALLOWED(ProcessSwapping, "didCommitLoadForFrame: frameID=%" PRIu64, frameID.toUInt64());
    m_provisionalLoadURL = { };
    m_process->removeMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_webPageID);

    m_wasCommitted = true;
    m_page.commitProvisionalPage(frameID, WTFMove(frameInfo), WTFMove(request), navigationID, mimeType, frameHasCustomContentProvider, frameLoadType, certificateInfo, usedLegacyTLS, containsPluginDocument, forcedHasInsecureContent, mouseEventPolicy, userData); // Will delete |this|.
}

void ProvisionalPageProxy::didNavigateWithNavigationData(const WebNavigationDataStore& store, FrameIdentifier frameID)
{
    if (!validateInput(frameID))
        return;

    m_page.didNavigateWithNavigationDataShared(m_process.copyRef(), store, frameID);
}

void ProvisionalPageProxy::didChangeProvisionalURLForFrame(FrameIdentifier frameID, uint64_t navigationID, URL&& url)
{
    if (!validateInput(frameID, navigationID))
        return;

    m_page.didChangeProvisionalURLForFrameShared(m_process.copyRef(), frameID, navigationID, WTFMove(url));
}

void ProvisionalPageProxy::decidePolicyForNavigationActionAsync(FrameIdentifier frameID, FrameInfoData&& frameInfo, WebCore::PolicyCheckIdentifier identifier,
    uint64_t navigationID, NavigationActionData&& navigationActionData, FrameInfoData&& originatingFrameInfo, Optional<WebPageProxyIdentifier> originatingPageID, const WebCore::ResourceRequest& originalRequest,
    WebCore::ResourceRequest&& request, IPC::FormDataReference&& requestBody, WebCore::ResourceResponse&& redirectResponse, const UserData& userData, uint64_t listenerID)
{
    if (!validateInput(frameID, navigationID))
        return;

    m_page.decidePolicyForNavigationActionAsyncShared(m_process.copyRef(), m_webPageID, frameID, WTFMove(frameInfo), identifier, navigationID, WTFMove(navigationActionData), WTFMove(originatingFrameInfo), originatingPageID, originalRequest, WTFMove(request), WTFMove(requestBody), WTFMove(redirectResponse), userData, listenerID);
}

void ProvisionalPageProxy::decidePolicyForResponse(FrameIdentifier frameID, FrameInfoData&& frameInfo, WebCore::PolicyCheckIdentifier identifier,
    uint64_t navigationID, const WebCore::ResourceResponse& response, const WebCore::ResourceRequest& request, bool canShowMIMEType, const String& downloadAttribute, uint64_t listenerID, const UserData& userData)
{
    if (!validateInput(frameID, navigationID))
        return;

    m_page.decidePolicyForResponseShared(m_process.copyRef(), m_webPageID, frameID, WTFMove(frameInfo), identifier, navigationID, response, request, canShowMIMEType, downloadAttribute, listenerID, userData);
}

void ProvisionalPageProxy::didPerformServerRedirect(const String& sourceURLString, const String& destinationURLString, FrameIdentifier frameID)
{
    if (!validateInput(frameID))
        return;

    m_page.didPerformServerRedirectShared(m_process.copyRef(), sourceURLString, destinationURLString, frameID);
}

void ProvisionalPageProxy::didReceiveServerRedirectForProvisionalLoadForFrame(FrameIdentifier frameID, uint64_t navigationID, WebCore::ResourceRequest&& request, const UserData& userData)
{
    if (!validateInput(frameID, navigationID))
        return;

    m_page.didReceiveServerRedirectForProvisionalLoadForFrameShared(m_process.copyRef(), frameID, navigationID, WTFMove(request), userData);
}

void ProvisionalPageProxy::startURLSchemeTask(URLSchemeTaskParameters&& parameters)
{
    m_page.startURLSchemeTaskShared(m_process.copyRef(), m_webPageID, WTFMove(parameters));
}

void ProvisionalPageProxy::backForwardGoToItem(const WebCore::BackForwardItemIdentifier& identifier, CompletionHandler<void(const WebBackForwardListCounts&)>&& completionHandler)
{
    m_page.backForwardGoToItemShared(m_process.copyRef(), identifier, WTFMove(completionHandler));
}

void ProvisionalPageProxy::decidePolicyForNavigationActionSync(FrameIdentifier frameID, bool isMainFrame, FrameInfoData&& frameInfoData, WebCore::PolicyCheckIdentifier identifier,
    uint64_t navigationID, NavigationActionData&& navigationActionData, FrameInfoData&& originatingFrameInfo, Optional<WebPageProxyIdentifier> originatingPageID,
    const WebCore::ResourceRequest& originalRequest, WebCore::ResourceRequest&& request, IPC::FormDataReference&& requestBody, WebCore::ResourceResponse&& redirectResponse,
    const UserData& userData, Messages::WebPageProxy::DecidePolicyForNavigationActionSync::DelayedReply&& reply)
{
    if (!isMainFrame || (m_mainFrame && m_mainFrame->frameID() != frameID) || navigationID != m_navigationID) {
        reply(PolicyDecision { identifier, WTF::nullopt, WebCore::PolicyAction::Ignore, navigationID, WTF::nullopt, WTF::nullopt });
        return;
    }

    if (!m_mainFrame) {
        // This synchronous IPC message was processed before the asynchronous DidCreateMainFrame one so we do not know about this frameID yet.
        didCreateMainFrame(frameID);
    }
    ASSERT(m_mainFrame);

    m_page.decidePolicyForNavigationActionSyncShared(m_process.copyRef(), frameID, isMainFrame, WTFMove(frameInfoData), identifier, navigationID, WTFMove(navigationActionData), WTFMove(originatingFrameInfo), originatingPageID, originalRequest, WTFMove(request), WTFMove(requestBody), WTFMove(redirectResponse), userData, WTFMove(reply));
}

#if USE(QUICK_LOOK)
void ProvisionalPageProxy::requestPasswordForQuickLookDocumentInMainFrame(const String& fileName, CompletionHandler<void(const String&)>&& completionHandler)
{
    m_page.requestPasswordForQuickLookDocumentInMainFrameShared(fileName, WTFMove(completionHandler));
}
#endif

#if PLATFORM(COCOA)
void ProvisionalPageProxy::registerWebProcessAccessibilityToken(const IPC::DataReference& data)
{
    m_accessibilityToken = data.vector();
}
#endif

#if ENABLE(CONTENT_FILTERING)
void ProvisionalPageProxy::contentFilterDidBlockLoadForFrame(const WebCore::ContentFilterUnblockHandler& unblockHandler, FrameIdentifier frameID)
{
    m_page.contentFilterDidBlockLoadForFrameShared(m_process.copyRef(), unblockHandler, frameID);
}
#endif

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
void ProvisionalPageProxy::didCreateContextForVisibilityPropagation(LayerHostingContextID contextID)
{
    RELEASE_LOG_IF_ALLOWED(ProcessSwapping, "didCreateContextForVisibilityPropagation: contextID=%u", contextID);
    m_contextIDForVisibilityPropagation = contextID;
}
#endif

void ProvisionalPageProxy::unfreezeLayerTreeDueToSwipeAnimation()
{
    send(Messages::WebPage::UnfreezeLayerTreeDueToSwipeAnimation());
}

void ProvisionalPageProxy::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    ASSERT(decoder.messageReceiverName() == Messages::WebPageProxy::messageReceiverName());

    if (decoder.messageName() == Messages::WebPageProxy::DidStartProgress::name()
        || decoder.messageName() == Messages::WebPageProxy::DidChangeProgress::name()
        || decoder.messageName() == Messages::WebPageProxy::DidDestroyNavigation::name()
        || decoder.messageName() == Messages::WebPageProxy::DidFinishProgress::name()
        || decoder.messageName() == Messages::WebPageProxy::BackForwardAddItem::name()
        || decoder.messageName() == Messages::WebPageProxy::LogDiagnosticMessage::name()
        || decoder.messageName() == Messages::WebPageProxy::LogDiagnosticMessageWithEnhancedPrivacy::name()
        || decoder.messageName() == Messages::WebPageProxy::LogDiagnosticMessageWithValueDictionary::name()
        || decoder.messageName() == Messages::WebPageProxy::SetNetworkRequestsInProgress::name()
        || decoder.messageName() == Messages::WebPageProxy::WillGoToBackForwardListItem::name()
#if USE(QUICK_LOOK)
        || decoder.messageName() == Messages::WebPageProxy::DidStartLoadForQuickLookDocumentInMainFrame::name()
        || decoder.messageName() == Messages::WebPageProxy::DidFinishLoadForQuickLookDocumentInMainFrame::name()
#endif
        || decoder.messageName() == Messages::WebPageProxy::CreateInspectorTarget::name()
        || decoder.messageName() == Messages::WebPageProxy::DestroyInspectorTarget::name()
        || decoder.messageName() == Messages::WebPageProxy::SendMessageToInspectorFrontend::name()
        )
    {
        m_page.didReceiveMessage(connection, decoder);
        return;
    }

#if PLATFORM(COCOA)
    if (decoder.messageName() == Messages::WebPageProxy::RegisterWebProcessAccessibilityToken::name()) {
        IPC::handleMessage<Messages::WebPageProxy::RegisterWebProcessAccessibilityToken>(decoder, this, &ProvisionalPageProxy::registerWebProcessAccessibilityToken);
        return;
    }
#endif

    if (decoder.messageName() == Messages::WebPageProxy::StartURLSchemeTask::name()) {
        IPC::handleMessage<Messages::WebPageProxy::StartURLSchemeTask>(decoder, this, &ProvisionalPageProxy::startURLSchemeTask);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DecidePolicyForNavigationActionAsync::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DecidePolicyForNavigationActionAsync>(decoder, this, &ProvisionalPageProxy::decidePolicyForNavigationActionAsync);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DecidePolicyForResponse::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DecidePolicyForResponse>(decoder, this, &ProvisionalPageProxy::decidePolicyForResponse);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DidChangeProvisionalURLForFrame::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidChangeProvisionalURLForFrame>(decoder, this, &ProvisionalPageProxy::didChangeProvisionalURLForFrame);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DidNavigateWithNavigationData::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidNavigateWithNavigationData>(decoder, this, &ProvisionalPageProxy::didNavigateWithNavigationData);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DidPerformClientRedirect::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidPerformClientRedirect>(decoder, this, &ProvisionalPageProxy::didPerformClientRedirect);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DidCreateMainFrame::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidCreateMainFrame>(decoder, this, &ProvisionalPageProxy::didCreateMainFrame);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DidStartProvisionalLoadForFrame::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidStartProvisionalLoadForFrame>(decoder, this, &ProvisionalPageProxy::didStartProvisionalLoadForFrame);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DidFailProvisionalLoadForFrame::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidFailProvisionalLoadForFrame>(decoder, this, &ProvisionalPageProxy::didFailProvisionalLoadForFrame);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DidCommitLoadForFrame::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidCommitLoadForFrame>(decoder, this, &ProvisionalPageProxy::didCommitLoadForFrame);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DidReceiveServerRedirectForProvisionalLoadForFrame::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidReceiveServerRedirectForProvisionalLoadForFrame>(decoder, this, &ProvisionalPageProxy::didReceiveServerRedirectForProvisionalLoadForFrame);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DidPerformServerRedirect::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidPerformServerRedirect>(decoder, this, &ProvisionalPageProxy::didPerformServerRedirect);
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
        IPC::handleMessage<Messages::WebPageProxy::ContentFilterDidBlockLoadForFrame>(decoder, this, &ProvisionalPageProxy::contentFilterDidBlockLoadForFrame);
        return;
    }
#endif

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    if (decoder.messageName() == Messages::WebPageProxy::DidCreateContextForVisibilityPropagation::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DidCreateContextForVisibilityPropagation>(decoder, this, &ProvisionalPageProxy::didCreateContextForVisibilityPropagation);
        return;
    }
#endif

    LOG(ProcessSwapping, "Unhandled message %s from provisional process", description(decoder.messageName()));
}

void ProvisionalPageProxy::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& replyEncoder)
{
    if (decoder.messageName() == Messages::WebPageProxy::BackForwardGoToItem::name()) {
        IPC::handleMessageSynchronous<Messages::WebPageProxy::BackForwardGoToItem>(connection, decoder, replyEncoder, this, &ProvisionalPageProxy::backForwardGoToItem);
        return;
    }

    if (decoder.messageName() == Messages::WebPageProxy::DecidePolicyForNavigationActionSync::name()) {
        IPC::handleMessageSynchronous<Messages::WebPageProxy::DecidePolicyForNavigationActionSync>(connection, decoder, replyEncoder, this, &ProvisionalPageProxy::decidePolicyForNavigationActionSync);
        return;
    }

    m_page.didReceiveSyncMessage(connection, decoder, replyEncoder);
}

IPC::Connection* ProvisionalPageProxy::messageSenderConnection() const
{
    return m_process->connection();
}

uint64_t ProvisionalPageProxy::messageSenderDestinationID() const
{
    return m_webPageID.toUInt64();
}

bool ProvisionalPageProxy::sendMessage(std::unique_ptr<IPC::Encoder> encoder, OptionSet<IPC::SendOption> sendOptions, Optional<std::pair<CompletionHandler<void(IPC::Decoder*)>, uint64_t>>&& asyncReplyInfo)
{
    // Send messages via the WebProcessProxy instead of the IPC::Connection since AuxiliaryProcessProxy implements queueing of messages
    // while the process is still launching.
    return m_process->sendMessage(WTFMove(encoder), sendOptions, WTFMove(asyncReplyInfo));
}

} // namespace WebKit

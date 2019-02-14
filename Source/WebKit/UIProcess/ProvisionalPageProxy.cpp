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
#include "DrawingAreaProxy.h"
#include "FormDataReference.h"
#include "Logging.h"
#include "PageClient.h"
#include "URLSchemeTaskParameters.h"
#include "WebBackForwardList.h"
#include "WebBackForwardListItem.h"
#include "WebErrors.h"
#include "WebNavigationDataStore.h"
#include "WebNavigationState.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebPageProxyMessages.h"
#include "WebProcessMessages.h"
#include "WebProcessProxy.h"
#include <WebCore/ShouldTreatAsContinuingLoad.h>

namespace WebKit {

#define RELEASE_LOG_IF_ALLOWED(channel, fmt, ...) RELEASE_LOG_IF(m_page.isAlwaysOnLoggingAllowed(), channel, "%p - ProvisionalPageProxy::" fmt, this, ##__VA_ARGS__)
#define RELEASE_LOG_ERROR_IF_ALLOWED(channel, fmt, ...) RELEASE_LOG_ERROR_IF(m_page.isAlwaysOnLoggingAllowed(), channel, "%p - ProvisionalPageProxy::" fmt, this, ##__VA_ARGS__)

ProvisionalPageProxy::ProvisionalPageProxy(WebPageProxy& page, Ref<WebProcessProxy>&& process, std::unique_ptr<SuspendedPageProxy> suspendedPage, uint64_t navigationID, bool isServerRedirect, const WebCore::ResourceRequest& request, ProcessSwapRequestedByClient processSwapRequestedByClient)
    : m_page(page)
    , m_process(WTFMove(process))
    , m_navigationID(navigationID)
    , m_isServerRedirect(isServerRedirect)
    , m_request(request)
    , m_processSwapRequestedByClient(processSwapRequestedByClient)
#if PLATFORM(IOS_FAMILY)
    , m_suspensionToken(m_process->throttler().foregroundActivityToken())
#endif
{
    m_process->addMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_page.pageID(), *this);
    m_process->addProvisionalPageProxy(*this);

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

    initializeWebPage();
}

ProvisionalPageProxy::~ProvisionalPageProxy()
{
    m_process->removeProvisionalPageProxy(*this);

    if (m_wasCommitted)
        return;

    m_process->removeMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_page.pageID());
    m_process->send(Messages::WebPage::Close(), m_page.pageID());

    RunLoop::main().dispatch([process = m_process.copyRef()] {
        process->maybeShutDown();
    });
}

void ProvisionalPageProxy::processDidTerminate()
{
    RELEASE_LOG_ERROR_IF_ALLOWED(ProcessSwapping, "processDidTerminate: pageID = %" PRIu64, m_page.pageID());
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

    RELEASE_LOG_IF_ALLOWED(ProcessSwapping, "cancel: Simulating a didFailProvisionalLoadForFrame for pageID = %" PRIu64, m_page.pageID());
    ASSERT(m_mainFrame);
    auto error = WebKit::cancelledError(m_request);
    error.setType(WebCore::ResourceError::Type::Cancellation);
    didFailProvisionalLoadForFrame(m_mainFrame->frameID(), { }, m_navigationID, m_provisionalLoadURL, error, UserData { }); // Will delete |this|.
}

void ProvisionalPageProxy::processDidFinishLaunching()
{
    RELEASE_LOG_IF_ALLOWED(ProcessSwapping, "processDidFinishLaunching: pageID = %" PRIu64, m_page.pageID());
    finishInitializingWebPageAfterProcessLaunch();
}

void ProvisionalPageProxy::finishInitializingWebPageAfterProcessLaunch()
{
    ASSERT(m_process->state() == WebProcessProxy::State::Running);

    // FIXME: The WebPageProxy delays adding the visited link store until after the process has launched
    // so the ProvisionalPageProxy does the same. However, do we really need to?
    m_process->addVisitedLinkStore(m_page.visitedLinkStore());
}

void ProvisionalPageProxy::initializeWebPage()
{
    m_drawingArea = m_page.pageClient().createDrawingAreaProxy(m_process);

    auto parameters = m_page.creationParameters(m_process);
    parameters.isProcessSwap = true;
    m_process->send(Messages::WebProcess::CreateWebPage(m_page.pageID(), parameters), 0);

    if (m_process->state() == WebProcessProxy::State::Running)
        finishInitializingWebPageAfterProcessLaunch();
}

void ProvisionalPageProxy::loadData(API::Navigation& navigation, const IPC::DataReference& data, const String& MIMEType, const String& encoding, const String& baseURL, API::Object* userData, Optional<WebsitePoliciesData>&& websitePolicies)
{
    RELEASE_LOG_IF_ALLOWED(ProcessSwapping, "loadData: pageID = %" PRIu64, m_page.pageID());

    m_page.loadDataWithNavigationShared(m_process.copyRef(), navigation, data, MIMEType, encoding, baseURL, userData, WebCore::ShouldTreatAsContinuingLoad::Yes, WTFMove(websitePolicies));
}

void ProvisionalPageProxy::loadRequest(API::Navigation& navigation, WebCore::ResourceRequest&& request, WebCore::ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy, API::Object* userData, Optional<WebsitePoliciesData>&& websitePolicies)
{
    RELEASE_LOG_IF_ALLOWED(ProcessSwapping, "loadRequest: pageID = %" PRIu64, m_page.pageID());

    // If this is a client-side redirect continuing in a new process, then the new process will overwrite the fromItem's URL. In this case,
    // we need to make sure we update fromItem's processIdentifier as we want future navigations to this BackForward item to happen in the
    // new process.
    if (navigation.fromItem() && navigation.lockBackForwardList() == WebCore::LockBackForwardList::Yes)
        navigation.fromItem()->setLastProcessIdentifier(m_process->coreProcessIdentifier());

    m_page.loadRequestWithNavigationShared(m_process.copyRef(), navigation, WTFMove(request), shouldOpenExternalURLsPolicy, userData, WebCore::ShouldTreatAsContinuingLoad::Yes, WTFMove(websitePolicies));
}

void ProvisionalPageProxy::goToBackForwardItem(API::Navigation& navigation, WebBackForwardListItem& item, Optional<WebsitePoliciesData>&& websitePolicies)
{
    RELEASE_LOG_IF_ALLOWED(ProcessSwapping, "goToBackForwardItem: pageID = %" PRIu64, m_page.pageID());

    auto itemStates = m_page.backForwardList().filteredItemStates([this, targetItem = &item](auto& item) {
        if (auto* page = item.suspendedPage()) {
            if (&page->process() == m_process.ptr())
                return false;
        }
        return &item != targetItem;
    });
    m_process->send(Messages::WebPage::UpdateBackForwardListForReattach(WTFMove(itemStates)), m_page.pageID());
    m_process->send(Messages::WebPage::GoToBackForwardItem(navigation.navigationID(), item.itemID(), *navigation.backForwardFrameLoadType(), WebCore::ShouldTreatAsContinuingLoad::Yes, WTFMove(websitePolicies)), m_page.pageID());
    m_process->responsivenessTimer().start();
}

void ProvisionalPageProxy::didCreateMainFrame(uint64_t frameID)
{
    RELEASE_LOG_IF_ALLOWED(ProcessSwapping, "didCreateMainFrame: pageID = %" PRIu64 ", frameID = %" PRIu64, m_page.pageID(), frameID);
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

void ProvisionalPageProxy::didPerformClientRedirect(const String& sourceURLString, const String& destinationURLString, uint64_t frameID)
{
    m_page.didPerformClientRedirectShared(m_process.copyRef(), sourceURLString, destinationURLString, frameID);
}

void ProvisionalPageProxy::didStartProvisionalLoadForFrame(uint64_t frameID, uint64_t navigationID, URL&& url, URL&& unreachableURL, const UserData& userData)
{
    // If the previous provisional load used the same process, we may receive IPC for this previous provisional's main frame that we need to ignore.
    if (!m_mainFrame || m_mainFrame->frameID() != frameID)
        return;

    RELEASE_LOG_IF_ALLOWED(ProcessSwapping, "didStartProvisionalLoadForFrame: pageID = %" PRIu64 ", frameID = %" PRIu64 ", navigationID = %" PRIu64, m_page.pageID(), frameID, navigationID);
    ASSERT(m_provisionalLoadURL.isNull());
    m_provisionalLoadURL = url;

    // Merely following a server side redirect so there is no need to send a didStartProvisionalLoad again.
    if (m_isServerRedirect)
        return;

    // Clients expect the Page's main frame's expectedURL to be the provisional one when a provisional load is started.
    if (auto* pageMainFrame = m_page.mainFrame())
        pageMainFrame->didStartProvisionalLoad(url);

    m_page.didStartProvisionalLoadForFrameShared(m_process.copyRef(), frameID, navigationID, WTFMove(url), WTFMove(unreachableURL), userData);
}

void ProvisionalPageProxy::didFailProvisionalLoadForFrame(uint64_t frameID, const WebCore::SecurityOriginData& frameSecurityOrigin, uint64_t navigationID, const String& provisionalURL, const WebCore::ResourceError& error, const UserData& userData)
{
    RELEASE_LOG_ERROR_IF_ALLOWED(ProcessSwapping, "didFailProvisionalLoadForFrame: pageID = %" PRIu64 ", frameID = %" PRIu64 ", navigationID = %" PRIu64, m_page.pageID(), frameID, navigationID);
    m_provisionalLoadURL = { };

    // Make sure the Page's main frame's expectedURL gets cleared since we updated it in didStartProvisionalLoad.
    if (auto* pageMainFrame = m_page.mainFrame())
        pageMainFrame->didFailProvisionalLoad();

    m_page.didFailProvisionalLoadForFrameShared(m_process.copyRef(), frameID, frameSecurityOrigin, navigationID, provisionalURL, error, userData); // Will delete |this|.
}

void ProvisionalPageProxy::didCommitLoadForFrame(uint64_t frameID, uint64_t navigationID, const String& mimeType, bool frameHasCustomContentProvider, uint32_t frameLoadType, const WebCore::CertificateInfo& certificateInfo, bool containsPluginDocument, Optional<WebCore::HasInsecureContent> forcedHasInsecureContent, const UserData& userData)
{
    // If the previous provisional load used the same process, we may receive IPC for this previous provisional's main frame that we need to ignore.
    if (!m_mainFrame || m_mainFrame->frameID() != frameID)
        return;

    RELEASE_LOG_IF_ALLOWED(ProcessSwapping, "didCommitLoadForFrame: pageID = %" PRIu64 ", frameID = %" PRIu64 ", navigationID = %" PRIu64, m_page.pageID(), frameID, navigationID);
    m_provisionalLoadURL = { };
    m_process->removeMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_page.pageID());

    m_wasCommitted = true;
    m_page.commitProvisionalPage(frameID, navigationID, mimeType, frameHasCustomContentProvider, frameLoadType, certificateInfo, containsPluginDocument, forcedHasInsecureContent, userData); // Will delete |this|.
}

void ProvisionalPageProxy::didNavigateWithNavigationData(const WebNavigationDataStore& store, uint64_t frameID)
{
    m_page.didNavigateWithNavigationDataShared(m_process.copyRef(), store, frameID);
}

void ProvisionalPageProxy::didChangeProvisionalURLForFrame(uint64_t frameID, uint64_t navigationID, URL&& url)
{
    m_page.didChangeProvisionalURLForFrameShared(m_process.copyRef(), frameID, navigationID, WTFMove(url));
}

void ProvisionalPageProxy::decidePolicyForNavigationActionAsync(uint64_t frameID, WebCore::SecurityOriginData&& frameSecurityOrigin, WebCore::PolicyCheckIdentifier identifier,
    uint64_t navigationID, NavigationActionData&& navigationActionData, FrameInfoData&& frameInfoData, uint64_t originatingPageID, const WebCore::ResourceRequest& originalRequest,
    WebCore::ResourceRequest&& request, IPC::FormDataReference&& requestBody, WebCore::ResourceResponse&& redirectResponse, const UserData& userData, uint64_t listenerID)
{
    ASSERT(m_mainFrame);
    ASSERT(m_mainFrame->frameID() == frameID);
    m_page.decidePolicyForNavigationActionAsyncShared(m_process.copyRef(), frameID, WTFMove(frameSecurityOrigin), identifier, navigationID, WTFMove(navigationActionData),
        WTFMove(frameInfoData), originatingPageID, originalRequest, WTFMove(request), WTFMove(requestBody), WTFMove(redirectResponse), userData, listenerID);
}

void ProvisionalPageProxy::decidePolicyForResponse(uint64_t frameID, const WebCore::SecurityOriginData& frameSecurityOrigin, WebCore::PolicyCheckIdentifier identifier,
    uint64_t navigationID, const WebCore::ResourceResponse& response, const WebCore::ResourceRequest& request, bool canShowMIMEType, uint64_t listenerID, const UserData& userData)
{
    m_page.decidePolicyForResponseShared(m_process.copyRef(), frameID, frameSecurityOrigin, identifier, navigationID, response, request, canShowMIMEType, listenerID, userData);
}

void ProvisionalPageProxy::startURLSchemeTask(URLSchemeTaskParameters&& parameters)
{
    m_page.startURLSchemeTaskShared(m_process.copyRef(), WTFMove(parameters));
}

void ProvisionalPageProxy::backForwardGoToItem(const WebCore::BackForwardItemIdentifier& identifier, SandboxExtension::Handle& handle)
{
    m_page.backForwardGoToItemShared(m_process.copyRef(), identifier, handle);
}

#if PLATFORM(COCOA)
void ProvisionalPageProxy::registerWebProcessAccessibilityToken(const IPC::DataReference& data)
{
    m_accessibilityToken = data.vector();
}
#endif

void ProvisionalPageProxy::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    ASSERT(decoder.messageReceiverName() == Messages::WebPageProxy::messageReceiverName());

    if (decoder.messageName() == Messages::WebPageProxy::DidStartProgress::name()
        || decoder.messageName() == Messages::WebPageProxy::DidChangeProgress::name()
        || decoder.messageName() == Messages::WebPageProxy::BackForwardAddItem::name()
        || decoder.messageName() == Messages::WebPageProxy::LogDiagnosticMessage::name()
        || decoder.messageName() == Messages::WebPageProxy::LogDiagnosticMessageWithEnhancedPrivacy::name()
        || decoder.messageName() == Messages::WebPageProxy::SetNetworkRequestsInProgress::name()
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

    LOG(ProcessSwapping, "Unhandled message %s::%s from provisional process", decoder.messageReceiverName().toString().data(), decoder.messageName().toString().data());
}

void ProvisionalPageProxy::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& replyEncoder)
{
    if (decoder.messageName() == Messages::WebPageProxy::BackForwardGoToItem::name()) {
        IPC::handleMessageLegacySync<Messages::WebPageProxy::BackForwardGoToItem>(decoder, *replyEncoder, this, &ProvisionalPageProxy::backForwardGoToItem);
        return;
    }

    m_page.didReceiveSyncMessage(connection, decoder, replyEncoder);
}

} // namespace WebKit

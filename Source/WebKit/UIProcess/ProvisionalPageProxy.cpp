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
#include "WebBackForwardListCounts.h"
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

#define MESSAGE_CHECK(process, assertion) MESSAGE_CHECK_BASE(assertion, process->connection())

namespace WebKit {

using namespace WebCore;

#define PROVISIONALPAGEPROXY_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", PID=%i, navigationID=%" PRIu64 "] ProvisionalPageProxy::" fmt, this, m_page.identifier().toUInt64(), m_webPageID.toUInt64(), m_process->processIdentifier(), m_navigationID, ##__VA_ARGS__)
#define PROVISIONALPAGEPROXY_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%p - [pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", PID=%i, navigationID=%" PRIu64 "] ProvisionalPageProxy::" fmt, this, m_page.identifier().toUInt64(), m_webPageID.toUInt64(), m_process->processIdentifier(), m_navigationID, ##__VA_ARGS__)

ProvisionalPageProxy::ProvisionalPageProxy(WebPageProxy& page, Ref<WebProcessProxy>&& process, std::unique_ptr<SuspendedPageProxy> suspendedPage, uint64_t navigationID, bool isServerRedirect, const WebCore::ResourceRequest& request, ProcessSwapRequestedByClient processSwapRequestedByClient, bool isProcessSwappingOnNavigationResponse, API::WebsitePolicies* websitePolicies)
    : m_page(page)
    , m_webPageID(suspendedPage ? suspendedPage->webPageID() : PageIdentifier::generate())
    , m_process(WTFMove(process))
    , m_navigationID(navigationID)
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

    m_process->addMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_webPageID, *this);
    m_process->addProvisionalPageProxy(*this);

    m_websiteDataStore = m_process->websiteDataStore();
    ASSERT(m_websiteDataStore);
    if (m_websiteDataStore && m_websiteDataStore != &m_page.websiteDataStore())
        m_process->processPool().pageBeginUsingWebsiteDataStore(m_page.identifier(), *m_websiteDataStore);

    // If we are reattaching to a SuspendedPage, then the WebProcess' WebPage already exists and
    // WebPageProxy::didCreateMainFrame() will not be called to initialize m_mainFrame. In such
    // case, we need to initialize m_mainFrame to reflect the fact the the WebProcess' WebPage
    // already exists and already has a main frame.
    if (suspendedPage) {
        ASSERT(&suspendedPage->process() == m_process.ptr());
        suspendedPage->unsuspend();
        m_mainFrame = &suspendedPage->mainFrame();
    }

    initializeWebPage(websitePolicies);

    m_page.inspectorController().didCreateProvisionalPage(*this);
}

ProvisionalPageProxy::~ProvisionalPageProxy()
{
#if PLATFORM(GTK) || PLATFORM(WPE)
    if (m_accessibilityBindCompletionHandler)
        m_accessibilityBindCompletionHandler({ });
#endif

    if (!m_wasCommitted) {
        m_page.inspectorController().willDestroyProvisionalPage(*this);

        auto dataStore = m_process->websiteDataStore();
        if (dataStore && dataStore!= &m_page.websiteDataStore())
            m_process->processPool().pageEndUsingWebsiteDataStore(m_page.identifier(), *dataStore);

        m_process->removeMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_webPageID);
        send(Messages::WebPage::Close());
        m_process->removeVisitedLinkStoreUser(m_page.visitedLinkStore(), m_page.identifier());
    }

    m_process->removeProvisionalPageProxy(*this);
}

void ProvisionalPageProxy::processDidTerminate()
{
    PROVISIONALPAGEPROXY_RELEASE_LOG_ERROR(ProcessSwapping, "processDidTerminate:");
    m_page.provisionalProcessDidTerminate();
}

std::unique_ptr<DrawingAreaProxy> ProvisionalPageProxy::takeDrawingArea()
{
    return WTFMove(m_drawingArea);
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
        m_request,
        SecurityOriginData::fromURL(m_request.url()),
        { },
        m_mainFrame->frameID(),
        std::nullopt,
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

void ProvisionalPageProxy::loadData(API::Navigation& navigation, const IPC::DataReference& data, const String& mimeType, const String& encoding, const String& baseURL, API::Object* userData, WebCore::ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, std::optional<WebsitePoliciesData>&& websitePolicies, SubstituteData::SessionHistoryVisibility sessionHistoryVisibility)
{
    PROVISIONALPAGEPROXY_RELEASE_LOG(ProcessSwapping, "loadData:");
    ASSERT(shouldTreatAsContinuingLoad != WebCore::ShouldTreatAsContinuingLoad::No);

    m_page.loadDataWithNavigationShared(m_process.copyRef(), m_webPageID, navigation, data, mimeType, encoding, baseURL, userData, shouldTreatAsContinuingLoad, isNavigatingToAppBoundDomain, WTFMove(websitePolicies), navigation.lastNavigationAction().shouldOpenExternalURLsPolicy, sessionHistoryVisibility);
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

    m_page.loadRequestWithNavigationShared(m_process.copyRef(), m_webPageID, navigation, WTFMove(request), navigation.lastNavigationAction().shouldOpenExternalURLsPolicy, userData, shouldTreatAsContinuingLoad, isNavigatingToAppBoundDomain, WTFMove(websitePolicies), existingNetworkResourceLoadIdentifierToResume);
}

void ProvisionalPageProxy::goToBackForwardItem(API::Navigation& navigation, WebBackForwardListItem& item, RefPtr<API::WebsitePolicies>&& websitePolicies, WebCore::ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad, std::optional<NetworkResourceLoadIdentifier> existingNetworkResourceLoadIdentifierToResume)
{
    PROVISIONALPAGEPROXY_RELEASE_LOG(ProcessSwapping, "goToBackForwardItem: existingNetworkResourceLoadIdentifierToResume=%" PRIu64, valueOrDefault(existingNetworkResourceLoadIdentifierToResume).toUInt64());

    auto itemStates = m_page.backForwardList().filteredItemStates([this, targetItem = &item](auto& item) {
        if (auto* backForwardCacheEntry = item.backForwardCacheEntry()) {
            if (backForwardCacheEntry->processIdentifier() == m_process->coreProcessIdentifier())
                return false;
        }
        return &item != targetItem;
    });
    
    std::optional<WebsitePoliciesData> websitePoliciesData;
    if (websitePolicies)
        websitePoliciesData = websitePolicies->data();
    
    send(Messages::WebPage::UpdateBackForwardListForReattach(WTFMove(itemStates)));
    send(Messages::WebPage::GoToBackForwardItem(navigation.navigationID(), item.itemID(), *navigation.backForwardFrameLoadType(), shouldTreatAsContinuingLoad, WTFMove(websitePoliciesData), m_page.lastNavigationWasAppInitiated(), existingNetworkResourceLoadIdentifierToResume));
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

    m_mainFrame = WebFrameProxy::create(m_page, m_process, m_webPageID, frameID);

    // This navigation was destroyed so no need to notify of redirect.
    if (!m_page.navigationState().hasNavigation(m_navigationID))
        return;

    // Restore the main frame's committed URL as some clients may rely on it until the next load is committed.
    RefPtr previousMainFrame = m_page.mainFrame();
    if (previousMainFrame) {
        m_mainFrame->frameLoadState().setURL(previousMainFrame->url());
        previousMainFrame->transferNavigationCallbackToFrame(*m_mainFrame);
    }

    // Normally, notification of a server redirect comes from the WebContent process.
    // If we are process swapping in response to a server redirect then that notification will not come from the new WebContent process.
    // In this case we have the UIProcess synthesize the redirect notification at the appropriate time.
    if (m_isServerRedirect) {
        m_mainFrame->frameLoadState().didStartProvisionalLoad(m_request.url());
        m_page.didReceiveServerRedirectForProvisionalLoadForFrameShared(m_process.copyRef(), m_mainFrame->frameID(), m_navigationID, WTFMove(m_request), { });
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

    m_page.didPerformClientRedirectShared(m_process.copyRef(), sourceURLString, destinationURLString, frameID);
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
    if (auto* pageMainFrame = m_page.mainFrame())
        pageMainFrame->didStartProvisionalLoad(url);

    m_page.didStartProvisionalLoadForFrameShared(m_process.copyRef(), frameID, WTFMove(frameInfo), WTFMove(request), navigationID, WTFMove(url), WTFMove(unreachableURL), userData);
}

void ProvisionalPageProxy::didFailProvisionalLoadForFrame(FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, uint64_t navigationID, const String& provisionalURL, const WebCore::ResourceError& error, WebCore::WillContinueLoading willContinueLoading, const UserData& userData)
{
    if (!validateInput(frameID, navigationID))
        return;

    PROVISIONALPAGEPROXY_RELEASE_LOG_ERROR(ProcessSwapping, "didFailProvisionalLoadForFrame: frameID=%" PRIu64, frameID.object().toUInt64());
    ASSERT(!m_provisionalLoadURL.isNull());
    m_provisionalLoadURL = { };

    // Make sure the Page's main frame's expectedURL gets cleared since we updated it in didStartProvisionalLoad.
    if (auto* pageMainFrame = m_page.mainFrame())
        pageMainFrame->didFailProvisionalLoad();

    RefPtr frame = WebFrameProxy::webFrame(frameID);
    MESSAGE_CHECK(m_process, frame);
    m_page.didFailProvisionalLoadForFrameShared(m_process.copyRef(), *frame, WTFMove(frameInfo), WTFMove(request), navigationID, provisionalURL, error, willContinueLoading, userData); // May delete |this|.
}

void ProvisionalPageProxy::didCommitLoadForFrame(FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, uint64_t navigationID, const String& mimeType, bool frameHasCustomContentProvider, WebCore::FrameLoadType frameLoadType, const WebCore::CertificateInfo& certificateInfo, bool usedLegacyTLS, bool privateRelayed, bool containsPluginDocument, std::optional<WebCore::HasInsecureContent> forcedHasInsecureContent, WebCore::MouseEventPolicy mouseEventPolicy, const UserData& userData)
{
    if (!validateInput(frameID, navigationID))
        return;

    PROVISIONALPAGEPROXY_RELEASE_LOG(ProcessSwapping, "didCommitLoadForFrame: frameID=%" PRIu64, frameID.object().toUInt64());
    m_provisionalLoadURL = { };
    m_process->removeMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_webPageID);

    m_wasCommitted = true;
    m_page.commitProvisionalPage(frameID, WTFMove(frameInfo), WTFMove(request), navigationID, mimeType, frameHasCustomContentProvider, frameLoadType, certificateInfo, usedLegacyTLS, privateRelayed, containsPluginDocument, forcedHasInsecureContent, mouseEventPolicy, userData); // Will delete |this|.
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

void ProvisionalPageProxy::decidePolicyForResponse(FrameIdentifier frameID, FrameInfoData&& frameInfo, WebCore::PolicyCheckIdentifier identifier, uint64_t navigationID, const WebCore::ResourceResponse& response, const WebCore::ResourceRequest& request, bool canShowMIMEType, const String& downloadAttribute, uint64_t listenerID, const UserData& userData)
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

void ProvisionalPageProxy::logDiagnosticMessageFromWebProcess(const String& message, const String& description, WebCore::ShouldSample shouldSample)
{
    MESSAGE_CHECK(m_process, message.isAllASCII());

    m_page.logDiagnosticMessage(message, description, shouldSample);
}

void ProvisionalPageProxy::logDiagnosticMessageWithEnhancedPrivacyFromWebProcess(const String& message, const String& description, WebCore::ShouldSample shouldSample)
{
    MESSAGE_CHECK(m_process, message.isAllASCII());

    m_page.logDiagnosticMessageWithEnhancedPrivacy(message, description, shouldSample);
}

void ProvisionalPageProxy::logDiagnosticMessageWithValueDictionaryFromWebProcess(const String& message, const String& description, const WebCore::DiagnosticLoggingClient::ValueDictionary& valueDictionary, WebCore::ShouldSample shouldSample)
{
    MESSAGE_CHECK(m_process, message.isAllASCII());

    m_page.logDiagnosticMessageWithValueDictionary(message, description, valueDictionary, shouldSample);
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
    m_page.contentFilterDidBlockLoadForFrameShared(m_process.copyRef(), unblockHandler, frameID);
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
        || decoder.messageName() == Messages::WebPageProxy::DidDestroyNavigation::name()
        || decoder.messageName() == Messages::WebPageProxy::DidFinishProgress::name()
        || decoder.messageName() == Messages::WebPageProxy::BackForwardAddItem::name()
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

    if (decoder.messageName() == Messages::WebPageProxy::DecidePolicyForResponse::name()) {
        IPC::handleMessage<Messages::WebPageProxy::DecidePolicyForResponse>(connection, decoder, this, &ProvisionalPageProxy::decidePolicyForResponse);
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

    return m_page.didReceiveSyncMessage(connection, decoder, replyEncoder);
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

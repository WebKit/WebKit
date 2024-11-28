/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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
#include "WebFrameProxy.h"

#include "APINavigation.h"
#include "BrowsingContextGroup.h"
#include "Connection.h"
#include "DrawingAreaMessages.h"
#include "DrawingAreaProxy.h"
#include "FrameProcess.h"
#include "FrameTreeCreationParameters.h"
#include "FrameTreeNodeData.h"
#include "LoadedWebArchive.h"
#include "MessageSenderInlines.h"
#include "NetworkProcessMessages.h"
#include "ProvisionalFrameProxy.h"
#include "ProvisionalPageProxy.h"
#include "RemotePageProxy.h"
#include "WebBackForwardListFrameItem.h"
#include "WebFramePolicyListenerProxy.h"
#include "WebNavigationState.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebPageProxyMessages.h"
#include "WebPasteboardProxy.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebsiteDataStore.h"
#include "WebsitePoliciesData.h"
#include <WebCore/Image.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/NavigationScheduler.h>
#include <stdio.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/CheckedPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/WeakRef.h>
#include <wtf/text/WTFString.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, process().connection())

namespace WebKit {
using namespace WebCore;

class WebPageProxy;

static HashMap<FrameIdentifier, WeakRef<WebFrameProxy>>& allFrames()
{
    ASSERT(RunLoop::isMain());
    static NeverDestroyed<HashMap<FrameIdentifier, WeakRef<WebFrameProxy>>> map;
    return map.get();
}

WebFrameProxy* WebFrameProxy::webFrame(std::optional<FrameIdentifier> identifier)
{
    if (!identifier || !std::remove_reference_t<decltype(allFrames())>::isValidKey(*identifier))
        return nullptr;
    return allFrames().get(*identifier);
}

bool WebFrameProxy::canCreateFrame(FrameIdentifier frameID)
{
    return std::remove_reference_t<decltype(allFrames())>::isValidKey(frameID)
        && !allFrames().contains(frameID);
}

WebFrameProxy::WebFrameProxy(WebPageProxy& page, FrameProcess& process, FrameIdentifier frameID, SandboxFlags effectiveSandboxFlags, WebCore::ScrollbarMode scrollingMode, WebFrameProxy* opener, IsMainFrame isMainFrame)
    : m_page(page)
    , m_frameProcess(process)
    , m_opener(opener)
    , m_frameLoadState(isMainFrame)
    , m_frameID(frameID)
    , m_layerHostingContextIdentifier(LayerHostingContextIdentifier::generate())
    , m_effectiveSandboxFlags(effectiveSandboxFlags)
    , m_scrollingMode(scrollingMode)
{
    ASSERT(!allFrames().contains(frameID));
    allFrames().set(frameID, *this);
    WebProcessPool::statistics().wkFrameCount++;
}

WebFrameProxy::~WebFrameProxy()
{
    WebProcessPool::statistics().wkFrameCount--;
#if PLATFORM(GTK)
    WebPasteboardProxy::singleton().didDestroyFrame(this);
#endif

    if (m_navigateCallback)
        m_navigateCallback({ }, { });

    ASSERT(allFrames().get(m_frameID) == this);
    allFrames().remove(m_frameID);
}

WebPageProxy* WebFrameProxy::page() const
{
    return m_page.get();
}

RefPtr<WebPageProxy> WebFrameProxy::protectedPage() const
{
    return m_page.get();
}

std::unique_ptr<ProvisionalFrameProxy> WebFrameProxy::takeProvisionalFrame()
{
    return std::exchange(m_provisionalFrame, nullptr);
}

WebProcessProxy& WebFrameProxy::provisionalLoadProcess()
{
    if (m_provisionalFrame)
        return m_provisionalFrame->process();
    if (isMainFrame()) {
        if (WeakPtr provisionalPage = m_page ? m_page->provisionalPageProxy() : nullptr)
            return provisionalPage->process();
    }
    return process();
}

void WebFrameProxy::webProcessWillShutDown()
{
    for (auto& childFrame : std::exchange(m_childFrames, { }))
        childFrame->webProcessWillShutDown();

    m_page = nullptr;

    if (m_activeListener) {
        m_activeListener->ignore();
        m_activeListener = nullptr;
    }

    if (m_navigateCallback)
        m_navigateCallback({ }, { });
}

WebProcessProxy& WebFrameProxy::process() const
{
    return m_frameProcess->process();
}

ProcessID WebFrameProxy::processID() const
{
    return process().processID();
}

std::optional<PageIdentifier> WebFrameProxy::pageIdentifier() const
{
    if (!m_page)
        return { };
    return m_page->webPageIDInMainFrameProcess();
}

void WebFrameProxy::navigateServiceWorkerClient(WebCore::ScriptExecutionContextIdentifier documentIdentifier, const URL& url, CompletionHandler<void(std::optional<PageIdentifier>, std::optional<FrameIdentifier>)>&& callback)
{
    if (!m_page) {
        callback({ }, { });
        return;
    }

    protectedPage()->sendWithAsyncReplyToProcessContainingFrame(frameID(), Messages::WebPage::NavigateServiceWorkerClient { documentIdentifier, url }, CompletionHandler<void(WebCore::ScheduleLocationChangeResult)> { [this, protectedThis = Ref { *this }, url, callback = WTFMove(callback)](auto result) mutable {
        switch (result) {
        case WebCore::ScheduleLocationChangeResult::Stopped:
            callback({ }, { });
            return;
        case WebCore::ScheduleLocationChangeResult::Completed:
            callback(pageIdentifier(), frameID());
            return;
        case WebCore::ScheduleLocationChangeResult::Started:
            if (!m_activeListener) {
                callback(pageIdentifier(), frameID());
                return;
            }

            if (m_navigateCallback)
                m_navigateCallback({ }, { });

            m_navigateCallback = WTFMove(callback);
            return;
        }
    } });
}

void WebFrameProxy::bindAccessibilityFrameWithData(std::span<const uint8_t> data)
{
    if (!m_page)
        return;

    m_page->sendToProcessContainingFrame(m_frameID, Messages::WebProcess::BindAccessibilityFrameWithData(m_frameID, data));
}

void WebFrameProxy::loadURL(const URL& url, const String& referrer)
{
    if (RefPtr page = m_page.get())
        page->sendToProcessContainingFrame(m_frameID, Messages::WebPage::LoadURLInFrame(url, referrer, m_frameID));
}

void WebFrameProxy::loadData(std::span<const uint8_t> data, const String& type, const String& encodingName, const URL& baseURL)
{
    ASSERT(!isMainFrame());
    if (RefPtr page = m_page.get())
        page->sendToProcessContainingFrame(m_frameID, Messages::WebPage::LoadDataInFrame(data, type, encodingName, baseURL, m_frameID));
}
    
bool WebFrameProxy::canProvideSource() const
{
    return isDisplayingMarkupDocument();
}

bool WebFrameProxy::isDisplayingStandaloneImageDocument() const
{
    return Image::supportsType(m_MIMEType);
}

bool WebFrameProxy::isDisplayingStandaloneMediaDocument() const
{
    return MIMETypeRegistry::isSupportedMediaMIMEType(m_MIMEType);
}

bool WebFrameProxy::isDisplayingMarkupDocument() const
{
    // FIXME: This should be a call to a single MIMETypeRegistry function; adding a new one if needed.
    // FIXME: This is doing case sensitive comparisons on MIME types, should be using ASCII case insensitive instead.
    return m_MIMEType == "text/html"_s || m_MIMEType == "image/svg+xml"_s || m_MIMEType == "application/x-webarchive"_s || MIMETypeRegistry::isXMLMIMEType(m_MIMEType);
}

bool WebFrameProxy::isDisplayingPDFDocument() const
{
    return MIMETypeRegistry::isPDFMIMEType(m_MIMEType);
}

void WebFrameProxy::didStartProvisionalLoad(const URL& url)
{
    m_frameLoadState.didStartProvisionalLoad(url);
}

void WebFrameProxy::didExplicitOpen(URL&& url, String&& mimeType)
{
    m_MIMEType = WTFMove(mimeType);
    m_frameLoadState.didExplicitOpen(WTFMove(url));
}

void WebFrameProxy::didReceiveServerRedirectForProvisionalLoad(const URL& url)
{
    m_frameLoadState.didReceiveServerRedirectForProvisionalLoad(url);
}

void WebFrameProxy::didFailProvisionalLoad()
{
    m_frameLoadState.didFailProvisionalLoad();

    if (m_navigateCallback)
        m_navigateCallback({ }, { });
}

void WebFrameProxy::didCommitLoad(const String& contentType, const WebCore::CertificateInfo& certificateInfo, bool containsPluginDocument)
{
    m_frameLoadState.didCommitLoad();

    m_title = String();
    m_MIMEType = contentType;
    m_certificateInfo = certificateInfo;
    m_containsPluginDocument = containsPluginDocument;
}

void WebFrameProxy::didFinishLoad()
{
    m_frameLoadState.didFinishLoad();

    if (m_navigateCallback)
        m_navigateCallback(pageIdentifier(), frameID());
}

void WebFrameProxy::didFailLoad()
{
    m_frameLoadState.didFailLoad();

    if (m_navigateCallback)
        m_navigateCallback({ }, { });
}

void WebFrameProxy::didSameDocumentNavigation(const URL& url)
{
    m_frameLoadState.didSameDocumentNotification(url);
}

void WebFrameProxy::didChangeTitle(const String& title)
{
    m_title = title;
}

WebFramePolicyListenerProxy& WebFrameProxy::setUpPolicyListenerProxy(CompletionHandler<void(PolicyAction, API::WebsitePolicies*, ProcessSwapRequestedByClient, RefPtr<BrowsingWarning>&&, std::optional<NavigatingToAppBoundDomain>, WasNavigationIntercepted)>&& completionHandler, ShouldExpectSafeBrowsingResult expectSafeBrowsingResult, ShouldExpectAppBoundDomainResult expectAppBoundDomainResult, ShouldWaitForInitialLinkDecorationFilteringData shouldWaitForInitialLinkDecorationFilteringData)
{
    if (m_activeListener)
        m_activeListener->ignore();
    m_activeListener = WebFramePolicyListenerProxy::create([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] (PolicyAction action, API::WebsitePolicies* policies, ProcessSwapRequestedByClient processSwapRequestedByClient, RefPtr<BrowsingWarning>&& safeBrowsingWarning, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, WasNavigationIntercepted wasNavigationIntercepted) mutable {
        if (action != PolicyAction::Use && m_navigateCallback)
            m_navigateCallback(pageIdentifier(), frameID());

        completionHandler(action, policies, processSwapRequestedByClient, WTFMove(safeBrowsingWarning), isNavigatingToAppBoundDomain, wasNavigationIntercepted);
        m_activeListener = nullptr;
    }, expectSafeBrowsingResult, expectAppBoundDomainResult, shouldWaitForInitialLinkDecorationFilteringData);
    return *m_activeListener;
}

void WebFrameProxy::getWebArchive(CompletionHandler<void(API::Data*)>&& callback)
{
    if (RefPtr page = m_page.get())
        page->getWebArchiveOfFrame(this, WTFMove(callback));
    else
        callback(nullptr);
}

void WebFrameProxy::getMainResourceData(CompletionHandler<void(API::Data*)>&& callback)
{
    if (RefPtr page = m_page.get())
        page->getMainResourceDataOfFrame(this, WTFMove(callback));
    else
        callback(nullptr);
}

void WebFrameProxy::getResourceData(API::URL* resourceURL, CompletionHandler<void(API::Data*)>&& callback)
{
    if (RefPtr page = m_page.get())
        page->getResourceDataFromFrame(*this, resourceURL, WTFMove(callback));
    else
        callback(nullptr);
}

void WebFrameProxy::setUnreachableURL(const URL& unreachableURL)
{
    m_frameLoadState.setUnreachableURL(unreachableURL);
}

void WebFrameProxy::transferNavigationCallbackToFrame(WebFrameProxy& frame)
{
    frame.setNavigationCallback(WTFMove(m_navigateCallback));
}

void WebFrameProxy::setNavigationCallback(CompletionHandler<void(std::optional<WebCore::PageIdentifier>, std::optional<WebCore::FrameIdentifier>)>&& navigateCallback)
{
    ASSERT(!m_navigateCallback);
    m_navigateCallback = WTFMove(navigateCallback);
}

#if ENABLE(CONTENT_FILTERING)
bool WebFrameProxy::didHandleContentFilterUnblockNavigation(const ResourceRequest& request)
{
    if (!m_contentFilterUnblockHandler.canHandleRequest(request)) {
        m_contentFilterUnblockHandler = { };
        return false;
    }

    RefPtr page = m_page.get();
    ASSERT(page);
    m_contentFilterUnblockHandler.requestUnblockAsync([page](bool unblocked) {
        if (unblocked)
            page->reload({ });
    });
    return true;
}
#endif

#if PLATFORM(GTK)
void WebFrameProxy::collapseSelection()
{
    if (RefPtr page = m_page.get())
        page->sendToProcessContainingFrame(frameID(), Messages::WebPage::CollapseSelectionInFrame(m_frameID));
}
#endif

void WebFrameProxy::disconnect()
{
    if (RefPtr parentFrame = m_parentFrame.get())
        parentFrame->m_childFrames.remove(*this);
}

void WebFrameProxy::didCreateSubframe(WebCore::FrameIdentifier frameID, const String& frameName, SandboxFlags effectiveSandboxFlags, WebCore::ScrollbarMode scrollingMode)
{
    // The DecidePolicyForNavigationActionSync IPC is synchronous and may therefore get processed before the DidCreateSubframe one.
    // When this happens, decidePolicyForNavigationActionSync() calls didCreateSubframe() and we need to ignore the DidCreateSubframe
    // IPC when it later gets processed.
    if (WebFrameProxy::webFrame(frameID))
        return;

    RefPtr page = m_page.get();
    MESSAGE_CHECK(page);
    MESSAGE_CHECK(WebFrameProxy::canCreateFrame(frameID));
    MESSAGE_CHECK(frameID.processIdentifier() == process().coreProcessIdentifier());

    Ref child = WebFrameProxy::create(*page, m_frameProcess, frameID, effectiveSandboxFlags, scrollingMode, nullptr, IsMainFrame::No);
    child->m_parentFrame = *this;
    child->m_frameName = frameName;
    page->observeAndCreateRemoteSubframesInOtherProcesses(child, frameName);
    m_childFrames.add(WTFMove(child));
}

void WebFrameProxy::prepareForProvisionalLoadInProcess(WebProcessProxy& process, API::Navigation& navigation, BrowsingContextGroup& group, CompletionHandler<void()>&& completionHandler)
{
    if (isMainFrame())
        return completionHandler();

    Site navigationSite(navigation.currentRequest().url());
    RefPtr page = m_page.get();
    // FIXME: Main resource (of main or subframe) request redirects should go straight from the network to UI process so we don't need to make the processes for each domain in a redirect chain. <rdar://116202119>
    RegistrableDomain mainFrameDomain(page->mainFrame()->url());

    m_provisionalFrame = nullptr;
    m_provisionalFrame = makeUnique<ProvisionalFrameProxy>(*this, group.ensureProcessForSite(navigationSite, process, page->preferences()));
    page->websiteDataStore().protectedNetworkProcess()->addAllowedFirstPartyForCookies(process, mainFrameDomain, LoadedWebArchive::No, WTFMove(completionHandler));
}

void WebFrameProxy::commitProvisionalFrame(IPC::Connection& connection, FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, std::optional<WebCore::NavigationIdentifier> navigationID, const String& mimeType, bool frameHasCustomContentProvider, FrameLoadType frameLoadType, const CertificateInfo& certificateInfo, bool usedLegacyTLS, bool privateRelayed, bool containsPluginDocument, HasInsecureContent hasInsecureContent, MouseEventPolicy mouseEventPolicy, const UserData& userData)
{
    ASSERT(m_page);
    if (m_provisionalFrame) {
        protectedProcess()->send(Messages::WebPage::LoadDidCommitInAnotherProcess(frameID, m_layerHostingContextIdentifier), *webPageIDInCurrentProcess());
        if (RefPtr process = std::exchange(m_provisionalFrame, nullptr)->takeFrameProcess()) {
            m_frameProcess = process.releaseNonNull();
            if (m_remoteFrameSize)
                protectedProcess()->send(Messages::WebPage::UpdateFrameSize(frameID, *m_remoteFrameSize), *webPageIDInCurrentProcess());
        }
    }
    protectedPage()->didCommitLoadForFrame(connection, frameID, WTFMove(frameInfo), WTFMove(request), navigationID, mimeType, frameHasCustomContentProvider, frameLoadType, certificateInfo, usedLegacyTLS, privateRelayed, containsPluginDocument, hasInsecureContent, mouseEventPolicy, userData);
}

void WebFrameProxy::getFrameInfo(CompletionHandler<void(FrameTreeNodeData&&)>&& completionHandler)
{
    class FrameInfoCallbackAggregator : public RefCounted<FrameInfoCallbackAggregator> {
    public:
        static Ref<FrameInfoCallbackAggregator> create(CompletionHandler<void(FrameTreeNodeData&&)>&& completionHandler, size_t childCount) { return adoptRef(*new FrameInfoCallbackAggregator(WTFMove(completionHandler), childCount)); }
        void setCurrentFrameData(FrameInfoData&& data) { m_currentFrameData = WTFMove(data); }
        void addChildFrameData(size_t index, FrameTreeNodeData&& data) { m_childFrameData[index] = WTFMove(data); }
        ~FrameInfoCallbackAggregator()
        {
            // FIXME: We currently have to drop child frames that are currently not subframes of this frame
            // (e.g. they are in the back/forward cache). They really should not be part of m_childFrames.
            auto nonEmptyChildFrameData = WTF::compactMap(WTFMove(m_childFrameData), [](std::optional<FrameTreeNodeData>&& data) {
                return std::forward<decltype(data)>(data);
            });
            m_completionHandler(FrameTreeNodeData {
                WTFMove(m_currentFrameData),
                WTFMove(nonEmptyChildFrameData)
            });
        }

    private:
        FrameInfoCallbackAggregator(CompletionHandler<void(FrameTreeNodeData&&)>&& completionHandler, size_t childCount)
            : m_completionHandler(WTFMove(completionHandler))
            , m_childFrameData(childCount, { }) { }

        CompletionHandler<void(FrameTreeNodeData&&)> m_completionHandler;
        FrameInfoData m_currentFrameData;
        Vector<std::optional<FrameTreeNodeData>> m_childFrameData;
    };

    Ref aggregator = FrameInfoCallbackAggregator::create(WTFMove(completionHandler), m_childFrames.size());
    protectedProcess()->sendWithAsyncReply(Messages::WebPage::GetFrameInfo(m_frameID), [aggregator] (std::optional<FrameInfoData>&& info) {
        if (info)
            aggregator->setCurrentFrameData(WTFMove(*info));
    }, *webPageIDInCurrentProcess());

    bool isSiteIsolationEnabled = page() && page()->preferences().siteIsolationEnabled();
    size_t index = 0;
    for (Ref childFrame : m_childFrames) {
        childFrame->getFrameInfo([aggregator, index = index++, frameID = this->frameID(), isSiteIsolationEnabled] (FrameTreeNodeData&& data) {
            if (!data.info.frameID)
                return; // No WebFrame with the requested frameID in the WebProcess.

            // FIXME: m_childFrames currently contains iframes that are in the back/forward cache, not currently
            // connected to this parent frame. They should really not be part of m_childFrames anymore.
            // FIXME: With site isolation enabled, remote frames currently don't have a parentFrameID so we temporarily
            // ignore this check.
            if (data.info.parentFrameID != frameID && !isSiteIsolationEnabled)
                return;

            aggregator->addChildFrameData(index, WTFMove(data));
        });
    }
}

FrameTreeCreationParameters WebFrameProxy::frameTreeCreationParameters() const
{
    return {
        m_frameID,
        m_opener ? std::optional(m_opener->frameID()) : std::nullopt,
        m_frameName,
        WTF::map(m_childFrames, [] (auto& frame) {
            return frame->frameTreeCreationParameters();
        })
    };
}

void WebFrameProxy::setProcess(FrameProcess& process)
{
    ASSERT(m_frameProcess.ptr() != &process);
    m_frameProcess = process;
}

void WebFrameProxy::removeChildFrames()
{
    m_childFrames.clear();
}

bool WebFrameProxy::isFocused() const
{
    auto* webPage = page();
    return webPage && webPage->focusedFrame() == this;
}

void WebFrameProxy::remoteProcessDidTerminate(WebProcessProxy& process)
{
    for (Ref child : m_childFrames)
        child->remoteProcessDidTerminate(process);
    if (process.coreProcessIdentifier() != this->process().coreProcessIdentifier())
        return;
    if (m_frameLoadState.state() == FrameLoadState::State::Finished)
        return;
    notifyParentOfLoadCompletion(protectedProcess());
}

void WebFrameProxy::notifyParentOfLoadCompletion(WebProcessProxy& childFrameProcess)
{
    RefPtr parentFrame = this->parentFrame();
    if (!parentFrame)
        return;
    auto webPageID = parentFrame->webPageIDInCurrentProcess();
    if (!webPageID)
        return;
    Ref parentFrameProcess = parentFrame->process();
    if (parentFrameProcess->coreProcessIdentifier() == childFrameProcess.coreProcessIdentifier())
        return;
    parentFrameProcess->send(Messages::WebPage::DidFinishLoadInAnotherProcess(frameID()), *webPageID);
}

std::optional<WebCore::PageIdentifier> WebFrameProxy::webPageIDInCurrentProcess()
{
    if (m_page)
        return m_page->webPageIDInProcess(process());
    return std::nullopt;
}

auto WebFrameProxy::traverseNext() const -> TraversalResult
{
    if (RefPtr child = firstChild())
        return { WTFMove(child), DidWrap::No };

    RefPtr sibling = nextSibling();
    if (sibling)
        return { WTFMove(sibling), DidWrap::No };

    RefPtr frame = this;
    while (!sibling) {
        frame = frame->parentFrame();
        if (!frame)
            return { };
        sibling = frame->nextSibling();
    }

    if (frame)
        return { WTFMove(sibling), DidWrap::No };

    return { };
}

auto WebFrameProxy::traverseNext(CanWrap canWrap) const -> TraversalResult
{
    if (RefPtr frame = traverseNext().frame)
        return { WTFMove(frame), DidWrap::No };

    if (canWrap == CanWrap::Yes) {
        if (m_page)
            return { m_page->protectedMainFrame(), DidWrap::Yes };
    }
    return { };
}

auto WebFrameProxy::traversePrevious(CanWrap canWrap) -> TraversalResult
{
    if (RefPtr previousSibling = this->previousSibling())
        return { RefPtr { previousSibling->deepLastChild() }, DidWrap::No };
    if (RefPtr parent = parentFrame())
        return { WTFMove(parent), DidWrap::No };

    if (canWrap == CanWrap::Yes)
        return { RefPtr { deepLastChild() }, DidWrap::Yes };
    return { };
}

WebFrameProxy* WebFrameProxy::deepLastChild()
{
    RefPtr result = this;
    for (RefPtr last = lastChild(); last; last = last->lastChild())
        result = last;
    return result.get();
}

WebFrameProxy* WebFrameProxy::firstChild() const
{
    if (m_childFrames.isEmpty())
        return nullptr;
    return m_childFrames.first().ptr();
}

WebFrameProxy* WebFrameProxy::lastChild() const
{
    if (m_childFrames.isEmpty())
        return nullptr;
    return m_childFrames.last().ptr();
}

WebFrameProxy* WebFrameProxy::nextSibling() const
{
    if (!m_parentFrame)
        return nullptr;

    if (m_parentFrame->m_childFrames.last().ptr() == this)
        return nullptr;

    auto it = m_parentFrame->m_childFrames.find(this);
    if (it == m_childFrames.end()) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    return (++it)->ptr();
}

WebFrameProxy* WebFrameProxy::previousSibling() const
{
    if (!m_parentFrame)
        return nullptr;

    if (m_parentFrame->m_childFrames.first().ptr() == this)
        return nullptr;

    auto it = m_parentFrame->m_childFrames.find(this);
    if (it == m_childFrames.end()) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    return (--it)->ptr();
}

void WebFrameProxy::updateOpener(WebCore::FrameIdentifier newOpener)
{
    m_opener = WebFrameProxy::webFrame(newOpener);
}

WebFrameProxy& WebFrameProxy::rootFrame()
{
    Ref rootFrame = *this;
    while (rootFrame->m_parentFrame && rootFrame->m_parentFrame->process().coreProcessIdentifier() == process().coreProcessIdentifier())
        rootFrame = *rootFrame->m_parentFrame;
    return rootFrame;
}

bool WebFrameProxy::isMainFrame() const
{
    return m_frameLoadState.isMainFrame() == IsMainFrame::Yes;
}

void WebFrameProxy::setPendingChildBackForwardItem(WebBackForwardListFrameItem* pendingChildBackForwardItem)
{
    m_pendingChildBackForwardItem = pendingChildBackForwardItem;
}

WebBackForwardListFrameItem* WebFrameProxy::takePendingChildBackForwardItem()
{
    return std::exchange(m_pendingChildBackForwardItem, nullptr).get();
}

void WebFrameProxy::updateScrollingMode(WebCore::ScrollbarMode scrollingMode)
{
    m_scrollingMode = scrollingMode;
    if (RefPtr page = m_page.get())
        page->sendToProcessContainingFrame(m_frameID, Messages::WebPage::UpdateFrameScrollingMode(m_frameID, scrollingMode));
}

} // namespace WebKit

#undef MESSAGE_CHECK

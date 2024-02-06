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
#include "Connection.h"
#include "DrawingAreaMessages.h"
#include "DrawingAreaProxy.h"
#include "FrameTreeCreationParameters.h"
#include "FrameTreeNodeData.h"
#include "LoadedWebArchive.h"
#include "LocalFrameCreationParameters.h"
#include "MessageSenderInlines.h"
#include "NetworkProcessMessages.h"
#include "ProvisionalFrameProxy.h"
#include "ProvisionalPageProxy.h"
#include "RemotePageProxy.h"
#include "WebFramePolicyListenerProxy.h"
#include "WebNavigationState.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebPageProxyMessages.h"
#include "WebPasteboardProxy.h"
#include "WebProcessPool.h"
#include "WebsiteDataStore.h"
#include "WebsitePoliciesData.h"
#include <WebCore/Image.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/NavigationScheduler.h>
#include <stdio.h>
#include <wtf/CheckedPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/WeakRef.h>
#include <wtf/text/WTFString.h>

#define MESSAGE_CHECK(process, assertion) MESSAGE_CHECK_BASE(assertion, process->connection())

namespace WebKit {
using namespace WebCore;

class WebPageProxy;

static HashMap<FrameIdentifier, WeakRef<WebFrameProxy>>& allFrames()
{
    ASSERT(RunLoop::isMain());
    static NeverDestroyed<HashMap<FrameIdentifier, WeakRef<WebFrameProxy>>> map;
    return map.get();
}

WebFrameProxy* WebFrameProxy::webFrame(FrameIdentifier identifier)
{
    if (!std::remove_reference_t<decltype(allFrames())>::isValidKey(identifier))
        return nullptr;
    return allFrames().get(identifier);
}

bool WebFrameProxy::canCreateFrame(FrameIdentifier frameID)
{
    return std::remove_reference_t<decltype(allFrames())>::isValidKey(frameID)
        && !allFrames().contains(frameID);
}

WebFrameProxy::WebFrameProxy(WebPageProxy& page, WebProcessProxy& process, FrameIdentifier frameID)
    : m_page(page)
    , m_process(process)
    , m_frameID(frameID)
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

std::unique_ptr<ProvisionalFrameProxy> WebFrameProxy::takeProvisionalFrame()
{
    return std::exchange(m_provisionalFrame, nullptr);
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

bool WebFrameProxy::isMainFrame() const
{
    if (!m_page)
        return false;

    return this == m_page->mainFrame() || (m_page->provisionalPageProxy() && this == m_page->provisionalPageProxy()->mainFrame());
}

ProcessID WebFrameProxy::processID() const
{
    return m_process->processID();
}

std::optional<PageIdentifier> WebFrameProxy::pageIdentifier() const
{
    if (!m_page)
        return { };
    return m_page->webPageID();
}

void WebFrameProxy::navigateServiceWorkerClient(WebCore::ScriptExecutionContextIdentifier documentIdentifier, const URL& url, CompletionHandler<void(std::optional<PageIdentifier>, std::optional<FrameIdentifier>)>&& callback)
{
    if (!m_page) {
        callback({ }, { });
        return;
    }

    m_page->sendWithAsyncReply(Messages::WebPage::NavigateServiceWorkerClient { documentIdentifier, url }, [this, protectedThis = Ref { *this }, url, callback = WTFMove(callback)](auto result) mutable {
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
    });
}

void WebFrameProxy::loadURL(const URL& url, const String& referrer)
{
    if (!m_page)
        return;

    m_page->send(Messages::WebPage::LoadURLInFrame(url, referrer, m_frameID));
}

void WebFrameProxy::loadData(const IPC::DataReference& data, const String& MIMEType, const String& encodingName, const URL& baseURL)
{
    ASSERT(!isMainFrame());
    if (!m_page)
        return;

    m_page->send(Messages::WebPage::LoadDataInFrame(data, MIMEType, encodingName, baseURL, m_frameID));
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
    return MIMETypeRegistry::isPDFOrPostScriptMIMEType(m_MIMEType);
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

WebFramePolicyListenerProxy& WebFrameProxy::setUpPolicyListenerProxy(CompletionHandler<void(PolicyAction, API::WebsitePolicies*, ProcessSwapRequestedByClient, RefPtr<SafeBrowsingWarning>&&, std::optional<NavigatingToAppBoundDomain>, WasNavigationIntercepted)>&& completionHandler, ShouldExpectSafeBrowsingResult expectSafeBrowsingResult, ShouldExpectAppBoundDomainResult expectAppBoundDomainResult, ShouldWaitForInitialLinkDecorationFilteringData shouldWaitForInitialLinkDecorationFilteringData)
{
    if (m_activeListener)
        m_activeListener->ignore();
    m_activeListener = WebFramePolicyListenerProxy::create([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] (PolicyAction action, API::WebsitePolicies* policies, ProcessSwapRequestedByClient processSwapRequestedByClient, RefPtr<SafeBrowsingWarning>&& safeBrowsingWarning, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, WasNavigationIntercepted wasNavigationIntercepted) mutable {
        if (action != PolicyAction::Use && m_navigateCallback)
            m_navigateCallback(pageIdentifier(), frameID());

        completionHandler(action, policies, processSwapRequestedByClient, WTFMove(safeBrowsingWarning), isNavigatingToAppBoundDomain, wasNavigationIntercepted);
        m_activeListener = nullptr;
    }, expectSafeBrowsingResult, expectAppBoundDomainResult, shouldWaitForInitialLinkDecorationFilteringData);
    return *m_activeListener;
}

void WebFrameProxy::getWebArchive(CompletionHandler<void(API::Data*)>&& callback)
{
    if (!m_page)
        return callback(nullptr);
    m_page->getWebArchiveOfFrame(this, WTFMove(callback));
}

void WebFrameProxy::getMainResourceData(CompletionHandler<void(API::Data*)>&& callback)
{
    if (!m_page)
        return callback(nullptr);
    m_page->getMainResourceDataOfFrame(this, WTFMove(callback));
}

void WebFrameProxy::getResourceData(API::URL* resourceURL, CompletionHandler<void(API::Data*)>&& callback)
{
    if (!m_page)
        return callback(nullptr);
    m_page->getResourceDataFromFrame(*this, resourceURL, WTFMove(callback));
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

    RefPtr<WebPageProxy> page { m_page.get() };
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
    if (!m_page)
        return;

    m_page->send(Messages::WebPage::CollapseSelectionInFrame(m_frameID));
}
#endif

void WebFrameProxy::disconnect()
{
    if (m_parentFrame)
        m_parentFrame->m_childFrames.remove(*this);
}

void WebFrameProxy::didCreateSubframe(WebCore::FrameIdentifier frameID, const String& frameName)
{
    // The DecidePolicyForNavigationActionSync IPC is synchronous and may therefore get processed before the DidCreateSubframe one.
    // When this happens, decidePolicyForNavigationActionSync() calls didCreateSubframe() and we need to ignore the DidCreateSubframe
    // IPC when it later gets processed.
    if (WebFrameProxy::webFrame(frameID))
        return;

    MESSAGE_CHECK(m_process, m_page);
    MESSAGE_CHECK(m_process, WebFrameProxy::canCreateFrame(frameID));
    MESSAGE_CHECK(m_process, frameID.processIdentifier() == m_process->coreProcessIdentifier());

    auto child = WebFrameProxy::create(*m_page, m_process, frameID);
    child->m_parentFrame = *this;
    child->m_frameName = frameName;
    if (m_page)
        m_page->createRemoteSubframesInOtherProcesses(child, frameName);
    m_childFrames.add(WTFMove(child));
}

void WebFrameProxy::prepareForProvisionalNavigationInProcess(WebProcessProxy& process, const API::Navigation& navigation, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!isMainFrame());

    if (m_provisionalFrame && m_provisionalFrame->process().processID() == process.processID())
        return completionHandler();

    if (process.coreProcessIdentifier() == m_process->coreProcessIdentifier()) {
        m_provisionalFrame = nullptr;
        return completionHandler();
    }

    RegistrableDomain navigationDomain(navigation.currentRequest().url());
    if (!m_provisionalFrame || navigation.currentRequestIsCrossSiteRedirect()) {
        // FIXME: Main resource (of main or subframe) request redirects should go straight from the network to UI process so we don't need to make the processes for each domain in a redirect chain. <rdar://116202119>
        RefPtr remotePageProxy = m_page->remotePageProxyForRegistrableDomain(navigationDomain);
        RegistrableDomain mainFrameDomain(m_page->mainFrame()->url());

        if (remotePageProxy)
            ASSERT(remotePageProxy->process().coreProcessIdentifier() == process.coreProcessIdentifier());
        else if (navigationDomain != mainFrameDomain) {
            remotePageProxy = RemotePageProxy::create(*m_page, process, navigationDomain);
            remotePageProxy->injectPageIntoNewProcess();
        }

        m_provisionalFrame = makeUnique<ProvisionalFrameProxy>(*this, process, WTFMove(remotePageProxy));
        page()->websiteDataStore().networkProcess().sendWithAsyncReply(Messages::NetworkProcess::AddAllowedFirstPartyForCookies(process.coreProcessIdentifier(), mainFrameDomain, LoadedWebArchive::No), WTFMove(completionHandler));
    }

    if (m_process->processID() != process.processID()) {
        LocalFrameCreationParameters localFrameCreationParameters {
            m_provisionalFrame->layerHostingContextIdentifier()
        };
        process.send(Messages::WebPage::TransitionFrameToLocal(localFrameCreationParameters, frameID()), page()->webPageIDInProcessForDomain(navigationDomain));
    }

    if (completionHandler)
        completionHandler();
}

void WebFrameProxy::commitProvisionalFrame(FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, uint64_t navigationID, const String& mimeType, bool frameHasCustomContentProvider, WebCore::FrameLoadType frameLoadType, const WebCore::CertificateInfo& certificateInfo, bool usedLegacyTLS, bool privateRelayed, bool containsPluginDocument, WebCore::HasInsecureContent hasInsecureContent, WebCore::MouseEventPolicy mouseEventPolicy, const UserData& userData)
{
    ASSERT(m_page);
    if (m_provisionalFrame) {
        m_process->send(Messages::WebPage::DidCommitLoadInAnotherProcess(frameID, m_provisionalFrame->layerHostingContextIdentifier()), m_page->webPageID());
        m_process = m_provisionalFrame->process();
        if (m_remotePageProxy)
            m_remotePageProxy->removeFrame(*this);
        m_remotePageProxy = m_provisionalFrame->takeRemotePageProxy();
        if (m_remotePageProxy)
            m_remotePageProxy->addFrame(*this);
        m_provisionalFrame = nullptr;
    }
    m_page->didCommitLoadForFrame(frameID, WTFMove(frameInfo), WTFMove(request), navigationID, mimeType, frameHasCustomContentProvider, frameLoadType, certificateInfo, usedLegacyTLS, privateRelayed, containsPluginDocument, hasInsecureContent, mouseEventPolicy, userData);
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

    auto aggregator = FrameInfoCallbackAggregator::create(WTFMove(completionHandler), m_childFrames.size());
    m_process->sendWithAsyncReply(Messages::WebPage::GetFrameInfo(m_frameID), [aggregator] (std::optional<FrameInfoData>&& info) {
        if (info)
            aggregator->setCurrentFrameData(WTFMove(*info));
    }, m_page->webPageID());

    bool isSiteIsolationEnabled = page() && page()->preferences().siteIsolationEnabled();
    size_t index = 0;
    for (auto& childFrame : m_childFrames) {
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
        m_frameName,
        WTF::map(m_childFrames, [] (auto& frame) {
            return frame->frameTreeCreationParameters();
        })
    };
}

RefPtr<RemotePageProxy> WebFrameProxy::remotePageProxy()
{
    return m_remotePageProxy;
}

bool WebFrameProxy::isFocused() const
{
    auto* webPage = page();
    if (!webPage)
        return false;

    return webPage->focusedFrame() == this;
}

void WebFrameProxy::remoteProcessDidTerminate()
{
    if (m_frameLoadState.state() == FrameLoadState::State::Finished)
        return;
    notifyParentOfLoadCompletion(m_process);
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
    if (m_remotePageProxy)
        return m_remotePageProxy->pageID();
    if (m_page)
        return m_page->webPageID();
    return std::nullopt;
}

auto WebFrameProxy::traverseNext() const -> TraversalResult
{
    if (RefPtr child = firstChild())
        return { child, DidWrap::No };

    RefPtr sibling = nextSibling();
    if (sibling)
        return { sibling.get(), DidWrap::No };

    RefPtr frame = this;
    while (!sibling) {
        frame = frame->parentFrame();
        if (!frame)
            return { };
        sibling = frame->nextSibling();
    }

    if (frame)
        return { sibling.get(), DidWrap::No };

    return { };
}

auto WebFrameProxy::traverseNext(CanWrap canWrap) const -> TraversalResult
{
    if (RefPtr result = traverseNext().frame)
        return { result, DidWrap::No };

    if (canWrap == CanWrap::Yes) {
        if (m_page)
            return { m_page->mainFrame(), DidWrap::Yes };
    }
    return { };
}

auto WebFrameProxy::traversePrevious(CanWrap canWrap) -> TraversalResult
{
    if (RefPtr previousSibling = this->previousSibling())
        return { previousSibling->deepLastChild(), DidWrap::No };
    if (RefPtr parent = parentFrame())
        return { parent, DidWrap::No };

    if (canWrap == CanWrap::Yes)
        return { deepLastChild(), DidWrap::Yes };
    return { };
}

RefPtr<WebFrameProxy> WebFrameProxy::deepLastChild()
{
    RefPtr result = this;
    for (RefPtr last = lastChild(); last; last = last->lastChild())
        result = last;
    return result.get();
}

RefPtr<WebFrameProxy> WebFrameProxy::firstChild() const
{
    if (m_childFrames.isEmpty())
        return nullptr;
    return m_childFrames.first().ptr();
}

RefPtr<WebFrameProxy> WebFrameProxy::lastChild() const
{
    if (m_childFrames.isEmpty())
        return nullptr;
    return m_childFrames.last().ptr();
}

RefPtr<WebFrameProxy> WebFrameProxy::nextSibling() const
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

RefPtr<WebFrameProxy> WebFrameProxy::previousSibling() const
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

Ref<WebFrameProxy> WebFrameProxy::rootFrame()
{
    Ref rootFrame = *this;
    while (rootFrame->m_parentFrame && rootFrame->m_parentFrame->m_process == m_process)
        rootFrame = *rootFrame->m_parentFrame;
    return rootFrame;
}

} // namespace WebKit

#undef MESSAGE_CHECK

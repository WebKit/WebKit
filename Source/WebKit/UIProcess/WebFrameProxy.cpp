/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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
#include "APIUIClient.h"
#include "BrowsingContextGroup.h"
#include "Connection.h"
#include "DrawingAreaMessages.h"
#include "DrawingAreaProxy.h"
#include "FrameProcess.h"
#include "FrameTreeCreationParameters.h"
#include "FrameTreeNodeData.h"
#include "JSHandleInfo.h"
#include "LoadedWebArchive.h"
#include "MessageSenderInlines.h"
#include "NetworkProcessMessages.h"
#include "ProvisionalFrameCreationParameters.h"
#include "ProvisionalFrameProxy.h"
#include "ProvisionalPageProxy.h"
#include "RemotePageProxy.h"
#include "WebBackForwardListFrameItem.h"
#include "WebFrameInspectorTarget.h"
#include "WebFrameMessages.h"
#include "WebFramePolicyListenerProxy.h"
#include "WebNavigationState.h"
#include "WebPageInspectorController.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebPageProxyMessages.h"
#include "WebPasteboardProxy.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebsiteDataStore.h"
#include "WebsitePoliciesData.h"
#include <WebCore/FocusController.h>
#include <WebCore/FocusControllerTypes.h>
#include <WebCore/FocusEventData.h>
#include <WebCore/FrameTreeSyncData.h>
#include <WebCore/Image.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/NavigationScheduler.h>
#include <WebCore/RemoteFrameLayoutInfo.h>
#include <WebCore/ShareableBitmapHandle.h>
#include <WebCore/WebKitJSHandle.h>
#include <stdio.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/CheckedPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/WeakRef.h>
#include <wtf/text/WTFString.h>

#if ENABLE(WEBDRIVER_BIDI)
#include "WebAutomationSession.h"
#endif

#if ENABLE(APPLE_PAY)
#include <WebCore/PaymentSession.h>
#endif

#if HAVE(WEBCONTENTRESTRICTIONS)
#include <WebCore/ParentalControlsURLFilterParameters.h>
#endif

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

WebFrameProxy::WebFrameProxy(WebPageProxy& page, FrameProcess& process, FrameIdentifier frameID, SandboxFlags effectiveSandboxFlags, ReferrerPolicy effectiveReferrerPolicy, WebCore::ScrollbarMode scrollingMode, WebFrameProxy* opener, IsMainFrame isMainFrame)
    : m_page(page)
    , m_frameProcess(process)
    , m_opener(opener)
    , m_frameLoadState(isMainFrame)
    , m_frameID(frameID)
    , m_layerHostingContextIdentifier(LayerHostingContextIdentifier::generate())
    , m_effectiveSandboxFlags(effectiveSandboxFlags)
    , m_effectiveReferrerPolicy(effectiveReferrerPolicy)
    , m_scrollingMode(scrollingMode)
{
    ASSERT(!allFrames().contains(frameID));
    allFrames().set(frameID, *this);
    WebProcessPool::statistics().wkFrameCount++;

    page.inspectorController().createWebFrameInspectorTarget(*this, WebFrameInspectorTarget::toTargetID(frameID));

    protect(m_frameProcess)->incrementFrameCount();
}

WebFrameProxy::~WebFrameProxy()
{
    if (RefPtr page = m_page.get())
        page->inspectorController().destroyInspectorTarget(WebFrameInspectorTarget::toTargetID(frameID()));

    WebProcessPool::statistics().wkFrameCount--;
#if PLATFORM(GTK)
    WebPasteboardProxy::singleton().didDestroyFrame(this);
#endif

    if (m_navigateCallback)
        m_navigateCallback({ }, { });

    ASSERT(allFrames().get(m_frameID) == this);
    allFrames().remove(m_frameID);

    protect(m_frameProcess)->decrementFrameCount();
}

template<typename M, typename C> void WebFrameProxy::sendWithAsyncReply(M&& message, C&& completionHandler)
{
    // Use AuxiliaryProcessProxy::sendMessage to handle process crashes and launches more gracefully.
    protect(process())->sendWithAsyncReply(std::forward<M>(message), std::forward<C>(completionHandler), m_frameID);
}

template<typename M> void WebFrameProxy::send(M&& message)
{
    // Use AuxiliaryProcessProxy::sendMessage to handle process crashes and launches more gracefully.
    protect(process())->send(std::forward<M>(message), m_frameID);
}

WebPageProxy* WebFrameProxy::page() const
{
    return m_page.get();
}

RefPtr<ProvisionalFrameProxy> WebFrameProxy::takeProvisionalFrame()
{
    return std::exchange(m_provisionalFrame, nullptr);
}

WebProcessProxy& WebFrameProxy::provisionalLoadProcess()
{
    if (RefPtr provisionalFrame = m_provisionalFrame)
        return provisionalFrame->process();
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

    if (RefPtr page = m_page.get())
        page->inspectorController().destroyInspectorTarget(WebFrameInspectorTarget::toTargetID(frameID()));

    m_page = nullptr;

    if (RefPtr activeListener = m_activeListener) {
        activeListener->ignore();
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

    protect(page())->sendWithAsyncReplyToProcessContainingFrame(frameID(), Messages::WebPage::NavigateServiceWorkerClient { documentIdentifier, url }, CompletionHandler<void(WebCore::ScheduleLocationChangeResult)> { [this, protectedThis = Ref { *this }, callback = WTF::move(callback)](auto result) mutable {
        switch (result) {
        case WebCore::ScheduleLocationChangeResult::Stopped:
            callback({ }, { });
            return;
        case WebCore::ScheduleLocationChangeResult::Completed:
            callback(pageIdentifier(), frameID());
            return;
        case WebCore::ScheduleLocationChangeResult::Started:
            if (m_navigateCallback)
                m_navigateCallback({ }, { });

            m_navigateCallback = WTF::move(callback);
            return;
        }
    } });
}

void WebFrameProxy::bindAccessibilityFrameWithData(std::span<const uint8_t> data)
{
    if (RefPtr page = m_page.get())
        page->sendToProcessContainingFrame(m_frameID, Messages::WebProcess::BindAccessibilityFrameWithData(m_frameID, data));
}

void WebFrameProxy::loadURL(const URL& url, const String& referrer)
{
    if (RefPtr page = m_page.get())
        page->sendToProcessContainingFrame(m_frameID, Messages::WebPage::LoadURLInFrame(url, referrer, m_frameID));
}

void WebFrameProxy::loadData(std::span<const uint8_t> data, const String& type, const String& encodingName, const URL& baseURL)
{
    ASSERT(!isMainFrame());
    if (RefPtr page = m_page.get()) {
        if (baseURL.protocolIsFile())
            protect(process())->addPreviouslyApprovedFileURL(baseURL);
        page->sendToProcessContainingFrame(m_frameID, Messages::WebPage::LoadDataInFrame(data, type, encodingName, baseURL, m_frameID));
    }
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

void WebFrameProxy::didStartProvisionalLoad(URL&& url)
{
    m_frameLoadState.didStartProvisionalLoad(WTF::move(url));
}

void WebFrameProxy::didExplicitOpen(URL&& url, String&& mimeType)
{
    m_MIMEType = WTF::move(mimeType);
    m_frameLoadState.didExplicitOpen(WTF::move(url));
}

void WebFrameProxy::didReceiveServerRedirectForProvisionalLoad(URL&& url)
{
    m_frameLoadState.didReceiveServerRedirectForProvisionalLoad(WTF::move(url));
}

void WebFrameProxy::didFailProvisionalLoad()
{
    m_frameLoadState.didFailProvisionalLoad();

    if (m_navigateCallback)
        m_navigateCallback({ }, { });
}

void WebFrameProxy::didCommitLoad(const String& contentType, const WebCore::CertificateInfo& certificateInfo, bool containsPluginDocument, DocumentSecurityPolicy&& documentSecurityPolicy)
{
    m_frameLoadState.didCommitLoad();

    m_title = String();
    m_MIMEType = contentType;
    m_certificateInfo = certificateInfo;
    m_containsPluginDocument = containsPluginDocument;
    m_documentSecurityPolicy = WTF::move(documentSecurityPolicy);

    RefPtr webPage = page();
    if (webPage && protect(webPage->preferences())->siteIsolationEnabled())
        broadcastFrameTreeSyncData(calculateFrameTreeSyncData());
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

void WebFrameProxy::didSameDocumentNavigation(URL&& url)
{
    m_frameLoadState.didSameDocumentNotification(WTF::move(url));
}

void WebFrameProxy::didChangeTitle(String&& title)
{
    m_title = WTF::move(title);
}

WebFramePolicyListenerProxy& WebFrameProxy::setUpPolicyListenerProxy(CompletionHandler<void(PolicyAction, API::WebsitePolicies*, ProcessSwapRequestedByClient, std::optional<NavigatingToAppBoundDomain>, WasNavigationIntercepted)>&& completionHandler, ShouldExpectSafeBrowsingResult expectSafeBrowsingResult, ShouldExpectAppBoundDomainResult expectAppBoundDomainResult, ShouldWaitForInitialLinkDecorationFilteringData shouldWaitForInitialLinkDecorationFilteringData, ShouldWaitForSiteHasStorageCheck shouldWaitForSiteHasStorageCheck)
{
    if (RefPtr previousListener = m_activeListener)
        previousListener->ignore();
    m_activeListener = WebFramePolicyListenerProxy::create([this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)] (PolicyAction action, API::WebsitePolicies* policies, ProcessSwapRequestedByClient processSwapRequestedByClient, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain, WasNavigationIntercepted wasNavigationIntercepted) mutable {
        if (action != PolicyAction::Use && m_navigateCallback)
            m_navigateCallback(pageIdentifier(), frameID());

        completionHandler(action, policies, processSwapRequestedByClient, isNavigatingToAppBoundDomain, wasNavigationIntercepted);
        m_activeListener = nullptr;
    }, expectSafeBrowsingResult, expectAppBoundDomainResult, shouldWaitForInitialLinkDecorationFilteringData, shouldWaitForSiteHasStorageCheck);
    return *m_activeListener;
}

void WebFrameProxy::getWebArchive(CompletionHandler<void(API::Data*)>&& callback)
{
#if PLATFORM(COCOA)
    if (RefPtr page = m_page.get()) {
        page->getWebArchiveDataWithFrame(*this, WTF::move(callback));
        return;
    }
#endif
    callback(nullptr);
}

void WebFrameProxy::getMainResourceData(CompletionHandler<void(API::Data*)>&& callback)
{
    if (RefPtr page = m_page.get())
        page->getMainResourceDataOfFrame(this, WTF::move(callback));
    else
        callback(nullptr);
}

void WebFrameProxy::getResourceData(API::URL* resourceURL, CompletionHandler<void(API::Data*)>&& callback)
{
    if (RefPtr page = m_page.get())
        page->getResourceDataFromFrame(*this, resourceURL, WTF::move(callback));
    else
        callback(nullptr);
}

void WebFrameProxy::setUnreachableURL(const URL& unreachableURL)
{
    m_frameLoadState.setUnreachableURL(unreachableURL);
}

void WebFrameProxy::transferNavigationCallbackToFrame(WebFrameProxy& frame)
{
    frame.setNavigationCallback(WTF::move(m_navigateCallback));
}

void WebFrameProxy::setNavigationCallback(CompletionHandler<void(std::optional<WebCore::PageIdentifier>, std::optional<WebCore::FrameIdentifier>)>&& navigateCallback)
{
    ASSERT(!m_navigateCallback);
    m_navigateCallback = WTF::move(navigateCallback);
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

#if HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)
    m_contentFilterUnblockHandler.setConfigurationPath(protect(page->websiteDataStore())->configuration().webContentRestrictionsConfigurationFile());
#endif

#if HAVE(WEBCONTENTRESTRICTIONS)
    if (m_contentFilterUnblockHandler.needsNetworkProcess()) {
        if (auto evaluatedURL = m_contentFilterUnblockHandler.evaluatedURL()) {
            WebCore::ParentalControlsURLFilterParameters parameters {
                *evaluatedURL,
#if HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)
                m_contentFilterUnblockHandler.configurationPath()
#endif
            };
            protect(protect(page->websiteDataStore())->networkProcess())->allowEvaluatedURL(parameters, [page](bool unblocked) {
                if (unblocked)
                    page->reload({ });
            });
            return true;
        }
    }
#endif

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
    if (RefPtr parentFrame = m_parentFrame.get()) {
#if ENABLE(WEBDRIVER_BIDI)
        if (RefPtr page = m_page.get()) {
            if (RefPtr session = page->activeAutomationSession())
                session->willDestroyFrame(*this);
        }
#endif
        parentFrame->m_childFrames.remove(*this);
    }
    m_parentFrame = nullptr;
}

bool WebFrameProxy::isConnected() const
{
    if (RefPtr parentFrame = m_parentFrame.get())
        return parentFrame->m_childFrames.contains(*this);
    return false;
}

void WebFrameProxy::didCreateSubframe(WebCore::FrameIdentifier frameID, String&& frameName, SandboxFlags effectiveSandboxFlags, ReferrerPolicy effectiveReferrerPolicy, WebCore::ScrollbarMode scrollingMode)
{
    // The DecidePolicyForNavigationActionSync IPC is synchronous and may therefore get processed before the DidCreateSubframe one.
    // When this happens, decidePolicyForNavigationActionSync() calls didCreateSubframe() and we need to ignore the DidCreateSubframe
    // IPC when it later gets processed.
    if (WebFrameProxy::webFrame(frameID))
        return;

    RefPtr page = m_page.get();
    MESSAGE_CHECK(page);
    MESSAGE_CHECK(WebFrameProxy::canCreateFrame(frameID));

    // This can happen with site isolation right after a frame does a cross-site navigation
    // if the old process creates a subframe before it is told the frame has become a RemoteFrame.
    if ((frameID.toUInt64() >> 32) != process().coreProcessIdentifier().toUInt64())
        return;

    Ref child = WebFrameProxy::create(*page, m_frameProcess, frameID, effectiveSandboxFlags, effectiveReferrerPolicy, scrollingMode, nullptr, IsMainFrame::No);
    child->m_parentFrame = *this;
    child->m_frameName = WTF::move(frameName);
    page->observeAndCreateRemoteSubframesInOtherProcesses(child, child->m_frameName);
    m_childFrames.add(child.copyRef());

#if ENABLE(WEBDRIVER_BIDI)
    if (RefPtr session = page->activeAutomationSession())
        session->didCreateFrame(child);
#endif
}

void WebFrameProxy::prepareForProvisionalLoadInProcess(WebProcessProxy& process, API::Navigation& navigation, BrowsingContextGroup& group, std::optional<SecurityOriginData> effectiveOrigin, CompletionHandler<void(WebCore::PageIdentifier)>&& completionHandler)
{
    if (isMainFrame())
        return completionHandler(*webPageIDInCurrentProcess());

    Site site = effectiveOrigin ? Site { *effectiveOrigin } : Site { navigation.currentRequest().url() };
    RefPtr page = m_page.get();
    // FIXME: Main resource (of main or subframe) request redirects should go straight from the network to UI process so we don't need to make the processes for each domain in a redirect chain. <rdar://116202119>
    Site mainFrameSite(page->mainFrame()->url());
    auto mainFrameDomain = mainFrameSite.domain();

    // If we have an effectiveOrigin, it means we are loading about:blank which doesn't have any resources
    // to load can commit it's provisional frame immediately
    CommitTiming commitTiming = effectiveOrigin ? CommitTiming::Immediately : CommitTiming::WaitForLoad;

    m_provisionalFrame = nullptr;
    m_provisionalFrame = adoptRef(*new ProvisionalFrameProxy(*this, group.ensureProcessForSite(site, mainFrameSite, process, protect(page->preferences())), commitTiming));
    protect(protect(page->websiteDataStore())->networkProcess())->addAllowedFirstPartyForCookies(process, mainFrameDomain, LoadedWebArchive::No, [pageID = page->webPageIDInProcess(process), completionHandler = WTF::move(completionHandler)] mutable {
        completionHandler(pageID);
    });
}

void WebFrameProxy::commitProvisionalFrame(IPC::Connection& connection, FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, std::optional<WebCore::NavigationIdentifier> navigationID, String&& mimeType, bool frameHasCustomContentProvider, FrameLoadType frameLoadType, const CertificateInfo& certificateInfo, bool usedLegacyTLS, bool privateRelayed, String&& proxyName, WebCore::ResourceResponseSource source, bool containsPluginDocument, HasInsecureContent hasInsecureContent, MouseEventPolicy mouseEventPolicy, DocumentSecurityPolicy&& documentSecurityPolicy, const UserData& userData)
{
    ASSERT(m_page);
    if (m_provisionalFrame) {
        protect(process())->send(Messages::WebPage::LoadDidCommitInAnotherProcess(frameID, m_layerHostingContextIdentifier), *webPageIDInCurrentProcess());

        if (RefPtr process = std::exchange(m_provisionalFrame, nullptr)->takeFrameProcess())
            setProcess(process.releaseNonNull());
    }

    protect(page())->didCommitLoadForFrame(connection, frameID, WTF::move(frameInfo), WTF::move(request), navigationID, WTF::move(mimeType), frameHasCustomContentProvider, frameLoadType, certificateInfo, usedLegacyTLS, privateRelayed, WTF::move(proxyName), source, containsPluginDocument, hasInsecureContent, mouseEventPolicy, WTF::move(documentSecurityPolicy), userData);
}

void WebFrameProxy::getFrameInfo(CompletionHandler<void(std::optional<FrameInfoData>&&)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebFrame::GetFrameInfo(), WTF::move(completionHandler));
}

void WebFrameProxy::getFrameTree(CompletionHandler<void(std::optional<FrameTreeNodeData>&&)>&& completionHandler)
{
    class FrameInfoCallbackAggregator : public RefCounted<FrameInfoCallbackAggregator> {
    public:
        static Ref<FrameInfoCallbackAggregator> create(CompletionHandler<void(std::optional<FrameTreeNodeData>&&)>&& completionHandler, size_t childCount) { return adoptRef(*new FrameInfoCallbackAggregator(WTF::move(completionHandler), childCount)); }
        void setCurrentFrameData(FrameInfoData&& data) { m_currentFrameData = WTF::move(data); }
        void addChildFrameData(size_t index, FrameTreeNodeData&& data) { m_childFrameData[index] = WTF::move(data); }
        ~FrameInfoCallbackAggregator()
        {
            // FIXME: We currently have to drop child frames that are currently not subframes of this frame
            // (e.g. they are in the back/forward cache). They really should not be part of m_childFrames.
            auto nonEmptyChildFrameData = WTF::compactMap(WTF::move(m_childFrameData), [](std::optional<FrameTreeNodeData>&& data) {
                return std::forward<decltype(data)>(data);
            });
            m_completionHandler(m_currentFrameData ? std::optional(FrameTreeNodeData {
                WTF::move(*m_currentFrameData),
                WTF::move(nonEmptyChildFrameData)
            }) : std::nullopt);
        }

    private:
        FrameInfoCallbackAggregator(CompletionHandler<void(std::optional<FrameTreeNodeData>&&)>&& completionHandler, size_t childCount)
            : m_completionHandler(WTF::move(completionHandler))
            , m_childFrameData(childCount, { }) { }

        CompletionHandler<void(std::optional<FrameTreeNodeData>&&)> m_completionHandler;
        std::optional<FrameInfoData> m_currentFrameData;
        Vector<std::optional<FrameTreeNodeData>> m_childFrameData;
    };

    Ref aggregator = FrameInfoCallbackAggregator::create(WTF::move(completionHandler), m_childFrames.size());
    getFrameInfo([aggregator] (std::optional<FrameInfoData>&& info) {
        if (info)
            aggregator->setCurrentFrameData(WTF::move(*info));
    });

    RefPtr page = this->page();
    bool isSiteIsolationEnabled = page && protect(page->preferences())->siteIsolationEnabled();
    size_t index = 0;
    for (Ref childFrame : m_childFrames) {
        childFrame->getFrameTree([aggregator, index = index++, frameID = this->frameID(), isSiteIsolationEnabled] (std::optional<FrameTreeNodeData>&& data) {
            if (!data)
                return;

            // FIXME: m_childFrames currently contains iframes that are in the back/forward cache, not currently
            // connected to this parent frame. They should really not be part of m_childFrames anymore.
            // FIXME: With site isolation enabled, remote frames currently don't have a parentFrameID so we temporarily
            // ignore this check.
            if (data->info.parentFrameID != frameID && !isSiteIsolationEnabled)
                return;

            aggregator->addChildFrameData(index, WTF::move(*data));
        });
    }
}

FrameTreeCreationParameters WebFrameProxy::frameTreeCreationParameters() const
{
    return {
        m_frameID,
        m_opener ? std::optional(m_opener->frameID()) : std::nullopt,
        m_frameName,
        calculateFrameTreeSyncData(),
        WTF::map(m_childFrames, [] (auto& frame) {
            return frame->frameTreeCreationParameters();
        })
    };
}

void WebFrameProxy::setProcess(FrameProcess& process)
{
    ASSERT(m_frameProcess.ptr() != &process);

    protect(m_frameProcess)->decrementFrameCount();
    m_frameProcess = process;
    protect(m_frameProcess)->incrementFrameCount();
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

void WebFrameProxy::remoteProcessDidTerminate(WebProcessProxy& process, ClearFrameTreeSyncData clearFrameTreeSyncData)
{
    // Only clear the FrameTreeSyncData on all child processes once, when handling the main frame.
    // No point in clearing it multiple times in a tight loop.
    if (clearFrameTreeSyncData == ClearFrameTreeSyncData::Yes)
        broadcastFrameTreeSyncData(FrameTreeSyncData::create());

    for (Ref child : m_childFrames)
        child->remoteProcessDidTerminate(process, ClearFrameTreeSyncData::No);
    if (process.coreProcessIdentifier() != this->process().coreProcessIdentifier())
        return;
    if (m_frameLoadState.state() == FrameLoadState::State::Finished)
        return;

    notifyParentOfLoadCompletion(protect(this->process()));
}

Ref<FrameTreeSyncData> WebFrameProxy::calculateFrameTreeSyncData() const
{
#if ENABLE(APPLE_PAY)
    std::optional<const CertificateInfo> certificateInfo = m_certificateInfo.isEmpty() ? std::nullopt : std::optional<const CertificateInfo>(m_certificateInfo);
    bool isSecureForPaymentSession = PaymentSession::isSecureForSession(url(), WTF::move(certificateInfo));
#else
    bool isSecureForPaymentSession = false;
#endif

    return FrameTreeSyncData::create(isSecureForPaymentSession, securityOrigin(), m_documentSecurityPolicy, url().protocol().toString(), IntRect { }, LayoutRect { }, HashMap<FrameIdentifier, RemoteFrameLayoutInfo> { });
}

Ref<SecurityOrigin> WebFrameProxy::securityOrigin() const
{
    return SecurityOrigin::create(url());
}

bool WebFrameProxy::isSameOriginAs(const WebFrameProxy& frame) const
{
    return &frame == this || securityOrigin()->isSameOriginAs(frame.securityOrigin());
}

void WebFrameProxy::broadcastFrameTreeSyncData(Ref<FrameTreeSyncData>&& data)
{
    RefPtr webPage = m_page.get();
    if (!webPage)
        return;

    RELEASE_ASSERT(protect(webPage->preferences())->siteIsolationEnabled());

    webPage->forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.send(Messages::WebPage::AllFrameTreeSyncDataChangedInAnotherProcess(m_frameID, data), pageID);
    });
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
    if (RefPtr page = m_page.get())
        return page->webPageIDInProcess(protect(process()));
    return std::nullopt;
}

auto WebFrameProxy::traverseNext() const -> TraversalResult
{
    if (RefPtr child = firstChild())
        return { WTF::move(child), DidWrap::No };

    RefPtr sibling = nextSibling();
    if (sibling)
        return { WTF::move(sibling), DidWrap::No };

    RefPtr frame = this;
    while (!sibling) {
        frame = frame->parentFrame();
        if (!frame)
            return { };
        sibling = frame->nextSibling();
    }

    if (frame)
        return { WTF::move(sibling), DidWrap::No };

    return { };
}

auto WebFrameProxy::traverseNext(CanWrap canWrap) const -> TraversalResult
{
    if (RefPtr frame = traverseNext().frame)
        return { WTF::move(frame), DidWrap::No };

    if (canWrap == CanWrap::Yes) {
        if (RefPtr page = m_page.get())
            return { protect(page->mainFrame()), DidWrap::Yes };

    }
    return { };
}

auto WebFrameProxy::traversePrevious(CanWrap canWrap) -> TraversalResult
{
    if (RefPtr previousSibling = this->previousSibling())
        return { previousSibling->deepLastChild(), DidWrap::No };
    if (RefPtr parent = parentFrame())
        return { WTF::move(parent), DidWrap::No };

    if (canWrap == CanWrap::Yes)
        return { deepLastChild(), DidWrap::Yes };
    return { };
}

RefPtr<WebFrameProxy> WebFrameProxy::deepLastChild()
{
    RefPtr result = this;
    for (RefPtr last = lastChild(); last; last = last->lastChild())
        result = last;
    return result;
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

RefPtr<WebFrameProxy> WebFrameProxy::childFrame(uint64_t index) const
{
    RefPtr child = firstChild();
    for (uint64_t i = 0; i < index && child; i++)
        child = child->nextSibling();
    return child;
}

void WebFrameProxy::updateOpener(std::optional<WebCore::FrameIdentifier> newOpener)
{
    RefPtr previousOpener = m_opener.get();
    m_opener = WebFrameProxy::webFrame(newOpener);

    RefPtr webPage = page();
    if (!m_opener && webPage && !protect(webPage->preferences())->siteIsolationEnabled())
        m_disownedOpener = previousOpener.get();
}

Ref<WebFrameProxy> WebFrameProxy::rootFrame()
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

void WebFrameProxy::updateScrollingMode(WebCore::ScrollbarMode scrollingMode)
{
    m_scrollingMode = scrollingMode;
    if (RefPtr page = m_page.get())
        page->sendToProcessContainingFrame(m_frameID, Messages::WebPage::UpdateFrameScrollingMode(m_frameID, scrollingMode));
}

void WebFrameProxy::setAppBadge(const WebCore::SecurityOriginData& origin, std::optional<uint64_t> badge)
{
    if (RefPtr webPageProxy = m_page.get())
        webPageProxy->uiClient().updateAppBadge(*webPageProxy, origin, badge);
}

void WebFrameProxy::findFocusableElementDescendingIntoRemoteFrame(WebCore::FocusDirection direction, const WebCore::FocusEventData& focusEventData, WebCore::ShouldFocusElement shouldFocusElement, CompletionHandler<void(WebCore::FoundElementInRemoteFrame)>&& completionHandler)
{
    RefPtr page = m_page.get();
    if (!page) {
        completionHandler(WebCore::FoundElementInRemoteFrame::No);
        return;
    }

    sendWithAsyncReply(Messages::WebFrame::FindFocusableElementDescendingIntoRemoteFrame(direction, focusEventData, shouldFocusElement), WTF::move(completionHandler));
}

std::optional<SharedPreferencesForWebProcess> WebFrameProxy::sharedPreferencesForWebProcess() const
{
    return process().sharedPreferencesForWebProcess();
}

void WebFrameProxy::takeSnapshotOfNode(JSHandleIdentifier identifier, CompletionHandler<void(std::optional<ShareableBitmapHandle>&&)>&& completion)
{
    if (!m_page)
        return completion({ });

    sendWithAsyncReply(Messages::WebFrame::TakeSnapshotOfNode(identifier), WTF::move(completion));
}

void WebFrameProxy::sendMessageToInspectorFrontend(const String& targetId, const String& message)
{
    if (RefPtr page = m_page.get())
        page->inspectorController().sendMessageToInspectorFrontend(targetId, message);
}

void WebFrameProxy::requestTextExtraction(WebCore::TextExtraction::Request&& request, CompletionHandler<void(WebCore::TextExtraction::Result&&)>&& completion)
{
    if (RefPtr page = m_page.get(); !page || !page->hasRunningProcess())
        return completion({ });

    sendWithAsyncReply(Messages::WebFrame::RequestTextExtraction(WTF::move(request)), WTF::move(completion));
}

void WebFrameProxy::handleTextExtractionInteraction(TextExtraction::Interaction&& interaction, CompletionHandler<void(bool, String&&)>&& completion)
{
    if (RefPtr page = m_page.get(); !page || !page->hasRunningProcess()) {
        ASSERT_NOT_REACHED();
        return completion(false, "Internal inconsistency / unexpected state. Please file a bug"_s);
    }

    sendWithAsyncReply(Messages::WebFrame::HandleTextExtractionInteraction(WTF::move(interaction)), WTF::move(completion));
}

void WebFrameProxy::takeSnapshotOfExtractedText(TextExtraction::ExtractedText&& extractedText, CompletionHandler<void(RefPtr<TextIndicator>&&)>&& completion)
{
    if (RefPtr page = m_page.get(); !page || !page->hasRunningProcess())
        return completion({ });

    sendWithAsyncReply(Messages::WebFrame::TakeSnapshotOfExtractedText(WTF::move(extractedText)), WTF::move(completion));
}

void WebFrameProxy::describeTextExtractionInteraction(TextExtraction::Interaction&& interaction, CompletionHandler<void(TextExtraction::InteractionDescription&&)>&& completion)
{
    if (RefPtr page = m_page.get(); !page || !page->hasRunningProcess()) {
        ASSERT_NOT_REACHED();
        return completion({ "Internal inconsistency / unexpected state. Please file a bug"_s, { } });
    }

    sendWithAsyncReply(Messages::WebFrame::DescribeTextExtractionInteraction(WTF::move(interaction)), WTF::move(completion));
}

void WebFrameProxy::requestJSHandleForExtractedText(TextExtraction::ExtractedText&& extractedText, CompletionHandler<void(std::optional<JSHandleInfo>&&)>&& completion)
{
    if (RefPtr page = m_page.get(); !page || !page->hasRunningProcess())
        return completion({ });

    sendWithAsyncReply(Messages::WebFrame::RequestJSHandleForExtractedText(WTF::move(extractedText)), WTF::move(completion));
}

void WebFrameProxy::getSelectorPathsForNode(JSHandleInfo&& handle, CompletionHandler<void(Vector<HashSet<String>>&&)>&& completion)
{
    if (RefPtr page = m_page.get(); !page || !page->hasRunningProcess())
        return completion({ });

    sendWithAsyncReply(Messages::WebFrame::GetSelectorPathsForNode(WTF::move(handle)), WTF::move(completion));
}

void WebFrameProxy::getNodeForSelectorPaths(Vector<HashSet<String>>&& selectors, CompletionHandler<void(std::optional<JSHandleInfo>&&)>&& completion)
{
    if (RefPtr page = m_page.get(); !page || !page->hasRunningProcess())
        return completion({ });

    sendWithAsyncReply(Messages::WebFrame::GetNodeForSelectorPaths(WTF::move(selectors)), WTF::move(completion));
}

ProvisionalFrameCreationParameters WebFrameProxy::provisionalFrameCreationParameters(std::optional<WebCore::FrameIdentifier> frameIDBeforeProvisionalNavigation, std::optional<LayerHostingContextIdentifier> layerHostingContextIdentifier, CommitTiming commitTiming)
{
    return ProvisionalFrameCreationParameters {
        frameID(),
        frameIDBeforeProvisionalNavigation,
        layerHostingContextIdentifier,
        effectiveSandboxFlags(),
        effectiveReferrerPolicy(),
        scrollingMode(),
        remoteFrameRect(),
        commitTiming,
    };
}

} // namespace WebKit

#undef MESSAGE_CHECK

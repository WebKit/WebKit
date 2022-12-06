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
#include "ProvisionalFrameProxy.h"
#include "ProvisionalPageProxy.h"
#include "WebFrameMessages.h"
#include "WebFramePolicyListenerProxy.h"
#include "WebFrameProxyMessages.h"
#include "WebPageMessages.h"
#include "WebPageProxyMessages.h"
#include "WebPasteboardProxy.h"
#include "WebProcessPool.h"
#include "WebsiteDataStore.h"
#include "WebsitePoliciesData.h"
#include <WebCore/Image.h>
#include <WebCore/MIMETypeRegistry.h>
#include <stdio.h>
#include <wtf/RunLoop.h>
#include <wtf/text/WTFString.h>

#define MESSAGE_CHECK(process, assertion) MESSAGE_CHECK_BASE(assertion, process->connection())

namespace WebKit {
using namespace WebCore;

class WebPageProxy;

static HashMap<FrameIdentifier, WebFrameProxy*>& allFrames()
{
    ASSERT(RunLoop::isMain());
    static NeverDestroyed<HashMap<FrameIdentifier, WebFrameProxy*>> map;
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
    , m_webPageID(page.webPageID())
    , m_frameID(frameID)
{
    ASSERT(!allFrames().contains(frameID));
    allFrames().set(frameID, this);
    m_process->addMessageReceiver(Messages::WebFrameProxy::messageReceiverName(), m_frameID.object(), *this);
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

    m_process->removeMessageReceiver(Messages::WebFrameProxy::messageReceiverName(), m_frameID.object());

    ASSERT(allFrames().get(m_frameID) == this);
    allFrames().remove(m_frameID);

    if (m_parentFrameProcess) {
        m_process->removeMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_webPageID);
        m_process->removeFrameWithRemoteFrameProcess(*this);
    }
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

ProcessID WebFrameProxy::processIdentifier() const
{
    return m_process->processIdentifier();
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

    m_page->sendWithAsyncReply(Messages::WebPage::NavigateServiceWorkerClient { documentIdentifier, url }, [this, protectedThis = Ref { *this }, url, callback = WTFMove(callback)](bool result) mutable {
        if (!result) {
            callback({ }, { });
            return;
        }

        if (!m_activeListener) {
            callback(pageIdentifier(), frameID());
            return;
        }

        if (m_navigateCallback)
            m_navigateCallback({ }, { });

        m_navigateCallback = WTFMove(callback);
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

    if (m_parentFrameProcess)
        m_parentFrameProcess->send(Messages::WebFrame::DidFinishLoadInAnotherProcess(), m_frameID.object());
}

void WebFrameProxy::didFailLoad()
{
    m_frameLoadState.didFailLoad();

    if (m_navigateCallback)
        m_navigateCallback({ }, { });

    // FIXME: Should we send DidFinishLoadInAnotherProcess here too?
}

void WebFrameProxy::didSameDocumentNavigation(const URL& url)
{
    m_frameLoadState.didSameDocumentNotification(url);
}

void WebFrameProxy::didChangeTitle(const String& title)
{
    m_title = title;
}

WebFramePolicyListenerProxy& WebFrameProxy::setUpPolicyListenerProxy(CompletionHandler<void(PolicyAction, API::WebsitePolicies*, ProcessSwapRequestedByClient, RefPtr<SafeBrowsingWarning>&&, std::optional<NavigatingToAppBoundDomain>)>&& completionHandler, ShouldExpectSafeBrowsingResult expectSafeBrowsingResult, ShouldExpectAppBoundDomainResult expectAppBoundDomainResult)
{
    if (m_activeListener)
        m_activeListener->ignore();
    m_activeListener = WebFramePolicyListenerProxy::create([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] (PolicyAction action, API::WebsitePolicies* policies, ProcessSwapRequestedByClient processSwapRequestedByClient, RefPtr<SafeBrowsingWarning>&& safeBrowsingWarning, std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain) mutable {
        if (action != PolicyAction::Use && m_navigateCallback)
            m_navigateCallback(pageIdentifier(), frameID());

        completionHandler(action, policies, processSwapRequestedByClient, WTFMove(safeBrowsingWarning), isNavigatingToAppBoundDomain);
        m_activeListener = nullptr;
    }, expectSafeBrowsingResult, expectAppBoundDomainResult);
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

void WebFrameProxy::didCreateSubframe(WebCore::FrameIdentifier frameID)
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
    m_childFrames.add(WTFMove(child));
}

void WebFrameProxy::swapToProcess(Ref<WebProcessProxy>&& process, const WebCore::ResourceRequest& request)
{
    ASSERT(!isMainFrame());
    m_provisionalFrame = makeUnique<ProvisionalFrameProxy>(*this, WTFMove(process), request);
}

IPC::Connection* WebFrameProxy::messageSenderConnection() const
{
    return m_process->connection();
}

uint64_t WebFrameProxy::messageSenderDestinationID() const
{
    return m_frameID.object().toUInt64();
}

void WebFrameProxy::commitProvisionalFrame(FrameIdentifier frameID, FrameInfoData&& frameInfo, ResourceRequest&& request, uint64_t navigationID, const String& mimeType, bool frameHasCustomContentProvider, WebCore::FrameLoadType frameLoadType, const WebCore::CertificateInfo& certificateInfo, bool usedLegacyTLS, bool privateRelayed, bool containsPluginDocument, std::optional<WebCore::HasInsecureContent> forcedHasInsecureContent, WebCore::MouseEventPolicy mouseEventPolicy, const UserData& userData)
{
    // FIXME: Not only is this a race condition, but we still want to receive messages,
    // such as if the parent frame navigates the remote frame.
    m_provisionalFrame->process().provisionalFrameCommitted(*this);
    send(Messages::WebFrame::DidCommitLoadInAnotherProcess());
    m_process->removeMessageReceiver(Messages::WebFrameProxy::messageReceiverName(), m_frameID.object());
    m_parentFrameProcess = std::exchange(m_process, m_provisionalFrame->process());
    m_provisionalFrame = nullptr;
    m_process->addMessageReceiver(Messages::WebPageProxy::messageReceiverName(), m_webPageID, *page()); // FIXME: We might want another message relayer so we can tell if messages came from the remote frame process or m_process.
    m_process->addMessageReceiver(Messages::WebFrameProxy::messageReceiverName(), m_frameID.object(), *this);

    if (m_page)
        m_page->didCommitLoadForFrame(frameID, WTFMove(frameInfo), WTFMove(request), navigationID, mimeType, frameHasCustomContentProvider, frameLoadType, certificateInfo, usedLegacyTLS, privateRelayed, containsPluginDocument, forcedHasInsecureContent, mouseEventPolicy, userData);
}

} // namespace WebKit

#undef MESSAGE_CHECK

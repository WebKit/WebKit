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
#include "ProvisionalPageProxy.h"
#include "WebFramePolicyListenerProxy.h"
#include "WebPageMessages.h"
#include "WebPasteboardProxy.h"
#include "WebProcessPool.h"
#include "WebsiteDataStore.h"
#include "WebsitePoliciesData.h"
#include <WebCore/Image.h>
#include <WebCore/MIMETypeRegistry.h>
#include <stdio.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
using namespace WebCore;

class WebPageProxy;

WebFrameProxy::WebFrameProxy(WebPageProxy& page, FrameIdentifier frameID)
    : m_page(page)
    , m_frameID(frameID)
{
    WebProcessPool::statistics().wkFrameCount++;
}

WebFrameProxy::~WebFrameProxy()
{
    WebProcessPool::statistics().wkFrameCount--;
#if PLATFORM(GTK)
    WebPasteboardProxy::singleton().didDestroyFrame(this);
#endif

    if (m_navigateCallback)
        m_navigateCallback({ });
}

void WebFrameProxy::webProcessWillShutDown()
{
    m_page = nullptr;

    if (m_activeListener) {
        m_activeListener->ignore();
        m_activeListener = nullptr;
    }

    if (m_navigateCallback)
        m_navigateCallback({ });
}

bool WebFrameProxy::isMainFrame() const
{
    if (!m_page)
        return false;

    return this == m_page->mainFrame() || (m_page->provisionalPageProxy() && this == m_page->provisionalPageProxy()->mainFrame());
}

std::optional<PageIdentifier> WebFrameProxy::pageIdentifier() const
{
    if (!m_page)
        return { };
    return m_page->webPageID();
}

void WebFrameProxy::navigateServiceWorkerClient(WebCore::ScriptExecutionContextIdentifier documentIdentifier, const URL& url, CompletionHandler<void(std::optional<PageIdentifier>)>&& callback)
{
    if (!m_page) {
        callback({ });
        return;
    }

    m_page->sendWithAsyncReply(Messages::WebPage::NavigateServiceWorkerClient { documentIdentifier, url }, [this, protectedThis = Ref { *this }, url, callback = WTFMove(callback)](bool result) mutable {
        if (!result) {
            callback({ });
            return;
        }

        if (!m_activeListener) {
            callback(pageIdentifier());
            return;
        }

        if (m_navigateCallback)
            m_navigateCallback({ });

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
        m_navigateCallback({ });
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
        m_navigateCallback(pageIdentifier());
}

void WebFrameProxy::didFailLoad()
{
    m_frameLoadState.didFailLoad();

    if (m_navigateCallback)
        m_navigateCallback({ });
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
            m_navigateCallback(pageIdentifier());

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

void WebFrameProxy::setNavigationCallback(CompletionHandler<void(std::optional<WebCore::PageIdentifier>)>&& navigateCallback)
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

} // namespace WebKit

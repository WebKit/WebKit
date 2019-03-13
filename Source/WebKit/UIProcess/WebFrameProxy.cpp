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
#include "WebCertificateInfo.h"
#include "WebFramePolicyListenerProxy.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
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

WebFrameProxy::WebFrameProxy(WebPageProxy& page, uint64_t frameID)
    : m_page(makeWeakPtr(page))
    , m_isFrameSet(false)
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
}

void WebFrameProxy::webProcessWillShutDown()
{
    m_page = nullptr;

    if (m_activeListener) {
        m_activeListener->ignore();
        m_activeListener = nullptr;
    }
}

bool WebFrameProxy::isMainFrame() const
{
    if (!m_page)
        return false;

    return this == m_page->mainFrame() || (m_page->provisionalPageProxy() && this == m_page->provisionalPageProxy()->mainFrame());
}

void WebFrameProxy::loadURL(const URL& url)
{
    if (!m_page)
        return;

    m_page->process().send(Messages::WebPage::LoadURLInFrame(url, m_frameID), m_page->pageID());
}

void WebFrameProxy::loadData(const IPC::DataReference& data, const String& MIMEType, const String& encodingName, const URL& baseURL)
{
    if (!m_page)
        return;

    m_page->process().send(Messages::WebPage::LoadDataInFrame(data, MIMEType, encodingName, baseURL, m_frameID), m_page->pageID());
}

void WebFrameProxy::stopLoading() const
{
    if (!m_page)
        return;

    if (!m_page->isValid())
        return;

    m_page->process().send(Messages::WebPage::StopLoadingFrame(m_frameID), m_page->pageID());
}
    
bool WebFrameProxy::canProvideSource() const
{
    return isDisplayingMarkupDocument();
}

bool WebFrameProxy::canShowMIMEType(const String& mimeType) const
{
    if (!m_page)
        return false;

    return m_page->canShowMIMEType(mimeType);
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
    return m_MIMEType == "text/html" || m_MIMEType == "image/svg+xml" || m_MIMEType == "application/x-webarchive" || MIMETypeRegistry::isXMLMIMEType(m_MIMEType);
}

bool WebFrameProxy::isDisplayingPDFDocument() const
{
    return MIMETypeRegistry::isPDFOrPostScriptMIMEType(m_MIMEType);
}

void WebFrameProxy::didStartProvisionalLoad(const URL& url)
{
    m_frameLoadState.didStartProvisionalLoad(url);
}

void WebFrameProxy::didReceiveServerRedirectForProvisionalLoad(const URL& url)
{
    m_frameLoadState.didReceiveServerRedirectForProvisionalLoad(url);
}

void WebFrameProxy::didFailProvisionalLoad()
{
    m_frameLoadState.didFailProvisionalLoad();
}

void WebFrameProxy::didCommitLoad(const String& contentType, WebCertificateInfo& certificateInfo, bool containsPluginDocument)
{
    m_frameLoadState.didCommitLoad();

    m_title = String();
    m_MIMEType = contentType;
    m_isFrameSet = false;
    m_certificateInfo = &certificateInfo;
    m_containsPluginDocument = containsPluginDocument;
}

void WebFrameProxy::didFinishLoad()
{
    m_frameLoadState.didFinishLoad();
}

void WebFrameProxy::didFailLoad()
{
    m_frameLoadState.didFailLoad();
}

void WebFrameProxy::didSameDocumentNavigation(const URL& url)
{
    m_frameLoadState.didSameDocumentNotification(url);
}

void WebFrameProxy::didChangeTitle(const String& title)
{
    m_title = title;
}

WebFramePolicyListenerProxy& WebFrameProxy::setUpPolicyListenerProxy(CompletionHandler<void(PolicyAction, API::WebsitePolicies*, ProcessSwapRequestedByClient, RefPtr<SafeBrowsingWarning>&&)>&& completionHandler, ShouldExpectSafeBrowsingResult expect)
{
    if (m_activeListener)
        m_activeListener->ignore();
    m_activeListener = WebFramePolicyListenerProxy::create([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)] (PolicyAction action, API::WebsitePolicies* policies, ProcessSwapRequestedByClient processSwapRequestedByClient, RefPtr<SafeBrowsingWarning>&& safeBrowsingWarning) mutable {
        completionHandler(action, policies, processSwapRequestedByClient, WTFMove(safeBrowsingWarning));
        m_activeListener = nullptr;
    }, expect);
    return *m_activeListener;
}

void WebFrameProxy::getWebArchive(Function<void (API::Data*, CallbackBase::Error)>&& callbackFunction)
{
    if (!m_page) {
        callbackFunction(nullptr, CallbackBase::Error::Unknown);
        return;
    }

    m_page->getWebArchiveOfFrame(this, WTFMove(callbackFunction));
}

void WebFrameProxy::getMainResourceData(Function<void (API::Data*, CallbackBase::Error)>&& callbackFunction)
{
    if (!m_page) {
        callbackFunction(nullptr, CallbackBase::Error::Unknown);
        return;
    }

    m_page->getMainResourceDataOfFrame(this, WTFMove(callbackFunction));
}

void WebFrameProxy::getResourceData(API::URL* resourceURL, Function<void (API::Data*, CallbackBase::Error)>&& callbackFunction)
{
    if (!m_page) {
        callbackFunction(nullptr, CallbackBase::Error::Unknown);
        return;
    }

    m_page->getResourceDataFromFrame(this, resourceURL, WTFMove(callbackFunction));
}

void WebFrameProxy::setUnreachableURL(const URL& unreachableURL)
{
    m_frameLoadState.setUnreachableURL(unreachableURL);
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

    m_page->process().send(Messages::WebPage::CollapseSelectionInFrame(m_frameID), m_page->pageID());
}
#endif

} // namespace WebKit

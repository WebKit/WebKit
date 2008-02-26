/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebFrameLoaderClient.h"

#include "MarshallingHelpers.h"
#include "WebDocumentLoader.h"
#include "WebFrame.h"
#include "WebNotificationCenter.h"
#include "WebView.h"
#pragma warning(push, 0)
#include <WebCore/DocumentLoader.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameTree.h>
#include <WebCore/FrameView.h>
#include <WebCore/HTMLFrameElement.h>
#include <WebCore/HTMLFrameOwnerElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HistoryItem.h>
#pragma warning(pop)

using namespace WebCore;
using namespace HTMLNames;

WebFrameLoaderClient::WebFrameLoaderClient(WebFrame* webFrame)
    : m_webFrame(webFrame)
{
    ASSERT_ARG(webFrame, webFrame);
}

WebFrameLoaderClient::~WebFrameLoaderClient()
{
}

bool WebFrameLoaderClient::hasWebView() const
{
    return m_webFrame->webView();
}

bool WebFrameLoaderClient::hasFrameView() const
{
    return core(m_webFrame)->view();
}

void WebFrameLoaderClient::forceLayout()
{
    core(m_webFrame)->forceLayout(true);
}

void WebFrameLoaderClient::dispatchDidHandleOnloadEvents()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegatePrivate> frameLoadDelegatePriv;
    if (SUCCEEDED(webView->frameLoadDelegatePrivate(&frameLoadDelegatePriv)) && frameLoadDelegatePriv)
        frameLoadDelegatePriv->didHandleOnloadEventsForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didReceiveServerRedirectForProvisionalLoadForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchDidCancelClientRedirect()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didCancelClientRedirectForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchWillPerformClientRedirect(const KURL& url, double delay, double fireDate)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->willPerformClientRedirectToURL(webView, BString(url.string()), delay, MarshallingHelpers::CFAbsoluteTimeToDATE(fireDate), m_webFrame);
}

void WebFrameLoaderClient::dispatchDidChangeLocationWithinPage()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didChangeLocationWithinPageForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchWillClose()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->willCloseFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchDidReceiveIcon()
{
    m_webFrame->webView()->dispatchDidReceiveIconFromWebFrame(m_webFrame);
}

void WebFrameLoaderClient::dispatchDidStartProvisionalLoad()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didStartProvisionalLoadForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchDidReceiveTitle(const String& title)
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didReceiveTitle(webView, BString(title), m_webFrame);
}

void WebFrameLoaderClient::dispatchDidCommitLoad()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didCommitLoadForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchDidFinishDocumentLoad()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegatePrivate> frameLoadDelegatePriv;
    if (SUCCEEDED(webView->frameLoadDelegatePrivate(&frameLoadDelegatePriv)) && frameLoadDelegatePriv)
        frameLoadDelegatePriv->didFinishDocumentLoadForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchDidFinishLoad()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegate> frameLoadDelegate;
    if (SUCCEEDED(webView->frameLoadDelegate(&frameLoadDelegate)))
        frameLoadDelegate->didFinishLoadForFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchDidFirstLayout()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebFrameLoadDelegatePrivate> frameLoadDelegatePriv;
    if (SUCCEEDED(webView->frameLoadDelegatePrivate(&frameLoadDelegatePriv)) && frameLoadDelegatePriv)
        frameLoadDelegatePriv->didFirstLayoutInFrame(webView, m_webFrame);
}

void WebFrameLoaderClient::dispatchShow()
{
    WebView* webView = m_webFrame->webView();
    COMPtr<IWebUIDelegate> ui;
    if (SUCCEEDED(webView->uiDelegate(&ui)))
        ui->webViewShow(webView);
}

void WebFrameLoaderClient::postProgressStartedNotification()
{
    static BSTR progressStartedName = SysAllocString(WebViewProgressStartedNotification);
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->postNotificationName(progressStartedName, static_cast<IWebView*>(m_webFrame->webView()), 0);
}

void WebFrameLoaderClient::postProgressEstimateChangedNotification()
{
    static BSTR progressEstimateChangedName = SysAllocString(WebViewProgressEstimateChangedNotification);
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->postNotificationName(progressEstimateChangedName, static_cast<IWebView*>(m_webFrame->webView()), 0);
}

void WebFrameLoaderClient::postProgressFinishedNotification()
{
    static BSTR progressFinishedName = SysAllocString(WebViewProgressFinishedNotification);
    IWebNotificationCenter* notifyCenter = WebNotificationCenter::defaultCenterInternal();
    notifyCenter->postNotificationName(progressFinishedName, static_cast<IWebView*>(m_webFrame->webView()), 0);
}

PassRefPtr<DocumentLoader> WebFrameLoaderClient::createDocumentLoader(const ResourceRequest& request, const SubstituteData& substituteData)
{
    RefPtr<WebDocumentLoader> loader = new WebDocumentLoader(request, substituteData);

    COMPtr<WebDataSource> dataSource(AdoptCOM, WebDataSource::createInstance(loader.get()));

    loader->setDataSource(dataSource.get());
    return loader.release();
}

PassRefPtr<Frame> WebFrameLoaderClient::createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement,
                            const String& referrer, bool /*allowsScrolling*/, int /*marginWidth*/, int /*marginHeight*/)
{
    RefPtr<Frame> result = createFrame(url, name, ownerElement, referrer);
    if (!result)
        return 0;

    // Propagate the marginwidth/height and scrolling modes to the view.
    if (ownerElement->hasTagName(frameTag) || ownerElement->hasTagName(iframeTag)) {
        HTMLFrameElement* frameElt = static_cast<HTMLFrameElement*>(ownerElement);
        if (frameElt->scrollingMode() == ScrollbarAlwaysOff)
            result->view()->setScrollbarsMode(ScrollbarAlwaysOff);
        int marginWidth = frameElt->getMarginWidth();
        int marginHeight = frameElt->getMarginHeight();
        if (marginWidth != -1)
            result->view()->setMarginWidth(marginWidth);
        if (marginHeight != -1)
            result->view()->setMarginHeight(marginHeight);
    }

    return result.release();
}

PassRefPtr<Frame> WebFrameLoaderClient::createFrame(const KURL& URL, const String& name, HTMLFrameOwnerElement* ownerElement, const String& referrer)
{
    Frame* coreFrame = core(m_webFrame);
    ASSERT(coreFrame);

    COMPtr<WebFrame> webFrame(AdoptCOM, WebFrame::createInstance());

    webFrame->initWithWebFrameView(0, m_webFrame->webView(), coreFrame->page(), ownerElement);

    RefPtr<Frame> childFrame(adoptRef(core(webFrame.get()))); // We have to adopt, because Frames start out with a refcount of 1.
    ASSERT(childFrame);

    coreFrame->tree()->appendChild(childFrame);
    childFrame->tree()->setName(name);
    childFrame->init();

    loadURLIntoChild(URL, referrer, webFrame.get());

    // The frame's onload handler may have removed it from the document.
    if (!childFrame->tree()->parent())
        return 0;

    return childFrame.release();
}

void WebFrameLoaderClient::loadURLIntoChild(const KURL& originalURL, const String& referrer, WebFrame* childFrame)
{
    ASSERT(childFrame);
    ASSERT(core(childFrame));

    Frame* coreFrame = core(m_webFrame);
    ASSERT(coreFrame);

    HistoryItem* parentItem = coreFrame->loader()->currentHistoryItem();
    FrameLoadType loadType = coreFrame->loader()->loadType();
    FrameLoadType childLoadType = FrameLoadTypeRedirectWithLockedHistory;

    KURL url = originalURL;

    // If we're moving in the backforward list, we might want to replace the content
    // of this child frame with whatever was there at that point.
    // Reload will maintain the frame contents, LoadSame will not.
    if (parentItem && parentItem->children().size() &&
        (isBackForwardLoadType(loadType)
         || loadType == FrameLoadTypeReload
         || loadType == FrameLoadTypeReloadAllowingStaleData))
    {
        if (HistoryItem* childItem = parentItem->childItemWithName(core(childFrame)->tree()->name())) {
            // Use the original URL to ensure we get all the side-effects, such as
            // onLoad handlers, of any redirects that happened. An example of where
            // this is needed is Radar 3213556.
            url = childItem->originalURL();
            // These behaviors implied by these loadTypes should apply to the child frames
            childLoadType = loadType;

            if (isBackForwardLoadType(loadType))
                // For back/forward, remember this item so we can traverse any child items as child frames load
                core(childFrame)->loader()->setProvisionalHistoryItem(childItem);
            else
                // For reload, just reinstall the current item, since a new child frame was created but we won't be creating a new BF item
                core(childFrame)->loader()->setCurrentHistoryItem(childItem);
        }
    }

    // FIXME: Handle loading WebArchives here

    core(childFrame)->loader()->load(url, referrer, childLoadType, String(), 0, 0);
}

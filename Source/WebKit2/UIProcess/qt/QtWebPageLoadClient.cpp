/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "QtWebPageLoadClient.h"

#include "QtWebError.h"
#include "qquickwebview_p_p.h"

using namespace WebCore;

namespace WebKit {

QtWebPageLoadClient::QtWebPageLoadClient(WKPageRef pageRef, QQuickWebView* webView)
    : m_webView(webView)
{
    WKPageLoaderClient loadClient;
    memset(&loadClient, 0, sizeof(WKPageLoaderClient));
    loadClient.version = kWKPageLoaderClientCurrentVersion;
    loadClient.clientInfo = this;
    loadClient.didStartProvisionalLoadForFrame = didStartProvisionalLoadForFrame;
    loadClient.didFailProvisionalLoadWithErrorForFrame = didFailProvisionalLoadWithErrorForFrame;
    loadClient.didCommitLoadForFrame = didCommitLoadForFrame;
    loadClient.didFinishLoadForFrame = didFinishLoadForFrame;
    loadClient.didFailLoadWithErrorForFrame = didFailLoadWithErrorForFrame;
    loadClient.didSameDocumentNavigationForFrame = didSameDocumentNavigationForFrame;
    loadClient.didReceiveTitleForFrame = didReceiveTitleForFrame;
    loadClient.didStartProgress = didStartProgress;
    loadClient.didChangeProgress = didChangeProgress;
    loadClient.didFinishProgress = didFinishProgress;
    loadClient.didChangeBackForwardList = didChangeBackForwardList;
    WKPageSetPageLoaderClient(pageRef, &loadClient);
}

void QtWebPageLoadClient::didStartProvisionalLoad(const QUrl& url)
{
    m_webView->d_func()->provisionalLoadDidStart(url);
}

void QtWebPageLoadClient::didCommitLoad()
{
    m_webView->d_func()->loadDidCommit();
}

void QtWebPageLoadClient::didSameDocumentNavigation()
{
    m_webView->d_func()->didSameDocumentNavigation();
}

void QtWebPageLoadClient::didReceiveTitle()
{
    m_webView->d_func()->titleDidChange();
}

void QtWebPageLoadClient::didChangeProgress(int loadProgress)
{
    m_webView->d_func()->loadProgressDidChange(loadProgress);
}

void QtWebPageLoadClient::didChangeBackForwardList()
{
    m_webView->d_func()->backForwardListDidChange();
}

void QtWebPageLoadClient::dispatchLoadSucceeded()
{
    m_webView->d_func()->loadDidSucceed();
}

void QtWebPageLoadClient::dispatchLoadFailed(const QtWebError& error)
{
    int errorCode = error.errorCode();

    if (error.isCancellation() || errorCode == kWKErrorCodeFrameLoadInterruptedByPolicyChange || errorCode == kWKErrorCodePlugInWillHandleLoad) {
        // Make sure that LoadStartedStatus has a counterpart when e.g. requesting a download.
        dispatchLoadSucceeded();
        return;
    }

    m_webView->d_func()->loadDidFail(error);
}

static QtWebPageLoadClient* toQtWebPageLoadClient(const void* clientInfo)
{
    ASSERT(clientInfo);
    return reinterpret_cast<QtWebPageLoadClient*>(const_cast<void*>(clientInfo));
}

void QtWebPageLoadClient::didStartProvisionalLoadForFrame(WKPageRef, WKFrameRef frame, WKTypeRef, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;
    QString urlStr(toImpl(frame)->provisionalURL());
    QUrl qUrl = urlStr;
    toQtWebPageLoadClient(clientInfo)->didStartProvisionalLoad(qUrl);
}

void QtWebPageLoadClient::didFailProvisionalLoadWithErrorForFrame(WKPageRef, WKFrameRef frame, WKErrorRef error, WKTypeRef, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;
    toQtWebPageLoadClient(clientInfo)->dispatchLoadFailed(error);
}

void QtWebPageLoadClient::didCommitLoadForFrame(WKPageRef, WKFrameRef frame, WKTypeRef, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;
    toQtWebPageLoadClient(clientInfo)->didCommitLoad();
}

void QtWebPageLoadClient::didFinishLoadForFrame(WKPageRef, WKFrameRef frame, WKTypeRef, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;
    toQtWebPageLoadClient(clientInfo)->dispatchLoadSucceeded();
}

void QtWebPageLoadClient::didFailLoadWithErrorForFrame(WKPageRef, WKFrameRef frame, WKErrorRef error, WKTypeRef, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;
    toQtWebPageLoadClient(clientInfo)->dispatchLoadFailed(error);
}

void QtWebPageLoadClient::didSameDocumentNavigationForFrame(WKPageRef, WKFrameRef frame, WKSameDocumentNavigationType type, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;
    toQtWebPageLoadClient(clientInfo)->didSameDocumentNavigation();
}

void QtWebPageLoadClient::didReceiveTitleForFrame(WKPageRef, WKStringRef title, WKFrameRef frame, WKTypeRef, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;
    toQtWebPageLoadClient(clientInfo)->didReceiveTitle();
}

void QtWebPageLoadClient::didStartProgress(WKPageRef, const void* clientInfo)
{
    toQtWebPageLoadClient(clientInfo)->didChangeProgress(0);
}

void QtWebPageLoadClient::didChangeProgress(WKPageRef page, const void* clientInfo)
{
    toQtWebPageLoadClient(clientInfo)->didChangeProgress(WKPageGetEstimatedProgress(page) * 100);
}

void QtWebPageLoadClient::didFinishProgress(WKPageRef, const void* clientInfo)
{
    toQtWebPageLoadClient(clientInfo)->didChangeProgress(100);
}

void QtWebPageLoadClient::didChangeBackForwardList(WKPageRef, WKBackForwardListItemRef, WKArrayRef, const void *clientInfo)
{
    toQtWebPageLoadClient(clientInfo)->didChangeBackForwardList();
}

} // namespace Webkit

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
#include "WKStringQt.h"
#include "qquickwebview_p.h"
#include "qquickwebview_p_p.h"
#include "qwebloadrequest_p.h"
#include <KURL.h>
#include <WKFrame.h>
#include <WebPageProxy.h>

using namespace WebCore;

namespace WebKit {

QtWebPageLoadClient::QtWebPageLoadClient(WKPageRef pageRef, QQuickWebView* webView)
    : m_webView(webView)
    , m_webPageProxy(toImpl(pageRef))
    , m_loadProgress(0)
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
    loadClient.didFirstVisuallyNonEmptyLayoutForFrame = didFirstVisuallyNonEmptyLayoutForFrame;
    loadClient.didChangeBackForwardList = didChangeBackForwardList;
    WKPageSetPageLoaderClient(pageRef, &loadClient);
}

void QtWebPageLoadClient::completeLoadWhenProcessDidCrashIfNeeded()
{
    // Check if loading was ongoing, when process crashed.
    if (!m_loadProgress || m_loadProgress == 100)
        return;

    QUrl url(KURL(WebCore::ParsedURLString, m_webPageProxy->urlAtProcessExit()));
    QWebLoadRequest loadRequest(url, QQuickWebView::LoadFailedStatus, QLatin1String("The web process crashed."), QQuickWebView::InternalErrorDomain, 0);

    emit m_webView->loadingChanged(&loadRequest);
    setLoadProgress(100);
}

void QtWebPageLoadClient::didStartProvisionalLoad(const QUrl& url)
{
    QWebLoadRequest loadRequest(url, QQuickWebView::LoadStartedStatus);
    emit m_webView->loadingChanged(&loadRequest);
}

void QtWebPageLoadClient::didCommitLoad()
{
    emit m_webView->navigationHistoryChanged();
    emit m_webView->urlChanged();
    emit m_webView->titleChanged();
    m_webView->d_func()->loadDidCommit();
}

void QtWebPageLoadClient::didSameDocumentNavigation()
{
    emit m_webView->navigationHistoryChanged();
    emit m_webView->urlChanged();
}

void QtWebPageLoadClient::didReceiveTitle()
{
    emit m_webView->titleChanged();
}

void QtWebPageLoadClient::didFirstVisuallyNonEmptyLayout()
{
    m_webView->d_func()->didFinishFirstNonEmptyLayout();
}

void QtWebPageLoadClient::didChangeBackForwardList()
{
    m_webView->d_func()->didChangeBackForwardList();
}

void QtWebPageLoadClient::dispatchLoadSucceeded()
{
    m_webView->d_func()->loadDidSucceed();
}

void QtWebPageLoadClient::dispatchLoadFailed(WKErrorRef error)
{
    int errorCode = WKErrorGetErrorCode(error);
    if (toImpl(error)->platformError().isCancellation() || errorCode == kWKErrorCodeFrameLoadInterruptedByPolicyChange || errorCode == kWKErrorCodePlugInWillHandleLoad) {
        // Make sure that LoadStartedStatus has a counterpart when e.g. requesting a download.
        dispatchLoadSucceeded();
        return;
    }

    QtWebError qtError(error);
    QWebLoadRequest loadRequest(qtError.url(), QQuickWebView::LoadFailedStatus, qtError.description(), static_cast<QQuickWebView::ErrorDomain>(qtError.type()), qtError.errorCode());
    emit m_webView->loadingChanged(&loadRequest);
}

void QtWebPageLoadClient::setLoadProgress(int loadProgress)
{
    m_loadProgress = loadProgress;
    emit m_webView->loadProgressChanged();
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
    WebFrameProxy* wkframe = toImpl(frame);
    QString urlStr(wkframe->provisionalURL());
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
    QtWebPageLoadClient* client = toQtWebPageLoadClient(clientInfo);
    client->setLoadProgress(0);
    client->m_webView->d_func()->setIcon(QUrl());
}

void QtWebPageLoadClient::didChangeProgress(WKPageRef page, const void* clientInfo)
{
    toQtWebPageLoadClient(clientInfo)->setLoadProgress(WKPageGetEstimatedProgress(page) * 100);
}

void QtWebPageLoadClient::didFinishProgress(WKPageRef, const void* clientInfo)
{
    toQtWebPageLoadClient(clientInfo)->setLoadProgress(100);
}

void QtWebPageLoadClient::didFirstVisuallyNonEmptyLayoutForFrame(WKPageRef, WKFrameRef frame, WKTypeRef, const void *clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;
    toQtWebPageLoadClient(clientInfo)->didFirstVisuallyNonEmptyLayout();
}

void QtWebPageLoadClient::didChangeBackForwardList(WKPageRef, WKBackForwardListItemRef, WKArrayRef, const void *clientInfo)
{
    toQtWebPageLoadClient(clientInfo)->didChangeBackForwardList();
}

} // namespace Webkit

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

#include "WKStringQt.h"
#include "qquickwebview_p.h"
#include "qquickwebview_p_p.h"
#include <WKFrame.h>

QtWebPageLoadClient::QtWebPageLoadClient(WKPageRef pageRef, QQuickWebView* webView)
    : m_webView(webView)
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

void QtWebPageLoadClient::didStartProvisionalLoadForFrame()
{
    emit m_webView->navigationStateChanged();
    emit m_webView->loadStarted();
}

void QtWebPageLoadClient::didCommitLoadForFrame(const QUrl& url)
{
    emit m_webView->navigationStateChanged();
    emit m_webView->urlChanged(url);
    m_webView->d_func()->loadDidCommit();
}

void QtWebPageLoadClient::didSameDocumentNavigationForFrame(const QUrl& url)
{
    emit m_webView->navigationStateChanged();
    emit m_webView->urlChanged(url);
}

void QtWebPageLoadClient::didReceiveTitleForFrame(const QString& title)
{
    emit m_webView->titleChanged(title);
}

void QtWebPageLoadClient::didFirstVisuallyNonEmptyLayoutForFrame()
{
    m_webView->d_func()->didFinishFirstNonEmptyLayout();
}

void QtWebPageLoadClient::didChangeBackForwardList()
{
    m_webView->d_func()->didChangeBackForwardList();
}

void QtWebPageLoadClient::dispatchLoadSucceeded()
{
    emit m_webView->navigationStateChanged();
    emit m_webView->loadSucceeded();
}

void QtWebPageLoadClient::dispatchLoadFailed(WKErrorRef error)
{
    emit m_webView->navigationStateChanged();

    int errorCode = WKErrorGetErrorCode(error);
    if (toImpl(error)->platformError().isCancellation() || errorCode == kWKErrorCodeFrameLoadInterruptedByPolicyChange || errorCode == kWKErrorCodePlugInWillHandleLoad)
        return;

    QtWebError qtError(error);
    emit m_webView->loadFailed(static_cast<QQuickWebView::ErrorDomain>(qtError.type()), qtError.errorCode(), qtError.url(), qtError.description());
}

void QtWebPageLoadClient::setLoadProgress(int loadProgress)
{
    m_loadProgress = loadProgress;
    emit m_webView->loadProgressChanged(m_loadProgress);
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
    toQtWebPageLoadClient(clientInfo)->didStartProvisionalLoadForFrame();
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
    WebFrameProxy* wkframe = toImpl(frame);
    QString urlStr(wkframe->url());
    QUrl qUrl = urlStr;
    toQtWebPageLoadClient(clientInfo)->didCommitLoadForFrame(qUrl);
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

void QtWebPageLoadClient::didSameDocumentNavigationForFrame(WKPageRef page, WKFrameRef frame, WKSameDocumentNavigationType type, WKTypeRef userData, const void* clientInfo)
{
    WebFrameProxy* wkframe = toImpl(frame);
    QString urlStr(wkframe->url());
    QUrl qUrl = urlStr;
    toQtWebPageLoadClient(clientInfo)->didSameDocumentNavigationForFrame(qUrl);
}

void QtWebPageLoadClient::didReceiveTitleForFrame(WKPageRef, WKStringRef title, WKFrameRef frame, WKTypeRef, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;
    QString qTitle = WKStringCopyQString(title);
    toQtWebPageLoadClient(clientInfo)->didReceiveTitleForFrame(qTitle);
}

void QtWebPageLoadClient::didStartProgress(WKPageRef, const void* clientInfo)
{
    toQtWebPageLoadClient(clientInfo)->setLoadProgress(0);
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
    toQtWebPageLoadClient(clientInfo)->didFirstVisuallyNonEmptyLayoutForFrame();
}

void QtWebPageLoadClient::didChangeBackForwardList(WKPageRef, WKBackForwardListItemRef, WKArrayRef, const void *clientInfo)
{
    toQtWebPageLoadClient(clientInfo)->didChangeBackForwardList();
}

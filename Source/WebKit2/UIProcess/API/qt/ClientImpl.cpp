/*
    Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "ClientImpl.h"

#include "WebFrameProxy.h"
#include "WKAPICast.h"
#include "WKStringQt.h"
#include "WKURLQt.h"
#include <qwkcontext.h>
#include <qwkpage.h>
#include <qwkpage_p.h>
#include <WKFrame.h>
#include <WKType.h>

using namespace WebKit;

static QWKContext* toQWKContext(const void* clientInfo)
{
    if (clientInfo)
        return reinterpret_cast<QWKContext*>(const_cast<void*>(clientInfo));
    return 0;
}

static QWKPage* toQWKPage(const void* clientInfo)
{
    if (clientInfo)
        return reinterpret_cast<QWKPage*>(const_cast<void*>(clientInfo));
    return 0;
}

static void loadFinished(WKFrameRef frame, const void* clientInfo, bool ok)
{
    if (!WKFrameIsMainFrame(frame))
        return;
    emit toQWKPage(clientInfo)->loadFinished(ok);
    QWKPagePrivate::get(toQWKPage(clientInfo))->updateNavigationActions();
}

void qt_wk_didStartProvisionalLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;
    emit toQWKPage(clientInfo)->loadStarted();
    QWKPagePrivate::get(toQWKPage(clientInfo))->updateNavigationActions();
}

void qt_wk_didReceiveServerRedirectForProvisionalLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
}

void qt_wk_didFailProvisionalLoadWithErrorForFrame(WKPageRef page, WKFrameRef frame, WKErrorRef error, WKTypeRef userData, const void* clientInfo)
{
    loadFinished(frame, clientInfo, false);
}

void qt_wk_didCommitLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;
    WebFrameProxy* wkframe = toImpl(frame);
    QString urlStr(wkframe->url());
    QUrl qUrl = urlStr;
    emit toQWKPage(clientInfo)->urlChanged(qUrl);
    QWKPagePrivate::get(toQWKPage(clientInfo))->updateNavigationActions();
}

void qt_wk_didFinishDocumentLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    // FIXME: Implement (bug #44934)
}

void qt_wk_didFinishLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    loadFinished(frame, clientInfo, true);
}

void qt_wk_didFailLoadWithErrorForFrame(WKPageRef page, WKFrameRef frame, WKErrorRef error, WKTypeRef userData, const void* clientInfo)
{
    loadFinished(frame, clientInfo, false);
}

void qt_wk_didReceiveTitleForFrame(WKPageRef page, WKStringRef title, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;
    QString qTitle = WKStringCopyQString(title);
    emit toQWKPage(clientInfo)->titleChanged(qTitle);
}

void qt_wk_didFirstLayoutForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;
    emit toQWKPage(clientInfo)->initialLayoutCompleted();
}

void qt_wk_didRemoveFrameFromHierarchy(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    // FIXME: Implement (bug #46432)
}

void qt_wk_didFirstVisuallyNonEmptyLayoutForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;
    // FIXME: emit toWKView(clientInfo)->initialLayoutCompleted();
}

void qt_wk_didStartProgress(WKPageRef page, const void* clientInfo)
{
    emit toQWKPage(clientInfo)->loadProgress(0);
}

void qt_wk_didChangeProgress(WKPageRef page, const void* clientInfo)
{
    emit toQWKPage(clientInfo)->loadProgress(WKPageGetEstimatedProgress(page) * 100);
}

void qt_wk_didFinishProgress(WKPageRef page, const void* clientInfo)
{
    emit toQWKPage(clientInfo)->loadProgress(100);
}

void qt_wk_didBecomeUnresponsive(WKPageRef page, const void* clientInfo)
{
}

void qt_wk_didBecomeResponsive(WKPageRef page, const void* clientInfo)
{
}

WKPageRef qt_wk_createNewPage(WKPageRef page, WKDictionaryRef features, WKEventModifiers modifiers, WKEventMouseButton mouseButton, const void* clientInfo)
{
    QWKPage* wkPage = toQWKPage(clientInfo);
    QWKPagePrivate* d = QWKPagePrivate::get(wkPage);
    QWKPage::CreateNewPageFn createNewPageFn = d->createNewPageFn;

    if (!createNewPageFn)
        return 0;

    if (QWKPage* newPage = createNewPageFn(wkPage)) {
        WKRetain(newPage->pageRef());
        return newPage->pageRef();
    }

    return 0;
}

void qt_wk_showPage(WKPageRef page, const void* clientInfo)
{
}

void qt_wk_close(WKPageRef page, const void* clientInfo)
{
    emit toQWKPage(clientInfo)->windowCloseRequested();
}

void qt_wk_takeFocus(WKPageRef page, WKFocusDirection direction, const void *clientInfo)
{
    emit toQWKPage(clientInfo)->focusNextPrevChild(direction == kWKFocusDirectionForward);
}

void qt_wk_runJavaScriptAlert(WKPageRef page, WKStringRef alertText, WKFrameRef frame, const void* clientInfo)
{
}

void qt_wk_setStatusText(WKPageRef page, WKStringRef text, const void *clientInfo)
{
    QString qText = WKStringCopyQString(text);
    emit toQWKPage(clientInfo)->statusBarMessage(qText);
}

void qt_wk_didSameDocumentNavigationForFrame(WKPageRef page, WKFrameRef frame, WKSameDocumentNavigationType type, WKTypeRef userData, const void* clientInfo)
{
    WebFrameProxy* wkframe = toImpl(frame);
    QString urlStr(wkframe->url());
    QUrl qUrl = urlStr;
    emit toQWKPage(clientInfo)->urlChanged(qUrl);
    QWKPagePrivate::get(toQWKPage(clientInfo))->updateNavigationActions();
}

void qt_wk_didChangeIconForPageURL(WKIconDatabaseRef iconDatabase, WKURLRef pageURL, const void* clientInfo)
{
    QUrl qUrl = WKURLCopyQUrl(pageURL);
    emit toQWKContext(clientInfo)->iconChangedForPageURL(qUrl);
}

void qt_wk_didRemoveAllIcons(WKIconDatabaseRef iconDatabase, const void* clientInfo)
{
}

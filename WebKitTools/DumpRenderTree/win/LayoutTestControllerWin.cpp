/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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

#include "DumpRenderTree.h"
#include "LayoutTestController.h"

#include "EditingDelegate.h"
#include "WorkQueue.h"
#include "WorkQueueItem.h"
#include <WebCore/COMPtr.h>
#include <wtf/Platform.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <JavaScriptCore/Assertions.h>
#include <JavaScriptCore/JavaScriptCore.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <WebKit/IWebViewPrivate.h>
#include <string>
#include <CoreFoundation/CoreFoundation.h>

using std::string;
using std::wstring;

void LayoutTestController::addDisallowedURL(JSStringRef url)
{
    // FIXME: Implement!
}

void LayoutTestController::clearBackForwardList()
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebBackForwardList> backForwardList;
    if (FAILED(webView->backForwardList(&backForwardList)))
        return;

    COMPtr<IWebHistoryItem> item;
    if (FAILED(backForwardList->currentItem(&item)))
        return;

    // We clear the history by setting the back/forward list's capacity to 0
    // then restoring it back and adding back the current item.
    int capacity;
    if (FAILED(backForwardList->capacity(&capacity)))
        return;

    backForwardList->setCapacity(0);
    backForwardList->setCapacity(capacity);
    backForwardList->addItem(item.get());
    backForwardList->goToItem(item.get());
}

JSStringRef LayoutTestController::copyDecodedHostName(JSStringRef name)
{
    // FIXME: Implement!
    return 0;
}

JSStringRef LayoutTestController::copyEncodedHostName(JSStringRef name)
{
    // FIXME: Implement!
    return 0;
}

void LayoutTestController::display()
{
    displayWebView();
}

void LayoutTestController::keepWebHistory()
{
    // FIXME: Implement!
}

void LayoutTestController::notifyDone()
{
    // Same as on mac.  This can be shared.
    if (m_waitToDump && !topLoadingFrame && !WorkQueue::shared()->count())
        dump();
    m_waitToDump = false;
}

void LayoutTestController::queueBackNavigation(int howFarBack)
{
    // Same as on mac.  This can be shared.
    WorkQueue::shared()->queue(new BackItem(howFarBack));
}

void LayoutTestController::queueForwardNavigation(int howFarForward)
{
    // Same as on mac.  This can be shared.
    WorkQueue::shared()->queue(new ForwardItem(howFarForward));
}

static wstring jsStringRefToWString(JSStringRef jsStr)
{
    size_t length = JSStringGetLength(jsStr);
    Vector<WCHAR> buffer(length + 1);
    memcpy(buffer.data(), JSStringGetCharactersPtr(jsStr), length * sizeof(WCHAR));
    buffer[length] = '\0';

    return buffer.data();
}

void LayoutTestController::queueLoad(JSStringRef url, JSStringRef target)
{
    COMPtr<IWebDataSource> dataSource;
    if (FAILED(frame->dataSource(&dataSource)))
        return;

    COMPtr<IWebURLResponse> response;
    if (FAILED(dataSource->response(&response)) || !response)
        return;

    BSTR responseURLBSTR;
    if (FAILED(response->URL(&responseURLBSTR)))
        return;
    wstring responseURL(responseURLBSTR, SysStringLen(responseURLBSTR));
    SysFreeString(responseURLBSTR);

    // FIXME: We should do real relative URL resolution here.
    int lastSlash = responseURL.rfind('/');
    if (lastSlash != -1)
        responseURL = responseURL.substr(0, lastSlash);

    wstring wURL = jsStringRefToWString(url);
    wstring wAbosuluteURL = responseURL + TEXT("/") + wURL;
    JSRetainPtr<JSStringRef> jsAbsoluteURL(Adopt, JSStringCreateWithCharacters(wAbosuluteURL.data(), wAbosuluteURL.length()));

    WorkQueue::shared()->queue(new LoadItem(jsAbsoluteURL.get(), target));
}

void LayoutTestController::queueReload()
{
    WorkQueue::shared()->queue(new ReloadItem);
}

void LayoutTestController::queueScript(JSStringRef script)
{
    WorkQueue::shared()->queue(new ScriptItem(script));
}

void LayoutTestController::setAcceptsEditing(bool acceptsEditing)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewEditing> viewEditing;
    if (FAILED(webView->QueryInterface(&viewEditing)))
        return;

    COMPtr<IWebEditingDelegate> delegate;
    if (FAILED(viewEditing->editingDelegate(&delegate)))
        return;

    EditingDelegate* editingDelegate = (EditingDelegate*)(IWebEditingDelegate*)delegate.get();
    editingDelegate->setAcceptsEditing(acceptsEditing);
}

void LayoutTestController::setCustomPolicyDelegate(bool setDelegate)
{
    // FIXME: Implement!
}

void LayoutTestController::setMainFrameIsFirstResponder(bool flag)
{
    // FIXME: Implement!
}

void LayoutTestController::setTabKeyCyclesThroughElements(bool shouldCycle)
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewPrivate> viewPrivate;
    if (FAILED(webView->QueryInterface(&viewPrivate)))
        return;

    viewPrivate->setTabKeyCyclesThroughElements(shouldCycle ? TRUE : FALSE);
}

void LayoutTestController::setUseDashboardCompatibilityMode(bool flag)
{
    // FIXME: Implement!
}

void LayoutTestController::setUserStyleSheetEnabled(bool flag)
{
    // FIXME: Implement!
}

void LayoutTestController::setUserStyleSheetLocation(JSStringRef path)
{
    // FIXME: Implement!
}

void LayoutTestController::setWindowIsKey(bool flag)
{
    // FIXME: Implement!
}

static const CFTimeInterval waitToDumpWatchdogInterval = 10.0;

static void waitUntilDoneWatchdogFired(CFRunLoopTimerRef timer, void* info)
{
    const char* message = "FAIL: Timed out waiting for notifyDone to be called\n";
    fprintf(stderr, message);
    fprintf(stdout, message);
    dump();
}

void LayoutTestController::setWaitToDump(bool waitUntilDone)
{
    // Same as on mac.  This can be shared.
    m_waitToDump = waitUntilDone;
    if (m_waitToDump && !waitToDumpWatchdog)
        ::waitToDumpWatchdog = CFRunLoopTimerCreate(0, 0, waitToDumpWatchdogInterval, 0, 0, waitUntilDoneWatchdogFired, NULL);
}

int LayoutTestController::windowCount()
{
    // FIXME: Implement!
    return 1;
}

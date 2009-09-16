/*
 * Copyright (C) 2005, 2006, 2007, 2009 Apple Inc. All rights reserved.
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
#include "FrameLoadDelegate.h"

#include "AccessibilityController.h"
#include "DumpRenderTree.h"
#include "EventSender.h"
#include "GCController.h"
#include "LayoutTestController.h"
#include "WorkQueueItem.h"
#include "WorkQueue.h"
#include <WebCore/COMPtr.h>
#include <JavaScriptCore/Assertions.h>
#include <JavaScriptCore/JavaScriptCore.h>
#include <WebKit/WebKit.h>
#include <wtf/Vector.h>
#include <stdio.h>
#include <string>

using std::string;

static FrameLoadDelegate* g_delegateWaitingOnTimer;

string BSTRtoString(BSTR bstr)
{
    int result = WideCharToMultiByte(CP_UTF8, 0, bstr, SysStringLen(bstr) + 1, 0, 0, 0, 0);
    Vector<char> utf8Vector(result);
    result = WideCharToMultiByte(CP_UTF8, 0, bstr, SysStringLen(bstr) + 1, utf8Vector.data(), result, 0, 0);
    if (!result)
        return string();

    return string(utf8Vector.data(), utf8Vector.size() - 1);
}

string descriptionSuitableForTestResult(IWebFrame* webFrame)
{
    COMPtr<IWebView> webView;
    if (FAILED(webFrame->webView(&webView)))
        return string();

    COMPtr<IWebFrame> mainFrame;
    if (FAILED(webView->mainFrame(&mainFrame)))
        return string();

    BSTR frameNameBSTR;
    if (FAILED(webFrame->name(&frameNameBSTR)) || BSTRtoString(frameNameBSTR).empty() )
        return (webFrame == mainFrame) ? "main frame" : string();

    string frameName = (webFrame == mainFrame) ? "main frame" : "frame";
    frameName += " \"" + BSTRtoString(frameNameBSTR) + "\""; 

    SysFreeString(frameNameBSTR); 
    return frameName;
}

FrameLoadDelegate::FrameLoadDelegate()
    : m_refCount(1)
    , m_gcController(new GCController)
    , m_accessibilityController(new AccessibilityController)
{
}

FrameLoadDelegate::~FrameLoadDelegate()
{
}

HRESULT STDMETHODCALLTYPE FrameLoadDelegate::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebFrameLoadDelegate*>(this);
    else if (IsEqualGUID(riid, IID_IWebFrameLoadDelegate))
        *ppvObject = static_cast<IWebFrameLoadDelegate*>(this);
    else if (IsEqualGUID(riid, IID_IWebFrameLoadDelegatePrivate))
        *ppvObject = static_cast<IWebFrameLoadDelegatePrivate*>(this);
    else if (IsEqualGUID(riid, IID_IWebFrameLoadDelegatePrivate2))
        *ppvObject = static_cast<IWebFrameLoadDelegatePrivate2*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE FrameLoadDelegate::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE FrameLoadDelegate::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}


HRESULT STDMETHODCALLTYPE FrameLoadDelegate::didStartProvisionalLoadForFrame( 
    /* [in] */ IWebView* webView,
    /* [in] */ IWebFrame* frame) 
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks())
        printf("%s - didStartProvisionalLoadForFrame\n", descriptionSuitableForTestResult(frame).c_str());

    // Make sure we only set this once per test.  If it gets cleared, and then set again, we might
    // end up doing two dumps for one test.
    if (!topLoadingFrame && !done)
        topLoadingFrame = frame;

    return S_OK; 
}

HRESULT STDMETHODCALLTYPE FrameLoadDelegate::didReceiveServerRedirectForProvisionalLoadForFrame( 
    /* [in] */ IWebView *webView,
    /* [in] */ IWebFrame *frame)
{ 
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks())
        printf("%s - didReceiveServerRedirectForProvisionalLoadForFrame\n", descriptionSuitableForTestResult(frame).c_str());

    return S_OK;
}

HRESULT STDMETHODCALLTYPE FrameLoadDelegate::didFailProvisionalLoadWithError( 
    /* [in] */ IWebView *webView,
    /* [in] */ IWebError *error,
    /* [in] */ IWebFrame *frame)
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks())
        printf("%s - didFailProvisionalLoadWithError\n", descriptionSuitableForTestResult(frame).c_str());

    return S_OK;
}

HRESULT STDMETHODCALLTYPE FrameLoadDelegate::didCommitLoadForFrame( 
    /* [in] */ IWebView *webView,
    /* [in] */ IWebFrame *frame)
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks())
        printf("%s - didCommitLoadForFrame\n", descriptionSuitableForTestResult(frame).c_str());

    COMPtr<IWebViewPrivate> webViewPrivate;
    HRESULT hr = webView->QueryInterface(&webViewPrivate);
    if (FAILED(hr))
        return hr;
    webViewPrivate->updateFocusedAndActiveState();

    return S_OK;
}

HRESULT STDMETHODCALLTYPE FrameLoadDelegate::didReceiveTitle( 
    /* [in] */ IWebView *webView,
    /* [in] */ BSTR title,
    /* [in] */ IWebFrame *frame)
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks())
        printf("%s - didReceiveTitle: %S\n", descriptionSuitableForTestResult(frame).c_str(), title);

    if (::gLayoutTestController->dumpTitleChanges() && !done)
        printf("TITLE CHANGED: %S\n", title ? title : L"");
    return S_OK;
}

void FrameLoadDelegate::processWork()
{
    // if another load started, then wait for it to complete.
    if (topLoadingFrame)
        return;

    // if we finish all the commands, we're ready to dump state
    if (WorkQueue::shared()->processWork() && !::gLayoutTestController->waitToDump())
        dump();
}

void FrameLoadDelegate::resetToConsistentState()
{
    m_accessibilityController->resetToConsistentState();
}

static void CALLBACK processWorkTimer(HWND, UINT, UINT_PTR id, DWORD)
{
    ::KillTimer(0, id);
    FrameLoadDelegate* d = g_delegateWaitingOnTimer;
    g_delegateWaitingOnTimer = 0;
    d->processWork();
}

void FrameLoadDelegate::locationChangeDone(IWebError*, IWebFrame* frame)
{
    if (frame != topLoadingFrame)
        return;

    topLoadingFrame = 0;
    WorkQueue::shared()->setFrozen(true);

    if (::gLayoutTestController->waitToDump())
        return;

    if (WorkQueue::shared()->count()) {
        ASSERT(!g_delegateWaitingOnTimer);
        g_delegateWaitingOnTimer = this;
        ::SetTimer(0, 0, 0, processWorkTimer);
        return;
    }

    dump();
}

HRESULT STDMETHODCALLTYPE FrameLoadDelegate::didFinishLoadForFrame( 
    /* [in] */ IWebView* webView,
    /* [in] */ IWebFrame* frame)
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks())
        printf("%s - didFinishLoadForFrame\n", descriptionSuitableForTestResult(frame).c_str());

    locationChangeDone(0, frame);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE FrameLoadDelegate::didFailLoadWithError( 
    /* [in] */ IWebView* webView,
    /* [in] */ IWebError* error,
    /* [in] */ IWebFrame* frame)
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks())
        printf("%s - didFailLoadWithError\n", descriptionSuitableForTestResult(frame).c_str());

    locationChangeDone(error, frame);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE FrameLoadDelegate::willPerformClientRedirectToURL( 
    /* [in] */ IWebView *webView,
    /* [in] */ BSTR url,  
    /* [in] */ double delaySeconds,
    /* [in] */ DATE fireDate,
    /* [in] */ IWebFrame *frame)
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks())
        printf("%s - willPerformClientRedirectToURL: %S \n", descriptionSuitableForTestResult(frame).c_str(),
                urlSuitableForTestResult(std::wstring(url, ::SysStringLen(url))).c_str());

    return S_OK;
}

HRESULT STDMETHODCALLTYPE FrameLoadDelegate::didCancelClientRedirectForFrame( 
    /* [in] */ IWebView *webView,
    /* [in] */ IWebFrame *frame)
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks())
        printf("%s - didCancelClientRedirectForFrame\n", descriptionSuitableForTestResult(frame).c_str());

    return S_OK;
}


HRESULT STDMETHODCALLTYPE FrameLoadDelegate::willCloseFrame( 
    /* [in] */ IWebView *webView,
    /* [in] */ IWebFrame *frame)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE FrameLoadDelegate::didClearWindowObject( 
    /* [in] */ IWebView*webView,
    /* [in] */ JSContextRef context,
    /* [in] */ JSObjectRef windowObject,
    /* [in] */ IWebFrame* frame)
{
    JSValueRef exception = 0;

    ::gLayoutTestController->makeWindowObject(context, windowObject, &exception);
    ASSERT(!exception);

    m_gcController->makeWindowObject(context, windowObject, &exception);
    ASSERT(!exception);

    m_accessibilityController->makeWindowObject(context, windowObject, &exception);
    ASSERT(!exception);

    JSStringRef eventSenderStr = JSStringCreateWithUTF8CString("eventSender");
    JSValueRef eventSender = makeEventSender(context);
    JSObjectSetProperty(context, windowObject, eventSenderStr, eventSender, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, 0);
    JSStringRelease(eventSenderStr);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE FrameLoadDelegate::didFinishDocumentLoadForFrame( 
    /* [in] */ IWebView *sender,
    /* [in] */ IWebFrame *frame)
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks())
        printf("%s - didFinishDocumentLoadForFrame\n",
                descriptionSuitableForTestResult(frame).c_str());
    if (!done) {
        COMPtr<IWebFramePrivate> webFramePrivate;
        HRESULT hr = frame->QueryInterface(&webFramePrivate);
        if (FAILED(hr))
            return hr;
        unsigned pendingFrameUnloadEvents;
        hr = webFramePrivate->pendingFrameUnloadEventCount(&pendingFrameUnloadEvents);
        if (FAILED(hr))
            return hr;
        if (pendingFrameUnloadEvents)
            printf("%s - has %u onunload handler(s)\n",
                    descriptionSuitableForTestResult(frame).c_str(), pendingFrameUnloadEvents);
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE FrameLoadDelegate::didHandleOnloadEventsForFrame( 
    /* [in] */ IWebView *sender,
    /* [in] */ IWebFrame *frame)
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks())
        printf("%s - didHandleOnloadEventsForFrame\n",
                descriptionSuitableForTestResult(frame).c_str());

    return S_OK;
}

HRESULT STDMETHODCALLTYPE FrameLoadDelegate::didFirstVisuallyNonEmptyLayoutInFrame( 
    /* [in] */ IWebView *sender,
    /* [in] */ IWebFrame *frame)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE FrameLoadDelegate::didDisplayInsecureContent( 
    /* [in] */ IWebView *sender)
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks())
        printf("didDisplayInsecureContent\n");

    return S_OK;
}

HRESULT STDMETHODCALLTYPE FrameLoadDelegate::didRunInsecureContent( 
    /* [in] */ IWebView *sender,
    /* [in] */ IWebSecurityOrigin *origin)
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks())
        printf("didRunInsecureContent\n");

    return S_OK;
}


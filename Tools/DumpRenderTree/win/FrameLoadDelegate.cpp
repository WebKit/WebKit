/*
 * Copyright (C) 2005-2007, 2009, 2014-2015 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#include <comutil.h>
#include "DumpRenderTree.h"
#include "EventSender.h"
#include "GCController.h"
#include "JSWrapper.h"
#include "ReftestFunctions.h"
#include "TestRunner.h"
#include "TextInputController.h"
#include "WebCoreTestSupport.h"
#include "WorkQueueItem.h"
#include "WorkQueue.h"
#include <WebCore/COMPtr.h>
#include <JavaScriptCore/JavaScriptCore.h>
#include <WebKitLegacy/WebKit.h>
#include <stdio.h>
#include <string>
#include <wtf/Assertions.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>

using std::string;

string descriptionSuitableForTestResult(IWebFrame* webFrame)
{
    COMPtr<IWebView> webView;
    if (FAILED(webFrame->webView(&webView)))
        return string();

    COMPtr<IWebFrame> mainFrame;
    if (FAILED(webView->mainFrame(&mainFrame)))
        return string();

    _bstr_t frameNameBSTR;
    if (FAILED(webFrame->name(&frameNameBSTR.GetBSTR())) || !frameNameBSTR.length())
        return (webFrame == mainFrame) ? "main frame" : string();

    string frameName = (webFrame == mainFrame) ? "main frame" : "frame";
    frameName += " \"" + toUTF8(frameNameBSTR) + "\""; 

    return frameName;
}

FrameLoadDelegate::FrameLoadDelegate()
    : m_gcController(makeUnique<GCController>())
    , m_accessibilityController(makeUnique<AccessibilityController>())
    , m_textInputController(makeUnique<TextInputController>())
{
}

FrameLoadDelegate::~FrameLoadDelegate()
{
}

HRESULT FrameLoadDelegate::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebFrameLoadDelegate*>(this);
    else if (IsEqualGUID(riid, IID_IWebFrameLoadDelegate))
        *ppvObject = static_cast<IWebFrameLoadDelegate*>(this);
    else if (IsEqualGUID(riid, IID_IWebFrameLoadDelegatePrivate))
        *ppvObject = static_cast<IWebFrameLoadDelegatePrivate*>(this);
    else if (IsEqualGUID(riid, IID_IWebFrameLoadDelegatePrivate2))
        *ppvObject = static_cast<IWebFrameLoadDelegatePrivate2*>(this);
    else if (IsEqualGUID(riid, IID_IWebNotificationObserver))
        *ppvObject = static_cast<IWebNotificationObserver*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG FrameLoadDelegate::AddRef()
{
    return ++m_refCount;
}

ULONG FrameLoadDelegate::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

HRESULT FrameLoadDelegate::didStartProvisionalLoadForFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame* frame)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        fprintf(testResult, "%s - didStartProvisionalLoadForFrame\n", descriptionSuitableForTestResult(frame).c_str());

    // Make sure we only set this once per test.  If it gets cleared, and then set again, we might
    // end up doing two dumps for one test.
    if (!topLoadingFrame && !done)
        topLoadingFrame = frame;

    return S_OK; 
}

HRESULT FrameLoadDelegate::didReceiveServerRedirectForProvisionalLoadForFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame* frame)
{ 
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        fprintf(testResult, "%s - didReceiveServerRedirectForProvisionalLoadForFrame\n", descriptionSuitableForTestResult(frame).c_str());

    return S_OK;
}

HRESULT FrameLoadDelegate::didChangeLocationWithinPageForFrame(_In_opt_ IWebView* , _In_opt_ IWebFrame* frame)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        fprintf(testResult, "%s - didChangeLocationWithinPageForFrame\n", descriptionSuitableForTestResult(frame).c_str());

    return S_OK;
}

HRESULT FrameLoadDelegate::didFailProvisionalLoadWithError(_In_opt_ IWebView*, _In_opt_ IWebError* error, _In_opt_ IWebFrame* frame)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        fprintf(testResult, "%s - didFailProvisionalLoadWithError\n", descriptionSuitableForTestResult(frame).c_str());

    locationChangeDone(error, frame);
    return S_OK;
}

HRESULT FrameLoadDelegate::didCommitLoadForFrame(_In_opt_ IWebView* webView, _In_opt_ IWebFrame* frame)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        fprintf(testResult, "%s - didCommitLoadForFrame\n", descriptionSuitableForTestResult(frame).c_str());

    COMPtr<IWebViewPrivate2> webViewPrivate;
    HRESULT hr = webView->QueryInterface(&webViewPrivate);
    if (FAILED(hr))
        return hr;
    webViewPrivate->updateFocusedAndActiveState();

    return S_OK;
}

HRESULT FrameLoadDelegate::didReceiveTitle(_In_opt_ IWebView*, _In_ BSTR title, _In_opt_ IWebFrame* frame)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        fprintf(testResult, "%s - didReceiveTitle: %S\n", descriptionSuitableForTestResult(frame).c_str(), title);

    if (::gTestRunner->dumpTitleChanges() && !done)
        fprintf(testResult, "TITLE CHANGED: '%S'\n", title ? title : L"");
    return S_OK;
}

HRESULT FrameLoadDelegate::didChangeIcons(_In_opt_ IWebView*, _In_opt_ IWebFrame* frame)
{
    // This feature is no longer supported. The stub is here to keep backwards compatibility
    // with the COM interface.
    return S_OK;
}

static void CALLBACK dumpAfterWaitAttributeIsRemoved(HWND = nullptr, UINT = 0, UINT_PTR = 0, DWORD = 0)
{
    static UINT_PTR timerID;
    if (frame && WTR::hasReftestWaitAttribute(frame->globalContext())) {
        if (!timerID)
            timerID = ::SetTimer(nullptr, 0, 0, dumpAfterWaitAttributeIsRemoved);
    } else {
        if (timerID) {
            ::KillTimer(nullptr, timerID);
            timerID = 0;
        }
        dump();
    }
}

static void readyToDumpState()
{
    if (frame)
        WTR::sendTestRenderedEvent(frame->globalContext());
    dumpAfterWaitAttributeIsRemoved();
}

void FrameLoadDelegate::processWork()
{
    // if another load started, then wait for it to complete.
    if (topLoadingFrame)
        return;

    // if we finish all the commands, we're ready to dump state
    if (DRT::WorkQueue::singleton().processWork() && !::gTestRunner->waitToDump())
        readyToDumpState();
}

void FrameLoadDelegate::resetToConsistentState()
{
    m_accessibilityController->resetToConsistentState();
}

typedef Vector<COMPtr<FrameLoadDelegate> > DelegateVector;
static DelegateVector& delegatesWithDelayedWork()
{
    static NeverDestroyed<DelegateVector> delegates;
    return delegates;
}

static UINT_PTR processWorkTimerID;

static void CALLBACK processWorkTimer(HWND hwnd, UINT, UINT_PTR id, DWORD)
{
    ASSERT_ARG(id, id == processWorkTimerID);
    ::KillTimer(hwnd, id);
    processWorkTimerID = 0;

    DelegateVector delegates;
    delegates.swap(delegatesWithDelayedWork());

    for (size_t i = 0; i < delegates.size(); ++i)
        delegates[i]->processWork();
}

void FrameLoadDelegate::locationChangeDone(IWebError*, IWebFrame* frame)
{
    if (frame != topLoadingFrame)
        return;

    topLoadingFrame = nullptr;
    auto& workQueue = DRT::WorkQueue::singleton();
    workQueue.setFrozen(true);

    if (::gTestRunner->waitToDump())
        return;

    if (workQueue.count()) {
        if (!processWorkTimerID)
            processWorkTimerID = ::SetTimer(0, 0, 0, processWorkTimer);
        delegatesWithDelayedWork().append(this);
        return;
    }

    readyToDumpState();
}

HRESULT FrameLoadDelegate::didFinishLoadForFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame* frame)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        fprintf(testResult, "%s - didFinishLoadForFrame\n", descriptionSuitableForTestResult(frame).c_str());

    locationChangeDone(0, frame);
    return S_OK;
}

HRESULT FrameLoadDelegate::didFailLoadWithError(_In_opt_ IWebView*, _In_opt_ IWebError* error, _In_opt_ IWebFrame* frame)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        fprintf(testResult, "%s - didFailLoadWithError\n", descriptionSuitableForTestResult(frame).c_str());

    locationChangeDone(error, frame);
    return S_OK;
}

HRESULT FrameLoadDelegate::willPerformClientRedirectToURL(_In_opt_ IWebView*, _In_ BSTR url, double /*delaySeconds*/, DATE /*fireDate*/, _In_opt_ IWebFrame* frame)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        fprintf(testResult, "%s - willPerformClientRedirectToURL: %S \n", descriptionSuitableForTestResult(frame).c_str(),
                urlSuitableForTestResult(std::wstring(url, ::SysStringLen(url))).c_str());

    return S_OK;
}

HRESULT FrameLoadDelegate::didCancelClientRedirectForFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame* frame)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        fprintf(testResult, "%s - didCancelClientRedirectForFrame\n", descriptionSuitableForTestResult(frame).c_str());

    return S_OK;
}


HRESULT FrameLoadDelegate::willCloseFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame*)
{
    return E_NOTIMPL;
}

HRESULT FrameLoadDelegate::windowScriptObjectAvailable(IWebView*, JSContextRef, JSObjectRef)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        fprintf(testResult, "?? - windowScriptObjectAvailable\n");

    ASSERT_NOT_REACHED();

    return S_OK;
}

HRESULT FrameLoadDelegate::didClearWindowObject(IWebView*, JSContextRef, JSObjectRef, IWebFrame*)
{
    return E_NOTIMPL;
}

HRESULT FrameLoadDelegate::didClearWindowObjectForFrameInScriptWorld(_In_opt_ IWebView* webView, _In_opt_ IWebFrame* frame, _In_opt_ IWebScriptWorld* world)
{
    ASSERT_ARG(webView, webView);
    ASSERT_ARG(frame, frame);
    ASSERT_ARG(world, world);
    if (!webView || !frame || !world)
        return E_POINTER;

    COMPtr<IWebScriptWorld> standardWorld;
    if (FAILED(world->standardWorld(&standardWorld)))
        return S_OK;

    if (world == standardWorld)
        didClearWindowObjectForFrameInStandardWorld(frame);
    else
        didClearWindowObjectForFrameInIsolatedWorld(frame, world);
    return S_OK;
}

void FrameLoadDelegate::didClearWindowObjectForFrameInIsolatedWorld(IWebFrame* frame, IWebScriptWorld* world)
{
    COMPtr<IWebFramePrivate> framePrivate(Query, frame);
    if (!framePrivate)
        return;

    JSGlobalContextRef ctx = framePrivate->globalContextForScriptWorld(world);
    if (!ctx)
        return;

    JSObjectRef globalObject = JSContextGetGlobalObject(ctx);
    if (!globalObject)
        return;

    JSObjectSetProperty(ctx, globalObject, JSRetainPtr<JSStringRef>(Adopt, JSStringCreateWithUTF8CString("__worldID")).get(), JSValueMakeNumber(ctx, worldIDForWorld(world)), kJSPropertyAttributeReadOnly, 0);
}

void FrameLoadDelegate::didClearWindowObjectForFrameInStandardWorld(IWebFrame* frame)
{
    JSGlobalContextRef context = frame->globalContext();
    JSObjectRef windowObject = JSContextGetGlobalObject(context);

    IWebFrame* parentFrame = 0;
    frame->parentFrame(&parentFrame);

    JSValueRef exception = 0;

    ::gTestRunner->makeWindowObject(context, windowObject, &exception);
    ASSERT(!exception);

    m_gcController->makeWindowObject(context, windowObject, &exception);
    ASSERT(!exception);

    m_accessibilityController->makeWindowObject(context, windowObject, &exception);
    ASSERT(!exception);

    m_textInputController->makeWindowObject(context, windowObject, &exception);
    ASSERT(!exception);

    JSStringRef eventSenderStr = JSStringCreateWithUTF8CString("eventSender");
    JSValueRef eventSender = makeEventSender(context, !parentFrame);
    JSObjectSetProperty(context, windowObject, eventSenderStr, eventSender, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, 0);
    JSStringRelease(eventSenderStr);

    WebCoreTestSupport::injectInternalsObject(context);
}

HRESULT FrameLoadDelegate::didFinishDocumentLoadForFrame(_In_opt_ IWebView* /*sender*/, _In_opt_ IWebFrame* frame)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        fprintf(testResult, "%s - didFinishDocumentLoadForFrame\n",
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
            fprintf(testResult, "%s - has %u onunload handler(s)\n",
                    descriptionSuitableForTestResult(frame).c_str(), pendingFrameUnloadEvents);
    }

    return S_OK;
}

HRESULT FrameLoadDelegate::didHandleOnloadEventsForFrame(_In_opt_ IWebView* /*sender*/, _In_opt_ IWebFrame* frame)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        fprintf(testResult, "%s - didHandleOnloadEventsForFrame\n",
                descriptionSuitableForTestResult(frame).c_str());

    return S_OK;
}

HRESULT FrameLoadDelegate::didFirstVisuallyNonEmptyLayoutInFrame(_In_opt_ IWebView* /*sender*/, _In_opt_ IWebFrame* /*frame*/)
{
    return S_OK;
}

HRESULT FrameLoadDelegate::didDisplayInsecureContent(_In_opt_ IWebView* /*sender*/)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        fprintf(testResult, "didDisplayInsecureContent\n");

    return S_OK;
}

HRESULT FrameLoadDelegate::didRunInsecureContent(_In_opt_ IWebView* /*sender*/, _In_opt_ IWebSecurityOrigin* /*origin*/)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        fprintf(testResult, "didRunInsecureContent\n");

    return S_OK;
}

HRESULT FrameLoadDelegate::onNotify(_In_opt_ IWebNotification* notification)
{
    _bstr_t notificationName;
    HRESULT hr = notification->name(&notificationName.GetBSTR());
    if (FAILED(hr))
        return hr;

    static _bstr_t webViewProgressFinishedNotificationName(WebViewProgressFinishedNotification);

    if (!wcscmp(notificationName, webViewProgressFinishedNotificationName))
        webViewProgressFinishedNotification();

    return S_OK;
}

void FrameLoadDelegate::webViewProgressFinishedNotification()
{
    if (!done && gTestRunner->dumpProgressFinishedCallback())
        fprintf(testResult, "postProgressFinishedNotification\n");
}

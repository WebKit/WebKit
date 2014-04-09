/*
 * Copyright (C) 2005, 2006, 2007, 2009, 2014 Apple Inc. All rights reserved.
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
#include "TestRunner.h"
#include "TextInputController.h"
#include "WebCoreTestSupport.h"
#include "WorkQueueItem.h"
#include "WorkQueue.h"
#include <WebCore/COMPtr.h>
#include <JavaScriptCore/JavaScriptCore.h>
#include <WebKit/WebKit.h>
#include <stdio.h>
#include <string>
#include <wtf/Assertions.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>

using std::string;

static FrameLoadDelegate* g_delegateWaitingOnTimer;

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
    : m_refCount(1)
    , m_gcController(std::make_unique<GCController>())
    , m_accessibilityController(std::make_unique<AccessibilityController>())
    , m_textInputController(std::make_unique<TextInputController>())
{
}

FrameLoadDelegate::~FrameLoadDelegate()
{
}

HRESULT FrameLoadDelegate::QueryInterface(REFIID riid, void** ppvObject)
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

ULONG FrameLoadDelegate::AddRef(void)
{
    return ++m_refCount;
}

ULONG FrameLoadDelegate::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}


HRESULT FrameLoadDelegate::didStartProvisionalLoadForFrame(IWebView* /*webView*/, IWebFrame* frame)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        printf("%s - didStartProvisionalLoadForFrame\n", descriptionSuitableForTestResult(frame).c_str());

    // Make sure we only set this once per test.  If it gets cleared, and then set again, we might
    // end up doing two dumps for one test.
    if (!topLoadingFrame && !done)
        topLoadingFrame = frame;

    return S_OK; 
}

HRESULT FrameLoadDelegate::didReceiveServerRedirectForProvisionalLoadForFrame(IWebView* /*webView*/, IWebFrame* frame)
{ 
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        printf("%s - didReceiveServerRedirectForProvisionalLoadForFrame\n", descriptionSuitableForTestResult(frame).c_str());

    return S_OK;
}

HRESULT FrameLoadDelegate::didFailProvisionalLoadWithError(IWebView* /*webView*/, IWebError* error, IWebFrame* frame)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        printf("%s - didFailProvisionalLoadWithError\n", descriptionSuitableForTestResult(frame).c_str());

    locationChangeDone(error, frame);
    return S_OK;
}

HRESULT FrameLoadDelegate::didCommitLoadForFrame(IWebView* webView, IWebFrame* frame)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        printf("%s - didCommitLoadForFrame\n", descriptionSuitableForTestResult(frame).c_str());

    COMPtr<IWebViewPrivate> webViewPrivate;
    HRESULT hr = webView->QueryInterface(&webViewPrivate);
    if (FAILED(hr))
        return hr;
    webViewPrivate->updateFocusedAndActiveState();

    return S_OK;
}

HRESULT FrameLoadDelegate::didReceiveTitle(IWebView* /*webView*/, BSTR title, IWebFrame* frame)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        printf("%s - didReceiveTitle: %S\n", descriptionSuitableForTestResult(frame).c_str(), title);

    if (::gTestRunner->dumpTitleChanges() && !done)
        printf("TITLE CHANGED: '%S'\n", title ? title : L"");
    return S_OK;
}

HRESULT FrameLoadDelegate::didChangeIcons(
    /* [in] */ IWebView* webView,
    /* [in] */ IWebFrame* frame)
{
    if (!done && gTestRunner->dumpIconChanges())
        printf("%s - didChangeIcons\n", descriptionSuitableForTestResult(frame).c_str());

    return S_OK;
}

void FrameLoadDelegate::processWork()
{
    // if another load started, then wait for it to complete.
    if (topLoadingFrame)
        return;

    // if we finish all the commands, we're ready to dump state
    if (WorkQueue::shared()->processWork() && !::gTestRunner->waitToDump())
        dump();
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

    topLoadingFrame = 0;
    WorkQueue::shared()->setFrozen(true);

    if (::gTestRunner->waitToDump())
        return;

    if (WorkQueue::shared()->count()) {
        if (!processWorkTimerID)
            processWorkTimerID = ::SetTimer(0, 0, 0, processWorkTimer);
        delegatesWithDelayedWork().append(this);
        return;
    }

    dump();
}

HRESULT FrameLoadDelegate::didFinishLoadForFrame(IWebView* /*webView*/, IWebFrame* frame)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        printf("%s - didFinishLoadForFrame\n", descriptionSuitableForTestResult(frame).c_str());

    locationChangeDone(0, frame);
    return S_OK;
}

HRESULT FrameLoadDelegate::didFailLoadWithError(IWebView* /*webView*/, IWebError* error, IWebFrame* frame)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        printf("%s - didFailLoadWithError\n", descriptionSuitableForTestResult(frame).c_str());

    locationChangeDone(error, frame);
    return S_OK;
}

HRESULT FrameLoadDelegate::willPerformClientRedirectToURL(IWebView* /*webView*/, BSTR url, double /*delaySeconds*/, DATE /*fireDate*/, IWebFrame* frame)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        printf("%s - willPerformClientRedirectToURL: %S \n", descriptionSuitableForTestResult(frame).c_str(),
                urlSuitableForTestResult(std::wstring(url, ::SysStringLen(url))).c_str());

    return S_OK;
}

HRESULT FrameLoadDelegate::didCancelClientRedirectForFrame(IWebView* /*webView*/, IWebFrame* frame)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        printf("%s - didCancelClientRedirectForFrame\n", descriptionSuitableForTestResult(frame).c_str());

    return S_OK;
}


HRESULT FrameLoadDelegate::willCloseFrame(IWebView* /*webView*/, IWebFrame* /*frame*/)
{
    return E_NOTIMPL;
}

HRESULT FrameLoadDelegate::didClearWindowObject(IWebView*, JSContextRef, JSObjectRef, IWebFrame*)
{
    return E_NOTIMPL;
}

HRESULT FrameLoadDelegate::didClearWindowObjectForFrameInScriptWorld(IWebView* webView, IWebFrame* frame, IWebScriptWorld* world)
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
    return;
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

HRESULT FrameLoadDelegate::didFinishDocumentLoadForFrame(IWebView* /*sender*/, IWebFrame* frame)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
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

HRESULT FrameLoadDelegate::didHandleOnloadEventsForFrame(IWebView* /*sender*/, IWebFrame* frame)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        printf("%s - didHandleOnloadEventsForFrame\n",
                descriptionSuitableForTestResult(frame).c_str());

    return S_OK;
}

HRESULT FrameLoadDelegate::didFirstVisuallyNonEmptyLayoutInFrame(IWebView* /*sender*/, IWebFrame* /*frame*/)
{
    return S_OK;
}

HRESULT FrameLoadDelegate::didDisplayInsecureContent(IWebView* /*sender*/)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        printf("didDisplayInsecureContent\n");

    return S_OK;
}

HRESULT FrameLoadDelegate::didRunInsecureContent(IWebView* /*sender*/, IWebSecurityOrigin* /*origin*/)
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        printf("didRunInsecureContent\n");

    return S_OK;
}

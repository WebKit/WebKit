/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "WebKit.h"
#include "WebKitDLL.h"
#include "WebScriptDebugServer.h"

#include "WebScriptCallFrame.h"
#include "WebView.h"
#pragma warning(push, 0)
#include <WebCore/DOMWindow.h>
#include <WebCore/JSDOMWindow.h>
#include <WebCore/JavaScriptDebugServer.h>
#pragma warning(pop)
#include <kjs/ExecState.h>
#include <wtf/Assertions.h>
#include <wtf/Vector.h>

using namespace KJS;
using namespace WebCore;

typedef HashSet<COMPtr<IWebScriptDebugListener> > ListenerSet;

static ListenerSet s_Listeners;
static unsigned s_ListenerCount = 0;
static bool s_dying = false;

static Frame* frame(ExecState* exec)
{
    JSDOMWindow* window = static_cast<JSDOMWindow*>(exec->dynamicGlobalObject());
    return window->impl()->frame();
}

static WebFrame* webFrame(ExecState* exec)
{
    return kit(frame(exec));
}

static WebView* webView(ExecState* exec)
{
    return kit(frame(exec)->page());
}

unsigned WebScriptDebugServer::listenerCount() { return s_ListenerCount; };

// WebScriptDebugServer ------------------------------------------------------------

WebScriptDebugServer::WebScriptDebugServer()
    : m_refCount(0)
    , m_paused(false)
    , m_step(false)
{
    gClassCount++;
}

WebScriptDebugServer::~WebScriptDebugServer()
{
    gClassCount--;
}

WebScriptDebugServer* WebScriptDebugServer::createInstance()
{
    WebScriptDebugServer* instance = new WebScriptDebugServer;
    instance->AddRef();
    return instance;
}

WebScriptDebugServer* WebScriptDebugServer::sharedWebScriptDebugServer()
{
    static WebScriptDebugServer* server;

    if (!server) {
        s_dying = false;
        server = WebScriptDebugServer::createInstance();
    }

    return server;
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebScriptDebugServer::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<WebScriptDebugServer*>(this);
    else if (IsEqualGUID(riid, IID_IWebScriptDebugServer))
        *ppvObject = static_cast<WebScriptDebugServer*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebScriptDebugServer::AddRef()
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebScriptDebugServer::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebScriptDebugServer -----------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebScriptDebugServer::sharedWebScriptDebugServer( 
    /* [retval][out] */ IWebScriptDebugServer** server)
{
    if (!server)
        return E_POINTER;

    *server = sharedWebScriptDebugServer();
    (*server)->AddRef();

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebScriptDebugServer::addListener(
    /* [in] */ IWebScriptDebugListener* listener)
{
    if (s_dying)
        return E_FAIL;

    if (!listener)
        return E_POINTER;

    if (!s_ListenerCount)
        JavaScriptDebugServer::shared().addListener(this);

    ++s_ListenerCount;
    s_Listeners.add(listener);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebScriptDebugServer::removeListener(
    /* [in] */ IWebScriptDebugListener* listener)
{
    if (s_dying)
        return S_OK;

    if (!listener)
        return E_POINTER;

    if (!s_Listeners.contains(listener))
        return S_OK;

    s_Listeners.remove(listener);

    ASSERT(s_ListenerCount >= 1);
    if (--s_ListenerCount == 0) {
        JavaScriptDebugServer::shared().removeListener(this);
        resume();
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebScriptDebugServer::step()
{
    m_step = true;
    m_paused = false;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebScriptDebugServer::pause()
{
    m_paused = true;
    m_step = false;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebScriptDebugServer::resume()
{
    m_paused = false;
    m_step = false;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebScriptDebugServer::isPaused(
    /* [out, retval] */ BOOL* isPaused)
{
    if (!isPaused)
        return E_POINTER;

    *isPaused = m_paused;
    return S_OK;
}

static HWND comMessageWindow()
{
    static bool initialized;
    static HWND window;

    if (initialized)
        return window;
    initialized = true;

    static LPCTSTR windowClass = TEXT("OleMainThreadWndClass");
    static LPCTSTR windowText = TEXT("OleMainThreadWndName");
    static const DWORD currentProcess = GetCurrentProcessId();

    window = 0;
    DWORD windowProcess = 0;
    do {
        window = FindWindowEx(HWND_MESSAGE, window, windowClass, windowText);
        GetWindowThreadProcessId(window, &windowProcess);
    } while (window && windowProcess != currentProcess);

    return window;
}

void WebScriptDebugServer::suspendProcessIfPaused()
{
    static bool alreadyHere = false;

    if (alreadyHere)
        return;

    alreadyHere = true;

    // We only deliver messages to COM's message window to pause the process while still allowing RPC to work.
    // FIXME: It would be nice if we could keep delivering WM_[NC]PAINT messages to all windows to keep them painting on XP.

    HWND messageWindow = comMessageWindow();

    MSG msg;
    while (m_paused && GetMessage(&msg, messageWindow, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (m_step) {
        m_step = false;
        m_paused = true;
    }

    alreadyHere = false;
}

void WebScriptDebugServer::didLoadMainResourceForDataSource(IWebView* webView, IWebDataSource* dataSource)
{
    if (!webView || !dataSource)
        return;

    ListenerSet listenersCopy = s_Listeners;
    ListenerSet::iterator end = listenersCopy.end();
    for (ListenerSet::iterator it = listenersCopy.begin(); it != end; ++it)
        (**it).didLoadMainResourceForDataSource(webView, dataSource);
}

void WebScriptDebugServer::didParseSource(ExecState* exec, const UString& source, int startingLineNumber, const UString& sourceURL, int sourceID)
{
    BString bSource = source;
    BString bSourceURL = sourceURL;

    ListenerSet listenersCopy = s_Listeners;
    ListenerSet::iterator end = listenersCopy.end();
    for (ListenerSet::iterator it = listenersCopy.begin(); it != end; ++it)
        (**it).didParseSource(webView(exec), bSource, startingLineNumber, bSourceURL, sourceID, webFrame(exec));
}

void WebScriptDebugServer::failedToParseSource(ExecState* exec, const UString& source, int startingLineNumber, const UString& sourceURL, int errorLine, const UString& errorMessage)
{
    BString bSource = source;
    BString bSourceURL = sourceURL;

    // FIXME: the error var should be made with the information in the errorMsg.  It is not a simple
    // UString to BSTR conversion there is some logic involved that I don't fully understand yet.
    BString error(L"An Error Occurred.");

    ListenerSet listenersCopy = s_Listeners;
    ListenerSet::iterator end = listenersCopy.end();
    for (ListenerSet::iterator it = listenersCopy.begin(); it != end; ++it)
        (**it).failedToParseSource(webView(exec), bSource, startingLineNumber, bSourceURL, error, webFrame(exec));
}

void WebScriptDebugServer::didEnterCallFrame(ExecState* exec, int sourceID, int lineNumber)
{
    COMPtr<WebScriptCallFrame> callFrame(AdoptCOM, WebScriptCallFrame::createInstance(exec));
    ListenerSet listenersCopy = s_Listeners;
    ListenerSet::iterator end = listenersCopy.end();
    for (ListenerSet::iterator it = listenersCopy.begin(); it != end; ++it)
        (**it).didEnterCallFrame(webView(exec), callFrame.get(), sourceID, lineNumber, webFrame(exec));

    suspendProcessIfPaused();
}

void WebScriptDebugServer::willExecuteStatement(ExecState* exec, int sourceID, int lineNumber)
{
    COMPtr<WebScriptCallFrame> callFrame(AdoptCOM, WebScriptCallFrame::createInstance(exec));
    ListenerSet listenersCopy = s_Listeners;
    ListenerSet::iterator end = listenersCopy.end();
    for (ListenerSet::iterator it = listenersCopy.begin(); it != end; ++it)
        (**it).willExecuteStatement(webView(exec), callFrame.get(), sourceID, lineNumber, webFrame(exec));

    suspendProcessIfPaused();
}

void WebScriptDebugServer::willLeaveCallFrame(ExecState* exec, int sourceID, int lineNumber)
{
    COMPtr<WebScriptCallFrame> callFrame(AdoptCOM, WebScriptCallFrame::createInstance(exec->callingExecState()));
    ListenerSet listenersCopy = s_Listeners;
    ListenerSet::iterator end = listenersCopy.end();
    for (ListenerSet::iterator it = listenersCopy.begin(); it != end; ++it)
        (**it).willLeaveCallFrame(webView(exec), callFrame.get(), sourceID, lineNumber, webFrame(exec));

    suspendProcessIfPaused();
}

void WebScriptDebugServer::exceptionWasRaised(ExecState* exec, int sourceID, int lineNumber)
{
    COMPtr<WebScriptCallFrame> callFrame(AdoptCOM, WebScriptCallFrame::createInstance(exec));
    ListenerSet listenersCopy = s_Listeners;
    ListenerSet::iterator end = listenersCopy.end();
    for (ListenerSet::iterator it = listenersCopy.begin(); it != end; ++it)
        (**it).exceptionWasRaised(webView(exec), callFrame.get(), sourceID, lineNumber, webFrame(exec));

    suspendProcessIfPaused();
}

void WebScriptDebugServer::serverDidDie()
{
    s_dying = true;

    ListenerSet listenersCopy = s_Listeners;
    ListenerSet::iterator end = listenersCopy.end();
    for (ListenerSet::iterator it = listenersCopy.begin(); it != end; ++it) {
        (**it).serverDidDie();
        s_Listeners.remove((*it).get());
    }
}

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
#include "IWebScriptDebugListener.h"
#include "WebKitDLL.h"
#include "WebScriptDebugServer.h"

#include "WebView.h"
#include <wtf/Assertions.h>
#include <wtf/Vector.h>

static Vector<IWebView*> sViews;
static WebScriptDebugServer* sSharedWebScriptDebugServer;
static unsigned sListenerCount = 0;

unsigned WebScriptDebugServer::listenerCount() { return sListenerCount; };

// EnumViews ------------------------------------------------------------------

class EnumViews : public IEnumVARIANT
{
public:
    EnumViews() : m_refCount(1), m_current(sViews.begin()) { }

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    virtual HRESULT STDMETHODCALLTYPE Next(ULONG celt, VARIANT *rgVar, ULONG *pCeltFetched);
    virtual HRESULT STDMETHODCALLTYPE Skip(ULONG celt);
    virtual HRESULT STDMETHODCALLTYPE Reset(void);
    virtual HRESULT STDMETHODCALLTYPE Clone(IEnumVARIANT**);

private:
    ULONG m_refCount;
    Vector<IWebView*>::iterator m_current;
};

HRESULT STDMETHODCALLTYPE EnumViews::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown) || IsEqualGUID(riid, IID_IEnumVARIANT))
        *ppvObject = this;
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE EnumViews::AddRef()
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE EnumViews::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);
    return newRef;
}

HRESULT STDMETHODCALLTYPE EnumViews::Next(ULONG celt, VARIANT *rgVar, ULONG *pCeltFetched)
{
    if (pCeltFetched)
        *pCeltFetched = 0;
    if (!rgVar)
        return E_POINTER;
    VariantInit(rgVar);
    if (!celt || celt > 1)
        return S_FALSE;
    if (m_current == sViews.end())
        return S_FALSE;

    IUnknown* unknown;
    HRESULT hr = (*m_current++)->QueryInterface(IID_IUnknown, (void**)&unknown);
    if (FAILED(hr))
        return hr;

    V_VT(rgVar) = VT_UNKNOWN;
    V_UNKNOWN(rgVar) = unknown;

    if (pCeltFetched)
        *pCeltFetched = 1;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE EnumViews::Skip(ULONG celt)
{
    m_current += celt;
    return (m_current != sViews.end()) ? S_OK : S_FALSE;
}

HRESULT STDMETHODCALLTYPE EnumViews::Reset(void)
{
    m_current = sViews.begin();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE EnumViews::Clone(IEnumVARIANT**)
{
    return E_NOTIMPL;
}

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
    if (!sSharedWebScriptDebugServer)
        sSharedWebScriptDebugServer = WebScriptDebugServer::createInstance();

    return sSharedWebScriptDebugServer;
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

void WebScriptDebugServer::viewAdded(IWebView* view)
{
    sViews.append(view);
}

void WebScriptDebugServer::viewRemoved(IWebView* view)
{
    Vector<IWebView*>::iterator end = sViews.end();
    int i=0;
    for (Vector<IWebView*>::iterator it = sViews.begin(); it != end; ++it, ++i) {
        if (*it == view) {
            sViews.remove(i);
            break;
        }
    }
}

// IWebScriptDebugServer -----------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebScriptDebugServer::sharedWebScriptDebugServer( 
    /* [retval][out] */ IWebScriptDebugServer** server)
{
    if (!server)
        return E_POINTER;

    *server = WebScriptDebugServer::sharedWebScriptDebugServer();
    (*server)->AddRef();

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebScriptDebugServer::addListener(
    /* [in] */ IWebScriptDebugListener* listener)
{
    if (!listener)
        return E_POINTER;

    sListenerCount++;
    m_listeners.add(listener);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebScriptDebugServer::removeListener(
    /* [in] */ IWebScriptDebugListener* listener)
{
    if (!listener)
        return E_POINTER;

    if (!m_listeners.contains(listener))
        return S_OK;

    ASSERT(sListenerCount >= 1);
    sListenerCount--;
    m_listeners.remove(listener);

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


void WebScriptDebugServer::suspendProcessIfPaused()
{
    // FIXME: There needs to be some sort of busy wait here.

    if (m_step) {
        m_step = false;
        m_paused = true;
    }
}

// IWebScriptDebugListener
HRESULT STDMETHODCALLTYPE WebScriptDebugServer::didLoadMainResourceForDataSource(
    /* [in] */ IWebView* webView,
    /* [in] */ IWebDataSource* dataSource)
{
    if (!webView || !dataSource)
        return E_FAIL;

    ListenerSet listenersCopy = m_listeners;
    for (ListenerSet::iterator it = listenersCopy.begin(); it != listenersCopy.end(); ++it)
        (**it).didLoadMainResourceForDataSource(webView, dataSource);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebScriptDebugServer::didParseSource(
    /* [in] */ IWebView* webView,
    /* [in] */ BSTR sourceCode,
    /* [in] */ UINT baseLineNumber,
    /* [in] */ BSTR url,
    /* [in] */ int sourceID,
    /* [in] */ IWebFrame* webFrame)
{
    if (!webView || !sourceCode || !url || !webFrame)
        return E_FAIL;

    ListenerSet listenersCopy = m_listeners;
    for (ListenerSet::iterator it = listenersCopy.begin(); it != listenersCopy.end(); ++it)
        (**it).didParseSource(webView, sourceCode, baseLineNumber, url, sourceID, webFrame);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebScriptDebugServer::failedToParseSource(
    /* [in] */ IWebView* webView,
    /* [in] */ BSTR sourceCode,
    /* [in] */ UINT baseLineNumber,
    /* [in] */ BSTR url,
    /* [in] */ BSTR error,
    /* [in] */ IWebFrame* webFrame)
{
    if (!webView || !sourceCode || !url || !error || !webFrame)
        return E_FAIL;

    ListenerSet listenersCopy = m_listeners;
    for (ListenerSet::iterator it = listenersCopy.begin(); it != listenersCopy.end(); ++it)
        (**it).failedToParseSource(webView, sourceCode, baseLineNumber, url, error, webFrame);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebScriptDebugServer::didEnterCallFrame(
    /* [in] */ IWebView* webView,
    /* [in] */ IWebScriptCallFrame* frame,
    /* [in] */ int sourceID,
    /* [in] */ int lineNumber,
    /* [in] */ IWebFrame* webFrame)
{
    if (!webView || !frame || !webFrame)
        return E_FAIL;

    ListenerSet listenersCopy = m_listeners;
    for (ListenerSet::iterator it = listenersCopy.begin(); it != listenersCopy.end(); ++it)
        (**it).didEnterCallFrame(webView, frame, sourceID, lineNumber, webFrame);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebScriptDebugServer::willExecuteStatement(
    /* [in] */ IWebView* webView,
    /* [in] */ IWebScriptCallFrame* frame,
    /* [in] */ int sourceID,
    /* [in] */ int lineNumber,
    /* [in] */ IWebFrame* webFrame)
{
    if (!webView || !frame || !webFrame)
        return E_FAIL;

    ListenerSet listenersCopy = m_listeners;
    for (ListenerSet::iterator it = listenersCopy.begin(); it != listenersCopy.end(); ++it)
        (**it).willExecuteStatement(webView, frame, sourceID, lineNumber, webFrame);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebScriptDebugServer::willLeaveCallFrame(
    /* [in] */ IWebView* webView,
    /* [in] */ IWebScriptCallFrame* frame,
    /* [in] */ int sourceID,
    /* [in] */ int lineNumber,
    /* [in] */ IWebFrame* webFrame)
{
    if (!webView || !frame || !webFrame)
        return E_FAIL;

    ListenerSet listenersCopy = m_listeners;
    for (ListenerSet::iterator it = listenersCopy.begin(); it != listenersCopy.end(); ++it)
        (**it).willLeaveCallFrame(webView, frame, sourceID, lineNumber, webFrame);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebScriptDebugServer::exceptionWasRaised(
    /* [in] */ IWebView* webView,
    /* [in] */ IWebScriptCallFrame* frame,
    /* [in] */ int sourceID,
    /* [in] */ int lineNumber,
    /* [in] */ IWebFrame* webFrame)
{
    if (!webView || !frame || !webFrame)
        return E_FAIL;

    ListenerSet listenersCopy = m_listeners;
    for (ListenerSet::iterator it = listenersCopy.begin(); it != listenersCopy.end(); ++it)
        (**it).exceptionWasRaised(webView, frame, sourceID, lineNumber, webFrame);

    return S_OK;
}



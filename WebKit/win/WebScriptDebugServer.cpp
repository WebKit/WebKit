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
#include "WebKitDLL.h"
#include "WebScriptDebugServer.h"

#include "WebView.h"
#include <wtf/Assertions.h>
#include <wtf/Vector.h>

static Vector<IWebView*> sViews;

// EnumViews ------------------------------------------------------------------

class EnumViews : public IEnumVARIANT
{
public:
    EnumViews() : m_refCount(1), m_current(sViews.begin()) { }

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject)
    {
        *ppvObject = 0;
        if (IsEqualGUID(riid, IID_IUnknown) || IsEqualGUID(riid, IID_IEnumVARIANT))
            *ppvObject = this;
        else
            return E_NOINTERFACE;

        AddRef();
        return S_OK;
    }

    virtual ULONG STDMETHODCALLTYPE AddRef(void)
    {
        return ++m_refCount;
    }

    virtual ULONG STDMETHODCALLTYPE Release(void)
    {
        ULONG newRef = --m_refCount;
        if (!newRef)
            delete(this);
        return newRef;
    }

    virtual HRESULT STDMETHODCALLTYPE Next(ULONG celt, VARIANT *rgVar, ULONG *pCeltFetched)
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

    virtual HRESULT STDMETHODCALLTYPE Skip(ULONG celt)
    {
        m_current += celt;
        return (m_current != sViews.end()) ? S_OK : S_FALSE;
    }

    virtual HRESULT STDMETHODCALLTYPE Reset(void)
    {
        m_current = sViews.begin();
        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE Clone(IEnumVARIANT**)
    {
        return E_NOTIMPL;
    }

private:
    ULONG m_refCount;
    Vector<IWebView*>::iterator m_current;
};

// WebScriptDebugServer ------------------------------------------------------------

WebScriptDebugServer::WebScriptDebugServer()
: m_refCount(0)
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

ULONG STDMETHODCALLTYPE WebScriptDebugServer::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebScriptDebugServer::Release(void)
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

HRESULT STDMETHODCALLTYPE WebScriptDebugServer::addListener(
    /* [in] */ const IWebScriptDebugListener*)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebScriptDebugServer::removeListener(
    /* [in] */ const IWebScriptDebugListener*)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebScriptDebugServer::step()
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebScriptDebugServer::pause()
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebScriptDebugServer::resume()
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebScriptDebugServer::isPaused(
    /* [out, retval] */ BOOL*)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

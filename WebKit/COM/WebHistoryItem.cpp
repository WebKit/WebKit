/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "WebKitDLL.h"
#include "WebHistoryItem.h"

// WebHistoryItem ----------------------------------------------------------------

WebHistoryItem::WebHistoryItem()
: m_refCount(0)
, m_url(0)
{
    gClassCount++;
}

WebHistoryItem::~WebHistoryItem()
{
    SysFreeString(m_url);
    gClassCount--;
}

WebHistoryItem* WebHistoryItem::createInstance()
{
    WebHistoryItem* instance = new WebHistoryItem();
    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebHistoryItem::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebHistoryItem*>(this);
    else if (IsEqualGUID(riid, IID_IWebHistoryItem))
        *ppvObject = static_cast<IWebHistoryItem*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebHistoryItem::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebHistoryItem::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebHistoryItem -------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebHistoryItem::initWithURLString( 
    /* [in] */ BSTR title,
    /* [in] */ UINT /*time*/)
{
    m_url = SysAllocString(title);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::originalURLString( 
    /* [retval][out] */ BSTR* /*url*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::URLString( 
    /* [retval][out] */ BSTR* url)
{
    *url = SysAllocString(m_url);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::title( 
    /* [retval][out] */ BSTR* /*pageTitle*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::lastVisitedTimeInterval( 
    /* [retval][out] */ UINT* /*time*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::setAlternateTitle( 
    /* [retval][out] */ BSTR* /*title*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::alternateTitle( 
    /* [in] */ BSTR /*title*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebHistoryItem::icon( 
    /* [in] */ IWebImage* /*image*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

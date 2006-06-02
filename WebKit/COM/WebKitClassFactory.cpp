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
#include "WebKitClassFactory.h"
#include "WebMutableURLRequest.h"
#include "IWebURLResponse.h"
#include "WebKit.h"
#include "WebFrame.h"
#include "WebView.h"
#include "WebIconDatabase.h"

// WebKitClassFactory ---------------------------------------------------------

WebKitClassFactory::WebKitClassFactory(CLSID targetClass)
: m_targetClass(targetClass)
, m_refCount(0)
{
    gClassCount++;
}

WebKitClassFactory::~WebKitClassFactory()
{
    gClassCount--;
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebKitClassFactory::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IUnknown*>(this);
    else if (IsEqualGUID(riid, IID_IClassFactory))
        *ppvObject = static_cast<IClassFactory*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebKitClassFactory::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebKitClassFactory::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef && !gLockCount)
        delete(this);

    return newRef;
}

// IClassFactory --------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebKitClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject)
{
    IUnknown* unknown = 0;
    *ppvObject = 0;
    
    if (pUnkOuter)
        return CLASS_E_NOAGGREGATION;

    if (IsEqualGUID(m_targetClass, CLSID_WebView))
        unknown = static_cast<IWebView*>(WebView::createInstance());
    else if (IsEqualGUID(m_targetClass, CLSID_WebIconDatabase))
        unknown = static_cast<IWebIconDatabase*>(WebIconDatabase::createInstance());
    else if (IsEqualGUID(m_targetClass, CLSID_WebMutableURLRequest))
        unknown = static_cast<IWebMutableURLRequest*>(WebMutableURLRequest::createInstance());
    else
        return CLASS_E_CLASSNOTAVAILABLE;

    if (!unknown)
        return E_OUTOFMEMORY;

    HRESULT hr = unknown->QueryInterface(riid, ppvObject);
    if (FAILED(hr))
        delete unknown;
    else
        unknown->Release(); // both createInstance() and QueryInterface() added refs

    return hr;
}

HRESULT STDMETHODCALLTYPE WebKitClassFactory::LockServer(BOOL fLock)
{
    if (fLock)
        gLockCount++;
    else
        gLockCount--;

    return S_OK;
}

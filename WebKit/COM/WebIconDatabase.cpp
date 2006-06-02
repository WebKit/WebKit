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
#include "WebIconDatabase.h"

// WebIconDatabase ----------------------------------------------------------------

WebIconDatabase::WebIconDatabase()
: m_refCount(0)
{
    gClassCount++;
}

WebIconDatabase::~WebIconDatabase()
{
    gClassCount--;
}

WebIconDatabase* WebIconDatabase::createInstance()
{
    WebIconDatabase* instance = new WebIconDatabase();
    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebIconDatabase::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebIconDatabase*>(this);
    else if (IsEqualGUID(riid, IID_IWebIconDatabase))
        *ppvObject = static_cast<IWebIconDatabase*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebIconDatabase::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebIconDatabase::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebIconDatabase --------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebIconDatabase::sharedIconDatabase( 
        /* [retval][out] */ IWebIconDatabase* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebIconDatabase::iconForURL( 
        /* [in] */ BSTR /*url*/,
        /* [optional][in] */ LPSIZE /*size*/,
        /* [optional][in] */ BOOL /*cache*/,
        /* [retval][out] */ IWebImage** /*image*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebIconDatabase::defaultIconWithSize( 
        /* [in] */ LPSIZE /*size*/,
        /* [retval][out] */ IWebImage** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebIconDatabase::retainIconForURL( 
        /* [in] */ BSTR /*url*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebIconDatabase::releaseIconForURL( 
        /* [in] */ BSTR /*url*/)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebIconDatabase::delayDatabaseCleanup(void)
{
    DebugBreak();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE WebIconDatabase::allowDatabaseCleanup(void)
{
    DebugBreak();
    return E_NOTIMPL;
}

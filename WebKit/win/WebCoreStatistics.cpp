/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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
#include "WebCoreStatistics.h"

#include <WebCore/IconDatabase.h>

#pragma warning(push, 0)
#include <JavaScriptCore/collector.h>
#pragma warning(pop)

using namespace KJS;
using namespace WebCore;

// WebCoreStatistics ---------------------------------------------------------------------------

WebCoreStatistics::WebCoreStatistics()
: m_refCount(0)
{
    gClassCount++;
}

WebCoreStatistics::~WebCoreStatistics()
{
    gClassCount--;
}

WebCoreStatistics* WebCoreStatistics::createInstance()
{
    WebCoreStatistics* instance = new WebCoreStatistics();
    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebCoreStatistics::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<WebCoreStatistics*>(this);
    else if (IsEqualGUID(riid, IID_IWebCoreStatistics))
        *ppvObject = static_cast<WebCoreStatistics*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebCoreStatistics::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebCoreStatistics::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebCoreStatistics ------------------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebCoreStatistics::javaScriptObjectsCount( 
    /* [retval][out] */ UINT* count)
{
    if (!count)
        return E_POINTER;

    JSLock lock;
    *count = (UINT) Collector::size();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebCoreStatistics::javaScriptGlobalObjectsCount( 
    /* [retval][out] */ UINT* count)
{
    if (!count)
        return E_POINTER;

    JSLock lock;
    *count = (UINT) Collector::globalObjectCount();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebCoreStatistics::javaScriptProtectedObjectsCount( 
    /* [retval][out] */ UINT* count)
{
    if (!count)
        return E_POINTER;

    JSLock lock;
    *count = (UINT) Collector::protectedObjectCount();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebCoreStatistics::javaScriptProtectedGlobalObjectsCount( 
    /* [retval][out] */ UINT* count)
{
    if (!count)
        return E_POINTER;

    JSLock lock;
    *count = (UINT) Collector::protectedGlobalObjectCount();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebCoreStatistics::iconPageURLMappingCount( 
    /* [retval][out] */ UINT* count)
{
    if (!count)
        return E_POINTER;
    *count = (UINT) iconDatabase()->pageURLMappingCount();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebCoreStatistics::iconRetainedPageURLCount( 
    /* [retval][out] */ UINT *count)
{
    if (!count)
        return E_POINTER;
    *count = (UINT) iconDatabase()->retainedPageURLCount();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebCoreStatistics::iconRecordCount( 
    /* [retval][out] */ UINT *count)
{
    if (!count)
        return E_POINTER;
    *count = (UINT) iconDatabase()->iconRecordCount();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebCoreStatistics::iconsWithDataCount( 
    /* [retval][out] */ UINT *count)
{
    if (!count)
        return E_POINTER;
    *count = (UINT) iconDatabase()->iconRecordCountWithData();
    return S_OK;
}

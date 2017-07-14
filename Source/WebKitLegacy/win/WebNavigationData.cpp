/*
 * Copyright (C) 2009, 2015 Apple Inc.  All rights reserved.
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

#include "WebKitDLL.h"
#include "WebNavigationData.h"

using namespace WebCore;

// IUnknown -------------------------------------------------------------------

HRESULT WebNavigationData::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebNavigationData*>(this);
    else if (IsEqualGUID(riid, IID_IWebNavigationData))
        *ppvObject = static_cast<IWebNavigationData*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebNavigationData::AddRef()
{
    return ++m_refCount;
}

ULONG WebNavigationData::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// WebNavigationData -------------------------------------------------------------------

WebNavigationData::WebNavigationData(const String& url, const String& title, IWebURLRequest* request, IWebURLResponse* response, bool hasSubstituteData, const String& clientRedirectSource)
    : m_url(url)
    , m_title(title)
    , m_request(request)
    , m_response(response)
    , m_hasSubstituteData(hasSubstituteData)
    , m_clientRedirectSource(clientRedirectSource)

{
    gClassCount++;
    gClassNameCount().add("WebNavigationData");
}

WebNavigationData::~WebNavigationData()
{
    gClassCount--;
    gClassNameCount().remove("WebNavigationData");
}

WebNavigationData* WebNavigationData::createInstance(const String& url, const String& title, IWebURLRequest* request, IWebURLResponse* response, bool hasSubstituteData, const String& clientRedirectSource)
{
    WebNavigationData* instance = new WebNavigationData(url, title, request, response, hasSubstituteData, clientRedirectSource);
    instance->AddRef();
    return instance;
}

// IWebNavigationData -------------------------------------------------------------------

HRESULT WebNavigationData::url(__deref_opt_out BSTR* url)
{
    if (!url)
        return E_POINTER;
    *url = BString(m_url).release();
    return S_OK;
}

HRESULT WebNavigationData::title(__deref_opt_out BSTR* title)
{
    if (!title)
        return E_POINTER;
    *title = BString(m_title).release();
    return S_OK;
}

HRESULT WebNavigationData::originalRequest(_COM_Outptr_opt_ IWebURLRequest** request)
{
    if (!request)
        return E_POINTER;
    *request = m_request.get();
    m_request->AddRef();
    return S_OK;
}

HRESULT WebNavigationData::response(_COM_Outptr_opt_ IWebURLResponse** response)
{
    if (!response)
        return E_POINTER;
    *response = m_response.get();
    m_response->AddRef();
    return S_OK;
}

HRESULT WebNavigationData::hasSubstituteData(_Out_ BOOL* hasSubstituteData)
{
    if (!hasSubstituteData)
        return E_POINTER;
    *hasSubstituteData = m_hasSubstituteData;
    return S_OK;
}

HRESULT WebNavigationData::clientRedirectSource(__deref_opt_out BSTR* clientRedirectSource)
{
    if (!clientRedirectSource)
        return E_POINTER;

    *clientRedirectSource = BString(m_clientRedirectSource).release();
    return S_OK;
}

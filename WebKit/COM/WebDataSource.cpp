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

#include "config.h"
#include "WebKitDLL.h"
#include "WebDataSource.h"
#include "IWebMutableURLRequest.h"
#include "WebFrame.h"

#pragma warning( push, 0 )
#include "KURL.h"
#pragma warning(pop)

// WebDataSource ----------------------------------------------------------------

WebDataSource::WebDataSource()
: m_refCount(0)
, m_request(0)
, m_initialRequest(0)
, m_frame(0)
{
    gClassCount++;
}

WebDataSource::~WebDataSource()
{
    if (m_request) {
        m_request->Release();
        m_request = 0;
    }

    if (m_initialRequest)
        m_initialRequest->Release();

    gClassCount--;
}

WebDataSource* WebDataSource::createInstance(WebFrame* frame)
{
    WebDataSource* instance = new WebDataSource();
    instance->m_frame = frame;
    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebDataSource::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebDataSource*>(this);
    else if (IsEqualGUID(riid, IID_IWebDataSource))
        *ppvObject = static_cast<IWebDataSource*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebDataSource::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebDataSource::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebDataSource --------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebDataSource::initWithRequest( 
    /* [in] */ IWebURLRequest* request)
{
    HRESULT hr = S_OK;

    if (m_request)
        return E_FAIL;

    request->AddRef();
    m_request = static_cast<IWebMutableURLRequest*>(request);

    hr = m_frame->loadDataSource(this);

    if (FAILED(hr)) {
        request->Release();
        m_request = 0;
    }

    return hr;
}

HRESULT STDMETHODCALLTYPE WebDataSource::data( 
    /* [retval][out] */ IStream** /*stream*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebDataSource::representation( 
    /* [retval][out] */ IWebDocumentRepresentation** /*rep*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebDataSource::webFrame( 
    /* [retval][out] */ IWebFrame** frame)
{
    *frame = m_frame;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebDataSource::initialRequest( 
    /* [retval][out] */ IWebURLRequest** request)
{
    if (m_initialRequest)
        m_initialRequest->AddRef();
    *request = m_initialRequest;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebDataSource::request( 
    /* [retval][out] */ IWebMutableURLRequest** request)
{
    if (m_request)
        m_request->AddRef();
    *request = m_request;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebDataSource::response( 
    /* [retval][out] */ IWebURLResponse** /*response*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebDataSource::textEncodingName( 
    /* [retval][out] */ BSTR* /*name*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebDataSource::isLoading( 
    /* [retval][out] */ BOOL* /*loading*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebDataSource::pageTitle( 
    /* [retval][out] */ BSTR* /*title*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebDataSource::unreachableURL( 
    /* [retval][out] */ BSTR* /*url*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebDataSource::webArchive( 
    /* [retval][out] */ IWebArchive* /*archive*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebDataSource::mainResource( 
    /* [retval][out] */ IWebResource* /*resource*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebDataSource::subresources( 
    /* [out] */ int* /*resourceCount*/,
    /* [retval][out] */ IWebResource*** /*resources*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebDataSource::subresourceForURL( 
    /* [in] */ BSTR /*url*/,
    /* [retval][out] */ IWebResource* /*resource*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebDataSource::addSubresource( 
    /* [in] */ IWebResource* /*subresource*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

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
#include "WebMutableURLRequest.h"

// IWebURLRequest ----------------------------------------------------------------

WebMutableURLRequest::WebMutableURLRequest()
: m_refCount(0)
, m_url(0)
, m_cachePolicy(WebURLRequestUseProtocolCachePolicy)
, m_timeoutInterval(0)
, m_method(0)
, m_submitFormData(0)
{
    gClassCount++;
}

WebMutableURLRequest::~WebMutableURLRequest()
{
    SysFreeString(m_url);
    SysFreeString(m_method);
    gClassCount--;
}

WebMutableURLRequest* WebMutableURLRequest::createInstance()
{
    WebMutableURLRequest* instance = new WebMutableURLRequest();
    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebMutableURLRequest::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebURLRequest*>(this);
    else if (IsEqualGUID(riid, IID_IWebMutableURLRequest))
        *ppvObject = static_cast<IWebMutableURLRequest*>(this);
    else if (IsEqualGUID(riid, IID_IWebURLRequest))
        *ppvObject = static_cast<IWebURLRequest*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebMutableURLRequest::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebMutableURLRequest::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebURLRequest --------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebMutableURLRequest::requestWithURL( 
    /* [in] */ BSTR /*theURL*/,
    /* [optional][in] */ WebURLRequestCachePolicy /*cachePolicy*/,
    /* [optional][in] */ UINT /*timeoutInterval*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebMutableURLRequest::allHTTPHeaderFields( 
    /* [retval][out] */ IPropertyBag** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebMutableURLRequest::cachePolicy( 
    /* [retval][out] */ WebURLRequestCachePolicy* result)
{
    *result = m_cachePolicy;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebMutableURLRequest::HTTPBody( 
    /* [retval][out] */ IStream** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebMutableURLRequest::HTTPBodyStream( 
    /* [retval][out] */ IStream** /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebMutableURLRequest::HTTPMethod( 
    /* [retval][out] */ BSTR* result)
{
    *result = SysAllocString(m_method ? m_method : TEXT("GET"));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebMutableURLRequest::HTTPShouldHandleCookies( 
    /* [retval][out] */ BOOL* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebMutableURLRequest::initWithURL( 
    /* [in] */ BSTR url,
    /* [optional][in] */ WebURLRequestCachePolicy cachePolicy,
    /* [optional][in] */ UINT timeoutInterval)
{
    m_url = SysAllocString(url);
    m_cachePolicy = cachePolicy;
    m_timeoutInterval = timeoutInterval;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebMutableURLRequest::mainDocumentURL( 
    /* [retval][out] */ BSTR* result)
{
    *result = SysAllocString(m_url);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebMutableURLRequest::timeoutInterval( 
    /* [retval][out] */ UINT* result)
{
    *result = m_timeoutInterval;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebMutableURLRequest::URL( 
    /* [retval][out] */ BSTR* result)
{
    *result = 0;
    if (!m_url)
        return E_FAIL;

    *result = SysAllocString(m_url);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebMutableURLRequest::valueForHTTPHeaderField( 
    /* [in] */ BSTR /*field*/,
    /* [retval][out] */ BSTR* /*result*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

// IWebMutableURLRequest --------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebMutableURLRequest::addValue( 
    /* [in] */ BSTR /*value*/,
    /* [in] */ BSTR /*field*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebMutableURLRequest::setAllHTTPHeaderFields( 
    /* [in] */ IPropertyBag* /*headerFields*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebMutableURLRequest::setCachePolicy( 
    /* [in] */ WebURLRequestCachePolicy /*policy*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebMutableURLRequest::setHTTPBody( 
    /* [in] */ IStream* /*data*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebMutableURLRequest::setHTTPBodyStream( 
    /* [in] */ IStream* /*data*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebMutableURLRequest::setHTTPMethod( 
    /* [in] */ BSTR method)
{
    m_method = SysAllocString(method);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebMutableURLRequest::setHTTPShouldHandleCookies( 
    /* [in] */ BOOL /*handleCookies*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebMutableURLRequest::setMainDocumentURL( 
    /* [in] */ BSTR /*theURL*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebMutableURLRequest::setTimeoutInterval( 
    /* [in] */ UINT /*timeoutInterval*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebMutableURLRequest::setURL( 
    /* [in] */ BSTR /*theURL*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WebMutableURLRequest::setValue( 
    /* [in] */ BSTR /*value*/,
    /* [in] */ BSTR /*field*/)
{
    DebugBreak();
    return E_NOTIMPL;
}

// IWebMutableURLRequest ----------------------------------------------------

void WebMutableURLRequest::setFormData(const WebCore::FormData* data)
{
    m_submitFormData = data;
}

const WebCore::FormData* WebMutableURLRequest::formData()
{
    return m_submitFormData;
}
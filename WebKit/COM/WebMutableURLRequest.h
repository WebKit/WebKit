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

#ifndef IWebMutableURLRequest_H
#define IWebMutableURLRequest_H

#include "config.h"

#include "IWebMutableURLRequest.h"

namespace WebCore
{
    class FormData;
}

class WebMutableURLRequest : public IWebMutableURLRequest
{
public:
    static WebMutableURLRequest* createInstance();
protected:
    WebMutableURLRequest();
    ~WebMutableURLRequest();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);

    // IWebURLRequest
    virtual HRESULT STDMETHODCALLTYPE requestWithURL( 
        /* [in] */ BSTR theURL,
        /* [optional][in] */ WebURLRequestCachePolicy cachePolicy,
        /* [optional][in] */ UINT timeoutInterval);
    
    virtual HRESULT STDMETHODCALLTYPE allHTTPHeaderFields( 
        /* [retval][out] */ IPropertyBag **result);
    
    virtual HRESULT STDMETHODCALLTYPE cachePolicy( 
        /* [retval][out] */ WebURLRequestCachePolicy *result);
    
    virtual HRESULT STDMETHODCALLTYPE HTTPBody( 
        /* [retval][out] */ IStream **result);
    
    virtual HRESULT STDMETHODCALLTYPE HTTPBodyStream( 
        /* [retval][out] */ IStream **result);
    
    virtual HRESULT STDMETHODCALLTYPE HTTPMethod( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE HTTPShouldHandleCookies( 
        /* [retval][out] */ BOOL *result);
    
    virtual HRESULT STDMETHODCALLTYPE initWithURL( 
        /* [in] */ BSTR url,
        /* [optional][in] */ WebURLRequestCachePolicy cachePolicy,
        /* [optional][in] */ UINT timeoutInterval);
    
    virtual HRESULT STDMETHODCALLTYPE mainDocumentURL( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE timeoutInterval( 
        /* [retval][out] */ UINT *result);
    
    virtual HRESULT STDMETHODCALLTYPE URL( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE valueForHTTPHeaderField( 
        /* [in] */ BSTR field,
        /* [retval][out] */ BSTR *result);

    // WebMutableURLRequest
    virtual HRESULT STDMETHODCALLTYPE addValue( 
        /* [in] */ BSTR value,
        /* [in] */ BSTR field);
    
    virtual HRESULT STDMETHODCALLTYPE setAllHTTPHeaderFields( 
        /* [in] */ IPropertyBag *headerFields);
    
    virtual HRESULT STDMETHODCALLTYPE setCachePolicy( 
        /* [in] */ WebURLRequestCachePolicy policy);
    
    virtual HRESULT STDMETHODCALLTYPE setHTTPBody( 
        /* [in] */ IStream *data);
    
    virtual HRESULT STDMETHODCALLTYPE setHTTPBodyStream( 
        /* [in] */ IStream *data);
    
    virtual HRESULT STDMETHODCALLTYPE setHTTPMethod( 
        /* [in] */ BSTR method);
    
    virtual HRESULT STDMETHODCALLTYPE setHTTPShouldHandleCookies( 
        /* [in] */ BOOL handleCookies);
    
    virtual HRESULT STDMETHODCALLTYPE setMainDocumentURL( 
        /* [in] */ BSTR theURL);
    
    virtual HRESULT STDMETHODCALLTYPE setTimeoutInterval( 
        /* [in] */ UINT timeoutInterval);
    
    virtual HRESULT STDMETHODCALLTYPE setURL( 
        /* [in] */ BSTR theURL);
    
    virtual HRESULT STDMETHODCALLTYPE setValue( 
        /* [in] */ BSTR value,
        /* [in] */ BSTR field);

    // IWebURLRequest
    void setFormData(const WebCore::FormData* data);
    const WebCore::FormData* formData();

protected:
    ULONG                       m_refCount;
    BSTR                        m_url;
    BSTR                        m_method;
    WebURLRequestCachePolicy    m_cachePolicy;
    UINT                        m_timeoutInterval;
    const WebCore::FormData*    m_submitFormData;
};

#endif

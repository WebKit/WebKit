/*
 * Copyright (C) 2006-2007, 2015 Apple Inc.  All rights reserved.
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

#ifndef WebMutableURLRequest_H
#define WebMutableURLRequest_H

#include "WebKit.h"
#include <WebCore/ResourceRequest.h>

namespace WebCore
{
    class FormData;
}

inline WebCore::ResourceRequestCachePolicy core(WebURLRequestCachePolicy policy)
{
    return static_cast<WebCore::ResourceRequestCachePolicy>(policy);
}

inline WebURLRequestCachePolicy kit(WebCore::ResourceRequestCachePolicy policy)
{
    return static_cast<WebURLRequestCachePolicy>(policy);
}

class WebMutableURLRequest final : public IWebMutableURLRequest, IWebMutableURLRequestPrivate
{
public:
    static WebMutableURLRequest* createInstance();
    static WebMutableURLRequest* createInstance(IWebMutableURLRequest* req);
    static WebMutableURLRequest* createInstance(const WebCore::ResourceRequest&);

    static WebMutableURLRequest* createImmutableInstance();
    static WebMutableURLRequest* createImmutableInstance(const WebCore::ResourceRequest&);
protected:
    WebMutableURLRequest(bool isMutable);
    ~WebMutableURLRequest();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebURLRequest
    virtual HRESULT STDMETHODCALLTYPE requestWithURL(_In_ BSTR theURL, WebURLRequestCachePolicy, double timeoutInterval);
    virtual HRESULT STDMETHODCALLTYPE allHTTPHeaderFields(_COM_Outptr_opt_ IPropertyBag** result);
    virtual HRESULT STDMETHODCALLTYPE cachePolicy(_Out_ WebURLRequestCachePolicy*);
    virtual HRESULT STDMETHODCALLTYPE HTTPBody(_COM_Outptr_opt_ IStream** result);
    virtual HRESULT STDMETHODCALLTYPE HTTPBodyStream(_COM_Outptr_opt_ IStream** result);
    virtual HRESULT STDMETHODCALLTYPE HTTPMethod(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE HTTPShouldHandleCookies(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE initWithURL(_In_ BSTR url, WebURLRequestCachePolicy, double timeoutInterval);
    virtual HRESULT STDMETHODCALLTYPE mainDocumentURL(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE timeoutInterval(_Out_ double*);
    virtual HRESULT STDMETHODCALLTYPE URL(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE valueForHTTPHeaderField(_In_ BSTR field, __deref_opt_out BSTR* result);
    virtual HRESULT STDMETHODCALLTYPE isEmpty(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE mutableCopy(_COM_Outptr_opt_ IWebMutableURLRequest**);
    virtual HRESULT STDMETHODCALLTYPE isEqual(_In_opt_ IWebURLRequest* other, _Out_ BOOL* result);

    // IWebMutableURLRequest
    virtual HRESULT STDMETHODCALLTYPE addValue(_In_ BSTR value, _In_ BSTR field);
    virtual HRESULT STDMETHODCALLTYPE setAllHTTPHeaderFields(_In_opt_ IPropertyBag*);
    virtual HRESULT STDMETHODCALLTYPE setCachePolicy(WebURLRequestCachePolicy);
    virtual HRESULT STDMETHODCALLTYPE setHTTPBody(_In_opt_ IStream*);
    virtual HRESULT STDMETHODCALLTYPE setHTTPBodyStream(_In_opt_ IStream*);
    virtual HRESULT STDMETHODCALLTYPE setHTTPMethod(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE setHTTPShouldHandleCookies(BOOL);
    virtual HRESULT STDMETHODCALLTYPE setMainDocumentURL(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE setTimeoutInterval(double);
    virtual HRESULT STDMETHODCALLTYPE setURL(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE setValue(_In_ BSTR value, _In_ BSTR field);
    virtual HRESULT STDMETHODCALLTYPE setAllowsAnyHTTPSCertificate();

    // IWebMutableURLRequestPrivate
    virtual HRESULT STDMETHODCALLTYPE setClientCertificate(ULONG_PTR);
    virtual /* [local] */ CFURLRequestRef STDMETHODCALLTYPE cfRequest();

    // WebMutableURLRequest
    void setFormData(RefPtr<WebCore::FormData>&&);
    const RefPtr<WebCore::FormData> formData() const;
    
    const WebCore::HTTPHeaderMap& httpHeaderFields() const;

    const WebCore::ResourceRequest& resourceRequest() const;
protected:
    ULONG m_refCount { 0 };
    bool m_isMutable;
    WebCore::ResourceRequest m_request;
};

#endif

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

#pragma once

#include "WebKit.h"
#include <WebCore/ResourceResponse.h>

class WebURLResponse final : public IWebHTTPURLResponse, IWebURLResponsePrivate
{
public:
    static WebURLResponse* createInstance();
    static WebURLResponse* createInstance(const WebCore::ResourceResponse& response);
protected:
    WebURLResponse();
    ~WebURLResponse();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebURLResponse
    virtual HRESULT STDMETHODCALLTYPE expectedContentLength(_Out_ long long*);    
    virtual HRESULT STDMETHODCALLTYPE initWithURL(_In_ BSTR url, _In_ BSTR mimeType, int expectedContentLength, _In_ BSTR textEncodingName);    
    virtual HRESULT STDMETHODCALLTYPE MIMEType(__deref_opt_out BSTR* result);
    virtual HRESULT STDMETHODCALLTYPE suggestedFilename(__deref_opt_out BSTR* result);
    virtual HRESULT STDMETHODCALLTYPE textEncodingName(__deref_opt_out BSTR* result);
    virtual HRESULT STDMETHODCALLTYPE URL(__deref_opt_out BSTR* result);

    // IWebHTTPURLResponse
    virtual HRESULT STDMETHODCALLTYPE allHeaderFields(_COM_Outptr_opt_ IPropertyBag** headerFields);    
    virtual HRESULT STDMETHODCALLTYPE localizedStringForStatusCode(int statusCode, __deref_opt_out BSTR* statusString);    
    virtual HRESULT STDMETHODCALLTYPE statusCode(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE isAttachment(_Out_ BOOL*);

    // IWebURLResponsePrivate
    virtual HRESULT STDMETHODCALLTYPE sslPeerCertificate(_Out_ ULONG_PTR* result);
    
    const WebCore::ResourceResponse& resourceResponse() const;

protected:
    HRESULT suggestedFileExtension(BSTR* result);

#if USE(CFURLCONNECTION)
    CFDictionaryRef certificateDictionary() const;
#endif

protected:
    ULONG m_refCount { 0 };
    WebCore::ResourceResponse m_response;

#if USE(CFURLCONNECTION)
    mutable RetainPtr<CFDictionaryRef> m_SSLCertificateInfo;    // this ensures certificate contexts are valid for the lifetime of this WebURLResponse.
#endif
};

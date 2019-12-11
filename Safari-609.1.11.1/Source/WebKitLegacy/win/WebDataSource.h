/*
 * Copyright (C) 2006, 2007, 2015 Apple Inc.  All rights reserved.
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

#ifndef WebDataSource_H
#define WebDataSource_H

#include "WebKit.h"
#include <WebCore/COMPtr.h>
#include <wtf/RefPtr.h>

class WebDocumentLoader;
class WebMutableURLRequest;

extern const GUID IID_WebDataSource;

class WebDataSource final : public IWebDataSource, public IWebDataSourcePrivate
{
public:
    static WebDataSource* createInstance(WebDocumentLoader*);
protected:
    WebDataSource(WebDocumentLoader*);
    ~WebDataSource();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebDataSource
    virtual HRESULT STDMETHODCALLTYPE initWithRequest(_In_opt_ IWebURLRequest*);
    virtual HRESULT STDMETHODCALLTYPE data(_COM_Outptr_opt_ IStream**);
    virtual HRESULT STDMETHODCALLTYPE representation(_COM_Outptr_opt_ IWebDocumentRepresentation**);
    virtual HRESULT STDMETHODCALLTYPE webFrame(_COM_Outptr_opt_ IWebFrame**);
    virtual HRESULT STDMETHODCALLTYPE initialRequest(_COM_Outptr_opt_ IWebURLRequest**);
    virtual HRESULT STDMETHODCALLTYPE request(_COM_Outptr_opt_ IWebMutableURLRequest**);
    virtual HRESULT STDMETHODCALLTYPE response(_COM_Outptr_opt_ IWebURLResponse**);
    virtual HRESULT STDMETHODCALLTYPE textEncodingName(_Deref_opt_out_ BSTR*);
    virtual HRESULT STDMETHODCALLTYPE isLoading(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE pageTitle(_Deref_opt_out_ BSTR*);
    virtual HRESULT STDMETHODCALLTYPE unreachableURL(_Deref_opt_out_ BSTR*);
    virtual HRESULT STDMETHODCALLTYPE webArchive(_COM_Outptr_opt_ IWebArchive**);
    virtual HRESULT STDMETHODCALLTYPE mainResource(_COM_Outptr_opt_ IWebResource**);
    virtual HRESULT STDMETHODCALLTYPE subresources(_COM_Outptr_opt_ IEnumVARIANT** enumResources);
    virtual HRESULT STDMETHODCALLTYPE subresourceForURL(_In_ BSTR url, _COM_Outptr_opt_ IWebResource**);
    virtual HRESULT STDMETHODCALLTYPE addSubresource(_In_opt_ IWebResource*);

    // IWebDataSourcePrivate
    virtual HRESULT STDMETHODCALLTYPE overrideEncoding(_Deref_opt_out_ BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setOverrideEncoding(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE mainDocumentError(_COM_Outptr_opt_ IWebError**);
    virtual HRESULT STDMETHODCALLTYPE setDeferMainResourceDataLoad(BOOL);

    // WebDataSource
    WebDocumentLoader* documentLoader() const;

protected:
    ULONG m_refCount { 0 };
    RefPtr<WebDocumentLoader> m_loader;
    COMPtr<IWebDocumentRepresentation> m_representation;
};

#endif

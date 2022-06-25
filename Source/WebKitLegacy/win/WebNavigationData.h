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

#ifndef WebNavigationData_h
#define WebNavigationData_h

#include "WebKit.h"

#include <WebCore/BString.h>
#include <WebCore/COMPtr.h>

class WebNavigationData final : public IWebNavigationData {
public:
    static WebNavigationData* createInstance(const WTF::String& url, const WTF::String& title, IWebURLRequest*, IWebURLResponse*, bool hasSubstituteData, const WTF::String& clientRedirectSource);
private:
    WebNavigationData(const WTF::String& url, const WTF::String& title, IWebURLRequest*, IWebURLResponse*, bool hasSubstituteData, const WTF::String& clientRedirectSource);
    ~WebNavigationData();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebNavigationData
    virtual HRESULT STDMETHODCALLTYPE url(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE title(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE originalRequest(_COM_Outptr_opt_ IWebURLRequest**);
    virtual HRESULT STDMETHODCALLTYPE response(_COM_Outptr_opt_ IWebURLResponse**);
    virtual HRESULT STDMETHODCALLTYPE hasSubstituteData(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE clientRedirectSource(__deref_opt_out BSTR*);

private:
    ULONG m_refCount { 0 };
    WebCore::BString m_url;
    WebCore::BString m_title;
    COMPtr<IWebURLRequest> m_request;
    COMPtr<IWebURLResponse> m_response;
    bool m_hasSubstituteData;
    WebCore::BString m_clientRedirectSource;

};

#endif // WebNavigationData_h

/*
 * Copyright (C) 2007, 2015 Apple Inc.  All rights reserved.
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
#include <WebCore/COMPtr.h>
#include <WebCore/ResourceError.h>
#include <wtf/RetainPtr.h>

class WebError final : public IWebError, IWebErrorPrivate {
public:
    static WebError* createInstance(const WebCore::ResourceError&, IPropertyBag* userInfo = 0);
    static WebError* createInstance();
protected:
    WebError(const WebCore::ResourceError&, IPropertyBag* userInfo);
    ~WebError();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebError
    virtual HRESULT STDMETHODCALLTYPE init(_In_ BSTR domain, int code, _In_ BSTR url);
    virtual HRESULT STDMETHODCALLTYPE code(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE domain(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE localizedDescription(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE localizedFailureReason(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE localizedRecoveryOptions(__deref_opt_out IEnumVARIANT** result);
    virtual HRESULT STDMETHODCALLTYPE localizedRecoverySuggestion(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE recoverAttempter(__deref_opt_out IUnknown** result);
    virtual HRESULT STDMETHODCALLTYPE userInfo(_COM_Outptr_opt_ IPropertyBag **result);
    virtual HRESULT STDMETHODCALLTYPE failingURL(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE isPolicyChangeError(_Out_ BOOL*);

    // IWebErrorPrivate
    virtual HRESULT STDMETHODCALLTYPE sslPeerCertificate(_Out_ ULONG_PTR*);

    const WebCore::ResourceError& resourceError() const;

private:
    ULONG m_refCount { 0 };
    COMPtr<IPropertyBag> m_userInfo;
#if USE(CFURLCONNECTION)
    RetainPtr<CFDictionaryRef> m_cfErrorUserInfoDict;
#endif
    WebCore::ResourceError m_error;
};

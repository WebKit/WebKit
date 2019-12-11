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

#ifndef WebURLCredential_h
#define WebURLCredential_h

#include "WebKit.h"
#include <WebCore/Credential.h>

class WebURLCredential final : public IWebURLCredential
{
public:
    static WebURLCredential* createInstance();
    static WebURLCredential* createInstance(const WebCore::Credential&);
private:
    WebURLCredential(const WebCore::Credential&);
    ~WebURLCredential();
public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebURLCredential
    virtual HRESULT STDMETHODCALLTYPE hasPassword(_Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE initWithUser(_In_ BSTR user, _In_ BSTR password, WebURLCredentialPersistence);
    virtual HRESULT STDMETHODCALLTYPE password(__deref_opt_out BSTR* password);
    virtual HRESULT STDMETHODCALLTYPE persistence(_Out_ WebURLCredentialPersistence* result);
    virtual HRESULT STDMETHODCALLTYPE user(__deref_opt_out BSTR* result);

    // WebURLCredential
    const WebCore::Credential& credential() const;

protected:
    ULONG m_refCount { 0 };
    WebCore::Credential m_credential;
};


#endif

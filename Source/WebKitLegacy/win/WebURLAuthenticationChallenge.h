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

#ifndef WebURLAuthenticationChallenge_h
#define WebURLAuthenticationChallenge_h

#include "WebKit.h"
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/COMPtr.h>

class DECLSPEC_UUID("FD3B2381-0BB6-4B59-AF09-0E599C8901CF") WebURLAuthenticationChallenge final : public IWebURLAuthenticationChallenge {
public:
    static WebURLAuthenticationChallenge* createInstance(const WebCore::AuthenticationChallenge&);
    static WebURLAuthenticationChallenge* createInstance(const WebCore::AuthenticationChallenge&, IWebURLAuthenticationChallengeSender*);
private:
    WebURLAuthenticationChallenge(const WebCore::AuthenticationChallenge&, IWebURLAuthenticationChallengeSender*);
    ~WebURLAuthenticationChallenge();
public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebURLAuthenticationChallenge
    virtual HRESULT STDMETHODCALLTYPE initWithProtectionSpace(_In_opt_ IWebURLProtectionSpace*, _In_opt_ IWebURLCredential*, 
        int previousFailureCount, _In_opt_ IWebURLResponse* failureResponse, _In_opt_ IWebError*, _In_opt_ IWebURLAuthenticationChallengeSender*);
    virtual HRESULT STDMETHODCALLTYPE initWithAuthenticationChallenge(_In_opt_ IWebURLAuthenticationChallenge*, _In_opt_ IWebURLAuthenticationChallengeSender*);
    virtual HRESULT STDMETHODCALLTYPE error(_COM_Outptr_opt_ IWebError**);
    virtual HRESULT STDMETHODCALLTYPE failureResponse(_COM_Outptr_opt_ IWebURLResponse**);
    virtual HRESULT STDMETHODCALLTYPE previousFailureCount(_Out_ UINT*);
    virtual HRESULT STDMETHODCALLTYPE proposedCredential(_COM_Outptr_opt_ IWebURLCredential**);
    virtual HRESULT STDMETHODCALLTYPE protectionSpace(_COM_Outptr_opt_ IWebURLProtectionSpace**);
    virtual HRESULT STDMETHODCALLTYPE sender(_COM_Outptr_opt_ IWebURLAuthenticationChallengeSender**);

    // WebURLAuthenticationChallenge
    const WebCore::AuthenticationChallenge& authenticationChallenge() const;

protected:
    ULONG m_refCount { 0 };

    WebCore::AuthenticationChallenge m_authenticationChallenge;
    COMPtr<IWebURLAuthenticationChallengeSender> m_sender;
};


#endif

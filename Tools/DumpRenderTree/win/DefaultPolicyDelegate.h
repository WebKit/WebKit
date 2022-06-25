/*
 * Copyright (C) 2007-2019 Apple Inc.  All rights reserved.
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

#ifndef DefaultPolicyDelegate_h
#define DefaultPolicyDelegate_h

#include <WebKitLegacy/WebKit.h>

class DefaultPolicyDelegate final : public IWebPolicyDelegate {
public:
    static DefaultPolicyDelegate* sharedInstance();
    static DefaultPolicyDelegate* createInstance();

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebPolicyDelegate
    virtual HRESULT STDMETHODCALLTYPE decidePolicyForNavigationAction(_In_opt_ IWebView*, _In_opt_ IPropertyBag* actionInformation,
        _In_opt_ IWebURLRequest*, _In_opt_ IWebFrame*, _In_opt_ IWebPolicyDecisionListener*);
    
    virtual HRESULT STDMETHODCALLTYPE decidePolicyForNewWindowAction(_In_opt_ IWebView*, _In_opt_ IPropertyBag* actionInformation,
        _In_opt_ IWebURLRequest*, _In_ BSTR frameName, _In_opt_ IWebPolicyDecisionListener*);
    
    virtual HRESULT STDMETHODCALLTYPE decidePolicyForMIMEType(_In_opt_ IWebView*, _In_ BSTR type, _In_opt_ IWebURLRequest*,
        _In_opt_ IWebFrame*, _In_opt_ IWebPolicyDecisionListener*);
    
    virtual HRESULT STDMETHODCALLTYPE unableToImplementPolicyWithError(_In_opt_ IWebView*, _In_opt_ IWebError*, _In_opt_ IWebFrame*);

private:
    DefaultPolicyDelegate();
    ~DefaultPolicyDelegate();

    ULONG m_refCount { 0 };
};

#endif // DefaultPolicyDelegate_h

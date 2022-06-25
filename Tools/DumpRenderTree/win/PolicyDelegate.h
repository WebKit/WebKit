/*
 * Copyright (C) 2007, 2009, 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PolicyDelegate_h
#define PolicyDelegate_h

#include <WebKitLegacy/WebKit.h>

class TestRunner;

class PolicyDelegate final : public IWebPolicyDelegate {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PolicyDelegate();

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebPolicyDelegate
    virtual HRESULT STDMETHODCALLTYPE decidePolicyForNavigationAction(_In_opt_ IWebView*, _In_opt_ IPropertyBag* actionInformation,
        _In_opt_ IWebURLRequest*, _In_opt_ IWebFrame*, _In_opt_ IWebPolicyDecisionListener*);    
    virtual HRESULT STDMETHODCALLTYPE decidePolicyForNewWindowAction(_In_opt_ IWebView*, _In_opt_ IPropertyBag* actionInformation,
        _In_opt_ IWebURLRequest*, _In_ BSTR frameName, _In_opt_ IWebPolicyDecisionListener*)
    {
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE decidePolicyForMIMEType(_In_opt_ IWebView*, _In_ BSTR type, _In_opt_ IWebURLRequest*,
        _In_opt_ IWebFrame*, _In_opt_ IWebPolicyDecisionListener*)
    {
        return E_NOTIMPL;
    }
    
    virtual HRESULT STDMETHODCALLTYPE unableToImplementPolicyWithError(_In_opt_ IWebView*, _In_opt_ IWebError*, _In_opt_ IWebFrame*);

    // PolicyDelegate
    void setPermissive(bool permissive) { m_permissiveDelegate = permissive; }
    void setControllerToNotifyDone(TestRunner* controller) { m_controllerToNotifyDone = controller; }

private:
    TestRunner* m_controllerToNotifyDone { nullptr };
    ULONG m_refCount { 1 };
    bool m_permissiveDelegate { false };
};

#endif // PolicyDelegate_h

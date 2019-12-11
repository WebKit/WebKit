/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#ifndef ResourceLoadDelegate_h
#define ResourceLoadDelegate_h

#include <WebKitLegacy/WebKit.h>
#include <string>
#include <wtf/HashMap.h>

class ResourceLoadDelegate : public IWebResourceLoadDelegate, public IWebResourceLoadDelegatePrivate2 {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ResourceLoadDelegate();
    virtual ~ResourceLoadDelegate();

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebResourceLoadDelegate
    virtual HRESULT STDMETHODCALLTYPE identifierForInitialRequest(_In_opt_ IWebView*, _In_opt_ IWebURLRequest*, _In_opt_ IWebDataSource*, unsigned long identifier);        
    virtual HRESULT STDMETHODCALLTYPE willSendRequest(_In_opt_ IWebView*, unsigned long identifier, _In_opt_ IWebURLRequest*, _In_opt_ IWebURLResponse*,
        _In_opt_ IWebDataSource*, _COM_Outptr_opt_ IWebURLRequest**);
    virtual HRESULT STDMETHODCALLTYPE didReceiveAuthenticationChallenge(_In_opt_ IWebView*, unsigned long identifier, _In_opt_ IWebURLAuthenticationChallenge*, _In_opt_ IWebDataSource*);
    virtual HRESULT STDMETHODCALLTYPE didCancelAuthenticationChallenge(_In_opt_ IWebView*, unsigned long identifier, _In_opt_ IWebURLAuthenticationChallenge*, _In_opt_ IWebDataSource *dataSource)
    {
        return E_NOTIMPL;
    }
        
    virtual HRESULT STDMETHODCALLTYPE didReceiveResponse(_In_opt_ IWebView*, unsigned long identifier, _In_opt_ IWebURLResponse*, _In_opt_ IWebDataSource*);
        
    virtual HRESULT STDMETHODCALLTYPE didReceiveContentLength(_In_opt_ IWebView*, unsigned long identifier, UINT length, _In_opt_ IWebDataSource*)
    {
        return E_NOTIMPL; 
    }
        
    virtual HRESULT STDMETHODCALLTYPE didFinishLoadingFromDataSource(_In_opt_ IWebView*, unsigned long identifier, _In_opt_ IWebDataSource*);
    virtual HRESULT STDMETHODCALLTYPE didFailLoadingWithError(_In_opt_ IWebView*, unsigned long identifier, _In_opt_ IWebError*, _In_opt_ IWebDataSource*);
    virtual HRESULT STDMETHODCALLTYPE plugInFailedWithError(_In_opt_ IWebView*, _In_opt_ IWebError*, _In_opt_ IWebDataSource*) { return E_NOTIMPL; }

    // IWebResourceLoadDelegatePrivate2
    virtual HRESULT STDMETHODCALLTYPE removeIdentifierForRequest(_In_opt_ IWebView*, unsigned long identifier);
    
private:
    static std::wstring descriptionSuitableForTestResult(IWebURLRequest*);
    static std::wstring descriptionSuitableForTestResult(IWebURLResponse*);
    std::wstring descriptionSuitableForTestResult(unsigned long) const;
    std::wstring descriptionSuitableForTestResult(IWebError*, unsigned long) const;

    typedef HashMap<unsigned long, std::wstring> IdentifierMap;
    IdentifierMap& urlMap() { return m_urlMap; }
    IdentifierMap m_urlMap;

    ULONG m_refCount { 1 };
};

#endif // ResourceLoadDelegate_h

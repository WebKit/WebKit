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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#include <WebKit/WebKit.h>

class ResourceLoadDelegate : public IWebResourceLoadDelegate {
public:
    ResourceLoadDelegate();
    virtual ~ResourceLoadDelegate();

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);

    // IWebResourceLoadDelegate
    virtual HRESULT STDMETHODCALLTYPE identifierForInitialRequest( 
        /* [in] */ IWebView *webView,
        /* [in] */ IWebURLRequest *request,
        /* [in] */ IWebDataSource *dataSource,
        /* [in] */ unsigned long identifier);
        
    virtual HRESULT STDMETHODCALLTYPE willSendRequest( 
        /* [in] */ IWebView *webView,
        /* [in] */ unsigned long identifier,
        /* [in] */ IWebURLRequest *request,
        /* [in] */ IWebURLResponse *redirectResponse,
        /* [in] */ IWebDataSource *dataSource,
        /* [retval][out] */ IWebURLRequest **newRequest);
        
    virtual HRESULT STDMETHODCALLTYPE didReceiveAuthenticationChallenge( 
        /* [in] */ IWebView *webView,
        /* [in] */ unsigned long identifier,
        /* [in] */ IWebURLAuthenticationChallenge *challenge,
        /* [in] */ IWebDataSource *dataSource);
        
    virtual HRESULT STDMETHODCALLTYPE didCancelAuthenticationChallenge( 
        /* [in] */ IWebView *webView,
        /* [in] */ unsigned long identifier,
        /* [in] */ IWebURLAuthenticationChallenge *challenge,
        /* [in] */ IWebDataSource *dataSource) { return E_NOTIMPL; }
        
    virtual HRESULT STDMETHODCALLTYPE didReceiveResponse( 
        /* [in] */ IWebView *webView,
        /* [in] */ unsigned long identifier,
        /* [in] */ IWebURLResponse *response,
        /* [in] */ IWebDataSource *dataSource);
        
    virtual HRESULT STDMETHODCALLTYPE didReceiveContentLength( 
        /* [in] */ IWebView *webView,
        /* [in] */ unsigned long identifier,
        /* [in] */ UINT length,
        /* [in] */ IWebDataSource *dataSource) { return E_NOTIMPL; }
        
    virtual HRESULT STDMETHODCALLTYPE didFinishLoadingFromDataSource( 
        /* [in] */ IWebView *webView,
        /* [in] */ unsigned long identifier,
        /* [in] */ IWebDataSource *dataSource);
        
    virtual HRESULT STDMETHODCALLTYPE didFailLoadingWithError( 
        /* [in] */ IWebView *webView,
        /* [in] */ unsigned long identifier,
        /* [in] */ IWebError *error,
        /* [in] */ IWebDataSource *dataSource);
        
    virtual HRESULT STDMETHODCALLTYPE plugInFailedWithError( 
        /* [in] */ IWebView *webView,
        /* [in] */ IWebError *error,
        /* [in] */ IWebDataSource *dataSource) { return E_NOTIMPL; }
    
protected:
    ULONG m_refCount;
};

#endif // ResourceLoadDelegate_h

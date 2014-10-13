/*
* Copyright (C) 2014 Apple Inc. All Rights Reserved.
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

#ifndef ResourceLoadDelegate_h
#define ResourceLoadDelegate_h

#include <WebKit/WebKit.h>

class WinLauncher;

class ResourceLoadDelegate : public IWebResourceLoadDelegate {
public:
    ResourceLoadDelegate(WinLauncher* client)
        : m_refCount(1), m_client(client) { }

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);

    // IWebResourceLoadDelegate
    virtual HRESULT STDMETHODCALLTYPE identifierForInitialRequest(IWebView*, IWebURLRequest*, IWebDataSource*, unsigned long identifier);
    virtual HRESULT STDMETHODCALLTYPE willSendRequest(IWebView*, unsigned long identifier, IWebURLRequest*, IWebURLResponse* redirectResponse, IWebDataSource*, IWebURLRequest** newRequest);
    virtual HRESULT STDMETHODCALLTYPE didReceiveAuthenticationChallenge(IWebView*, unsigned long identifier, IWebURLAuthenticationChallenge*, IWebDataSource*);
    virtual HRESULT STDMETHODCALLTYPE didCancelAuthenticationChallenge(IWebView*, unsigned long identifier, IWebURLAuthenticationChallenge*, IWebDataSource*);
    virtual HRESULT STDMETHODCALLTYPE didReceiveResponse(IWebView*, unsigned long identifier, IWebURLResponse*, IWebDataSource*);
    virtual HRESULT STDMETHODCALLTYPE didReceiveContentLength(IWebView*, unsigned long identifier, UINT length, IWebDataSource*);
    virtual HRESULT STDMETHODCALLTYPE didFinishLoadingFromDataSource(IWebView*, unsigned long identifier, IWebDataSource*);
    virtual HRESULT STDMETHODCALLTYPE didFailLoadingWithError(IWebView*, unsigned long identifier, IWebError*, IWebDataSource*);
    virtual HRESULT STDMETHODCALLTYPE plugInFailedWithError(IWebView*, IWebError*, IWebDataSource*);

private:
    WinLauncher* m_client;
    int m_refCount;
};

#endif // ResourceLoadDelegate

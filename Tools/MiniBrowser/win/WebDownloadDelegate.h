/*
* Copyright (C) 2015 Apple Inc. All Rights Reserved.
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

#ifndef WebDownloadDelegate_h
#define WebDownloadDelegate_h

#include <WebKitLegacy/WebKit.h>
#include <WebKitLegacy/WebKitCOMAPI.h>

class WebKitLegacyBrowserWindow;

class WebDownloadDelegate : public IWebDownloadDelegate {
public:
    WebDownloadDelegate(WebKitLegacyBrowserWindow& client);
    virtual ~WebDownloadDelegate();

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    virtual HRESULT STDMETHODCALLTYPE decideDestinationWithSuggestedFilename(_In_opt_ IWebDownload*, _In_ BSTR filename);
    virtual HRESULT STDMETHODCALLTYPE didCancelAuthenticationChallenge(_In_opt_ IWebDownload*, _In_opt_ IWebURLAuthenticationChallenge*);
    virtual HRESULT STDMETHODCALLTYPE didCreateDestination(_In_opt_ IWebDownload*, _In_ BSTR destination);
    virtual HRESULT STDMETHODCALLTYPE didFailWithError(_In_opt_ IWebDownload*, _In_opt_ IWebError*);
    virtual HRESULT STDMETHODCALLTYPE didReceiveAuthenticationChallenge(_In_opt_ IWebDownload*, _In_opt_ IWebURLAuthenticationChallenge*);
    virtual HRESULT STDMETHODCALLTYPE didReceiveDataOfLength(_In_opt_ IWebDownload*, unsigned length);
    virtual HRESULT STDMETHODCALLTYPE didReceiveResponse(_In_opt_ IWebDownload*, _In_opt_ IWebURLResponse*);
    virtual HRESULT STDMETHODCALLTYPE shouldDecodeSourceDataOfMIMEType(_In_opt_ IWebDownload*, _In_ BSTR encodingType, _Out_ BOOL* shouldDecode);
    virtual HRESULT STDMETHODCALLTYPE willResumeWithResponse(_In_opt_ IWebDownload*, _In_opt_ IWebURLResponse*, long long fromByte);
    virtual HRESULT STDMETHODCALLTYPE willSendRequest(_In_opt_ IWebDownload*, _In_opt_ IWebMutableURLRequest*, _In_opt_ IWebURLResponse* redirectResponse, _COM_Outptr_opt_ IWebMutableURLRequest** finalRequest);
    virtual HRESULT STDMETHODCALLTYPE didBegin(_In_opt_ IWebDownload*);
    virtual HRESULT STDMETHODCALLTYPE didFinish(_In_opt_ IWebDownload*);

private:
    WebKitLegacyBrowserWindow& m_client;
};

#endif

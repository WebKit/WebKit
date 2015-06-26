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

#include <WebKit/WebKit.h>
#include <WebKit/WebKitCOMAPI.h>

class WebDownloadDelegate : public IWebDownloadDelegate {
public:
    WebDownloadDelegate();
    virtual ~WebDownloadDelegate();

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release(void);

    virtual HRESULT STDMETHODCALLTYPE decideDestinationWithSuggestedFilename(IWebDownload*, BSTR filename);
    virtual HRESULT STDMETHODCALLTYPE didCancelAuthenticationChallenge(IWebDownload*, IWebURLAuthenticationChallenge*);
    virtual HRESULT STDMETHODCALLTYPE didCreateDestination(IWebDownload*, BSTR destination);
    virtual HRESULT STDMETHODCALLTYPE didFailWithError(IWebDownload*, IWebError*);
    virtual HRESULT STDMETHODCALLTYPE didReceiveAuthenticationChallenge(IWebDownload*, IWebURLAuthenticationChallenge*);
    virtual HRESULT STDMETHODCALLTYPE didReceiveDataOfLength(IWebDownload*, unsigned length);
    virtual HRESULT STDMETHODCALLTYPE didReceiveResponse(IWebDownload*, IWebURLResponse*);
    virtual HRESULT STDMETHODCALLTYPE shouldDecodeSourceDataOfMIMEType(IWebDownload*, BSTR encodingType, BOOL* shouldDecode);
    virtual HRESULT STDMETHODCALLTYPE willResumeWithResponse(IWebDownload*, IWebURLResponse*, long long fromByte);
    virtual HRESULT STDMETHODCALLTYPE willSendRequest(IWebDownload*, IWebMutableURLRequest*, IWebURLResponse* redirectResponse, IWebMutableURLRequest** finalRequest);
    virtual HRESULT STDMETHODCALLTYPE didBegin(IWebDownload*);
    virtual HRESULT STDMETHODCALLTYPE didFinish(IWebDownload*);

private:
    int m_refCount;
};

#endif

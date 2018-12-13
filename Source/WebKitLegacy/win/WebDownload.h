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
#include <wtf/RetainPtr.h>
#include <wtf/WallTime.h>
#include <wtf/text/WTFString.h>

#if USE(CFURLCONNECTION)
#include <CFNetwork/CFURLDownloadPriv.h>
#elif USE(CURL)
#include <WebCore/CurlDownload.h>
#endif

namespace WebCore {
    class ResourceHandle;
    class ResourceRequest;
    class ResourceResponse;
}

class WebDownload final
: public IWebDownload
, public IWebURLAuthenticationChallengeSender
#if USE(CURL)
, public WebCore::CurlDownloadListener
#endif
{
public:
    static WebDownload* createInstance(const URL&, IWebDownloadDelegate*);
    static WebDownload* createInstance(WebCore::ResourceHandle*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, IWebDownloadDelegate*);
    static WebDownload* createInstance();
private:
    WebDownload();
    void init(WebCore::ResourceHandle*, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&, IWebDownloadDelegate*);
    void init(const URL&, IWebDownloadDelegate*);
    ~WebDownload();
public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebDownload
    virtual HRESULT STDMETHODCALLTYPE initWithRequest(_In_opt_ IWebURLRequest*, _In_opt_ IWebDownloadDelegate*);
    virtual HRESULT STDMETHODCALLTYPE initToResumeWithBundle(_In_ BSTR bundlePath, _In_opt_ IWebDownloadDelegate*);
    virtual HRESULT STDMETHODCALLTYPE canResumeDownloadDecodedWithEncodingMIMEType(_In_ BSTR mimeType, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE start();
    virtual HRESULT STDMETHODCALLTYPE cancel();
    virtual HRESULT STDMETHODCALLTYPE cancelForResume();
    virtual HRESULT STDMETHODCALLTYPE deletesFileUponFailure(_Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE bundlePathForTargetPath(_In_ BSTR target, __deref_out_opt BSTR* bundle);
    virtual HRESULT STDMETHODCALLTYPE request(_COM_Outptr_opt_ IWebURLRequest**);
    virtual HRESULT STDMETHODCALLTYPE setDeletesFileUponFailure(BOOL);
    virtual HRESULT STDMETHODCALLTYPE setDestination(_In_ BSTR path, BOOL allowOverwrite);

    // IWebURLAuthenticationChallengeSender
    virtual HRESULT STDMETHODCALLTYPE cancelAuthenticationChallenge(_In_opt_ IWebURLAuthenticationChallenge*);
    virtual HRESULT STDMETHODCALLTYPE continueWithoutCredentialForAuthenticationChallenge(_In_opt_ IWebURLAuthenticationChallenge*);
    virtual HRESULT STDMETHODCALLTYPE useCredential(_In_opt_ IWebURLCredential*, _In_opt_ IWebURLAuthenticationChallenge*);

#if USE(CFURLCONNECTION)
    // CFURLDownload Callbacks
    void didStart();
    CFURLRequestRef willSendRequest(CFURLRequestRef, CFURLResponseRef);
    void didReceiveAuthenticationChallenge(CFURLAuthChallengeRef);
    void didReceiveResponse(CFURLResponseRef);
    void willResumeWithResponse(CFURLResponseRef, UInt64);
    void didReceiveData(CFIndex);
    bool shouldDecodeDataOfMIMEType(CFStringRef);
    void decideDestinationWithSuggestedObjectName(CFStringRef);
    void didCreateDestination(CFURLRef);
    void didFinish();
    void didFail(CFErrorRef);
#elif USE(CURL)
    virtual void didReceiveResponse(const WebCore::ResourceResponse&);
    virtual void didReceiveDataOfLength(int size);
    virtual void didFinish();
    virtual void didFail();
#endif

protected:
    ULONG m_refCount { 0 };

    WTF::String m_destination;
    WTF::String m_bundlePath;
#if USE(CFURLCONNECTION)
    RetainPtr<CFURLDownloadRef> m_download;
#elif USE(CURL)
    RefPtr<WebCore::CurlDownload> m_download;
#endif
    COMPtr<IWebMutableURLRequest> m_request;
    COMPtr<IWebDownloadDelegate> m_delegate;

#ifndef NDEBUG
    WallTime m_startTime;
    WallTime m_dataTime;
    int m_received;
#endif
};

/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef WebScriptDebugServer_H
#define WebScriptDebugServer_H

#include "WebKit.h"

#include <wtf/HashSet.h>
#pragma warning(push, 0)
#include <WebCore/COMPtr.h>
#pragma warning(pop)

interface IWebView;

class WebScriptDebugServer : public IWebScriptDebugServer, public IWebScriptDebugListener
{
public:
    static WebScriptDebugServer* createInstance();
    static WebScriptDebugServer* sharedWebScriptDebugServer();

private:
    WebScriptDebugServer();

public:
    virtual ~WebScriptDebugServer();

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface( 
        /* [in] */ REFIID riid,
        /* [iid_is][out] */ void** ppvObject);

    virtual ULONG STDMETHODCALLTYPE AddRef();

    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebScriptDebugServer
    virtual HRESULT STDMETHODCALLTYPE sharedWebScriptDebugServer( 
        /* [retval][out] */ IWebScriptDebugServer** server);

    virtual HRESULT STDMETHODCALLTYPE addListener(
        /* [in] */ IWebScriptDebugListener*);

    virtual HRESULT STDMETHODCALLTYPE removeListener(
        /* [in] */ IWebScriptDebugListener*);

    virtual HRESULT STDMETHODCALLTYPE step();

    virtual HRESULT STDMETHODCALLTYPE pause();

    virtual HRESULT STDMETHODCALLTYPE resume();

    virtual HRESULT STDMETHODCALLTYPE isPaused(
        /* [out, retval] */ BOOL* isPaused);

    // IWebScriptDebugListener
    virtual HRESULT STDMETHODCALLTYPE didLoadMainResourceForDataSource(
        /* [in] */ IWebView* webView,
        /* [in] */ IWebDataSource* dataSource);

    virtual HRESULT STDMETHODCALLTYPE didParseSource(
        /* [in] */ IWebView* webView,
        /* [in] */ BSTR sourceCode,
        /* [in] */ UINT baseLineNumber,
        /* [in] */ BSTR url,
        /* [in] */ int sourceID,
        /* [in] */ IWebFrame* webFrame);

    virtual HRESULT STDMETHODCALLTYPE failedToParseSource(
        /* [in] */ IWebView* webView,
        /* [in] */ BSTR sourceCode,
        /* [in] */ UINT baseLineNumber,
        /* [in] */ BSTR url,
        /* [in] */ BSTR error,
        /* [in] */ IWebFrame*);

    virtual HRESULT STDMETHODCALLTYPE didEnterCallFrame(
        /* [in] */ IWebView* webView,
        /* [in] */ IWebScriptCallFrame* frame,
        /* [in] */ int sourceID,
        /* [in] */ int lineNumber,
        /* [in] */ IWebFrame*);

    virtual HRESULT STDMETHODCALLTYPE willExecuteStatement(
        /* [in] */ IWebView*,
        /* [in] */ IWebScriptCallFrame*,
        /* [in] */ int sourceID,
        /* [in] */ int lineNumber,
        /* [in] */ IWebFrame*);

    virtual HRESULT STDMETHODCALLTYPE willLeaveCallFrame(
        /* [in] */ IWebView* webView,
        /* [in] */ IWebScriptCallFrame* frame,
        /* [in] */ int sourceID,
        /* [in] */ int lineNumber,
        /* [in] */ IWebFrame*);

    virtual HRESULT STDMETHODCALLTYPE exceptionWasRaised(
        /* [in] */ IWebView* webView,
        /* [in] */ IWebScriptCallFrame*,
        /* [in] */ int sourceID,
        /* [in] */ int lineNumber,
        /* [in] */ IWebFrame*);

    virtual HRESULT STDMETHODCALLTYPE serverDidDie();

    void suspendProcessIfPaused();
    static unsigned listenerCount();

private:
    bool m_paused;
    bool m_step;

    ULONG m_refCount;
};

#endif

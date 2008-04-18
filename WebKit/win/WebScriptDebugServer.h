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
#pragma warning(push, 0)
#include <WebCore/JavaScriptDebugListener.h>
#pragma warning(pop)

namespace WebCore {
    class Page;
}

interface IWebView;

class WebScriptDebugServer : public IWebScriptDebugServer, WebCore::JavaScriptDebugListener {
public:
    static WebScriptDebugServer* createInstance();
    static WebScriptDebugServer* sharedWebScriptDebugServer();

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

    void didLoadMainResourceForDataSource(IWebView*, IWebDataSource*);
    void serverDidDie();

    static unsigned listenerCount();

private:
    WebScriptDebugServer();
    ~WebScriptDebugServer();

    void suspendProcessIfPaused();

    // JavaScriptDebugListener
    virtual void didParseSource(KJS::ExecState*, const KJS::UString& source, int startingLineNumber, const KJS::UString& sourceURL, int sourceID);
    virtual void failedToParseSource(KJS::ExecState*, const KJS::UString& source, int startingLineNumber, const KJS::UString& sourceURL, int errorLine, const KJS::UString& errorMessage);
    virtual void didEnterCallFrame(KJS::ExecState*, int sourceID, int lineNumber);
    virtual void willExecuteStatement(KJS::ExecState*, int sourceID, int lineNumber);
    virtual void willLeaveCallFrame(KJS::ExecState*, int sourceID, int lineNumber);
    virtual void exceptionWasRaised(KJS::ExecState*, int sourceID, int lineNumber);

    bool m_paused;
    bool m_step;
    bool m_callingListeners;

    ULONG m_refCount;
};

#endif

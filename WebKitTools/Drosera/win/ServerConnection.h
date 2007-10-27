/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2006, 2007 Vladimir Olexa (vladimir.olexa@gmail.com)
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

#ifndef ServerConnection_H
#define ServerConnection_H

#include <string>
#include <WebCore/COMPtr.h>
#include <WebKit/IWebScriptDebugListener.h>
#include <WebKit/IWebScriptDebugServer.h>

class DebuggerClient;
interface IWebScriptCallFrame;

typedef struct OpaqueJSContext* JSGlobalContextRef;

class ServerConnection : public IWebScriptDebugListener {
public:
    ServerConnection();
    ~ServerConnection();

    void setGlobalContext(JSGlobalContextRef);
    void pause();
    void resume();
    void stepInto();

    void applicationTerminating();
    void serverConnectionDidDie();
    IWebScriptCallFrame* currentFrame() const;

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(
        /* [in] */ REFIID riid,
        /* [retval][out] */ void** ppvObject);

    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

    // IWebScriptDebugListener
    HRESULT STDMETHODCALLTYPE didLoadMainResourceForDataSource(
        /* [in] */ IWebView*,
        /* [in] */ IWebDataSource* dataSource);

    HRESULT STDMETHODCALLTYPE didParseSource(
        /* [in] */ IWebView*,
        /* [in] */ BSTR sourceCode,
        /* [in] */ UINT baseLineNumber,
        /* [in] */ BSTR url,
        /* [in] */ int sourceID,
        /* [in] */ IWebFrame* webFrame);

    HRESULT STDMETHODCALLTYPE failedToParseSource(
        /* [in] */ IWebView*,
        /* [in] */ BSTR sourceCode,
        /* [in] */ UINT baseLineNumber,
        /* [in] */ BSTR url,
        /* [in] */ BSTR error,
        /* [in] */ IWebFrame*);

    HRESULT STDMETHODCALLTYPE didEnterCallFrame(
        /* [in] */ IWebView*,
        /* [in] */ IWebScriptCallFrame* frame,
        /* [in] */ int sourceID,
        /* [in] */ int lineNumber,
        /* [in] */ IWebFrame*);

    HRESULT STDMETHODCALLTYPE willExecuteStatement(
        /* [in] */ IWebView*,
        /* [in] */ IWebScriptCallFrame*,
        /* [in] */ int sourceID,
        /* [in] */ int lineNumber,
        /* [in] */ IWebFrame*);

    HRESULT STDMETHODCALLTYPE willLeaveCallFrame(
        /* [in] */ IWebView*,
        /* [in] */ IWebScriptCallFrame* frame,
        /* [in] */ int sourceID,
        /* [in] */ int lineNumber,
        /* [in] */ IWebFrame*);

    HRESULT STDMETHODCALLTYPE exceptionWasRaised(
        /* [in] */ IWebView*,
        /* [in] */ IWebScriptCallFrame*,
        /* [in] */ int sourceID,
        /* [in] */ int lineNumber,
        /* [in] */ IWebFrame*);

private:
    std::wstring m_currentServerName;

    // FIXME: make this a COMPtr when the Interface exists and the destructor can be called.
    IWebScriptCallFrame* m_currentFrame;
    COMPtr<IWebScriptDebugServer> m_server;
    JSGlobalContextRef m_globalContext;
};

#endif //ServerConnection_H

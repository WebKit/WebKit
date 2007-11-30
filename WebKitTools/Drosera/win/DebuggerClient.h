/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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

#ifndef DebuggerClient_H
#define DebuggerClient_H

#include "BaseDelegate.h"

#include <string>
#include <WebCore/COMPtr.h>
#include <wtf/OwnPtr.h>

class DebuggerDocument;
interface IWebView;
interface IWebFrame;
interface IWebViewPrivate;

typedef const struct OpaqueJSContext* JSContextRef;
typedef struct OpaqueJSValue* JSObjectRef;

class DebuggerClient : public BaseDelegate {
public:
    DebuggerClient();
    ~DebuggerClient();
    explicit DebuggerClient(const std::wstring& serverName);

    LRESULT DebuggerClient::onSize(WPARAM, LPARAM);

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(
        /* [in] */ REFIID riid,
        /* [retval][out] */ void** ppvObject);

    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

    // IWebFrameLoadDelegate
    HRESULT STDMETHODCALLTYPE didFinishLoadForFrame( 
        /* [in] */ IWebView*,
        /* [in] */ IWebFrame*);

    HRESULT STDMETHODCALLTYPE windowScriptObjectAvailable( 
        /* [in] */ IWebView*,
        /* [in] */ JSContextRef,
        /* [in] */ JSObjectRef);

    // IWebUIDelegate
    HRESULT STDMETHODCALLTYPE runJavaScriptAlertPanelWithMessage( 
        /* [in] */ IWebView*,
        /* [in] */ BSTR);

    HRESULT STDMETHODCALLTYPE createWebViewWithRequest( 
        /* [in] */ IWebView*,
        /* [in] */ IWebURLRequest*,
        /* [retval][out] */ IWebView**);

    bool webViewLoaded() const { return m_webViewLoaded; }

    // Pause & Step
    void resume();
    void pause();
    void stepInto();
    void stepOver();
    void stepOut();
    void showConsole();
    void closeCurrentFile();

    // Server Connection Functions
    bool serverConnected() const;
    void attemptToCreateServerConnection();

private:
    bool m_webViewLoaded;
    JSGlobalContextRef m_globalContext;

    HWND m_consoleWindow;
    COMPtr<IWebViewPrivate> m_webViewPrivate;

    OwnPtr<DebuggerDocument> m_debuggerDocument;
};

#endif //DebuggerClient_H

/*
 * Copyright (C) 2007, 2015 Apple Inc. All rights reserved.
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

#ifndef WebInspector_h
#define WebInspector_h

#include "WebKit.h"
#include <wtf/Noncopyable.h>

class WebInspectorClient;
class WebInspectorFrontendClient;
class WebView;

class WebInspector final : public IWebInspector, public IWebInspectorPrivate {
    WTF_MAKE_NONCOPYABLE(WebInspector);
public:
    static WebInspector* createInstance(WebView* inspectedWebView, WebInspectorClient*);

    void inspectedWebViewClosed();

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    virtual HRESULT STDMETHODCALLTYPE show();
    virtual HRESULT STDMETHODCALLTYPE showConsole();
    virtual HRESULT STDMETHODCALLTYPE unused1();
    virtual HRESULT STDMETHODCALLTYPE close();
    virtual HRESULT STDMETHODCALLTYPE attach();
    virtual HRESULT STDMETHODCALLTYPE detach();

    virtual HRESULT STDMETHODCALLTYPE isDebuggingJavaScript(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE toggleDebuggingJavaScript();

    virtual HRESULT STDMETHODCALLTYPE isProfilingJavaScript(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE toggleProfilingJavaScript();

    virtual HRESULT STDMETHODCALLTYPE evaluateInFrontend(_In_ BSTR script);

    virtual HRESULT STDMETHODCALLTYPE isTimelineProfilingEnabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setTimelineProfilingEnabled(BOOL);

private:
    WebInspector(WebView* inspectedWebView, WebInspectorClient*);
    ~WebInspector();

    WebInspectorFrontendClient* frontendClient();

    ULONG m_refCount { 0 };
    WebView* m_inspectedWebView { nullptr };
    WebInspectorClient* m_inspectorClient { nullptr };
};

#endif // !defined(WebInspector_h)

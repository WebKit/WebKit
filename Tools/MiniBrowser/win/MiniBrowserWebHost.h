/*
 * Copyright (C) 2006, 2014-2015 Apple Inc.  All rights reserved.
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

#include "resource.h"
#include <WebKitLegacy/WebKit.h>
#include <comutil.h>

class WebKitLegacyBrowserWindow;

class MiniBrowserWebHost : public IWebFrameLoadDelegate, public IWebFrameLoadDelegatePrivate, public IWebNotificationObserver {
public:
    MiniBrowserWebHost(WebKitLegacyBrowserWindow* client)
        : m_client(client) { }

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebFrameLoadDelegate
    virtual HRESULT STDMETHODCALLTYPE didStartProvisionalLoadForFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame*);
    virtual HRESULT STDMETHODCALLTYPE didReceiveServerRedirectForProvisionalLoadForFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame*) { return S_OK; }
    virtual HRESULT STDMETHODCALLTYPE didFailProvisionalLoadWithError(_In_opt_ IWebView*, _In_opt_ IWebError*, _In_opt_ IWebFrame*);
    virtual HRESULT STDMETHODCALLTYPE didCommitLoadForFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame*);
    virtual HRESULT STDMETHODCALLTYPE didReceiveTitle(_In_opt_ IWebView*, _In_ BSTR title, _In_opt_ IWebFrame*) { return S_OK; }
    virtual HRESULT STDMETHODCALLTYPE didChangeIcons(_In_opt_ IWebView*, _In_opt_ IWebFrame*) { return S_OK; }
    virtual HRESULT STDMETHODCALLTYPE didReceiveIcon(_In_opt_ IWebView*, _In_ HBITMAP, _In_opt_ IWebFrame*) { return S_OK; }
    virtual HRESULT STDMETHODCALLTYPE didFinishLoadForFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame*);
    virtual HRESULT STDMETHODCALLTYPE didFailLoadWithError(_In_opt_ IWebView*, _In_opt_ IWebError*, _In_opt_ IWebFrame*);
    virtual HRESULT STDMETHODCALLTYPE didChangeLocationWithinPageForFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame*);
    virtual HRESULT STDMETHODCALLTYPE willPerformClientRedirectToURL(_In_opt_ IWebView*, _In_ BSTR url, double delaySeconds, DATE fireDate, _In_opt_ IWebFrame*) { return S_OK; }
    virtual HRESULT STDMETHODCALLTYPE didCancelClientRedirectForFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame*) { return S_OK; }
    virtual HRESULT STDMETHODCALLTYPE willCloseFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame*) { return S_OK; }
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE windowScriptObjectAvailable(IWebView*, JSContextRef, JSObjectRef)  { return S_OK; }
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE didClearWindowObject(IWebView*, JSContextRef, JSObjectRef, IWebFrame*) { return S_OK; }

    // IWebFrameLoadDelegatePrivate
    virtual HRESULT STDMETHODCALLTYPE didFinishDocumentLoadForFrame(_In_opt_ IWebView* sender, _In_opt_ IWebFrame*) { return S_OK; }
    virtual HRESULT STDMETHODCALLTYPE didFirstLayoutInFrame(_In_opt_ IWebView* sender, _In_opt_ IWebFrame*);
    virtual HRESULT STDMETHODCALLTYPE didHandleOnloadEventsForFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame*);
    virtual HRESULT STDMETHODCALLTYPE didFirstVisuallyNonEmptyLayoutInFrame(_In_opt_ IWebView* sender, _In_opt_ IWebFrame*)  { return S_OK; }

    // IWebNotificationObserver
    virtual HRESULT STDMETHODCALLTYPE onNotify(_In_opt_ IWebNotification*);

private:
    WebKitLegacyBrowserWindow* m_client { nullptr };
};

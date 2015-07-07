/*
 * Copyright (C) 2006, 2014 Apple Inc.  All rights reserved.
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
#include <WebKit/WebKit.h>

class WinLauncher;

class WinLauncherWebHost : public IWebFrameLoadDelegate {
public:
    WinLauncherWebHost(WinLauncher* client, HWND urlBar)
        : m_refCount(1), m_client(client), m_hURLBarWnd(urlBar) { }

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebFrameLoadDelegate
    virtual HRESULT STDMETHODCALLTYPE didStartProvisionalLoadForFrame(IWebView*, IWebFrame*) { return S_OK; }
    virtual HRESULT STDMETHODCALLTYPE didReceiveServerRedirectForProvisionalLoadForFrame(IWebView*, IWebFrame*) { return S_OK; }
    virtual HRESULT STDMETHODCALLTYPE didFailProvisionalLoadWithError(IWebView*, IWebError*, IWebFrame*);
    virtual HRESULT STDMETHODCALLTYPE didCommitLoadForFrame(IWebView* webView, IWebFrame*)
    {
        if (!webView)
            return E_POINTER;

        return updateAddressBar(*webView);
    }
    
    virtual HRESULT STDMETHODCALLTYPE didReceiveTitle(IWebView*, BSTR title, IWebFrame*) { return S_OK; }
    virtual HRESULT STDMETHODCALLTYPE didChangeIcons(IWebView*, IWebFrame*) { return S_OK; }
    virtual HRESULT STDMETHODCALLTYPE didReceiveIcon(IWebView*, HBITMAP, IWebFrame*) { return S_OK; }
    virtual HRESULT STDMETHODCALLTYPE didFinishLoadForFrame(IWebView*, IWebFrame*);   
    virtual HRESULT STDMETHODCALLTYPE didFailLoadWithError(IWebView*, IWebError*, IWebFrame*) { return S_OK; }
    virtual HRESULT STDMETHODCALLTYPE didChangeLocationWithinPageForFrame(IWebView*, IWebFrame*) { return S_OK; }
    virtual HRESULT STDMETHODCALLTYPE willPerformClientRedirectToURL(IWebView*, BSTR url, double delaySeconds, DATE fireDate, IWebFrame*) { return S_OK; }
    virtual HRESULT STDMETHODCALLTYPE didCancelClientRedirectForFrame(IWebView*, IWebFrame*) { return S_OK; }
    virtual HRESULT STDMETHODCALLTYPE willCloseFrame(IWebView*, IWebFrame*) { return S_OK; }
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE windowScriptObjectAvailable(IWebView*, JSContextRef, JSObjectRef)  { return S_OK; }
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE didClearWindowObject(IWebView*, JSContextRef, JSObjectRef, IWebFrame*) { return S_OK; }

protected:
    HRESULT updateAddressBar(IWebView&);

private:
    HWND m_hURLBarWnd;
    ULONG m_refCount;
    WinLauncher* m_client;
};

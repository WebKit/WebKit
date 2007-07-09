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

#ifndef Drosera_H
#define Drosera_H

#include "BaseDelegate.h"

#include <WebCore/COMPtr.h>
#include <WebKit/IWebView.h>
#include <WebKit/IWebViewPrivate.h>

class Drosera : BaseDelegate {
public:
    Drosera();
    HRESULT initUI(HINSTANCE hInstance, int nCmdShow);
    bool webViewLoaded() const { return m_webViewLoaded; }

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

    // IWebNotificationObserver
    HRESULT STDMETHODCALLTYPE onNotify(
        /* [in] */ IWebNotification*);

    LRESULT onSize(WPARAM, LPARAM);

    static HINSTANCE getInst() { return m_hInst; } const
    static void setInst(HINSTANCE in) { m_hInst = in; }
private:

    HWND m_hWnd;

    COMPtr<IWebView> m_webView;
    COMPtr<IWebViewPrivate> m_webViewPrivate;
    bool m_webViewLoaded;

    static HINSTANCE m_hInst;
};

#endif //HelperFunctions_H

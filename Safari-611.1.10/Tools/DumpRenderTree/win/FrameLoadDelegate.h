/*
 * Copyright (C) 2005-2007, 2015 Apple Inc.  All rights reserved.
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

#ifndef FrameLoadDelegate_h
#define FrameLoadDelegate_h

#include <WebKitLegacy/WebKit.h>

class AccessibilityController;
class TextInputController;
class GCController;

class FrameLoadDelegate : public IWebFrameLoadDelegate, public IWebFrameLoadDelegatePrivate2, public IWebNotificationObserver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FrameLoadDelegate();
    virtual ~FrameLoadDelegate();

    void processWork();

    void resetToConsistentState();

    AccessibilityController* accessibilityController() const { return m_accessibilityController.get(); }

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebFrameLoadDelegate
    virtual HRESULT STDMETHODCALLTYPE didStartProvisionalLoadForFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame*);
    virtual HRESULT STDMETHODCALLTYPE didReceiveServerRedirectForProvisionalLoadForFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame*);
    virtual HRESULT STDMETHODCALLTYPE didFailProvisionalLoadWithError(_In_opt_ IWebView*, _In_opt_ IWebError*, _In_opt_ IWebFrame*);
    virtual HRESULT STDMETHODCALLTYPE didCommitLoadForFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame*);
    virtual HRESULT STDMETHODCALLTYPE didReceiveTitle(_In_opt_ IWebView*, _In_ BSTR title, _In_opt_ IWebFrame*);
    virtual HRESULT STDMETHODCALLTYPE didChangeIcons(_In_opt_ IWebView*, _In_opt_ IWebFrame*);
    virtual HRESULT STDMETHODCALLTYPE didReceiveIcon(_In_opt_ IWebView*, _In_ HBITMAP, _In_opt_ IWebFrame*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE didFinishLoadForFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame*);
    virtual HRESULT STDMETHODCALLTYPE didFailLoadWithError(_In_opt_ IWebView*, _In_opt_ IWebError*, _In_opt_ IWebFrame*);
    virtual HRESULT STDMETHODCALLTYPE didChangeLocationWithinPageForFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame*);
    virtual HRESULT STDMETHODCALLTYPE willPerformClientRedirectToURL(_In_opt_ IWebView*, _In_ BSTR url, double delaySeconds, DATE fireDate, _In_opt_ IWebFrame*);
    virtual HRESULT STDMETHODCALLTYPE didCancelClientRedirectForFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame*);
    virtual HRESULT STDMETHODCALLTYPE willCloseFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame*);
    virtual HRESULT STDMETHODCALLTYPE windowScriptObjectAvailable(IWebView*, JSContextRef, JSObjectRef windowObject);
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE didClearWindowObject(IWebView*, JSContextRef, JSObjectRef windowObject, IWebFrame*);

    // IWebFrameLoadDelegatePrivate
    virtual HRESULT STDMETHODCALLTYPE didFinishDocumentLoadForFrame(_In_opt_ IWebView* sender, _In_opt_ IWebFrame*);      
    virtual HRESULT STDMETHODCALLTYPE didFirstLayoutInFrame(_In_opt_ IWebView* sender, _In_opt_ IWebFrame*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE didHandleOnloadEventsForFrame(_In_opt_ IWebView* sender, _In_opt_ IWebFrame*);
    virtual HRESULT STDMETHODCALLTYPE didFirstVisuallyNonEmptyLayoutInFrame(_In_opt_ IWebView* sender, _In_opt_ IWebFrame*);

    // IWebFrameLoadDelegatePrivate2
    virtual HRESULT STDMETHODCALLTYPE didDisplayInsecureContent(_In_opt_ IWebView*);
    virtual HRESULT STDMETHODCALLTYPE didRunInsecureContent(_In_opt_ IWebView*, _In_opt_ IWebSecurityOrigin*);
    virtual HRESULT STDMETHODCALLTYPE didClearWindowObjectForFrameInScriptWorld(_In_opt_ IWebView*, _In_opt_ IWebFrame*, _In_opt_ IWebScriptWorld*);
    virtual HRESULT STDMETHODCALLTYPE didPushStateWithinPageForFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame*) { return E_NOTIMPL; }    
    virtual HRESULT STDMETHODCALLTYPE didReplaceStateWithinPageForFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE didPopStateWithinPageForFrame(_In_opt_ IWebView*, _In_opt_ IWebFrame*) { return E_NOTIMPL; }

    // IWebNotificationObserver
    virtual HRESULT STDMETHODCALLTYPE onNotify(_In_opt_ IWebNotification*);

private:
    void didClearWindowObjectForFrameInIsolatedWorld(IWebFrame*, IWebScriptWorld*);
    void didClearWindowObjectForFrameInStandardWorld(IWebFrame*);

    void locationChangeDone(IWebError*, IWebFrame*);
    void webViewProgressFinishedNotification();

    std::unique_ptr<GCController> m_gcController;
    std::unique_ptr<AccessibilityController> m_accessibilityController;
    std::unique_ptr<TextInputController> m_textInputController;
    ULONG m_refCount { 1 };
};

#endif // FrameLoadDelegate_h

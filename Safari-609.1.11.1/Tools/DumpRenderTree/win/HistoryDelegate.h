/*
 * Copyright (C) 2009 Apple Inc.  All rights reserved.
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

#ifndef HistoryDelegate_h
#define HistoryDelegate_h

#include <WebKitLegacy/WebKit.h>

class HistoryDelegate : public IWebHistoryDelegate {
    WTF_MAKE_FAST_ALLOCATED;
public:
    HistoryDelegate();
    virtual ~HistoryDelegate();

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebHistoryDelegate
    virtual HRESULT STDMETHODCALLTYPE didNavigateWithNavigationData(_In_opt_ IWebView*, _In_opt_ IWebNavigationData*, _In_opt_ IWebFrame*);
    virtual HRESULT STDMETHODCALLTYPE didPerformClientRedirectFromURL(_In_opt_ IWebView*, _In_ BSTR sourceURL, _In_ BSTR destinationURL, _In_opt_ IWebFrame*);    
    virtual HRESULT STDMETHODCALLTYPE didPerformServerRedirectFromURL(_In_opt_ IWebView*, _In_ BSTR sourceURL, _In_ BSTR destinationURL, _In_opt_ IWebFrame*);
    virtual HRESULT STDMETHODCALLTYPE updateHistoryTitle(_In_opt_ IWebView*, _In_ BSTR title, _In_ BSTR url);    
    virtual HRESULT STDMETHODCALLTYPE populateVisitedLinksForWebView(_In_opt_ IWebView*);

private:
    ULONG m_refCount { 1 };
};

#endif // HistoryDelegate_h

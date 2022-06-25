/*
 * Copyright (C) 2007-2008, 2015 Apple Inc. All rights reserved.
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

#ifndef WebActionPropertyBag_h
#define WebActionPropertyBag_h

#include "ocidl.h"
#include <WebCore/Frame.h>
#include <WebCore/HTMLFormElement.h>
#include <WebCore/NavigationAction.h>

class WebActionPropertyBag final : public IPropertyBag {
public:
    static WebActionPropertyBag* createInstance(const WebCore::NavigationAction&, RefPtr<WebCore::HTMLFormElement>&&, RefPtr<WebCore::Frame>&&);

private:
    WebActionPropertyBag(const WebCore::NavigationAction&, RefPtr<WebCore::HTMLFormElement>&&, RefPtr<WebCore::Frame>&&);
    ~WebActionPropertyBag();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IPropertyBag
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Read(/* [in] */ LPCOLESTR pszPropName, /* [out][in] */ VARIANT*, /* [in] */ IErrorLog*);   
    virtual HRESULT STDMETHODCALLTYPE Write(_In_ LPCOLESTR pszPropName, _In_ VARIANT* pVar);

private:
    ULONG m_refCount { 0 };
    WebCore::NavigationAction m_action;
    RefPtr<WebCore::HTMLFormElement> m_form;
    RefPtr<WebCore::Frame> m_frame;
};

#endif

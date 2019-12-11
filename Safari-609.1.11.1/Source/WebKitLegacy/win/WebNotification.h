/*
 * Copyright (C) 2006-2007, 2015 Apple Inc.  All rights reserved.
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

#ifndef WebNotification_H
#define WebNotification_H

#include "WebKit.h"

class WebNotification final : public IWebNotification
{
public:
    static WebNotification* createInstance(BSTR name = 0, IUnknown* anObject = 0, IPropertyBag* userInfo = 0);
protected:
    WebNotification(BSTR name, IUnknown* anObject, IPropertyBag* userInfo);
    ~WebNotification();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebNotification
    virtual HRESULT STDMETHODCALLTYPE notificationWithName(_In_ BSTR aName, _In_opt_ IUnknown* anObject, _In_opt_ IPropertyBag* userInfo);    
    virtual HRESULT STDMETHODCALLTYPE name(__deref_out_opt BSTR* result);    
    virtual HRESULT STDMETHODCALLTYPE getObject(_COM_Outptr_opt_ IUnknown** result);
    virtual HRESULT STDMETHODCALLTYPE userInfo(_COM_Outptr_opt_ IPropertyBag ** result);

protected:
    ULONG m_refCount { 0 };
    BSTR m_name { nullptr };
    IUnknown* m_anObject;
    IPropertyBag* m_userInfo;
};

#endif

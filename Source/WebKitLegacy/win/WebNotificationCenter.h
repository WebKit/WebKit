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

#ifndef WebNotificationCenter_H
#define WebNotificationCenter_H

#include "WebKit.h"

struct WebNotificationCenterPrivate;

class WebNotificationCenter final : public IWebNotificationCenter {
public:
    static WebNotificationCenter* createInstance();

protected:
    WebNotificationCenter();
    ~WebNotificationCenter();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebNotificationCenter
    virtual HRESULT STDMETHODCALLTYPE defaultCenter(_COM_Outptr_opt_ IWebNotificationCenter**);
    virtual HRESULT STDMETHODCALLTYPE addObserver(_In_opt_ IWebNotificationObserver*, _In_ BSTR notificationName, _In_opt_ IUnknown* anObject);
    virtual HRESULT STDMETHODCALLTYPE postNotification(_In_opt_ IWebNotification*);
    virtual HRESULT STDMETHODCALLTYPE postNotificationName(_In_ BSTR notificationName, _In_opt_ IUnknown* anObject, _In_opt_ IPropertyBag* userInfo);
    virtual HRESULT STDMETHODCALLTYPE removeObserver(_In_opt_ IWebNotificationObserver*, _In_ BSTR notificationName, _In_opt_ IUnknown* anObject);

    // WebNotificationCenter
    static IWebNotificationCenter* defaultCenterInternal();
    void postNotificationInternal(IWebNotification* notification, BSTR notificationName, IUnknown* anObject);

protected:
    ULONG m_refCount { 0 };
    std::unique_ptr<WebNotificationCenterPrivate> d;
    static IWebNotificationCenter* m_defaultCenter;
};

#endif // WebNotificationCenter_H

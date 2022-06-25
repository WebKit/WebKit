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

#ifndef DRTDesktopNotificationPresenter_h
#define DRTDesktopNotificationPresenter_h

#include <WebKitLegacy/WebKit.h>
#include <windef.h>

class DRTDesktopNotificationPresenter final : public IWebDesktopNotificationsDelegate {
    WTF_MAKE_FAST_ALLOCATED;
public:
    DRTDesktopNotificationPresenter();

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebDesktopNotificationsDelegate
    virtual HRESULT STDMETHODCALLTYPE showDesktopNotification(_In_opt_ IWebDesktopNotification*);
    virtual HRESULT STDMETHODCALLTYPE cancelDesktopNotification(_In_opt_ IWebDesktopNotification*);
    virtual HRESULT STDMETHODCALLTYPE notificationDestroyed(_In_opt_ IWebDesktopNotification*);
    virtual HRESULT STDMETHODCALLTYPE checkNotificationPermission(_In_ BSTR origin, _Out_ int* result);
    virtual HRESULT STDMETHODCALLTYPE requestNotificationPermission(_In_ BSTR origin);

private:
    ULONG m_refCount { 1 };
};

#endif

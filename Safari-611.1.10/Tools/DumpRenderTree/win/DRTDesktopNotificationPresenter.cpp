/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2014-2015 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DRTDesktopNotificationPresenter.h"

#include "DumpRenderTree.h"
#include "TestRunner.h"
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSStringRefBSTR.h>
#include <WebCore/NotificationClient.h>
#include <comutil.h>

DRTDesktopNotificationPresenter::DRTDesktopNotificationPresenter()
{
} 

HRESULT DRTDesktopNotificationPresenter::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<DRTDesktopNotificationPresenter*>(this);
    else if (IsEqualGUID(riid, IID_IWebDesktopNotificationsDelegate))
        *ppvObject = static_cast<DRTDesktopNotificationPresenter*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG DRTDesktopNotificationPresenter::AddRef()
{
    return ++m_refCount;
}

ULONG DRTDesktopNotificationPresenter::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

HRESULT DRTDesktopNotificationPresenter::showDesktopNotification(_In_opt_ IWebDesktopNotification* notification)
{
    _bstr_t title, text, url;
    BOOL html;

    if (SUCCEEDED(notification->isHTML(&html)) && html) {
        notification->contentsURL(&url.GetBSTR());    
        fprintf(testResult, "DESKTOP NOTIFICATION: contents at %S\n", static_cast<wchar_t*>(url));
    } else {
        notification->iconURL(&url.GetBSTR());
        notification->title(&title.GetBSTR());
        notification->text(&text.GetBSTR());
        fprintf(testResult, "DESKTOP NOTIFICATION: icon %S, title %S, text %S\n", static_cast<wchar_t*>(url), static_cast<wchar_t*>(title), static_cast<wchar_t*>(text));
    }

    // In this stub implementation, the notification is displayed immediately;
    // we dispatch the display event to mimic that.
    notification->notifyDisplay();

    return S_OK;
}

HRESULT DRTDesktopNotificationPresenter::cancelDesktopNotification(_In_opt_ IWebDesktopNotification* notification)
{
    _bstr_t identifier;
    BOOL html;
    notification->isHTML(&html);
    if (html)
        notification->contentsURL(&identifier.GetBSTR());
    else
        notification->title(&identifier.GetBSTR());

    fprintf(testResult, "DESKTOP NOTIFICATION CLOSED: %S\n", static_cast<wchar_t*>(identifier));
    notification->notifyClose(false);

    return S_OK;
}

HRESULT DRTDesktopNotificationPresenter::notificationDestroyed(_In_opt_ IWebDesktopNotification* /*notification*/)
{
    // Since in these tests events happen immediately, we don't hold on to
    // Notification pointers.  So there's no cleanup to do.
    return S_OK;
}

HRESULT DRTDesktopNotificationPresenter::checkNotificationPermission(_In_ BSTR /*origin*/, _Out_ int* result)
{
    if (!result)
        return E_POINTER;

    *result = 0;

#if ENABLE(NOTIFICATIONS)
    JSStringRef jsOrigin = JSStringCreateWithBSTR(origin);
    bool allowed = ::gTestRunner->checkDesktopNotificationPermission(jsOrigin);

    if (allowed)
        *result = WebCore::NotificationClient::PermissionAllowed;
    else
        *result = WebCore::NotificationClient::PermissionDenied;

    JSStringRelease(jsOrigin);
#endif
    return S_OK;
}

HRESULT DRTDesktopNotificationPresenter::requestNotificationPermission(_In_ BSTR origin)
{
    fprintf(testResult, "DESKTOP NOTIFICATION PERMISSION REQUESTED: %S\n", origin ? origin : L"");
    return S_OK;
}

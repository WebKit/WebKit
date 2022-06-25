/*
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 * Copyright (C) 2014-2015 Apple Inc.  All rights reserved.
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

#include "WebInspectorDelegate.h"

WebInspectorDelegate::WebInspectorDelegate()
{
}

WebInspectorDelegate* WebInspectorDelegate::createInstance()
{
    WebInspectorDelegate* instance = new WebInspectorDelegate;
    instance->AddRef();
    return instance;
}

HRESULT WebInspectorDelegate::QueryInterface(_In_ REFIID, _COM_Outptr_ void** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
};

ULONG WebInspectorDelegate::AddRef()
{
    return ++m_refCount;
}

ULONG WebInspectorDelegate::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

HRESULT WebInspectorDelegate::dragDestinationActionMaskForDraggingInfo(_In_opt_ IWebView*, _In_opt_ IDataObject*, _Out_ WebDragDestinationAction* action)
{
    if (!action)
        return E_POINTER;

    *action = WebDragDestinationActionNone;

    return S_OK;
}

HRESULT WebInspectorDelegate::createWebViewWithRequest(_In_opt_ IWebView*, _In_opt_ IWebURLRequest*, _COM_Outptr_opt_ IWebView** webView)
{
    if (!webView)
        return E_POINTER;
    *webView = nullptr;
    return E_NOTIMPL;
}

HRESULT WebInspectorDelegate::willPerformDragSourceAction(_In_opt_ IWebView*, WebDragSourceAction, _In_ LPPOINT, _In_opt_ IDataObject*, _COM_Outptr_opt_ IDataObject** dataObject)
{
    if (!dataObject)
        return E_POINTER;
    *dataObject = nullptr;
    return E_NOTIMPL;
}

HRESULT WebInspectorDelegate::createModalDialog(_In_opt_ IWebView*, _In_opt_ IWebURLRequest*, _COM_Outptr_opt_ IWebView** dialog)
{
    if (!dialog)
        return E_POINTER;
    *dialog = nullptr;
    return E_NOTIMPL;
}

HRESULT WebInspectorDelegate::desktopNotificationsDelegate(_COM_Outptr_opt_ IWebDesktopNotificationsDelegate** notificationDelegate)
{
    if (!notificationDelegate)
        return E_POINTER;
    *notificationDelegate = nullptr;
    return E_NOTIMPL;
}

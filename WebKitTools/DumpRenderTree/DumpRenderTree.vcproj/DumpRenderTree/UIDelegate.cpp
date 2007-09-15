/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc.  All rights reserved.
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

#include "DumpRenderTree.h"
#include "UIDelegate.h"

#include "DraggingInfo.h"
#include "EventSender.h"

#include <WebCore/COMPtr.h>
#include <wtf/Platform.h>
#include <JavaScriptCore/Assertions.h>
#include <JavaScriptCore/JavaScriptCore.h>
#include <WebKit/IWebFramePrivate.h>
#include <WebKit/IWebViewPrivate.h>
#include <stdio.h>

HRESULT STDMETHODCALLTYPE UIDelegate::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebUIDelegate*>(this);
    else if (IsEqualGUID(riid, IID_IWebUIDelegate))
        *ppvObject = static_cast<IWebUIDelegate*>(this);
    else if (IsEqualGUID(riid, IID_IWebUIDelegatePrivate))
        *ppvObject = static_cast<IWebUIDelegatePrivate*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE UIDelegate::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE UIDelegate::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

HRESULT STDMETHODCALLTYPE UIDelegate::hasCustomMenuImplementation( 
        /* [retval][out] */ BOOL *hasCustomMenus)
{
    *hasCustomMenus = FALSE;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE UIDelegate::setFrame( 
        /* [in] */ IWebView* /*sender*/,
        /* [in] */ RECT* frame)
{
    m_frame = frame;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE UIDelegate::webViewFrame( 
        /* [in] */ IWebView* /*sender*/,
        /* [retval][out] */ RECT* frame)
{
    frame = m_frame;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE UIDelegate::runJavaScriptAlertPanelWithMessage( 
        /* [in] */ IWebView* /*sender*/,
        /* [in] */ BSTR message)
{
    wprintf(L"ALERT: %s\n", message ? message : L"");

    return S_OK;
}

HRESULT STDMETHODCALLTYPE UIDelegate::webViewAddMessageToConsole( 
    /* [in] */ IWebView* sender,
    /* [in] */ BSTR message,
    /* [in] */ int lineNumber,
    /* [in] */ BSTR url,
    /* [in] */ BOOL isError)
{
    wprintf(L"CONSOLE MESSAGE: line %d: %s\n", lineNumber, message ? message : L"");

    return S_OK;
}

HRESULT STDMETHODCALLTYPE UIDelegate::doDragDrop( 
    /* [in] */ IWebView* sender,
    /* [in] */ IDataObject* object,
    /* [in] */ IDropSource* source,
    /* [in] */ DWORD okEffect,
    /* [retval][out] */ DWORD* performedEffect)
{
    if (!performedEffect)
        return E_POINTER;

    *performedEffect = 0;

    draggingInfo = new DraggingInfo(object, source);

    replaySavedEvents();

    return S_OK;
}

HRESULT STDMETHODCALLTYPE UIDelegate::webViewGetDlgCode( 
    /* [in] */ IWebView* /*sender*/,
    /* [in] */ UINT /*keyCode*/,
    /* [retval][out] */ LONG_PTR *code)
{
    if (!code)
        return E_POINTER;
    *code = 0;
    return E_NOTIMPL;
}

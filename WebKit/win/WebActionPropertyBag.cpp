/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "WebKitDLL.h"

#include "IWebView.h"
#include "IWebPolicyDelegate.h"
#include "WebActionPropertyBag.h"
#include "WebElementPropertyBag.h"
#include "COMPtr.h"

#pragma warning(push, 0)
#include <WebCore/BString.h>
#include <WebCore/EventHandler.h>
#include <WebCore/MouseEvent.h>
#include <WebCore/HitTestResult.h>
#pragma warning(pop)

using namespace WebCore;

// WebActionPropertyBag ------------------------------------------------
WebActionPropertyBag::WebActionPropertyBag(const NavigationAction& action, Frame* frame)
    : m_refCount(0)
    , m_action(action) 
    , m_frame(frame)
{
}

WebActionPropertyBag::~WebActionPropertyBag()
{
}

WebActionPropertyBag* WebActionPropertyBag::createInstance(const NavigationAction& action, Frame* frame)
{
    WebActionPropertyBag* instance = new WebActionPropertyBag(action, frame); 
    instance->AddRef();

    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebActionPropertyBag::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IPropertyBag*>(this);
    else if (IsEqualGUID(riid, IID_IPropertyBag))
        *ppvObject = static_cast<IPropertyBag*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebActionPropertyBag::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebActionPropertyBag::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete this;

    return newRef;
}

static bool isEqual(LPCWSTR s1, LPCWSTR s2)
{
    return !wcscmp(s1, s2);
}

static const MouseEvent* findMouseEvent(const Event* event)
{
    for (const Event* e = event; e; e = e->underlyingEvent())
        if (e->isMouseEvent())
            return static_cast<const MouseEvent*>(e);
    return 0;
}

HRESULT STDMETHODCALLTYPE WebActionPropertyBag::Read(LPCOLESTR pszPropName, VARIANT *pVar, IErrorLog * /*pErrorLog*/)
{
    if (!pszPropName)
        return E_POINTER;

    VariantClear(pVar);

    if (isEqual(pszPropName, WebActionNavigationTypeKey)) {
        V_VT(pVar) = VT_I4;
        V_I4(pVar) = m_action.type();
        return S_OK;
    } else if (isEqual(pszPropName, WebActionElementKey)) {
        if (const MouseEvent* mouseEvent = findMouseEvent(m_action.event())) {
            IntPoint point(mouseEvent->clientX(), mouseEvent->clientY());
            COMPtr<WebElementPropertyBag> elementPropertyBag;
            elementPropertyBag.adoptRef(WebElementPropertyBag::createInstance(m_frame->eventHandler()->hitTestResultAtPoint(point, false)));

            V_VT(pVar) = VT_UNKNOWN;
            elementPropertyBag->QueryInterface(IID_IUnknown, (void**)V_UNKNOWNREF(pVar));
            return S_OK;
        }
    } else if (isEqual(pszPropName, WebActionButtonKey)) {
        if (const MouseEvent* mouseEvent = findMouseEvent(m_action.event())) {
            V_VT(pVar) = VT_I4;
            V_I4(pVar) = mouseEvent->button();
            return S_OK;
        }
    } else if (isEqual(pszPropName, WebActionOriginalURLKey)) {
        V_VT(pVar) = VT_BSTR;
        V_BSTR(pVar) = BString(m_action.URL().url()).release();
        return S_OK;
    } else if (isEqual(pszPropName, WebActionModifierFlagsKey)) {
        if (const UIEventWithKeyState* keyEvent = findEventWithKeyState(const_cast<Event*>(m_action.event()))) {
            int modifiers = 0;

            if (keyEvent->ctrlKey())
                modifiers |= MK_CONTROL;
            if (keyEvent->shiftKey())
                modifiers |= MK_SHIFT;
            if (keyEvent->altKey())
                modifiers |= MK_ALT;

            V_VT(pVar) = VT_I4;
            V_I4(pVar) = modifiers;
            return S_OK;
        }
    }
    return E_INVALIDARG;
}

HRESULT STDMETHODCALLTYPE WebActionPropertyBag::Write(LPCOLESTR pszPropName, VARIANT* pVar)
{
    if (!pszPropName || !pVar)
        return E_POINTER;
    VariantClear(pVar);
    return E_FAIL;
}

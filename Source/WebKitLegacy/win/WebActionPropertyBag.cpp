/*
 * Copyright (C) 2007, 2008, 2015 Apple Inc. All rights reserved.
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

#include "WebKitDLL.h"
#include "WebActionPropertyBag.h"

#include "DOMCoreClasses.h"
#include "WebElementPropertyBag.h"
#include "WebKit.h"
#include <WebCore/BString.h>
#include <WebCore/COMPtr.h>
#include <WebCore/EventHandler.h>
#include <WebCore/HitTestResult.h>
#include <WebCore/MouseEvent.h>

using namespace WebCore;

// WebActionPropertyBag ------------------------------------------------

WebActionPropertyBag::WebActionPropertyBag(const NavigationAction& action, RefPtr<HTMLFormElement>&& form, RefPtr<Frame>&& frame)
    : m_action(action)
    , m_form(WTFMove(form))
    , m_frame(WTFMove(frame))
{
    gClassCount++;
    gClassNameCount().add("WebActionPropertyBag");
}

WebActionPropertyBag::~WebActionPropertyBag()
{
    gClassCount--;
    gClassNameCount().remove("WebActionPropertyBag");
}

WebActionPropertyBag* WebActionPropertyBag::createInstance(const NavigationAction& action, RefPtr<HTMLFormElement>&& form, RefPtr<Frame>&& frame)
{
    WebActionPropertyBag* instance = new WebActionPropertyBag(action, WTFMove(form), WTFMove(frame));
    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT WebActionPropertyBag::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IPropertyBag*>(this);
    else if (IsEqualGUID(riid, IID_IPropertyBag))
        *ppvObject = static_cast<IPropertyBag*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebActionPropertyBag::AddRef()
{
    return ++m_refCount;
}

ULONG WebActionPropertyBag::Release()
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

HRESULT WebActionPropertyBag::Read(LPCOLESTR pszPropName, VARIANT *pVar, IErrorLog * /*pErrorLog*/)
{
    if (!pszPropName)
        return E_POINTER;

    ::VariantClear(pVar);

    if (isEqual(pszPropName, WebActionNavigationTypeKey)) {
        V_VT(pVar) = VT_I4;
        V_I4(pVar) = static_cast<LONG>(m_action.type());
        return S_OK;
    }
    if (isEqual(pszPropName, WebActionElementKey)) {
        if (auto mouseEvent = m_action.mouseEventData()) {
            V_VT(pVar) = VT_UNKNOWN;
            V_UNKNOWN(pVar) = WebElementPropertyBag::createInstance(m_frame->eventHandler().hitTestResultAtPoint(mouseEvent->absoluteLocation, HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowUserAgentShadowContent | HitTestRequest::AllowChildFrameContent));
            return S_OK;
        }
    }
    if (isEqual(pszPropName, WebActionButtonKey)) {
        if (auto mouseEvent = m_action.mouseEventData()) {
            V_VT(pVar) = VT_I4;
            V_I4(pVar) = mouseEvent->button;
            return S_OK;
        }
    }
    if (isEqual(pszPropName, WebActionOriginalURLKey)) {
        V_VT(pVar) = VT_BSTR;
        V_BSTR(pVar) = BString(m_action.url().string()).release();
        return S_OK;
    }
    if (isEqual(pszPropName, WebActionModifierFlagsKey)) {
        if (auto keyEvent = m_action.keyStateEventData()) {
            unsigned modifiers = 0;

            if (keyEvent->ctrlKey)
                modifiers |= MK_CONTROL;
            if (keyEvent->shiftKey)
                modifiers |= MK_SHIFT;
            if (keyEvent->altKey)
                modifiers |= MK_ALT;

            V_VT(pVar) = VT_UI4;
            V_UI4(pVar) = modifiers;
            return S_OK;
        }
    }
    if (isEqual(pszPropName, WebActionFormKey)) {
        IDOMNode* form = DOMNode::createInstance(m_form.get());
        V_VT(pVar) = VT_UNKNOWN;
        V_UNKNOWN(pVar) = form;
        return S_OK;
    }
    return E_INVALIDARG;
}

HRESULT WebActionPropertyBag::Write(_In_ LPCOLESTR pszPropName, _In_ VARIANT* pVar)
{
    if (!pszPropName || !pVar)
        return E_POINTER;

    return E_NOTIMPL;
}

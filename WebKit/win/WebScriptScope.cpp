/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "WebKitDLL.h"
#include "WebScriptScope.h"

#include "COMEnumVariant.h"
#include "WebScriptCallFrame.h"

#include <memory>
#include <JavaScriptCore/PropertyNameArray.h>
#include <wtf/Assertions.h>
#include <wtf/OwnPtr.h>

#pragma warning(push, 0)
#include <WebCore/BString.h>
#include <WebCore/PlatformString.h>
#pragma warning(pop)

using namespace KJS;
using namespace std;

// WebScriptScope ------------------------------------------------------------

WebScriptScope::WebScriptScope(JSObject* scope)
    : m_refCount(0)
    , m_scope(scope)
{
    gClassCount++;
}

WebScriptScope::~WebScriptScope()
{
    gClassCount--;
}

WebScriptScope* WebScriptScope::createInstance(JSObject* scope)
{
    WebScriptScope* instance = new WebScriptScope(scope);
    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebScriptScope::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebScriptScope*>(this);
    else if (IsEqualGUID(riid, IID_IWebScriptScope))
        *ppvObject = static_cast<IWebScriptScope*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebScriptScope::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebScriptScope::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

template<> struct COMVariantSetter<Identifier>
{
    static void setVariant(VARIANT* variant, const Identifier& value)
    {
        ASSERT(V_VT(variant) == VT_EMPTY);

        V_VT(variant) = VT_BSTR;
        V_BSTR(variant) = WebCore::BString(reinterpret_cast<const wchar_t*>(value.data()), value.size()).release();
    }
};

// WebScriptScope ------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebScriptScope::variableNames(
    /* [in] */ IWebScriptCallFrame* frame,
    /* [out, retval] */ IEnumVARIANT** variableNames)
{
    if (!variableNames)
        return E_POINTER;

    *variableNames = 0;

    if (!frame)
        return E_FAIL;

    PropertyNameArray propertyNames;
    m_scope->getPropertyNames(frame->impl()->state(), propertyNames);

    *variableNames = COMEnumVariant<PropertyNameArray>::adopt(propertyNames);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebScriptScope::valueForVariable(
    /* [in] */ IWebScriptCallFrame* frame,
    /* [in] */ BSTR key,
    /* [out, retval] */ BSTR* value)
{
    if (!frame || !key)
        return E_FAIL;

    if (!value)
        return E_POINTER;

    *value = 0;

    Identifier identKey(reinterpret_cast<KJS::UChar*>(key), SysStringLen(key));

    JSValue* jsvalue = m_scope->get(frame->impl()->state(), identKey);

    UString string;
    const JSType type = jsvalue->type();
    switch (type) {
        case NullType:
        case UndefinedType:
        case UnspecifiedType:
        case GetterSetterType:
            break;
        case StringType:
            string = jsvalue->getString();
            break;
        case NumberType:
            string = UString::from(jsvalue->getNumber());
            break;
        case BooleanType:
            string = jsvalue->getBoolean() ? "True" : "False";
            break;
        case ObjectType: {
            JSObject* jsobject = jsvalue->getObject();
            jsvalue = jsobject->defaultValue(frame->impl()->state(), StringType);
            string = jsvalue->getString();
            break;
        }
    }

    *value = WebCore::BString(string).release();
    return S_OK;
}

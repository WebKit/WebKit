/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
#include "DOMCSSClasses.h"

#pragma warning(push, 0)
#include <WebCore/PlatformString.h>
#pragma warning(pop)

// DeprecatedDOMCSSStyleDeclaration - DeprecatedDOMCSSStyleDeclaration ----------------------------

DeprecatedDOMCSSStyleDeclaration::DeprecatedDOMCSSStyleDeclaration(WebCore::CSSStyleDeclaration* s)
: m_style(0)
{
    if (s)
        s->ref();

    m_style = s;
}

DeprecatedDOMCSSStyleDeclaration::~DeprecatedDOMCSSStyleDeclaration()
{
    if (m_style)
        m_style->deref();
}

IDeprecatedDOMCSSStyleDeclaration* DeprecatedDOMCSSStyleDeclaration::createInstance(WebCore::CSSStyleDeclaration* s)
{
    if (!s)
        return 0;

    HRESULT hr;
    IDeprecatedDOMCSSStyleDeclaration* domStyle = 0;

    DeprecatedDOMCSSStyleDeclaration* newStyle = new DeprecatedDOMCSSStyleDeclaration(s);
    hr = newStyle->QueryInterface(IID_IDeprecatedDOMCSSStyleDeclaration, (void**)&domStyle);

    if (FAILED(hr))
        return 0;

    return domStyle;
}

// DeprecatedDOMCSSStyleDeclaration - IUnknown ------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMCSSStyleDeclaration::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDeprecatedDOMCSSStyleDeclaration))
        *ppvObject = static_cast<IDeprecatedDOMCSSStyleDeclaration*>(this);
    else
        return DeprecatedDOMObject::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DeprecatedDOMCSSStyleDeclaration - IDeprecatedDOMCSSStyleDeclaration ---------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMCSSStyleDeclaration::cssText( 
    /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMCSSStyleDeclaration::setCssText( 
    /* [in] */ BSTR cssText)
{
    WebCore::String cssTextString(cssText);
    // FIXME: <rdar://5148045> return DOM exception info
    WebCore::ExceptionCode ec;
    m_style->setCssText(cssTextString, ec);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMCSSStyleDeclaration::getPropertyValue( 
    /* [in] */ BSTR propertyName,
    /* [retval][out] */ BSTR* result)
{
    WebCore::String propertyNameString(propertyName);
    WebCore::String value = m_style->getPropertyValue(propertyNameString);
    *result = SysAllocStringLen(value.characters(), value.length());
    if (value.length() && !*result)
        return E_OUTOFMEMORY;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMCSSStyleDeclaration::getPropertyCSSValue( 
    /* [in] */ BSTR /*propertyName*/,
    /* [retval][out] */ IDeprecatedDOMCSSValue** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMCSSStyleDeclaration::removeProperty( 
    /* [in] */ BSTR /*propertyName*/,
    /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMCSSStyleDeclaration::getPropertyPriority( 
    /* [in] */ BSTR /*propertyName*/,
    /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMCSSStyleDeclaration::setProperty( 
    /* [in] */ BSTR propertyName,
    /* [in] */ BSTR value,
    /* [in] */ BSTR priority)
{
    WebCore::String propertyNameString(propertyName);
    WebCore::String valueString(value);
    WebCore::String priorityString(priority);
    // FIXME: <rdar://5148045> return DOM exception info
    WebCore::ExceptionCode code;
    m_style->setProperty(propertyNameString, valueString, priorityString, code);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMCSSStyleDeclaration::length( 
    /* [retval][out] */ UINT* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMCSSStyleDeclaration::item( 
    /* [in] */ UINT /*index*/,
    /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMCSSStyleDeclaration::parentRule( 
    /* [retval][out] */ IDeprecatedDOMCSSRule** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

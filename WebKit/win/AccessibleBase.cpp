/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "WebKitDLL.h"
#include "AccessibleBase.h"

#include <oleacc.h>
#include <WebCore/AccessibilityObject.h>
#include <WebCore/AXObjectCache.h>
#include <WebCore/BString.h>
#include <WebCore/Element.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLFrameElementBase.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/RenderFrame.h>
#include <WebCore/RenderView.h>
#include <WebCore/FrameView.h>
#include <WebCore/IntRect.h>
#include <WebCore/PlatformString.h>
#include <WebCore/RenderObject.h>
#include "WebView.h"
#include <wtf/RefPtr.h>

using namespace WebCore;

AccessibleBase::AccessibleBase(AccessibilityObject* obj)
    : AccessibilityObjectWrapper(obj)
    , m_refCount(0)
{
    ASSERT_ARG(obj, obj);
    m_object->setWrapper(this);
    ++gClassCount;
}

AccessibleBase::~AccessibleBase()
{
    --gClassCount;
}

AccessibleBase* AccessibleBase::createInstance(AccessibilityObject* obj)
{
    ASSERT_ARG(obj, obj);

    return new AccessibleBase(obj);
}

// IUnknown
HRESULT STDMETHODCALLTYPE AccessibleBase::QueryInterface(REFIID riid, void** ppvObject)
{
    if (IsEqualGUID(riid, __uuidof(IAccessible)))
        *ppvObject = this;
    else if (IsEqualGUID(riid, __uuidof(IDispatch)))
        *ppvObject = this;
    else if (IsEqualGUID(riid, __uuidof(IUnknown)))
        *ppvObject = this;
    else {
        *ppvObject = 0;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE AccessibleBase::Release(void)
{
    ASSERT(m_refCount > 0);
    if (--m_refCount)
        return m_refCount;
    delete this;
    return 0;
}

// IAccessible
HRESULT STDMETHODCALLTYPE AccessibleBase::get_accParent(IDispatch** parent)
{
    *parent = 0;

    if (!m_object)
        return E_FAIL;

    return WebView::AccessibleObjectFromWindow(m_object->topRenderer()->view()->frameView()->containingWindow(),
        OBJID_WINDOW, __uuidof(IAccessible), reinterpret_cast<void**>(parent));
}

HRESULT STDMETHODCALLTYPE AccessibleBase::get_accChildCount(long* count)
{
    if (!m_object)
        return E_FAIL;
    if (!count)
        return E_POINTER;
    *count = static_cast<long>(m_object->children().size());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::get_accChild(VARIANT vChild, IDispatch** ppChild)
{
    if (!ppChild)
        return E_POINTER;

    *ppChild = 0;

    AccessibilityObject* childObj;

    HRESULT hr = getAccessibilityObjectForChild(vChild, childObj);
    if (FAILED(hr))
        return hr;

    *ppChild = static_cast<IDispatch*>(wrapper(childObj));
    (*ppChild)->AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::get_accName(VARIANT, BSTR*)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::get_accValue(VARIANT, BSTR*)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::get_accDescription(VARIANT, BSTR*)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::get_accRole(VARIANT, VARIANT*)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::get_accState(VARIANT, VARIANT*)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::get_accHelp(VARIANT, BSTR*)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::get_accKeyboardShortcut(VARIANT, BSTR*)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::accSelect(long, VARIANT)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::get_accFocus(VARIANT*)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::get_accSelection(VARIANT*)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::get_accDefaultAction(VARIANT, BSTR*)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::accLocation(long*, long*, long*, long*, VARIANT)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::accNavigate(long, VARIANT, VARIANT*)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::accHitTest(long, long, VARIANT*)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::accDoDefaultAction(VARIANT)
{
    return E_NOTIMPL;
}

HRESULT AccessibleBase::getAccessibilityObjectForChild(VARIANT vChild, Accessibi
lityObject*& childObj) const
{
    childObj = 0;

    if (!m_object)
        return E_FAIL;

    if (vChild.vt != VT_I4)
        return E_INVALIDARG;

    if (vChild.lVal == CHILDID_SELF)
        childObj = m_object;
    else {
        size_t childIndex = static_cast<size_t>(vChild.lVal - 1);

        if (childIndex >= m_object->children().size())
            return E_FAIL;
        childObj = m_object->children().at(childIndex).get();
    }

    if (!childObj)
        return E_FAIL;

    return S_OK;
}

AccessibleBase* AccessibleBase::wrapper(AccessibilityObject* obj)
{
    AccessibleBase* result = static_cast<AccessibleBase*>(obj->wrapper());
    if (!result)
        result = createInstance(obj);
    return result;
}


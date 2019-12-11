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

#include "WebKitDLL.h"

#include "WebNotification.h"

#include <wtf/Assertions.h>

// WebNotification ------------------------------------------------------------

WebNotification::WebNotification(BSTR name, IUnknown* anObject, IPropertyBag* userInfo)
    : m_anObject(anObject)
    , m_userInfo(userInfo)
{
    if (name)
        m_name = SysAllocString(name);
    if (m_anObject)
        m_anObject->AddRef();
    if (m_userInfo)
        m_userInfo->AddRef();

    gClassCount++;
    gClassNameCount().add("WebNotification");
}

WebNotification::~WebNotification()
{
    if (m_name)
        SysFreeString(m_name);
    if (m_anObject)
        m_anObject->Release();
    if (m_userInfo)
        m_userInfo->Release();

    gClassCount--;
    gClassNameCount().remove("WebNotification");
}

WebNotification* WebNotification::createInstance(BSTR name /*=0*/, IUnknown* anObject /*=0*/, IPropertyBag* userInfo /*=0*/)
{
    WebNotification* instance = new WebNotification(name, anObject, userInfo);
    instance->AddRef();
    return instance;
}

// IUnknown -------------------------------------------------------------------

HRESULT WebNotification::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebNotification*>(this);
    else if (IsEqualGUID(riid, IID_IWebNotification))
        *ppvObject = static_cast<IWebNotification*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebNotification::AddRef()
{
    return ++m_refCount;
}

ULONG WebNotification::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebNotification -----------------------------------------------------------

HRESULT WebNotification::notificationWithName(_In_ BSTR /*aName*/, _In_opt_ IUnknown* /*anObject*/, _In_opt_ IPropertyBag* /*userInfo*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT WebNotification::name(__deref_out_opt BSTR* result)
{
    if (!result)
        return E_POINTER;

    *result = nullptr;
    if (m_name) {
        *result = SysAllocString(m_name);
        if (!*result)
            return E_OUTOFMEMORY;
    }
    
    return S_OK;
}

HRESULT WebNotification::getObject(_COM_Outptr_opt_ IUnknown** result)
{
    if (!result)
        return E_POINTER;

    *result = m_anObject;

    if (*result)
        (*result)->AddRef();

    return S_OK;
}

HRESULT WebNotification::userInfo(_COM_Outptr_opt_ IPropertyBag** result)
{
    if (!result)
        return E_POINTER;

    *result = m_userInfo;

    if (*result)
        (*result)->AddRef();

    return S_OK;
}

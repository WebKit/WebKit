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

#include "WebScriptObject.h"

#include "WebKitDLL.h"

#include <wtf/Assertions.h>

// WebScriptObject ------------------------------------------------------------

WebScriptObject::WebScriptObject()
{
    gClassCount++;
    gClassNameCount().add("WebScriptObject");
}

WebScriptObject::~WebScriptObject()
{
    gClassCount--;
    gClassNameCount().remove("WebScriptObject");
}

// IUnknown -------------------------------------------------------------------

HRESULT WebScriptObject::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebScriptObject*>(this);
    else if (IsEqualGUID(riid, IID_IWebScriptObject))
        *ppvObject = static_cast<IWebScriptObject*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG WebScriptObject::AddRef()
{
    return ++m_refCount;
}

ULONG WebScriptObject::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// WebScriptObject ------------------------------------------------------------

HRESULT WebScriptObject::throwException(_In_ BSTR /*exceptionMessage*/, _Out_ BOOL* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = FALSE;
    return E_NOTIMPL;
}

HRESULT WebScriptObject::callWebScriptMethod(_In_ BSTR name, __in_ecount_opt(cArgs) const VARIANT args[], int cArgs, _Out_ VARIANT* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    return E_NOTIMPL;
}

HRESULT WebScriptObject::evaluateWebScript(_In_ BSTR /*script*/, _Out_ VARIANT* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT WebScriptObject::removeWebScriptKey(_In_ BSTR /*name*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT WebScriptObject::stringRepresentation(__deref_opt_out BSTR* /*stringRepresentation*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT WebScriptObject::webScriptValueAtIndex(unsigned /*index*/, _Out_ VARIANT* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT WebScriptObject::setWebScriptValueAtIndex(unsigned /*index*/, VARIANT /*val*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT WebScriptObject::setException(_In_ BSTR /*description*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

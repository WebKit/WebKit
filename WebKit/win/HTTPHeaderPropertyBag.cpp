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
#include "HTTPHeaderPropertyBag.h"

#include "HTTPHeaderMap.h"

#include <WebCore/BString.h>

// HTTPHeaderPropertyBag -----------------------------------------------

HTTPHeaderPropertyBag::HTTPHeaderPropertyBag(WebURLResponse* response)
    : m_refCount(1)
    , m_response(response)
{
}

HTTPHeaderPropertyBag* HTTPHeaderPropertyBag::createInstance(WebURLResponse* response)
{
    return new HTTPHeaderPropertyBag(response);
}

// IUnknown -------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE HTTPHeaderPropertyBag::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IPropertyBag*>(this);
    else if (IsEqualGUID(riid, IID_IPropertyBag))
        *ppvObject = static_cast<IPropertyBag*>(this);
    else if (IsEqualGUID(riid, __uuidof(HTTPHeaderPropertyBag)))
        *ppvObject = this;
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE HTTPHeaderPropertyBag::AddRef(void)
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE HTTPHeaderPropertyBag::Release(void)
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete this;

    return newRef;
}

static inline WebCore::String ConvertFromLPCOLESTRToString(LPCOLESTR pszPropName)
{
    return WebCore::String(pszPropName);
}

static bool ConvertFromStringToVariant(VARIANT* pVar, const WebCore::String& stringVal)
{
    if (!pVar)
        return false;

    if (V_VT(pVar) == VT_BSTR) {
        V_BSTR(pVar) = WebCore::BString(stringVal);
        return true;
    }

    return false;
}

// IPropertyBag ------------------------------------------------------------

HRESULT STDMETHODCALLTYPE HTTPHeaderPropertyBag::Read(LPCOLESTR pszPropName, VARIANT* pVar, IErrorLog* /*pErrorLog*/)
{
    if (!pszPropName)
        return E_POINTER;

    if (!m_response)
        return E_FAIL;

    const WebCore::HTTPHeaderMap& headerMap = m_response->resourceResponse().httpHeaderFields();
    WebCore::String key = ConvertFromLPCOLESTRToString(pszPropName);
    WebCore::String value = headerMap.get(key);

    if (!ConvertFromStringToVariant(pVar, value))
        return E_INVALIDARG;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE HTTPHeaderPropertyBag::Write(LPCOLESTR /*pszPropName*/, VARIANT* /*pVar*/)
{
    // We cannot add to the Resource Response's header hash map
    return E_FAIL;
}

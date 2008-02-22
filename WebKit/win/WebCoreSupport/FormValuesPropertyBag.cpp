/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "FormValuesPropertyBag.h"

using namespace WebCore;

HRESULT STDMETHODCALLTYPE FormValuesPropertyBag::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = this;
    else if (IsEqualGUID(riid, IID_IPropertyBag))
        *ppvObject = static_cast<IPropertyBag*>(this);
    else if (IsEqualGUID(riid, IID_IPropertyBag2))
        *ppvObject = static_cast<IPropertyBag2*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE FormValuesPropertyBag::AddRef(void)
{
    return 1;
}

ULONG STDMETHODCALLTYPE FormValuesPropertyBag::Release(void)
{
    return 0;
}

HRESULT STDMETHODCALLTYPE FormValuesPropertyBag::Read(LPCOLESTR pszPropName, VARIANT* pVar, IErrorLog* /*pErrorLog*/)
{
    HRESULT hr = S_OK;

    if (!pszPropName || !pVar)
        return E_POINTER;

    String key(pszPropName);
    if (!m_formValues->contains(key))
        return E_INVALIDARG;

    String value = m_formValues->get(key);

    VARTYPE requestedType = V_VT(pVar);
    VariantClear(pVar);
    V_VT(pVar) = VT_BSTR;
    V_BSTR(pVar) = SysAllocStringLen(value.characters(), value.length());
    if (value.length() && !V_BSTR(pVar))
        return E_OUTOFMEMORY;

    if (requestedType != VT_BSTR && requestedType != VT_EMPTY)
        hr = VariantChangeType(pVar, pVar, VARIANT_NOUSEROVERRIDE | VARIANT_ALPHABOOL, requestedType);

    return hr;
}

HRESULT STDMETHODCALLTYPE FormValuesPropertyBag::Write(LPCOLESTR pszPropName, VARIANT* pVar)
{
    if (!pszPropName || !pVar)
        return E_POINTER;
    VariantClear(pVar);
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE FormValuesPropertyBag::Read(
    /* [in] */ ULONG cProperties,
    /* [in] */ PROPBAG2* pPropBag,
    /* [in] */ IErrorLog* pErrLog,
    /* [out] */ VARIANT* pvarValue,
    /* [out] */ HRESULT* phrError)
{
    if (cProperties > (size_t)m_formValues->size())
        return E_INVALIDARG;
    if (!pPropBag || !pvarValue || !phrError)
        return E_POINTER;

    for (ULONG i=0; i<cProperties; i++) {
        VariantInit(&pvarValue[i]);
        phrError[i] = Read(pPropBag->pstrName, &pvarValue[i], pErrLog);
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE FormValuesPropertyBag::Write(
    /* [in] */ ULONG /*cProperties*/,
    /* [in] */ PROPBAG2* pPropBag,
    /* [in] */ VARIANT* pvarValue)
{
    if (!pPropBag || !pvarValue)
        return E_POINTER;
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE FormValuesPropertyBag::CountProperties(
    /* [out] */ ULONG* pcProperties)
{
    *pcProperties = m_formValues->size();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE FormValuesPropertyBag::GetPropertyInfo(
    /* [in] */ ULONG iProperty,
    /* [in] */ ULONG cProperties,
    /* [out] */ PROPBAG2* pPropBag,
    /* [out] */ ULONG* pcProperties)
{
    if (iProperty > (size_t)m_formValues->size() || iProperty+cProperties > (size_t)m_formValues->size())
        return E_INVALIDARG;
    if (!pPropBag || !pcProperties)
        return E_POINTER;

    *pcProperties = 0;
    ULONG i = 0;
    ULONG endProperty = iProperty + cProperties;
    for (HashMap<String, String>::iterator it = m_formValues->begin(); i<endProperty; i++) {
        if (i >= iProperty) {
            int storeIndex = (*pcProperties)++;
            pPropBag[storeIndex].dwType = PROPBAG2_TYPE_DATA;
            pPropBag[storeIndex].vt = VT_BSTR;
            pPropBag[storeIndex].cfType = CF_TEXT;
            pPropBag[storeIndex].dwHint = 0;
            pPropBag[storeIndex].pstrName = const_cast<LPOLESTR>(it->first.charactersWithNullTermination());
        }
        ++it;
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE FormValuesPropertyBag::LoadObject(
    /* [in] */ LPCOLESTR pstrName,
    /* [in] */ DWORD /*dwHint*/,
    /* [in] */ IUnknown* pUnkObject,
    /* [in] */ IErrorLog* /*pErrLog*/)
{
    if (!pstrName || !pUnkObject)
        return E_POINTER;
    return E_FAIL;
}

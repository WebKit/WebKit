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

#ifndef FormValuesPropertyBag_h
#define FormValuesPropertyBag_h

#pragma warning(push, 0)
#include <WebCore/StringHash.h>
#pragma warning(pop)
#include <wtf/HashMap.h>

class FormValuesPropertyBag : public IPropertyBag, public IPropertyBag2 {
public:
    FormValuesPropertyBag(HashMap<WebCore::String, WebCore::String>* formValues)
        : m_formValues(formValues)
    {
    }

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IPropertyBag
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Read(
        /* [in] */ LPCOLESTR pszPropName,
        /* [out][in] */ VARIANT*,
        /* [in] */ IErrorLog*);

    virtual HRESULT STDMETHODCALLTYPE Write(
        /* [in] */ LPCOLESTR pszPropName,
        /* [in] */ VARIANT*);

    // IPropertyBag2
    virtual HRESULT STDMETHODCALLTYPE Read(
        /* [in] */ ULONG cProperties,
        /* [in] */ PROPBAG2*,
        /* [in] */ IErrorLog*,
        /* [out] */ VARIANT* pvarValue,
        /* [out] */ HRESULT* phrError);

    virtual HRESULT STDMETHODCALLTYPE Write(
        /* [in] */ ULONG cProperties,
        /* [in] */ PROPBAG2*,
        /* [in] */ VARIANT*);

    virtual HRESULT STDMETHODCALLTYPE CountProperties(
        /* [out] */ ULONG* pcProperties);

    virtual HRESULT STDMETHODCALLTYPE GetPropertyInfo(
        /* [in] */ ULONG iProperty,
        /* [in] */ ULONG cProperties,
        /* [out] */ PROPBAG2* pPropBag,
        /* [out] */ ULONG* pcProperties);

    virtual HRESULT STDMETHODCALLTYPE LoadObject(
        /* [in] */ LPCOLESTR pstrName,
        /* [in] */ DWORD dwHint,
        /* [in] */ IUnknown*,
        /* [in] */ IErrorLog*);

protected:
    HashMap<WebCore::String, WebCore::String>* m_formValues;
};

#endif // FormValuesPropertyBag_h

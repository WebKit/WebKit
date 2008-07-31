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

#ifndef COMPropertyBag_h
#define COMPropertyBag_h

#define NOMINMAX
#include <unknwn.h>

#include <wtf/Noncopyable.h>
#include <wtf/HashMap.h>

#include "COMVariantSetter.h"

template<typename ValueType, typename HashType = typename WebCore::StringHash>
class COMPropertyBag : public IPropertyBag, Noncopyable {
public:
    typedef HashMap<WebCore::String, ValueType, HashType> HashMapType;

    static COMPropertyBag* createInstance(const HashMapType&);
    static COMPropertyBag* adopt(HashMapType&);

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IPropertyBag
    virtual HRESULT STDMETHODCALLTYPE Read(LPCOLESTR pszPropName, VARIANT*, IErrorLog*);
    virtual HRESULT STDMETHODCALLTYPE Write(LPCOLESTR pszPropName, VARIANT*);

private:
    COMPropertyBag()
        : m_refCount(0)
    {
    }

    COMPropertyBag(const HashMapType& hashMap)
        : m_refCount(0)
        , m_hashMap(hashMap)
    {
    }

    ~COMPropertyBag() {}

    ULONG m_refCount;
    HashMapType m_hashMap;
};

// COMPropertyBag ------------------------------------------------------------------
template<typename ValueType, typename HashType>
COMPropertyBag<ValueType, HashType>* COMPropertyBag<typename ValueType, HashType>::createInstance(const HashMapType& hashMap)
{
    COMPropertyBag* instance = new COMPropertyBag(hashMap);
    instance->AddRef();
    return instance;
}

template<typename ValueType, typename HashType>
COMPropertyBag<ValueType, HashType>* COMPropertyBag<typename ValueType, HashType>::adopt(HashMapType& hashMap)
{
    COMPropertyBag* instance = new COMPropertyBag;
    instance->m_hashMap.swap(hashMap);
    instance->AddRef();
    return instance;
}

// IUnknown ------------------------------------------------------------------------
template<typename ValueType, typename HashType>
HRESULT STDMETHODCALLTYPE COMPropertyBag<ValueType, HashType>::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<COMPropertyBag*>(this);
    else if (IsEqualGUID(riid, IID_IPropertyBag))
        *ppvObject = static_cast<COMPropertyBag*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

template<typename ValueType, typename HashType>
ULONG STDMETHODCALLTYPE COMPropertyBag<ValueType, HashType>::AddRef()
{
    return ++m_refCount;
}

template<typename ValueType, typename HashType>
ULONG STDMETHODCALLTYPE COMPropertyBag<ValueType, HashType>::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete this;

    return newRef;
}

template<typename ValueType, typename HashType>
HRESULT STDMETHODCALLTYPE COMPropertyBag<ValueType, HashType>::Read(LPCOLESTR pszPropName, VARIANT* pVar, IErrorLog* pErrorLog)
{
    if (!pszPropName)
        return E_POINTER;

    HashMapType::const_iterator it = m_hashMap.find(String(pszPropName));
    HashMapType::const_iterator end = m_hashMap.end();
    if (it == end)
        return E_INVALIDARG;

    VARTYPE requestedType = V_VT(pVar);
    V_VT(pVar) = VT_EMPTY;
    COMVariantSetter<ValueType>::setVariant(pVar, it->second);

    if (requestedType != COMVariantSetter<ValueType>::VariantType && requestedType != VT_EMPTY)
        return ::VariantChangeType(pVar, pVar, VARIANT_NOUSEROVERRIDE | VARIANT_ALPHABOOL, requestedType);

    return S_OK;
}

template<typename ValueType, typename HashType>
HRESULT STDMETHODCALLTYPE COMPropertyBag<ValueType, HashType>::Write(LPCOLESTR pszPropName, VARIANT* pVar)
{
    return E_FAIL;
}

#endif // COMPropertyBag_h

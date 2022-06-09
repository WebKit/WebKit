/*
 * Copyright (C) 2010, 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "WebUserContentURLPattern.h"

#include "MarshallingHelpers.h"
#include "WebKitDLL.h"

#include <WebCore/BString.h>
#include <wtf/URL.h>

using namespace WebCore;

inline WebUserContentURLPattern::WebUserContentURLPattern()
{
    ++gClassCount;
    gClassNameCount().add("WebUserContentURLPattern");
}

WebUserContentURLPattern::~WebUserContentURLPattern()
{
    --gClassCount;
    gClassNameCount().remove("WebUserContentURLPattern");
}

COMPtr<WebUserContentURLPattern> WebUserContentURLPattern::createInstance()
{
    return new WebUserContentURLPattern;
}

ULONG WebUserContentURLPattern::AddRef()
{
    return ++m_refCount;
}

ULONG WebUserContentURLPattern::Release()
{
    ULONG newRefCount = --m_refCount;
    if (!newRefCount)
        delete this;
    return newRefCount;
}

HRESULT WebUserContentURLPattern::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;

    if (IsEqualIID(riid, __uuidof(WebUserContentURLPattern)))
        *ppvObject = this;
    else if (IsEqualIID(riid, __uuidof(IWebUserContentURLPattern)))
        *ppvObject = static_cast<IWebUserContentURLPattern*>(this);
    else if (IsEqualIID(riid, IID_IUnknown))
        *ppvObject = static_cast<IUnknown*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

HRESULT WebUserContentURLPattern::parse(_In_ BSTR patternString)
{
    m_pattern = UserContentURLPattern(String(patternString, SysStringLen(patternString)));
    return S_OK;
}

HRESULT WebUserContentURLPattern::isValid(_Out_ BOOL* isValid)
{
    if (!isValid)
        return E_POINTER;
    *isValid = m_pattern.isValid();
    return S_OK;
}

HRESULT WebUserContentURLPattern::scheme(_Deref_opt_out_ BSTR* scheme)
{
    if (!scheme)
        return E_POINTER;
    *scheme = BString(m_pattern.scheme()).release();
    return S_OK;
}

HRESULT WebUserContentURLPattern::host(_Deref_opt_out_ BSTR* host)
{
    if (!host)
        return E_POINTER;
    *host = BString(m_pattern.host()).release();
    return S_OK;
}

HRESULT WebUserContentURLPattern::matchesSubdomains(_Out_ BOOL* matches)
{
    if (!matches)
        return E_POINTER;
    *matches = m_pattern.matchSubdomains();
    return S_OK;
}

HRESULT WebUserContentURLPattern::matchesURL(_In_ BSTR url, _Out_ BOOL* matches)
{
    if (!matches)
        return E_POINTER;
    *matches = m_pattern.matches(MarshallingHelpers::BSTRToKURL(url));
    return S_OK;
}

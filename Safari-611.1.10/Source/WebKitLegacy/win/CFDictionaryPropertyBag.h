/*
 * Copyright (C) 2006-2019 Apple Inc.  All rights reserved.
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

#pragma once

#include "WebKit.h"
#include <WebCore/COMPtr.h>
#include <wtf/RetainPtr.h>

#if USE(CF)
typedef struct __CFDictionary* CFMutableDictionaryRef;
#endif

class CFDictionaryPropertyBag : public IPropertyBag {
public:
    static COMPtr<CFDictionaryPropertyBag> createInstance();

    // IUnknown
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

#if USE(CF)
    void setDictionary(CFMutableDictionaryRef dictionary);
    WEBKIT_API CFMutableDictionaryRef dictionary() const;
#endif

private:
    CFDictionaryPropertyBag();
    ~CFDictionaryPropertyBag();

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID, _COM_Outptr_ void** ppvObject);

    // IPropertyBag
    virtual HRESULT STDMETHODCALLTYPE Read(LPCOLESTR pszPropName, VARIANT*, IErrorLog*);
    virtual HRESULT STDMETHODCALLTYPE Write(_In_ LPCOLESTR pszPropName, _In_  VARIANT*);

#if USE(CF)
    RetainPtr<CFMutableDictionaryRef> m_dictionary;
#endif
    ULONG m_refCount { 0 };
};

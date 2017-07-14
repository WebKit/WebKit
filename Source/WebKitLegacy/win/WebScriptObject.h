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

#ifndef WebScriptObject_H
#define WebScriptObject_H

#include "WebKit.h"

class WebScriptObject : public IWebScriptObject
{
public:
    WebScriptObject();
    virtual ~WebScriptObject();

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException(_In_ BSTR exceptionMessage, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod(_In_ BSTR name, __in_ecount_opt(cArgs) const VARIANT args[], int cArgs, _Out_ VARIANT* result);
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript(_In_ BSTR script, _Out_ VARIANT* result);
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey(_In_ BSTR name);
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation(__deref_opt_out BSTR* stringRepresentation);
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex(unsigned index, _Out_ VARIANT* result);
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex(unsigned index, VARIANT val);
    virtual HRESULT STDMETHODCALLTYPE setException(_In_ BSTR description);

protected:
    ULONG m_refCount { 0 };
};

#endif

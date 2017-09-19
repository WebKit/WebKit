/*
* Copyright (C) 2015 Apple Inc. All Rights Reserved.
* Copyright (C) 2011 Anthony Johnson. All Rights Reserved.
 * Copyright (C) 2011 Brent Fulgham. All Rights Reserved.
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

#ifndef DOMDefaultImpl_h
#define DOMDefaultImpl_h

#include <WebKitLegacy/WebKit.h>

class WebScriptObject : public IWebScriptObject {
public:
    WebScriptObject()
    {
    }

    virtual ~WebScriptObject()
    {
    }

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException(_In_ BSTR, _Out_ BOOL*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod(_In_ BSTR, __in_ecount(cArgs) const VARIANT[], int cArgs, _Out_ VARIANT*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript(_In_ BSTR, _Out_ VARIANT*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey(_In_ BSTR) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation(__deref_opt_out BSTR*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex(unsigned, _Out_ VARIANT*)  { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex(unsigned, VARIANT)  { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE setException(_In_ BSTR)  { return E_NOTIMPL; }

protected:
    ULONG m_refCount { 0 };
};


class DOMObject : public WebScriptObject, public IDOMObject {
public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
};


class DOMEventListener : public DOMObject, public IDOMEventListener {
public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException(_In_ BSTR, _Out_ BOOL*)  { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod(_In_ BSTR, __in_ecount(cArgs) const VARIANT[], int cArgs, _Out_ VARIANT*)  { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript(_In_ BSTR, _Out_ VARIANT*)  { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey(_In_ BSTR)  { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation(__deref_opt_out BSTR*)  { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex(unsigned, _Out_ VARIANT*)  { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex(unsigned, VARIANT)  { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE setException(_In_ BSTR) { return E_NOTIMPL; }

    // IDOMEventListener
    virtual HRESULT STDMETHODCALLTYPE handleEvent(_In_opt_ IDOMEvent*) { return E_NOTIMPL; }
};

#endif

/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
#include "WebKitDLL.h"
#include "WebScriptCallFrame.h"

#include "Identifier.h"
#include "IWebScriptScope.h"
#include "Function.h"
#include "WebScriptScope.h"

#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSStringRefBSTR.h>
#include <JavaScriptCore/JSValueRef.h>

#pragma warning(push, 0)
#include <WebCore/BString.h>
#include <WebCore/PlatformString.h>
#pragma warning(pop)

#include <wtf/Assertions.h>

using namespace KJS;

// EnumScopes -----------------------------------------------------------------

class EnumScopes : public IEnumVARIANT {
public:
    static EnumScopes* createInstance(ScopeChain chain);

private:
    EnumScopes(ScopeChain chain)
        : m_refCount(0)
        , m_chain(chain)
        , m_current(chain.begin())
    {
    }

public:
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();
    virtual HRESULT STDMETHODCALLTYPE Next(ULONG celt, VARIANT* rgVar, ULONG* pCeltFetched);
    virtual HRESULT STDMETHODCALLTYPE Skip(ULONG celt);
    virtual HRESULT STDMETHODCALLTYPE Reset(void);
    virtual HRESULT STDMETHODCALLTYPE Clone(IEnumVARIANT**);

private:
    ULONG m_refCount;
    ScopeChain m_chain;
    ScopeChainIterator m_current;
};

EnumScopes* EnumScopes::createInstance(ScopeChain chain)
{
    EnumScopes* instance = new EnumScopes(chain);
    instance->AddRef();
    return instance;
}

HRESULT STDMETHODCALLTYPE EnumScopes::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown) || IsEqualGUID(riid, IID_IEnumVARIANT))
        *ppvObject = this;
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE EnumScopes::AddRef()
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE EnumScopes::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);
    return newRef;
}

HRESULT STDMETHODCALLTYPE EnumScopes::Next(ULONG celt, VARIANT* rgVar, ULONG* pCeltFetched)
{
    if (pCeltFetched)
        *pCeltFetched = 0;
    if (!rgVar)
        return E_POINTER;
    VariantInit(rgVar);
    if (!celt || celt > 1)
        return S_FALSE;
    if (m_current == m_chain.end())
        return S_FALSE;

    // Create a WebScriptScope from the m_current then put it in an IUnknown.
    COMPtr<IWebScriptScope> scope = WebScriptScope::createInstance(*m_current);
    COMPtr<IUnknown> unknown = static_cast<IUnknown*>(scope.get());
    ++m_current;
    if (!unknown)
        return E_FAIL;

    V_VT(rgVar) = VT_UNKNOWN;
    V_UNKNOWN(rgVar) = unknown.get();

    if (pCeltFetched)
        *pCeltFetched = 1;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE EnumScopes::Skip(ULONG celt)
{
    for (ULONG i = 0; i < celt; ++i)
        ++m_current;
    return (m_current != m_chain.end()) ? S_OK : S_FALSE;
}

HRESULT STDMETHODCALLTYPE EnumScopes::Reset(void)
{
    m_current = m_chain.begin();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE EnumScopes::Clone(IEnumVARIANT**)
{
    return E_NOTIMPL;
}
// WebScriptCallFrame -----------------------------------------------------------



WebScriptCallFrame::WebScriptCallFrame()
    : m_refCount(0)
{
    gClassCount++;
}

WebScriptCallFrame::~WebScriptCallFrame()
{
    gClassCount--;
}

WebScriptCallFrame* WebScriptCallFrame::createInstance()
{
    WebScriptCallFrame* instance = new WebScriptCallFrame();
    instance->AddRef();
    return instance;
}

// IUnknown ------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebScriptCallFrame::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IUnknown))
        *ppvObject = static_cast<IWebScriptCallFrame*>(this);
    else if (IsEqualGUID(riid, IID_IWebScriptCallFrame))
        *ppvObject = static_cast<IWebScriptCallFrame*>(this);
    else
        return E_NOINTERFACE;

    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE WebScriptCallFrame::AddRef()
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE WebScriptCallFrame::Release()
{
    ULONG newRef = --m_refCount;
    if (!newRef)
        delete(this);

    return newRef;
}

// IWebScriptCallFrame -----------------------------------------------------------

HRESULT STDMETHODCALLTYPE WebScriptCallFrame::caller(
    /* [out, retval] */ IWebScriptCallFrame** callFrame)
{
    if (!callFrame)
        return E_POINTER;

    *callFrame = m_caller.get();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebScriptCallFrame::scopeChain(
    /* [out, retval] */ IEnumVARIANT** result)
{
    if (!result)
        return E_POINTER;

    // FIXME: If there is no current body do we need to make scope chain from the global object?

    *result = EnumScopes::createInstance(m_state->scopeChain());

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebScriptCallFrame::functionName(
    /* [out, retval] */ BSTR* funcName)
{
    if (!funcName)
        return E_POINTER;

    if (m_state->currentBody()) {
        FunctionImp* func = m_state->function();
        if (!func)
            return E_FAIL;

        *funcName = WebCore::BString(func->functionName()).release();
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebScriptCallFrame::stringByEvaluatingJavaScriptFromString(
    /* [in] */ BSTR script,
    /* [out, retval] */ BSTR* result)
{
    if (!script)
        return E_FAIL;

    if (!result)
        return E_POINTER;

    *result = 0;

    JSLock lock;

    ExecState* state = m_state;
    Interpreter* interp  = state->dynamicInterpreter();
    JSObject* globObj = interp->globalObject();

    // find "eval"
    JSObject* eval = 0;
    if (state->currentBody()) {  // "eval" won't work without context (i.e. at global scope)
        JSValue* v = globObj->get(state, "eval");
        if (v->isObject() && static_cast<JSObject*>(v)->implementsCall())
            eval = static_cast<JSObject*>(v);
        else
            // no "eval" - fallback operates on global exec state
            state = interp->globalExec();
    }

    JSValue* savedException = state->exception();
    state->clearException();

    UString code(reinterpret_cast<KJS::UChar*>(script), SysStringLen(script));

    // evaluate
    JSValue* scriptExecutionResult;
    if (eval) {
        List args;
        args.append(jsString(code));
        scriptExecutionResult = eval->call(state, 0, args);
    } else
        // no "eval", or no context (i.e. global scope) - use global fallback
        scriptExecutionResult = interp->evaluate(UString(), 0, code.data(), code.size(), globObj).value();

    if (state->hadException())
        scriptExecutionResult = state->exception();    // (may be redundant depending on which eval path was used)
    state->setException(savedException);

    if (!scriptExecutionResult)
        return E_FAIL;

    scriptExecutionResult->isString();
    *result = WebCore::BString(WebCore::String(scriptExecutionResult->getString())).release();

    return S_OK;
}

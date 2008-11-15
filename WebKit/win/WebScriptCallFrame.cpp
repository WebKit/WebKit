/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
#include "WebScriptCallFrame.h"

#include "COMEnumVariant.h"
#include "WebKitDLL.h"

#include <JavaScriptCore/Interpreter.h>
#include <JavaScriptCore/JSFunction.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/JSStringRefBSTR.h>
#include <JavaScriptCore/JSValueRef.h>
#include <JavaScriptCore/PropertyNameArray.h>

#pragma warning(push, 0)
#include <WebCore/BString.h>
#include <WebCore/PlatformString.h>
#pragma warning(pop)

#include <wtf/Assertions.h>

using namespace JSC;

UString WebScriptCallFrame::jsValueToString(JSC::ExecState* state, JSValue* jsvalue)
{
    if (!jsvalue)
        return "undefined";

    if (jsvalue->isString())
        return jsvalue->getString();
    else if (jsvalue->isNumber())
        return UString::from(jsvalue->getNumber());
    else if (jsvalue->isBoolean())
        return jsvalue->getBoolean() ? "True" : "False";
    else if (jsvalue->isObject()) {
        jsvalue = jsvalue->getObject()->defaultValue(state, PreferString);
        return jsvalue->getString();
    }

    return "undefined";
}

// WebScriptCallFrame -----------------------------------------------------------

static ExecState* callingFunctionOrGlobalExecState(ExecState* exec)
{
#if 0
    for (ExecState* current = exec; current; current = current->callingExecState())
        if (current->codeType() == FunctionCode || current->codeType() == GlobalCode)
            return current;
#endif
    return 0;
}

WebScriptCallFrame::WebScriptCallFrame(ExecState* state)
    : m_refCount(0)
    , m_state(callingFunctionOrGlobalExecState(state))
{
    ASSERT_ARG(state, state);
    ASSERT(m_state);
    gClassCount++;
    gClassNameCount.add("WebScriptCallFrame");
}

WebScriptCallFrame::~WebScriptCallFrame()
{
    gClassCount--;
    gClassNameCount.remove("WebScriptCallFrame");
}

WebScriptCallFrame* WebScriptCallFrame::createInstance(ExecState* state)
{
    WebScriptCallFrame* instance = new WebScriptCallFrame(state);
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
#if 0
    *callFrame = m_state->callingExecState() ? WebScriptCallFrame::createInstance(m_state->callingExecState()) : 0;
#endif
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebScriptCallFrame::functionName(
    /* [out, retval] */ BSTR* funcName)
{
    if (!funcName)
        return E_POINTER;

    *funcName = 0;
#if 0
    if (!m_state->scopeNode())
        return S_OK;

    JSFunction* func = m_state->function();
    if (!func)
        return E_FAIL;

    const Identifier& funcIdent = func->functionName();
    if (!funcIdent.isEmpty())
        *funcName = WebCore::BString(funcIdent).release();
#endif
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

    JSLock lock(false);

    JSValue* scriptExecutionResult = valueByEvaluatingJavaScriptFromString(script);
    *result = WebCore::BString(jsValueToString(m_state, scriptExecutionResult)).release();

    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebScriptCallFrame::variableNames(
    /* [out, retval] */ IEnumVARIANT** variableNames)
{
    if (!variableNames)
        return E_POINTER;

    *variableNames = 0;

    PropertyNameArray propertyNames(m_state);
#if 0
    m_state->scopeChain().top()->getPropertyNames(m_state, propertyNames);
    // FIXME: It would be more efficient to use ::adopt here, but PropertyNameArray doesn't have a swap function.
    *variableNames = COMEnumVariant<PropertyNameArray>::createInstance(propertyNames);

#endif
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WebScriptCallFrame::valueForVariable(
    /* [in] */ BSTR key,
    /* [out, retval] */ BSTR* value)
{
    if (!key)
        return E_FAIL;

    if (!value)
        return E_POINTER;

    *value = 0;

    Identifier identKey(m_state, reinterpret_cast<UChar*>(key), SysStringLen(key));

#if 0
    JSValue* jsvalue = noValue();
    ScopeChain scopeChain = m_state->scopeChain();
    for (ScopeChainIterator it = scopeChain.begin(); it != scopeChain.end() && !jsvalue; ++it)
        jsvalue = (*it)->get(m_state, identKey);
    *value = WebCore::BString(jsValueToString(m_state, jsvalue)).release();
#endif

    return S_OK;
}

JSValue* WebScriptCallFrame::valueByEvaluatingJavaScriptFromString(BSTR script)
{
#if 0
    ExecState* state = m_state;
    JSGlobalObject* globObj = state->dynamicGlobalObject();

    // find "eval"
    JSObject* eval = 0;
    if (state->scopeNode()) {  // "eval" won't work without context (i.e. at global scope)
        JSValue* v = globObj->get(state, "eval");
        if (v->isObject() && asObject(v)->implementsCall())
            eval = asObject(v);
        else
            // no "eval" - fallback operates on global exec state
            state = globObj->globalExec();
    }

    JSValue* savedException = state->exception();
    state->clearException();

    UString code(reinterpret_cast<UChar*>(script), SysStringLen(script));

    // evaluate
    JSValue* scriptExecutionResult;
    if (eval) {
        ArgList args;
        args.append(jsString(state, code));
        scriptExecutionResult = eval->call(state, 0, args);
    } else
        // no "eval", or no context (i.e. global scope) - use global fallback
        scriptExecutionResult = JSC::evaluate(state, UString(), 0, code.data(), code.size(), globObj).value();

    if (state->hadException())
        scriptExecutionResult = state->exception();    // (may be redundant depending on which eval path was used)
    state->setException(savedException);

    return scriptExecutionResult;
#else
    return jsNull();
#endif
}

template<> struct COMVariantSetter<Identifier>
{
    static void setVariant(VARIANT* variant, const Identifier& value)
    {
        ASSERT(V_VT(variant) == VT_EMPTY);

        V_VT(variant) = VT_BSTR;
        V_BSTR(variant) = WebCore::BString(reinterpret_cast<const wchar_t*>(value.data()), value.size()).release();
    }
};

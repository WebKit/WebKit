/*
 * Copyright (C) 2007 Apple, Inc.  All rights reserved.
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
#include "DebuggerDocument.h"

#include "ServerConnection.h"

#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSStringRefBSTR.h>
#include <WebKit/IWebScriptCallFrame.h>
#include <WebKit/IWebScriptScope.h>

JSValueRef JSValueRefCreateWithBSTR(JSContextRef context, BSTR string)
{
    JSRetainPtr<JSStringRef> jsString(Adopt, JSStringCreateWithBSTR(string));
    return JSValueMakeString(context, jsString.get());
}

// DebuggerDocument platform specific implementations

void DebuggerDocument::platformPause()
{
    m_server->pause();
}

void DebuggerDocument::platformResume()
{
    m_server->resume();
}

void DebuggerDocument::platformStepInto()
{
    m_server->stepInto();
}

JSValueRef DebuggerDocument::platformEvaluateScript(JSContextRef context, JSStringRef script, int callFrame)
{
    HRESULT ret = S_OK;

    COMPtr<IWebScriptCallFrame> cframe = m_server->getCallerFrame(callFrame);
    if (!cframe)
        return JSValueMakeUndefined(context);

    // Convert script to BSTR
    BSTR scriptBSTR = JSStringCopyBSTR(script);
    BSTR value = 0;
    ret = cframe->stringByEvaluatingJavaScriptFromString(scriptBSTR, &value);
    SysFreeString(scriptBSTR);
    if (FAILED(ret)) {
        SysFreeString(value);
        return JSValueMakeUndefined(context);
    }

    JSValueRef returnValue = JSValueRefCreateWithBSTR(context, value);
    SysFreeString(value);
    return returnValue;

}

void DebuggerDocument::getPlatformCurrentFunctionStack(JSContextRef context, Vector<JSValueRef>& currentStack)
{
    HRESULT ret = S_OK;

    COMPtr<IWebScriptCallFrame> caller;
    for (COMPtr<IWebScriptCallFrame> frame = m_server->currentFrame(); frame; frame = caller) {
        BSTR function = 0;
        ret = frame->functionName(&function);
        if (FAILED(ret)) {
            SysFreeString(function);
            return;
        }

        ret = frame->caller(&caller);
        if (FAILED(ret))
            return;

        bool needToFree = true;
        if (!function) {
            needToFree = false;
            if (caller)
                function = L"(anonymous function)";
            else
                function = L"(global scope)";
        }

        currentStack.append(JSValueRefCreateWithBSTR(context, function));
        if (needToFree)
            SysFreeString(function);
    }
}

void DebuggerDocument::getPlatformLocalScopeVariableNamesForCallFrame(JSContextRef context, int callFrame, Vector<JSValueRef>& variableNames)
{
    HRESULT ret = S_OK;

    COMPtr<IWebScriptCallFrame> cframe = m_server->getCallerFrame(callFrame);
    if (!cframe)
        return;

    COMPtr<IEnumVARIANT> scopeChain;
    ret = cframe->scopeChain(&scopeChain);
    if (FAILED(ret))
        return;

    VARIANT var;
    VariantInit(&var);
    ret = scopeChain->Next(1, &var, 0); // local is always first
    if (FAILED(ret)) {
        VariantClear(&var);
        return;
    }

    ASSERT(V_VT(&var) == VT_UNKNOWN);
    COMPtr<IWebScriptScope> scope;
    scope.query(V_UNKNOWN(&var));
    VariantClear(&var);
    if (!scope)
        return;

    COMPtr<IEnumVARIANT> localScopeVariableNames;
    ret = scope->variableNames(&localScopeVariableNames);
    if (FAILED(ret))
        return;

    while (localScopeVariableNames->Next(1, &var, 0) == S_OK) {
        ASSERT(V_VT(&var) == VT_BSTR);
        BSTR variableName;

        variableName = V_BSTR(&var);
        variableNames.append(JSValueRefCreateWithBSTR(context, variableName));

        SysFreeString(variableName);
        VariantClear(&var);
    }
}

JSValueRef DebuggerDocument::platformValueForScopeVariableNamed(JSContextRef context, JSStringRef key, int callFrame)
{
    HRESULT ret = S_OK;

    COMPtr<IWebScriptCallFrame> cframe = m_server->getCallerFrame(callFrame);
    if (!cframe)
        return JSValueMakeUndefined(context);

    COMPtr<IEnumVARIANT> scopeChain;
    ret = cframe->scopeChain(&scopeChain);
    if (FAILED(ret))
        return JSValueMakeUndefined(context);

    VARIANT var;
    VariantInit(&var);

    BSTR resultString = 0;
    BSTR bstrKey = JSStringCopyBSTR(key);
    while (scopeChain->Next(1, &var, 0) == S_OK && resultString == 0) {
        ASSERT(V_VT(&var) == VT_UNKNOWN);

        COMPtr<IWebScriptScope> scope;
        scope.query(V_UNKNOWN(&var));
        if (!scope)
            return JSValueMakeUndefined(context);

        ret = scope->valueForVariable(bstrKey, &resultString);
        VariantClear(&var);

        if (FAILED(ret)) {
            SysFreeString(bstrKey);
            SysFreeString(resultString);
            return JSValueMakeUndefined(context);
        }
    }
    SysFreeString(bstrKey);

    JSValueRef returnValue = JSValueRefCreateWithBSTR(context, resultString);
    SysFreeString(resultString);

    return returnValue;
}


void DebuggerDocument::platformLog(JSStringRef msg)
{
    printf("%S\n", JSStringGetCharactersPtr(msg));
}

/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef JSMainThreadExecState_h
#define JSMainThreadExecState_h

#include "InspectorInstrumentation.h"
#include "JSDOMBinding.h"
#include <runtime/Completion.h>
#include <runtime/Executable.h>
#ifndef NDEBUG
#include <wtf/MainThread.h>
#endif

namespace WebCore {

class Page;

class JSMainThreadExecState {
    WTF_MAKE_NONCOPYABLE(JSMainThreadExecState);
public:
    static JSC::ExecState* currentState()
    { 
        ASSERT(isMainThread());
        return s_mainThreadState;
    };
    
    static JSC::JSValue call(JSC::ExecState* exec, JSC::JSValue functionObject, JSC::CallType callType, const JSC::CallData& callData, JSC::JSValue thisValue, const JSC::ArgList& args)
    {
        JSMainThreadExecState currentState(exec);
        // Ensure DOM global object is unwrapped to the shell.
        if (thisValue.isObject())
            thisValue = thisValue.toThisObject(exec);
        return JSC::call(exec, functionObject, callType, callData, thisValue, args);
    };

    static JSC::JSValue instrumentedCall(Page* page, JSC::ExecState* exec, JSC::JSValue functionObject, JSC::CallType callType, const JSC::CallData& callData, JSC::JSValue thisValue, const JSC::ArgList& args)
    {
        InspectorInstrumentationCookie cookie;
        if (InspectorInstrumentation::hasFrontends()) {
            String resourceName;
            int lineNumber = 1;

            if (callType == JSC::CallTypeJS) {
                resourceName = ustringToString(callData.js.functionExecutable->sourceURL());
                lineNumber = callData.js.functionExecutable->lineNo();
            } else
                resourceName = "undefined";

            cookie = InspectorInstrumentation::willCallFunction(page, resourceName, lineNumber);
        }

        JSC::JSValue value = call(exec, functionObject, callType, callData, thisValue, args);

        InspectorInstrumentation::didCallFunction(cookie);

        return value;
    };

    static JSC::JSValue evaluate(JSC::ExecState* exec, JSC::ScopeChainNode* chain, const JSC::SourceCode& source, JSC::JSValue thisValue, JSC::JSValue* exception)
    {
        JSMainThreadExecState currentState(exec);
        return JSC::evaluate(exec, chain, source, thisValue, exception);
    };

protected:
    explicit JSMainThreadExecState(JSC::ExecState* exec)
        : m_previousState(s_mainThreadState)
    {
        ASSERT(isMainThread());
        s_mainThreadState = exec;

#if ENABLE(MUTATION_OBSERVERS)
        ASSERT(s_recursionLevel >= 0);
        ++s_recursionLevel;
#endif
    };

    ~JSMainThreadExecState()
    {
        ASSERT(isMainThread());
        s_mainThreadState = m_previousState;

#if ENABLE(MUTATION_OBSERVERS)
        ASSERT(s_recursionLevel > 0);
        if (!--s_recursionLevel)
            didLeaveScriptContext();
#endif
    }

private:
    static JSC::ExecState* s_mainThreadState;
    JSC::ExecState* m_previousState;

#if ENABLE(MUTATION_OBSERVERS)
    static void didLeaveScriptContext();
    static int s_recursionLevel;
#endif
};

// Null state prevents origin security checks.
class JSMainThreadNullState : private JSMainThreadExecState {
public:
    explicit JSMainThreadNullState() : JSMainThreadExecState(0) {};
};

} // namespace WebCore

#endif // JSMainThreadExecState_h

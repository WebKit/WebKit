/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#pragma once

#include "CustomElementReactionQueue.h"
#include "JSDOMBinding.h"
#include <runtime/CatchScope.h>
#include <runtime/Completion.h>
#include <runtime/Microtask.h>
#include <wtf/MainThread.h>

namespace WebCore {

class InspectorInstrumentationCookie;
class ScriptExecutionContext;

class JSMainThreadExecState {
    WTF_MAKE_NONCOPYABLE(JSMainThreadExecState);
    friend class JSMainThreadNullState;
public:
    static JSC::ExecState* currentState()
    {
        ASSERT(isMainThread());
        return s_mainThreadState;
    };
    
    static JSC::JSValue call(JSC::ExecState* exec, JSC::JSValue functionObject, JSC::CallType callType, const JSC::CallData& callData, JSC::JSValue thisValue, const JSC::ArgList& args, NakedPtr<JSC::Exception>& returnedException)
    {
        JSMainThreadExecState currentState(exec);
        return JSC::call(exec, functionObject, callType, callData, thisValue, args, returnedException);
    };

    static JSC::JSValue evaluate(JSC::ExecState* exec, const JSC::SourceCode& source, JSC::JSValue thisValue, NakedPtr<JSC::Exception>& returnedException)
    {
        JSMainThreadExecState currentState(exec);
        return JSC::evaluate(exec, source, thisValue, returnedException);
    };

    static JSC::JSValue evaluate(JSC::ExecState* exec, const JSC::SourceCode& source, JSC::JSValue thisValue = JSC::JSValue())
    {
        NakedPtr<JSC::Exception> unused;
        return evaluate(exec, source, thisValue, unused);
    };

    static JSC::JSValue profiledCall(JSC::ExecState* exec, JSC::ProfilingReason reason, JSC::JSValue functionObject, JSC::CallType callType, const JSC::CallData& callData, JSC::JSValue thisValue, const JSC::ArgList& args, NakedPtr<JSC::Exception>& returnedException)
    {
        JSMainThreadExecState currentState(exec);
        return JSC::profiledCall(exec, reason, functionObject, callType, callData, thisValue, args, returnedException);
    }

    static JSC::JSValue profiledEvaluate(JSC::ExecState* exec, JSC::ProfilingReason reason, const JSC::SourceCode& source, JSC::JSValue thisValue, NakedPtr<JSC::Exception>& returnedException)
    {
        JSMainThreadExecState currentState(exec);
        return JSC::profiledEvaluate(exec, reason, source, thisValue, returnedException);
    }

    static JSC::JSValue profiledEvaluate(JSC::ExecState* exec, JSC::ProfilingReason reason, const JSC::SourceCode& source, JSC::JSValue thisValue = JSC::JSValue())
    {
        NakedPtr<JSC::Exception> unused;
        return profiledEvaluate(exec, reason, source, thisValue, unused);
    }

    static void runTask(JSC::ExecState* exec, JSC::Microtask& task)
    {
        JSMainThreadExecState currentState(exec);
        task.run(exec);
    }

    static JSC::JSInternalPromise& loadModule(JSC::ExecState& state, const String& moduleName, JSC::JSValue parameters, JSC::JSValue scriptFetcher)
    {
        JSMainThreadExecState currentState(&state);
        return *JSC::loadModule(&state, moduleName, parameters, scriptFetcher);
    }

    static JSC::JSInternalPromise& loadModule(JSC::ExecState& state, const JSC::SourceCode& sourceCode, JSC::JSValue scriptFetcher)
    {
        JSMainThreadExecState currentState(&state);
        return *JSC::loadModule(&state, sourceCode, scriptFetcher);
    }

    static JSC::JSValue linkAndEvaluateModule(JSC::ExecState& state, const JSC::Identifier& moduleKey, JSC::JSValue scriptFetcher, NakedPtr<JSC::Exception>& returnedException)
    {
        JSC::VM& vm = state.vm();
        auto scope = DECLARE_CATCH_SCOPE(vm);
    
        JSMainThreadExecState currentState(&state);
        auto returnValue = JSC::linkAndEvaluateModule(&state, moduleKey, scriptFetcher);
        if (UNLIKELY(scope.exception())) {
            returnedException = scope.exception();
            scope.clearException();
            return JSC::jsUndefined();
        }
        return returnValue;
    }

    static InspectorInstrumentationCookie instrumentFunctionCall(ScriptExecutionContext*, JSC::CallType, const JSC::CallData&);
    static InspectorInstrumentationCookie instrumentFunctionConstruct(ScriptExecutionContext*, JSC::ConstructType, const JSC::ConstructData&);

private:
    explicit JSMainThreadExecState(JSC::ExecState* exec)
        : m_previousState(s_mainThreadState)
        , m_lock(exec)
    {
        ASSERT(isMainThread());
        s_mainThreadState = exec;
    };

    ~JSMainThreadExecState()
    {
        JSC::VM& vm = s_mainThreadState->vm();
        auto scope = DECLARE_CATCH_SCOPE(vm);
        ASSERT(isMainThread());
        scope.assertNoException();

        JSC::ExecState* state = s_mainThreadState;
        bool didExitJavaScript = s_mainThreadState && !m_previousState;

        s_mainThreadState = m_previousState;

        if (didExitJavaScript)
            didLeaveScriptContext(state);
    }

    template<typename Type, Type jsType, typename DataType> static InspectorInstrumentationCookie instrumentFunctionInternal(ScriptExecutionContext*, Type, const DataType&);

    WEBCORE_EXPORT static JSC::ExecState* s_mainThreadState;
    JSC::ExecState* m_previousState;
    JSC::JSLockHolder m_lock;

    static void didLeaveScriptContext(JSC::ExecState*);
};

// Null state prevents origin security checks.
// Used by non-JavaScript bindings (ObjC, GObject).
class JSMainThreadNullState {
    WTF_MAKE_NONCOPYABLE(JSMainThreadNullState);
public:
    explicit JSMainThreadNullState()
        : m_previousState(JSMainThreadExecState::s_mainThreadState)
    {
        ASSERT(isMainThread());
        JSMainThreadExecState::s_mainThreadState = nullptr;
    }

    ~JSMainThreadNullState()
    {
        ASSERT(isMainThread());
        JSMainThreadExecState::s_mainThreadState = m_previousState;
    }

private:
    JSC::ExecState* m_previousState;
    CustomElementReactionStack m_customElementReactionStack;
};

JSC::JSValue functionCallHandlerFromAnyThread(JSC::ExecState*, JSC::JSValue functionObject, JSC::CallType, const JSC::CallData&, JSC::JSValue thisValue, const JSC::ArgList& args, NakedPtr<JSC::Exception>& returnedException);
JSC::JSValue evaluateHandlerFromAnyThread(JSC::ExecState*, const JSC::SourceCode&, JSC::JSValue thisValue, NakedPtr<JSC::Exception>& returnedException);

} // namespace WebCore

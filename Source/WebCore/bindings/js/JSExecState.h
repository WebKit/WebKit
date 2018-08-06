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
#include "ThreadGlobalData.h"
#include <JavaScriptCore/CatchScope.h>
#include <JavaScriptCore/Completion.h>
#include <JavaScriptCore/Microtask.h>
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/MainThread.h>

namespace WebCore {

class InspectorInstrumentationCookie;
class ScriptExecutionContext;

class JSExecState {
    WTF_MAKE_NONCOPYABLE(JSExecState);
    WTF_FORBID_HEAP_ALLOCATION;
    friend class JSMainThreadNullState;
public:
    static JSC::ExecState* currentState()
    {
        return threadGlobalData().currentState();
    };
    
    static JSC::JSValue call(JSC::ExecState* exec, JSC::JSValue functionObject, JSC::CallType callType, const JSC::CallData& callData, JSC::JSValue thisValue, const JSC::ArgList& args, NakedPtr<JSC::Exception>& returnedException)
    {
        JSExecState currentState(exec);
        return JSC::call(exec, functionObject, callType, callData, thisValue, args, returnedException);
    };

    static JSC::JSValue evaluate(JSC::ExecState* exec, const JSC::SourceCode& source, JSC::JSValue thisValue, NakedPtr<JSC::Exception>& returnedException)
    {
        JSExecState currentState(exec);
        return JSC::evaluate(exec, source, thisValue, returnedException);
    };

    static JSC::JSValue evaluate(JSC::ExecState* exec, const JSC::SourceCode& source, JSC::JSValue thisValue = JSC::JSValue())
    {
        NakedPtr<JSC::Exception> unused;
        return evaluate(exec, source, thisValue, unused);
    };

    static JSC::JSValue profiledCall(JSC::ExecState* exec, JSC::ProfilingReason reason, JSC::JSValue functionObject, JSC::CallType callType, const JSC::CallData& callData, JSC::JSValue thisValue, const JSC::ArgList& args, NakedPtr<JSC::Exception>& returnedException)
    {
        JSExecState currentState(exec);
        return JSC::profiledCall(exec, reason, functionObject, callType, callData, thisValue, args, returnedException);
    }

    static JSC::JSValue profiledEvaluate(JSC::ExecState* exec, JSC::ProfilingReason reason, const JSC::SourceCode& source, JSC::JSValue thisValue, NakedPtr<JSC::Exception>& returnedException)
    {
        JSExecState currentState(exec);
        return JSC::profiledEvaluate(exec, reason, source, thisValue, returnedException);
    }

    static JSC::JSValue profiledEvaluate(JSC::ExecState* exec, JSC::ProfilingReason reason, const JSC::SourceCode& source, JSC::JSValue thisValue = JSC::JSValue())
    {
        NakedPtr<JSC::Exception> unused;
        return profiledEvaluate(exec, reason, source, thisValue, unused);
    }

    static void runTask(JSC::ExecState* exec, JSC::Microtask& task)
    {
        JSExecState currentState(exec);
        task.run(exec);
    }

    static JSC::JSInternalPromise& loadModule(JSC::ExecState& state, const String& moduleName, JSC::JSValue parameters, JSC::JSValue scriptFetcher)
    {
        JSExecState currentState(&state);
        return *JSC::loadModule(&state, moduleName, parameters, scriptFetcher);
    }

    static JSC::JSInternalPromise& loadModule(JSC::ExecState& state, const JSC::SourceCode& sourceCode, JSC::JSValue scriptFetcher)
    {
        JSExecState currentState(&state);
        return *JSC::loadModule(&state, sourceCode, scriptFetcher);
    }

    static JSC::JSValue linkAndEvaluateModule(JSC::ExecState& state, const JSC::Identifier& moduleKey, JSC::JSValue scriptFetcher, NakedPtr<JSC::Exception>& returnedException)
    {
        JSC::VM& vm = state.vm();
        auto scope = DECLARE_CATCH_SCOPE(vm);
    
        JSExecState currentState(&state);
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
    explicit JSExecState(JSC::ExecState* exec)
        : m_previousState(currentState())
        , m_lock(exec)
    {
        setCurrentState(exec);
    };

    ~JSExecState()
    {
        JSC::VM& vm = currentState()->vm();
        auto scope = DECLARE_CATCH_SCOPE(vm);
        scope.assertNoException();

        JSC::ExecState* state = currentState();
        bool didExitJavaScript = state && !m_previousState;

        setCurrentState(m_previousState);

        if (didExitJavaScript)
            didLeaveScriptContext(state);
    }

    static void setCurrentState(JSC::ExecState* state)
    {
        threadGlobalData().setCurrentState(state);
    }

    template<typename Type, Type jsType, typename DataType> static InspectorInstrumentationCookie instrumentFunctionInternal(ScriptExecutionContext*, Type, const DataType&);

    JSC::ExecState* m_previousState;
    JSC::JSLockHolder m_lock;

    static void didLeaveScriptContext(JSC::ExecState*);
};

// Null state prevents origin security checks.
// Used by non-JavaScript bindings (ObjC, GObject).
class JSMainThreadNullState {
    WTF_MAKE_NONCOPYABLE(JSMainThreadNullState);
    WTF_FORBID_HEAP_ALLOCATION;
public:
    explicit JSMainThreadNullState()
        : m_previousState(JSExecState::currentState())
        , m_customElementReactionStack(m_previousState)
    {
        ASSERT(isMainThread());
        JSExecState::setCurrentState(nullptr);
    }

    ~JSMainThreadNullState()
    {
        ASSERT(isMainThread());
        JSExecState::setCurrentState(m_previousState);
    }

private:
    JSC::ExecState* m_previousState;
    CustomElementReactionStack m_customElementReactionStack;
};

JSC::JSValue functionCallHandlerFromAnyThread(JSC::ExecState*, JSC::JSValue functionObject, JSC::CallType, const JSC::CallData&, JSC::JSValue thisValue, const JSC::ArgList& args, NakedPtr<JSC::Exception>& returnedException);
JSC::JSValue evaluateHandlerFromAnyThread(JSC::ExecState*, const JSC::SourceCode&, JSC::JSValue thisValue, NakedPtr<JSC::Exception>& returnedException);

} // namespace WebCore

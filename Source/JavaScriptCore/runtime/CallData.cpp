/*
 * Copyright (C) 2008-2019 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "CallData.h"

#include "CatchScope.h"
#include "Interpreter.h"
#include "JSCInlines.h"
#include "JSFunction.h"
#include "ScriptProfilingScope.h"

namespace JSC {

JSValue call(ExecState* exec, JSValue functionObject, const ArgList& args, const char* errorMessage)
{
    return call(exec, functionObject, functionObject, args, errorMessage);
}

JSValue call(ExecState* exec, JSValue functionObject, JSValue thisValue, const ArgList& args, const char* errorMessage)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    CallData callData;
    CallType callType = getCallData(vm, functionObject, callData);
    if (callType == CallType::None)
        return throwTypeError(exec, scope, errorMessage);

    RELEASE_AND_RETURN(scope, call(exec, functionObject, callType, callData, thisValue, args));
}

JSValue call(ExecState* exec, JSValue functionObject, CallType callType, const CallData& callData, JSValue thisValue, const ArgList& args)
{
    VM& vm = exec->vm();
    ASSERT(callType == CallType::JS || callType == CallType::Host);
    return vm.interpreter->executeCall(exec, asObject(functionObject), callType, callData, thisValue, args);
}

JSValue call(ExecState* exec, JSValue functionObject, CallType callType, const CallData& callData, JSValue thisValue, const ArgList& args, NakedPtr<Exception>& returnedException)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);
    JSValue result = call(exec, functionObject, callType, callData, thisValue, args);
    if (UNLIKELY(scope.exception())) {
        returnedException = scope.exception();
        scope.clearException();
        return jsUndefined();
    }
    RELEASE_ASSERT(result);
    return result;
}

JSValue profiledCall(ExecState* exec, ProfilingReason reason, JSValue functionObject, CallType callType, const CallData& callData, JSValue thisValue, const ArgList& args)
{
    VM& vm = exec->vm();
    ScriptProfilingScope profilingScope(vm.vmEntryGlobalObject(exec), reason);
    return call(exec, functionObject, callType, callData, thisValue, args);
}

JSValue profiledCall(ExecState* exec, ProfilingReason reason, JSValue functionObject, CallType callType, const CallData& callData, JSValue thisValue, const ArgList& args, NakedPtr<Exception>& returnedException)
{
    VM& vm = exec->vm();
    ScriptProfilingScope profilingScope(vm.vmEntryGlobalObject(exec), reason);
    return call(exec, functionObject, callType, callData, thisValue, args, returnedException);
}

} // namespace JSC

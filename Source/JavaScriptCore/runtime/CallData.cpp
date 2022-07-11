/*
 * Copyright (C) 2008-2022 Apple Inc. All Rights Reserved.
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
#include "JSObjectInlines.h"
#include "ScriptProfilingScope.h"

namespace JSC {

JSValue call(JSGlobalObject* globalObject, JSValue functionObject, const ArgList& args, ASCIILiteral errorMessage)
{
    return call(globalObject, functionObject, functionObject, args, errorMessage);
}

JSValue call(JSGlobalObject* globalObject, JSValue functionObject, JSValue thisValue, const ArgList& args, ASCIILiteral errorMessage)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto callData = JSC::getCallData(functionObject);
    if (callData.type == CallData::Type::None)
        return throwTypeError(globalObject, scope, errorMessage);

    RELEASE_AND_RETURN(scope, call(globalObject, functionObject, callData, thisValue, args));
}

JSValue call(JSGlobalObject* globalObject, JSValue functionObject, const CallData& callData, JSValue thisValue, const ArgList& args)
{
    VM& vm = globalObject->vm();
    ASSERT(callData.type == CallData::Type::JS || callData.type == CallData::Type::Native);
    return vm.interpreter.executeCall(globalObject, asObject(functionObject), callData, thisValue, args);
}

JSValue call(JSGlobalObject* globalObject, JSValue functionObject, const CallData& callData, JSValue thisValue, const ArgList& args, NakedPtr<Exception>& returnedException)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);
    JSValue result = call(globalObject, functionObject, callData, thisValue, args);
    if (UNLIKELY(scope.exception())) {
        returnedException = scope.exception();
        scope.clearException();
        return jsUndefined();
    }
    RELEASE_ASSERT(result);
    return result;
}

JSValue profiledCall(JSGlobalObject* globalObject, ProfilingReason reason, JSValue functionObject, const CallData& callData, JSValue thisValue, const ArgList& args)
{
    VM& vm = globalObject->vm();
    ScriptProfilingScope profilingScope(vm.deprecatedVMEntryGlobalObject(globalObject), reason);
    return call(globalObject, functionObject, callData, thisValue, args);
}

JSValue profiledCall(JSGlobalObject* globalObject, ProfilingReason reason, JSValue functionObject, const CallData& callData, JSValue thisValue, const ArgList& args, NakedPtr<Exception>& returnedException)
{
    VM& vm = globalObject->vm();
    ScriptProfilingScope profilingScope(vm.deprecatedVMEntryGlobalObject(globalObject), reason);
    return call(globalObject, functionObject, callData, thisValue, args, returnedException);
}

} // namespace JSC

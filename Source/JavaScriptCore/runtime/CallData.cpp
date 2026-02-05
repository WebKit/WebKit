/*
 * Copyright (C) 2008-2023 Apple Inc. All rights reserved.
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

#include "Interpreter.h"
#include "InterpreterInlines.h"
#include "JSObjectInlines.h"
#include "ScriptProfilingScope.h"
#include "TopExceptionScope.h"

#if ASSERT_ENABLED
#include "IntegrityInlines.h"
#endif

namespace JSC {

JSValue call(JSGlobalObject* globalObject, JSValue functionObject, const ArgList& args, ASCIILiteral errorMessage)
{
    return call(globalObject, functionObject, functionObject, args, errorMessage);
}

JSValue call(JSGlobalObject* globalObject, JSValue functionObject, JSValue thisValue, const ArgList& args, ASCIILiteral errorMessage)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto callData = JSC::getCallDataInline(functionObject);
    if (callData.type == CallData::Type::None)
        return throwTypeError(globalObject, scope, errorMessage);

    RELEASE_AND_RETURN(scope, call(globalObject, functionObject, callData, thisValue, args));
}

JSValue call(JSGlobalObject* globalObject, JSValue functionObject, const CallData& callData, JSValue thisValue, const ArgList& args)
{
    VM& vm = globalObject->vm();
    ASSERT_WITH_MESSAGE(callData.type == CallData::Type::JS || callData.type == CallData::Type::Native, "Expected object to be callable but received %d", static_cast<int>(callData.type));
    ASSERT_WITH_MESSAGE(!thisValue.isEmpty(), "Expected thisValue to be non-empty. Use jsUndefined() if you meant to use undefined.");
#if ASSERT_ENABLED
    for (size_t i = 0; i < args.size(); ++i) {
        ASSERT_WITH_MESSAGE(!args.at(i).isEmpty(), "arguments[%zu] is JSValue(). Use jsUndefined() if you meant to make it undefined.", i);
        if (args.at(i).isCell())
            Integrity::auditCell(vm, args.at(i).asCell());
    }
#endif
    return vm.interpreter.executeCall(asObject(functionObject), callData, thisValue, nullptr, args);
}

JSValue call(JSGlobalObject* globalObject, JSValue functionObject, const CallData& callData, JSValue thisValue, const ArgList& args, NakedPtr<Exception>& returnedException)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
    JSValue result = call(globalObject, functionObject, callData, thisValue, args);
    if (scope.exception()) [[unlikely]] {
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

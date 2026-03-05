/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "WebAssemblyPromising.h"

#if ENABLE(WEBASSEMBLY)

#include "ArgList.h"
#include "ExceptionHelpers.h"
#include "JSFunctionWithFields.h"
#include "JSObjectInlines.h"
#include "JSPromise.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(runWebAssemblyPromisingFunction);

// This implements the logic invoked when a function produced by the expression
// 'WebAssembly.promising(wrappedFunction)' is called.

JSC_DEFINE_HOST_FUNCTION(runWebAssemblyPromisingFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* callee = callFrame->jsCallee();
    JSFunctionWithFields* thisFunction = jsCast<JSFunctionWithFields*>(callee);
    ASSERT(thisFunction);
    JSFunction* wrappedFunction = jsCast<JSFunction*>(thisFunction->getField(JSFunctionWithFields::Field::WebAssemblyPromisingWrappedFunction));

    MarkedArgumentBuffer args;
    for (unsigned i = 0; i < callFrame->argumentCount(); ++i)
        args.append(callFrame->uncheckedArgument(i));
    if (args.hasOverflowed()) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    auto callData = JSC::getCallData(wrappedFunction);
    if (callData.type == CallData::Type::None) {
        throwTypeError(globalObject, scope, "Object is not callable"_s);
        return { };
    }

    JSPromise* resultPromise = JSPromise::create(vm, globalObject->promiseStructure());
    JSPIContext context(JSPIContext::Purpose::Promising, vm, callFrame, resultPromise);

    JSValue result = call(globalObject, wrappedFunction, callData, jsUndefined(), args);

    context.deactivate(vm);
    if (scope.exception()) [[unlikely]] {
        // An exception was thrown in wasm code
        JSValue exceptionValue = scope.exception()->value();
        TRY_CLEAR_EXCEPTION(scope, { });
        resultPromise->reject(vm, globalObject, exceptionValue);
    } else if (!context.completion) [[likely]] {
        // The call returned without suspending, result is the returned value
        resultPromise->resolve(globalObject, vm, result);
    }
    // If neither of the the above conditions are true, the call was suspended
    // and all the promises involved are fully hooked up to do the right thing.

    scope.release();
    return JSValue::encode(resultPromise);
}

JSFunctionWithFields* createWebAssemblyPromisingFunction(VM& vm, JSGlobalObject* globalObject, JSFunction* wrappedFunction)
{
    const String name = "WebAssembly.promising"_s;
    NativeExecutable* executable = vm.getHostFunction(runWebAssemblyPromisingFunction, ImplementationVisibility::Public, NoIntrinsic, callHostFunctionAsConstructor, nullptr, name);
    constexpr unsigned length = 1;
    JSFunctionWithFields* function = JSFunctionWithFields::create(vm, globalObject, executable, length, name);
    function->setField(vm, JSFunctionWithFields::Field::WebAssemblyPromisingWrappedFunction, wrappedFunction);
    return function;
}

} // namespace JSC

#endif

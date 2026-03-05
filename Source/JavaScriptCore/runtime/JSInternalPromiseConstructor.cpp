/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "JSInternalPromiseConstructor.h"

#include "JSCBuiltins.h"
#include "JSCInlines.h"
#include "JSInternalPromise.h"
#include "JSInternalPromisePrototype.h"
#include "JSObjectInlines.h"
#include "JSPromiseCombinatorsContext.h"
#include "JSPromiseCombinatorsGlobalContext.h"
#include "Microtask.h"
#include "StructureInlines.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(internalPromiseConstructorFuncInternalAll);

}

#include "JSInternalPromiseConstructor.lut.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSInternalPromiseConstructor);

const ClassInfo JSInternalPromiseConstructor::s_info = { "Function"_s, &Base::s_info, &internalPromiseConstructorTable, nullptr, CREATE_METHOD_TABLE(JSInternalPromiseConstructor) };

/* Source for JSInternalPromiseConstructor.lut.h
@begin internalPromiseConstructorTable
  internalAll  internalPromiseConstructorFuncInternalAll  DontEnum|Function 1
@end
*/

JSInternalPromiseConstructor* JSInternalPromiseConstructor::create(VM& vm, Structure* structure, JSInternalPromisePrototype* promisePrototype)
{
    JSGlobalObject* globalObject = structure->globalObject();
    FunctionExecutable* executable = promiseConstructorInternalPromiseConstructorCodeGenerator(vm);
    JSInternalPromiseConstructor* constructor = new (NotNull, allocateCell<JSInternalPromiseConstructor>(vm)) JSInternalPromiseConstructor(vm, executable, globalObject, structure);
    constructor->finishCreation(vm, promisePrototype);
    return constructor;
}

Structure* JSInternalPromiseConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSFunctionType, StructureFlags), info());
}

JSInternalPromiseConstructor::JSInternalPromiseConstructor(VM& vm, FunctionExecutable* executable, JSGlobalObject* globalObject, Structure* structure)
    : Base(vm, executable, globalObject, structure)
{
}

// InternalPromise.internalAll(array)
// This function is intended to be used in the JSC internals.
// The implementation should take care not to perform the user observable / trappable operations.
// 1. Don't use for-of and iterables. This function only accepts the dense array of the promises.
// 2. Don't look up this.constructor / @@species. Always construct the plain InternalPromise object.
JSC_DEFINE_HOST_FUNCTION(internalPromiseConstructorFuncInternalAll, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* promise = JSInternalPromise::create(vm, globalObject->internalPromiseStructure());

    auto callReject = [&]() -> void {
        Exception* exception = scope.exception();
        ASSERT(exception);
        TRY_CLEAR_EXCEPTION(scope, void());
        scope.release();
        promise->reject(vm, globalObject, exception);
    };

    JSValue arrayValue = callFrame->argument(0);
    JSArray* array = jsDynamicCast<JSArray*>(arrayValue);
    ASSERT(array);

    uint64_t length = array->length();
    if (!length) {
        JSArray* values = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0);
        if (!values) [[unlikely]] {
            throwOutOfMemoryError(globalObject, scope);
            callReject();
            return JSValue::encode(promise);
        }
        scope.release();
        // Use fulfill instead of resolve to avoid looking up the then property.
        promise->fulfill(vm, globalObject, values);
        return JSValue::encode(promise);
    }

    JSArray* values = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), length);
    if (!values) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        callReject();
        return JSValue::encode(promise);
    }

    JSPromiseCombinatorsGlobalContext* globalContext = JSPromiseCombinatorsGlobalContext::create(vm, promise, values, jsNumber(length));
    for (unsigned index = 0; index < length; ++index) {
        JSValue value = array->getIndex(globalObject, index);
        RETURN_IF_EXCEPTION(scope, { });

        auto* nextPromise = jsCast<JSInternalPromise*>(value);
        ASSERT(nextPromise);
        JSPromiseCombinatorsContext* context = JSPromiseCombinatorsContext::create(vm, globalContext, index);

        nextPromise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::InternalPromiseAllResolveJob, promise, context);
        RETURN_IF_EXCEPTION(scope, { });
    }

    return JSValue::encode(promise);
}

} // namespace JSC

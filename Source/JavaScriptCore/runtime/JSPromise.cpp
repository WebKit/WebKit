/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
#include "JSPromise.h"

#include "BuiltinNames.h"
#include "Error.h"
#include "JSCInlines.h"
#include "JSInternalFieldObjectImplInlines.h"
#include "JSPromiseConstructor.h"
#include "Microtask.h"
#include "PromiseTimer.h"

namespace JSC {

const ClassInfo JSPromise::s_info = { "Promise", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSPromise) };

JSPromise* JSPromise::create(VM& vm, Structure* structure)
{
    JSPromise* promise = new (NotNull, allocateCell<JSPromise>(vm.heap)) JSPromise(vm, structure);
    promise->finishCreation(vm);
    return promise;
}

Structure* JSPromise::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSPromiseType, StructureFlags), info());
}

JSPromise::JSPromise(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void JSPromise::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    internalField(static_cast<unsigned>(Field::Flags)).set(vm, this, jsNumber(static_cast<unsigned>(Status::Pending)));
    internalField(static_cast<unsigned>(Field::ReactionsOrResult)).set(vm, this, jsUndefined());
}

void JSPromise::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    auto* thisObject = jsCast<JSPromise*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

auto JSPromise::status(VM&) const -> Status
{
    JSValue value = internalField(static_cast<unsigned>(Field::Flags)).get();
    uint32_t flags = value.asUInt32AsAnyInt();
    return static_cast<Status>(flags & stateMask);
}

JSValue JSPromise::result(VM& vm) const
{
    Status status = this->status(vm);
    if (status == Status::Pending)
        return jsUndefined();
    return internalField(static_cast<unsigned>(Field::ReactionsOrResult)).get();
}

uint32_t JSPromise::flags() const
{
    JSValue value = internalField(static_cast<unsigned>(Field::Flags)).get();
    return value.asUInt32AsAnyInt();
}

bool JSPromise::isHandled(VM&) const
{
    return flags() & isHandledFlag;
}

JSPromise::DeferredData JSPromise::createDeferredData(JSGlobalObject* globalObject, JSPromiseConstructor* promiseConstructor)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSFunction* newPromiseCapabilityFunction = globalObject->newPromiseCapabilityFunction();
    CallData callData;
    CallType callType = JSC::getCallData(globalObject->vm(), newPromiseCapabilityFunction, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(promiseConstructor);
    ASSERT(!arguments.hasOverflowed());
    JSValue deferred = call(globalObject, newPromiseCapabilityFunction, callType, callData, jsUndefined(), arguments);
    RETURN_IF_EXCEPTION(scope, { });

    DeferredData result;
    result.promise = jsCast<JSPromise*>(deferred.get(globalObject, vm.propertyNames->builtinNames().promisePrivateName()));
    RETURN_IF_EXCEPTION(scope, { });
    result.resolve = jsCast<JSFunction*>(deferred.get(globalObject, vm.propertyNames->builtinNames().resolvePrivateName()));
    RETURN_IF_EXCEPTION(scope, { });
    result.reject = jsCast<JSFunction*>(deferred.get(globalObject, vm.propertyNames->builtinNames().rejectPrivateName()));
    RETURN_IF_EXCEPTION(scope, { });

    return result;
}

JSPromise* JSPromise::resolvedPromise(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSFunction* function = globalObject->promiseResolveFunction();
    CallData callData;
    CallType callType = JSC::getCallData(vm, function, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(value);
    auto result = call(globalObject, function, callType, callData, globalObject->promiseConstructor(), arguments);
    RETURN_IF_EXCEPTION(scope, nullptr);
    ASSERT(result.inherits<JSPromise>(vm));
    return jsCast<JSPromise*>(result);
}

static inline void callFunction(JSGlobalObject* globalObject, JSValue function, JSPromise* promise, JSValue value)
{
    CallData callData;
    CallType callType = getCallData(globalObject->vm(), function, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(promise);
    arguments.append(value);
    ASSERT(!arguments.hasOverflowed());

    call(globalObject, function, callType, callData, jsUndefined(), arguments);
}

void JSPromise::resolve(JSGlobalObject* lexicalGlobalObject, JSValue value)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    uint32_t flags = this->flags();
    if (!(flags & isFirstResolvingFunctionCalledFlag)) {
        internalField(static_cast<unsigned>(Field::Flags)).set(vm, this, jsNumber(flags | isFirstResolvingFunctionCalledFlag));
        JSGlobalObject* globalObject = this->globalObject(vm);
        callFunction(lexicalGlobalObject, globalObject->resolvePromiseFunction(), this, value);
        RETURN_IF_EXCEPTION(scope, void());
    }
    vm.promiseTimer->cancelPendingPromise(this);
}

void JSPromise::reject(JSGlobalObject* lexicalGlobalObject, JSValue value)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    uint32_t flags = this->flags();
    if (!(flags & isFirstResolvingFunctionCalledFlag)) {
        internalField(static_cast<unsigned>(Field::Flags)).set(vm, this, jsNumber(flags | isFirstResolvingFunctionCalledFlag));
        JSGlobalObject* globalObject = this->globalObject(vm);
        callFunction(lexicalGlobalObject, globalObject->rejectPromiseFunction(), this, value);
        RETURN_IF_EXCEPTION(scope, void());
    }
    vm.promiseTimer->cancelPendingPromise(this);
}

void JSPromise::reject(JSGlobalObject* lexicalGlobalObject, Exception* reason)
{
    reject(lexicalGlobalObject, reason->value());
}

} // namespace JSC

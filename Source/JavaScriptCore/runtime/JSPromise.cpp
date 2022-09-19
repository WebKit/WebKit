/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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
#include "DeferredWorkTimer.h"
#include "JSCInlines.h"
#include "JSInternalFieldObjectImplInlines.h"
#include "JSPromiseConstructor.h"
#include "Microtask.h"

namespace JSC {

const ClassInfo JSPromise::s_info = { "Promise"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSPromise) };

JSPromise* JSPromise::create(VM& vm, Structure* structure)
{
    JSPromise* promise = new (NotNull, allocateCell<JSPromise>(vm)) JSPromise(vm, structure);
    promise->finishCreation(vm);
    return promise;
}

JSPromise* JSPromise::createWithInitialValues(VM& vm, Structure* structure)
{
    return create(vm, structure);
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
    auto values = initialValues();
    for (unsigned index = 0; index < values.size(); ++index)
        Base::internalField(index).set(vm, this, values[index]);
}

template<typename Visitor>
void JSPromise::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<JSPromise*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

DEFINE_VISIT_CHILDREN(JSPromise);

auto JSPromise::status(VM&) const -> Status
{
    JSValue value = internalField(Field::Flags).get();
    uint32_t flags = value.asUInt32AsAnyInt();
    return static_cast<Status>(flags & stateMask);
}

JSValue JSPromise::result(VM& vm) const
{
    Status status = this->status(vm);
    if (status == Status::Pending)
        return jsUndefined();
    return internalField(Field::ReactionsOrResult).get();
}

uint32_t JSPromise::flags() const
{
    JSValue value = internalField(Field::Flags).get();
    return value.asUInt32AsAnyInt();
}

bool JSPromise::isHandled(VM&) const
{
    return flags() & isHandledFlag;
}

JSValue JSPromise::createNewPromiseCapability(JSGlobalObject* globalObject, JSPromiseConstructor* promiseConstructor)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSFunction* newPromiseCapabilityFunction = globalObject->newPromiseCapabilityFunction();
    auto callData = JSC::getCallData(newPromiseCapabilityFunction);
    ASSERT(callData.type != CallData::Type::None);

    MarkedArgumentBuffer arguments;
    arguments.append(promiseConstructor);
    ASSERT(!arguments.hasOverflowed());
    RELEASE_AND_RETURN(scope, call(globalObject, newPromiseCapabilityFunction, callData, jsUndefined(), arguments));
}

JSPromise::DeferredData JSPromise::convertCapabilityToDeferredData(JSGlobalObject* globalObject, JSValue promiseCapability)
{
    DeferredData result;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    result.promise = promiseCapability.getAs<JSPromise*>(globalObject, vm.propertyNames->builtinNames().promisePrivateName());
    RETURN_IF_EXCEPTION(scope, { });
    result.resolve = promiseCapability.getAs<JSFunction*>(globalObject, vm.propertyNames->builtinNames().resolvePrivateName());
    RETURN_IF_EXCEPTION(scope, { });
    result.reject = promiseCapability.getAs<JSFunction*>(globalObject, vm.propertyNames->builtinNames().rejectPrivateName());
    RETURN_IF_EXCEPTION(scope, { });

    return result;
}

JSPromise::DeferredData JSPromise::createDeferredData(JSGlobalObject* globalObject, JSPromiseConstructor* promiseConstructor)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue capability = createNewPromiseCapability(globalObject, promiseConstructor);
    RETURN_IF_EXCEPTION(scope, { });
    RELEASE_AND_RETURN(scope, convertCapabilityToDeferredData(globalObject, capability));
}

JSPromise* JSPromise::resolvedPromise(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSFunction* function = globalObject->promiseResolveFunction();
    auto callData = JSC::getCallData(function);
    ASSERT(callData.type != CallData::Type::None);

    MarkedArgumentBuffer arguments;
    arguments.append(value);
    ASSERT(!arguments.hasOverflowed());
    auto result = call(globalObject, function, callData, globalObject->promiseConstructor(), arguments);
    RETURN_IF_EXCEPTION(scope, nullptr);
    ASSERT(result.inherits<JSPromise>());
    return jsCast<JSPromise*>(result);
}

// Keep in sync with @rejectPromise in JS.
JSPromise* JSPromise::rejectedPromise(JSGlobalObject* globalObject, JSValue value)
{
    // Because we create a promise in this function, we know that no promise reactions are registered.
    // We can skip triggering them, which completely avoids calling JS functions.
    VM& vm = globalObject->vm();
    JSPromise* promise = JSPromise::create(vm, globalObject->promiseStructure());
    promise->internalField(Field::ReactionsOrResult).set(vm, promise, value);
    promise->internalField(Field::Flags).set(vm, promise, jsNumber(promise->flags() | isFirstResolvingFunctionCalledFlag | static_cast<unsigned>(Status::Rejected)));
    if (globalObject->globalObjectMethodTable()->promiseRejectionTracker)
        globalObject->globalObjectMethodTable()->promiseRejectionTracker(globalObject, promise, JSPromiseRejectionOperation::Reject);
    else
        vm.promiseRejected(promise);
    return promise;
}

static inline void callFunction(JSGlobalObject* globalObject, JSValue function, JSPromise* promise, JSValue value)
{
    auto callData = getCallData(function);
    ASSERT(callData.type != CallData::Type::None);

    MarkedArgumentBuffer arguments;
    arguments.append(promise);
    arguments.append(value);
    ASSERT(!arguments.hasOverflowed());

    call(globalObject, function, callData, jsUndefined(), arguments);
}

void JSPromise::resolve(JSGlobalObject* lexicalGlobalObject, JSValue value)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    uint32_t flags = this->flags();
    ASSERT(!value.inherits<Exception>());
    if (!(flags & isFirstResolvingFunctionCalledFlag)) {
        internalField(Field::Flags).set(vm, this, jsNumber(flags | isFirstResolvingFunctionCalledFlag));
        JSGlobalObject* globalObject = this->globalObject();
        callFunction(lexicalGlobalObject, globalObject->resolvePromiseFunction(), this, value);
        RETURN_IF_EXCEPTION(scope, void());
    }
}

void JSPromise::reject(JSGlobalObject* lexicalGlobalObject, JSValue value)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    uint32_t flags = this->flags();
    ASSERT(!value.inherits<Exception>());
    if (!(flags & isFirstResolvingFunctionCalledFlag)) {
        internalField(Field::Flags).set(vm, this, jsNumber(flags | isFirstResolvingFunctionCalledFlag));
        JSGlobalObject* globalObject = this->globalObject();
        callFunction(lexicalGlobalObject, globalObject->rejectPromiseFunction(), this, value);
        RETURN_IF_EXCEPTION(scope, void());
    }
}

void JSPromise::rejectAsHandled(JSGlobalObject* lexicalGlobalObject, JSValue value)
{
    // Setting isHandledFlag before calling reject since this removes round-trip between JSC and PromiseRejectionTracker, and it does not show an user-observable behavior.
    VM& vm = lexicalGlobalObject->vm();
    uint32_t flags = this->flags();
    if (!(flags & isFirstResolvingFunctionCalledFlag))
        internalField(Field::Flags).set(vm, this, jsNumber(flags | isHandledFlag));
    reject(lexicalGlobalObject, value);
}

void JSPromise::reject(JSGlobalObject* lexicalGlobalObject, Exception* reason)
{
    reject(lexicalGlobalObject, reason->value());
}

void JSPromise::rejectAsHandled(JSGlobalObject* lexicalGlobalObject, Exception* reason)
{
    rejectAsHandled(lexicalGlobalObject, reason->value());
}

JSPromise* JSPromise::rejectWithCaughtException(JSGlobalObject* globalObject, ThrowScope& scope)
{
    VM& vm = globalObject->vm();
    Exception* exception = scope.exception();
    ASSERT(exception);
    if (UNLIKELY(vm.isTerminationException(exception))) {
        scope.release();
        return this;
    }
    scope.clearException();
    scope.release();
    reject(globalObject, exception->value());
    return this;
}

void JSPromise::performPromiseThen(JSGlobalObject* globalObject, JSFunction* onFulFilled, JSFunction* onRejected, JSValue resultCapability)
{
    JSFunction* performPromiseThenFunction = globalObject->performPromiseThenFunction();
    auto callData = JSC::getCallData(performPromiseThenFunction);
    ASSERT(callData.type != CallData::Type::None);

    MarkedArgumentBuffer arguments;
    arguments.append(this);
    arguments.append(onFulFilled);
    arguments.append(onRejected);
    arguments.append(resultCapability);
    arguments.append(jsUndefined());
    ASSERT(!arguments.hasOverflowed());
    call(globalObject, performPromiseThenFunction, callData, jsUndefined(), arguments);
}

} // namespace JSC

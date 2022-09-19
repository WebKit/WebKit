/*
 * Copyright (C) 2021 Igalia S.L.
 * Author: Caitlin Potter <caitp@igalia.com>
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
#include "JSRemoteFunction.h"

#include "ExecutableBaseInlines.h"
#include "JSCInlines.h"
#include "ShadowRealmObject.h"

#include <wtf/Assertions.h>

namespace JSC {

const ClassInfo JSRemoteFunction::s_info = { "Function"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSRemoteFunction) };

JSRemoteFunction::JSRemoteFunction(VM& vm, NativeExecutable* executable, JSGlobalObject* globalObject, Structure* structure, JSObject* targetFunction)
    : Base(vm, executable, globalObject, structure)
    , m_targetFunction(vm, this, targetFunction)
{
}

static JSValue wrapValue(JSGlobalObject* globalObject, JSGlobalObject* targetGlobalObject, JSValue value)
{
    VM& vm = globalObject->vm();

    if (value.isPrimitive())
        return value;

    if (value.isCallable()) {
        JSObject* targetFunction = static_cast<JSObject*>(value.asCell());
        return JSRemoteFunction::tryCreate(targetGlobalObject, vm, targetFunction);
    }

    return JSValue();
}

static inline JSValue wrapArgument(JSGlobalObject* globalObject, JSGlobalObject* targetGlobalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue result = wrapValue(globalObject, targetGlobalObject, value);
    RETURN_IF_EXCEPTION(scope, { });
    if (!result)
        throwTypeError(globalObject, scope, "value passing between realms must be callable or primitive"_s);
    RELEASE_AND_RETURN(scope, result);
}

static inline JSValue wrapReturnValue(JSGlobalObject* globalObject, JSGlobalObject* targetGlobalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue result = wrapValue(globalObject, targetGlobalObject, value);
    RETURN_IF_EXCEPTION(scope, { });
    if (!result)
        throwTypeError(globalObject, scope, "value passing between realms must be callable or primitive"_s);
    RELEASE_AND_RETURN(scope, result);
}

JSC_DEFINE_HOST_FUNCTION(remoteFunctionCallForJSFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSRemoteFunction* remoteFunction = jsCast<JSRemoteFunction*>(callFrame->jsCallee());
    ASSERT(remoteFunction->isRemoteFunction());
    JSFunction* targetFunction = jsCast<JSFunction*>(remoteFunction->targetFunction());
    JSGlobalObject* targetGlobalObject = targetFunction->globalObject();

    MarkedArgumentBuffer args;
    auto clearArgOverflowCheckAndReturnAbortValue = [&] () -> EncodedJSValue {
        // This is only called because we'll be imminently returning due to an
        // exception i.e. we won't be using the args, and we don't care if they
        // overflowed. However, we still need to call overflowCheckNotNeeded()
        // to placate an ASSERT in the MarkedArgumentBuffer destructor.
        args.overflowCheckNotNeeded();
        return { };
    };
    for (unsigned i = 0; i < callFrame->argumentCount(); ++i) {
        JSValue wrappedValue = wrapArgument(globalObject, targetGlobalObject, callFrame->uncheckedArgument(i));
        RETURN_IF_EXCEPTION(scope, clearArgOverflowCheckAndReturnAbortValue());
        args.append(wrappedValue);
    }
    if (UNLIKELY(args.hasOverflowed())) {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }
    ExecutableBase* executable = targetFunction->executable();
    if (executable->hasJITCodeForCall()) {
        // Force the executable to cache its arity entrypoint.
        executable->entrypointFor(CodeForCall, MustCheckArity);
    }

    auto callData = JSC::getCallData(targetFunction);
    ASSERT(callData.type != CallData::Type::None);
    auto result = call(targetGlobalObject, targetFunction, callData, jsUndefined(), args);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(wrapReturnValue(globalObject, globalObject, result)));
}

JSC_DEFINE_HOST_FUNCTION(remoteFunctionCallGeneric, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSRemoteFunction* remoteFunction = jsCast<JSRemoteFunction*>(callFrame->jsCallee());
    ASSERT(remoteFunction->isRemoteFunction());
    JSObject* targetFunction = remoteFunction->targetFunction();
    JSGlobalObject* targetGlobalObject = targetFunction->globalObject();

    MarkedArgumentBuffer args;
    auto clearArgOverflowCheckAndReturnAbortValue = [&] () -> EncodedJSValue {
        // This is only called because we'll be imminently returning due to an
        // exception i.e. we won't be using the args, and we don't care if they
        // overflowed. However, we still need to call overflowCheckNotNeeded()
        // to placate an ASSERT in the MarkedArgumentBuffer destructor.
        args.overflowCheckNotNeeded();
        return { };
    };
    for (unsigned i = 0; i < callFrame->argumentCount(); ++i) {
        JSValue wrappedValue = wrapArgument(globalObject, targetGlobalObject, callFrame->uncheckedArgument(i));
        RETURN_IF_EXCEPTION(scope, clearArgOverflowCheckAndReturnAbortValue());
        args.append(wrappedValue);
    }
    if (UNLIKELY(args.hasOverflowed())) {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    auto callData = JSC::getCallData(targetFunction);
    ASSERT(callData.type != CallData::Type::None);
    auto result = call(targetGlobalObject, targetFunction, callData, jsUndefined(), args);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(wrapReturnValue(globalObject, targetGlobalObject, result)));
}

JSC_DEFINE_HOST_FUNCTION(isRemoteFunction, (JSGlobalObject*, CallFrame* callFrame))
{
    ASSERT(callFrame->argumentCount() == 1);
    JSValue value = callFrame->uncheckedArgument(0);
    return JSValue::encode(jsBoolean(JSC::isRemoteFunction(value)));
}

JSC_DEFINE_HOST_FUNCTION(createRemoteFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(callFrame->argumentCount() == 2);
    JSValue targetFunction = callFrame->uncheckedArgument(0);
    ASSERT(targetFunction.isCallable());

    JSObject* targetCallable = jsCast<JSObject*>(targetFunction.asCell());
    JSGlobalObject* destinationGlobalObject = globalObject;
    if (!callFrame->uncheckedArgument(1).isUndefinedOrNull()) {
        if (auto shadowRealm = jsDynamicCast<ShadowRealmObject*>(callFrame->uncheckedArgument(1)))
            destinationGlobalObject = shadowRealm->globalObject();
        else
            destinationGlobalObject = jsCast<JSGlobalObject*>(callFrame->uncheckedArgument(1));
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(JSRemoteFunction::tryCreate(destinationGlobalObject, vm, targetCallable)));
}

inline Structure* getRemoteFunctionStructure(JSGlobalObject* globalObject)
{
    // FIXME: implement globalObject-aware structure caching
    return globalObject->remoteFunctionStructure();
}

JSRemoteFunction* JSRemoteFunction::tryCreate(JSGlobalObject* globalObject, VM& vm, JSObject* targetCallable)
{
    ASSERT(targetCallable && targetCallable->isCallable());
    if (auto remote = jsDynamicCast<JSRemoteFunction*>(targetCallable)) {
        targetCallable = remote->targetFunction();
        ASSERT(!JSC::isRemoteFunction(targetCallable));
    }

    bool isJSFunction = getJSFunction(targetCallable);
    NativeExecutable* executable = vm.getRemoteFunction(isJSFunction);
    Structure* structure = getRemoteFunctionStructure(globalObject);
    JSRemoteFunction* function = new (NotNull, allocateCell<JSRemoteFunction>(vm)) JSRemoteFunction(vm, executable, globalObject, structure, targetCallable);

    function->finishCreation(globalObject, vm);
    return function;
}

// https://tc39.es/proposal-shadowrealm/#sec-copynameandlength
void JSRemoteFunction::copyNameAndLength(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    PropertySlot slot(m_targetFunction.get(), PropertySlot::InternalMethodType::GetOwnProperty);
    bool targetHasLength = m_targetFunction->getOwnPropertySlotInline(globalObject, vm.propertyNames->length, slot);
    RETURN_IF_EXCEPTION(scope, void());

    if (targetHasLength) {
        JSValue targetLength;
        if (LIKELY(!slot.isTaintedByOpaqueObject()))
            targetLength = slot.getValue(globalObject, vm.propertyNames->length);
        else
            targetLength = m_targetFunction->get(globalObject, vm.propertyNames->length);
        RETURN_IF_EXCEPTION(scope, void());
        double targetLengthAsInt = targetLength.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, void());
        m_length = std::max(targetLengthAsInt, 0.0);
    }

    JSValue targetName = m_targetFunction->get(globalObject, vm.propertyNames->name);
    RETURN_IF_EXCEPTION(scope, void());
    if (targetName.isString()) {
        auto* targetString = asString(targetName);
        targetString->value(globalObject); // Resolving rope.
        RETURN_IF_EXCEPTION(scope, void());
        m_nameMayBeNull.set(vm, this, targetString);
    }
    ASSERT(!m_nameMayBeNull || !m_nameMayBeNull->isRope());
}

void JSRemoteFunction::finishCreation(JSGlobalObject* globalObject, VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    auto scope = DECLARE_THROW_SCOPE(vm);
    copyNameAndLength(globalObject);

    auto* exception = scope.exception();
    if (UNLIKELY(exception && !vm.isTerminationException(exception))) {
        scope.clearException();
        throwTypeError(globalObject, scope, "wrapping returned function throws an error"_s);
    }
}

template<typename Visitor>
void JSRemoteFunction::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSRemoteFunction* thisObject = jsCast<JSRemoteFunction*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    visitor.append(thisObject->m_targetFunction);
    visitor.append(thisObject->m_nameMayBeNull);
}

DEFINE_VISIT_CHILDREN(JSRemoteFunction);

} // namespace JSC

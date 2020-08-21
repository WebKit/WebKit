/*
 * Copyright (C) 2011, 2016-2017 Apple Inc. All rights reserved.
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
#include "JSBoundFunction.h"

#include "ExecutableBaseInlines.h"
#include "JSCInlines.h"

namespace JSC {

const ClassInfo JSBoundFunction::s_info = { "Function", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSBoundFunction) };

EncodedJSValue JSC_HOST_CALL boundThisNoArgsFunctionCall(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    JSBoundFunction* boundFunction = jsCast<JSBoundFunction*>(callFrame->jsCallee());

    JSImmutableButterfly* boundArgs = boundFunction->boundArgs();

    MarkedArgumentBuffer args;
    if (boundArgs) {
        // Starts with 1 since the first one is |this|.
        for (unsigned i = 1; i < boundArgs->length(); ++i)
            args.append(boundArgs->get(i));
    }
    for (unsigned i = 0; i < callFrame->argumentCount(); ++i)
        args.append(callFrame->uncheckedArgument(i));
    RELEASE_ASSERT(!args.hasOverflowed());

    JSFunction* targetFunction = jsCast<JSFunction*>(boundFunction->targetFunction());
    ExecutableBase* executable = targetFunction->executable();
    if (executable->hasJITCodeForCall()) {
        // Force the executable to cache its arity entrypoint.
        executable->entrypointFor(CodeForCall, MustCheckArity);
    }
    auto callData = getCallData(globalObject->vm(), targetFunction);
    ASSERT(callData.type != CallData::Type::None);
    return JSValue::encode(call(globalObject, targetFunction, callData, boundFunction->boundThis(), args));
}

EncodedJSValue JSC_HOST_CALL boundFunctionCall(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSBoundFunction* boundFunction = jsCast<JSBoundFunction*>(callFrame->jsCallee());

    JSImmutableButterfly* boundArgs = boundFunction->boundArgs();

    MarkedArgumentBuffer args;
    if (boundArgs) {
        // Starts with 1 since the first one is |this|.
        for (unsigned i = 1; i < boundArgs->length(); ++i)
            args.append(boundArgs->get(i));
    }
    for (unsigned i = 0; i < callFrame->argumentCount(); ++i)
        args.append(callFrame->uncheckedArgument(i));
    if (UNLIKELY(args.hasOverflowed())) {
        throwOutOfMemoryError(globalObject, scope);
        return encodedJSValue();
    }

    JSObject* targetFunction = boundFunction->targetFunction();
    auto callData = getCallData(vm, targetFunction);
    ASSERT(callData.type != CallData::Type::None);
    RELEASE_AND_RETURN(scope, JSValue::encode(call(globalObject, targetFunction, callData, boundFunction->boundThis(), args)));
}

EncodedJSValue JSC_HOST_CALL boundThisNoArgsFunctionConstruct(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    JSBoundFunction* boundFunction = jsCast<JSBoundFunction*>(callFrame->jsCallee());

    JSImmutableButterfly* boundArgs = boundFunction->boundArgs();

    MarkedArgumentBuffer args;
    if (boundArgs) {
        // Starts with 1 since the first one is |this|.
        for (unsigned i = 1; i < boundArgs->length(); ++i)
            args.append(boundArgs->get(i));
    }
    for (unsigned i = 0; i < callFrame->argumentCount(); ++i)
        args.append(callFrame->uncheckedArgument(i));
    RELEASE_ASSERT(!args.hasOverflowed());

    JSFunction* targetFunction = jsCast<JSFunction*>(boundFunction->targetFunction());
    auto constructData = getConstructData(globalObject->vm(), targetFunction);
    ASSERT(constructData.type != CallData::Type::None);

    JSValue newTarget = callFrame->newTarget();
    if (newTarget == boundFunction)
        newTarget = targetFunction;
    return JSValue::encode(construct(globalObject, targetFunction, constructData, args, newTarget));
}

EncodedJSValue JSC_HOST_CALL boundFunctionConstruct(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSBoundFunction* boundFunction = jsCast<JSBoundFunction*>(callFrame->jsCallee());

    JSImmutableButterfly* boundArgs = boundFunction->boundArgs();

    MarkedArgumentBuffer args;
    if (boundArgs) {
        // Starts with 1 since the first one is |this|.
        for (unsigned i = 1; i < boundArgs->length(); ++i)
            args.append(boundArgs->get(i));
    }
    for (unsigned i = 0; i < callFrame->argumentCount(); ++i)
        args.append(callFrame->uncheckedArgument(i));
    if (UNLIKELY(args.hasOverflowed())) {
        throwOutOfMemoryError(globalObject, scope);
        return encodedJSValue();
    }

    JSObject* targetFunction = boundFunction->targetFunction();
    auto constructData = getConstructData(vm, targetFunction);
    ASSERT(constructData.type != CallData::Type::None);

    JSValue newTarget = callFrame->newTarget();
    if (newTarget == boundFunction)
        newTarget = targetFunction;
    RELEASE_AND_RETURN(scope, JSValue::encode(construct(globalObject, targetFunction, constructData, args, newTarget)));
}

EncodedJSValue JSC_HOST_CALL isBoundFunction(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return JSValue::encode(JSValue(static_cast<bool>(jsDynamicCast<JSBoundFunction*>(globalObject->vm(), callFrame->uncheckedArgument(0)))));
}

EncodedJSValue JSC_HOST_CALL hasInstanceBoundFunction(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    JSBoundFunction* boundObject = jsCast<JSBoundFunction*>(callFrame->uncheckedArgument(0));
    JSValue value = callFrame->uncheckedArgument(1);

    return JSValue::encode(jsBoolean(boundObject->targetFunction()->hasInstance(globalObject, value)));
}

inline Structure* getBoundFunctionStructure(VM& vm, JSGlobalObject* globalObject, JSObject* targetFunction)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue prototype = targetFunction->getPrototype(vm, globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);
    JSFunction* targetJSFunction = jsDynamicCast<JSFunction*>(vm, targetFunction);

    // We only cache the structure of the bound function if the bindee is a JSFunction since there
    // isn't any good place to put the structure on Internal Functions.
    if (targetJSFunction) {
        Structure* structure = targetJSFunction->ensureRareData(vm)->getBoundFunctionStructure();
        if (structure && structure->storedPrototype() == prototype && structure->globalObject() == globalObject)
            return structure;
    }

    Structure* result = globalObject->boundFunctionStructure();

    // It would be nice if the structure map was keyed global objects in addition to the other things. Unfortunately, it is not
    // currently. Whoever works on caching structure changes for prototype transitions should consider this problem as well.
    // See: https://bugs.webkit.org/show_bug.cgi?id=152738
    if (prototype.isObject() && prototype.getObject()->globalObject(vm) == globalObject) {
        result = vm.structureCache.emptyStructureForPrototypeFromBaseStructure(globalObject, prototype.getObject(), result);
        ASSERT_WITH_SECURITY_IMPLICATION(result->globalObject() == globalObject);
    } else
        result = Structure::create(vm, globalObject, prototype, result->typeInfo(), result->classInfo());

    if (targetJSFunction)
        targetJSFunction->ensureRareData(vm)->setBoundFunctionStructure(vm, result);

    return result;
}

JSBoundFunction* JSBoundFunction::create(VM& vm, JSGlobalObject* globalObject, JSObject* targetFunction, JSValue boundThis, JSImmutableButterfly* boundArgs, double length, JSString* nameMayBeNull)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (nameMayBeNull) {
        nameMayBeNull->value(globalObject); // Resolving rope.
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    bool isJSFunction = getJSFunction(targetFunction);
    bool canConstruct = targetFunction->isConstructor(vm);

    NativeExecutable* executable = vm.getBoundFunction(isJSFunction, canConstruct);
    Structure* structure = getBoundFunctionStructure(vm, globalObject, targetFunction);
    RETURN_IF_EXCEPTION(scope, nullptr);
    JSBoundFunction* function = new (NotNull, allocateCell<JSBoundFunction>(vm.heap)) JSBoundFunction(vm, executable, globalObject, structure, targetFunction, boundThis, boundArgs, nameMayBeNull, length);

    function->finishCreation(vm);
    return function;
}

bool JSBoundFunction::customHasInstance(JSObject* object, JSGlobalObject* globalObject, JSValue value)
{
    return jsCast<JSBoundFunction*>(object)->m_targetFunction->hasInstance(globalObject, value);
}

JSBoundFunction::JSBoundFunction(VM& vm, NativeExecutable* executable, JSGlobalObject* globalObject, Structure* structure, JSObject* targetFunction, JSValue boundThis, JSImmutableButterfly* boundArgs, JSString* nameMayBeNull, double length)
    : Base(vm, executable, globalObject, structure)
    , m_targetFunction(vm, this, targetFunction)
    , m_boundThis(vm, this, boundThis)
    , m_boundArgs(vm, this, boundArgs, WriteBarrier<JSImmutableButterfly>::MayBeNull)
    , m_nameMayBeNull(vm, this, nameMayBeNull, WriteBarrier<JSString>::MayBeNull)
    , m_length(length)
{
    ASSERT(!m_nameMayBeNull || !m_nameMayBeNull->isRope());
    ASSERT(m_length >= 0);
}

JSArray* JSBoundFunction::boundArgsCopy(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSArray* result = constructEmptyArray(this->globalObject(), nullptr);
    RETURN_IF_EXCEPTION(scope, nullptr);
    if (m_boundArgs) {
        // Starts with 1 since the first one is bound |this|.
        for (unsigned i = 1; i < m_boundArgs->length(); ++i) {
            result->push(globalObject, m_boundArgs->get(i));
            RETURN_IF_EXCEPTION(scope, nullptr);
        }
    }
    return result;
}

void JSBoundFunction::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
}

void JSBoundFunction::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSBoundFunction* thisObject = jsCast<JSBoundFunction*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    visitor.append(thisObject->m_targetFunction);
    visitor.append(thisObject->m_boundThis);
    visitor.append(thisObject->m_boundArgs);
    visitor.append(thisObject->m_nameMayBeNull);
}

} // namespace JSC

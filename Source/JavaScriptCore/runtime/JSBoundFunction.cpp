/*
 * Copyright (C) 2011-2024 Apple Inc. All rights reserved.
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

#include "DeferTermination.h"
#include "ExecutableBaseInlines.h"
#include "FunctionPrototype.h"
#include "JSCInlines.h"
#include "VMTrapsInlines.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

const ClassInfo JSBoundFunction::s_info = { "Function"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSBoundFunction) };

JSC_DEFINE_HOST_FUNCTION(boundThisNoArgsFunctionCall, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSBoundFunction* boundFunction = jsCast<JSBoundFunction*>(callFrame->jsCallee());

    MarkedArgumentBuffer args;
    unsigned boundArgsLength = boundFunction->boundArgsLength();
    unsigned finalArgsCount = boundArgsLength + callFrame->argumentCount();
    if (finalArgsCount) {
        args.ensureCapacity(finalArgsCount);
        if (boundArgsLength) {
            boundFunction->forEachBoundArg([&](JSValue argument) -> IterationStatus {
                args.append(argument);
                return IterationStatus::Continue;
            });
        }
        for (unsigned i = 0; i < callFrame->argumentCount(); ++i)
            args.append(callFrame->uncheckedArgument(i));
        RELEASE_ASSERT(!args.hasOverflowed());
    }

    JSFunction* targetFunction = jsCast<JSFunction*>(boundFunction->targetFunction());
    ExecutableBase* executable = targetFunction->executable();
    if (executable->hasJITCodeForCall()) {
        // Force the executable to cache its arity entrypoint.
        executable->entrypointFor(CodeForCall, MustCheckArity);
    }
    auto callData = JSC::getCallData(targetFunction);
    ASSERT(callData.type != CallData::Type::None);
    return JSValue::encode(call(globalObject, targetFunction, callData, boundFunction->boundThis(), args));
}

JSC_DEFINE_HOST_FUNCTION(boundFunctionCall, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSBoundFunction* boundFunction = jsCast<JSBoundFunction*>(callFrame->jsCallee());

    MarkedArgumentBuffer args;
    unsigned boundArgsLength = boundFunction->boundArgsLength();
    unsigned finalArgsCount = boundArgsLength + callFrame->argumentCount();
    if (finalArgsCount) {
        args.ensureCapacity(finalArgsCount);
        if (boundArgsLength) {
            boundFunction->forEachBoundArg([&](JSValue argument) -> IterationStatus {
                args.append(argument);
                return IterationStatus::Continue;
            });
        }
        for (unsigned i = 0; i < callFrame->argumentCount(); ++i)
            args.append(callFrame->uncheckedArgument(i));
        if (UNLIKELY(args.hasOverflowed())) {
            throwOutOfMemoryError(globalObject, scope);
            return encodedJSValue();
        }
    }

    JSObject* targetFunction = boundFunction->targetFunction();
    auto callData = JSC::getCallData(targetFunction);
    ASSERT(callData.type != CallData::Type::None);
    RELEASE_AND_RETURN(scope, JSValue::encode(call(globalObject, targetFunction, callData, boundFunction->boundThis(), args)));
}

JSC_DEFINE_HOST_FUNCTION(boundFunctionConstruct, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSBoundFunction* boundFunction = jsCast<JSBoundFunction*>(callFrame->jsCallee());

    JSObject* targetFunction = boundFunction->targetFunction();
    auto constructData = JSC::getConstructData(targetFunction);
    if (UNLIKELY(constructData.type == CallData::Type::None))
        return throwVMError(globalObject, scope, createNotAConstructorError(globalObject, boundFunction));

    MarkedArgumentBuffer args;
    unsigned boundArgsLength = boundFunction->boundArgsLength();
    unsigned finalArgsCount = boundArgsLength + callFrame->argumentCount();
    if (finalArgsCount) {
        args.ensureCapacity(finalArgsCount);
        if (boundArgsLength) {
            boundFunction->forEachBoundArg([&](JSValue argument) -> IterationStatus {
                args.append(argument);
                return IterationStatus::Continue;
            });
        }
        for (unsigned i = 0; i < callFrame->argumentCount(); ++i)
            args.append(callFrame->uncheckedArgument(i));
        if (UNLIKELY(args.hasOverflowed())) {
            throwOutOfMemoryError(globalObject, scope);
            return encodedJSValue();
        }
    }

    JSValue newTarget = callFrame->newTarget();
    if (newTarget == boundFunction)
        newTarget = targetFunction;
    RELEASE_AND_RETURN(scope, JSValue::encode(construct(globalObject, targetFunction, constructData, args, newTarget)));
}

JSC_DEFINE_HOST_FUNCTION(isBoundFunction, (JSGlobalObject*, CallFrame* callFrame))
{
    return JSValue::encode(JSValue(static_cast<bool>(jsDynamicCast<JSBoundFunction*>(callFrame->uncheckedArgument(0)))));
}

JSC_DEFINE_HOST_FUNCTION(hasInstanceBoundFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSBoundFunction* boundObject = jsCast<JSBoundFunction*>(callFrame->uncheckedArgument(0));
    JSValue value = callFrame->uncheckedArgument(1);
    return JSValue::encode(jsBoolean(boundObject->targetFunction()->hasInstance(globalObject, value)));
}

inline Structure* getBoundFunctionStructure(VM& vm, JSGlobalObject* globalObject, JSObject* targetFunction)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSFunction* targetJSFunction = jsDynamicCast<JSFunction*>(targetFunction);
    if (LIKELY(targetJSFunction && targetJSFunction->getPrototypeDirect() == globalObject->functionPrototype()))
        return globalObject->boundFunctionStructure();

    JSValue prototype = targetFunction->getPrototype(vm, globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

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
    if (prototype.isObject() && prototype.getObject()->globalObject() == globalObject) {
        result = globalObject->structureCache().emptyStructureForPrototypeFromBaseStructure(globalObject, prototype.getObject(), result);
        ASSERT_WITH_SECURITY_IMPLICATION(result->globalObject() == globalObject);
    } else
        result = Structure::create(vm, globalObject, prototype, result->typeInfo(), result->classInfoForCells());

    if (targetJSFunction)
        targetJSFunction->ensureRareData(vm)->setBoundFunctionStructure(vm, result);

    return result;
}

JSBoundFunction* JSBoundFunction::create(VM& vm, JSGlobalObject* globalObject, JSObject* targetFunction, JSValue boundThis, ArgList args, double length, JSString* nameMayBeNull)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (nameMayBeNull) {
        nameMayBeNull->value(globalObject); // Resolving rope.
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    JSValue boundArgs[maxEmbeddedArgs] { };
    if (!args.isEmpty()) {
        if (args.size() <= maxEmbeddedArgs) {
            for (unsigned index = 0, size = args.size(); index < size; ++index)
                boundArgs[index] = args.at(index);
        } else {
            JSImmutableButterfly* butterfly = JSImmutableButterfly::tryCreate(vm, vm.immutableButterflyStructures[arrayIndexFromIndexingType(CopyOnWriteArrayWithContiguous) - NumberOfIndexingShapes].get(), args.size());
            if (UNLIKELY(!butterfly)) {
                throwOutOfMemoryError(globalObject, scope);
                return nullptr;
            }
            for (unsigned index = 0, size = args.size(); index < size; ++index)
                butterfly->setIndex(vm, index, args.at(index));
            boundArgs[0] = butterfly;
        }
    }

    bool isJSFunction = getJSFunction(targetFunction);
    NativeExecutable* executable = vm.getBoundFunction(isJSFunction);
    Structure* structure = getBoundFunctionStructure(vm, globalObject, targetFunction);
    RETURN_IF_EXCEPTION(scope, nullptr);
    JSBoundFunction* function = new (NotNull, allocateCell<JSBoundFunction>(vm)) JSBoundFunction(vm, executable, globalObject, structure, targetFunction, boundThis, args.size(), boundArgs[0], boundArgs[1], boundArgs[2], nameMayBeNull, length);

    function->finishCreation(vm);
    return function;
}

JSBoundFunction* JSBoundFunction::createRaw(VM& vm, JSGlobalObject* globalObject, JSFunction* targetFunction, unsigned boundArgsLength, JSValue boundThis, JSValue arg0, JSValue arg1, JSValue arg2)
{
    NativeExecutable* executable = vm.getBoundFunction(/* isJSFunction */ true);
    JSBoundFunction* function = new (NotNull, allocateCell<JSBoundFunction>(vm)) JSBoundFunction(vm, executable, globalObject, globalObject->boundFunctionStructure(), targetFunction, boundThis, boundArgsLength, arg0, arg1, arg2, nullptr, PNaN);
    function->finishCreation(vm);
    return function;
}

bool JSBoundFunction::customHasInstance(JSObject* object, JSGlobalObject* globalObject, JSValue value)
{
    return jsCast<JSBoundFunction*>(object)->m_targetFunction->hasInstance(globalObject, value);
}

JSBoundFunction::JSBoundFunction(VM& vm, NativeExecutable* executable, JSGlobalObject* globalObject, Structure* structure, JSObject* targetFunction, JSValue boundThis, unsigned boundArgsLength, JSValue arg0, JSValue arg1, JSValue arg2, JSString* nameMayBeNull, double length)
    : Base(vm, executable, globalObject, structure)
    , m_targetFunction(targetFunction, WriteBarrierEarlyInit)
    , m_boundThis(boundThis, WriteBarrierEarlyInit)
    , m_nameMayBeNull(nameMayBeNull, WriteBarrierEarlyInit)
    , m_length(length)
    , m_boundArgsLength(boundArgsLength)
{
    m_boundArgs[0].setWithoutWriteBarrier(arg0);
    m_boundArgs[1].setWithoutWriteBarrier(arg1);
    m_boundArgs[2].setWithoutWriteBarrier(arg2);
}

JSArray* JSBoundFunction::boundArgsCopy(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSArray* result = constructEmptyArray(this->globalObject(), nullptr);
    RETURN_IF_EXCEPTION(scope, nullptr);
    forEachBoundArg([&](JSValue argument) -> IterationStatus {
        auto scope = DECLARE_THROW_SCOPE(vm);
        result->push(globalObject, argument);
        RETURN_IF_EXCEPTION(scope, IterationStatus::Done);
        return IterationStatus::Continue;
    });
    RETURN_IF_EXCEPTION(scope, nullptr);
    return result;
}

JSString* JSBoundFunction::nameSlow(VM& vm)
{
    JSGlobalObject* globalObject = this->globalObject();
    DeferTerminationForAWhile deferScope(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    unsigned nestingCount = 0;
    JSObject* cursor = m_targetFunction.get();
    JSString* terminal = nullptr;
    while (true) {
        ASSERT(cursor->inherits<JSFunction>()); // If this is not JSFunction, we eagerly materialized the name.
        if (!cursor->inherits<JSBoundFunction>()) {
            terminal = jsCast<JSFunction*>(cursor)->originalName(globalObject);
            if (UNLIKELY(scope.exception())) {
                scope.clearException();
                terminal = jsEmptyString(vm);
            }
            break;
        }
        ++nestingCount;
        JSBoundFunction* boundFunction = jsCast<JSBoundFunction*>(cursor);
        terminal = boundFunction->nameMayBeNull();
        if (terminal)
            break;
        cursor = boundFunction->targetFunction();
    }

    if (nestingCount) {
        StringBuilder builder(OverflowPolicy::RecordOverflow);
        for (unsigned i = 0; i < nestingCount; ++i)
            builder.append("bound "_s);
        auto terminalString = terminal->value(globalObject); // Resolving rope.
        if (UNLIKELY(scope.exception())) {
            scope.clearException();
            terminal = jsEmptyString(vm);
        } else {
            builder.append(terminalString.data);
            if (builder.hasOverflowed())
                terminal = jsEmptyString(vm);
            else
                terminal = jsNontrivialString(vm, builder.toString());
        }
    }

    if (terminal) {
        terminal->value(globalObject); // Resolving rope.
        if (UNLIKELY(scope.exception())) {
            scope.clearException();
            terminal = jsEmptyString(vm);
        }
    }

    m_nameMayBeNull.set(vm, this, terminal);
    return terminal;
}

String JSBoundFunction::nameStringWithoutGCSlow(VM& vm)
{
    DisallowGC disallowGC;
    unsigned nestingCount = 0;
    JSObject* cursor = m_targetFunction.get();
    String terminal;
    while (true) {
        ASSERT(cursor->inherits<JSFunction>()); // If this is not JSFunction, we eagerly materialized the name.
        if (!cursor->inherits<JSBoundFunction>()) {
            terminal = jsCast<JSFunction*>(cursor)->nameWithoutGC(vm);
            break;
        }
        ++nestingCount;
        JSBoundFunction* boundFunction = jsCast<JSBoundFunction*>(cursor);
        if (boundFunction->nameMayBeNull()) {
            terminal = boundFunction->nameStringWithoutGC(vm);
            break;
        }
        cursor = boundFunction->targetFunction();
    }

    if (!nestingCount)
        return terminal;

    StringBuilder builder(OverflowPolicy::RecordOverflow);
    for (unsigned i = 0; i < nestingCount; ++i)
        builder.append("bound "_s);
    builder.append(WTFMove(terminal));
    if (builder.hasOverflowed())
        return emptyString();
    return builder.toString();
}

double JSBoundFunction::lengthSlow(VM& vm)
{
    double length = 0;
    unsigned numBoundArgs = boundArgsLength();
    JSObject* cursor = m_targetFunction.get();
    while (true) {
        ASSERT(cursor->inherits<JSFunction>()); // If this is not JSFunction, we eagerly materialized the length already.
        if (!cursor->inherits<JSBoundFunction>()) {
            length = jsCast<JSFunction*>(cursor)->originalLength(vm);
            break;
        }
        JSBoundFunction* boundFunction = jsCast<JSBoundFunction*>(cursor);
        if (!std::isnan(boundFunction->m_length)) {
            length = boundFunction->m_length;
            break;
        }
        numBoundArgs += boundFunction->boundArgsLength();

        cursor = boundFunction->targetFunction();
    }
    if (length > numBoundArgs)
        length -= numBoundArgs;
    else
        length = 0;
    m_length = length;
    return length;
}

bool JSBoundFunction::canConstructSlow()
{
    JSObject* cursor = m_targetFunction.get();
    while (true) {
        if (!cursor->inherits<JSBoundFunction>()) {
            auto constructData = JSC::getConstructData(cursor);
            m_canConstruct = constructData.type == CallData::Type::None ? TriState::False : TriState::True;
            return m_canConstruct == TriState::True;
        }
        JSBoundFunction* boundFunction = jsCast<JSBoundFunction*>(cursor);
        if (boundFunction->m_canConstruct != TriState::Indeterminate) {
            m_canConstruct = boundFunction->m_canConstruct;
            return m_canConstruct == TriState::True;
        }
        cursor = boundFunction->targetFunction();
    }
}

bool JSBoundFunction::canSkipNameAndLengthMaterialization(JSGlobalObject* globalObject, Structure* structure)
{
    if (structure->typeInfo().type() != JSFunctionType)
        return false;
    if (structure->didTransition())
        return false;
    if (structure->storedPrototype() != globalObject->functionPrototype())
        return false;
    if (structure->globalObject() != globalObject)
        return false;

    if (structure->classInfoForCells()->isSubClassOf(JSBoundFunction::info()))
        return true;
    if (structure == globalObject->arrowFunctionStructure(true) || structure == globalObject->arrowFunctionStructure(false))
        return true;
    if (structure == globalObject->strictFunctionStructure(true) || structure == globalObject->strictFunctionStructure(false))
        return true;
    if (structure == globalObject->sloppyFunctionStructure(true) || structure == globalObject->sloppyFunctionStructure(false))
        return true;

    return false;
}

template<typename Visitor>
void JSBoundFunction::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSBoundFunction* thisObject = jsCast<JSBoundFunction*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    visitor.append(thisObject->m_targetFunction);
    visitor.append(thisObject->m_boundThis);
    visitor.appendValues(thisObject->m_boundArgs.data(), thisObject->m_boundArgs.size());
    visitor.append(thisObject->m_nameMayBeNull);
}

DEFINE_VISIT_CHILDREN(JSBoundFunction);

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

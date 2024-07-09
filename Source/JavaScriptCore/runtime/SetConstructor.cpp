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
#include "SetConstructor.h"

#include "GetterSetter.h"
#include "IteratorOperations.h"
#include "JSCInlines.h"
#include "JSSetInlines.h"
#include "SetPrototype.h"

namespace JSC {

const ClassInfo SetConstructor::s_info = { "Function"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(SetConstructor) };

void SetConstructor::finishCreation(VM& vm, SetPrototype* setPrototype)
{
    Base::finishCreation(vm, 0, vm.propertyNames->Set.string(), PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, setPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    JSGlobalObject* globalObject = setPrototype->globalObject();
    GetterSetter* speciesGetterSetter = GetterSetter::create(vm, globalObject, JSFunction::create(vm, globalObject, 0, "get [Symbol.species]"_s, globalFuncSpeciesGetter, ImplementationVisibility::Public, SpeciesGetterIntrinsic), nullptr);
    putDirectNonIndexAccessorWithoutTransition(vm, vm.propertyNames->speciesSymbol, speciesGetterSetter, PropertyAttribute::Accessor | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
}

static JSC_DECLARE_HOST_FUNCTION(callSet);
static JSC_DECLARE_HOST_FUNCTION(constructSet);

SetConstructor::SetConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callSet, constructSet)
{
}

JSC_DEFINE_HOST_FUNCTION(callSet, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "Set"_s));
}

JSC_DEFINE_HOST_FUNCTION(constructSet, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* setStructure = JSC_GET_DERIVED_STRUCTURE(vm, setStructure, newTarget, callFrame->jsCallee());
    RETURN_IF_EXCEPTION(scope, { });

    JSValue iterable = callFrame->argument(0);
    if (iterable.isUndefinedOrNull())
        return JSValue::encode(JSSet::create(vm, setStructure));

    bool canPerformFastAdd = JSSet::isAddFastAndNonObservable(setStructure);
    if (auto* iterableSet = jsDynamicCast<JSSet*>(iterable)) {
        if (canPerformFastAdd && iterableSet->isIteratorProtocolFastAndNonObservable()) 
            RELEASE_AND_RETURN(scope, JSValue::encode(iterableSet->clone(globalObject, vm, setStructure)));
    }

    JSSet* set = JSSet::create(vm, setStructure);

    JSValue adderFunction;
    CallData adderFunctionCallData;
    if (!canPerformFastAdd) {
        adderFunction = set->JSObject::get(globalObject, vm.propertyNames->add);
        RETURN_IF_EXCEPTION(scope, { });

        adderFunctionCallData = JSC::getCallData(adderFunction);
        if (UNLIKELY(adderFunctionCallData.type == CallData::Type::None))
            return throwVMTypeError(globalObject, scope, "'add' property of a Set should be callable."_s);
    }

    scope.release();
    forEachInIterable(globalObject, iterable, [&](VM&, JSGlobalObject* globalObject, JSValue nextValue) {
        if (canPerformFastAdd) {
            set->add(setStructure->globalObject(), nextValue);
            return;
        }

        MarkedArgumentBuffer arguments;
        arguments.append(nextValue);
        ASSERT(!arguments.hasOverflowed());
        call(globalObject, adderFunction, adderFunctionCallData, set, arguments);
    });

    return JSValue::encode(set);
}

JSC_DEFINE_HOST_FUNCTION(setPrivateFuncSetStorage, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ASSERT(jsDynamicCast<JSSet*>(callFrame->argument(0)));
    JSSet* set = jsCast<JSSet*>(callFrame->uncheckedArgument(0));
    return JSValue::encode(set->storageOrSentinel(getVM(globalObject)));
}

JSC_DEFINE_HOST_FUNCTION(setPrivateFuncSetIterationNext, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ASSERT(callFrame->argument(0).isCell() && (callFrame->argument(1).isInt32()));

    VM& vm = globalObject->vm();
    JSCell* cell = callFrame->uncheckedArgument(0).asCell();
    if (cell == vm.orderedHashTableSentinel())
        return JSValue::encode(vm.orderedHashTableSentinel());

    JSSet::Storage& storage = *jsCast<JSSet::Storage*>(cell);
    JSSet::Helper::Entry entry = JSSet::Helper::toNumber(callFrame->uncheckedArgument(1));
    return JSValue::encode(JSSet::Helper::nextAndUpdateIterationEntry(vm, storage, entry));
}

JSC_DEFINE_HOST_FUNCTION(setPrivateFuncSetIterationEntry, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ASSERT(callFrame->argument(0).isCell());

    VM& vm = globalObject->vm();
    JSCell* cell = callFrame->uncheckedArgument(0).asCell();
    ASSERT_UNUSED(vm, cell != vm.orderedHashTableSentinel());

    JSSet::Storage& storage = *jsCast<JSSet::Storage*>(cell);
    return JSValue::encode(JSSet::Helper::getIterationEntry(storage));
}

JSC_DEFINE_HOST_FUNCTION(setPrivateFuncSetIterationEntryKey, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ASSERT(callFrame->argument(0).isCell());

    VM& vm = globalObject->vm();
    JSCell* cell = callFrame->uncheckedArgument(0).asCell();
    ASSERT_UNUSED(vm, cell != vm.orderedHashTableSentinel());

    JSSet::Storage& storage = *jsCast<JSSet::Storage*>(cell);
    return JSValue::encode(JSSet::Helper::getIterationEntryKey(storage));
}

JSC_DEFINE_HOST_FUNCTION(setPrivateFuncClone, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(jsDynamicCast<JSSet*>(callFrame->argument(0)));
    JSSet* set = jsCast<JSSet*>(callFrame->uncheckedArgument(0));
    RELEASE_AND_RETURN(scope, JSValue::encode(set->clone(globalObject, vm, globalObject->setStructure())));
}

}

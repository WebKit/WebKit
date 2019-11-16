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

#include "Error.h"
#include "GetterSetter.h"
#include "IteratorOperations.h"
#include "JSCInlines.h"
#include "JSGlobalObject.h"
#include "JSObjectInlines.h"
#include "JSSet.h"
#include "SetPrototype.h"

namespace JSC {

const ClassInfo SetConstructor::s_info = { "Function", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(SetConstructor) };

void SetConstructor::finishCreation(VM& vm, SetPrototype* setPrototype, GetterSetter* speciesSymbol)
{
    Base::finishCreation(vm, vm.propertyNames->Set.string(), NameAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, setPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(0), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectNonIndexAccessorWithoutTransition(vm, vm.propertyNames->speciesSymbol, speciesSymbol, PropertyAttribute::Accessor | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
}

static EncodedJSValue JSC_HOST_CALL callSet(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL constructSet(JSGlobalObject*, CallFrame*);

SetConstructor::SetConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callSet, constructSet)
{
}

static EncodedJSValue JSC_HOST_CALL callSet(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "Set"));
}

static EncodedJSValue JSC_HOST_CALL constructSet(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Structure* setStructure = InternalFunction::createSubclassStructure(globalObject, callFrame->jsCallee(), callFrame->newTarget(), globalObject->setStructure());
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue iterable = callFrame->argument(0);
    if (iterable.isUndefinedOrNull()) 
        RELEASE_AND_RETURN(scope, JSValue::encode(JSSet::create(globalObject, vm, setStructure)));

    if (auto* iterableSet = jsDynamicCast<JSSet*>(vm, iterable)) {
        if (iterableSet->canCloneFastAndNonObservable(setStructure)) 
            RELEASE_AND_RETURN(scope, JSValue::encode(iterableSet->clone(globalObject, vm, setStructure)));
    }

    JSSet* set = JSSet::create(globalObject, vm, setStructure);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    JSValue adderFunction = set->JSObject::get(globalObject, vm.propertyNames->add);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    CallData adderFunctionCallData;
    CallType adderFunctionCallType = getCallData(vm, adderFunction, adderFunctionCallData);
    if (UNLIKELY(adderFunctionCallType == CallType::None))
        return JSValue::encode(throwTypeError(globalObject, scope));

    scope.release();
    forEachInIterable(globalObject, iterable, [&](VM&, JSGlobalObject* globalObject, JSValue nextValue) {
        MarkedArgumentBuffer arguments;
        arguments.append(nextValue);
        ASSERT(!arguments.hasOverflowed());
        call(globalObject, adderFunction, adderFunctionCallType, adderFunctionCallData, set, arguments);
    });

    return JSValue::encode(set);
}

EncodedJSValue JSC_HOST_CALL setPrivateFuncSetBucketHead(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    ASSERT_UNUSED(globalObject, jsDynamicCast<JSSet*>(globalObject->vm(), callFrame->argument(0)));
    JSSet* set = jsCast<JSSet*>(callFrame->uncheckedArgument(0));
    auto* head = set->head();
    ASSERT(head);
    return JSValue::encode(head);
}

EncodedJSValue JSC_HOST_CALL setPrivateFuncSetBucketNext(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    ASSERT(jsDynamicCast<JSSet::BucketType*>(globalObject->vm(), callFrame->argument(0)));
    auto* bucket = jsCast<JSSet::BucketType*>(callFrame->uncheckedArgument(0));
    ASSERT(bucket);
    bucket = bucket->next();
    while (bucket) {
        if (!bucket->deleted())
            return JSValue::encode(bucket);
        bucket = bucket->next();
    }
    return JSValue::encode(globalObject->vm().sentinelSetBucket());
}

EncodedJSValue JSC_HOST_CALL setPrivateFuncSetBucketKey(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    ASSERT_UNUSED(globalObject, jsDynamicCast<JSSet::BucketType*>(globalObject->vm(), callFrame->argument(0)));
    auto* bucket = jsCast<JSSet::BucketType*>(callFrame->uncheckedArgument(0));
    ASSERT(bucket);
    return JSValue::encode(bucket->key());
}

}

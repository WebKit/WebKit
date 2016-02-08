/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "SetPrototype.h"

#include "CachedCall.h"
#include "Error.h"
#include "ExceptionHelpers.h"
#include "GetterSetter.h"
#include "IteratorOperations.h"
#include "JSCJSValueInlines.h"
#include "JSFunctionInlines.h"
#include "JSSet.h"
#include "JSSetIterator.h"
#include "Lookup.h"
#include "MapDataInlines.h"
#include "StructureInlines.h"

#include "SetPrototype.lut.h"

namespace JSC {

const ClassInfo SetPrototype::s_info = { "Set", &Base::s_info, &setPrototypeTable, CREATE_METHOD_TABLE(SetPrototype) };

/* Source for SetIteratorPrototype.lut.h
@begin setPrototypeTable
  forEach   JSBuiltin  DontEnum|Function 0
@end
*/

static EncodedJSValue JSC_HOST_CALL setProtoFuncAdd(ExecState*);
static EncodedJSValue JSC_HOST_CALL setProtoFuncClear(ExecState*);
static EncodedJSValue JSC_HOST_CALL setProtoFuncDelete(ExecState*);
static EncodedJSValue JSC_HOST_CALL setProtoFuncHas(ExecState*);
static EncodedJSValue JSC_HOST_CALL setProtoFuncValues(ExecState*);
static EncodedJSValue JSC_HOST_CALL setProtoFuncEntries(ExecState*);


static EncodedJSValue JSC_HOST_CALL setProtoFuncSize(ExecState*);

void SetPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    vm.prototypeMap.addPrototype(this);

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->add, setProtoFuncAdd, DontEnum, 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->clear, setProtoFuncClear, DontEnum, 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->deleteKeyword, setProtoFuncDelete, DontEnum, 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->has, setProtoFuncHas, DontEnum, 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->entries, setProtoFuncEntries, DontEnum, 0);

    JSFunction* values = JSFunction::create(vm, globalObject, 0, vm.propertyNames->values.string(), setProtoFuncValues);
    putDirectWithoutTransition(vm, vm.propertyNames->values, values, DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->keys, values, DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->iteratorSymbol, values, DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->toStringTagSymbol, jsString(&vm, "Set"), DontEnum | ReadOnly);

    GetterSetter* accessor = GetterSetter::create(vm, globalObject);
    JSFunction* function = JSFunction::create(vm, globalObject, 0, vm.propertyNames->size.string(), setProtoFuncSize);
    accessor->setGetter(vm, globalObject, function);
    putDirectNonIndexAccessor(vm, vm.propertyNames->size, accessor, DontEnum | Accessor);
}

bool SetPrototype::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    return getStaticFunctionSlot<Base>(exec, setPrototypeTable, jsCast<SetPrototype*>(object), propertyName, slot);
}


ALWAYS_INLINE static JSSet* getSet(CallFrame* callFrame, JSValue thisValue)
{
    if (!thisValue.isObject()) {
        throwVMError(callFrame, createNotAnObjectError(callFrame, thisValue));
        return nullptr;
    }
    JSSet* set = jsDynamicCast<JSSet*>(thisValue);
    if (!set) {
        throwTypeError(callFrame, ASCIILiteral("Set operation called on non-Set object"));
        return nullptr;
    }
    return set;
}

EncodedJSValue JSC_HOST_CALL setProtoFuncAdd(CallFrame* callFrame)
{
    JSValue thisValue = callFrame->thisValue();
    JSSet* set = getSet(callFrame, thisValue);
    if (!set)
        return JSValue::encode(jsUndefined());
    set->add(callFrame, callFrame->argument(0));
    return JSValue::encode(thisValue);
}

EncodedJSValue JSC_HOST_CALL setProtoFuncClear(CallFrame* callFrame)
{
    JSSet* set = getSet(callFrame, callFrame->thisValue());
    if (!set)
        return JSValue::encode(jsUndefined());
    set->clear(callFrame);
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL setProtoFuncDelete(CallFrame* callFrame)
{
    JSSet* set = getSet(callFrame, callFrame->thisValue());
    if (!set)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsBoolean(set->remove(callFrame, callFrame->argument(0))));
}

EncodedJSValue JSC_HOST_CALL setProtoFuncHas(CallFrame* callFrame)
{
    JSSet* set = getSet(callFrame, callFrame->thisValue());
    if (!set)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsBoolean(set->has(callFrame, callFrame->argument(0))));
}

EncodedJSValue JSC_HOST_CALL setProtoFuncSize(CallFrame* callFrame)
{
    JSSet* set = getSet(callFrame, callFrame->thisValue());
    if (!set)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsNumber(set->size(callFrame)));
}
    
EncodedJSValue JSC_HOST_CALL setProtoFuncValues(CallFrame* callFrame)
{
    JSSet* thisObj = jsDynamicCast<JSSet*>(callFrame->thisValue());
    if (!thisObj)
        return JSValue::encode(throwTypeError(callFrame, ASCIILiteral("Cannot create a Set value iterator for a non-Set object.")));
    return JSValue::encode(JSSetIterator::create(callFrame->vm(), callFrame->callee()->globalObject()->setIteratorStructure(), thisObj, SetIterateValue));
}

EncodedJSValue JSC_HOST_CALL setProtoFuncEntries(CallFrame* callFrame)
{
    JSSet* thisObj = jsDynamicCast<JSSet*>(callFrame->thisValue());
    if (!thisObj)
        return JSValue::encode(throwTypeError(callFrame, ASCIILiteral("Cannot create a Set entry iterator for a non-Set object.")));
    return JSValue::encode(JSSetIterator::create(callFrame->vm(), callFrame->callee()->globalObject()->setIteratorStructure(), thisObj, SetIterateKeyValue));
}

EncodedJSValue JSC_HOST_CALL privateFuncIsSet(ExecState* exec)
{
    return JSValue::encode(jsBoolean(jsDynamicCast<JSSet*>(exec->uncheckedArgument(0))));
}

EncodedJSValue JSC_HOST_CALL privateFuncSetIterator(ExecState* exec)
{
    ASSERT(jsDynamicCast<JSSet*>(exec->uncheckedArgument(0)));
    JSSet* set = jsCast<JSSet*>(exec->uncheckedArgument(0));
    return JSValue::encode(JSSetIterator::create(exec->vm(), exec->callee()->globalObject()->setIteratorStructure(), set, SetIterateKey));
}

EncodedJSValue JSC_HOST_CALL privateFuncSetIteratorNext(ExecState* exec)
{
    ASSERT(jsDynamicCast<JSSetIterator*>(exec->thisValue()));
    JSSetIterator* iterator = jsCast<JSSetIterator*>(exec->thisValue());
    JSValue result;
    if (iterator->next(exec, result)) {
        JSArray* resultArray = jsCast<JSArray*>(exec->uncheckedArgument(0));
        resultArray->putDirectIndex(exec, 0, result);
        return JSValue::encode(jsBoolean(false));
    }
    iterator->finish();
    return JSValue::encode(jsBoolean(true));
}

}

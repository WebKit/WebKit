/*
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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

#include "BuiltinNames.h"
#include "Error.h"
#include "ExceptionHelpers.h"
#include "IteratorOperations.h"
#include "JSCInlines.h"
#include "JSSet.h"
#include "Lookup.h"

#include "SetPrototype.lut.h"

namespace JSC {

const ClassInfo SetPrototype::s_info = { "Set", &Base::s_info, &setPrototypeTable, nullptr, CREATE_METHOD_TABLE(SetPrototype) };

/* Source for SetIteratorPrototype.lut.h
@begin setPrototypeTable
  forEach   JSBuiltin  DontEnum|Function 0
  entries   JSBuiltin  DontEnum|Function 0
@end
*/

static EncodedJSValue JSC_HOST_CALL setProtoFuncAdd(ExecState*);
static EncodedJSValue JSC_HOST_CALL setProtoFuncClear(ExecState*);
static EncodedJSValue JSC_HOST_CALL setProtoFuncDelete(ExecState*);
static EncodedJSValue JSC_HOST_CALL setProtoFuncHas(ExecState*);


static EncodedJSValue JSC_HOST_CALL setProtoFuncSize(ExecState*);

void SetPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    didBecomePrototype();

    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->add, setProtoFuncAdd, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, JSSetAddIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->clear, setProtoFuncClear, static_cast<unsigned>(PropertyAttribute::DontEnum), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->deleteKeyword, setProtoFuncDelete, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->has, setProtoFuncHas, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, JSSetHasIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().hasPrivateName(), setProtoFuncHas, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, JSSetHasIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().addPrivateName(), setProtoFuncAdd, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, JSSetAddIntrinsic);

    JSFunction* values = JSFunction::create(vm, setPrototypeValuesCodeGenerator(vm), globalObject);
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().valuesPublicName(), values, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().keysPublicName(), values, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->iteratorSymbol, values, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->toStringTagSymbol, jsString(&vm, "Set"), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);

    JSC_NATIVE_GETTER_WITHOUT_TRANSITION(vm.propertyNames->size, setProtoFuncSize, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
}

ALWAYS_INLINE static JSSet* getSet(CallFrame* callFrame, JSValue thisValue)
{
    VM& vm = callFrame->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(!thisValue.isCell())) {
        throwVMError(callFrame, scope, createNotAnObjectError(callFrame, thisValue));
        return nullptr;
    }
    auto* set = jsDynamicCast<JSSet*>(vm, thisValue.asCell());
    if (LIKELY(set))
        return set;
    throwTypeError(callFrame, scope, "Set operation called on non-Set object"_s);
    return nullptr;
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
    return JSValue::encode(jsNumber(set->size()));
}

}

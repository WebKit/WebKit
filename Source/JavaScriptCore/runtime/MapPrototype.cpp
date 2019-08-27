/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#include "MapPrototype.h"

#include "BuiltinNames.h"
#include "Error.h"
#include "ExceptionHelpers.h"
#include "IteratorOperations.h"
#include "JSCInlines.h"
#include "JSMap.h"
#include "Lookup.h"

#include "MapPrototype.lut.h"

namespace JSC {

const ClassInfo MapPrototype::s_info = { "Map", &Base::s_info, &mapPrototypeTable, nullptr, CREATE_METHOD_TABLE(MapPrototype) };

/* Source for MapPrototype.lut.h
@begin mapPrototypeTable
  forEach   JSBuiltin  DontEnum|Function 0
  values    JSBuiltin  DontEnum|Function 0
  keys      JSBuiltin  DontEnum|Function 0
@end
*/

static EncodedJSValue JSC_HOST_CALL mapProtoFuncClear(ExecState*);
static EncodedJSValue JSC_HOST_CALL mapProtoFuncDelete(ExecState*);
static EncodedJSValue JSC_HOST_CALL mapProtoFuncGet(ExecState*);
static EncodedJSValue JSC_HOST_CALL mapProtoFuncHas(ExecState*);
static EncodedJSValue JSC_HOST_CALL mapProtoFuncSet(ExecState*);

static EncodedJSValue JSC_HOST_CALL mapProtoFuncSize(ExecState*);
    
void MapPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->clear, mapProtoFuncClear, static_cast<unsigned>(PropertyAttribute::DontEnum), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->deleteKeyword, mapProtoFuncDelete, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->get, mapProtoFuncGet, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, JSMapGetIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->has, mapProtoFuncHas, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, JSMapHasIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->set, mapProtoFuncSet, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, JSMapSetIntrinsic);

    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().getPrivateName(), mapProtoFuncGet, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, JSMapGetIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().setPrivateName(), mapProtoFuncSet, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, JSMapSetIntrinsic);

    JSFunction* entries = JSFunction::create(vm, mapPrototypeEntriesCodeGenerator(vm), globalObject);
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().entriesPublicName(), entries, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->iteratorSymbol, entries, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->toStringTagSymbol, jsString(vm, "Map"), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);

    JSC_NATIVE_GETTER_WITHOUT_TRANSITION(vm.propertyNames->size, mapProtoFuncSize, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
}

ALWAYS_INLINE static JSMap* getMap(CallFrame* callFrame, JSValue thisValue)
{
    VM& vm = callFrame->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(!thisValue.isCell())) {
        throwVMError(callFrame, scope, createNotAnObjectError(callFrame, thisValue));
        return nullptr;
    }

    auto* map = jsDynamicCast<JSMap*>(vm, thisValue.asCell());
    if (LIKELY(map))
        return map;
    throwTypeError(callFrame, scope, "Map operation called on non-Map object"_s);
    return nullptr;
}

EncodedJSValue JSC_HOST_CALL mapProtoFuncClear(CallFrame* callFrame)
{
    JSMap* map = getMap(callFrame, callFrame->thisValue());
    if (!map)
        return JSValue::encode(jsUndefined());
    map->clear(callFrame);
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL mapProtoFuncDelete(CallFrame* callFrame)
{
    JSMap* map = getMap(callFrame, callFrame->thisValue());
    if (!map)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsBoolean(map->remove(callFrame, callFrame->argument(0))));
}

EncodedJSValue JSC_HOST_CALL mapProtoFuncGet(CallFrame* callFrame)
{
    JSMap* map = getMap(callFrame, callFrame->thisValue());
    if (!map)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(map->get(callFrame, callFrame->argument(0)));
}

EncodedJSValue JSC_HOST_CALL mapProtoFuncHas(CallFrame* callFrame)
{
    JSMap* map = getMap(callFrame, callFrame->thisValue());
    if (!map)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsBoolean(map->has(callFrame, callFrame->argument(0))));
}

EncodedJSValue JSC_HOST_CALL mapProtoFuncSet(CallFrame* callFrame)
{
    JSValue thisValue = callFrame->thisValue();
    JSMap* map = getMap(callFrame, thisValue);
    if (!map)
        return JSValue::encode(jsUndefined());
    map->set(callFrame, callFrame->argument(0), callFrame->argument(1));
    return JSValue::encode(thisValue);
}

EncodedJSValue JSC_HOST_CALL mapProtoFuncSize(CallFrame* callFrame)
{
    JSMap* map = getMap(callFrame, callFrame->thisValue());
    if (!map)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsNumber(map->size()));
}

}

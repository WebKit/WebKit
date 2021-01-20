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
#include "JSCInlines.h"
#include "JSMap.h"
#include "JSMapIterator.h"

#include "MapPrototype.lut.h"

namespace JSC {

const ClassInfo MapPrototype::s_info = { "Map", &Base::s_info, &mapPrototypeTable, nullptr, CREATE_METHOD_TABLE(MapPrototype) };

/* Source for MapPrototype.lut.h
@begin mapPrototypeTable
  forEach   JSBuiltin  DontEnum|Function 0
@end
*/

static JSC_DECLARE_HOST_FUNCTION(mapProtoFuncClear);
static JSC_DECLARE_HOST_FUNCTION(mapProtoFuncDelete);
static JSC_DECLARE_HOST_FUNCTION(mapProtoFuncGet);
static JSC_DECLARE_HOST_FUNCTION(mapProtoFuncHas);
static JSC_DECLARE_HOST_FUNCTION(mapProtoFuncSet);
static JSC_DECLARE_HOST_FUNCTION(mapProtoFuncValues);
static JSC_DECLARE_HOST_FUNCTION(mapProtoFuncKeys);
static JSC_DECLARE_HOST_FUNCTION(mapProtoFuncEntries);

static JSC_DECLARE_HOST_FUNCTION(mapProtoFuncSize);
    
void MapPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->clear, mapProtoFuncClear, static_cast<unsigned>(PropertyAttribute::DontEnum), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->deleteKeyword, mapProtoFuncDelete, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->get, mapProtoFuncGet, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, JSMapGetIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->has, mapProtoFuncHas, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, JSMapHasIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->set, mapProtoFuncSet, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, JSMapSetIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().keysPublicName(), mapProtoFuncKeys, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, JSMapKeysIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().valuesPublicName(), mapProtoFuncValues, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, JSMapValuesIntrinsic);

    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().getPrivateName(), mapProtoFuncGet, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, JSMapGetIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().setPrivateName(), mapProtoFuncSet, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, JSMapSetIntrinsic);

    JSFunction* entries = JSFunction::create(vm, globalObject, 0, vm.propertyNames->builtinNames().entriesPublicName().string(), mapProtoFuncEntries, JSMapEntriesIntrinsic);
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().entriesPublicName(), entries, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->iteratorSymbol, entries, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();

    JSC_NATIVE_GETTER_WITHOUT_TRANSITION(vm.propertyNames->size, mapProtoFuncSize, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);

    globalObject->installMapPrototypeWatchpoint(this);
}

ALWAYS_INLINE static JSMap* getMap(JSGlobalObject* globalObject, JSValue thisValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(!thisValue.isCell())) {
        throwVMError(globalObject, scope, createNotAnObjectError(globalObject, thisValue));
        return nullptr;
    }

    auto* map = jsDynamicCast<JSMap*>(vm, thisValue.asCell());
    if (LIKELY(map))
        return map;
    throwTypeError(globalObject, scope, "Map operation called on non-Map object"_s);
    return nullptr;
}

JSC_DEFINE_HOST_FUNCTION(mapProtoFuncClear, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSMap* map = getMap(globalObject, callFrame->thisValue());
    if (!map)
        return JSValue::encode(jsUndefined());
    map->clear(globalObject);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(mapProtoFuncDelete, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSMap* map = getMap(globalObject, callFrame->thisValue());
    if (!map)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsBoolean(map->remove(globalObject, callFrame->argument(0))));
}

JSC_DEFINE_HOST_FUNCTION(mapProtoFuncGet, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSMap* map = getMap(globalObject, callFrame->thisValue());
    if (!map)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(map->get(globalObject, callFrame->argument(0)));
}

JSC_DEFINE_HOST_FUNCTION(mapProtoFuncHas, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSMap* map = getMap(globalObject, callFrame->thisValue());
    if (!map)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsBoolean(map->has(globalObject, callFrame->argument(0))));
}

JSC_DEFINE_HOST_FUNCTION(mapProtoFuncSet, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue thisValue = callFrame->thisValue();
    JSMap* map = getMap(globalObject, thisValue);
    if (!map)
        return JSValue::encode(jsUndefined());
    map->set(globalObject, callFrame->argument(0), callFrame->argument(1));
    return JSValue::encode(thisValue);
}

inline JSValue createMapIteratorObject(JSGlobalObject* globalObject, CallFrame* callFrame, IterationKind kind)
{
    VM& vm = globalObject->vm();
    JSValue thisValue = callFrame->thisValue();
    JSMap* map = getMap(globalObject, thisValue);
    if (!map)
        return jsUndefined();
    return JSMapIterator::create(vm, globalObject->mapIteratorStructure(), map, kind);
}

JSC_DEFINE_HOST_FUNCTION(mapProtoFuncValues, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(createMapIteratorObject(globalObject, callFrame, IterationKind::Values));
}

JSC_DEFINE_HOST_FUNCTION(mapProtoFuncKeys, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(createMapIteratorObject(globalObject, callFrame, IterationKind::Keys));
}

JSC_DEFINE_HOST_FUNCTION(mapProtoFuncEntries, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(createMapIteratorObject(globalObject, callFrame, IterationKind::Entries));
}

JSC_DEFINE_HOST_FUNCTION(mapProtoFuncSize, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSMap* map = getMap(globalObject, callFrame->thisValue());
    if (!map)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsNumber(map->size()));
}

}

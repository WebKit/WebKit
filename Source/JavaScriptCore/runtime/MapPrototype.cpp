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
#include "MapPrototype.h"

#include "BuiltinNames.h"
#include "GetterSetter.h"
#include "JSCInlines.h"
#include "JSMapInlines.h"
#include "JSMapIterator.h"

namespace JSC {

const ClassInfo MapPrototype::s_info = { "Map"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(MapPrototype) };

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
    ASSERT(inherits(info()));

    JSFunction* clearFunc = JSFunction::create(vm, globalObject, 0, vm.propertyNames->clear.string(), mapProtoFuncClear, ImplementationVisibility::Public);
    putDirectWithoutTransition(vm, vm.propertyNames->clear, clearFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().clearPrivateName(), clearFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSFunction* deleteFunc = JSFunction::create(vm, globalObject, 1, vm.propertyNames->deleteKeyword.string(), mapProtoFuncDelete, ImplementationVisibility::Public, JSMapDeleteIntrinsic);
    putDirectWithoutTransition(vm, vm.propertyNames->deleteKeyword, deleteFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().deletePrivateName(), deleteFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSFunction* entries = JSFunction::create(vm, globalObject, 0, vm.propertyNames->builtinNames().entriesPublicName().string(), mapProtoFuncEntries, ImplementationVisibility::Public, JSMapEntriesIntrinsic);
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().entriesPublicName(), entries, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().entriesPrivateName(), entries, static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSFunction* forEachFunc = JSFunction::create(vm, mapPrototypeForEachCodeGenerator(vm), globalObject);
    putDirectWithoutTransition(vm, vm.propertyNames->forEach, forEachFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().forEachPrivateName(), forEachFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSFunction* getFunc = JSFunction::create(vm, globalObject, 1, vm.propertyNames->get.string(), mapProtoFuncGet, ImplementationVisibility::Public, JSMapGetIntrinsic);
    putDirectWithoutTransition(vm, vm.propertyNames->get, getFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().getPrivateName(), getFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSFunction* hasFunc = JSFunction::create(vm, globalObject, 1, vm.propertyNames->has.string(), mapProtoFuncHas, ImplementationVisibility::Public, JSMapHasIntrinsic);
    putDirectWithoutTransition(vm, vm.propertyNames->has, hasFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().hasPrivateName(), hasFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSFunction* keysFunc = JSFunction::create(vm, globalObject, 0, vm.propertyNames->builtinNames().keysPublicName().string(), mapProtoFuncKeys, ImplementationVisibility::Public, JSMapKeysIntrinsic);
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().keysPublicName(), keysFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().keysPrivateName(), keysFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSFunction* setFunc = JSFunction::create(vm, globalObject, 2, vm.propertyNames->set.string(), mapProtoFuncSet, ImplementationVisibility::Public, JSMapSetIntrinsic);
    putDirectWithoutTransition(vm, vm.propertyNames->set, setFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().setPrivateName(), setFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSFunction* sizeGetter = JSFunction::create(vm, globalObject, 0, "get size"_s, mapProtoFuncSize, ImplementationVisibility::Public);
    GetterSetter* sizeAccessor = GetterSetter::create(vm, globalObject, sizeGetter, nullptr);
    putDirectNonIndexAccessorWithoutTransition(vm, vm.propertyNames->size, sizeAccessor, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    putDirectNonIndexAccessorWithoutTransition(vm, vm.propertyNames->builtinNames().sizePrivateName(), sizeAccessor, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);

    JSFunction* valuesFunc = JSFunction::create(vm, globalObject, 0, vm.propertyNames->builtinNames().valuesPublicName().string(), mapProtoFuncValues, ImplementationVisibility::Public, JSMapValuesIntrinsic);
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().valuesPublicName(), valuesFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().valuesPrivateName(), valuesFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));

    putDirectWithoutTransition(vm, vm.propertyNames->iteratorSymbol, entries, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();

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

    auto* map = jsDynamicCast<JSMap*>(thisValue.asCell());
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
    map->clear(globalObject->vm());
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

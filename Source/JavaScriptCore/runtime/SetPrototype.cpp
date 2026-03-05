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
#include "SetPrototype.h"

#include "CachedCallInlines.h"
#include "InterpreterInlines.h"
#include "BuiltinNames.h"
#include "GetterSetter.h"
#include "IteratorOperations.h"
#include "JSCInlines.h"
#include "JSSet.h"
#include "JSSetIterator.h"
#include "SetPrototypeInlines.h"
#include "VMEntryScopeInlines.h"

namespace JSC {

const ClassInfo SetPrototype::s_info = { "Set"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(SetPrototype) };

static JSC_DECLARE_HOST_FUNCTION(setProtoFuncAdd);
static JSC_DECLARE_HOST_FUNCTION(setProtoFuncClear);
static JSC_DECLARE_HOST_FUNCTION(setProtoFuncDelete);
static JSC_DECLARE_HOST_FUNCTION(setProtoFuncHas);
static JSC_DECLARE_HOST_FUNCTION(setProtoFuncValues);
static JSC_DECLARE_HOST_FUNCTION(setProtoFuncEntries);
static JSC_DECLARE_HOST_FUNCTION(setProtoFuncIntersection);
static JSC_DECLARE_HOST_FUNCTION(setProtoFuncUnion);
static JSC_DECLARE_HOST_FUNCTION(setProtoFuncDifference);
static JSC_DECLARE_HOST_FUNCTION(setProtoFuncSymmetricDifference);
static JSC_DECLARE_HOST_FUNCTION(setProtoFuncIsSubsetOf);
static JSC_DECLARE_HOST_FUNCTION(setProtoFuncIsSupersetOf);
static JSC_DECLARE_HOST_FUNCTION(setProtoFuncIsDisjointFrom);

static JSC_DECLARE_HOST_FUNCTION(setProtoFuncSize);

void SetPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    JSFunction* addFunc = JSFunction::create(vm, globalObject, 1, vm.propertyNames->add.string(), setProtoFuncAdd, ImplementationVisibility::Public, JSSetAddIntrinsic);
    putDirectWithoutTransition(vm, vm.propertyNames->add, addFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().addPrivateName(), addFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSFunction* clearFunc = JSFunction::create(vm, globalObject, 0, vm.propertyNames->clear.string(), setProtoFuncClear, ImplementationVisibility::Public);
    putDirectWithoutTransition(vm, vm.propertyNames->clear, clearFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().clearPrivateName(), clearFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSFunction* deleteFunc = JSFunction::create(vm, globalObject, 1, vm.propertyNames->deleteKeyword.string(), setProtoFuncDelete, ImplementationVisibility::Public, JSSetDeleteIntrinsic);
    putDirectWithoutTransition(vm, vm.propertyNames->deleteKeyword, deleteFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().deletePrivateName(), deleteFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSFunction* entriesFunc = JSFunction::create(vm, globalObject, 0, vm.propertyNames->builtinNames().entriesPublicName().string(), setProtoFuncEntries, ImplementationVisibility::Public, JSSetEntriesIntrinsic);
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().entriesPublicName(), entriesFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().entriesPrivateName(), entriesFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSFunction* forEachFunc = JSFunction::create(vm, globalObject, setPrototypeForEachCodeGenerator(vm), globalObject);
    putDirectWithoutTransition(vm, vm.propertyNames->forEach, forEachFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().forEachPrivateName(), forEachFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSFunction* hasFunc = JSFunction::create(vm, globalObject, 1, vm.propertyNames->has.string(), setProtoFuncHas, ImplementationVisibility::Public, JSSetHasIntrinsic);
    putDirectWithoutTransition(vm, vm.propertyNames->has, hasFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().hasPrivateName(), hasFunc, static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSFunction* values = JSFunction::create(vm, globalObject, 0, vm.propertyNames->builtinNames().valuesPublicName().string(), setProtoFuncValues, ImplementationVisibility::Public, JSSetValuesIntrinsic);
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().keysPublicName(), values, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().keysPrivateName(), values, static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSFunction* sizeGetter = JSFunction::create(vm, globalObject, 0, "get size"_s, setProtoFuncSize, ImplementationVisibility::Public, JSSetSizeIntrinsic);
    GetterSetter* sizeAccessor = GetterSetter::create(vm, globalObject, sizeGetter, nullptr);
    putDirectNonIndexAccessorWithoutTransition(vm, vm.propertyNames->size, sizeAccessor, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    putDirectNonIndexAccessorWithoutTransition(vm, vm.propertyNames->builtinNames().sizePrivateName(), sizeAccessor, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);

    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().valuesPublicName(), values, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().valuesPrivateName(), values, static_cast<unsigned>(PropertyAttribute::DontEnum));

    putDirectWithoutTransition(vm, vm.propertyNames->iteratorSymbol, values, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("union"_s, setProtoFuncUnion, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("intersection"_s, setProtoFuncIntersection, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("difference"_s, setProtoFuncDifference, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("symmetricDifference"_s, setProtoFuncSymmetricDifference, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("isSubsetOf"_s, setProtoFuncIsSubsetOf, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("isSupersetOf"_s, setProtoFuncIsSupersetOf, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("isDisjointFrom"_s, setProtoFuncIsDisjointFrom, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);

    globalObject->installSetPrototypeWatchpoint(this);
}

ALWAYS_INLINE static JSSet* getSet(JSGlobalObject* globalObject, JSValue thisValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!thisValue.isCell()) [[unlikely]] {
        throwVMError(globalObject, scope, createNotAnObjectError(globalObject, thisValue));
        return nullptr;
    }
    if (auto* set = jsDynamicCast<JSSet*>(thisValue.asCell())) [[likely]]
        return set;
    throwTypeError(globalObject, scope, "Set operation called on non-Set object"_s);
    return nullptr;
}

JSC_DEFINE_HOST_FUNCTION(setProtoFuncAdd, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSSet* set = getSet(globalObject, thisValue);
    RETURN_IF_EXCEPTION(scope, JSValue::encode(jsUndefined()));

    set->add(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, JSValue::encode(jsUndefined()));
    return JSValue::encode(thisValue);
}

JSC_DEFINE_HOST_FUNCTION(setProtoFuncClear, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSSet* set = getSet(globalObject, callFrame->thisValue());
    RETURN_IF_EXCEPTION(scope, JSValue::encode(jsUndefined()));

    scope.release();
    set->clear(globalObject);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(setProtoFuncDelete, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSSet* set = getSet(globalObject, callFrame->thisValue());
    RETURN_IF_EXCEPTION(scope, JSValue::encode(jsUndefined()));

    RELEASE_AND_RETURN(scope, JSValue::encode(jsBoolean(set->remove(globalObject, callFrame->argument(0)))));
}

JSC_DEFINE_HOST_FUNCTION(setProtoFuncHas, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSSet* set = getSet(globalObject, callFrame->thisValue());
    RETURN_IF_EXCEPTION(scope, JSValue::encode(jsUndefined()));

    RELEASE_AND_RETURN(scope, JSValue::encode(jsBoolean(set->has(globalObject, callFrame->argument(0)))));
}

JSC_DEFINE_HOST_FUNCTION(setProtoFuncSize, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSSet* set = getSet(globalObject, callFrame->thisValue());
    RETURN_IF_EXCEPTION(scope, JSValue::encode(jsUndefined()));

    return JSValue::encode(jsNumber(set->size()));
}

// https://tc39.es/ecma262/#sec-getsetrecord ( Step 1 ~ Step 7 )
static uint32_t getSetSizeAsInt(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!value.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Set operation expects first argument to be an object");

    JSObject* obj = asObject(value);

    JSValue rawSize = obj->get(globalObject, vm.propertyNames->size);
    RETURN_IF_EXCEPTION(scope, 0);

    double numSize = rawSize.toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, 0);

    if (std::isnan(numSize)) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Set operation expects first argument to have non-NaN 'size' property"_s);

    double intOrInfSize = jsNumber(numSize).toIntegerOrInfinity(globalObject);

    if (intOrInfSize < 0) [[unlikely]]
        return throwVMRangeError(globalObject, scope, "Set operation expects first argument to have non-negative 'size' property"_s);

    if (std::isinf(intOrInfSize)) [[unlikely]]
        return std::numeric_limits<uint32_t>::max();
    return static_cast<uint32_t>(intOrInfSize);
}

static EncodedJSValue fastSetIntersection(JSGlobalObject* globalObject, JSSet* thisSet, JSSet* otherSet)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSSet* result = JSSet::create(vm, globalObject->setStructure());

    JSSet* sourceSet = thisSet->size() <= otherSet->size() ? thisSet : otherSet;
    JSSet* targetSet = thisSet->size() <= otherSet->size() ? otherSet : thisSet;

    JSCell* sourceStorageCell = sourceSet->storageOrSentinel(vm);
    if (sourceStorageCell == vm.orderedHashTableSentinel())
        return JSValue::encode(result);

    auto* sourceStorage = jsCast<JSSet::Storage*>(sourceStorageCell);
    JSSet::Helper::Entry entry = 0;

    while (true) {
        sourceStorageCell = JSSet::Helper::nextAndUpdateIterationEntry(vm, *sourceStorage, entry);
        if (sourceStorageCell == vm.orderedHashTableSentinel())
            break;

        auto* currentStorage = jsCast<JSSet::Storage*>(sourceStorageCell);
        entry = JSSet::Helper::iterationEntry(*currentStorage) + 1;
        JSValue entryKey = JSSet::Helper::getIterationEntryKey(*currentStorage);

        bool targetHasEntry = targetSet->has(globalObject, entryKey);
        RETURN_IF_EXCEPTION(scope, { });
        if (targetHasEntry) {
            result->add(globalObject, entryKey);
            RETURN_IF_EXCEPTION(scope, { });
        }

        sourceStorage = currentStorage;
    }
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(setProtoFuncIntersection, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSSet* thisSet = getSet(globalObject, callFrame->thisValue());
    RETURN_IF_EXCEPTION(scope, { });

    JSValue otherValue = callFrame->argument(0);

    if (otherValue.isCell()) [[likely]] {
        if (auto* otherSet = jsDynamicCast<JSSet*>(otherValue.asCell())) [[likely]] {
            if (setPrimordialWatchpointIsValid(vm, otherSet)) [[likely]] {
                scope.release();
                return fastSetIntersection(globalObject, thisSet, otherSet);
            }
        }
    }

    uint32_t size = getSetSizeAsInt(globalObject, otherValue);
    RETURN_IF_EXCEPTION(scope, { });

    ASSERT(otherValue.isObject());
    JSObject* otherObject = asObject(otherValue);

    JSValue has = otherObject->get(globalObject, vm.propertyNames->has);
    RETURN_IF_EXCEPTION(scope, { });
    if (!has.isCallable()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Set.prototype.intersection expects other.has to be callable");

    JSValue keys = otherObject->get(globalObject, vm.propertyNames->keys);
    RETURN_IF_EXCEPTION(scope, { });
    if (!keys.isCallable()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Set.prototype.intersection expects other.keys to be callable");

    JSSet* result = JSSet::create(vm, globalObject->setStructure());
    if (thisSet->size() <= size) {
        JSCell* storageCell = thisSet->storageOrSentinel(vm);
        if (storageCell == vm.orderedHashTableSentinel())
            return JSValue::encode(result);

        auto* storage = jsCast<JSSet::Storage*>(storageCell);
        JSSet::Helper::Entry entry = 0;
        CallData hasCallData = JSC::getCallDataInline(has);

        std::optional<CachedCall> cachedHasCall;
        if (hasCallData.type == CallData::Type::JS) [[likely]] {
            cachedHasCall.emplace(globalObject, jsCast<JSFunction*>(has), 1);
            RETURN_IF_EXCEPTION(scope, { });
        }

        while (true) {
            storageCell = JSSet::Helper::nextAndUpdateIterationEntry(vm, *storage, entry);
            if (storageCell == vm.orderedHashTableSentinel())
                break;

            storage = jsCast<JSSet::Storage*>(storageCell);
            entry = JSSet::Helper::iterationEntry(*storage) + 1;
            JSValue entryKey = JSSet::Helper::getIterationEntryKey(*storage);

            JSValue hasResult;
            if (cachedHasCall) [[likely]] {
                hasResult = cachedHasCall->callWithArguments(globalObject, otherValue, entryKey);
                RETURN_IF_EXCEPTION(scope, { });
            } else {
                MarkedArgumentBuffer args;
                args.append(entryKey);
                ASSERT(!args.hasOverflowed());
                hasResult = call(globalObject, has, hasCallData, otherValue, args);
                RETURN_IF_EXCEPTION(scope, { });
            }

            bool hasResultBool = hasResult.toBoolean(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            if (hasResultBool) {
                result->add(globalObject, entryKey);
                RETURN_IF_EXCEPTION(scope, { });
            }
        }
    } else {
        CallData keysCallData = JSC::getCallDataInline(keys);
        MarkedArgumentBuffer args;
        ASSERT(!args.hasOverflowed());
        JSValue iterator = call(globalObject, keys, keysCallData, otherValue, args);
        RETURN_IF_EXCEPTION(scope, { });
        scope.release();
        forEachInIteratorProtocol(globalObject, iterator, [&](VM&, JSGlobalObject* globalObject, JSValue key) -> void {
            bool thisSetHasKey = thisSet->has(globalObject, key);
            RETURN_IF_EXCEPTION(scope, void());
            if (thisSetHasKey) {
                result->add(globalObject, key);
                RETURN_IF_EXCEPTION(scope, void());
            }
        });
    }

    return JSValue::encode(result);
}

static EncodedJSValue fastSetUnion(JSGlobalObject* globalObject, JSSet* thisSet, JSSet* otherSet)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSSet* result = thisSet->clone(globalObject, vm, globalObject->setStructure());
    RETURN_IF_EXCEPTION(scope, { });

    JSCell* otherStorageCell = otherSet->storageOrSentinel(vm);
    if (otherStorageCell != vm.orderedHashTableSentinel()) {
        auto* otherStorage = jsCast<JSSet::Storage*>(otherStorageCell);
        JSSet::Helper::Entry entry = 0;

        while (true) {
            otherStorageCell = JSSet::Helper::nextAndUpdateIterationEntry(vm, *otherStorage, entry);
            if (otherStorageCell == vm.orderedHashTableSentinel())
                break;

            auto* currentStorage = jsCast<JSSet::Storage*>(otherStorageCell);
            entry = JSSet::Helper::iterationEntry(*currentStorage) + 1;
            JSValue entryKey = JSSet::Helper::getIterationEntryKey(*currentStorage);

            result->add(globalObject, entryKey);
            RETURN_IF_EXCEPTION(scope, { });

            otherStorage = currentStorage;
        }
    }

    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(setProtoFuncUnion, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSSet* thisSet = getSet(globalObject, callFrame->thisValue());
    RETURN_IF_EXCEPTION(scope, { });

    JSValue otherValue = callFrame->argument(0);

    if (otherValue.isCell()) [[likely]] {
        if (auto* otherSet = jsDynamicCast<JSSet*>(otherValue.asCell())) [[likely]] {
            if (setPrimordialWatchpointIsValid(vm, otherSet)) [[likely]] {
                scope.release();
                return fastSetUnion(globalObject, thisSet, otherSet);
            }
        }
    }

    // unused but getSetSizeAsInt call is observable
    getSetSizeAsInt(globalObject, otherValue);
    RETURN_IF_EXCEPTION(scope, { });

    ASSERT(otherValue.isObject());
    JSObject* otherObject = asObject(otherValue);

    JSValue has = otherObject->get(globalObject, vm.propertyNames->has);
    RETURN_IF_EXCEPTION(scope, { });
    if (!has.isCallable()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Set.prototype.union expects other.has to be callable"_s);

    JSValue keys = otherObject->get(globalObject, vm.propertyNames->keys);
    RETURN_IF_EXCEPTION(scope, { });
    if (!keys.isCallable()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Set.prototype.union expects other.keys to be callable"_s);

    CallData keysCallData = JSC::getCallDataInline(keys);
    MarkedArgumentBuffer args;
    ASSERT(!args.hasOverflowed());
    JSValue iterator = call(globalObject, keys, keysCallData, otherValue, args);
    RETURN_IF_EXCEPTION(scope, { });

    IterationRecord iterationRecord = iteratorDirect(globalObject, iterator);
    RETURN_IF_EXCEPTION(scope, { });

    JSSet* result = thisSet->clone(globalObject, vm, globalObject->setStructure());
    RETURN_IF_EXCEPTION(scope, { });

    scope.release();
    forEachInIterationRecord(globalObject, iterationRecord, [&](VM&, JSGlobalObject* globalObject, JSValue key) -> void {
        result->add(globalObject, key);
        RETURN_IF_EXCEPTION(scope, void());
    });

    return JSValue::encode(result);
}

static EncodedJSValue fastSetIsSubsetOf(JSGlobalObject* globalObject, JSSet* thisSet, JSSet* otherSet)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (thisSet->size() > otherSet->size())
        return JSValue::encode(jsBoolean(false));

    JSCell* thisStorageCell = thisSet->storageOrSentinel(vm);
    if (thisStorageCell == vm.orderedHashTableSentinel())
        return JSValue::encode(jsBoolean(true));

    auto* thisStorage = jsCast<JSSet::Storage*>(thisStorageCell);
    JSSet::Helper::Entry entry = 0;

    while (true) {
        thisStorageCell = JSSet::Helper::nextAndUpdateIterationEntry(vm, *thisStorage, entry);
        if (thisStorageCell == vm.orderedHashTableSentinel())
            break;

        auto* currentStorage = jsCast<JSSet::Storage*>(thisStorageCell);
        entry = JSSet::Helper::iterationEntry(*currentStorage) + 1;
        JSValue entryKey = JSSet::Helper::getIterationEntryKey(*currentStorage);

        bool otherHasEntry = otherSet->has(globalObject, entryKey);
        RETURN_IF_EXCEPTION(scope, { });
        if (!otherHasEntry)
            return JSValue::encode(jsBoolean(false));

        thisStorage = currentStorage;
    }

    return JSValue::encode(jsBoolean(true));
}

static EncodedJSValue fastSetDifference(JSGlobalObject* globalObject, JSSet* thisSet, JSSet* otherSet)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSSet* result = JSSet::create(vm, globalObject->setStructure());

    JSCell* thisStorageCell = thisSet->storageOrSentinel(vm);
    if (thisStorageCell == vm.orderedHashTableSentinel())
        return JSValue::encode(result);

    auto* thisStorage = jsCast<JSSet::Storage*>(thisStorageCell);
    JSSet::Helper::Entry entry = 0;

    while (true) {
        thisStorageCell = JSSet::Helper::nextAndUpdateIterationEntry(vm, *thisStorage, entry);
        if (thisStorageCell == vm.orderedHashTableSentinel())
            break;

        auto* currentStorage = jsCast<JSSet::Storage*>(thisStorageCell);
        entry = JSSet::Helper::iterationEntry(*currentStorage) + 1;
        JSValue entryKey = JSSet::Helper::getIterationEntryKey(*currentStorage);

        bool otherHasEntry = otherSet->has(globalObject, entryKey);
        RETURN_IF_EXCEPTION(scope, { });
        if (!otherHasEntry) {
            result->add(globalObject, entryKey);
            RETURN_IF_EXCEPTION(scope, { });
        }

        thisStorage = currentStorage;
    }

    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(setProtoFuncDifference, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSSet* thisSet = getSet(globalObject, callFrame->thisValue());
    RETURN_IF_EXCEPTION(scope, { });

    JSValue otherValue = callFrame->argument(0);

    if (otherValue.isCell()) [[likely]] {
        if (auto* otherSet = jsDynamicCast<JSSet*>(otherValue.asCell())) [[likely]] {
            if (setPrimordialWatchpointIsValid(vm, otherSet)) [[likely]] {
                scope.release();
                return fastSetDifference(globalObject, thisSet, otherSet);
            }
        }
    }

    uint32_t otherSize = getSetSizeAsInt(globalObject, otherValue);
    RETURN_IF_EXCEPTION(scope, { });

    ASSERT(otherValue.isObject());
    JSObject* otherObject = asObject(otherValue);

    JSValue has = otherObject->get(globalObject, vm.propertyNames->has);
    RETURN_IF_EXCEPTION(scope, { });
    if (!has.isCallable()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Set.prototype.difference expects other.has to be callable"_s);

    JSValue keys = otherObject->get(globalObject, vm.propertyNames->keys);
    RETURN_IF_EXCEPTION(scope, { });
    if (!keys.isCallable()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Set.prototype.difference expects other.keys to be callable"_s);

    JSSet* result = thisSet->clone(globalObject, vm, globalObject->setStructure());
    RETURN_IF_EXCEPTION(scope, { });

    if (result->size() <= otherSize) {
        JSCell* resultStorageCell = result->storageOrSentinel(vm);
        if (resultStorageCell == vm.orderedHashTableSentinel())
            return JSValue::encode(result);

        CallData hasCallData = JSC::getCallDataInline(has);
        std::optional<CachedCall> cachedHasCall;
        if (hasCallData.type == CallData::Type::JS) [[likely]] {
            cachedHasCall.emplace(globalObject, jsCast<JSFunction*>(has), 1);
            RETURN_IF_EXCEPTION(scope, { });
        }

        auto* resultStorage = jsCast<JSSet::Storage*>(resultStorageCell);
        JSSet::Helper::Entry entry = 0;

        while (true) {
            resultStorageCell = JSSet::Helper::nextAndUpdateIterationEntry(vm, *resultStorage, entry);
            if (resultStorageCell == vm.orderedHashTableSentinel())
                break;

            auto* currentStorage = jsCast<JSSet::Storage*>(resultStorageCell);
            entry = JSSet::Helper::iterationEntry(*currentStorage) + 1;
            JSValue entryKey = JSSet::Helper::getIterationEntryKey(*currentStorage);

            JSValue hasResult;
            if (cachedHasCall) [[likely]] {
                hasResult = cachedHasCall->callWithArguments(globalObject, otherValue, entryKey);
                RETURN_IF_EXCEPTION(scope, { });
            } else {
                MarkedArgumentBuffer hasArgs;
                hasArgs.append(entryKey);
                ASSERT(!hasArgs.hasOverflowed());
                hasResult = call(globalObject, has, hasCallData, otherValue, hasArgs);
                RETURN_IF_EXCEPTION(scope, { });
            }

            bool otherHasValue = hasResult.toBoolean(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            if (otherHasValue) {
                result->remove(globalObject, entryKey);
                RETURN_IF_EXCEPTION(scope, { });
            }

            resultStorage = currentStorage;
        }
    } else {
        CallData keysCallData = JSC::getCallDataInline(keys);
        MarkedArgumentBuffer keysArgs;
        ASSERT(!keysArgs.hasOverflowed());
        JSValue keysResult = call(globalObject, keys, keysCallData, otherValue, keysArgs);
        RETURN_IF_EXCEPTION(scope, { });

        JSValue nextMethod = keysResult.get(globalObject, vm.propertyNames->next);
        RETURN_IF_EXCEPTION(scope, { });
        if (!nextMethod.isCallable()) [[unlikely]]
            return throwVMTypeError(globalObject, scope, "Set.prototype.difference expects other.keys().next to be callable"_s);

        CallData nextCallData = JSC::getCallDataInline(nextMethod);

        std::optional<CachedCall> cachedNextCall;
        if (nextCallData.type == CallData::Type::JS) [[likely]] {
            cachedNextCall.emplace(globalObject, jsCast<JSFunction*>(nextMethod), 0);
            RETURN_IF_EXCEPTION(scope, { });
        }

        while (true) {
            JSValue nextResult;
            if (cachedNextCall) [[likely]] {
                nextResult = cachedNextCall->callWithArguments(globalObject, keysResult);
                RETURN_IF_EXCEPTION(scope, { });
            } else {
                MarkedArgumentBuffer nextArgs;
                ASSERT(!nextArgs.hasOverflowed());
                nextResult = call(globalObject, nextMethod, nextCallData, keysResult, nextArgs);
                RETURN_IF_EXCEPTION(scope, { });
            }

            JSValue doneValue = nextResult.get(globalObject, vm.propertyNames->done);
            RETURN_IF_EXCEPTION(scope, { });

            bool done = doneValue.toBoolean(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            if (done)
                break;

            JSValue value = nextResult.get(globalObject, vm.propertyNames->value);
            RETURN_IF_EXCEPTION(scope, { });

            bool resultHasValue = result->has(globalObject, value);
            RETURN_IF_EXCEPTION(scope, { });
            if (resultHasValue) {
                result->remove(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
            }
        }
    }

    return JSValue::encode(result);
}

static EncodedJSValue fastSetSymmetricDifference(JSGlobalObject* globalObject, JSSet* thisSet, JSSet* otherSet)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSSet* result = thisSet->clone(globalObject, vm, globalObject->setStructure());
    RETURN_IF_EXCEPTION(scope, { });

    JSCell* otherStorageCell = otherSet->storageOrSentinel(vm);
    if (otherStorageCell != vm.orderedHashTableSentinel()) {
        auto* otherStorage = jsCast<JSSet::Storage*>(otherStorageCell);
        JSSet::Helper::Entry entry = 0;

        while (true) {
            otherStorageCell = JSSet::Helper::nextAndUpdateIterationEntry(vm, *otherStorage, entry);
            if (otherStorageCell == vm.orderedHashTableSentinel())
                break;

            auto* currentStorage = jsCast<JSSet::Storage*>(otherStorageCell);
            entry = JSSet::Helper::iterationEntry(*currentStorage) + 1;
            JSValue entryKey = JSSet::Helper::getIterationEntryKey(*currentStorage);

            bool thisHasEntry = thisSet->has(globalObject, entryKey);
            RETURN_IF_EXCEPTION(scope, { });

            if (thisHasEntry) {
                result->remove(globalObject, entryKey);
                RETURN_IF_EXCEPTION(scope, { });
            } else {
                result->add(globalObject, entryKey);
                RETURN_IF_EXCEPTION(scope, { });
            }

            otherStorage = currentStorage;
        }
    }

    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(setProtoFuncSymmetricDifference, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSSet* thisSet = getSet(globalObject, callFrame->thisValue());
    RETURN_IF_EXCEPTION(scope, { });

    JSValue otherValue = callFrame->argument(0);

    if (otherValue.isCell()) [[likely]] {
        if (auto* otherSet = jsDynamicCast<JSSet*>(otherValue.asCell())) [[likely]] {
            if (setPrimordialWatchpointIsValid(vm, otherSet)) [[likely]] {
                scope.release();
                return fastSetSymmetricDifference(globalObject, thisSet, otherSet);
            }
        }
    }

    getSetSizeAsInt(globalObject, otherValue);
    RETURN_IF_EXCEPTION(scope, { });

    ASSERT(otherValue.isObject());
    JSObject* otherObject = asObject(otherValue);

    JSValue has = otherObject->get(globalObject, vm.propertyNames->has);
    RETURN_IF_EXCEPTION(scope, { });
    if (!has.isCallable()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Set.prototype.symmetricDifference expects other.has to be callable"_s);

    JSValue keys = otherObject->get(globalObject, vm.propertyNames->keys);
    RETURN_IF_EXCEPTION(scope, { });
    if (!keys.isCallable()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Set.prototype.symmetricDifference expects other.keys to be callable"_s);

    CallData keysCallData = JSC::getCallDataInline(keys);
    MarkedArgumentBuffer keysArgs;
    ASSERT(!keysArgs.hasOverflowed());
    JSValue keysResult = call(globalObject, keys, keysCallData, otherValue, keysArgs);
    RETURN_IF_EXCEPTION(scope, { });

    JSValue nextMethod = keysResult.get(globalObject, vm.propertyNames->next);
    RETURN_IF_EXCEPTION(scope, { });
    if (!nextMethod.isCallable()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Set.prototype.symmetricDifference expects other.keys().next to be callable"_s);

    JSSet* result = thisSet->clone(globalObject, vm, globalObject->setStructure());
    RETURN_IF_EXCEPTION(scope, { });

    CallData nextCallData = JSC::getCallDataInline(nextMethod);

    std::optional<CachedCall> cachedNextCall;
    if (nextCallData.type == CallData::Type::JS) [[likely]] {
        cachedNextCall.emplace(globalObject, jsCast<JSFunction*>(nextMethod), 0);
        RETURN_IF_EXCEPTION(scope, { });
    }

    while (true) {
        JSValue nextResult;
        if (cachedNextCall) [[likely]] {
            nextResult = cachedNextCall->callWithArguments(globalObject, keysResult);
            RETURN_IF_EXCEPTION(scope, { });
        } else {
            MarkedArgumentBuffer nextArgs;
            ASSERT(!nextArgs.hasOverflowed());
            nextResult = call(globalObject, nextMethod, nextCallData, keysResult, nextArgs);
            RETURN_IF_EXCEPTION(scope, { });
        }

        JSValue doneValue = nextResult.get(globalObject, vm.propertyNames->done);
        RETURN_IF_EXCEPTION(scope, { });

        bool done = doneValue.toBoolean(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (done)
            break;

        JSValue value = nextResult.get(globalObject, vm.propertyNames->value);
        RETURN_IF_EXCEPTION(scope, { });

        bool thisHasValue = thisSet->has(globalObject, value);
        RETURN_IF_EXCEPTION(scope, { });

        if (thisHasValue) {
            result->remove(globalObject, value);
            RETURN_IF_EXCEPTION(scope, { });
        } else {
            result->add(globalObject, value);
            RETURN_IF_EXCEPTION(scope, { });
        }
    }

    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(setProtoFuncIsSubsetOf, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSSet* thisSet = getSet(globalObject, callFrame->thisValue());
    RETURN_IF_EXCEPTION(scope, { });

    JSValue otherValue = callFrame->argument(0);

    if (otherValue.isCell()) [[likely]] {
        if (auto* otherSet = jsDynamicCast<JSSet*>(otherValue.asCell())) [[likely]] {
            if (setPrimordialWatchpointIsValid(vm, otherSet)) [[likely]] {
                scope.release();
                return fastSetIsSubsetOf(globalObject, thisSet, otherSet);
            }
        }
    }

    uint32_t otherSize = getSetSizeAsInt(globalObject, otherValue);
    RETURN_IF_EXCEPTION(scope, { });

    ASSERT(otherValue.isObject());
    JSObject* otherObject = asObject(otherValue);

    JSValue has = otherObject->get(globalObject, vm.propertyNames->has);
    RETURN_IF_EXCEPTION(scope, { });
    if (!has.isCallable()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Set.prototype.isSubsetOf expects other.has to be callable"_s);

    JSValue keys = otherObject->get(globalObject, vm.propertyNames->keys);
    RETURN_IF_EXCEPTION(scope, { });
    if (!keys.isCallable()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Set.prototype.isSubsetOf expects other.keys to be callable"_s);

    if (thisSet->size() > otherSize)
        return JSValue::encode(jsBoolean(false));

    CallData hasCallData = JSC::getCallDataInline(has);
    JSCell* thisStorageCell = thisSet->storageOrSentinel(vm);
    if (thisStorageCell == vm.orderedHashTableSentinel())
        return JSValue::encode(jsBoolean(true));

    auto* thisStorage = jsCast<JSSet::Storage*>(thisStorageCell);
    JSSet::Helper::Entry entry = 0;

    std::optional<CachedCall> cachedHasCall;
    if (hasCallData.type == CallData::Type::JS) [[likely]] {
        cachedHasCall.emplace(globalObject, jsCast<JSFunction*>(has), 1);
        RETURN_IF_EXCEPTION(scope, { });
    }

    while (true) {
        thisStorageCell = JSSet::Helper::nextAndUpdateIterationEntry(vm, *thisStorage, entry);
        if (thisStorageCell == vm.orderedHashTableSentinel())
            break;

        auto* currentStorage = jsCast<JSSet::Storage*>(thisStorageCell);
        entry = JSSet::Helper::iterationEntry(*currentStorage) + 1;
        JSValue entryKey = JSSet::Helper::getIterationEntryKey(*currentStorage);

        JSValue hasResult;
        if (cachedHasCall) [[likely]] {
            hasResult = cachedHasCall->callWithArguments(globalObject, otherValue, entryKey);
            RETURN_IF_EXCEPTION(scope, { });
        } else {
            MarkedArgumentBuffer args;
            args.append(entryKey);
            ASSERT(!args.hasOverflowed());
            hasResult = call(globalObject, has, hasCallData, otherValue, args);
            RETURN_IF_EXCEPTION(scope, { });
        }

        bool hasResultBool = hasResult.toBoolean(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (!hasResultBool)
            return JSValue::encode(jsBoolean(false));

        thisStorage = currentStorage;
    }

    return JSValue::encode(jsBoolean(true));
}

static EncodedJSValue fastSetIsSupersetOf(JSGlobalObject* globalObject, JSSet* thisSet, JSSet* otherSet)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (thisSet->size() < otherSet->size())
        return JSValue::encode(jsBoolean(false));

    JSCell* otherStorageCell = otherSet->storageOrSentinel(vm);
    if (otherStorageCell == vm.orderedHashTableSentinel())
        return JSValue::encode(jsBoolean(true));

    auto* otherStorage = jsCast<JSSet::Storage*>(otherStorageCell);
    JSSet::Helper::Entry entry = 0;

    while (true) {
        otherStorageCell = JSSet::Helper::nextAndUpdateIterationEntry(vm, *otherStorage, entry);
        if (otherStorageCell == vm.orderedHashTableSentinel())
            break;

        auto* currentStorage = jsCast<JSSet::Storage*>(otherStorageCell);
        entry = JSSet::Helper::iterationEntry(*currentStorage) + 1;
        JSValue entryKey = JSSet::Helper::getIterationEntryKey(*currentStorage);

        bool thisHasEntry = thisSet->has(globalObject, entryKey);
        RETURN_IF_EXCEPTION(scope, { });
        if (!thisHasEntry)
            return JSValue::encode(jsBoolean(false));

        otherStorage = currentStorage;
    }

    return JSValue::encode(jsBoolean(true));
}

JSC_DEFINE_HOST_FUNCTION(setProtoFuncIsSupersetOf, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSSet* thisSet = getSet(globalObject, callFrame->thisValue());
    RETURN_IF_EXCEPTION(scope, { });

    JSValue otherValue = callFrame->argument(0);

    if (otherValue.isCell()) [[likely]] {
        if (auto* otherSet = jsDynamicCast<JSSet*>(otherValue.asCell())) [[likely]] {
            if (setPrimordialWatchpointIsValid(vm, otherSet)) [[likely]] {
                scope.release();
                return fastSetIsSupersetOf(globalObject, thisSet, otherSet);
            }
        }
    }

    uint32_t otherSize = getSetSizeAsInt(globalObject, otherValue);
    RETURN_IF_EXCEPTION(scope, { });

    ASSERT(otherValue.isObject());
    JSObject* otherObject = asObject(otherValue);

    JSValue has = otherObject->get(globalObject, vm.propertyNames->has);
    RETURN_IF_EXCEPTION(scope, { });
    if (!has.isCallable()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Set.prototype.isSupersetOf expects other.has to be callable"_s);

    JSValue keys = otherObject->get(globalObject, vm.propertyNames->keys);
    RETURN_IF_EXCEPTION(scope, { });
    if (!keys.isCallable()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Set.prototype.isSupersetOf expects other.keys to be callable"_s);

    if (thisSet->size() < otherSize)
        return JSValue::encode(jsBoolean(false));

    CallData keysCallData = JSC::getCallDataInline(keys);
    MarkedArgumentBuffer keysArgs;
    ASSERT(!keysArgs.hasOverflowed());
    JSValue keysResult = call(globalObject, keys, keysCallData, otherValue, keysArgs);
    RETURN_IF_EXCEPTION(scope, { });

    JSValue nextMethod = keysResult.get(globalObject, vm.propertyNames->next);
    RETURN_IF_EXCEPTION(scope, { });
    if (!nextMethod.isCallable()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Set.prototype.isSupersetOf expects other.keys().next to be callable"_s);

    CallData nextCallData = JSC::getCallDataInline(nextMethod);

    std::optional<CachedCall> cachedNextCall;
    if (nextCallData.type == CallData::Type::JS) [[likely]] {
        cachedNextCall.emplace(globalObject, jsCast<JSFunction*>(nextMethod), 0);
        RETURN_IF_EXCEPTION(scope, { });
    }

    while (true) {
        JSValue nextResult;
        if (cachedNextCall) [[likely]] {
            nextResult = cachedNextCall->callWithArguments(globalObject, keysResult);
            RETURN_IF_EXCEPTION(scope, { });
        } else {
            MarkedArgumentBuffer nextArgs;
            ASSERT(!nextArgs.hasOverflowed());
            nextResult = call(globalObject, nextMethod, nextCallData, keysResult, nextArgs);
            RETURN_IF_EXCEPTION(scope, { });
        }

        JSValue doneValue = nextResult.get(globalObject, vm.propertyNames->done);
        RETURN_IF_EXCEPTION(scope, { });

        bool done = doneValue.toBoolean(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (done)
            break;

        JSValue value = nextResult.get(globalObject, vm.propertyNames->value);
        RETURN_IF_EXCEPTION(scope, { });

        bool thisHasValue = thisSet->has(globalObject, value);
        RETURN_IF_EXCEPTION(scope, { });
        if (!thisHasValue) {
            scope.release();
            iteratorClose(globalObject, keysResult);
            return JSValue::encode(jsBoolean(false));
        }
    }

    return JSValue::encode(jsBoolean(true));
}

static EncodedJSValue fastSetIsDisjointFrom(JSGlobalObject* globalObject, JSSet* thisSet, JSSet* otherSet)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSSet* smallerSet = otherSet;
    JSSet* largerSet = thisSet;
    if (thisSet->size() <= otherSet->size()) {
        smallerSet = thisSet;
        largerSet = otherSet;
    }

    JSCell* smallerStorageCell = smallerSet->storageOrSentinel(vm);
    if (smallerStorageCell == vm.orderedHashTableSentinel())
        return JSValue::encode(jsBoolean(true));

    auto* smallerStorage = jsCast<JSSet::Storage*>(smallerStorageCell);
    JSSet::Helper::Entry entry = 0;

    while (true) {
        smallerStorageCell = JSSet::Helper::nextAndUpdateIterationEntry(vm, *smallerStorage, entry);
        if (smallerStorageCell == vm.orderedHashTableSentinel())
            break;

        auto* currentStorage = jsCast<JSSet::Storage*>(smallerStorageCell);
        entry = JSSet::Helper::iterationEntry(*currentStorage) + 1;
        JSValue entryKey = JSSet::Helper::getIterationEntryKey(*currentStorage);

        bool largerHasEntry = largerSet->has(globalObject, entryKey);
        RETURN_IF_EXCEPTION(scope, { });
        if (largerHasEntry)
            return JSValue::encode(jsBoolean(false));

        smallerStorage = currentStorage;
    }

    return JSValue::encode(jsBoolean(true));
}

JSC_DEFINE_HOST_FUNCTION(setProtoFuncIsDisjointFrom, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSSet* thisSet = getSet(globalObject, callFrame->thisValue());
    RETURN_IF_EXCEPTION(scope, { });

    JSValue otherValue = callFrame->argument(0);

    if (otherValue.isCell()) [[likely]] {
        if (auto* otherSet = jsDynamicCast<JSSet*>(otherValue.asCell())) [[likely]] {
            if (setPrimordialWatchpointIsValid(vm, otherSet)) [[likely]] {
                scope.release();
                return fastSetIsDisjointFrom(globalObject, thisSet, otherSet);
            }
        }
    }

    uint32_t otherSize = getSetSizeAsInt(globalObject, otherValue);
    RETURN_IF_EXCEPTION(scope, { });

    ASSERT(otherValue.isObject());
    JSObject* otherObject = asObject(otherValue);

    JSValue has = otherObject->get(globalObject, vm.propertyNames->has);
    RETURN_IF_EXCEPTION(scope, { });
    if (!has.isCallable()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Set.prototype.isDisjointFrom expects other.has to be callable"_s);

    JSValue keys = otherObject->get(globalObject, vm.propertyNames->keys);
    RETURN_IF_EXCEPTION(scope, { });
    if (!keys.isCallable()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Set.prototype.isDisjointFrom expects other.keys to be callable"_s);

    if (thisSet->size() <= otherSize) {
        JSCell* thisStorageCell = thisSet->storageOrSentinel(vm);
        if (thisStorageCell == vm.orderedHashTableSentinel())
            return JSValue::encode(jsBoolean(true));

        auto* thisStorage = jsCast<JSSet::Storage*>(thisStorageCell);
        JSSet::Helper::Entry entry = 0;

        CallData hasCallData = JSC::getCallDataInline(has);
        std::optional<CachedCall> cachedHasCall;
        if (hasCallData.type == CallData::Type::JS) [[likely]] {
            cachedHasCall.emplace(globalObject, jsCast<JSFunction*>(has), 1);
            RETURN_IF_EXCEPTION(scope, { });
        }

        while (true) {
            thisStorageCell = JSSet::Helper::nextAndUpdateIterationEntry(vm, *thisStorage, entry);
            if (thisStorageCell == vm.orderedHashTableSentinel())
                break;

            auto* currentStorage = jsCast<JSSet::Storage*>(thisStorageCell);
            entry = JSSet::Helper::iterationEntry(*currentStorage) + 1;
            JSValue entryKey = JSSet::Helper::getIterationEntryKey(*currentStorage);

            JSValue hasResult;
            if (cachedHasCall) [[likely]] {
                hasResult = cachedHasCall->callWithArguments(globalObject, otherValue, entryKey);
                RETURN_IF_EXCEPTION(scope, { });
            } else {
                MarkedArgumentBuffer hasArgs;
                hasArgs.append(entryKey);
                ASSERT(!hasArgs.hasOverflowed());
                hasResult = call(globalObject, has, hasCallData, otherValue, hasArgs);
                RETURN_IF_EXCEPTION(scope, { });
            }

            bool otherHasValue = hasResult.toBoolean(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            if (otherHasValue)
                return JSValue::encode(jsBoolean(false));

            thisStorage = currentStorage;
        }
    } else {
        CallData keysCallData = JSC::getCallDataInline(keys);
        MarkedArgumentBuffer keysArgs;
        ASSERT(!keysArgs.hasOverflowed());
        JSValue keysResult = call(globalObject, keys, keysCallData, otherValue, keysArgs);
        RETURN_IF_EXCEPTION(scope, { });

        JSValue nextMethod = keysResult.get(globalObject, vm.propertyNames->next);
        RETURN_IF_EXCEPTION(scope, { });
        if (!nextMethod.isCallable()) [[unlikely]]
            return throwVMTypeError(globalObject, scope, "Set.prototype.isDisjointFrom expects other.keys().next to be callable"_s);

        CallData nextCallData = JSC::getCallDataInline(nextMethod);

        std::optional<CachedCall> cachedNextCall;
        if (nextCallData.type == CallData::Type::JS) [[likely]] {
            cachedNextCall.emplace(globalObject, jsCast<JSFunction*>(nextMethod), 0);
            RETURN_IF_EXCEPTION(scope, { });
        }

        while (true) {
            JSValue nextResult;
            if (cachedNextCall) [[likely]] {
                nextResult = cachedNextCall->callWithArguments(globalObject, keysResult);
                RETURN_IF_EXCEPTION(scope, { });
            } else {
                MarkedArgumentBuffer nextArgs;
                ASSERT(!nextArgs.hasOverflowed());
                nextResult = call(globalObject, nextMethod, nextCallData, keysResult, nextArgs);
                RETURN_IF_EXCEPTION(scope, { });
            }

            JSValue doneValue = nextResult.get(globalObject, vm.propertyNames->done);
            RETURN_IF_EXCEPTION(scope, { });

            bool done = doneValue.toBoolean(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            if (done)
                break;

            JSValue value = nextResult.get(globalObject, vm.propertyNames->value);
            RETURN_IF_EXCEPTION(scope, { });

            bool thisHasValue = thisSet->has(globalObject, value);
            RETURN_IF_EXCEPTION(scope, { });
            if (thisHasValue) {
                scope.release();
                iteratorClose(globalObject, keysResult);
                return JSValue::encode(jsBoolean(false));
            }
        }
    }

    return JSValue::encode(jsBoolean(true));
}

inline JSValue createSetIteratorObject(JSGlobalObject* globalObject, CallFrame* callFrame, IterationKind kind)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    JSSet* set = getSet(globalObject, thisValue);
    RETURN_IF_EXCEPTION(scope, jsUndefined());

    RELEASE_AND_RETURN(scope, JSSetIterator::create(vm, globalObject->setIteratorStructure(), set, kind));
}

JSC_DEFINE_HOST_FUNCTION(setProtoFuncValues, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(createSetIteratorObject(globalObject, callFrame, IterationKind::Values));
}

JSC_DEFINE_HOST_FUNCTION(setProtoFuncEntries, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(createSetIteratorObject(globalObject, callFrame, IterationKind::Entries));
}

}

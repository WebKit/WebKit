/*
 * Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#include <JavaScriptCore/CachedCallInlines.h>
#include <JavaScriptCore/IterationModeMetadata.h>
#include <JavaScriptCore/JSArrayIterator.h>
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/JSGlobalObjectInlines.h>
#include <JavaScriptCore/JSMapInlines.h>
#include <JavaScriptCore/JSMapIterator.h>
#include <JavaScriptCore/JSObjectInlines.h>
#include <JavaScriptCore/JSSetInlines.h>
#include <JavaScriptCore/JSSetIterator.h>
#include <JavaScriptCore/MapIteratorPrototypeInlines.h>
#include <JavaScriptCore/SetIteratorPrototypeInlines.h>
#include <JavaScriptCore/ThrowScope.h>

namespace JSC {

struct IterationRecord {
    JSValue iterator;
    JSValue nextMethod;
};

JSValue iteratorNext(JSGlobalObject*, IterationRecord, JSValue argument = JSValue());
JS_EXPORT_PRIVATE JSValue iteratorNextExported(JSGlobalObject*, IterationRecord, JSValue argument = JSValue());
JSValue iteratorNextWithCachedCall(JSGlobalObject*, IterationRecord, CachedCall*, JSValue argument = JSValue());
JS_EXPORT_PRIVATE JSValue iteratorValue(JSGlobalObject*, JSValue iterResult);
bool iteratorComplete(JSGlobalObject*, JSValue iterResult);
JS_EXPORT_PRIVATE bool iteratorCompleteExported(JSGlobalObject*, JSValue iterResult);
JS_EXPORT_PRIVATE JSValue iteratorStep(JSGlobalObject*, IterationRecord);
JS_EXPORT_PRIVATE JSValue iteratorStepWithCachedCall(JSGlobalObject*, IterationRecord, CachedCall*);
JS_EXPORT_PRIVATE void iteratorClose(JSGlobalObject*, JSValue iterator);
JS_EXPORT_PRIVATE JSObject* createIteratorResultObject(JSGlobalObject*, JSValue, bool done);

Structure* createIteratorResultObjectStructure(VM&, JSGlobalObject&);

JS_EXPORT_PRIVATE JSValue iteratorMethod(JSGlobalObject*, JSObject*);
JS_EXPORT_PRIVATE IterationRecord iteratorForIterable(JSGlobalObject*, JSObject*, JSValue iteratorMethod);
JS_EXPORT_PRIVATE IterationRecord iteratorForIterable(JSGlobalObject*, JSValue iterable);
JS_EXPORT_PRIVATE IterationRecord iteratorDirect(JSGlobalObject*, JSValue);
IterationRecord getAsyncIterator(JSGlobalObject&, JSValue);
JS_EXPORT_PRIVATE IterationRecord getAsyncIteratorExported(JSGlobalObject&, JSValue);

JS_EXPORT_PRIVATE JSValue iteratorMethod(JSGlobalObject*, JSObject*);
JS_EXPORT_PRIVATE bool hasIteratorMethod(JSGlobalObject*, JSValue);

enum class IterableValidationResult : uint8_t {
    Valid,
    NullNotIterable,
    UndefinedNotIterable,
    NumberNotIterable,
    BooleanNotIterable,
    SymbolNotIterable,
    ObjectNotIterable,
    ValueNotIterable
};

JS_EXPORT_PRIVATE IterableValidationResult validateIterable(VM&, JSValue iterable, JSValue symbolIterator);
JS_EXPORT_PRIVATE ASCIILiteral getIteratorErrorMessage(IterableValidationResult, JSValue iterable);

JS_EXPORT_PRIVATE IterationMode getIterationMode(VM&, JSGlobalObject*, JSValue iterable);
JS_EXPORT_PRIVATE IterationMode getIterationMode(VM&, JSGlobalObject*, JSValue iterable, JSValue symbolIterator);


static ALWAYS_INLINE void forEachInMapStorage(VM& vm, JSGlobalObject* globalObject, JSCell* storageCell, JSMap::Helper::Entry startEntry, IterationKind iterationKind, NOESCAPE const Invocable<void(VM&, JSGlobalObject*, JSValue)> auto& callback)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* storage = jsCast<JSMap::Storage*>(storageCell);
    JSMap::Helper::Entry entry = startEntry;

    while (true) {
        storageCell = JSMap::Helper::nextAndUpdateIterationEntry(vm, *storage, entry);
        if (storageCell == vm.orderedHashTableSentinel())
            break;

        storage = jsCast<JSMap::Storage*>(storageCell);
        entry = JSMap::Helper::iterationEntry(*storage) + 1;

        JSValue value;
        switch (iterationKind) {
        case IterationKind::Keys:
            value = JSMap::Helper::getIterationEntryKey(*storage);
            break;
        case IterationKind::Values:
            value = JSMap::Helper::getIterationEntryValue(*storage);
            break;
        case IterationKind::Entries: {
            JSValue entryKey = JSMap::Helper::getIterationEntryKey(*storage);
            JSValue entryValue = JSMap::Helper::getIterationEntryValue(*storage);
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=298574
            value = constructArrayPair(globalObject, entryKey, entryValue);
            RETURN_IF_EXCEPTION(scope, void());
            break;
        }
        }

        callback(vm, globalObject, value);
        RETURN_IF_EXCEPTION(scope, void());
    }
}

static ALWAYS_INLINE void forEachInSetStorage(VM& vm, JSGlobalObject* globalObject, JSCell* storageCell, JSSet::Helper::Entry startEntry, NOESCAPE const Invocable<void(VM&, JSGlobalObject*, JSValue)> auto& callback)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* storage = jsCast<JSSet::Storage*>(storageCell);
    JSSet::Helper::Entry entry = startEntry;

    while (true) {
        storageCell = JSSet::Helper::nextAndUpdateIterationEntry(vm, *storage, entry);
        if (storageCell == vm.orderedHashTableSentinel())
            break;

        storage = jsCast<JSSet::Storage*>(storageCell);
        entry = JSSet::Helper::iterationEntry(*storage) + 1;
        JSValue entryKey = JSSet::Helper::getIterationEntryKey(*storage);

        callback(vm, globalObject, entryKey);
        RETURN_IF_EXCEPTION(scope, void());
    }
}

static ALWAYS_INLINE void forEachInFastArray(JSGlobalObject* globalObject, JSValue iterable, JSArray* array, NOESCAPE const Invocable<void(VM&, JSGlobalObject*, JSValue)> auto& callback)
{
    UNUSED_PARAM(iterable);

    auto& vm = getVM(globalObject);
    ASSERT(getIterationMode(vm, globalObject, iterable) == IterationMode::FastArray);

    auto scope = DECLARE_THROW_SCOPE(vm);

    for (unsigned index = 0; index < array->length(); ++index) {
        JSValue nextValue = array->getIndex(globalObject, index);
        RETURN_IF_EXCEPTION(scope, void());
        callback(vm, globalObject, nextValue);
        if (scope.exception()) [[unlikely]] {
            scope.release();
            JSArrayIterator* iterator = JSArrayIterator::create(vm, globalObject->arrayIteratorStructure(), array, IterationKind::Values);
            iterator->internalField(JSArrayIterator::Field::Index).setWithoutWriteBarrier(jsNumber(index + 1));
            iteratorClose(globalObject, iterator);
            return;
        }
    }
}

ALWAYS_INLINE void forEachInIterationRecord(JSGlobalObject* globalObject, IterationRecord iterationRecord, NOESCAPE const Invocable<void(VM&, JSGlobalObject*, JSValue)> auto& callback)
{
    auto& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue nextMethod = iterationRecord.nextMethod;
    auto callData = getCallDataInline(nextMethod);

    std::optional<CachedCall> cachedCallHolder;
    CachedCall* cachedCall = nullptr;
    if (callData.type == CallData::Type::JS) [[likely]] {
        cachedCallHolder.emplace(globalObject, jsCast<JSFunction*>(nextMethod), 0);
        if (scope.exception()) [[unlikely]]
            return;
        cachedCall = &cachedCallHolder.value();
    }

    while (true) {
        JSValue next;
        if (cachedCall) [[likely]] {
            cachedCall->clearArguments();
            next = iteratorStepWithCachedCall(globalObject, iterationRecord, cachedCall);
        } else
            next = iteratorStep(globalObject, iterationRecord);
        if (scope.exception()) [[unlikely]]
            return;
        if (next.isFalse())
            return;

        JSValue nextValue = iteratorValue(globalObject, next);
        RETURN_IF_EXCEPTION(scope, void());

        callback(vm, globalObject, nextValue);
        if (scope.exception()) [[unlikely]] {
            scope.release();
            iteratorClose(globalObject, iterationRecord.iterator);
            return;
        }
    }
}

void forEachInIterable(JSGlobalObject* globalObject, JSValue iterable, NOESCAPE const Invocable<void(VM&, JSGlobalObject*, JSValue)> auto& callback)
{
    auto& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (getIterationMode(vm, globalObject, iterable) == IterationMode::FastArray) {
        auto* array = jsCast<JSArray*>(iterable);
        forEachInFastArray(globalObject, iterable, array, callback);
        RETURN_IF_EXCEPTION(scope, void());
        return;
    }

    if (auto* jsMap = jsDynamicCast<JSMap*>(iterable)) {
        if (jsMap->isIteratorProtocolFastAndNonObservable()) {
            JSCell* storageCell = jsMap->storageOrSentinel(vm);
            if (storageCell != vm.orderedHashTableSentinel()) {
                forEachInMapStorage(vm, globalObject, storageCell, 0, IterationKind::Entries, callback);
                RETURN_IF_EXCEPTION(scope, void());
            }
            return;
        }
    } else if (auto* jsSet = jsDynamicCast<JSSet*>(iterable)) {
        if (jsSet->isIteratorProtocolFastAndNonObservable()) {
            JSCell* storageCell = jsSet->storageOrSentinel(vm);
            if (storageCell != vm.orderedHashTableSentinel()) {
                forEachInSetStorage(vm, globalObject, storageCell, 0, callback);
                RETURN_IF_EXCEPTION(scope, void());
            }
            return;
        }
    }

    IterationRecord iterationRecord = iteratorForIterable(globalObject, iterable);
    RETURN_IF_EXCEPTION(scope, void());
    scope.release();
    forEachInIterationRecord(globalObject, iterationRecord, callback);
}

void forEachInIterable(JSGlobalObject& globalObject, JSObject* iterable, JSValue iteratorMethod, NOESCAPE const Invocable<void(VM&, JSGlobalObject&, JSValue)> auto& callback)
{
    auto& vm = getVM(&globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto iterationMode = getIterationMode(vm, &globalObject, iterable, iteratorMethod);
    if (iterationMode == IterationMode::FastArray) {
        auto* array = jsCast<JSArray*>(iterable);
        for (unsigned index = 0; index < array->length(); ++index) {
            JSValue nextValue = array->getIndex(&globalObject, index);
            RETURN_IF_EXCEPTION(scope, void());
            callback(vm, globalObject, nextValue);
            if (scope.exception()) [[unlikely]] {
                scope.release();
                JSArrayIterator* iterator = JSArrayIterator::create(vm, globalObject.arrayIteratorStructure(), array, IterationKind::Values);
                iterator->internalField(JSArrayIterator::Field::Index).setWithoutWriteBarrier(jsNumber(index + 1));
                iteratorClose(&globalObject, iterator);
                return;
            }
        }
        return;
    }

    auto validationResult = validateIterable(vm, iterable, iteratorMethod);
    if (validationResult != IterableValidationResult::Valid) [[unlikely]] {
        throwTypeError(&globalObject, scope, getIteratorErrorMessage(validationResult, iterable));
        return;
    }

    auto iterationRecord = iteratorForIterable(&globalObject, iterable, iteratorMethod);
    RETURN_IF_EXCEPTION(scope, void());

    JSValue nextMethod = iterationRecord.nextMethod;
    auto callData = getCallDataInline(nextMethod);

    std::optional<CachedCall> cachedCallHolder;
    CachedCall* cachedCall = nullptr;
    if (callData.type == CallData::Type::JS) [[likely]] {
        cachedCallHolder.emplace(&globalObject, jsCast<JSFunction*>(nextMethod), 0);
        if (scope.exception()) [[unlikely]]
            return;
        cachedCall = &cachedCallHolder.value();
    }

    while (true) {
        JSValue next;
        if (cachedCall) [[likely]] {
            cachedCall->clearArguments();
            next = iteratorStepWithCachedCall(&globalObject, iterationRecord, cachedCall);
        } else
            next = iteratorStep(&globalObject, iterationRecord);
        if (scope.exception()) [[unlikely]]
            return;
        if (next.isFalse())
            return;

        JSValue nextValue = iteratorValue(&globalObject, next);
        RETURN_IF_EXCEPTION(scope, void());

        callback(vm, globalObject, nextValue);
        if (scope.exception()) [[unlikely]] {
            scope.release();
            iteratorClose(&globalObject, iterationRecord.iterator);
            return;
        }
    }
}

void forEachInIteratorProtocol(JSGlobalObject* globalObject, JSValue iterable, NOESCAPE const Invocable<void(VM&, JSGlobalObject*, JSValue)> auto& callback)
{
    auto& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (auto* mapIterator = jsDynamicCast<JSMapIterator*>(iterable)) {
        if (mapIteratorProtocolIsFastAndNonObservable(vm, mapIterator)) {
            if (JSMap* iteratedMap = jsCast<JSMap*>(mapIterator->iteratedObject())) {
                JSCell* storageCell = iteratedMap->storageOrSentinel(vm);
                if (storageCell != vm.orderedHashTableSentinel()) {
                    JSMap::Helper::Entry startEntry = mapIterator->entry();
                    IterationKind iterationKind = mapIterator->kind();
                    forEachInMapStorage(vm, globalObject, storageCell, startEntry, iterationKind, callback);
                    RETURN_IF_EXCEPTION(scope, void());
                }
                return;
            }
        }
    } else if (auto* setIterator = jsDynamicCast<JSSetIterator*>(iterable)) {
        if (setIteratorProtocolIsFastAndNonObservable(vm, setIterator)) {
            if (JSSet* iteratedSet = jsCast<JSSet*>(setIterator->iteratedObject())) {
                JSCell* storageCell = iteratedSet->storageOrSentinel(vm);
                if (storageCell != vm.orderedHashTableSentinel()) {
                    JSSet::Helper::Entry startEntry = setIterator->entry();
                    forEachInSetStorage(vm, globalObject, storageCell, startEntry, callback);
                    RETURN_IF_EXCEPTION(scope, void());
                }
                return;
            }
        }
    }

    IterationRecord iterationRecord = iteratorDirect(globalObject, iterable);
    RETURN_IF_EXCEPTION(scope, void());
    scope.release();
    forEachInIterationRecord(globalObject, iterationRecord, callback);
}

} // namespace JSC

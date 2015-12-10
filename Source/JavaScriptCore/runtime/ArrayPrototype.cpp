/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007, 2008, 2009, 2011, 2013, 2015 Apple Inc. All rights reserved.
 *  Copyright (C) 2003 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 */

#include "config.h"
#include "ArrayPrototype.h"

#include "BuiltinNames.h"
#include "ButterflyInlines.h"
#include "CachedCall.h"
#include "CodeBlock.h"
#include "CopiedSpaceInlines.h"
#include "Error.h"
#include "Interpreter.h"
#include "JIT.h"
#include "JSArrayIterator.h"
#include "JSCBuiltins.h"
#include "JSCInlines.h"
#include "JSStringBuilder.h"
#include "JSStringJoiner.h"
#include "Lookup.h"
#include "ObjectConstructor.h"
#include "ObjectPrototype.h"
#include "StringRecursionChecker.h"
#include <algorithm>
#include <wtf/Assertions.h>
#include <wtf/HashSet.h>

namespace JSC {

EncodedJSValue JSC_HOST_CALL arrayProtoFuncToLocaleString(ExecState*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncConcat(ExecState*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncJoin(ExecState*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncPop(ExecState*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncPush(ExecState*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncReverse(ExecState*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncShift(ExecState*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncSlice(ExecState*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncSplice(ExecState*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncUnShift(ExecState*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncIndexOf(ExecState*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncReduce(ExecState*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncReduceRight(ExecState*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncLastIndexOf(ExecState*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncKeys(ExecState*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncEntries(ExecState*);

// ------------------------------ ArrayPrototype ----------------------------

const ClassInfo ArrayPrototype::s_info = {"Array", &JSArray::s_info, nullptr, CREATE_METHOD_TABLE(ArrayPrototype)};

ArrayPrototype* ArrayPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    ArrayPrototype* prototype = new (NotNull, allocateCell<ArrayPrototype>(vm.heap)) ArrayPrototype(vm, structure);
    prototype->finishCreation(vm, globalObject);
    return prototype;
}

// ECMA 15.4.4
ArrayPrototype::ArrayPrototype(VM& vm, Structure* structure)
    : JSArray(vm, structure, 0)
{
}

void ArrayPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    vm.prototypeMap.addPrototype(this);

    putDirectWithoutTransition(vm, vm.propertyNames->values, globalObject->arrayProtoValuesFunction(), DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->iteratorSymbol, globalObject->arrayProtoValuesFunction(), DontEnum);
    
    JSC_NATIVE_FUNCTION(vm.propertyNames->toString, arrayProtoFuncToString, DontEnum, 0);
    JSC_NATIVE_FUNCTION(vm.propertyNames->toLocaleString, arrayProtoFuncToLocaleString, DontEnum, 0);
    JSC_NATIVE_FUNCTION("concat", arrayProtoFuncConcat, DontEnum, 1);
    JSC_BUILTIN_FUNCTION("fill", arrayPrototypeFillCodeGenerator, DontEnum);
    JSC_NATIVE_FUNCTION(vm.propertyNames->join, arrayProtoFuncJoin, DontEnum, 1);
    JSC_NATIVE_INTRINSIC_FUNCTION("pop", arrayProtoFuncPop, DontEnum, 0, ArrayPopIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION(vm.propertyNames->builtinNames().pushPublicName(), arrayProtoFuncPush, DontEnum, 1, ArrayPushIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION(vm.propertyNames->builtinNames().pushPrivateName(), arrayProtoFuncPush, DontEnum | DontDelete | ReadOnly, 1, ArrayPushIntrinsic);
    JSC_NATIVE_FUNCTION("reverse", arrayProtoFuncReverse, DontEnum, 0);
    JSC_NATIVE_FUNCTION(vm.propertyNames->builtinNames().shiftPublicName(), arrayProtoFuncShift, DontEnum, 0);
    JSC_NATIVE_FUNCTION(vm.propertyNames->builtinNames().shiftPrivateName(), arrayProtoFuncShift, DontEnum | DontDelete | ReadOnly, 0);
    JSC_NATIVE_FUNCTION(vm.propertyNames->slice, arrayProtoFuncSlice, DontEnum, 2);
    JSC_BUILTIN_FUNCTION("sort", arrayPrototypeSortCodeGenerator, DontEnum);
    JSC_NATIVE_FUNCTION("splice", arrayProtoFuncSplice, DontEnum, 2);
    JSC_NATIVE_FUNCTION("unshift", arrayProtoFuncUnShift, DontEnum, 1);
    JSC_BUILTIN_FUNCTION("every", arrayPrototypeEveryCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION("forEach", arrayPrototypeForEachCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION("some", arrayPrototypeSomeCodeGenerator, DontEnum);
    JSC_NATIVE_FUNCTION("indexOf", arrayProtoFuncIndexOf, DontEnum, 1);
    JSC_NATIVE_FUNCTION("lastIndexOf", arrayProtoFuncLastIndexOf, DontEnum, 1);
    JSC_BUILTIN_FUNCTION("filter", arrayPrototypeFilterCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION("reduce", arrayPrototypeReduceCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION("reduceRight", arrayPrototypeReduceRightCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION("map", arrayPrototypeMapCodeGenerator, DontEnum);
    JSC_NATIVE_FUNCTION(vm.propertyNames->entries, arrayProtoFuncEntries, DontEnum, 0);
    JSC_NATIVE_FUNCTION(vm.propertyNames->keys, arrayProtoFuncKeys, DontEnum, 0);
    JSC_BUILTIN_FUNCTION("find", arrayPrototypeFindCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION("findIndex", arrayPrototypeFindIndexCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION("includes", arrayPrototypeIncludesCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION("copyWithin", arrayPrototypeCopyWithinCodeGenerator, DontEnum);
    
    JSObject* unscopables = constructEmptyObject(globalObject->globalExec(), globalObject->nullPrototypeObjectStructure());
    const char* unscopableNames[] = {
        "copyWithin",
        "entries",
        "fill",
        "find",
        "findIndex",
        "keys",
        "values"
    };
    for (const char* unscopableName : unscopableNames)
        unscopables->putDirect(vm, Identifier::fromString(&vm, unscopableName), jsBoolean(true));
    putDirectWithoutTransition(vm, vm.propertyNames->unscopablesSymbol, unscopables, DontEnum | ReadOnly);
}

// ------------------------------ Array Functions ----------------------------

static ALWAYS_INLINE JSValue getProperty(ExecState* exec, JSObject* object, unsigned index)
{
    if (JSValue result = object->tryGetIndexQuickly(index))
        return result;
    PropertySlot slot(object);
    if (!object->getPropertySlot(exec, index, slot))
        return JSValue();
    return slot.getValue(exec, index);
}

static ALWAYS_INLINE unsigned getLength(ExecState* exec, JSObject* obj)
{
    if (isJSArray(obj))
        return jsCast<JSArray*>(obj)->length();
    return obj->get(exec, exec->propertyNames().length).toUInt32(exec);
}

static void putLength(ExecState* exec, JSObject* obj, JSValue value)
{
    PutPropertySlot slot(obj);
    obj->methodTable()->put(obj, exec, exec->propertyNames().length, value, slot);
}

static inline unsigned argumentClampedIndexFromStartOrEnd(ExecState* exec, int argument, unsigned length, unsigned undefinedValue = 0)
{
    JSValue value = exec->argument(argument);
    if (value.isUndefined())
        return undefinedValue;

    double indexDouble = value.toInteger(exec);
    if (indexDouble < 0) {
        indexDouble += length;
        return indexDouble < 0 ? 0 : static_cast<unsigned>(indexDouble);
    }
    return indexDouble > length ? length : static_cast<unsigned>(indexDouble);
}

// The shift/unshift function implement the shift/unshift behaviour required
// by the corresponding array prototype methods, and by splice. In both cases,
// the methods are operating an an array or array like object.
//
//  header  currentCount  (remainder)
// [------][------------][-----------]
//  header  resultCount  (remainder)
// [------][-----------][-----------]
//
// The set of properties in the range 'header' must be unchanged. The set of
// properties in the range 'remainder' (where remainder = length - header -
// currentCount) will be shifted to the left or right as appropriate; in the
// case of shift this must be removing values, in the case of unshift this
// must be introducing new values.

template<JSArray::ShiftCountMode shiftCountMode>
void shift(ExecState* exec, JSObject* thisObj, unsigned header, unsigned currentCount, unsigned resultCount, unsigned length)
{
    RELEASE_ASSERT(currentCount > resultCount);
    unsigned count = currentCount - resultCount;

    RELEASE_ASSERT(header <= length);
    RELEASE_ASSERT(currentCount <= (length - header));

    if (isJSArray(thisObj)) {
        JSArray* array = asArray(thisObj);
        if (array->length() == length && array->shiftCount<shiftCountMode>(exec, header, count))
            return;
    }

    for (unsigned k = header; k < length - currentCount; ++k) {
        unsigned from = k + currentCount;
        unsigned to = k + resultCount;
        if (JSValue value = getProperty(exec, thisObj, from)) {
            if (exec->hadException())
                return;
            thisObj->putByIndexInline(exec, to, value, true);
            if (exec->hadException())
                return;
        } else if (!thisObj->methodTable(exec->vm())->deletePropertyByIndex(thisObj, exec, to)) {
            throwTypeError(exec, ASCIILiteral("Unable to delete property."));
            return;
        }
    }
    for (unsigned k = length; k > length - count; --k) {
        if (!thisObj->methodTable(exec->vm())->deletePropertyByIndex(thisObj, exec, k - 1)) {
            throwTypeError(exec, ASCIILiteral("Unable to delete property."));
            return;
        }
    }
}

template<JSArray::ShiftCountMode shiftCountMode>
void unshift(ExecState* exec, JSObject* thisObj, unsigned header, unsigned currentCount, unsigned resultCount, unsigned length)
{
    RELEASE_ASSERT(resultCount > currentCount);
    unsigned count = resultCount - currentCount;

    RELEASE_ASSERT(header <= length);
    RELEASE_ASSERT(currentCount <= (length - header));

    // Guard against overflow.
    if (count > (UINT_MAX - length)) {
        throwOutOfMemoryError(exec);
        return;
    }

    if (isJSArray(thisObj)) {
        JSArray* array = asArray(thisObj);
        if (array->length() == length && array->unshiftCount<shiftCountMode>(exec, header, count))
            return;
    }

    for (unsigned k = length - currentCount; k > header; --k) {
        unsigned from = k + currentCount - 1;
        unsigned to = k + resultCount - 1;
        if (JSValue value = getProperty(exec, thisObj, from)) {
            if (exec->hadException())
                return;
            thisObj->putByIndexInline(exec, to, value, true);
        } else if (!thisObj->methodTable(exec->vm())->deletePropertyByIndex(thisObj, exec, to)) {
            throwTypeError(exec, ASCIILiteral("Unable to delete property."));
            return;
        }
        if (exec->hadException())
            return;
    }
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncToString(ExecState* exec)
{
    JSValue thisValue = exec->thisValue().toThis(exec, StrictMode);

    // 1. Let array be the result of calling ToObject on the this value.
    JSObject* thisObject = thisValue.toObject(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    
    // 2. Let func be the result of calling the [[Get]] internal method of array with argument "join".
    JSValue function = JSValue(thisObject).get(exec, exec->propertyNames().join);

    // 3. If IsCallable(func) is false, then let func be the standard built-in method Object.prototype.toString (15.2.4.2).
    if (!function.isCell())
        return JSValue::encode(jsMakeNontrivialString(exec, "[object ", thisObject->methodTable(exec->vm())->className(thisObject), "]"));
    CallData callData;
    CallType callType = getCallData(function, callData);
    if (callType == CallTypeNone)
        return JSValue::encode(jsMakeNontrivialString(exec, "[object ", thisObject->methodTable(exec->vm())->className(thisObject), "]"));

    // 4. Return the result of calling the [[Call]] internal method of func providing array as the this value and an empty arguments list.
    if (!isJSArray(thisObject) || callType != CallTypeHost || callData.native.function != arrayProtoFuncJoin)
        return JSValue::encode(call(exec, function, callType, callData, thisObject, exec->emptyList()));

    ASSERT(isJSArray(thisValue));
    JSArray* thisArray = asArray(thisValue);

    unsigned length = thisArray->length();

    StringRecursionChecker checker(exec, thisArray);
    if (JSValue earlyReturnValue = checker.earlyReturnValue())
        return JSValue::encode(earlyReturnValue);

    JSStringJoiner joiner(*exec, ',', length);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    for (unsigned i = 0; i < length; ++i) {
        JSValue element = thisArray->tryGetIndexQuickly(i);
        if (!element) {
            element = thisArray->get(exec, i);
            if (exec->hadException())
                return JSValue::encode(jsUndefined());
        }
        joiner.append(*exec, element);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    }

    return JSValue::encode(joiner.join(*exec));
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncToLocaleString(ExecState* exec)
{
    JSValue thisValue = exec->thisValue().toThis(exec, StrictMode);

    JSObject* thisObject = thisValue.toObject(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    unsigned length = getLength(exec, thisObject);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    StringRecursionChecker checker(exec, thisObject);
    if (JSValue earlyReturnValue = checker.earlyReturnValue())
        return JSValue::encode(earlyReturnValue);

    JSStringJoiner stringJoiner(*exec, ',', length);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    for (unsigned i = 0; i < length; ++i) {
        JSValue element = thisObject->getIndex(exec, i);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
        if (element.isUndefinedOrNull())
            continue;
        JSValue conversionFunction = element.get(exec, exec->propertyNames().toLocaleString);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
        CallData callData;
        CallType callType = getCallData(conversionFunction, callData);
        if (callType != CallTypeNone) {
            element = call(exec, conversionFunction, callType, callData, element, exec->emptyList());
            if (exec->hadException())
                return JSValue::encode(jsUndefined());
        }
        stringJoiner.append(*exec, element);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    }

    return JSValue::encode(stringJoiner.join(*exec));
}

static inline bool isHole(double value)
{
    return std::isnan(value);
}

static inline bool isHole(const WriteBarrier<Unknown>& value)
{
    return !value;
}

template<typename T> static inline bool containsHole(T* data, unsigned length)
{
    for (unsigned i = 0; i < length; ++i) {
        if (isHole(data[i]))
            return true;
    }
    return false;
}

static inline bool holesMustForwardToPrototype(ExecState& state, JSObject* object)
{
    auto& vm = state.vm();
    return object->structure(vm)->holesMustForwardToPrototype(vm);
}

static inline JSValue join(ExecState& state, JSObject* thisObject, StringView separator)
{
    unsigned length = getLength(&state, thisObject);
    if (state.hadException())
        return jsUndefined();

    switch (thisObject->indexingType()) {
    case ALL_CONTIGUOUS_INDEXING_TYPES:
    case ALL_INT32_INDEXING_TYPES: {
        auto& butterfly = *thisObject->butterfly();
        if (length > butterfly.publicLength())
            break;
        JSStringJoiner joiner(state, separator, length);
        if (state.hadException())
            return jsUndefined();
        auto data = butterfly.contiguous().data();
        bool holesKnownToBeOK = false;
        for (unsigned i = 0; i < length; ++i) {
            if (JSValue value = data[i].get()) {
                joiner.append(state, value);
                if (state.hadException())
                    return jsUndefined();
            } else {
                if (!holesKnownToBeOK) {
                    if (holesMustForwardToPrototype(state, thisObject))
                        goto generalCase;
                    holesKnownToBeOK = true;
                }
                joiner.appendEmptyString();
            }
        }
        return joiner.join(state);
    }
    case ALL_DOUBLE_INDEXING_TYPES: {
        auto& butterfly = *thisObject->butterfly();
        if (length > butterfly.publicLength())
            break;
        JSStringJoiner joiner(state, separator, length);
        if (state.hadException())
            return jsUndefined();
        auto data = butterfly.contiguousDouble().data();
        bool holesKnownToBeOK = false;
        for (unsigned i = 0; i < length; ++i) {
            double value = data[i];
            if (!isHole(value))
                joiner.append(state, jsDoubleNumber(value));
            else {
                if (!holesKnownToBeOK) {
                    if (thisObject->structure(state.vm())->holesMustForwardToPrototype(state.vm()))
                        goto generalCase;
                    holesKnownToBeOK = true;
                }
                joiner.appendEmptyString();
            }
        }
        return joiner.join(state);
    }
    case ALL_ARRAY_STORAGE_INDEXING_TYPES: {
        auto& storage = *thisObject->butterfly()->arrayStorage();
        if (length > storage.vectorLength())
            break;
        if (storage.hasHoles() && thisObject->structure(state.vm())->holesMustForwardToPrototype(state.vm()))
            break;
        JSStringJoiner joiner(state, separator, length);
        if (state.hadException())
            return jsUndefined();
        auto data = storage.vector().data();
        for (unsigned i = 0; i < length; ++i) {
            if (JSValue value = data[i].get()) {
                joiner.append(state, value);
                if (state.hadException())
                    return jsUndefined();
            } else
                joiner.appendEmptyString();
        }
        return joiner.join(state);
    }
    }

generalCase:
    JSStringJoiner joiner(state, separator, length);
    if (state.hadException())
        return jsUndefined();
    for (unsigned i = 0; i < length; ++i) {
        JSValue element = thisObject->getIndex(&state, i);
        if (state.hadException())
            return jsUndefined();
        joiner.append(state, element);
        if (state.hadException())
            return jsUndefined();
    }
    return joiner.join(state);
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncJoin(ExecState* exec)
{
    JSObject* thisObject = exec->thisValue().toThis(exec, StrictMode).toObject(exec);

    StringRecursionChecker checker(exec, thisObject);
    if (JSValue earlyReturnValue = checker.earlyReturnValue())
        return JSValue::encode(earlyReturnValue);

    JSValue separatorValue = exec->argument(0);
    if (separatorValue.isUndefined()) {
        const LChar comma = ',';
        return JSValue::encode(join(*exec, thisObject, { &comma, 1 }));
    }

    JSString* separator = separatorValue.toString(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    return JSValue::encode(join(*exec, thisObject, separator->view(exec)));
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncConcat(ExecState* exec)
{
    JSValue thisValue = exec->thisValue().toThis(exec, StrictMode);
    unsigned argCount = exec->argumentCount();
    JSValue curArg = thisValue.toObject(exec);
    Checked<unsigned, RecordOverflow> finalArraySize = 0;

    JSArray* currentArray = nullptr;
    JSArray* previousArray = nullptr;
    for (unsigned i = 0; ; ++i) {
        previousArray = currentArray;
        currentArray = jsDynamicCast<JSArray*>(curArg);
        if (currentArray) {
            // Can't use JSArray::length here because this might be a RuntimeArray!
            finalArraySize += getLength(exec, currentArray);
            if (exec->hadException())
                return JSValue::encode(jsUndefined());
        } else
            ++finalArraySize;
        if (i == argCount)
            break;
        curArg = exec->uncheckedArgument(i);
    }

    if (finalArraySize.hasOverflowed())
        return JSValue::encode(throwOutOfMemoryError(exec));

    if (argCount == 1 && previousArray && currentArray && finalArraySize.unsafeGet() < MIN_SPARSE_ARRAY_INDEX) {
        IndexingType type = JSArray::fastConcatType(exec->vm(), *previousArray, *currentArray);
        if (type != NonArray)
            return previousArray->fastConcatWith(*exec, *currentArray);
    }

    JSArray* arr = constructEmptyArray(exec, nullptr, finalArraySize.unsafeGet());
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    curArg = thisValue.toObject(exec);
    unsigned n = 0;
    for (unsigned i = 0; ; ++i) {
        if (JSArray* currentArray = jsDynamicCast<JSArray*>(curArg)) {
            // Can't use JSArray::length here because this might be a RuntimeArray!
            unsigned length = getLength(exec, currentArray);
            if (exec->hadException())
                return JSValue::encode(jsUndefined());
            for (unsigned k = 0; k < length; ++k) {
                JSValue v = getProperty(exec, currentArray, k);
                if (exec->hadException())
                    return JSValue::encode(jsUndefined());
                if (v)
                    arr->putDirectIndex(exec, n, v);
                n++;
            }
        } else {
            arr->putDirectIndex(exec, n, curArg);
            n++;
        }
        if (i == argCount)
            break;
        curArg = exec->uncheckedArgument(i);
    }
    arr->setLength(exec, n);
    return JSValue::encode(arr);
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncPop(ExecState* exec)
{
    JSValue thisValue = exec->thisValue().toThis(exec, StrictMode);

    if (isJSArray(thisValue))
        return JSValue::encode(asArray(thisValue)->pop(exec));

    JSObject* thisObj = thisValue.toObject(exec);
    unsigned length = getLength(exec, thisObj);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    JSValue result;
    if (length == 0) {
        putLength(exec, thisObj, jsNumber(length));
        result = jsUndefined();
    } else {
        result = thisObj->get(exec, length - 1);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
        if (!thisObj->methodTable(exec->vm())->deletePropertyByIndex(thisObj, exec, length - 1)) {
            throwTypeError(exec, ASCIILiteral("Unable to delete property."));
            return JSValue::encode(jsUndefined());
        }
        putLength(exec, thisObj, jsNumber(length - 1));
    }
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncPush(ExecState* exec)
{
    JSValue thisValue = exec->thisValue().toThis(exec, StrictMode);

    if (isJSArray(thisValue) && exec->argumentCount() == 1) {
        JSArray* array = asArray(thisValue);
        array->push(exec, exec->uncheckedArgument(0));
        return JSValue::encode(jsNumber(array->length()));
    }
    
    JSObject* thisObj = thisValue.toObject(exec);
    unsigned length = getLength(exec, thisObj);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    for (unsigned n = 0; n < exec->argumentCount(); n++) {
        // Check for integer overflow; where safe we can do a fast put by index.
        if (length + n >= length)
            thisObj->methodTable()->putByIndex(thisObj, exec, length + n, exec->uncheckedArgument(n), true);
        else {
            PutPropertySlot slot(thisObj);
            Identifier propertyName = Identifier::fromString(exec, JSValue(static_cast<int64_t>(length) + static_cast<int64_t>(n)).toWTFString(exec));
            thisObj->methodTable()->put(thisObj, exec, propertyName, exec->uncheckedArgument(n), slot);
        }
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    }
    
    JSValue newLength(static_cast<int64_t>(length) + static_cast<int64_t>(exec->argumentCount()));
    putLength(exec, thisObj, newLength);
    return JSValue::encode(newLength);
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncReverse(ExecState* exec)
{
    JSObject* thisObject = exec->thisValue().toThis(exec, StrictMode).toObject(exec);

    unsigned length = getLength(exec, thisObject);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    switch (thisObject->indexingType()) {
    case ALL_CONTIGUOUS_INDEXING_TYPES:
    case ALL_INT32_INDEXING_TYPES: {
        auto& butterfly = *thisObject->butterfly();
        if (length > butterfly.publicLength())
            break;
        auto data = butterfly.contiguous().data();
        if (containsHole(data, length) && holesMustForwardToPrototype(*exec, thisObject))
            break;
        std::reverse(data, data + length);
        return JSValue::encode(thisObject);
    }
    case ALL_DOUBLE_INDEXING_TYPES: {
        auto& butterfly = *thisObject->butterfly();
        if (length > butterfly.publicLength())
            break;
        auto data = butterfly.contiguousDouble().data();
        if (containsHole(data, length) && holesMustForwardToPrototype(*exec, thisObject))
            break;
        std::reverse(data, data + length);
        return JSValue::encode(thisObject);
    }
    case ALL_ARRAY_STORAGE_INDEXING_TYPES: {
        auto& storage = *thisObject->butterfly()->arrayStorage();
        if (length > storage.vectorLength())
            break;
        if (storage.hasHoles() && holesMustForwardToPrototype(*exec, thisObject))
            break;
        auto data = storage.vector().data();
        std::reverse(data, data + length);
        return JSValue::encode(thisObject);
    }
    }

    unsigned middle = length / 2;
    for (unsigned k = 0; k < middle; k++) {
        unsigned lk1 = length - k - 1;
        JSValue obj2 = getProperty(exec, thisObject, lk1);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
        JSValue obj = getProperty(exec, thisObject, k);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());

        if (obj2) {
            thisObject->putByIndexInline(exec, k, obj2, true);
            if (exec->hadException())
                return JSValue::encode(jsUndefined());
        } else if (!thisObject->methodTable(exec->vm())->deletePropertyByIndex(thisObject, exec, k)) {
            throwTypeError(exec, ASCIILiteral("Unable to delete property."));
            return JSValue::encode(jsUndefined());
        }

        if (obj) {
            thisObject->putByIndexInline(exec, lk1, obj, true);
            if (exec->hadException())
                return JSValue::encode(jsUndefined());
        } else if (!thisObject->methodTable(exec->vm())->deletePropertyByIndex(thisObject, exec, lk1)) {
            throwTypeError(exec, ASCIILiteral("Unable to delete property."));
            return JSValue::encode(jsUndefined());
        }
    }
    return JSValue::encode(thisObject);
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncShift(ExecState* exec)
{
    JSObject* thisObj = exec->thisValue().toThis(exec, StrictMode).toObject(exec);
    unsigned length = getLength(exec, thisObj);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    JSValue result;
    if (length == 0) {
        putLength(exec, thisObj, jsNumber(length));
        result = jsUndefined();
    } else {
        result = thisObj->getIndex(exec, 0);
        shift<JSArray::ShiftCountForShift>(exec, thisObj, 0, 1, 0, length);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
        putLength(exec, thisObj, jsNumber(length - 1));
    }
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncSlice(ExecState* exec)
{
    // http://developer.netscape.com/docs/manuals/js/client/jsref/array.htm#1193713 or 15.4.4.10
    JSObject* thisObj = exec->thisValue().toThis(exec, StrictMode).toObject(exec);
    unsigned length = getLength(exec, thisObj);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    unsigned begin = argumentClampedIndexFromStartOrEnd(exec, 0, length);
    unsigned end = argumentClampedIndexFromStartOrEnd(exec, 1, length, length);

    if (isJSArray(thisObj)) {
        if (JSArray* result = asArray(thisObj)->fastSlice(*exec, begin, end - begin))
            return JSValue::encode(result);
    }

    JSArray* result = constructEmptyArray(exec, nullptr, end - begin);

    unsigned n = 0;
    for (unsigned k = begin; k < end; k++, n++) {
        JSValue v = getProperty(exec, thisObj, k);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
        if (v)
            result->putDirectIndex(exec, n, v);
    }
    result->setLength(exec, n);
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncSplice(ExecState* exec)
{
    // 15.4.4.12

    VM& vm = exec->vm();

    JSObject* thisObj = exec->thisValue().toThis(exec, StrictMode).toObject(exec);
    unsigned length = getLength(exec, thisObj);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    
    if (!exec->argumentCount())
        return JSValue::encode(constructEmptyArray(exec, nullptr));

    unsigned begin = argumentClampedIndexFromStartOrEnd(exec, 0, length);

    unsigned deleteCount = length - begin;
    if (exec->argumentCount() > 1) {
        double deleteDouble = exec->uncheckedArgument(1).toInteger(exec);
        if (deleteDouble < 0)
            deleteCount = 0;
        else if (deleteDouble > length - begin)
            deleteCount = length - begin;
        else
            deleteCount = static_cast<unsigned>(deleteDouble);
    }

    JSArray* result = nullptr;

    if (isJSArray(thisObj))
        result = asArray(thisObj)->fastSlice(*exec, begin, deleteCount);

    if (!result) {
        result = JSArray::tryCreateUninitialized(vm, exec->lexicalGlobalObject()->arrayStructureForIndexingTypeDuringAllocation(ArrayWithUndecided), deleteCount);
        if (!result)
            return JSValue::encode(throwOutOfMemoryError(exec));

        for (unsigned k = 0; k < deleteCount; ++k) {
            JSValue v = getProperty(exec, thisObj, k + begin);
            if (exec->hadException())
                return JSValue::encode(jsUndefined());
            result->initializeIndex(vm, k, v);
        }
    }

    unsigned additionalArgs = std::max<int>(exec->argumentCount() - 2, 0);
    if (additionalArgs < deleteCount) {
        shift<JSArray::ShiftCountForSplice>(exec, thisObj, begin, deleteCount, additionalArgs, length);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    } else if (additionalArgs > deleteCount) {
        unshift<JSArray::ShiftCountForSplice>(exec, thisObj, begin, deleteCount, additionalArgs, length);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    }
    for (unsigned k = 0; k < additionalArgs; ++k) {
        thisObj->putByIndexInline(exec, k + begin, exec->uncheckedArgument(k + 2), true);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    }

    putLength(exec, thisObj, jsNumber(length - deleteCount + additionalArgs));
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncUnShift(ExecState* exec)
{
    // 15.4.4.13

    JSObject* thisObj = exec->thisValue().toThis(exec, StrictMode).toObject(exec);
    unsigned length = getLength(exec, thisObj);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    unsigned nrArgs = exec->argumentCount();
    if (nrArgs) {
        unshift<JSArray::ShiftCountForShift>(exec, thisObj, 0, 0, nrArgs, length);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    }
    for (unsigned k = 0; k < nrArgs; ++k) {
        thisObj->putByIndexInline(exec, k, exec->uncheckedArgument(k), true);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    }
    JSValue result = jsNumber(length + nrArgs);
    putLength(exec, thisObj, result);
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncIndexOf(ExecState* exec)
{
    // 15.4.4.14
    JSObject* thisObj = exec->thisValue().toThis(exec, StrictMode).toObject(exec);
    unsigned length = getLength(exec, thisObj);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    unsigned index = argumentClampedIndexFromStartOrEnd(exec, 1, length);
    JSValue searchElement = exec->argument(0);
    for (; index < length; ++index) {
        JSValue e = getProperty(exec, thisObj, index);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
        if (!e)
            continue;
        if (JSValue::strictEqual(exec, searchElement, e))
            return JSValue::encode(jsNumber(index));
    }

    return JSValue::encode(jsNumber(-1));
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncLastIndexOf(ExecState* exec)
{
    // 15.4.4.15
    JSObject* thisObj = exec->thisValue().toThis(exec, StrictMode).toObject(exec);
    unsigned length = getLength(exec, thisObj);
    if (!length)
        return JSValue::encode(jsNumber(-1));

    unsigned index = length - 1;
    if (exec->argumentCount() >= 2) {
        JSValue fromValue = exec->uncheckedArgument(1);
        double fromDouble = fromValue.toInteger(exec);
        if (fromDouble < 0) {
            fromDouble += length;
            if (fromDouble < 0)
                return JSValue::encode(jsNumber(-1));
        }
        if (fromDouble < length)
            index = static_cast<unsigned>(fromDouble);
    }

    JSValue searchElement = exec->argument(0);
    do {
        RELEASE_ASSERT(index < length);
        JSValue e = getProperty(exec, thisObj, index);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
        if (!e)
            continue;
        if (JSValue::strictEqual(exec, searchElement, e))
            return JSValue::encode(jsNumber(index));
    } while (index--);

    return JSValue::encode(jsNumber(-1));
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncValues(ExecState* exec)
{
    JSObject* thisObj = exec->thisValue().toThis(exec, StrictMode).toObject(exec);
    return JSValue::encode(JSArrayIterator::create(exec, exec->callee()->globalObject()->arrayIteratorStructure(), ArrayIterateValue, thisObj));
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncEntries(ExecState* exec)
{
    JSObject* thisObj = exec->thisValue().toThis(exec, StrictMode).toObject(exec);
    return JSValue::encode(JSArrayIterator::create(exec, exec->callee()->globalObject()->arrayIteratorStructure(), ArrayIterateKeyValue, thisObj));
}
    
EncodedJSValue JSC_HOST_CALL arrayProtoFuncKeys(ExecState* exec)
{
    JSObject* thisObj = exec->thisValue().toThis(exec, StrictMode).toObject(exec);
    return JSValue::encode(JSArrayIterator::create(exec, exec->callee()->globalObject()->arrayIteratorStructure(), ArrayIterateKey, thisObj));
}

} // namespace JSC

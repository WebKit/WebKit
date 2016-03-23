/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007-2009, 2011, 2013, 2015-2016 Apple Inc. All rights reserved.
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

#include "AdaptiveInferredPropertyValueWatchpointBase.h"
#include "ArrayConstructor.h"
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
EncodedJSValue JSC_HOST_CALL arrayProtoFuncLastIndexOf(ExecState*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncKeys(ExecState*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncEntries(ExecState*);

// ------------------------------ ArrayPrototype ----------------------------

const ClassInfo ArrayPrototype::s_info = {"Array", &JSArray::s_info, nullptr, CREATE_METHOD_TABLE(ArrayPrototype)};

ArrayPrototype* ArrayPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    ArrayPrototype* prototype = new (NotNull, allocateCell<ArrayPrototype>(vm.heap)) ArrayPrototype(vm, structure);
    prototype->finishCreation(vm, globalObject);
    vm.heap.addFinalizer(prototype, destroy);
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
    
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->toString, arrayProtoFuncToString, DontEnum, 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->toLocaleString, arrayProtoFuncToLocaleString, DontEnum, 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("concat", arrayProtoFuncConcat, DontEnum, 1);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("fill", arrayPrototypeFillCodeGenerator, DontEnum);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->join, arrayProtoFuncJoin, DontEnum, 1);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION("pop", arrayProtoFuncPop, DontEnum, 0, ArrayPopIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().pushPublicName(), arrayProtoFuncPush, DontEnum, 1, ArrayPushIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().pushPrivateName(), arrayProtoFuncPush, DontEnum | DontDelete | ReadOnly, 1, ArrayPushIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("reverse", arrayProtoFuncReverse, DontEnum, 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().shiftPublicName(), arrayProtoFuncShift, DontEnum, 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().shiftPrivateName(), arrayProtoFuncShift, DontEnum | DontDelete | ReadOnly, 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->slice, arrayProtoFuncSlice, DontEnum, 2);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("sort", arrayPrototypeSortCodeGenerator, DontEnum);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("splice", arrayProtoFuncSplice, DontEnum, 2);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("unshift", arrayProtoFuncUnShift, DontEnum, 1);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("every", arrayPrototypeEveryCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("forEach", arrayPrototypeForEachCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("some", arrayPrototypeSomeCodeGenerator, DontEnum);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("indexOf", arrayProtoFuncIndexOf, DontEnum, 1);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("lastIndexOf", arrayProtoFuncLastIndexOf, DontEnum, 1);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("filter", arrayPrototypeFilterCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("reduce", arrayPrototypeReduceCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("reduceRight", arrayPrototypeReduceRightCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("map", arrayPrototypeMapCodeGenerator, DontEnum);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->entries, arrayProtoFuncEntries, DontEnum, 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->keys, arrayProtoFuncKeys, DontEnum, 0);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("find", arrayPrototypeFindCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("findIndex", arrayPrototypeFindIndexCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("includes", arrayPrototypeIncludesCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("copyWithin", arrayPrototypeCopyWithinCodeGenerator, DontEnum);
    
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

void ArrayPrototype::destroy(JSC::JSCell* cell)
{
    ArrayPrototype* thisObject = static_cast<ArrayPrototype*>(cell);
    thisObject->ArrayPrototype::~ArrayPrototype();
}

// ------------------------------ Array Functions ----------------------------

static ALWAYS_INLINE JSValue getProperty(ExecState* exec, JSObject* object, unsigned index)
{
    if (JSValue result = object->tryGetIndexQuickly(index))
        return result;
    // We want to perform get and has in the same operation.
    // We can only do so when this behavior is not observable. The
    // only time it is observable is when we encounter a ProxyObject
    // somewhere in the prototype chain.
    PropertySlot slot(object, PropertySlot::InternalMethodType::HasProperty);
    if (!object->getPropertySlot(exec, index, slot))
        return JSValue();
    if (UNLIKELY(slot.isTaintedByProxy()))
        return object->get(exec, index);
    return slot.getValue(exec, index);
}

static ALWAYS_INLINE void putLength(ExecState* exec, JSObject* obj, JSValue value)
{
    PutPropertySlot slot(obj);
    obj->methodTable()->put(obj, exec, exec->propertyNames().length, value, slot);
}

static ALWAYS_INLINE void setLength(ExecState* exec, JSObject* obj, unsigned value)
{
    if (isJSArray(obj))
        jsCast<JSArray*>(obj)->setLength(exec, value);
    putLength(exec, obj, jsNumber(value));
}

enum class SpeciesConstructResult {
    FastPath,
    Exception,
    CreatedObject
};

static ALWAYS_INLINE std::pair<SpeciesConstructResult, JSObject*> speciesConstructArray(ExecState* exec, JSObject* thisObject, unsigned length)
{
    // ECMA 9.4.2.3: https://tc39.github.io/ecma262/#sec-arrayspeciescreate
    JSValue constructor = jsUndefined();
    if (LIKELY(isArray(exec, thisObject))) {
        // Fast path in the normal case where the user has not set an own constructor and the Array.prototype.constructor is normal.
        // We need prototype check for subclasses of Array, which are Array objects but have a different prototype by default.
        if (LIKELY(!thisObject->hasCustomProperties()
            && thisObject->globalObject()->arrayPrototype() == thisObject->getPrototypeDirect()
            && !thisObject->globalObject()->arrayPrototype()->didChangeConstructorOrSpeciesProperties()))
            return std::make_pair(SpeciesConstructResult::FastPath, nullptr);

        constructor = thisObject->get(exec, exec->propertyNames().constructor);
        if (exec->hadException())
            return std::make_pair(SpeciesConstructResult::Exception, nullptr);
        if (constructor.isConstructor()) {
            JSObject* constructorObject = jsCast<JSObject*>(constructor);
            if (exec->lexicalGlobalObject() != constructorObject->globalObject())
                return std::make_pair(SpeciesConstructResult::FastPath, nullptr);;
        }
        if (constructor.isObject()) {
            constructor = constructor.get(exec, exec->propertyNames().speciesSymbol);
            if (exec->hadException())
                return std::make_pair(SpeciesConstructResult::Exception, nullptr);
            if (constructor.isNull())
                return std::make_pair(SpeciesConstructResult::FastPath, nullptr);;
        }
    } else if (exec->hadException())
        return std::make_pair(SpeciesConstructResult::Exception, nullptr);

    if (constructor.isUndefined())
        return std::make_pair(SpeciesConstructResult::FastPath, nullptr);

    MarkedArgumentBuffer args;
    args.append(jsNumber(length));
    JSObject* newObject = construct(exec, constructor, args, "Species construction did not get a valid constructor");
    if (exec->hadException())
        return std::make_pair(SpeciesConstructResult::Exception, nullptr);
    return std::make_pair(SpeciesConstructResult::CreatedObject, newObject);
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
    VM& vm = exec->vm();
    if (UNLIKELY(vm.exception()))
        return JSValue::encode(jsUndefined());
    
    // 2. Let func be the result of calling the [[Get]] internal method of array with argument "join".
    JSValue function = JSValue(thisObject).get(exec, exec->propertyNames().join);

    // 3. If IsCallable(func) is false, then let func be the standard built-in method Object.prototype.toString (15.2.4.2).
    bool customJoinCase = false;
    if (!function.isCell())
        customJoinCase = true;
    CallData callData;
    CallType callType = getCallData(function, callData);
    if (callType == CallType::None)
        customJoinCase = true;

    if (UNLIKELY(customJoinCase))
        return JSValue::encode(jsMakeNontrivialString(exec, "[object ", thisObject->methodTable(exec->vm())->className(thisObject), "]"));

    // 4. Return the result of calling the [[Call]] internal method of func providing array as the this value and an empty arguments list.
    if (!isJSArray(thisObject) || callType != CallType::Host || callData.native.function != arrayProtoFuncJoin)
        return JSValue::encode(call(exec, function, callType, callData, thisObject, exec->emptyList()));

    ASSERT(isJSArray(thisValue));
    JSArray* thisArray = asArray(thisValue);

    unsigned length = thisArray->length();

    StringRecursionChecker checker(exec, thisArray);
    if (JSValue earlyReturnValue = checker.earlyReturnValue())
        return JSValue::encode(earlyReturnValue);

    JSStringJoiner joiner(*exec, ',', length);
    if (UNLIKELY(vm.exception()))
        return JSValue::encode(jsUndefined());

    for (unsigned i = 0; i < length; ++i) {
        JSValue element = thisArray->tryGetIndexQuickly(i);
        if (!element) {
            element = thisArray->get(exec, i);
            if (UNLIKELY(vm.exception()))
                return JSValue::encode(jsUndefined());
        }
        joiner.append(*exec, element);
        if (UNLIKELY(vm.exception()))
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

#if ENABLE(INTL)
    ArgList arguments(exec);
    for (unsigned i = 0; i < length; ++i) {
        JSValue element = thisObject->getIndex(exec, i);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
        if (element.isUndefinedOrNull())
            element = jsEmptyString(exec);
        else {
            JSValue conversionFunction = element.get(exec, exec->propertyNames().toLocaleString);
            if (exec->hadException())
                return JSValue::encode(jsUndefined());
            CallData callData;
            CallType callType = getCallData(conversionFunction, callData);
            if (callType != CallType::None) {
                element = call(exec, conversionFunction, callType, callData, element, arguments);
                if (exec->hadException())
                return JSValue::encode(jsUndefined());
            }
        }
        stringJoiner.append(*exec, element);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    }
#else // !ENABLE(INTL)
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
        if (callType != CallType::None) {
            element = call(exec, conversionFunction, callType, callData, element, exec->emptyList());
            if (exec->hadException())
                return JSValue::encode(jsUndefined());
        }
        stringJoiner.append(*exec, element);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    }
#endif // !ENABLE(INTL)

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
                if (!joiner.appendWithoutSideEffects(state, value))
                    goto generalCase;
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
                if (!joiner.appendWithoutSideEffects(state, value))
                    goto generalCase;
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
    if (!thisObject)
        return JSValue::encode(JSValue());

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
    return JSValue::encode(join(*exec, thisObject, separator->view(exec).get()));
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncConcat(ExecState* exec)
{
    JSValue thisValue = exec->thisValue().toThis(exec, StrictMode);
    unsigned argCount = exec->argumentCount();
    JSValue curArg = thisValue.toObject(exec);
    if (!curArg)
        return JSValue::encode(JSValue());
    Checked<unsigned, RecordOverflow> finalArraySize = 0;

    // We need to do species construction before geting the rest of the elements.
    std::pair<SpeciesConstructResult, JSObject*> speciesResult = speciesConstructArray(exec, curArg.getObject(), 0);
    if (speciesResult.first == SpeciesConstructResult::Exception)
        return JSValue::encode(jsUndefined());

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

    if (speciesResult.first == SpeciesConstructResult::FastPath && argCount == 1 && previousArray && currentArray && finalArraySize.unsafeGet() < MIN_SPARSE_ARRAY_INDEX) {
        IndexingType type = JSArray::fastConcatType(exec->vm(), *previousArray, *currentArray);
        if (type != NonArray)
            return previousArray->fastConcatWith(*exec, *currentArray);
    }

    ASSERT(speciesResult.first != SpeciesConstructResult::Exception);

    JSObject* result;
    if (speciesResult.first == SpeciesConstructResult::CreatedObject)
        result = speciesResult.second;
    else {
        // We add the newTarget because the compiler gets confused between 0 being a number and a pointer.
        result = constructEmptyArray(exec, nullptr, 0, JSValue());
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    }

    curArg = thisValue.toObject(exec);
    ASSERT(!exec->hadException());
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
                    result->putDirectIndex(exec, n, v);
                n++;
            }
        } else {
            result->putDirectIndex(exec, n, curArg);
            n++;
        }
        if (i == argCount)
            break;
        curArg = exec->uncheckedArgument(i);
    }
    setLength(exec, result, n);
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncPop(ExecState* exec)
{
    JSValue thisValue = exec->thisValue().toThis(exec, StrictMode);

    if (isJSArray(thisValue))
        return JSValue::encode(asArray(thisValue)->pop(exec));

    JSObject* thisObj = thisValue.toObject(exec);
    if (!thisObj)
        return JSValue::encode(JSValue());
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
    if (!thisObj)
        return JSValue::encode(JSValue());
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
    if (!thisObject)
        return JSValue::encode(JSValue());

    VM& vm = exec->vm();
    unsigned length = getLength(exec, thisObject);
    if (vm.exception())
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
    for (unsigned lower = 0; lower < middle; lower++) {
        unsigned upper = length - lower - 1;
        bool lowerExists = thisObject->hasProperty(exec, lower);
        if (vm.exception())
            return JSValue::encode(jsUndefined());
        JSValue lowerValue;
        if (lowerExists)
            lowerValue = thisObject->get(exec, lower);

        bool upperExists = thisObject->hasProperty(exec, upper);
        if (vm.exception())
            return JSValue::encode(jsUndefined());
        JSValue upperValue;
        if (upperExists)
            upperValue = thisObject->get(exec, upper);

        if (upperExists) {
            thisObject->putByIndexInline(exec, lower, upperValue, true);
            if (vm.exception())
                return JSValue::encode(JSValue());
        } else if (!thisObject->methodTable(vm)->deletePropertyByIndex(thisObject, exec, lower)) {
            if (!vm.exception())
                throwTypeError(exec, ASCIILiteral("Unable to delete property."));
            return JSValue::encode(JSValue());
        }

        if (lowerExists) {
            thisObject->putByIndexInline(exec, upper, lowerValue, true);
            if (vm.exception())
                return JSValue::encode(JSValue());
        } else if (!thisObject->methodTable(vm)->deletePropertyByIndex(thisObject, exec, upper)) {
            if (!vm.exception())
                throwTypeError(exec, ASCIILiteral("Unable to delete property."));
            return JSValue::encode(JSValue());
        }
    }
    return JSValue::encode(thisObject);
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncShift(ExecState* exec)
{
    JSObject* thisObj = exec->thisValue().toThis(exec, StrictMode).toObject(exec);
    if (!thisObj)
        return JSValue::encode(JSValue());
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
    if (!thisObj)
        return JSValue::encode(JSValue());
    unsigned length = getLength(exec, thisObj);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    unsigned begin = argumentClampedIndexFromStartOrEnd(exec, 0, length);
    unsigned end = argumentClampedIndexFromStartOrEnd(exec, 1, length, length);

    std::pair<SpeciesConstructResult, JSObject*> speciesResult = speciesConstructArray(exec, thisObj, end - begin);
    // We can only get an exception if we call some user function.
    if (UNLIKELY(speciesResult.first == SpeciesConstructResult::Exception))
        return JSValue::encode(jsUndefined());

    if (LIKELY(speciesResult.first == SpeciesConstructResult::FastPath && isJSArray(thisObj))) {
        if (JSArray* result = asArray(thisObj)->fastSlice(*exec, begin, end - begin))
            return JSValue::encode(result);
    }

    JSObject* result;
    if (speciesResult.first == SpeciesConstructResult::CreatedObject)
        result = speciesResult.second;
    else
        result = constructEmptyArray(exec, nullptr, end - begin);

    unsigned n = 0;
    for (unsigned k = begin; k < end; k++, n++) {
        JSValue v = getProperty(exec, thisObj, k);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
        if (v)
            result->putDirectIndex(exec, n, v);
    }
    setLength(exec, result, n);
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncSplice(ExecState* exec)
{
    // 15.4.4.12

    VM& vm = exec->vm();

    JSObject* thisObj = exec->thisValue().toThis(exec, StrictMode).toObject(exec);
    if (!thisObj)
        return JSValue::encode(JSValue());
    unsigned length = getLength(exec, thisObj);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    if (!exec->argumentCount()) {
        std::pair<SpeciesConstructResult, JSObject*> speciesResult = speciesConstructArray(exec, thisObj, 0);
        if (speciesResult.first == SpeciesConstructResult::Exception)
            return JSValue::encode(jsUndefined());

        JSObject* result;
        if (speciesResult.first == SpeciesConstructResult::CreatedObject)
            result = speciesResult.second;
        else
            result = constructEmptyArray(exec, nullptr);

        setLength(exec, result, 0);
        return JSValue::encode(result);
    }

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

    std::pair<SpeciesConstructResult, JSObject*> speciesResult = speciesConstructArray(exec, thisObj, deleteCount);
    if (speciesResult.first == SpeciesConstructResult::Exception)
        return JSValue::encode(jsUndefined());

    JSObject* result = nullptr;
    if (speciesResult.first == SpeciesConstructResult::FastPath && isJSArray(thisObj))
        result = asArray(thisObj)->fastSlice(*exec, begin, deleteCount);

    if (!result) {
        if (speciesResult.first == SpeciesConstructResult::CreatedObject) {
            result = speciesResult.second;
            
            for (unsigned k = 0; k < deleteCount; ++k) {
                JSValue v = getProperty(exec, thisObj, k + begin);
                if (exec->hadException())
                    return JSValue::encode(jsUndefined());
                result->putByIndexInline(exec, k, v, true);
                if (exec->hadException())
                    return JSValue::encode(jsUndefined());
            }
        } else {
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

    setLength(exec, thisObj, length - deleteCount + additionalArgs);
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncUnShift(ExecState* exec)
{
    // 15.4.4.13

    JSObject* thisObj = exec->thisValue().toThis(exec, StrictMode).toObject(exec);
    if (!thisObj)
        return JSValue::encode(JSValue());
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
    if (!thisObj)
        return JSValue::encode(JSValue());
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
    if (!thisObj)
        return JSValue::encode(JSValue());
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
    if (!thisObj)
        return JSValue::encode(JSValue());
    return JSValue::encode(JSArrayIterator::create(exec, exec->callee()->globalObject()->arrayIteratorStructure(), ArrayIterateValue, thisObj));
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncEntries(ExecState* exec)
{
    JSObject* thisObj = exec->thisValue().toThis(exec, StrictMode).toObject(exec);
    if (!thisObj)
        return JSValue::encode(JSValue());
    return JSValue::encode(JSArrayIterator::create(exec, exec->callee()->globalObject()->arrayIteratorStructure(), ArrayIterateKeyValue, thisObj));
}
    
EncodedJSValue JSC_HOST_CALL arrayProtoFuncKeys(ExecState* exec)
{
    JSObject* thisObj = exec->thisValue().toThis(exec, StrictMode).toObject(exec);
    if (!thisObj)
        return JSValue::encode(JSValue());
    return JSValue::encode(JSArrayIterator::create(exec, exec->callee()->globalObject()->arrayIteratorStructure(), ArrayIterateKey, thisObj));
}

// -------------------- ArrayPrototype.constructor Watchpoint ------------------

class ArrayPrototypeAdaptiveInferredPropertyWatchpoint : public AdaptiveInferredPropertyValueWatchpointBase {
public:
    typedef AdaptiveInferredPropertyValueWatchpointBase Base;
    ArrayPrototypeAdaptiveInferredPropertyWatchpoint(const ObjectPropertyCondition&, ArrayPrototype*);

private:
    void handleFire(const FireDetail&) override;

    ArrayPrototype* m_arrayPrototype;
};

void ArrayPrototype::setConstructor(VM& vm, JSObject* constructorProperty, unsigned attributes)
{
    putDirectWithoutTransition(vm, vm.propertyNames->constructor, constructorProperty, attributes);

    // Do the watchpoint on our constructor property
    PropertyOffset offset = this->structure()->get(vm, vm.propertyNames->constructor);
    ASSERT(isValidOffset(offset));
    this->structure()->startWatchingPropertyForReplacements(vm, offset);

    ObjectPropertyCondition condition = ObjectPropertyCondition::equivalence(vm, this, this, vm.propertyNames->constructor.impl(), constructorProperty);
    ASSERT(condition.isWatchable());

    m_constructorWatchpoint = std::make_unique<ArrayPrototypeAdaptiveInferredPropertyWatchpoint>(condition, this);
    m_constructorWatchpoint->install();
    
    // Do the watchpoint on the constructor's Symbol.species property
    offset = constructorProperty->structure()->get(vm, vm.propertyNames->speciesSymbol);
    ASSERT(isValidOffset(offset));
    constructorProperty->structure()->startWatchingPropertyForReplacements(vm, offset);

    ASSERT(constructorProperty->getDirect(offset).isGetterSetter());
    condition = ObjectPropertyCondition::equivalence(vm, this, constructorProperty, vm.propertyNames->speciesSymbol.impl(), constructorProperty->getDirect(offset));
    ASSERT(condition.isWatchable());

    m_constructorSpeciesWatchpoint = std::make_unique<ArrayPrototypeAdaptiveInferredPropertyWatchpoint>(condition, this);
    m_constructorSpeciesWatchpoint->install();
}

ArrayPrototypeAdaptiveInferredPropertyWatchpoint::ArrayPrototypeAdaptiveInferredPropertyWatchpoint(const ObjectPropertyCondition& key, ArrayPrototype* prototype)
    : Base(key)
    , m_arrayPrototype(prototype)
{
}

void ArrayPrototypeAdaptiveInferredPropertyWatchpoint::handleFire(const FireDetail& detail)
{
    StringPrintStream out;
    out.print("ArrayPrototype adaption of ", key(), " failed: ", detail);

    StringFireDetail stringDetail(out.toCString().data());

    m_arrayPrototype->m_didChangeConstructorOrSpeciesProperties = true;
}

} // namespace JSC

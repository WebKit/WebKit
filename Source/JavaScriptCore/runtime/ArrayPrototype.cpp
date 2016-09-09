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
#include "CodeBlock.h"
#include "CopiedSpaceInlines.h"
#include "Error.h"
#include "GetterSetter.h"
#include "Interpreter.h"
#include "JIT.h"
#include "JSArrayInlines.h"
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

namespace JSC {

EncodedJSValue JSC_HOST_CALL arrayProtoFuncToLocaleString(ExecState*);
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

    putDirectWithoutTransition(vm, vm.propertyNames->toString, globalObject->arrayProtoToStringFunction(), DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().valuesPublicName(), globalObject->arrayProtoValuesFunction(), DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->iteratorSymbol, globalObject->arrayProtoValuesFunction(), DontEnum);

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->toLocaleString, arrayProtoFuncToLocaleString, DontEnum, 0);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("concat", arrayPrototypeConcatCodeGenerator, DontEnum);
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
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().entriesPublicName(), arrayPrototypeEntriesCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().keysPublicName(), arrayPrototypeKeysCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("find", arrayPrototypeFindCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("findIndex", arrayPrototypeFindIndexCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("includes", arrayPrototypeIncludesCodeGenerator, DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION("copyWithin", arrayPrototypeCopyWithinCodeGenerator, DontEnum);

    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().entriesPrivateName(), getDirect(vm, vm.propertyNames->builtinNames().entriesPublicName()), ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().forEachPrivateName(), getDirect(vm, vm.propertyNames->builtinNames().forEachPublicName()), ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().keysPrivateName(), getDirect(vm, vm.propertyNames->builtinNames().keysPublicName()), ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().valuesPrivateName(), globalObject->arrayProtoValuesFunction(), ReadOnly);

    JSObject* unscopables = constructEmptyObject(globalObject->globalExec(), globalObject->nullPrototypeObjectStructure());
    const char* unscopableNames[] = {
        "copyWithin",
        "entries",
        "fill",
        "find",
        "findIndex",
        "includes",
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
    // only time it is observable is when we encounter an opaque objects (ProxyObject and JSModuleNamespaceObject)
    // somewhere in the prototype chain.
    PropertySlot slot(object, PropertySlot::InternalMethodType::HasProperty);
    if (!object->getPropertySlot(exec, index, slot))
        return JSValue();
    if (UNLIKELY(slot.isTaintedByOpaqueObject()))
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

inline bool speciesWatchpointsValid(ExecState* exec, JSObject* thisObject)
{
    ArrayPrototype* arrayPrototype = thisObject->globalObject()->arrayPrototype();
    ArrayPrototype::SpeciesWatchpointStatus status = arrayPrototype->speciesWatchpointStatus();
    if (UNLIKELY(status == ArrayPrototype::SpeciesWatchpointStatus::Uninitialized))
        status = arrayPrototype->attemptToInitializeSpeciesWatchpoint(exec);
    ASSERT(status != ArrayPrototype::SpeciesWatchpointStatus::Uninitialized);
    return !thisObject->hasCustomProperties()
        && arrayPrototype == thisObject->getPrototypeDirect()
        && status == ArrayPrototype::SpeciesWatchpointStatus::Initialized;
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
        if (LIKELY(speciesWatchpointsValid(exec, thisObject)))
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
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

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
        } else if (!thisObj->methodTable(vm)->deletePropertyByIndex(thisObj, exec, to)) {
            throwTypeError(exec, scope, ASCIILiteral("Unable to delete property."));
            return;
        }
    }
    for (unsigned k = length; k > length - count; --k) {
        if (!thisObj->methodTable(vm)->deletePropertyByIndex(thisObj, exec, k - 1)) {
            throwTypeError(exec, scope, ASCIILiteral("Unable to delete property."));
            return;
        }
    }
}

template<JSArray::ShiftCountMode shiftCountMode>
void unshift(ExecState* exec, JSObject* thisObj, unsigned header, unsigned currentCount, unsigned resultCount, unsigned length)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RELEASE_ASSERT(resultCount > currentCount);
    unsigned count = resultCount - currentCount;

    RELEASE_ASSERT(header <= length);
    RELEASE_ASSERT(currentCount <= (length - header));

    // Guard against overflow.
    if (count > (UINT_MAX - length)) {
        throwOutOfMemoryError(exec, scope);
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
        } else if (!thisObj->methodTable(vm)->deletePropertyByIndex(thisObj, exec, to)) {
            throwTypeError(exec, scope, ASCIILiteral("Unable to delete property."));
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
    if (UNLIKELY(vm.exception()))
        return JSValue::encode(jsUndefined());

    // 3. If IsCallable(func) is false, then let func be the standard built-in method Object.prototype.toString (15.2.4.2).
    bool customJoinCase = false;
    if (!function.isCell())
        customJoinCase = true;
    CallData callData;
    CallType callType = getCallData(function, callData);
    if (callType == CallType::None)
        customJoinCase = true;

    if (UNLIKELY(customJoinCase))
        return JSValue::encode(jsMakeNontrivialString(exec, "[object ", thisObject->methodTable(vm)->className(thisObject), "]"));

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

static JSValue slowJoin(ExecState& exec, JSObject* thisObject, JSString* separator, uint64_t length)
{
    // 5. If len is zero, return the empty String.
    if (!length)
        return jsEmptyString(&exec);

    VM& vm = exec.vm();

    // 6. Let element0 be Get(O, "0").
    JSValue element0 = thisObject->getIndex(&exec, 0);
    if (vm.exception())
        return JSValue();

    // 7. If element0 is undefined or null, let R be the empty String; otherwise, let R be ? ToString(element0).
    JSString* r = nullptr;
    if (element0.isUndefinedOrNull())
        r = jsEmptyString(&exec);
    else
        r = element0.toString(&exec);
    if (vm.exception())
        return JSValue();

    // 8. Let k be 1.
    // 9. Repeat, while k < len
    // 9.e Increase k by 1..
    for (uint64_t k = 1; k < length; ++k) {
        // b. Let element be ? Get(O, ! ToString(k)).
        JSValue element = thisObject->get(&exec, Identifier::fromString(&exec, AtomicString::number(k)));
        if (vm.exception())
            return JSValue();

        // c. If element is undefined or null, let next be the empty String; otherwise, let next be ? ToString(element).
        JSString* next = nullptr;
        if (element.isUndefinedOrNull()) {
            if (!separator->length())
                continue;
            next = jsEmptyString(&exec);
        } else
            next = element.toString(&exec);
        if (vm.exception())
            return JSValue();

        // a. Let S be the String value produced by concatenating R and sep.
        // d. Let R be a String value produced by concatenating S and next.
        r = JSRopeString::create(vm, r, separator, next);
    }
    // 10. Return R.
    return r;
}

static inline bool canUseFastJoin(const JSObject* thisObject)
{
    switch (thisObject->indexingType()) {
    case ALL_CONTIGUOUS_INDEXING_TYPES:
    case ALL_INT32_INDEXING_TYPES:
    case ALL_DOUBLE_INDEXING_TYPES:
        return true;
    default:
        break;
    }
    return false;
}

static inline JSValue fastJoin(ExecState& state, JSObject* thisObject, StringView separator, unsigned length)
{
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
    // 1. Let O be ? ToObject(this value).
    JSObject* thisObject = exec->thisValue().toThis(exec, StrictMode).toObject(exec);
    if (!thisObject)
        return JSValue::encode(JSValue());

    StringRecursionChecker checker(exec, thisObject);
    if (JSValue earlyReturnValue = checker.earlyReturnValue())
        return JSValue::encode(earlyReturnValue);

    // 2. Let len be ? ToLength(? Get(O, "length")).
    double length = toLength(exec, thisObject);
    if (exec->hadException())
        return JSValue::encode(JSValue());

    // 3. If separator is undefined, let separator be the single-element String ",".
    JSValue separatorValue = exec->argument(0);
    if (separatorValue.isUndefined()) {
        const LChar comma = ',';

        if (UNLIKELY(length > std::numeric_limits<unsigned>::max() || !canUseFastJoin(thisObject))) {
            uint64_t length64 = static_cast<uint64_t>(length);
            ASSERT(static_cast<double>(length64) == length);
            JSString* jsSeparator = jsSingleCharacterString(exec, comma);
            if (exec->hadException())
                return JSValue::encode(JSValue());

            return JSValue::encode(slowJoin(*exec, thisObject, jsSeparator, length64));
        }

        unsigned unsignedLength = static_cast<unsigned>(length);
        ASSERT(static_cast<double>(unsignedLength) == length);
        return JSValue::encode(fastJoin(*exec, thisObject, { &comma, 1 }, unsignedLength));
    }

    // 4. Let sep be ? ToString(separator).
    JSString* jsSeparator = separatorValue.toString(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    if (UNLIKELY(length > std::numeric_limits<unsigned>::max() || !canUseFastJoin(thisObject))) {
        uint64_t length64 = static_cast<uint64_t>(length);
        ASSERT(static_cast<double>(length64) == length);
        return JSValue::encode(slowJoin(*exec, thisObject, jsSeparator, length64));
    }

    return JSValue::encode(fastJoin(*exec, thisObject, jsSeparator->view(exec).get(), length));
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncPop(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

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
        if (!thisObj->methodTable(vm)->deletePropertyByIndex(thisObj, exec, length - 1)) {
            throwTypeError(exec, scope, ASCIILiteral("Unable to delete property."));
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
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* thisObject = exec->thisValue().toThis(exec, StrictMode).toObject(exec);
    if (!thisObject)
        return JSValue::encode(JSValue());

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
        if (lowerExists) {
            lowerValue = thisObject->get(exec, lower);
            if (UNLIKELY(vm.exception()))
                return JSValue::encode(jsUndefined());
        }

        bool upperExists = thisObject->hasProperty(exec, upper);
        if (UNLIKELY(vm.exception()))
            return JSValue::encode(jsUndefined());
        JSValue upperValue;
        if (upperExists) {
            upperValue = thisObject->get(exec, upper);
            if (UNLIKELY(vm.exception()))
                return JSValue::encode(jsUndefined());
        }

        if (upperExists) {
            thisObject->putByIndexInline(exec, lower, upperValue, true);
            if (vm.exception())
                return JSValue::encode(JSValue());
        } else if (!thisObject->methodTable(vm)->deletePropertyByIndex(thisObject, exec, lower)) {
            if (!vm.exception())
                throwTypeError(exec, scope, ASCIILiteral("Unable to delete property."));
            return JSValue::encode(JSValue());
        }

        if (lowerExists) {
            thisObject->putByIndexInline(exec, upper, lowerValue, true);
            if (vm.exception())
                return JSValue::encode(JSValue());
        } else if (!thisObject->methodTable(vm)->deletePropertyByIndex(thisObject, exec, upper)) {
            if (!vm.exception())
                throwTypeError(exec, scope, ASCIILiteral("Unable to delete property."));
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
    VM& vm = exec->vm();
    JSObject* thisObj = exec->thisValue().toThis(exec, StrictMode).toObject(exec);
    if (!thisObj)
        return JSValue::encode(JSValue());
    unsigned length = getLength(exec, thisObj);
    if (UNLIKELY(vm.exception()))
        return JSValue::encode(jsUndefined());

    unsigned begin = argumentClampedIndexFromStartOrEnd(exec, 0, length);
    unsigned end = argumentClampedIndexFromStartOrEnd(exec, 1, length, length);

    std::pair<SpeciesConstructResult, JSObject*> speciesResult = speciesConstructArray(exec, thisObj, end - begin);
    // We can only get an exception if we call some user function.
    if (UNLIKELY(speciesResult.first == SpeciesConstructResult::Exception))
        return JSValue::encode(jsUndefined());

    if (LIKELY(speciesResult.first == SpeciesConstructResult::FastPath && isJSArray(thisObj) && length == getLength(exec, thisObj))) {
        if (JSArray* result = asArray(thisObj)->fastSlice(*exec, begin, end - begin))
            return JSValue::encode(result);
    }

    JSObject* result;
    if (speciesResult.first == SpeciesConstructResult::CreatedObject)
        result = speciesResult.second;
    else {
        result = constructEmptyArray(exec, nullptr, end - begin);
        if (UNLIKELY(vm.exception()))
            return JSValue::encode(jsUndefined());
    }

    unsigned n = 0;
    for (unsigned k = begin; k < end; k++, n++) {
        JSValue v = getProperty(exec, thisObj, k);
        if (UNLIKELY(vm.exception()))
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
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* thisObj = exec->thisValue().toThis(exec, StrictMode).toObject(exec);
    if (!thisObj)
        return JSValue::encode(JSValue());
    unsigned length = getLength(exec, thisObj);
    if (UNLIKELY(vm.exception()))
        return JSValue::encode(jsUndefined());

    if (!exec->argumentCount()) {
        std::pair<SpeciesConstructResult, JSObject*> speciesResult = speciesConstructArray(exec, thisObj, 0);
        if (speciesResult.first == SpeciesConstructResult::Exception)
            return JSValue::encode(jsUndefined());

        JSObject* result;
        if (speciesResult.first == SpeciesConstructResult::CreatedObject)
            result = speciesResult.second;
        else {
            result = constructEmptyArray(exec, nullptr);
            if (UNLIKELY(vm.exception()))
                return JSValue::encode(jsUndefined());
        }

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
    if (speciesResult.first == SpeciesConstructResult::FastPath && isJSArray(thisObj) && length == getLength(exec, thisObj))
        result = asArray(thisObj)->fastSlice(*exec, begin, deleteCount);

    if (!result) {
        if (speciesResult.first == SpeciesConstructResult::CreatedObject) {
            result = speciesResult.second;
            
            for (unsigned k = 0; k < deleteCount; ++k) {
                JSValue v = getProperty(exec, thisObj, k + begin);
                if (UNLIKELY(vm.exception()))
                    return JSValue::encode(jsUndefined());
                if (UNLIKELY(!v))
                    continue;
                result->putByIndexInline(exec, k, v, true);
                if (UNLIKELY(vm.exception()))
                    return JSValue::encode(jsUndefined());
            }
        } else {
            result = JSArray::tryCreateUninitialized(vm, exec->lexicalGlobalObject()->arrayStructureForIndexingTypeDuringAllocation(ArrayWithUndecided), deleteCount);
            if (!result)
                return JSValue::encode(throwOutOfMemoryError(exec, scope));
            
            for (unsigned k = 0; k < deleteCount; ++k) {
                JSValue v = getProperty(exec, thisObj, k + begin);
                if (UNLIKELY(vm.exception()))
                    return JSValue::encode(jsUndefined());
                if (UNLIKELY(!v))
                    continue;
                result->initializeIndex(vm, k, v);
            }
        }
    }

    unsigned additionalArgs = std::max<int>(exec->argumentCount() - 2, 0);
    if (additionalArgs < deleteCount) {
        shift<JSArray::ShiftCountForSplice>(exec, thisObj, begin, deleteCount, additionalArgs, length);
        if (UNLIKELY(vm.exception()))
            return JSValue::encode(jsUndefined());
    } else if (additionalArgs > deleteCount) {
        unshift<JSArray::ShiftCountForSplice>(exec, thisObj, begin, deleteCount, additionalArgs, length);
        if (UNLIKELY(vm.exception()))
            return JSValue::encode(jsUndefined());
    }
    for (unsigned k = 0; k < additionalArgs; ++k) {
        thisObj->putByIndexInline(exec, k + begin, exec->uncheckedArgument(k + 2), true);
        if (UNLIKELY(vm.exception()))
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
    VM& vm = exec->vm();
    unsigned length = getLength(exec, thisObj);
    if (UNLIKELY(vm.exception()))
        return JSValue::encode(jsUndefined());

    unsigned index = argumentClampedIndexFromStartOrEnd(exec, 1, length);
    JSValue searchElement = exec->argument(0);
    for (; index < length; ++index) {
        JSValue e = getProperty(exec, thisObj, index);
        if (UNLIKELY(vm.exception()))
            return JSValue::encode(jsUndefined());
        if (!e)
            continue;
        if (JSValue::strictEqual(exec, searchElement, e))
            return JSValue::encode(jsNumber(index));
        if (UNLIKELY(vm.exception()))
            return JSValue::encode(jsUndefined());
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

    VM& vm = exec->vm();
    JSValue searchElement = exec->argument(0);
    do {
        RELEASE_ASSERT(index < length);
        JSValue e = getProperty(exec, thisObj, index);
        if (UNLIKELY(vm.exception()))
            return JSValue::encode(jsUndefined());
        if (!e)
            continue;
        if (JSValue::strictEqual(exec, searchElement, e))
            return JSValue::encode(jsNumber(index));
        if (UNLIKELY(vm.exception()))
            return JSValue::encode(jsUndefined());
    } while (index--);

    return JSValue::encode(jsNumber(-1));
}

static bool moveElements(ExecState* exec, VM& vm, JSArray* target, unsigned targetOffset, JSArray* source, unsigned sourceLength)
{
    if (LIKELY(!hasAnyArrayStorage(source->indexingType()) && !source->structure()->holesMustForwardToPrototype(vm))) {
        for (unsigned i = 0; i < sourceLength; ++i) {
            JSValue value = source->tryGetIndexQuickly(i);
            if (value) {
                target->putDirectIndex(exec, targetOffset + i, value);
                if (vm.exception())
                    return false;
            }
        }
    } else {
        for (unsigned i = 0; i < sourceLength; ++i) {
            JSValue value = getProperty(exec, source, i);
            if (vm.exception())
                return false;
            if (value) {
                target->putDirectIndex(exec, targetOffset + i, value);
                if (vm.exception())
                    return false;
            }
        }
    }
    return true;
}

static EncodedJSValue concatAppendOne(ExecState* exec, VM& vm, JSArray* first, JSValue second)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(!isJSArray(second));
    ASSERT(!shouldUseSlowPut(first->indexingType()));
    Butterfly* firstButterfly = first->butterfly();
    unsigned firstArraySize = firstButterfly->publicLength();

    IndexingType type = first->mergeIndexingTypeForCopying(indexingTypeForValue(second) | IsArray);
    
    if (type == NonArray)
        type = first->indexingType();

    Structure* resultStructure = exec->lexicalGlobalObject()->arrayStructureForIndexingTypeDuringAllocation(type);
    JSArray* result = JSArray::create(vm, resultStructure, firstArraySize + 1);
    if (!result)
        return JSValue::encode(throwOutOfMemoryError(exec, scope));

    if (!result->appendMemcpy(exec, vm, 0, first)) {
        if (!moveElements(exec, vm, result, 0, first, firstArraySize)) {
            ASSERT(vm.exception());
            return JSValue::encode(JSValue());
        }
    }

    result->putDirectIndex(exec, firstArraySize, second);
    return JSValue::encode(result);

}


EncodedJSValue JSC_HOST_CALL arrayProtoPrivateFuncConcatMemcpy(ExecState* exec)
{
    ASSERT(exec->argumentCount() == 2);
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSArray* firstArray = jsCast<JSArray*>(exec->uncheckedArgument(0));
    
    // This code assumes that neither array has set Symbol.isConcatSpreadable. If the first array
    // has indexed accessors then one of those accessors might change the value of Symbol.isConcatSpreadable
    // on the second argument.
    if (UNLIKELY(shouldUseSlowPut(firstArray->indexingType())))
        return JSValue::encode(jsNull());

    // We need to check the species constructor here since checking it in the JS wrapper is too expensive for the non-optimizing tiers.
    if (UNLIKELY(!speciesWatchpointsValid(exec, firstArray)))
        return JSValue::encode(jsNull());

    JSValue second = exec->uncheckedArgument(1);
    if (!isJSArray(second))
        return concatAppendOne(exec, vm, firstArray, second);

    JSArray* secondArray = jsCast<JSArray*>(second);
    
    Butterfly* firstButterfly = firstArray->butterfly();
    Butterfly* secondButterfly = secondArray->butterfly();

    unsigned firstArraySize = firstButterfly->publicLength();
    unsigned secondArraySize = secondButterfly->publicLength();

    IndexingType secondType = secondArray->indexingType();
    IndexingType type = firstArray->mergeIndexingTypeForCopying(secondType);
    if (type == NonArray || !firstArray->canFastCopy(vm, secondArray) || firstArraySize + secondArraySize >= MIN_SPARSE_ARRAY_INDEX) {
        JSArray* result = constructEmptyArray(exec, nullptr, firstArraySize + secondArraySize);
        if (vm.exception())
            return JSValue::encode(JSValue());

        if (!moveElements(exec, vm, result, 0, firstArray, firstArraySize)
            || !moveElements(exec, vm, result, firstArraySize, secondArray, secondArraySize)) {
            ASSERT(vm.exception());
            return JSValue::encode(JSValue());
        }

        return JSValue::encode(result);
    }

    Structure* resultStructure = exec->lexicalGlobalObject()->arrayStructureForIndexingTypeDuringAllocation(type);
    JSArray* result = JSArray::tryCreateUninitialized(vm, resultStructure, firstArraySize + secondArraySize);
    if (!result)
        return JSValue::encode(throwOutOfMemoryError(exec, scope));
    
    if (type == ArrayWithDouble) {
        double* buffer = result->butterfly()->contiguousDouble().data();
        memcpy(buffer, firstButterfly->contiguousDouble().data(), sizeof(JSValue) * firstArraySize);
        memcpy(buffer + firstArraySize, secondButterfly->contiguousDouble().data(), sizeof(JSValue) * secondArraySize);
    } else if (type != ArrayWithUndecided) {
        WriteBarrier<Unknown>* buffer = result->butterfly()->contiguous().data();
        memcpy(buffer, firstButterfly->contiguous().data(), sizeof(JSValue) * firstArraySize);
        if (secondType != ArrayWithUndecided)
            memcpy(buffer + firstArraySize, secondButterfly->contiguous().data(), sizeof(JSValue) * secondArraySize);
        else {
            for (unsigned i = secondArraySize; i--;)
                buffer[i + firstArraySize].clear();
        }
    }

    result->butterfly()->setPublicLength(firstArraySize + secondArraySize);
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL arrayProtoPrivateFuncAppendMemcpy(ExecState* exec)
{
    ASSERT(exec->argumentCount() == 3);

    VM& vm = exec->vm();
    JSArray* resultArray = jsCast<JSArray*>(exec->uncheckedArgument(0));
    JSArray* otherArray = jsCast<JSArray*>(exec->uncheckedArgument(1));
    JSValue startValue = exec->uncheckedArgument(2);
    ASSERT(startValue.isAnyInt() && startValue.asAnyInt() >= 0 && startValue.asAnyInt() <= std::numeric_limits<unsigned>::max());
    unsigned startIndex = static_cast<unsigned>(startValue.asAnyInt());
    if (!resultArray->appendMemcpy(exec, vm, startIndex, otherArray))
        moveElements(exec, vm, resultArray, startIndex, otherArray, otherArray->length());

    return JSValue::encode(jsUndefined());
}


// -------------------- ArrayPrototype.constructor Watchpoint ------------------

static bool verbose = false;

class ArrayPrototypeAdaptiveInferredPropertyWatchpoint : public AdaptiveInferredPropertyValueWatchpointBase {
public:
    typedef AdaptiveInferredPropertyValueWatchpointBase Base;
    ArrayPrototypeAdaptiveInferredPropertyWatchpoint(const ObjectPropertyCondition&, ArrayPrototype*);

private:
    void handleFire(const FireDetail&) override;

    ArrayPrototype* m_arrayPrototype;
};

ArrayPrototype::SpeciesWatchpointStatus ArrayPrototype::attemptToInitializeSpeciesWatchpoint(ExecState* exec)
{
    ASSERT(m_speciesWatchpointStatus == SpeciesWatchpointStatus::Uninitialized);

    VM& vm = exec->vm();

    if (verbose)
        dataLog("Attempting to initialize Array species watchpoints for Array.prototype: ", pointerDump(this), " with structure: ", pointerDump(this->structure()), "\nand Array: ", pointerDump(this->globalObject()->arrayConstructor()), " with structure: ", pointerDump(this->globalObject()->arrayConstructor()->structure()), "\n");
    // First we need to make sure that the Array.prototype.constructor property points to Array
    // and that Array[Symbol.species] is the primordial GetterSetter.

    // We only initialize once so flattening the structures does not have any real cost.
    Structure* prototypeStructure = this->structure(vm);
    if (prototypeStructure->isDictionary())
        prototypeStructure = prototypeStructure->flattenDictionaryStructure(vm, this);
    RELEASE_ASSERT(!prototypeStructure->isDictionary());

    JSGlobalObject* globalObject = this->globalObject();
    ArrayConstructor* arrayConstructor = globalObject->arrayConstructor();

    PropertySlot constructorSlot(this, PropertySlot::InternalMethodType::VMInquiry);
    JSValue(this).get(exec, vm.propertyNames->constructor, constructorSlot);
    if (constructorSlot.slotBase() != this
        || !constructorSlot.isCacheableValue()
        || constructorSlot.getValue(exec, vm.propertyNames->constructor) != arrayConstructor)
        return m_speciesWatchpointStatus = SpeciesWatchpointStatus::Fired;

    Structure* constructorStructure = arrayConstructor->structure(vm);
    if (constructorStructure->isDictionary())
        constructorStructure = constructorStructure->flattenDictionaryStructure(vm, arrayConstructor);

    PropertySlot speciesSlot(arrayConstructor, PropertySlot::InternalMethodType::VMInquiry);
    JSValue(arrayConstructor).get(exec, vm.propertyNames->speciesSymbol, speciesSlot);
    if (speciesSlot.slotBase() != arrayConstructor
        || !speciesSlot.isCacheableGetter()
        || speciesSlot.getterSetter() != globalObject->speciesGetterSetter())
        return m_speciesWatchpointStatus = SpeciesWatchpointStatus::Fired;

    // Now we need to setup the watchpoints to make sure these conditions remain valid.
    prototypeStructure->startWatchingPropertyForReplacements(vm, constructorSlot.cachedOffset());
    constructorStructure->startWatchingPropertyForReplacements(vm, speciesSlot.cachedOffset());

    ObjectPropertyCondition constructorCondition = ObjectPropertyCondition::equivalence(vm, this, this, vm.propertyNames->constructor.impl(), arrayConstructor);
    ObjectPropertyCondition speciesCondition = ObjectPropertyCondition::equivalence(vm, this, arrayConstructor, vm.propertyNames->speciesSymbol.impl(), globalObject->speciesGetterSetter());

    if (!constructorCondition.isWatchable() || !speciesCondition.isWatchable())
        return m_speciesWatchpointStatus = SpeciesWatchpointStatus::Fired;

    m_constructorWatchpoint = std::make_unique<ArrayPrototypeAdaptiveInferredPropertyWatchpoint>(constructorCondition, this);
    m_constructorWatchpoint->install();

    m_constructorSpeciesWatchpoint = std::make_unique<ArrayPrototypeAdaptiveInferredPropertyWatchpoint>(speciesCondition, this);
    m_constructorSpeciesWatchpoint->install();

    return m_speciesWatchpointStatus = SpeciesWatchpointStatus::Initialized;
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

    if (verbose)
        WTF::dataLog(stringDetail, "\n");

    m_arrayPrototype->m_speciesWatchpointStatus = ArrayPrototype::SpeciesWatchpointStatus::Fired;
}

} // namespace JSC

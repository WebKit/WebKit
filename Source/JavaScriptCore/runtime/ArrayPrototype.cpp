/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2019 Apple Inc. All rights reserved.
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
#include "Error.h"
#include "GetterSetter.h"
#include "Interpreter.h"
#include "JIT.h"
#include "JSArrayInlines.h"
#include "JSArrayIterator.h"
#include "JSCBuiltins.h"
#include "JSCInlines.h"
#include "JSImmutableButterfly.h"
#include "JSStringJoiner.h"
#include "Lookup.h"
#include "ObjectConstructor.h"
#include "ObjectPrototype.h"
#include "Operations.h"
#include "StringRecursionChecker.h"
#include <algorithm>
#include <wtf/Assertions.h>

namespace JSC {

EncodedJSValue JSC_HOST_CALL arrayProtoFuncToLocaleString(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncJoin(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncKeys(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncEntries(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncPop(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncPush(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncReverse(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncShift(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncSlice(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncSplice(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncUnShift(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncIndexOf(JSGlobalObject*, CallFrame*);
EncodedJSValue JSC_HOST_CALL arrayProtoFuncLastIndexOf(JSGlobalObject*, CallFrame*);

// ------------------------------ ArrayPrototype ----------------------------

const ClassInfo ArrayPrototype::s_info = {"Array", &JSArray::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ArrayPrototype)};

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
    ASSERT(inherits(vm, info()));

    putDirectWithoutTransition(vm, vm.propertyNames->toString, globalObject->arrayProtoToStringFunction(), static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().valuesPublicName(), globalObject->arrayProtoValuesFunction(), static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->iteratorSymbol, globalObject->arrayProtoValuesFunction(), static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->toLocaleString, arrayProtoFuncToLocaleString, static_cast<unsigned>(PropertyAttribute::DontEnum), 0);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().concatPublicName(), arrayPrototypeConcatCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().fillPublicName(), arrayPrototypeFillCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->join, arrayProtoFuncJoin, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION("pop", arrayProtoFuncPop, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ArrayPopIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().pushPublicName(), arrayProtoFuncPush, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ArrayPushIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().pushPrivateName(), arrayProtoFuncPush, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly, 1, ArrayPushIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("reverse", arrayProtoFuncReverse, static_cast<unsigned>(PropertyAttribute::DontEnum), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().shiftPublicName(), arrayProtoFuncShift, static_cast<unsigned>(PropertyAttribute::DontEnum), 0);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().shiftPrivateName(), arrayProtoFuncShift, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly, 0);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->slice, arrayProtoFuncSlice, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ArraySliceIntrinsic);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().sortPublicName(), arrayPrototypeSortCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("splice", arrayProtoFuncSplice, static_cast<unsigned>(PropertyAttribute::DontEnum), 2);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("unshift", arrayProtoFuncUnShift, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().everyPublicName(), arrayPrototypeEveryCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().forEachPublicName(), arrayPrototypeForEachCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().somePublicName(), arrayPrototypeSomeCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION("indexOf", arrayProtoFuncIndexOf, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ArrayIndexOfIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("lastIndexOf", arrayProtoFuncLastIndexOf, static_cast<unsigned>(PropertyAttribute::DontEnum), 1);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().filterPublicName(), arrayPrototypeFilterCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().flatPublicName(), arrayPrototypeFlatCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().flatMapPublicName(), arrayPrototypeFlatMapCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().reducePublicName(), arrayPrototypeReduceCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().reduceRightPublicName(), arrayPrototypeReduceRightCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().mapPublicName(), arrayPrototypeMapCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().keysPublicName(), arrayProtoFuncKeys, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ArrayKeysIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().entriesPublicName(), arrayProtoFuncEntries, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ArrayEntriesIntrinsic);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().findPublicName(), arrayPrototypeFindCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().findIndexPublicName(), arrayPrototypeFindIndexCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().includesPublicName(), arrayPrototypeIncludesCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().copyWithinPublicName(), arrayPrototypeCopyWithinCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));

    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().entriesPrivateName(), getDirect(vm, vm.propertyNames->builtinNames().entriesPublicName()), static_cast<unsigned>(PropertyAttribute::ReadOnly));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().forEachPrivateName(), getDirect(vm, vm.propertyNames->builtinNames().forEachPublicName()), static_cast<unsigned>(PropertyAttribute::ReadOnly));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().keysPrivateName(), getDirect(vm, vm.propertyNames->builtinNames().keysPublicName()), static_cast<unsigned>(PropertyAttribute::ReadOnly));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().valuesPrivateName(), globalObject->arrayProtoValuesFunction(), static_cast<unsigned>(PropertyAttribute::ReadOnly));

    JSObject* unscopables = constructEmptyObject(vm, globalObject->nullPrototypeObjectStructure());
    unscopables->convertToDictionary(vm);
    const Identifier* const unscopableNames[] = {
        &vm.propertyNames->builtinNames().copyWithinPublicName(),
        &vm.propertyNames->builtinNames().entriesPublicName(),
        &vm.propertyNames->builtinNames().fillPublicName(),
        &vm.propertyNames->builtinNames().findPublicName(),
        &vm.propertyNames->builtinNames().findIndexPublicName(),
        &vm.propertyNames->builtinNames().flatPublicName(),
        &vm.propertyNames->builtinNames().flatMapPublicName(),
        &vm.propertyNames->builtinNames().includesPublicName(),
        &vm.propertyNames->builtinNames().keysPublicName(),
        &vm.propertyNames->builtinNames().valuesPublicName()
    };
    for (const auto* unscopableName : unscopableNames)
        unscopables->putDirect(vm, *unscopableName, jsBoolean(true));
    putDirectWithoutTransition(vm, vm.propertyNames->unscopablesSymbol, unscopables, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
}

// ------------------------------ Array Functions ----------------------------

static ALWAYS_INLINE JSValue getProperty(JSGlobalObject* globalObject, JSObject* object, unsigned index)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    if (JSValue result = object->tryGetIndexQuickly(index))
        return result;
    // We want to perform get and has in the same operation.
    // We can only do so when this behavior is not observable. The
    // only time it is observable is when we encounter an opaque objects (ProxyObject and JSModuleNamespaceObject)
    // somewhere in the prototype chain.
    PropertySlot slot(object, PropertySlot::InternalMethodType::HasProperty);
    bool hasProperty = object->getPropertySlot(globalObject, index, slot);
    EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
    if (!hasProperty)
        return { };
    if (UNLIKELY(slot.isTaintedByOpaqueObject()))
        RELEASE_AND_RETURN(scope, object->get(globalObject, index));

    RELEASE_AND_RETURN(scope, slot.getValue(globalObject, index));
}

static ALWAYS_INLINE void putLength(JSGlobalObject* globalObject, VM& vm, JSObject* obj, JSValue value)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    PutPropertySlot slot(obj);
    bool success = obj->methodTable(vm)->put(obj, globalObject, vm.propertyNames->length, value, slot);
    RETURN_IF_EXCEPTION(scope, void());
    if (UNLIKELY(!success))
        throwTypeError(globalObject, scope, ReadonlyPropertyWriteError);
}

static ALWAYS_INLINE void setLength(JSGlobalObject* globalObject, VM& vm, JSObject* obj, unsigned value)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    static constexpr bool throwException = true;
    if (isJSArray(obj)) {
        jsCast<JSArray*>(obj)->setLength(globalObject, value, throwException);
        RETURN_IF_EXCEPTION(scope, void());
    }
    scope.release();
    putLength(globalObject, vm, obj, jsNumber(value));
}

namespace ArrayPrototypeInternal {
static bool verbose = false;
}

ALWAYS_INLINE bool speciesWatchpointIsValid(VM& vm, JSObject* thisObject)
{
    JSGlobalObject* globalObject = thisObject->globalObject(vm);
    ArrayPrototype* arrayPrototype = globalObject->arrayPrototype();

    if (globalObject->arraySpeciesWatchpointSet().stateOnJSThread() == ClearWatchpoint) {
        dataLogLnIf(ArrayPrototypeInternal::verbose, "Initializing Array species watchpoints for Array.prototype: ", pointerDump(arrayPrototype), " with structure: ", pointerDump(arrayPrototype->structure(vm)), "\nand Array: ", pointerDump(globalObject->arrayConstructor()), " with structure: ", pointerDump(globalObject->arrayConstructor()->structure(vm)));
        globalObject->tryInstallArraySpeciesWatchpoint();
        ASSERT(globalObject->arraySpeciesWatchpointSet().stateOnJSThread() != ClearWatchpoint);
    }

    return !thisObject->hasCustomProperties(vm)
        && arrayPrototype == thisObject->getPrototypeDirect(vm)
        && globalObject->arraySpeciesWatchpointSet().stateOnJSThread() == IsWatched;
}

enum class SpeciesConstructResult {
    FastPath,
    Exception,
    CreatedObject
};

static ALWAYS_INLINE std::pair<SpeciesConstructResult, JSObject*> speciesConstructArray(JSGlobalObject* globalObject, JSObject* thisObject, uint64_t length)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto exceptionResult = [] () {
        return std::make_pair(SpeciesConstructResult::Exception, nullptr);
    };

    // ECMA 9.4.2.3: https://tc39.github.io/ecma262/#sec-arrayspeciescreate
    JSValue constructor = jsUndefined();
    bool thisIsArray = isArray(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, exceptionResult());
    if (LIKELY(thisIsArray)) {
        // Fast path in the normal case where the user has not set an own constructor and the Array.prototype.constructor is normal.
        // We need prototype check for subclasses of Array, which are Array objects but have a different prototype by default.
        bool isValid = speciesWatchpointIsValid(vm, thisObject);
        scope.assertNoException();
        if (LIKELY(isValid))
            return std::make_pair(SpeciesConstructResult::FastPath, nullptr);

        constructor = thisObject->get(globalObject, vm.propertyNames->constructor);
        RETURN_IF_EXCEPTION(scope, exceptionResult());
        if (constructor.isConstructor(vm)) {
            JSObject* constructorObject = jsCast<JSObject*>(constructor);
            bool isArrayConstructorFromAnotherRealm = globalObject != constructorObject->globalObject(vm)
                && constructorObject->inherits<ArrayConstructor>(vm);
            if (isArrayConstructorFromAnotherRealm)
                return std::make_pair(SpeciesConstructResult::FastPath, nullptr);
        }
        if (constructor.isObject()) {
            constructor = constructor.get(globalObject, vm.propertyNames->speciesSymbol);
            RETURN_IF_EXCEPTION(scope, exceptionResult());
            if (constructor.isNull())
                return std::make_pair(SpeciesConstructResult::FastPath, nullptr);
        }
    } else {
        // If isArray is false, return ? ArrayCreate(length).
        return std::make_pair(SpeciesConstructResult::FastPath, nullptr);
    }

    if (constructor.isUndefined())
        return std::make_pair(SpeciesConstructResult::FastPath, nullptr);

    MarkedArgumentBuffer args;
    args.append(jsNumber(length));
    ASSERT(!args.hasOverflowed());
    JSObject* newObject = construct(globalObject, constructor, args, "Species construction did not get a valid constructor");
    RETURN_IF_EXCEPTION(scope, exceptionResult());
    return std::make_pair(SpeciesConstructResult::CreatedObject, newObject);
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncSpeciesCreate(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* object = asObject(callFrame->uncheckedArgument(0));
    uint64_t length = static_cast<uint64_t>(callFrame->uncheckedArgument(1).asNumber());

    std::pair<SpeciesConstructResult, JSObject*> speciesResult = speciesConstructArray(globalObject, object, length);
    EXCEPTION_ASSERT(!!scope.exception() == (speciesResult.first == SpeciesConstructResult::Exception));
    if (UNLIKELY(speciesResult.first == SpeciesConstructResult::Exception))
        return { };
    if (speciesResult.first == SpeciesConstructResult::CreatedObject)
        return JSValue::encode(speciesResult.second);

    if (length > std::numeric_limits<unsigned>::max()) {
        throwRangeError(globalObject, scope, "Array size is not a small enough positive integer."_s);
        return { };
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(constructEmptyArray(globalObject, nullptr, static_cast<unsigned>(length))));
}

static inline unsigned argumentClampedIndexFromStartOrEnd(JSGlobalObject* globalObject, JSValue value, unsigned length, unsigned undefinedValue = 0)
{
    if (value.isUndefined())
        return undefinedValue;

    double indexDouble = value.toInteger(globalObject);
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
void shift(JSGlobalObject* globalObject, JSObject* thisObj, unsigned header, unsigned currentCount, unsigned resultCount, unsigned length)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RELEASE_ASSERT(currentCount > resultCount);
    unsigned count = currentCount - resultCount;

    RELEASE_ASSERT(header <= length);
    RELEASE_ASSERT(currentCount <= (length - header));

    if (isJSArray(thisObj)) {
        JSArray* array = asArray(thisObj);
        if (array->length() == length && array->shiftCount<shiftCountMode>(globalObject, header, count))
            return;
    }

    for (unsigned k = header; k < length - currentCount; ++k) {
        unsigned from = k + currentCount;
        unsigned to = k + resultCount;
        JSValue value = getProperty(globalObject, thisObj, from);
        RETURN_IF_EXCEPTION(scope, void());
        if (value) {
            thisObj->putByIndexInline(globalObject, to, value, true);
            RETURN_IF_EXCEPTION(scope, void());
        } else {
            bool success = thisObj->methodTable(vm)->deletePropertyByIndex(thisObj, globalObject, to);
            RETURN_IF_EXCEPTION(scope, void());
            if (!success) {
                throwTypeError(globalObject, scope, UnableToDeletePropertyError);
                return;
            }
        }
    }
    for (unsigned k = length; k > length - count; --k) {
        bool success = thisObj->methodTable(vm)->deletePropertyByIndex(thisObj, globalObject, k - 1);
        RETURN_IF_EXCEPTION(scope, void());
        if (!success) {
            throwTypeError(globalObject, scope, UnableToDeletePropertyError);
            return;
        }
    }
}

template<JSArray::ShiftCountMode shiftCountMode>
void unshift(JSGlobalObject* globalObject, JSObject* thisObj, unsigned header, unsigned currentCount, unsigned resultCount, unsigned length)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RELEASE_ASSERT(resultCount > currentCount);
    unsigned count = resultCount - currentCount;

    RELEASE_ASSERT(header <= length);
    RELEASE_ASSERT(currentCount <= (length - header));

    // Guard against overflow.
    if (count > UINT_MAX - length) {
        throwOutOfMemoryError(globalObject, scope);
        return;
    }

    if (isJSArray(thisObj)) {
        JSArray* array = asArray(thisObj);
        if (array->length() == length) {
            bool handled = array->unshiftCount<shiftCountMode>(globalObject, header, count);
            EXCEPTION_ASSERT(!scope.exception() || handled);
            if (handled)
                return;
        }
    }

    for (unsigned k = length - currentCount; k > header; --k) {
        unsigned from = k + currentCount - 1;
        unsigned to = k + resultCount - 1;
        JSValue value = getProperty(globalObject, thisObj, from);
        RETURN_IF_EXCEPTION(scope, void());
        if (value) {
            thisObj->putByIndexInline(globalObject, to, value, true);
            RETURN_IF_EXCEPTION(scope, void());
        } else {
            bool success = thisObj->methodTable(vm)->deletePropertyByIndex(thisObj, globalObject, to);
            RETURN_IF_EXCEPTION(scope, void());
            if (UNLIKELY(!success)) {
                throwTypeError(globalObject, scope, UnableToDeletePropertyError);
                return;
            }
        }
    }
}

inline bool canUseFastJoin(const JSObject* thisObject)
{
    switch (thisObject->indexingType()) {
    case ALL_CONTIGUOUS_INDEXING_TYPES:
    case ALL_INT32_INDEXING_TYPES:
    case ALL_DOUBLE_INDEXING_TYPES:
    case ALL_UNDECIDED_INDEXING_TYPES:
        return true;
    default:
        break;
    }
    return false;
}

inline bool holesMustForwardToPrototype(VM& vm, JSObject* object)
{
    return object->structure(vm)->holesMustForwardToPrototype(vm, object);
}

inline bool isHole(double value)
{
    return std::isnan(value);
}

inline bool isHole(const WriteBarrier<Unknown>& value)
{
    return !value;
}

template<typename T>
inline bool containsHole(T* data, unsigned length)
{
    for (unsigned i = 0; i < length; ++i) {
        if (isHole(data[i]))
            return true;
    }
    return false;
}

inline JSValue fastJoin(JSGlobalObject* globalObject, JSObject* thisObject, StringView separator, unsigned length, bool* sawHoles = nullptr)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    switch (thisObject->indexingType()) {
    case ALL_INT32_INDEXING_TYPES: {
        auto& butterfly = *thisObject->butterfly();
        if (UNLIKELY(length > butterfly.publicLength()))
            break;
        JSStringJoiner joiner(globalObject, separator, length);
        RETURN_IF_EXCEPTION(scope, { });
        auto data = butterfly.contiguous().data();
        bool holesKnownToBeOK = false;
        for (unsigned i = 0; i < length; ++i) {
            JSValue value = data[i].get();
            if (LIKELY(value))
                joiner.appendNumber(vm, value.asInt32());
            else {
                if (sawHoles)
                    *sawHoles = true;
                if (!holesKnownToBeOK) {
                    if (holesMustForwardToPrototype(vm, thisObject))
                        goto generalCase;
                    holesKnownToBeOK = true;
                }
                joiner.appendEmptyString();
            }
        }
        RELEASE_AND_RETURN(scope, joiner.join(globalObject));
    }
    case ALL_CONTIGUOUS_INDEXING_TYPES: {
        auto& butterfly = *thisObject->butterfly();
        if (UNLIKELY(length > butterfly.publicLength()))
            break;
        JSStringJoiner joiner(globalObject, separator, length);
        RETURN_IF_EXCEPTION(scope, { });
        auto data = butterfly.contiguous().data();
        bool holesKnownToBeOK = false;
        for (unsigned i = 0; i < length; ++i) {
            if (JSValue value = data[i].get()) {
                if (!joiner.appendWithoutSideEffects(globalObject, value))
                    goto generalCase;
                RETURN_IF_EXCEPTION(scope, { });
            } else {
                if (sawHoles)
                    *sawHoles = true;
                if (!holesKnownToBeOK) {
                    if (holesMustForwardToPrototype(vm, thisObject))
                        goto generalCase;
                    holesKnownToBeOK = true;
                }
                joiner.appendEmptyString();
            }
        }
        RELEASE_AND_RETURN(scope, joiner.join(globalObject));
    }
    case ALL_DOUBLE_INDEXING_TYPES: {
        auto& butterfly = *thisObject->butterfly();
        if (UNLIKELY(length > butterfly.publicLength()))
            break;
        JSStringJoiner joiner(globalObject, separator, length);
        RETURN_IF_EXCEPTION(scope, { });
        auto data = butterfly.contiguousDouble().data();
        bool holesKnownToBeOK = false;
        for (unsigned i = 0; i < length; ++i) {
            double value = data[i];
            if (LIKELY(!isHole(value)))
                joiner.appendNumber(vm, value);
            else {
                if (sawHoles)
                    *sawHoles = true;
                if (!holesKnownToBeOK) {
                    if (holesMustForwardToPrototype(vm, thisObject))
                        goto generalCase;
                    holesKnownToBeOK = true;
                }
                joiner.appendEmptyString();
            }
        }
        RELEASE_AND_RETURN(scope, joiner.join(globalObject));
    }
    case ALL_UNDECIDED_INDEXING_TYPES: {
        if (length && holesMustForwardToPrototype(vm, thisObject))
            goto generalCase;
        switch (separator.length()) {
        case 0:
            RELEASE_AND_RETURN(scope, jsEmptyString(vm));
        case 1: {
            if (length <= 1)
                RELEASE_AND_RETURN(scope, jsEmptyString(vm));
            if (separator.is8Bit())
                RELEASE_AND_RETURN(scope, repeatCharacter(globalObject, separator.characters8()[0], length - 1));
            RELEASE_AND_RETURN(scope, repeatCharacter(globalObject, separator.characters16()[0], length - 1));
        default:
            JSString* result = jsEmptyString(vm);
            if (length <= 1)
                return result;

            JSString* operand = jsString(vm, separator.toString());
            RETURN_IF_EXCEPTION(scope, { });
            unsigned count = length - 1;
            for (;;) {
                if (count & 1) {
                    result = jsString(globalObject, result, operand);
                    RETURN_IF_EXCEPTION(scope, { });
                }
                count >>= 1;
                if (!count)
                    return result;
                operand = jsString(globalObject, operand, operand);
                RETURN_IF_EXCEPTION(scope, { });
            }
        }
        }
    }
    }

generalCase:
    JSStringJoiner joiner(globalObject, separator, length);
    RETURN_IF_EXCEPTION(scope, { });
    for (unsigned i = 0; i < length; ++i) {
        JSValue element = thisObject->getIndex(globalObject, i);
        RETURN_IF_EXCEPTION(scope, { });
        joiner.append(globalObject, element);
        RETURN_IF_EXCEPTION(scope, { });
    }
    RELEASE_AND_RETURN(scope, joiner.join(globalObject));
}

inline bool canUseDefaultArrayJoinForToString(VM& vm, JSObject* thisObject)
{
    JSGlobalObject* globalObject = thisObject->globalObject();

    if (globalObject->arrayJoinWatchpointSet().stateOnJSThread() != IsWatched)
        return false;

    Structure* structure = thisObject->structure(vm);

    // This is the fast case. Many arrays will be an original array.
    // We are doing very simple check here. If we do more complicated checks like looking into getDirect "join" of thisObject,
    // it would be possible that just looking into "join" function will show the same performance.
    return globalObject->isOriginalArrayStructure(structure);
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncToString(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue().toThis(globalObject, StrictMode);

    // 1. Let array be the result of calling ToObject on the this value.
    JSObject* thisObject = thisValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (!canUseDefaultArrayJoinForToString(vm, thisObject)) {
        // 2. Let func be the result of calling the [[Get]] internal method of array with argument "join".
        JSValue function = JSValue(thisObject).get(globalObject, vm.propertyNames->join);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());

        // 3. If IsCallable(func) is false, then let func be the standard built-in method Object.prototype.toString (15.2.4.2).
        bool customJoinCase = false;
        if (!function.isCell())
            customJoinCase = true;
        CallData callData;
        CallType callType = getCallData(vm, function, callData);
        if (callType == CallType::None)
            customJoinCase = true;

        if (UNLIKELY(customJoinCase))
            RELEASE_AND_RETURN(scope, JSValue::encode(jsMakeNontrivialString(globalObject, "[object ", thisObject->methodTable(vm)->className(thisObject, vm), "]")));

        // 4. Return the result of calling the [[Call]] internal method of func providing array as the this value and an empty arguments list.
        if (!isJSArray(thisObject) || callType != CallType::Host || callData.native.function != arrayProtoFuncJoin)
            RELEASE_AND_RETURN(scope, JSValue::encode(call(globalObject, function, callType, callData, thisObject, *vm.emptyList)));
    }

    ASSERT(isJSArray(thisValue));
    JSArray* thisArray = asArray(thisValue);

    unsigned length = thisArray->length();

    StringRecursionChecker checker(globalObject, thisArray);
    EXCEPTION_ASSERT(!scope.exception() || checker.earlyReturnValue());
    if (JSValue earlyReturnValue = checker.earlyReturnValue())
        return JSValue::encode(earlyReturnValue);

    if (LIKELY(canUseFastJoin(thisArray))) {
        const LChar comma = ',';
        scope.release();

        bool isCoW = isCopyOnWrite(thisArray->indexingMode());
        JSImmutableButterfly* immutableButterfly = nullptr;
        if (isCoW) {
            immutableButterfly = JSImmutableButterfly::fromButterfly(thisArray->butterfly());
            auto iter = vm.heap.immutableButterflyToStringCache.find(immutableButterfly);
            if (iter != vm.heap.immutableButterflyToStringCache.end())
                return JSValue::encode(iter->value);
        }

        bool sawHoles = false;
        JSValue result = fastJoin(globalObject, thisArray, { &comma, 1 }, length, &sawHoles);

        if (!sawHoles && result && isJSString(result) && isCoW) {
            ASSERT(JSImmutableButterfly::fromButterfly(thisArray->butterfly()) == immutableButterfly);
            vm.heap.immutableButterflyToStringCache.add(immutableButterfly, jsCast<JSString*>(result));
        }

        return JSValue::encode(result);
    }

    JSStringJoiner joiner(globalObject, ',', length);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    for (unsigned i = 0; i < length; ++i) {
        JSValue element = thisArray->tryGetIndexQuickly(i);
        if (!element) {
            element = thisArray->get(globalObject, i);
            RETURN_IF_EXCEPTION(scope, encodedJSValue());
        }
        joiner.append(globalObject, element);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(joiner.join(globalObject)));
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncToLocaleString(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue().toThis(globalObject, StrictMode);

    JSObject* thisObject = thisValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    unsigned length = toLength(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    StringRecursionChecker checker(globalObject, thisObject);
    EXCEPTION_ASSERT(!scope.exception() || checker.earlyReturnValue());
    if (JSValue earlyReturnValue = checker.earlyReturnValue())
        return JSValue::encode(earlyReturnValue);

    JSStringJoiner stringJoiner(globalObject, ',', length);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

#if ENABLE(INTL)
    ArgList arguments(callFrame);
#endif
    for (unsigned i = 0; i < length; ++i) {
        JSValue element = thisObject->getIndex(globalObject, i);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        if (element.isUndefinedOrNull())
            element = jsEmptyString(vm);
        else {
            JSValue conversionFunction = element.get(globalObject, vm.propertyNames->toLocaleString);
            RETURN_IF_EXCEPTION(scope, encodedJSValue());
            CallData callData;
            CallType callType = getCallData(vm, conversionFunction, callData);
            if (callType != CallType::None) {
#if ENABLE(INTL)
                element = call(globalObject, conversionFunction, callType, callData, element, arguments);
#else
                element = call(globalObject, conversionFunction, callType, callData, element, *vm.emptyList);
#endif
                RETURN_IF_EXCEPTION(scope, encodedJSValue());
            }
        }
        stringJoiner.append(globalObject, element);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(stringJoiner.join(globalObject)));
}

static JSValue slowJoin(JSGlobalObject* globalObject, JSObject* thisObject, JSString* separator, uint64_t length)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 5. If len is zero, return the empty String.
    if (!length)
        return jsEmptyString(vm);

    // 6. Let element0 be Get(O, "0").
    JSValue element0 = thisObject->getIndex(globalObject, 0);
    RETURN_IF_EXCEPTION(scope, { });

    // 7. If element0 is undefined or null, let R be the empty String; otherwise, let R be ? ToString(element0).
    JSString* r = nullptr;
    if (element0.isUndefinedOrNull())
        r = jsEmptyString(vm);
    else
        r = element0.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    // 8. Let k be 1.
    // 9. Repeat, while k < len
    // 9.e Increase k by 1..
    for (uint64_t k = 1; k < length; ++k) {
        // b. Let element be ? Get(O, ! ToString(k)).
        JSValue element = thisObject->get(globalObject, Identifier::fromString(vm, AtomString::number(k)));
        RETURN_IF_EXCEPTION(scope, { });

        // c. If element is undefined or null, let next be the empty String; otherwise, let next be ? ToString(element).
        JSString* next = nullptr;
        if (element.isUndefinedOrNull()) {
            if (!separator->length())
                continue;
            next = jsEmptyString(vm);
        } else
            next = element.toString(globalObject);
        RETURN_IF_EXCEPTION(scope, { });

        // a. Let S be the String value produced by concatenating R and sep.
        // d. Let R be a String value produced by concatenating S and next.
        r = jsString(globalObject, r, separator, next);
        RETURN_IF_EXCEPTION(scope, { });
    }
    // 10. Return R.
    return r;
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncJoin(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. Let O be ? ToObject(this value).
    JSObject* thisObject = callFrame->thisValue().toThis(globalObject, StrictMode).toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObject);
    if (UNLIKELY(!thisObject))
        return encodedJSValue();

    StringRecursionChecker checker(globalObject, thisObject);
    EXCEPTION_ASSERT(!scope.exception() || checker.earlyReturnValue());
    if (JSValue earlyReturnValue = checker.earlyReturnValue())
        return JSValue::encode(earlyReturnValue);

    // 2. Let len be ? ToLength(? Get(O, "length")).
    double length = toLength(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    // 3. If separator is undefined, let separator be the single-element String ",".
    JSValue separatorValue = callFrame->argument(0);
    if (separatorValue.isUndefined()) {
        const LChar comma = ',';

        if (UNLIKELY(length > std::numeric_limits<unsigned>::max() || !canUseFastJoin(thisObject))) {
            uint64_t length64 = static_cast<uint64_t>(length);
            ASSERT(static_cast<double>(length64) == length);
            JSString* jsSeparator = jsSingleCharacterString(vm, comma);
            RETURN_IF_EXCEPTION(scope, encodedJSValue());

            RELEASE_AND_RETURN(scope, JSValue::encode(slowJoin(globalObject, thisObject, jsSeparator, length64)));
        }

        unsigned unsignedLength = static_cast<unsigned>(length);
        ASSERT(static_cast<double>(unsignedLength) == length);

        RELEASE_AND_RETURN(scope, JSValue::encode(fastJoin(globalObject, thisObject, { &comma, 1 }, unsignedLength)));
    }

    // 4. Let sep be ? ToString(separator).
    JSString* jsSeparator = separatorValue.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (UNLIKELY(length > std::numeric_limits<unsigned>::max() || !canUseFastJoin(thisObject))) {
        uint64_t length64 = static_cast<uint64_t>(length);
        ASSERT(static_cast<double>(length64) == length);

        RELEASE_AND_RETURN(scope, JSValue::encode(slowJoin(globalObject, thisObject, jsSeparator, length64)));
    }

    auto viewWithString = jsSeparator->viewWithUnderlyingString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    RELEASE_AND_RETURN(scope, JSValue::encode(fastJoin(globalObject, thisObject, viewWithString.view, length)));
}

inline EncodedJSValue createArrayIteratorObject(JSGlobalObject* globalObject, CallFrame* callFrame, IterationKind kind)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* thisObject  = callFrame->thisValue().toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObject);
    UNUSED_PARAM(scope);
    if (UNLIKELY(!thisObject))
        return encodedJSValue();

    return JSValue::encode(JSArrayIterator::create(vm, globalObject->arrayIteratorStructure(), thisObject, jsNumber(static_cast<unsigned>(kind))));
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncValues(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return createArrayIteratorObject(globalObject, callFrame, IterationKind::Values);
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncEntries(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return createArrayIteratorObject(globalObject, callFrame, IterationKind::Entries);
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncKeys(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    return createArrayIteratorObject(globalObject, callFrame, IterationKind::Keys);
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncPop(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, StrictMode);

    if (isJSArray(thisValue))
        RELEASE_AND_RETURN(scope, JSValue::encode(asArray(thisValue)->pop(globalObject)));

    JSObject* thisObj = thisValue.toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObj);
    if (UNLIKELY(!thisObj))
        return encodedJSValue();
    unsigned length = toLength(globalObject, thisObj);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (length == 0) {
        scope.release();
        putLength(globalObject, vm, thisObj, jsNumber(length));
        return JSValue::encode(jsUndefined());
    }

    JSValue result = thisObj->get(globalObject, length - 1);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    bool success = thisObj->methodTable(vm)->deletePropertyByIndex(thisObj, globalObject, length - 1);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (UNLIKELY(!success)) {
        throwTypeError(globalObject, scope, UnableToDeletePropertyError);
        return encodedJSValue();
    }
    scope.release();
    putLength(globalObject, vm, thisObj, jsNumber(length - 1));
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncPush(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue().toThis(globalObject, StrictMode);

    if (LIKELY(isJSArray(thisValue) && callFrame->argumentCount() == 1)) {
        JSArray* array = asArray(thisValue);
        scope.release();
        array->pushInline(globalObject, callFrame->uncheckedArgument(0));
        return JSValue::encode(jsNumber(array->length()));
    }
    
    JSObject* thisObj = thisValue.toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObj);
    if (UNLIKELY(!thisObj))
        return encodedJSValue();
    unsigned length = toLength(globalObject, thisObj);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    for (unsigned n = 0; n < callFrame->argumentCount(); n++) {
        // Check for integer overflow; where safe we can do a fast put by index.
        if (length + n >= length)
            thisObj->methodTable(vm)->putByIndex(thisObj, globalObject, length + n, callFrame->uncheckedArgument(n), true);
        else {
            PutPropertySlot slot(thisObj);
            auto string = JSValue(static_cast<int64_t>(length) + static_cast<int64_t>(n)).toWTFString(globalObject);
            RETURN_IF_EXCEPTION(scope, encodedJSValue());
            Identifier propertyName = Identifier::fromString(vm, string);
            thisObj->methodTable(vm)->put(thisObj, globalObject, propertyName, callFrame->uncheckedArgument(n), slot);
        }
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    
    JSValue newLength(static_cast<int64_t>(length) + static_cast<int64_t>(callFrame->argumentCount()));
    scope.release();
    putLength(globalObject, vm, thisObj, newLength);
    return JSValue::encode(newLength);
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncReverse(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* thisObject = callFrame->thisValue().toThis(globalObject, StrictMode).toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObject);
    if (UNLIKELY(!thisObject))
        return encodedJSValue();

    unsigned length = toLength(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    thisObject->ensureWritable(vm);

    switch (thisObject->indexingType()) {
    case ALL_CONTIGUOUS_INDEXING_TYPES:
    case ALL_INT32_INDEXING_TYPES: {
        auto& butterfly = *thisObject->butterfly();
        if (length > butterfly.publicLength())
            break;
        auto data = butterfly.contiguous().data();
        if (containsHole(data, length) && holesMustForwardToPrototype(vm, thisObject))
            break;
        std::reverse(data, data + length);
        if (!hasInt32(thisObject->indexingType()))
            vm.heap.writeBarrier(thisObject);
        return JSValue::encode(thisObject);
    }
    case ALL_DOUBLE_INDEXING_TYPES: {
        auto& butterfly = *thisObject->butterfly();
        if (length > butterfly.publicLength())
            break;
        auto data = butterfly.contiguousDouble().data();
        if (containsHole(data, length) && holesMustForwardToPrototype(vm, thisObject))
            break;
        std::reverse(data, data + length);
        return JSValue::encode(thisObject);
    }
    case ALL_ARRAY_STORAGE_INDEXING_TYPES: {
        auto& storage = *thisObject->butterfly()->arrayStorage();
        if (length > storage.vectorLength())
            break;
        if (storage.hasHoles() && holesMustForwardToPrototype(vm, thisObject))
            break;
        auto data = storage.vector().data();
        std::reverse(data, data + length);
        vm.heap.writeBarrier(thisObject);
        return JSValue::encode(thisObject);
    }
    }

    unsigned middle = length / 2;
    for (unsigned lower = 0; lower < middle; lower++) {
        unsigned upper = length - lower - 1;
        bool lowerExists = thisObject->hasProperty(globalObject, lower);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        JSValue lowerValue;
        if (lowerExists) {
            lowerValue = thisObject->get(globalObject, lower);
            RETURN_IF_EXCEPTION(scope, encodedJSValue());
        }

        bool upperExists = thisObject->hasProperty(globalObject, upper);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        JSValue upperValue;
        if (upperExists) {
            upperValue = thisObject->get(globalObject, upper);
            RETURN_IF_EXCEPTION(scope, encodedJSValue());
        }

        if (upperExists) {
            thisObject->putByIndexInline(globalObject, lower, upperValue, true);
            RETURN_IF_EXCEPTION(scope, encodedJSValue());
        } else {
            bool success = thisObject->methodTable(vm)->deletePropertyByIndex(thisObject, globalObject, lower);
            RETURN_IF_EXCEPTION(scope, encodedJSValue());
            if (UNLIKELY(!success)) {
                throwTypeError(globalObject, scope, UnableToDeletePropertyError);
                return encodedJSValue();
            }
        }

        if (lowerExists) {
            thisObject->putByIndexInline(globalObject, upper, lowerValue, true);
            RETURN_IF_EXCEPTION(scope, encodedJSValue());
        } else {
            bool success = thisObject->methodTable(vm)->deletePropertyByIndex(thisObject, globalObject, upper);
            RETURN_IF_EXCEPTION(scope, encodedJSValue());
            if (UNLIKELY(!success)) {
                throwTypeError(globalObject, scope, UnableToDeletePropertyError);
                return encodedJSValue();
            }
        }
    }
    return JSValue::encode(thisObject);
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncShift(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* thisObj = callFrame->thisValue().toThis(globalObject, StrictMode).toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObj);
    if (UNLIKELY(!thisObj))
        return encodedJSValue();
    unsigned length = toLength(globalObject, thisObj);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (length == 0) {
        scope.release();
        putLength(globalObject, vm, thisObj, jsNumber(length));
        return JSValue::encode(jsUndefined());
    }

    JSValue result = thisObj->getIndex(globalObject, 0);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    shift<JSArray::ShiftCountForShift>(globalObject, thisObj, 0, 1, 0, length);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    scope.release();
    putLength(globalObject, vm, thisObj, jsNumber(length - 1));
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncSlice(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    // https://tc39.github.io/ecma262/#sec-array.prototype.slice
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* thisObj = callFrame->thisValue().toThis(globalObject, StrictMode).toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObj);
    if (UNLIKELY(!thisObj))
        return { };
    unsigned length = toLength(globalObject, thisObj);
    RETURN_IF_EXCEPTION(scope, { });

    unsigned begin = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(0), length);
    RETURN_IF_EXCEPTION(scope, { });
    unsigned end = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(1), length, length);
    RETURN_IF_EXCEPTION(scope, { });
    if (end < begin)
        end = begin;

    std::pair<SpeciesConstructResult, JSObject*> speciesResult = speciesConstructArray(globalObject, thisObj, end - begin);
    // We can only get an exception if we call some user function.
    EXCEPTION_ASSERT(!!scope.exception() == (speciesResult.first == SpeciesConstructResult::Exception));
    if (UNLIKELY(speciesResult.first == SpeciesConstructResult::Exception))
        return { };

    bool okToDoFastPath = speciesResult.first == SpeciesConstructResult::FastPath && isJSArray(thisObj) && length == toLength(globalObject, thisObj);
    RETURN_IF_EXCEPTION(scope, { });
    if (LIKELY(okToDoFastPath)) {
        if (JSArray* result = asArray(thisObj)->fastSlice(globalObject, begin, end - begin))
            return JSValue::encode(result);
    }

    JSObject* result;
    if (speciesResult.first == SpeciesConstructResult::CreatedObject)
        result = speciesResult.second;
    else {
        result = constructEmptyArray(globalObject, nullptr, end - begin);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // Document that we need to keep the source array alive until after anything
    // that can GC (e.g. allocating the result array).
    thisObj->use();

    unsigned n = 0;
    for (unsigned k = begin; k < end; k++, n++) {
        JSValue v = getProperty(globalObject, thisObj, k);
        RETURN_IF_EXCEPTION(scope, { });
        if (v) {
            result->putDirectIndex(globalObject, n, v, 0, PutDirectIndexShouldThrow);
            RETURN_IF_EXCEPTION(scope, { });
        }
    }
    scope.release();
    setLength(globalObject, vm, result, n);
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncSplice(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    // 15.4.4.12

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* thisObj = callFrame->thisValue().toThis(globalObject, StrictMode).toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObj);
    if (UNLIKELY(!thisObj))
        return encodedJSValue();
    unsigned length = toLength(globalObject, thisObj);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (!callFrame->argumentCount()) {
        std::pair<SpeciesConstructResult, JSObject*> speciesResult = speciesConstructArray(globalObject, thisObj, 0);
        EXCEPTION_ASSERT(!!scope.exception() == (speciesResult.first == SpeciesConstructResult::Exception));
        if (UNLIKELY(speciesResult.first == SpeciesConstructResult::Exception))
            return encodedJSValue();

        JSObject* result;
        if (speciesResult.first == SpeciesConstructResult::CreatedObject)
            result = speciesResult.second;
        else {
            result = constructEmptyArray(globalObject, nullptr);
            RETURN_IF_EXCEPTION(scope, encodedJSValue());
        }

        setLength(globalObject, vm, result, 0);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        scope.release();
        setLength(globalObject, vm, thisObj, length);
        return JSValue::encode(result);
    }

    unsigned actualStart = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(0), length);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    unsigned actualDeleteCount = length - actualStart;
    if (callFrame->argumentCount() > 1) {
        double deleteCount = callFrame->uncheckedArgument(1).toInteger(globalObject);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        if (deleteCount < 0)
            actualDeleteCount = 0;
        else if (deleteCount > length - actualStart)
            actualDeleteCount = length - actualStart;
        else
            actualDeleteCount = static_cast<unsigned>(deleteCount);
    }

    std::pair<SpeciesConstructResult, JSObject*> speciesResult = speciesConstructArray(globalObject, thisObj, actualDeleteCount);
    EXCEPTION_ASSERT(!!scope.exception() == (speciesResult.first == SpeciesConstructResult::Exception));
    if (speciesResult.first == SpeciesConstructResult::Exception)
        return JSValue::encode(jsUndefined());

    JSObject* result = nullptr;
    bool okToDoFastPath = speciesResult.first == SpeciesConstructResult::FastPath && isJSArray(thisObj) && length == toLength(globalObject, thisObj);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (LIKELY(okToDoFastPath))
        result = asArray(thisObj)->fastSlice(globalObject, actualStart, actualDeleteCount);

    if (!result) {
        if (speciesResult.first == SpeciesConstructResult::CreatedObject)
            result = speciesResult.second;
        else {
            result = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithUndecided), actualDeleteCount);
            if (UNLIKELY(!result)) {
                throwOutOfMemoryError(globalObject, scope);
                return encodedJSValue();
            }
        }
        for (unsigned k = 0; k < actualDeleteCount; ++k) {
            JSValue v = getProperty(globalObject, thisObj, k + actualStart);
            RETURN_IF_EXCEPTION(scope, encodedJSValue());
            if (UNLIKELY(!v))
                continue;
            result->putDirectIndex(globalObject, k, v, 0, PutDirectIndexShouldThrow);
            RETURN_IF_EXCEPTION(scope, encodedJSValue());
        }
    }

    unsigned itemCount = std::max<int>(callFrame->argumentCount() - 2, 0);
    if (itemCount < actualDeleteCount) {
        shift<JSArray::ShiftCountForSplice>(globalObject, thisObj, actualStart, actualDeleteCount, itemCount, length);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    } else if (itemCount > actualDeleteCount) {
        unshift<JSArray::ShiftCountForSplice>(globalObject, thisObj, actualStart, actualDeleteCount, itemCount, length);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    for (unsigned k = 0; k < itemCount; ++k) {
        thisObj->putByIndexInline(globalObject, k + actualStart, callFrame->uncheckedArgument(k + 2), true);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    
    scope.release();
    setLength(globalObject, vm, thisObj, length - actualDeleteCount + itemCount);
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncUnShift(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    // 15.4.4.13

    JSObject* thisObj = callFrame->thisValue().toThis(globalObject, StrictMode).toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObj);
    if (UNLIKELY(!thisObj))
        return encodedJSValue();
    double doubleLength = toLength(globalObject, thisObj);
    unsigned length = doubleLength;
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    unsigned nrArgs = callFrame->argumentCount();
    if (nrArgs) {
        if (UNLIKELY(doubleLength + static_cast<double>(nrArgs) > maxSafeInteger()))
            return throwVMTypeError(globalObject, scope, "Cannot shift to offset greater than (2 ** 53) - 1"_s);
        unshift<JSArray::ShiftCountForShift>(globalObject, thisObj, 0, 0, nrArgs, length);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    for (unsigned k = 0; k < nrArgs; ++k) {
        thisObj->putByIndexInline(globalObject, k, callFrame->uncheckedArgument(k), true);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    JSValue result = jsNumber(length + nrArgs);
    scope.release();
    putLength(globalObject, vm, thisObj, result);
    return JSValue::encode(result);
}

enum class IndexOfDirection { Forward, Backward };
template<IndexOfDirection direction>
ALWAYS_INLINE JSValue fastIndexOf(JSGlobalObject* globalObject, VM& vm, JSArray* array, unsigned length, JSValue searchElement, unsigned index)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool canDoFastPath = array->canDoFastIndexedAccess(vm)
        && array->getArrayLength() == length; // The effects in getting `index` could have changed the length of this array.
    if (!canDoFastPath)
        return JSValue();

    switch (array->indexingType()) {
    case ALL_INT32_INDEXING_TYPES: {
        if (!searchElement.isNumber())
            return jsNumber(-1);
        JSValue searchInt32;
        if (searchElement.isInt32())
            searchInt32 = searchElement;
        else {
            double searchNumber = searchElement.asNumber();
            if (!canBeInt32(searchNumber))
                return jsNumber(-1);
            searchInt32 = jsNumber(static_cast<int32_t>(searchNumber));
        }
        auto& butterfly = *array->butterfly();
        auto data = butterfly.contiguous().data();
        if (direction == IndexOfDirection::Forward) {
            for (; index < length; ++index) {
                // Array#indexOf uses `===` semantics (not HashMap isEqual semantics).
                // And the hole never matches against Int32 value.
                if (searchInt32 == data[index].get())
                    return jsNumber(index);
            }
        } else {
            do {
                ASSERT(index < length);
                // Array#lastIndexOf uses `===` semantics (not HashMap isEqual semantics).
                // And the hole never matches against Int32 value.
                if (searchInt32 == data[index].get())
                    return jsNumber(index);
            } while (index--);
        }
        return jsNumber(-1);
    }
    case ALL_CONTIGUOUS_INDEXING_TYPES: {
        auto& butterfly = *array->butterfly();
        auto data = butterfly.contiguous().data();

        if (direction == IndexOfDirection::Forward) {
            for (; index < length; ++index) {
                JSValue value = data[index].get();
                if (!value)
                    continue;
                bool isEqual = JSValue::strictEqual(globalObject, searchElement, value);
                RETURN_IF_EXCEPTION(scope, { });
                if (isEqual)
                    return jsNumber(index);
            }
        } else {
            do {
                ASSERT(index < length);
                JSValue value = data[index].get();
                if (!value)
                    continue;
                bool isEqual = JSValue::strictEqual(globalObject, searchElement, value);
                RETURN_IF_EXCEPTION(scope, { });
                if (isEqual)
                    return jsNumber(index);
            } while (index--);
        }
        return jsNumber(-1);
    }
    case ALL_DOUBLE_INDEXING_TYPES: {
        if (!searchElement.isNumber())
            return jsNumber(-1);
        double searchNumber = searchElement.asNumber();
        auto& butterfly = *array->butterfly();
        auto data = butterfly.contiguousDouble().data();
        if (direction == IndexOfDirection::Forward) {
            for (; index < length; ++index) {
                // Array#indexOf uses `===` semantics (not HashMap isEqual semantics).
                // And the hole never matches since it is NaN.
                if (data[index] == searchNumber)
                    return jsNumber(index);
            }
        } else {
            do {
                ASSERT(index < length);
                // Array#lastIndexOf uses `===` semantics (not HashMap isEqual semantics).
                // And the hole never matches since it is NaN.
                if (data[index] == searchNumber)
                    return jsNumber(index);
            } while (index--);
        }
        return jsNumber(-1);
    }
    default:
        return JSValue();
    }
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncIndexOf(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 15.4.4.14
    JSObject* thisObject = callFrame->thisValue().toThis(globalObject, StrictMode).toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObject);
    if (UNLIKELY(!thisObject))
        return { };
    unsigned length = toLength(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });

    unsigned index = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(1), length);
    RETURN_IF_EXCEPTION(scope, { });
    JSValue searchElement = callFrame->argument(0);

    if (isJSArray(thisObject)) {
        JSValue result = fastIndexOf<IndexOfDirection::Forward>(globalObject, vm, asArray(thisObject), length, searchElement, index);
        RETURN_IF_EXCEPTION(scope, { });
        if (result)
            return JSValue::encode(result);
    }

    for (; index < length; ++index) {
        JSValue e = getProperty(globalObject, thisObject, index);
        RETURN_IF_EXCEPTION(scope, { });
        if (!e)
            continue;
        bool isEqual = JSValue::strictEqual(globalObject, searchElement, e);
        RETURN_IF_EXCEPTION(scope, { });
        if (isEqual)
            return JSValue::encode(jsNumber(index));
    }

    return JSValue::encode(jsNumber(-1));
}

EncodedJSValue JSC_HOST_CALL arrayProtoFuncLastIndexOf(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 15.4.4.15
    JSObject* thisObject = callFrame->thisValue().toThis(globalObject, StrictMode).toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObject);
    if (UNLIKELY(!thisObject))
        return { };
    unsigned length = toLength(globalObject, thisObject);
    if (UNLIKELY(scope.exception()) || !length)
        return JSValue::encode(jsNumber(-1));

    unsigned index = length - 1;
    if (callFrame->argumentCount() >= 2) {
        JSValue fromValue = callFrame->uncheckedArgument(1);
        double fromDouble = fromValue.toInteger(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        if (fromDouble < 0) {
            fromDouble += length;
            if (fromDouble < 0)
                return JSValue::encode(jsNumber(-1));
        }
        if (fromDouble < length)
            index = static_cast<unsigned>(fromDouble);
    }

    JSValue searchElement = callFrame->argument(0);

    if (isJSArray(thisObject)) {
        JSValue result = fastIndexOf<IndexOfDirection::Backward>(globalObject, vm, asArray(thisObject), length, searchElement, index);
        RETURN_IF_EXCEPTION(scope, { });
        if (result)
            return JSValue::encode(result);
    }

    do {
        ASSERT(index < length);
        JSValue e = getProperty(globalObject, thisObject, index);
        RETURN_IF_EXCEPTION(scope, { });
        if (!e)
            continue;
        bool isEqual = JSValue::strictEqual(globalObject, searchElement, e);
        RETURN_IF_EXCEPTION(scope, { });
        if (isEqual)
            return JSValue::encode(jsNumber(index));
    } while (index--);

    return JSValue::encode(jsNumber(-1));
}

static bool moveElements(JSGlobalObject* globalObject, VM& vm, JSArray* target, unsigned targetOffset, JSArray* source, unsigned sourceLength)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (LIKELY(!hasAnyArrayStorage(source->indexingType()) && !holesMustForwardToPrototype(vm, source))) {
        for (unsigned i = 0; i < sourceLength; ++i) {
            JSValue value = source->tryGetIndexQuickly(i);
            if (value) {
                target->putDirectIndex(globalObject, targetOffset + i, value, 0, PutDirectIndexShouldThrow);
                RETURN_IF_EXCEPTION(scope, false);
            }
        }
    } else {
        for (unsigned i = 0; i < sourceLength; ++i) {
            JSValue value = getProperty(globalObject, source, i);
            RETURN_IF_EXCEPTION(scope, false);
            if (value) {
                target->putDirectIndex(globalObject, targetOffset + i, value, 0, PutDirectIndexShouldThrow);
                RETURN_IF_EXCEPTION(scope, false);
            }
        }
    }
    return true;
}

static EncodedJSValue concatAppendOne(JSGlobalObject* globalObject, VM& vm, JSArray* first, JSValue second)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(!isJSArray(second));
    ASSERT(!shouldUseSlowPut(first->indexingType()));
    Butterfly* firstButterfly = first->butterfly();
    unsigned firstArraySize = firstButterfly->publicLength();

    Checked<unsigned, RecordOverflow> checkedResultSize = firstArraySize;
    checkedResultSize += 1;
    if (UNLIKELY(checkedResultSize.hasOverflowed())) {
        throwOutOfMemoryError(globalObject, scope);
        return encodedJSValue();
    }

    unsigned resultSize = checkedResultSize.unsafeGet();
    IndexingType type = first->mergeIndexingTypeForCopying(indexingTypeForValue(second) | IsArray);
    
    if (type == NonArray)
        type = first->indexingType();

    Structure* resultStructure = globalObject->arrayStructureForIndexingTypeDuringAllocation(type);
    JSArray* result = JSArray::tryCreate(vm, resultStructure, resultSize);
    if (UNLIKELY(!result)) {
        throwOutOfMemoryError(globalObject, scope);
        return encodedJSValue();
    }

    bool success = result->appendMemcpy(globalObject, vm, 0, first);
    EXCEPTION_ASSERT(!scope.exception() || !success);
    if (!success) {
        RETURN_IF_EXCEPTION(scope, encodedJSValue());

        bool success = moveElements(globalObject, vm, result, 0, first, firstArraySize);
        EXCEPTION_ASSERT(!scope.exception() == success);
        if (UNLIKELY(!success))
            return encodedJSValue();
    }

    scope.release();
    result->putDirectIndex(globalObject, firstArraySize, second);
    return JSValue::encode(result);

}

template<typename T>
void clearElement(T& element)
{
    element.clear();
}

template<>
void clearElement(double& element)
{
    element = PNaN;
}

template<typename T>
ALWAYS_INLINE void copyElements(T* buffer, unsigned offset, T* source, unsigned sourceSize, IndexingType sourceType)
{
    if (sourceType != ArrayWithUndecided) {
        gcSafeMemcpy(buffer + offset, source, sizeof(JSValue) * sourceSize);
        return;
    }

    for (unsigned i = sourceSize; i--;)
        clearElement<T>(buffer[i + offset]);
};

EncodedJSValue JSC_HOST_CALL arrayProtoPrivateFuncConcatMemcpy(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    ASSERT(callFrame->argumentCount() == 2);
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSArray* firstArray = jsCast<JSArray*>(callFrame->uncheckedArgument(0));
    
    // This code assumes that neither array has set Symbol.isConcatSpreadable. If the first array
    // has indexed accessors then one of those accessors might change the value of Symbol.isConcatSpreadable
    // on the second argument.
    if (UNLIKELY(shouldUseSlowPut(firstArray->indexingType())))
        return JSValue::encode(jsNull());

    // We need to check the species constructor here since checking it in the JS wrapper is too expensive for the non-optimizing tiers.
    bool isValid = speciesWatchpointIsValid(vm, firstArray);
    scope.assertNoException();
    if (UNLIKELY(!isValid))
        return JSValue::encode(jsNull());

    JSValue second = callFrame->uncheckedArgument(1);
    if (!isJSArray(second))
        RELEASE_AND_RETURN(scope, concatAppendOne(globalObject, vm, firstArray, second));

    JSArray* secondArray = jsCast<JSArray*>(second);
    
    Butterfly* firstButterfly = firstArray->butterfly();
    Butterfly* secondButterfly = secondArray->butterfly();

    unsigned firstArraySize = firstButterfly->publicLength();
    unsigned secondArraySize = secondButterfly->publicLength();

    Checked<unsigned, RecordOverflow> checkedResultSize = firstArraySize;
    checkedResultSize += secondArraySize;

    if (UNLIKELY(checkedResultSize.hasOverflowed())) {
        throwOutOfMemoryError(globalObject, scope);
        return encodedJSValue();
    }

    unsigned resultSize = checkedResultSize.unsafeGet();
    IndexingType firstType = firstArray->indexingType();
    IndexingType secondType = secondArray->indexingType();
    IndexingType type = firstArray->mergeIndexingTypeForCopying(secondType);
    if (type == NonArray || !firstArray->canFastCopy(vm, secondArray) || resultSize >= MIN_SPARSE_ARRAY_INDEX) {
        JSArray* result = constructEmptyArray(globalObject, nullptr, resultSize);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());

        bool success = moveElements(globalObject, vm, result, 0, firstArray, firstArraySize);
        EXCEPTION_ASSERT(!scope.exception() == success);
        if (UNLIKELY(!success))
            return encodedJSValue();
        success = moveElements(globalObject, vm, result, firstArraySize, secondArray, secondArraySize);
        EXCEPTION_ASSERT(!scope.exception() == success);
        if (UNLIKELY(!success))
            return encodedJSValue();

        return JSValue::encode(result);
    }

    Structure* resultStructure = globalObject->arrayStructureForIndexingTypeDuringAllocation(type);
    if (UNLIKELY(hasAnyArrayStorage(resultStructure->indexingType())))
        return JSValue::encode(jsNull());

    ASSERT(!globalObject->isHavingABadTime());
    ObjectInitializationScope initializationScope(vm);
    JSArray* result = JSArray::tryCreateUninitializedRestricted(initializationScope, resultStructure, resultSize);
    if (UNLIKELY(!result)) {
        throwOutOfMemoryError(globalObject, scope);
        return encodedJSValue();
    }

    if (type == ArrayWithDouble) {
        double* buffer = result->butterfly()->contiguousDouble().data();
        copyElements(buffer, 0, firstButterfly->contiguousDouble().data(), firstArraySize, firstType);
        copyElements(buffer, firstArraySize, secondButterfly->contiguousDouble().data(), secondArraySize, secondType);

    } else if (type != ArrayWithUndecided) {
        WriteBarrier<Unknown>* buffer = result->butterfly()->contiguous().data();
        copyElements(buffer, 0, firstButterfly->contiguous().data(), firstArraySize, firstType);
        copyElements(buffer, firstArraySize, secondButterfly->contiguous().data(), secondArraySize, secondType);
    }

    ASSERT(result->butterfly()->publicLength() == resultSize);
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL arrayProtoPrivateFuncAppendMemcpy(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    ASSERT(callFrame->argumentCount() == 3);

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSArray* resultArray = jsCast<JSArray*>(callFrame->uncheckedArgument(0));
    JSArray* otherArray = jsCast<JSArray*>(callFrame->uncheckedArgument(1));
    JSValue startValue = callFrame->uncheckedArgument(2);
    ASSERT(startValue.isUInt32AsAnyInt());
    unsigned startIndex = startValue.asUInt32AsAnyInt();
    bool success = resultArray->appendMemcpy(globalObject, vm, startIndex, otherArray);
    EXCEPTION_ASSERT(!scope.exception() || !success);
    if (success)
        return JSValue::encode(jsUndefined());
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    scope.release();
    moveElements(globalObject, vm, resultArray, startIndex, otherArray, otherArray->length());
    return JSValue::encode(jsUndefined());
}

} // namespace JSC

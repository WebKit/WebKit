/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2024 Apple Inc. All rights reserved.
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

#include "ArrayConstructor.h"
#include "ArrayPrototypeInlines.h"
#include "BuiltinNames.h"
#include "CachedCallInlines.h"
#include "IntegrityInlines.h"
#include "InterpreterInlines.h"
#include "JSArrayInlines.h"
#include "JSArrayIterator.h"
#include "JSCBuiltins.h"
#include "JSCInlines.h"
#include "JSCellButterfly.h"
#include "JSStringJoiner.h"
#include "ObjectConstructor.h"
#include "ObjectPrototypeInlines.h"
#include "StableSort.h"
#include "StringRecursionChecker.h"
#include "VMEntryScopeInlines.h"
#include <algorithm>
#include <wtf/Assertions.h>
#include <wtf/StdMap.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncToLocaleString);
static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncJoin);
static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncKeys);
static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncEntries);
static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncPop);
static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncPush);
static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncReverse);
static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncShift);
static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncSlice);
static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncSort);
static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncSplice);
static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncUnShift);
static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncIndexOf);
static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncLastIndexOf);
static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncConcat);
static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncFill);
static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncToReversed);
static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncToSorted);
static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncWith);
static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncIncludes);
static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncCopyWithin);
static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncToSpliced);

// ------------------------------ ArrayPrototype ----------------------------

const ClassInfo ArrayPrototype::s_info = { "Array"_s, &JSArray::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ArrayPrototype) };

ArrayPrototype* ArrayPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    ArrayPrototype* prototype = new (NotNull, allocateCell<ArrayPrototype>(vm)) ArrayPrototype(vm, structure);
    prototype->finishCreation(vm, globalObject);
    return prototype;
}

// ECMA 15.4.4
ArrayPrototype::ArrayPrototype(VM& vm, Structure* structure)
    : JSArray(vm, structure, nullptr)
{
}

void ArrayPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    putDirectWithoutTransition(vm, vm.propertyNames->toString, globalObject->arrayProtoToStringFunction(), static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().valuesPublicName(), globalObject->arrayProtoValuesFunction(), static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->iteratorSymbol, globalObject->arrayProtoValuesFunction(), static_cast<unsigned>(PropertyAttribute::DontEnum));

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->toLocaleString, arrayProtoFuncToLocaleString, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().concatPublicName(), arrayProtoFuncConcat, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->fill, arrayProtoFuncFill, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->join, arrayProtoFuncJoin, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION("pop"_s, arrayProtoFuncPop, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public, ArrayPopIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().pushPublicName(), arrayProtoFuncPush, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public, ArrayPushIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("reverse"_s, arrayProtoFuncReverse, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().shiftPublicName(), arrayProtoFuncShift, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().shiftPrivateName(), arrayProtoFuncShift, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly, 0, ImplementationVisibility::Public);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->slice, arrayProtoFuncSlice, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public, ArraySliceIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->sort, arrayProtoFuncSort, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION("splice"_s, arrayProtoFuncSplice, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public, ArraySpliceIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("unshift"_s, arrayProtoFuncUnShift, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().everyPublicName(), arrayPrototypeEveryCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().forEachPublicName(), arrayPrototypeForEachCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().somePublicName(), arrayPrototypeSomeCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().indexOfPublicName(), arrayProtoFuncIndexOf, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public, ArrayIndexOfIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("lastIndexOf"_s, arrayProtoFuncLastIndexOf, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().filterPublicName(), arrayPrototypeFilterCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().flatPublicName(), arrayPrototypeFlatCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().flatMapPublicName(), arrayPrototypeFlatMapCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().reducePublicName(), arrayPrototypeReduceCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().reduceRightPublicName(), arrayPrototypeReduceRightCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().mapPublicName(), arrayPrototypeMapCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().keysPublicName(), arrayProtoFuncKeys, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public, ArrayKeysIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().entriesPublicName(), arrayProtoFuncEntries, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public, ArrayEntriesIntrinsic);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().findPublicName(), arrayPrototypeFindCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().findLastPublicName(), arrayPrototypeFindLastCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().findIndexPublicName(), arrayPrototypeFindIndexCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().findLastIndexPublicName(), arrayPrototypeFindLastIndexCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->includes, arrayProtoFuncIncludes, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public, ArrayIncludesIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->copyWithin, arrayProtoFuncCopyWithin, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().atPublicName(), arrayPrototypeAtCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->toReversed, arrayProtoFuncToReversed, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->toSorted, arrayProtoFuncToSorted, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->toSpliced, arrayProtoFuncToSpliced, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->with, arrayProtoFuncWith, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().entriesPrivateName(), getDirect(vm, vm.propertyNames->builtinNames().entriesPublicName()), static_cast<unsigned>(PropertyAttribute::ReadOnly));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().forEachPrivateName(), getDirect(vm, vm.propertyNames->builtinNames().forEachPublicName()), static_cast<unsigned>(PropertyAttribute::ReadOnly));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().includesPrivateName(), getDirect(vm, vm.propertyNames->includes), static_cast<unsigned>(PropertyAttribute::ReadOnly));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().indexOfPrivateName(), getDirect(vm, vm.propertyNames->builtinNames().indexOfPublicName()), static_cast<unsigned>(PropertyAttribute::ReadOnly));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().keysPrivateName(), getDirect(vm, vm.propertyNames->builtinNames().keysPublicName()), static_cast<unsigned>(PropertyAttribute::ReadOnly));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().mapPrivateName(), getDirect(vm, vm.propertyNames->builtinNames().mapPublicName()), static_cast<unsigned>(PropertyAttribute::ReadOnly));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().popPrivateName(), getDirect(vm, vm.propertyNames->builtinNames().popPublicName()), static_cast<unsigned>(PropertyAttribute::ReadOnly));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().valuesPrivateName(), globalObject->arrayProtoValuesFunction(), static_cast<unsigned>(PropertyAttribute::ReadOnly));

    JSObject* unscopables = constructEmptyObject(vm, globalObject->nullPrototypeObjectStructure());
    unscopables->convertToDictionary(vm);
    const Identifier* const unscopableNames[] = {
        &vm.propertyNames->builtinNames().atPublicName(),
        &vm.propertyNames->copyWithin,
        &vm.propertyNames->builtinNames().entriesPublicName(),
        &vm.propertyNames->fill,
        &vm.propertyNames->builtinNames().findPublicName(),
        &vm.propertyNames->builtinNames().findIndexPublicName(),
        &vm.propertyNames->builtinNames().findLastPublicName(),
        &vm.propertyNames->builtinNames().findLastIndexPublicName(),
        &vm.propertyNames->builtinNames().flatPublicName(),
        &vm.propertyNames->builtinNames().flatMapPublicName(),
        &vm.propertyNames->includes,
        &vm.propertyNames->builtinNames().keysPublicName(),
        &vm.propertyNames->toReversed,
        &vm.propertyNames->toSorted,
        &vm.propertyNames->toSpliced,
        &vm.propertyNames->builtinNames().valuesPublicName()
    };
    for (const auto* unscopableName : unscopableNames) {
        if (unscopableName)
            unscopables->putDirect(vm, *unscopableName, jsBoolean(true));
    }
    putDirectWithoutTransition(vm, vm.propertyNames->unscopablesSymbol, unscopables, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
}

// ------------------------------ Array Functions ----------------------------

enum class RelativeNegativeIndex : bool {
    No,
    Yes,
};

template <RelativeNegativeIndex relativeNegativeIndex>
static inline uint64_t argumentClampedIndexFromStartOrEnd(JSGlobalObject* globalObject, JSValue value, uint64_t length, uint64_t undefinedValue = 0)
{
    if (value.isUndefined())
        return undefinedValue;

    if (value.isInt32()) [[likely]] {
        int64_t indexInt64 = value.asInt32();
        if (indexInt64 < 0) {
            if constexpr (relativeNegativeIndex == RelativeNegativeIndex::Yes) {
                indexInt64 += length;
                return indexInt64 < 0 ? 0 : static_cast<uint64_t>(indexInt64);
            } else
                return 0;
        }
        uint64_t indexUInt64 = static_cast<uint64_t>(indexInt64);
        return std::min(indexUInt64, length);
    }

    double indexDouble = value.toIntegerOrInfinity(globalObject);
    if (indexDouble < 0) {
        if constexpr (relativeNegativeIndex == RelativeNegativeIndex::Yes) {
            indexDouble += length;
            return indexDouble < 0 ? 0 : static_cast<uint64_t>(indexDouble);
        } else
            return 0;
    }
    return indexDouble > length ? length : static_cast<uint64_t>(indexDouble);
}

static inline int64_t argumentUnclampedIndexFromStartOrEnd(JSGlobalObject* globalObject, JSValue value, uint64_t length, uint64_t undefinedValue = 0)
{
    if (value.isUndefined())
        return undefinedValue;

    if (value.isInt32()) [[likely]] {
        int64_t indexInt64 = value.asInt32();
        if (indexInt64 < 0)
            indexInt64 += length;
        return indexInt64;
    }

    double indexDouble = value.toIntegerOrInfinity(globalObject);
    if (indexDouble < 0)
        indexDouble += length;
    if (std::isinf(indexDouble)) [[unlikely]]
        return std::signbit(indexDouble) ? std::numeric_limits<int64_t>::min() : std::numeric_limits<int64_t>::max();
    return static_cast<int64_t>(indexDouble);
}

ALWAYS_INLINE JSString* fastArrayJoin(JSGlobalObject* globalObject, JSObject* thisObject, StringView separator, unsigned length)
{
    bool sawHoles = false;
    bool genericCase = false;
    return fastArrayJoin(globalObject, thisObject, separator, length, sawHoles, genericCase);
}

inline bool canUseDefaultArrayJoinForToString(JSObject* thisObject)
{
    JSGlobalObject* globalObject = thisObject->globalObject();

    if (!globalObject->arrayJoinWatchpointSet().isStillValid())
        return false;

    Structure* structure = thisObject->structure();

    // This is the fast case. Many arrays will be an original array.
    // We are doing very simple check here. If we do more complicated checks like looking into getDirect "join" of thisObject,
    // it would be possible that just looking into "join" function will show the same performance.
    return globalObject->isOriginalArrayStructure(structure);
}

JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());

    // 1. Let array be the result of calling ToObject on the this value.
    JSObject* thisObject = thisValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    Integrity::auditStructureID(thisObject->structureID());
    if (!canUseDefaultArrayJoinForToString(thisObject)) [[unlikely]] {
        // 2. Let func be the result of calling the [[Get]] internal method of array with argument "join".
        JSValue function = thisObject->get(globalObject, vm.propertyNames->join);
        RETURN_IF_EXCEPTION(scope, { });

        // 3. If IsCallable(func) is false, then let func be the standard built-in method Object.prototype.toString (15.2.4.2).
        auto callData = JSC::getCallDataInline(function);
        if (callData.type == CallData::Type::None) [[unlikely]]
            RELEASE_AND_RETURN(scope, JSValue::encode(objectPrototypeToString(globalObject, thisObject)));

        // 4. Return the result of calling the [[Call]] internal method of func providing array as the this value and an empty arguments list.
        if (!isJSArray(thisObject) || callData.type != CallData::Type::Native || callData.native.function != arrayProtoFuncJoin)
            RELEASE_AND_RETURN(scope, JSValue::encode(call(globalObject, function, callData, thisObject, *vm.emptyList)));
    }

    ASSERT(isJSArray(thisValue));
    RELEASE_AND_RETURN(scope, JSValue::encode(asArray(thisValue)->fastToString(globalObject)));
}

static JSString* toLocaleString(JSGlobalObject* globalObject, JSValue value, JSValue locales, JSValue options)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue toLocaleStringMethod = value.get(globalObject, vm.propertyNames->toLocaleString);
    RETURN_IF_EXCEPTION(scope, { });

    auto callData = JSC::getCallDataInline(toLocaleStringMethod);
    if (callData.type == CallData::Type::None) {
        throwTypeError(globalObject, scope, "toLocaleString is not callable"_s);
        return { };
    }

    MarkedArgumentBuffer arguments;
    arguments.append(locales);
    arguments.append(options);
    ASSERT(!arguments.hasOverflowed());

    JSValue result = call(globalObject, toLocaleStringMethod, callData, value, arguments);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, result.toString(globalObject));
}

// https://tc39.es/ecma402/#sup-array.prototype.tolocalestring
JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncToLocaleString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());

    JSValue locales = callFrame->argument(0);
    JSValue options = callFrame->argument(1);

    // 1. Let array be ? ToObject(this value).
    JSObject* thisObject = thisValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    StringRecursionChecker checker(globalObject, thisObject);
    EXCEPTION_ASSERT(!scope.exception() || checker.earlyReturnValue());
    if (JSValue earlyReturnValue = checker.earlyReturnValue())
        return JSValue::encode(earlyReturnValue);

    // 2. Let len be ? ToLength(? Get(array, "length")).
    uint64_t length = toLength(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });

    // 3. Let separator be the String value for the list-separator String appropriate for
    // the host environment's current locale (this is derived in an implementation-defined way).
    const Latin1Character comma = ',';
    JSString* separator = jsSingleCharacterString(vm, comma);

    // 4. Let R be the empty String.
    if (!length)
        return JSValue::encode(jsEmptyString(vm));

    // 5. Let k be 0.
    JSValue element0 = thisObject->getIndex(globalObject, 0);
    RETURN_IF_EXCEPTION(scope, { });

    // 6. Repeat, while k < len,
    // 6.a. If k > 0, then
    // 6.a.i. Set R to the string-concatenation of R and separator.

    JSString* r = nullptr;
    if (element0.isUndefinedOrNull())
        r = jsEmptyString(vm);
    else {
        r = toLocaleString(globalObject, element0, locales, options);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // 8. Let k be 1.
    // 9. Repeat, while k < len
    // 9.e Increase k by 1..
    for (uint64_t k = 1; k < length; ++k) {
        // 6.b. Let nextElement be ? Get(array, ! ToString(k)).
        JSValue element = thisObject->getIndex(globalObject, k);
        RETURN_IF_EXCEPTION(scope, { });

        // c. If nextElement is not undefined or null, then
        JSString* next = nullptr;
        if (element.isUndefinedOrNull())
            next = jsEmptyString(vm);
        else {
            // i. Let S be ? ToString(? Invoke(nextElement, "toLocaleString", « locales, options »)).
            // ii. Set R to the string-concatenation of R and S.
            next = toLocaleString(globalObject, element, locales, options);
            RETURN_IF_EXCEPTION(scope, { });
        }

        // d. Increase k by 1.
        r = jsString(globalObject, r, separator, next);
        RETURN_IF_EXCEPTION(scope, { });
    }

    // 7. Return R.
    return JSValue::encode(r);
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
        JSValue element = thisObject->getIndex(globalObject, k);
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

JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncJoin, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 1. Let O be ? ToObject(this value).
    JSObject* thisObject = callFrame->thisValue().toThis(globalObject, ECMAMode::strict()).toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObject);
    if (!thisObject) [[unlikely]]
        return encodedJSValue();

    StringRecursionChecker checker(globalObject, thisObject);
    EXCEPTION_ASSERT(!scope.exception() || checker.earlyReturnValue());
    if (JSValue earlyReturnValue = checker.earlyReturnValue())
        return JSValue::encode(earlyReturnValue);

    // 2. Let len be ? ToLength(? Get(O, "length")).
    uint64_t length = toLength(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    // 3. If separator is undefined, let separator be the single-element String ",".
    JSValue separatorValue = callFrame->argument(0);
    if (separatorValue.isUndefined()) {
        const Latin1Character comma = ',';

        if (length > std::numeric_limits<unsigned>::max() || !canUseFastArrayJoin(thisObject)) [[unlikely]] {
            JSString* jsSeparator = jsSingleCharacterString(vm, comma);
            RETURN_IF_EXCEPTION(scope, encodedJSValue());

            RELEASE_AND_RETURN(scope, JSValue::encode(slowJoin(globalObject, thisObject, jsSeparator, length)));
        }

        unsigned unsignedLength = static_cast<unsigned>(length);
        ASSERT(static_cast<double>(unsignedLength) == length);

        RELEASE_AND_RETURN(scope, JSValue::encode(fastArrayJoin(globalObject, thisObject, span(comma), unsignedLength)));
    }

    // 4. Let sep be ? ToString(separator).
    JSString* jsSeparator = separatorValue.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (length > std::numeric_limits<unsigned>::max() || !canUseFastArrayJoin(thisObject)) [[unlikely]]
        RELEASE_AND_RETURN(scope, JSValue::encode(slowJoin(globalObject, thisObject, jsSeparator, length)));

    auto view = jsSeparator->view(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    RELEASE_AND_RETURN(scope, JSValue::encode(fastArrayJoin(globalObject, thisObject, view, length)));
}

inline EncodedJSValue createArrayIteratorObject(JSGlobalObject* globalObject, CallFrame* callFrame, IterationKind kind)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* thisObject = callFrame->thisValue().toThis(globalObject, ECMAMode::strict()).toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObject);
    UNUSED_PARAM(scope);
    if (!thisObject) [[unlikely]]
        return encodedJSValue();

    return JSValue::encode(JSArrayIterator::create(vm, globalObject->arrayIteratorStructure(), thisObject, jsNumber(static_cast<unsigned>(kind))));
}

JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncValues, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return createArrayIteratorObject(globalObject, callFrame, IterationKind::Values);
}

JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncEntries, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return createArrayIteratorObject(globalObject, callFrame, IterationKind::Entries);
}

JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncKeys, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return createArrayIteratorObject(globalObject, callFrame, IterationKind::Keys);
}

JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncPop, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());

    if (isJSArray(thisValue)) [[likely]]
        RELEASE_AND_RETURN(scope, JSValue::encode(asArray(thisValue)->pop(globalObject)));

    JSObject* thisObj = thisValue.toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObj);
    if (!thisObj) [[unlikely]]
        return encodedJSValue();
    uint64_t length = toLength(globalObject, thisObj);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (length == 0) {
        scope.release();
        setLength(globalObject, vm, thisObj, length);
        return JSValue::encode(jsUndefined());
    }

    static_assert(MAX_ARRAY_INDEX + 1 > MAX_ARRAY_INDEX);
    uint64_t index = length - 1;
    JSValue result = thisObj->get(globalObject, index);
    RETURN_IF_EXCEPTION(scope, { });
    bool success = thisObj->deleteProperty(globalObject, index);
    RETURN_IF_EXCEPTION(scope, { });
    if (!success) [[unlikely]] {
        throwTypeError(globalObject, scope, UnableToDeletePropertyError);
        return { };
    }

    scope.release();
    setLength(globalObject, vm, thisObj, index);
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncPush, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());

    if (isJSArray(thisValue) && callFrame->argumentCount() == 1) [[likely]] {
        JSArray* array = asArray(thisValue);
        scope.release();
        array->pushInline(globalObject, callFrame->uncheckedArgument(0));
        return JSValue::encode(jsNumber(array->length()));
    }
    
    JSObject* thisObj = thisValue.toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObj);
    if (!thisObj) [[unlikely]]
        return encodedJSValue();
    uint64_t length = toLength(globalObject, thisObj);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    unsigned argCount = callFrame->argumentCount();

    if (length + argCount > maxSafeIntegerAsUInt64()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "push cannot produce an array of length larger than (2 ** 53) - 1"_s);

    for (unsigned n = 0; n < argCount; n++) {
        thisObj->putByIndexInline(globalObject, length + n, callFrame->uncheckedArgument(n), true);
        RETURN_IF_EXCEPTION(scope, { });
    }
    
    uint64_t newLength = length + argCount;
    scope.release();
    setLength(globalObject, vm, thisObj, newLength);
    return JSValue::encode(jsNumber(newLength));
}

JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncReverse, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* thisObject = callFrame->thisValue().toThis(globalObject, ECMAMode::strict()).toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObject);
    if (!thisObject) [[unlikely]]
        return encodedJSValue();

    uint64_t length = toLength(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    thisObject->ensureWritable(vm);

    switch (thisObject->indexingType()) {
    case ALL_CONTIGUOUS_INDEXING_TYPES:
    case ALL_INT32_INDEXING_TYPES: {
        auto& butterfly = *thisObject->butterfly();
        if (length > butterfly.publicLength())
            break;
        auto data = butterfly.contiguous().data();
        if (containsHole(data, static_cast<uint32_t>(length)) && holesMustForwardToPrototype(thisObject))
            break;
        std::reverse(data, data + length);
        if (!hasInt32(thisObject->indexingType()))
            vm.writeBarrier(thisObject);
        return JSValue::encode(thisObject);
    }
    case ALL_DOUBLE_INDEXING_TYPES: {
        auto& butterfly = *thisObject->butterfly();
        if (length > butterfly.publicLength())
            break;
        auto data = butterfly.contiguousDouble().data();
        if (containsHole(data, static_cast<uint32_t>(length)) && holesMustForwardToPrototype(thisObject))
            break;
        std::reverse(data, data + length);
        return JSValue::encode(thisObject);
    }
    case ALL_ARRAY_STORAGE_INDEXING_TYPES: {
        auto& storage = *thisObject->butterfly()->arrayStorage();
        if (length > storage.vectorLength())
            break;
        if (storage.hasHoles() && holesMustForwardToPrototype(thisObject))
            break;
        auto data = storage.vector().data();
        std::reverse(data, data + length);
        vm.writeBarrier(thisObject);
        return JSValue::encode(thisObject);
    }
    }

    uint64_t middle = length / 2;
    for (uint64_t lower = 0; lower < middle; lower++) {
        uint64_t upper = length - lower - 1;
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

        if (!lowerExists && !upperExists) {
            // Spec says to do nothing when neither lower nor upper exist.
            continue;
        }

        if (upperExists) {
            thisObject->putByIndexInline(globalObject, lower, upperValue, true);
            RETURN_IF_EXCEPTION(scope, encodedJSValue());
        } else {
            bool success = thisObject->deleteProperty(globalObject, lower);
            RETURN_IF_EXCEPTION(scope, encodedJSValue());
            if (!success) [[unlikely]] {
                throwTypeError(globalObject, scope, UnableToDeletePropertyError);
                return encodedJSValue();
            }
        }

        if (lowerExists) {
            thisObject->putByIndexInline(globalObject, upper, lowerValue, true);
            RETURN_IF_EXCEPTION(scope, encodedJSValue());
        } else {
            bool success = thisObject->deleteProperty(globalObject, upper);
            RETURN_IF_EXCEPTION(scope, encodedJSValue());
            if (!success) [[unlikely]] {
                throwTypeError(globalObject, scope, UnableToDeletePropertyError);
                return encodedJSValue();
            }
        }
    }
    return JSValue::encode(thisObject);
}

JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncShift, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* thisObj = callFrame->thisValue().toThis(globalObject, ECMAMode::strict()).toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObj);
    if (!thisObj) [[unlikely]]
        return encodedJSValue();
    uint64_t length = toLength(globalObject, thisObj);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (length == 0) {
        scope.release();
        setLength(globalObject, vm, thisObj, length);
        return JSValue::encode(jsUndefined());
    }

    JSValue result = thisObj->getIndex(globalObject, 0);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    shift<JSArray::ShiftCountForShift>(globalObject, thisObj, 0, 1, 0, length);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    scope.release();
    setLength(globalObject, vm, thisObj, length - 1);
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncSlice, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    // https://tc39.github.io/ecma262/#sec-array.prototype.slice
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* thisObj = callFrame->thisValue().toThis(globalObject, ECMAMode::strict()).toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObj);
    if (!thisObj) [[unlikely]]
        return { };
    uint64_t length = toLength(globalObject, thisObj);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t begin = argumentClampedIndexFromStartOrEnd<RelativeNegativeIndex::Yes>(globalObject, callFrame->argument(0), length);
    RETURN_IF_EXCEPTION(scope, { });
    uint64_t end = argumentClampedIndexFromStartOrEnd<RelativeNegativeIndex::Yes>(globalObject, callFrame->argument(1), length, length);
    RETURN_IF_EXCEPTION(scope, { });
    if (end < begin)
        end = begin;

    std::pair<SpeciesConstructResult, JSObject*> speciesResult = speciesConstructArray(globalObject, thisObj, end - begin);
    // We can only get an exception if we call some user function.
    EXCEPTION_ASSERT(!!scope.exception() == (speciesResult.first == SpeciesConstructResult::Exception));
    if (speciesResult.first == SpeciesConstructResult::Exception) [[unlikely]]
        return { };

    if (speciesResult.first == SpeciesConstructResult::FastPath) [[likely]] {
        JSArray* result = JSArray::fastSlice(globalObject, thisObj, begin, end - begin);
        if (result) {
            scope.assertNoExceptionExceptTermination();
            return JSValue::encode(result);
        }
        RETURN_IF_EXCEPTION(scope, { });
    }

    JSObject* result;
    if (speciesResult.first == SpeciesConstructResult::CreatedObject)
        result = speciesResult.second;
    else {
        if (end - begin > std::numeric_limits<uint32_t>::max()) [[unlikely]] {
            throwRangeError(globalObject, scope, LengthExceededTheMaximumArrayLengthError);
            return encodedJSValue();
        }
        result = constructEmptyArray(globalObject, nullptr, static_cast<uint32_t>(end - begin));
        RETURN_IF_EXCEPTION(scope, { });
    }

    // Document that we need to keep the source array alive until after anything
    // that can GC (e.g. allocating the result array).
    thisObj->use();

    uint64_t n = 0;
    for (uint64_t k = begin; k < end; k++, n++) {
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

using SortJSValueVector = MarkedVector<JSValue, 64, RecordOverflow>;
using SortEntryVector = Vector<std::tuple<JSValue, String>>;

static ALWAYS_INLINE std::tuple<uint64_t, IndexingType, std::span<EncodedJSValue>> sortCompact(JSGlobalObject* globalObject, JSObject* thisObject, uint64_t length, SortJSValueVector& compactedRoot)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    uint64_t undefinedCount = 0;

    if (isJSArray(thisObject) && !holesMustForwardToPrototype(thisObject)) [[likely]] {
        IndexingType indexingType = thisObject->indexingType();
        switch (indexingType) {
        case ALL_INT32_INDEXING_TYPES: {
            auto& butterfly = *thisObject->butterfly();
            unsigned butterflyLength = butterfly.publicLength();
            auto data = butterfly.contiguous().data();
            unsigned count = 0;
            compactedRoot.fill(vm, butterflyLength, [&](JSValue* buffer) {
                for (unsigned i = 0; i < butterflyLength; ++i) {
                    if (JSValue value = data[i].get(); value) [[likely]]
                        buffer[count++] = value;
                }
            });
            if (compactedRoot.hasOverflowed()) [[unlikely]] {
                throwOutOfMemoryError(globalObject, scope);
                return { };
            }
            return std::tuple { 0, ArrayWithInt32, std::span { compactedRoot.data(), count } };
        }
        case ALL_CONTIGUOUS_INDEXING_TYPES: {
            auto& butterfly = *thisObject->butterfly();
            unsigned butterflyLength = butterfly.publicLength();
            auto data = butterfly.contiguous().data();
            unsigned count = 0;
            compactedRoot.fill(vm, butterflyLength, [&](JSValue* buffer) {
                for (unsigned i = 0; i < butterflyLength; ++i) {
                    if (JSValue value = data[i].get(); value) [[likely]] {
                        if (!value.isUndefined()) [[likely]]
                            buffer[count++] = value;
                        else
                            ++undefinedCount;
                    }
                }
            });
            if (compactedRoot.hasOverflowed()) [[unlikely]] {
                throwOutOfMemoryError(globalObject, scope);
                return { };
            }
            return std::tuple { undefinedCount, ArrayWithContiguous, std::span { compactedRoot.data(), count } };
        }
        case ALL_DOUBLE_INDEXING_TYPES: {
            auto& butterfly = *thisObject->butterfly();
            unsigned butterflyLength = butterfly.publicLength();
            auto data = butterfly.contiguousDouble().data();
            unsigned count = 0;
            compactedRoot.fill(vm, butterflyLength, [&](JSValue* buffer) {
                for (unsigned i = 0; i < butterflyLength; ++i) {
                    double number = data[i];
                    if (!isHole(number)) [[likely]]
                        buffer[count++] = jsDoubleNumber(number);
                }
            });
            if (compactedRoot.hasOverflowed()) [[unlikely]] {
                throwOutOfMemoryError(globalObject, scope);
                return { };
            }
            return std::tuple { 0, ArrayWithDouble, std::span { compactedRoot.data(), count } };
        }
        default:
            break;
        }
    }

    for (uint64_t index = 0; index < length; ++index) {
        JSValue value = thisObject->getIfPropertyExists(globalObject, index);
        RETURN_IF_EXCEPTION(scope, { });
        if (value) {
            if (value.isUndefined())
                ++undefinedCount;
            else {
                compactedRoot.append(value);
                if (compactedRoot.hasOverflowed()) [[unlikely]] {
                    throwOutOfMemoryError(globalObject, scope);
                    return { };
                }
            }
        }
    }

    return std::tuple { undefinedCount, ArrayWithContiguous, std::span { compactedRoot.data(), compactedRoot.size() } };
}

static unsigned sortBucketSort(std::span<EncodedJSValue> sorted, unsigned dst, SortEntryVector& bucket, unsigned depth)
{
    if (bucket.size() < 32 || depth > 32) {
        std::ranges::sort(bucket, WTF::codePointCompareLessThan, [](const auto& element) {
            return std::get<1>(element);
        });
        for (auto& entry : bucket)
            sorted[dst++] = JSValue::encode(std::get<0>(entry));
        return dst;
    }

    StdMap<char16_t, SortEntryVector> buckets;
    for (const auto& entry : bucket) {
        if (std::get<1>(entry).length() == depth) {
            sorted[dst++] = JSValue::encode(std::get<0>(entry));
            continue;
        }

        char16_t character = std::get<1>(entry).characterAt(depth);
        buckets.insert(std::pair { character, SortEntryVector { } }).first->second.append(entry);
    }

    for (auto& entries : buckets)
        dst = sortBucketSort(sorted, dst, entries.second, depth + 1);

    return dst;
}

static ALWAYS_INLINE std::span<EncodedJSValue> sortStableSort(JSGlobalObject* globalObject, std::span<EncodedJSValue> sorted, std::span<EncodedJSValue> compacted, JSObject* comparator)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto callData = JSC::getCallDataInline(comparator);
    ASSERT(callData.type != CallData::Type::None);

    if (callData.type == CallData::Type::JS) [[likely]] {
        CachedCall cachedCall(globalObject, jsCast<JSFunction*>(comparator), 2);
        RETURN_IF_EXCEPTION(scope, sorted);
        RELEASE_AND_RETURN(scope, arrayStableSort(vm, compacted, sorted, [&](auto left, auto right) ALWAYS_INLINE_LAMBDA {
            auto scope = DECLARE_THROW_SCOPE(vm);

            JSValue jsResult = cachedCall.callWithArguments(globalObject, jsUndefined(), JSValue::decode(left), JSValue::decode(right));
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, false);

            RELEASE_AND_RETURN(scope, coerceComparatorResultToBoolean(globalObject, jsResult));
        }));
    }

    MarkedArgumentBuffer args;
    RELEASE_AND_RETURN(scope, arrayStableSort(vm, compacted, sorted, [&](auto left, auto right) ALWAYS_INLINE_LAMBDA {
        auto scope = DECLARE_THROW_SCOPE(vm);

        args.clear();

        args.append(JSValue::decode(left));
        args.append(JSValue::decode(right));
        if (args.hasOverflowed()) [[unlikely]] {
            throwOutOfMemoryError(globalObject, scope);
            return false;
        }

        JSValue jsResult = call(globalObject, comparator, callData, jsUndefined(), args);
        RETURN_IF_EXCEPTION(scope, false);

        RELEASE_AND_RETURN(scope, coerceComparatorResultToBoolean(globalObject, jsResult));
    }));
}

static ALWAYS_INLINE void sortCommit(JSGlobalObject* globalObject, JSObject* thisObject, uint64_t length, IndexingType indexingType, std::span<const EncodedJSValue> sorted, uint64_t undefinedCount)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned index = 0;

    bool appended = false;
    if (isJSArray(thisObject)) [[likely]] {
        appended = jsCast<JSArray*>(thisObject)->appendMemcpy(globalObject, vm, 0, indexingType, sorted);
        RETURN_IF_EXCEPTION(scope, void());
    }

    if (!appended) [[unlikely]] {
        for (EncodedJSValue encodedValue : sorted) {
            JSValue value = JSValue::decode(encodedValue);
            constexpr bool shouldThrow = true;
            thisObject->putByIndexInline(globalObject, index++, value, shouldThrow);
            RETURN_IF_EXCEPTION(scope, void());
        }
    } else {
        index = sorted.size();
        if (index == length) [[likely]]
            return;
    }

    uint64_t index64 = index;
    uint64_t undefinedMax = static_cast<uint64_t>(sorted.size()) + undefinedCount;
    for (; index64 < undefinedMax; ++index64) {
        constexpr bool shouldThrow = true;
        thisObject->putByIndexInline(globalObject, index64, jsUndefined(), shouldThrow);
        RETURN_IF_EXCEPTION(scope, void());
    }

    for (; index64 < length; ++index64) {
        bool deleted = thisObject->deleteProperty(globalObject, index64);
        RETURN_IF_EXCEPTION(scope, void());
        if (!deleted) [[unlikely]] {
            throwTypeError(globalObject, scope, UnableToDeletePropertyError);
            return;
        }
    }
}

static ALWAYS_INLINE void sortImpl(JSGlobalObject* globalObject, JSObject* thisObject, uint64_t length, JSValue comparatorValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // For compatibility with Firefox and Chrome, do nothing observable
    // to the target array if it has 0 or 1 sortable properties.
    if (length < 2)
        return;

    bool isStringSort = comparatorValue.isUndefined();

    SortJSValueVector compactedRoot;
    SortJSValueVector sortedRoot;

    auto [undefinedCount, indexingType, compacted] = sortCompact(globalObject, thisObject, length, compactedRoot);
    RETURN_IF_EXCEPTION(scope, void());

    sortedRoot.fill(vm, compacted.size(), [](JSValue*) { });
    if (sortedRoot.hasOverflowed()) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return;
    }
    std::span<EncodedJSValue> sorted { sortedRoot.data(), sortedRoot.size() };
    std::span<EncodedJSValue> dest;
    if (isStringSort) {
        SortEntryVector entries; // Keep in mind that all JSValues are also stored in SortJSValueVector (compacted). Thus, we do not need to keep them marked here.
        entries.reserveInitialCapacity(compacted.size());
        for (EncodedJSValue encodedValue : compacted) {
            JSValue value = JSValue::decode(encodedValue);
            String string = value.toWTFString(globalObject);
            RETURN_IF_EXCEPTION(scope, void());
            entries.append(std::tuple { value, WTFMove(string) });
        }
        sortBucketSort(sorted, 0, entries, 0);
        dest = sorted;
    } else {
        dest = sortStableSort(globalObject, sorted, compacted, asObject(comparatorValue));
        RETURN_IF_EXCEPTION(scope, void());
    }

    scope.release();
    sortCommit(globalObject, thisObject, length, indexingType, dest, undefinedCount);
}

JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncSort, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    // https://tc39.es/ecma262/#sec-array.prototype.sort

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue comparatorValue = callFrame->argument(0);
    if (!comparatorValue.isUndefined() && !comparatorValue.isCallable()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Array.prototype.sort requires the comparator argument to be a function or undefined"_s);

    JSObject* thisObject = callFrame->thisValue().toThis(globalObject, ECMAMode::strict()).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t length = toLength(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });

    scope.release();
    sortImpl(globalObject, thisObject, length, comparatorValue);

    return JSValue::encode(thisObject);
}

JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncSplice, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    // 15.4.4.12

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* thisObj = callFrame->thisValue().toThis(globalObject, ECMAMode::strict()).toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObj);
    if (!thisObj) [[unlikely]]
        return encodedJSValue();
    uint64_t length = toLength(globalObject, thisObj);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (!callFrame->argumentCount()) {
        std::pair<SpeciesConstructResult, JSObject*> speciesResult = speciesConstructArray(globalObject, thisObj, 0);
        EXCEPTION_ASSERT(!!scope.exception() == (speciesResult.first == SpeciesConstructResult::Exception));
        if (speciesResult.first == SpeciesConstructResult::Exception) [[unlikely]]
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

    uint64_t actualStart = argumentClampedIndexFromStartOrEnd<RelativeNegativeIndex::Yes>(globalObject, callFrame->argument(0), length);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    uint64_t itemCount = 0;
    uint64_t actualDeleteCount = 0;
    if (callFrame->argumentCount() == 1)
        actualDeleteCount = length - actualStart;
    else if (callFrame->argumentCount() > 1) {
        itemCount = callFrame->argumentCount() - 2;
        actualDeleteCount = argumentClampedIndexFromStartOrEnd<RelativeNegativeIndex::No>(globalObject, callFrame->uncheckedArgument(1), length - actualStart);
        RETURN_IF_EXCEPTION(scope, { });
    }
    ASSERT(callFrame->argumentCount() || (!itemCount && !actualDeleteCount));

    if (length - actualDeleteCount + itemCount > maxSafeIntegerAsUInt64()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Splice cannot produce an array of length larger than (2 ** 53) - 1"_s);

    std::pair<SpeciesConstructResult, JSObject*> speciesResult = speciesConstructArray(globalObject, thisObj, actualDeleteCount);
    EXCEPTION_ASSERT(!!scope.exception() == (speciesResult.first == SpeciesConstructResult::Exception));
    if (speciesResult.first == SpeciesConstructResult::Exception)
        return JSValue::encode(jsUndefined());

    JSObject* result = nullptr;
    if (speciesResult.first == SpeciesConstructResult::FastPath) [[likely]] {
        result = JSArray::fastSlice(globalObject, thisObj, actualStart, actualDeleteCount);
        RETURN_IF_EXCEPTION(scope, { });
    }

    if (!result) {
        if (speciesResult.first == SpeciesConstructResult::CreatedObject)
            result = speciesResult.second;
        else {
            if (actualDeleteCount > std::numeric_limits<uint32_t>::max()) [[unlikely]] {
                throwRangeError(globalObject, scope, LengthExceededTheMaximumArrayLengthError);
                return encodedJSValue();
            }
            result = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithUndecided), static_cast<uint32_t>(actualDeleteCount));
            if (!result) [[unlikely]] {
                throwOutOfMemoryError(globalObject, scope);
                return encodedJSValue();
            }
        }
        for (uint64_t k = 0; k < actualDeleteCount; ++k) {
            JSValue v = getProperty(globalObject, thisObj, k + actualStart);
            RETURN_IF_EXCEPTION(scope, encodedJSValue());
            if (!v) [[unlikely]]
                continue;
            result->putDirectIndex(globalObject, k, v, 0, PutDirectIndexShouldThrow);
            RETURN_IF_EXCEPTION(scope, encodedJSValue());
        }
        setLength(globalObject, vm, result, actualDeleteCount);
        RETURN_IF_EXCEPTION(scope, { });
    }

    if (itemCount < actualDeleteCount) {
        shift<JSArray::ShiftCountForSplice>(globalObject, thisObj, actualStart, actualDeleteCount, itemCount, length);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    } else if (itemCount > actualDeleteCount) {
        unshift(globalObject, thisObj, actualStart, actualDeleteCount, itemCount, length);
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

JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncUnShift, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    // 15.4.4.13

    JSObject* thisObj = callFrame->thisValue().toThis(globalObject, ECMAMode::strict()).toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObj);
    if (!thisObj) [[unlikely]]
        return encodedJSValue();
    uint64_t length = toLength(globalObject, thisObj);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    unsigned nrArgs = callFrame->argumentCount();
    if (nrArgs) {
        if (length + nrArgs > maxSafeIntegerAsUInt64()) [[unlikely]]
            return throwVMTypeError(globalObject, scope, "unshift cannot produce an array of length larger than (2 ** 53) - 1"_s);
        unshift(globalObject, thisObj, 0, 0, nrArgs, length);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    for (unsigned k = 0; k < nrArgs; ++k) {
        thisObj->putByIndexInline(globalObject, k, callFrame->uncheckedArgument(k), true);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }
    uint64_t newLength = length + nrArgs;
    scope.release();
    setLength(globalObject, vm, thisObj, newLength);
    return JSValue::encode(jsNumber(newLength));
}

enum class IndexOfDirection { Forward, Backward };
template<IndexOfDirection direction>
ALWAYS_INLINE JSValue fastIndexOf(JSGlobalObject* globalObject, VM& vm, JSArray* array, uint64_t length64, JSValue searchElement, uint64_t index64)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool canDoFastPath = array->canDoFastIndexedAccess()
        && array->getArrayLength() == length64 // The effects in getting `index` could have changed the length of this array.
        && static_cast<uint32_t>(index64) == index64;
    if (!canDoFastPath)
        return JSValue();

    uint32_t length = static_cast<uint32_t>(length64);
    uint32_t index = static_cast<uint32_t>(index64);

    if constexpr (direction == IndexOfDirection::Forward) {
        if (index >= length)
            return jsNumber(-1);
    }

    switch (array->indexingType()) {
    case ALL_INT32_INDEXING_TYPES: {
        auto& butterfly = *array->butterfly();
        auto data = butterfly.contiguous().data();

        int32_t int32Value = 0;
        if (searchElement.isInt32AsAnyInt())
            int32Value = searchElement.asInt32AsAnyInt();
        else if (!searchElement.isNumber() || searchElement.asNumber() != 0.0) [[unlikely]]
            return jsNumber(-1);
        JSValue searchInt32 = jsNumber(int32Value);

        if constexpr (direction == IndexOfDirection::Forward) {
            EncodedJSValue encodedSearchElement = JSValue::encode(searchInt32);
            auto* result = std::bit_cast<const WriteBarrier<Unknown>*>(WTF::find64(std::bit_cast<const uint64_t*>(data + index), encodedSearchElement, length - index));
            if (result)
                return jsNumber(result - data);
        } else {
            do {
                ASSERT(index < length);
                // Array#lastIndexOf uses `===` semantics (not UncheckedKeyHashMap isEqual semantics).
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

        if constexpr (direction == IndexOfDirection::Forward) {
            if (searchElement.isObject()) {
                auto* result = std::bit_cast<const WriteBarrier<Unknown>*>(WTF::find64(std::bit_cast<const uint64_t*>(data + index), JSValue::encode(searchElement), length - index));
                if (result)
                    return jsNumber(result - data);
                return jsNumber(-1);
            }

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
        if constexpr (direction == IndexOfDirection::Forward) {
            for (; index < length; ++index) {
                // Array#indexOf uses `===` semantics (not UncheckedKeyHashMap isEqual semantics).
                // And the hole never matches since it is NaN.
                if (data[index] == searchNumber)
                    return jsNumber(index);
            }
        } else {
            do {
                ASSERT(index < length);
                // Array#lastIndexOf uses `===` semantics (not UncheckedKeyHashMap isEqual semantics).
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

JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncIndexOf, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 15.4.4.14
    JSObject* thisObject = callFrame->thisValue().toThis(globalObject, ECMAMode::strict()).toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObject);
    if (!thisObject) [[unlikely]]
        return { };
    uint64_t length = toLength(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });
    if (!length)
        return JSValue::encode(jsNumber(-1));

    uint64_t index = argumentClampedIndexFromStartOrEnd<RelativeNegativeIndex::Yes>(globalObject, callFrame->argument(1), length);
    RETURN_IF_EXCEPTION(scope, { });
    JSValue searchElement = callFrame->argument(0);

    if (isJSArray(thisObject)) [[likely]] {
        JSArray* array = asArray(thisObject);
        Butterfly* butterfly = array->butterfly();
        if (isCopyOnWrite(array->indexingMode()) && JSCellButterfly::isOnlyAtomStringsStructure(vm, butterfly) && searchElement.isString()) {
            auto search = asString(searchElement)->toAtomString(globalObject);
            RETURN_IF_EXCEPTION(scope, { });

            JSValue result = jsNumber(-1);
            if (vm.atomStringToJSStringMap.contains(search.data)) {
                auto data = butterfly->contiguous().data();
                for (unsigned i = index; i < length; ++i) {
                    JSValue value = data[i].get();
                    if (asString(value)->getValueImpl() == search.data) {
                        result = jsNumber(i);
                        break;
                    }
                }
            }

#if ASSERT_ENABLED
            JSValue expected = fastIndexOf<IndexOfDirection::Forward>(globalObject, vm, array, length, searchElement, index);
            RETURN_IF_EXCEPTION(scope, { });
            if (expected)
                ASSERT(expected.asNumber() == result.asNumber());
#endif
            return JSValue::encode(result);
        }

        JSValue result = fastIndexOf<IndexOfDirection::Forward>(globalObject, vm, array, length, searchElement, index);
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

JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncLastIndexOf, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 15.4.4.15
    JSObject* thisObject = callFrame->thisValue().toThis(globalObject, ECMAMode::strict()).toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObject);
    if (!thisObject) [[unlikely]]
        return { };
    uint64_t length = toLength(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });
    if (!length)
        return JSValue::encode(jsNumber(-1));

    uint64_t index = length - 1;
    if (callFrame->argumentCount() >= 2) {
        JSValue fromValue = callFrame->uncheckedArgument(1);
        if (fromValue.isInt32()) [[likely]] {
            int64_t fromInt = fromValue.asInt32();
            if (fromInt < 0) {
                fromInt += length;
                if (fromInt < 0)
                    return JSValue::encode(jsNumber(-1));
            }
            uint64_t fromUint = static_cast<uint64_t>(fromInt);
            if (fromUint < length)
                index = fromUint;
        } else {
            double fromDouble = fromValue.toIntegerOrInfinity(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            if (fromDouble < 0) {
                fromDouble += length;
                if (fromDouble < 0)
                    return JSValue::encode(jsNumber(-1));
            }
            if (fromDouble < length)
                index = static_cast<uint64_t>(fromDouble);
        }
    }

    JSValue searchElement = callFrame->argument(0);

    if (isJSArray(thisObject)) [[likely]] {
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

static JSArray* concatAppendOne(JSGlobalObject* globalObject, VM& vm, JSArray* first, JSValue second)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(!isJSArray(second));
    ASSERT(!shouldUseSlowPut(first->indexingType()));
    Butterfly* firstButterfly = first->butterfly();
    unsigned firstArraySize = firstButterfly->publicLength();

    CheckedUint32 checkedResultSize = firstArraySize;
    checkedResultSize += 1;
    if (checkedResultSize.hasOverflowed()) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    // This code still can see concat-spreadable items. It is ProxyObject and DerivedArray.
    // We should not use `isArray` check here since it is side-effectful for ProxyObject. Let's just bail out if it is ProxyObject or DerivedArray.
    if (second.isObject()) {
        JSType type = asObject(second)->type();
        if (type == ProxyObjectType || type == DerivedArrayType) [[unlikely]]
            return { };
    }

    unsigned resultSize = checkedResultSize;
    bool allowPromotion = false;
    IndexingType type = first->mergeIndexingTypeForCopying(indexingTypeForValue(second) | IsArray, allowPromotion);
    
    if (type == NonArray)
        type = first->indexingType();

    Structure* resultStructure = globalObject->arrayStructureForIndexingTypeDuringAllocation(type);
    JSArray* result = JSArray::tryCreate(vm, resultStructure, resultSize);
    if (!result) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    bool success = result->appendMemcpy(globalObject, vm, 0, first);
    EXCEPTION_ASSERT(!scope.exception() || !success);
    if (!success) {
        RETURN_IF_EXCEPTION(scope, { });

        bool success = moveArrayElements<ArrayFillMode::Empty>(globalObject, vm, result, 0, first, firstArraySize);
        EXCEPTION_ASSERT(!scope.exception() == success);
        if (!success) [[unlikely]]
            return { };
    }

    scope.release();
    result->putDirectIndex(globalObject, firstArraySize, second);
    return result;
}

static JSArray* concatAppendArray(JSGlobalObject* globalObject, VM& vm, JSArray* firstArray, JSArray* secondArray)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    Butterfly* firstButterfly = firstArray->butterfly();
    Butterfly* secondButterfly = secondArray->butterfly();

    unsigned firstArraySize = firstButterfly->publicLength();
    unsigned secondArraySize = secondButterfly->publicLength();

    CheckedUint32 checkedResultSize = firstArraySize;
    checkedResultSize += secondArraySize;

    if (checkedResultSize.hasOverflowed()) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    unsigned resultSize = checkedResultSize;
    IndexingType firstType = firstArray->indexingType();
    IndexingType secondType = secondArray->indexingType();
    bool allowPromotion = true;
    IndexingType type = firstArray->mergeIndexingTypeForCopying(secondType, allowPromotion);
    if (type == NonArray || !firstArray->canFastCopy(secondArray) || resultSize >= MIN_SPARSE_ARRAY_INDEX) {
        JSArray* result = constructEmptyArray(globalObject, nullptr, resultSize);
        RETURN_IF_EXCEPTION(scope, { });

        bool success = moveArrayElements<ArrayFillMode::Empty>(globalObject, vm, result, 0, firstArray, firstArraySize);
        EXCEPTION_ASSERT(!scope.exception() == success);
        if (!success) [[unlikely]]
            return { };
        success = moveArrayElements<ArrayFillMode::Empty>(globalObject, vm, result, firstArraySize, secondArray, secondArraySize);
        EXCEPTION_ASSERT(!scope.exception() == success);
        if (!success) [[unlikely]]
            return { };

        return result;
    }

    if (!globalObject->isHavingABadTime()) [[likely]] {
        if (!resultSize)
            RELEASE_AND_RETURN(scope, constructEmptyArray(globalObject, nullptr));

        if (!secondArraySize) {
            if (isCopyOnWrite(firstArray->indexingMode()))
                return JSArray::createWithButterfly(vm, nullptr, globalObject->originalArrayStructureForIndexingType(firstArray->indexingMode()), firstArray->butterfly());
        } else if (!firstArraySize) {
            if (isCopyOnWrite(secondArray->indexingMode()))
                return JSArray::createWithButterfly(vm, nullptr, globalObject->originalArrayStructureForIndexingType(secondArray->indexingMode()), secondArray->butterfly());
        }
    }

    Structure* resultStructure = globalObject->arrayStructureForIndexingTypeDuringAllocation(type);
    if (hasAnyArrayStorage(resultStructure->indexingType())) [[unlikely]]
        return { };

    ASSERT(!globalObject->isHavingABadTime());
    auto vectorLength = Butterfly::optimalContiguousVectorLength(resultStructure, resultSize);
    if (vectorLength > MAX_STORAGE_VECTOR_LENGTH) [[unlikely]]
        return { };

    ASSERT(!resultStructure->outOfLineCapacity());
    void* memory = vm.auxiliarySpace().allocate(vm, Butterfly::totalSize(0, 0, true, vectorLength * sizeof(EncodedJSValue)), nullptr, AllocationFailureMode::ReturnNull);
    if (!memory) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }
    auto* butterfly = Butterfly::fromBase(memory, 0, 0);
    butterfly->setVectorLength(vectorLength);
    butterfly->setPublicLength(resultSize);

    // We can use memcpy / memset since butterfly is not connected to cells yet.
    if (type == ArrayWithDouble) {
        double* buffer = butterfly->contiguousDouble().data();
        if (firstType == ArrayWithDouble)
            copyArrayElements<ArrayFillMode::Empty, NeedsGCSafeOps::No>(buffer, 0, firstButterfly->contiguousDouble().data(), 0, firstArraySize, firstType);
        else
            copyArrayElements<ArrayFillMode::Empty, NeedsGCSafeOps::No>(buffer, 0, firstButterfly->contiguous().data(), 0, firstArraySize, firstType);
        if (secondType == ArrayWithDouble)
            copyArrayElements<ArrayFillMode::Empty, NeedsGCSafeOps::No>(buffer, firstArraySize, secondButterfly->contiguousDouble().data(), 0, secondArraySize, secondType);
        else
            copyArrayElements<ArrayFillMode::Empty, NeedsGCSafeOps::No>(buffer, firstArraySize, secondButterfly->contiguous().data(), 0, secondArraySize, secondType);
    } else if (type != ArrayWithUndecided) {
        WriteBarrier<Unknown>* buffer = butterfly->contiguous().data();
        copyArrayElements<ArrayFillMode::Empty, NeedsGCSafeOps::No>(buffer, 0, firstButterfly->contiguous().data(), 0, firstArraySize, firstType);
        copyArrayElements<ArrayFillMode::Empty, NeedsGCSafeOps::No>(buffer, firstArraySize, secondButterfly->contiguous().data(), 0, secondArraySize, secondType);
    }

    Butterfly::clearRange(type, butterfly, resultSize, vectorLength);
    return JSArray::createWithButterfly(vm, nullptr, resultStructure, butterfly);
}

JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncConcat, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();

    if (!callFrame->argumentCount()) {
        if (isJSArray(thisValue)) [[likely]] {
            auto* array = jsCast<JSArray*>(thisValue);
            if (arrayMissingIsConcatSpreadable(vm, array) && arraySpeciesWatchpointIsValid(vm, array)) [[likely]] {
                JSArray* result = tryCloneArrayFromFast<ArrayFillMode::Empty>(globalObject, array);
                RETURN_IF_EXCEPTION(scope, { });
                if (result) [[likely]]
                    return JSValue::encode(result);
            }
        }
    } else if (callFrame->argumentCount() == 1) {
        JSValue argumentValue = callFrame->uncheckedArgument(0);
        if (isJSArray(thisValue)) [[likely]] {
            auto* firstArray = jsCast<JSArray*>(thisValue);
            if (arrayMissingIsConcatSpreadable(vm, firstArray) && arraySpeciesWatchpointIsValid(vm, firstArray)) [[likely]] {
                // This code assumes that neither array has set Symbol.isConcatSpreadable. If the first array
                // has indexed accessors then one of those accessors might change the value of Symbol.isConcatSpreadable
                // on the second argument.
                if (!shouldUseSlowPut(firstArray->indexingType())) [[likely]] {
                    if (!argumentValue.isObject()) {
                        auto* result = concatAppendOne(globalObject, vm, firstArray, argumentValue);
                        RETURN_IF_EXCEPTION(scope, { });
                        if (result) [[likely]]
                            return JSValue::encode(result);
                    } else {
                        auto* argumentObject = jsCast<JSObject*>(argumentValue);
                        if (arrayMissingIsConcatSpreadable(vm, argumentObject)) [[likely]] {
                            if (!isJSArray(argumentObject)) {
                                auto* result = concatAppendOne(globalObject, vm, firstArray, argumentValue);
                                RETURN_IF_EXCEPTION(scope, { });
                                if (result) [[likely]]
                                    return JSValue::encode(result);
                            } else {
                                auto* result = concatAppendArray(globalObject, vm, firstArray, jsCast<JSArray*>(argumentValue));
                                RETURN_IF_EXCEPTION(scope, { });
                                if (result) [[likely]]
                                    return JSValue::encode(result);
                            }
                        }
                    }
                }
            }
        }
    }

    thisValue = thisValue.toThis(globalObject, ECMAMode::strict());
    RETURN_IF_EXCEPTION(scope, { });
    if (thisValue.isUndefinedOrNull())
        return throwVMTypeError(globalObject, scope, "Array.prototype.concat requires that |this| not be null or undefined"_s);
    JSObject* currentElementObject = thisValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    std::pair<SpeciesConstructResult, JSObject*> speciesResult = speciesConstructArray(globalObject, currentElementObject, 0);
    EXCEPTION_ASSERT(!!scope.exception() == (speciesResult.first == SpeciesConstructResult::Exception));
    if (speciesResult.first == SpeciesConstructResult::Exception) [[unlikely]]
        return { };

    JSObject* result;
    if (speciesResult.first == SpeciesConstructResult::CreatedObject)
        result = speciesResult.second;
    else {
        result = constructEmptyArray(globalObject, nullptr);
        RETURN_IF_EXCEPTION(scope, { });
    }

    uint64_t resultIndex = 0;
    uint64_t argumentIndex = 0;
    JSValue currentElement = currentElementObject;
    for (;;) {
        bool isSpreadable = false;
        if (currentElement.isObject()) {
            auto* object = asObject(currentElement);
            if (arrayMissingIsConcatSpreadable(vm, object)) {
                isSpreadable = JSC::isArray(globalObject, object);
                RETURN_IF_EXCEPTION(scope, { });
            } else {
                JSValue spreadable = object->get(globalObject, vm.propertyNames->isConcatSpreadableSymbol);
                RETURN_IF_EXCEPTION(scope, { });
                if (spreadable.isUndefined()) {
                    isSpreadable = JSC::isArray(globalObject, object);
                    RETURN_IF_EXCEPTION(scope, { });
                } else {
                    isSpreadable = spreadable.toBoolean(globalObject);
                    RETURN_IF_EXCEPTION(scope, { });
                }
            }
        }

        if (isSpreadable) {
            ASSERT(currentElement.isObject());
            auto* object = asObject(currentElement);
            uint64_t length = toLength(globalObject, object);
            RETURN_IF_EXCEPTION(scope, { });

            CheckedUint64 checkedResultSize = length;
            checkedResultSize += resultIndex;
            if (checkedResultSize.hasOverflowed() || checkedResultSize.value() > maxSafeIntegerAsUInt64()) [[unlikely]] {
                throwTypeError(globalObject, scope, "Length exceeded the maximum array length"_s);
                return { };
            }
            uint64_t resultSize = checkedResultSize.value();

            if (isJSArray(result) && isJSArray(object) && resultSize <= MAX_ARRAY_INDEX) {
                JSArray* resultArray = jsCast<JSArray*>(result);
                JSArray* otherArray = jsCast<JSArray*>(object);
                unsigned startIndex = resultIndex;
                bool success = resultArray->appendMemcpy(globalObject, vm, resultIndex, otherArray);
                RETURN_IF_EXCEPTION(scope, encodedJSValue());
                if (!success) {
                    moveArrayElements<ArrayFillMode::Empty>(globalObject, vm, resultArray, startIndex, otherArray, otherArray->length());
                    RETURN_IF_EXCEPTION(scope, { });
                }
                resultIndex += length;
            } else {
                for (uint64_t index = 0; index < length; ++index) {
                    JSValue element = getProperty(globalObject, object, index);
                    RETURN_IF_EXCEPTION(scope, { });
                    if (element) {
                        result->putDirectIndex(globalObject, resultIndex, element, 0, PutDirectIndexShouldThrow);
                        RETURN_IF_EXCEPTION(scope, { });
                    }
                    ++resultIndex;
                }
            }
        } else {
            if (resultIndex >= maxSafeIntegerAsUInt64()) [[unlikely]] {
                throwTypeError(globalObject, scope, "Length exceeded the maximum array length"_s);
                return { };
            }
            result->putDirectIndex(globalObject, resultIndex, currentElement, 0, PutDirectIndexShouldThrow);
            RETURN_IF_EXCEPTION(scope, { });
            ++resultIndex;
        }

        if (argumentIndex >= callFrame->argumentCount())
            break;
        currentElement = callFrame->uncheckedArgument(argumentIndex);
        ++argumentIndex;
    }

    scope.release();
    setLength(globalObject, vm, result, resultIndex);
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncFill, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    RETURN_IF_EXCEPTION(scope, { });
    if (thisValue.isUndefinedOrNull()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Array.prototype.fill requires that |this| not be null or undefined"_s);
    auto* thisObject = thisValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t length = toLength(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });

    JSValue argStart = callFrame->argument(1);
    uint64_t k = argumentClampedIndexFromStartOrEnd<RelativeNegativeIndex::Yes>(globalObject, argStart, length, 0);
    RETURN_IF_EXCEPTION(scope, { });

    JSValue argEnd = callFrame->argument(2);
    uint64_t finalIndex = argumentClampedIndexFromStartOrEnd<RelativeNegativeIndex::Yes>(globalObject, argEnd, length, length);
    RETURN_IF_EXCEPTION(scope, { });

    if (k > finalIndex)
        return JSValue::encode(thisObject);

    JSValue value = callFrame->argument(0);
    if (isJSArray(thisValue)) [[likely]] {
        auto* array = jsCast<JSArray*>(thisValue);
        if (array->fastFill(vm, k, finalIndex, value))
            return JSValue::encode(array);
    }

    for (; k < finalIndex; k++) {
        thisObject->putByIndexInline(globalObject, k, value, true);
        RETURN_IF_EXCEPTION(scope, { });
    }

    return JSValue::encode(thisObject);
}

JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncToReversed, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    RETURN_IF_EXCEPTION(scope, { });
    if (thisValue.isUndefinedOrNull()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Array.prototype.toReversed requires that |this| not be null or undefined"_s);
    auto* thisObject = thisValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t length = toLength(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (length > std::numeric_limits<uint32_t>::max()) [[unlikely]] {
        throwRangeError(globalObject, scope, "Array length must be a positive integer of safe magnitude."_s);
        return { };
    }

    if (isJSArray(thisObject)) [[likely]] {
        JSArray* thisArray = jsCast<JSArray*>(thisObject);
        if (auto fastResult = thisArray->fastToReversed(globalObject, length))
            return JSValue::encode(fastResult);
    }

    JSArray* result = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithUndecided), length);
    if (!result) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    for (uint32_t k = 0; k < length; k++) {
        auto fromValue = thisObject->getIndex(globalObject, length - k - 1);
        RETURN_IF_EXCEPTION(scope, { });
        result->putDirectIndex(globalObject, k, fromValue, 0, PutDirectIndexShouldThrow);
        RETURN_IF_EXCEPTION(scope, { });
    }

    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncToSorted, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue comparatorValue = callFrame->argument(0);
    if (!comparatorValue.isUndefined() && !comparatorValue.isCallable()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Array.prototype.toSorted requires the comparator argument to be a function or undefined"_s);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    RETURN_IF_EXCEPTION(scope, { });

    if (thisValue.isUndefinedOrNull()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Array.prototype.toSorted requires that |this| not be null or undefined"_s);
    auto* thisObject = thisValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t length = toLength(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (length > std::numeric_limits<uint32_t>::max()) [[unlikely]] {
        throwRangeError(globalObject, scope, "Array length must be a positive integer of safe magnitude."_s);
        return { };
    }

    JSArray* result = nullptr;
    if (isJSArray(thisValue)) [[likely]] {
        result = tryCloneArrayFromFast<ArrayFillMode::Undefined>(globalObject, thisValue);
        RETURN_IF_EXCEPTION(scope, { });
    }
    if (!result) {
        result = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithUndecided), length);
        if (!result) [[unlikely]] {
            throwOutOfMemoryError(globalObject, scope);
            return { };
        }
        for (uint32_t k = 0; k < length; k++) {
            auto fromValue = thisObject->getIndex(globalObject, k);
            RETURN_IF_EXCEPTION(scope, { });
            result->putDirectIndex(globalObject, k, fromValue, 0, PutDirectIndexShouldThrow);
            RETURN_IF_EXCEPTION(scope, { });
        }
    }

    scope.release();
    sortImpl(globalObject, result, length, comparatorValue);

    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncWith, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    RETURN_IF_EXCEPTION(scope, { });

    if (thisValue.isUndefinedOrNull()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Array.prototype.with requires that |this| not be null or undefined"_s);
    auto* thisObject = thisValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t length = toLength(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });

    int64_t actualIndex = argumentUnclampedIndexFromStartOrEnd(globalObject, callFrame->argument(0), length);
    RETURN_IF_EXCEPTION(scope, { });

    if (actualIndex >= static_cast<int64_t>(length) || actualIndex < 0) [[unlikely]] {
        throwRangeError(globalObject, scope, "Array index out of range"_s);
        return { };
    }

    if (length > std::numeric_limits<uint32_t>::max()) [[unlikely]] {
        throwRangeError(globalObject, scope, "Array length must be a positive integer of safe magnitude."_s);
        return { };
    }
    JSValue value = callFrame->argument(1);
    if (isJSArray(thisObject)) [[likely]] {
        JSArray* thisArray = jsCast<JSArray*>(thisObject);
        if (auto fastResult = thisArray->fastWith(globalObject, actualIndex, value, length))
            return JSValue::encode(fastResult);
    }

    JSArray* result = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithUndecided), length);
    if (!result) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    for (uint32_t k = 0; k < length; k++) {
        if (k == actualIndex) {
            result->putDirectIndex(globalObject, k, value, 0, PutDirectIndexShouldThrow);
            RETURN_IF_EXCEPTION(scope, { });
        } else {
            auto fromValue = thisObject->getIndex(globalObject, k);
            RETURN_IF_EXCEPTION(scope, { });
            result->putDirectIndex(globalObject, k, fromValue, 0, PutDirectIndexShouldThrow);
            RETURN_IF_EXCEPTION(scope, { });
        }
    }

    return JSValue::encode(result);
}

// FIXME: We can optimize `Array.prototype.includes` for atom string arrays too. https://bugs.webkit.org/show_bug.cgi?id=288695
JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncIncludes, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    RETURN_IF_EXCEPTION(scope, { });

    if (thisValue.isUndefinedOrNull()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Array.prototype.includes requires that |this| not be null or undefined"_s);
    auto* thisObject = thisValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t length = toLength(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (!length)
        return JSValue::encode(jsBoolean(false));

    uint64_t index = argumentClampedIndexFromStartOrEnd<RelativeNegativeIndex::Yes>(globalObject, callFrame->argument(1), length, 0);
    RETURN_IF_EXCEPTION(scope, { });

    ASSERT(index <= length);
    if (index == length)
        return JSValue::encode(jsBoolean(false));

    JSValue searchElement = callFrame->argument(0);

    if (isJSArray(thisObject)) [[likely]] {
        JSArray* thisArray = jsCast<JSArray*>(thisObject);
        auto fastResult = thisArray->fastIncludes(globalObject, searchElement, index, length);
        RETURN_IF_EXCEPTION(scope, { });
        if (fastResult)
            return JSValue::encode(jsBoolean(fastResult.value()));
    }

    for (; index < length; ++index) {
        auto currentElement = thisObject->getIndex(globalObject, index);
        RETURN_IF_EXCEPTION(scope, { });
        bool isEqual = sameValueZero(globalObject, searchElement, currentElement);
        RETURN_IF_EXCEPTION(scope, { });
        if (isEqual)
            return JSValue::encode(jsBoolean(true));
    }

    return JSValue::encode(jsBoolean(false));
}

JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncCopyWithin, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    RETURN_IF_EXCEPTION(scope, { });

    if (thisValue.isUndefinedOrNull()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Array.prototype.copyWithin requires that |this| not be null or undefined"_s);
    auto* thisObject = thisValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t length = toLength(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t to = argumentClampedIndexFromStartOrEnd<RelativeNegativeIndex::Yes>(globalObject, callFrame->argument(0), length);
    RETURN_IF_EXCEPTION(scope, { });
    uint64_t from = argumentClampedIndexFromStartOrEnd<RelativeNegativeIndex::Yes>(globalObject, callFrame->argument(1), length);
    RETURN_IF_EXCEPTION(scope, { });
    uint64_t finalIndex = argumentClampedIndexFromStartOrEnd<RelativeNegativeIndex::Yes>(globalObject, callFrame->argument(2), length, length);
    RETURN_IF_EXCEPTION(scope, { });

    if (finalIndex < from)
        return JSValue::encode(thisValue);

    ASSERT(to <= length);
    ASSERT(from <= length);

    uint64_t count = std::min(length - std::max(to, from), finalIndex - from);
    ASSERT(count <= length);
    if (!count)
        return JSValue::encode(thisObject);

    if (isJSArray(thisObject)) [[likely]] {
        JSArray* thisArray = asArray(thisObject);
        if (thisArray->fastCopyWithin(globalObject, from, to, count, length))
            return JSValue::encode(thisValue);
    }

    int8_t direction = 1;
    if (from < to && to < from + count) {
        direction = -1;
        from = from + count - 1;
        to = to + count - 1;
    }

    for (uint64_t i = 0; i < count; ++i, from += direction, to += direction) {
        bool hasProperty = thisObject->hasProperty(globalObject, from);
        RETURN_IF_EXCEPTION(scope, { });
        if (hasProperty) {
            JSValue fromValue = thisObject->getIndex(globalObject, from);
            RETURN_IF_EXCEPTION(scope, { });
            thisObject->putByIndexInline(globalObject, to, fromValue, true);
            RETURN_IF_EXCEPTION(scope, { });
        } else {
            bool success = thisObject->deleteProperty(globalObject, to);
            RETURN_IF_EXCEPTION(scope, { });
            if (!success) [[unlikely]] {
                throwTypeError(globalObject, scope, UnableToDeletePropertyError);
                return { };
            }
        }
    }

    return JSValue::encode(thisObject);
}

JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncToSpliced, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    RETURN_IF_EXCEPTION(scope, { });

    if (thisValue.isUndefinedOrNull()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "Array.prototype.toSpliced requires that |this| not be null or undefined"_s);
    auto* thisObject = thisValue.toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t length = toLength(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t start = argumentClampedIndexFromStartOrEnd<RelativeNegativeIndex::Yes>(globalObject, callFrame->argument(0), length);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t insertCount = 0;
    uint64_t deleteCount = 0;
    if (callFrame->argumentCount() == 1)
        deleteCount = length - start;
    else if (callFrame->argumentCount() > 1) {
        insertCount = callFrame->argumentCount() - 2;
        deleteCount = argumentClampedIndexFromStartOrEnd<RelativeNegativeIndex::No>(globalObject, callFrame->uncheckedArgument(1), length - start);
        RETURN_IF_EXCEPTION(scope, { });
    }

    uint64_t newLen = length + insertCount - deleteCount;

    if (newLen >= maxSafeIntegerAsUInt64()) [[unlikely]] {
        throwTypeError(globalObject, scope, "Array length exceeds 2**53 - 1"_s);
        return { };
    }

    if (newLen > std::numeric_limits<uint32_t>::max()) {
        throwRangeError(globalObject, scope, "Array length must be a positive integer of safe magnitude."_s);
        return { };
    }

    if (isJSArray(thisObject)) [[likely]] {
        JSArray* thisArray = asArray(thisObject);
        if (JSArray* result = thisArray->fastToSpliced(globalObject, callFrame, length, newLen, start, deleteCount, insertCount))
            return JSValue::encode(result);
    }

    JSArray* result = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithUndecided), newLen);
    if (!result) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    uint64_t k = 0;

    for (; k < start; k++) {
        JSValue fromValue = thisObject->getIndex(globalObject, k);
        RETURN_IF_EXCEPTION(scope, { });
        result->putDirectIndex(globalObject, k, fromValue, 0, PutDirectIndexShouldThrow);
        RETURN_IF_EXCEPTION(scope, { });
    }

    for (uint64_t i = 0; i < insertCount; i++, k++) {
        result->putDirectIndex(globalObject, k, callFrame->argument(i + 2), 0, PutDirectIndexShouldThrow);
        RETURN_IF_EXCEPTION(scope, { });
    }

    for (; k < newLen; k++) {
        JSValue fromValue = thisObject->getIndex(globalObject, k + deleteCount - insertCount);
        RETURN_IF_EXCEPTION(scope, { });
        result->putDirectIndex(globalObject, k, fromValue, 0, PutDirectIndexShouldThrow);
        RETURN_IF_EXCEPTION(scope, { });
    }

    return JSValue::encode(result);
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

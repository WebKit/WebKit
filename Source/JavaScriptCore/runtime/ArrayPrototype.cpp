/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2021 Apple Inc. All rights reserved.
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
#include "IntegrityInlines.h"
#include "JSArrayInlines.h"
#include "JSArrayIterator.h"
#include "JSCBuiltins.h"
#include "JSCInlines.h"
#include "JSImmutableButterfly.h"
#include "JSStringJoiner.h"
#include "ObjectConstructor.h"
#include "ObjectPrototypeInlines.h"
#include "StringRecursionChecker.h"
#include <algorithm>
#include <wtf/Assertions.h>

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
static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncSplice);
static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncUnShift);
static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncIndexOf);
static JSC_DECLARE_HOST_FUNCTION(arrayProtoFuncLastIndexOf);

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
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().concatPublicName(), arrayPrototypeConcatCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().fillPublicName(), arrayPrototypeFillCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->join, arrayProtoFuncJoin, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION("pop"_s, arrayProtoFuncPop, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public, ArrayPopIntrinsic);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().pushPublicName(), arrayProtoFuncPush, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public, ArrayPushIntrinsic);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("reverse"_s, arrayProtoFuncReverse, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().shiftPublicName(), arrayProtoFuncShift, static_cast<unsigned>(PropertyAttribute::DontEnum), 0, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().shiftPrivateName(), arrayProtoFuncShift, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly, 0, ImplementationVisibility::Public);
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->slice, arrayProtoFuncSlice, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public, ArraySliceIntrinsic);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().sortPublicName(), arrayPrototypeSortCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
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
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().includesPublicName(), arrayPrototypeIncludesCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().copyWithinPublicName(), arrayPrototypeCopyWithinCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().atPublicName(), arrayPrototypeAtCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().toReversedPublicName(), arrayPrototypeToReversedCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().toSortedPublicName(), arrayPrototypeToSortedCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().toSplicedPublicName(), arrayPrototypeToSplicedCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().withPublicName(), arrayPrototypeWithCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().entriesPrivateName(), getDirect(vm, vm.propertyNames->builtinNames().entriesPublicName()), static_cast<unsigned>(PropertyAttribute::ReadOnly));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().forEachPrivateName(), getDirect(vm, vm.propertyNames->builtinNames().forEachPublicName()), static_cast<unsigned>(PropertyAttribute::ReadOnly));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().includesPrivateName(), getDirect(vm, vm.propertyNames->builtinNames().includesPublicName()), static_cast<unsigned>(PropertyAttribute::ReadOnly));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().indexOfPrivateName(), getDirect(vm, vm.propertyNames->builtinNames().indexOfPublicName()), static_cast<unsigned>(PropertyAttribute::ReadOnly));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().keysPrivateName(), getDirect(vm, vm.propertyNames->builtinNames().keysPublicName()), static_cast<unsigned>(PropertyAttribute::ReadOnly));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().mapPrivateName(), getDirect(vm, vm.propertyNames->builtinNames().mapPublicName()), static_cast<unsigned>(PropertyAttribute::ReadOnly));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().popPrivateName(), getDirect(vm, vm.propertyNames->builtinNames().popPublicName()), static_cast<unsigned>(PropertyAttribute::ReadOnly));
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().valuesPrivateName(), globalObject->arrayProtoValuesFunction(), static_cast<unsigned>(PropertyAttribute::ReadOnly));

    JSObject* unscopables = constructEmptyObject(vm, globalObject->nullPrototypeObjectStructure());
    unscopables->convertToDictionary(vm);
    const Identifier* const unscopableNames[] = {
        &vm.propertyNames->builtinNames().atPublicName(),
        &vm.propertyNames->builtinNames().copyWithinPublicName(),
        &vm.propertyNames->builtinNames().entriesPublicName(),
        &vm.propertyNames->builtinNames().fillPublicName(),
        &vm.propertyNames->builtinNames().findPublicName(),
        &vm.propertyNames->builtinNames().findIndexPublicName(),
        &vm.propertyNames->builtinNames().findLastPublicName(),
        &vm.propertyNames->builtinNames().findLastIndexPublicName(),
        &vm.propertyNames->builtinNames().flatPublicName(),
        &vm.propertyNames->builtinNames().flatMapPublicName(),
        &vm.propertyNames->builtinNames().includesPublicName(),
        &vm.propertyNames->builtinNames().keysPublicName(),
        &vm.propertyNames->builtinNames().toReversedPublicName(),
        &vm.propertyNames->builtinNames().toSortedPublicName(),
        &vm.propertyNames->builtinNames().toSplicedPublicName(),
        &vm.propertyNames->builtinNames().valuesPublicName()
    };
    for (const auto* unscopableName : unscopableNames) {
        if (unscopableName)
            unscopables->putDirect(vm, *unscopableName, jsBoolean(true));
    }
    putDirectWithoutTransition(vm, vm.propertyNames->unscopablesSymbol, unscopables, PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
}

// ------------------------------ Array Functions ----------------------------

static inline uint64_t argumentClampedIndexFromStartOrEnd(JSGlobalObject* globalObject, JSValue value, uint64_t length, uint64_t undefinedValue = 0)
{
    if (value.isUndefined())
        return undefinedValue;

    if (LIKELY(value.isInt32())) {
        int64_t indexInt64 = value.asInt32();
        if (indexInt64 < 0) {
            indexInt64 += length;
            return indexInt64 < 0 ? 0 : static_cast<uint64_t>(indexInt64);
        }
        uint64_t indexUInt64 = static_cast<uint64_t>(indexInt64);
        return std::min(indexUInt64, length);
    }

    double indexDouble = value.toIntegerOrInfinity(globalObject);
    if (indexDouble < 0) {
        indexDouble += length;
        return indexDouble < 0 ? 0 : static_cast<uint64_t>(indexDouble);
    }
    return indexDouble > length ? length : static_cast<uint64_t>(indexDouble);
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

inline bool holesMustForwardToPrototype(JSObject* object)
{
    return object->structure()->holesMustForwardToPrototype(object);
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

inline JSValue fastJoin(JSGlobalObject* globalObject, JSObject* thisObject, StringView separator, unsigned length, bool& sawHoles, bool& genericCase)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSStringJoiner joiner(separator);

    unsigned i = 0;
    switch (thisObject->indexingType()) {
    case ALL_INT32_INDEXING_TYPES: {
        auto& butterfly = *thisObject->butterfly();
        if (UNLIKELY(length > butterfly.publicLength()))
            break;
        joiner.reserveCapacity(globalObject, length);
        RETURN_IF_EXCEPTION(scope, { });
        auto data = butterfly.contiguous().data();
        bool holesKnownToBeOK = false;
        for (; i < length; ++i) {
            JSValue value = data[i].get();
            if (LIKELY(value))
                joiner.appendNumber(vm, value.asInt32());
            else {
                sawHoles = true;
                if (!holesKnownToBeOK) {
                    if (holesMustForwardToPrototype(thisObject))
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
        auto data = butterfly.contiguous().data();
        bool holesKnownToBeOK = false;
        for (; i < length; ++i) {
            if (JSValue value = data[i].get()) {
                if (!joiner.appendWithoutSideEffects(globalObject, value))
                    goto generalCase;
                RETURN_IF_EXCEPTION(scope, { });
            } else {
                sawHoles = true;
                if (!holesKnownToBeOK) {
                    if (holesMustForwardToPrototype(thisObject))
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
        joiner.reserveCapacity(globalObject, length);
        RETURN_IF_EXCEPTION(scope, { });
        auto data = butterfly.contiguousDouble().data();
        bool holesKnownToBeOK = false;
        for (; i < length; ++i) {
            double value = data[i];
            if (LIKELY(!isHole(value)))
                joiner.appendNumber(vm, value);
            else {
                sawHoles = true;
                if (!holesKnownToBeOK) {
                    if (holesMustForwardToPrototype(thisObject))
                        goto generalCase;
                    holesKnownToBeOK = true;
                }
                joiner.appendEmptyString();
            }
        }
        RELEASE_AND_RETURN(scope, joiner.join(globalObject));
    }
    case ALL_UNDECIDED_INDEXING_TYPES: {
        if (length && holesMustForwardToPrototype(thisObject))
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

            JSString* operand = jsString(vm, separator);
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
    genericCase = true;
    for (; i < length; ++i) {
        JSValue element = thisObject->getIndex(globalObject, i);
        RETURN_IF_EXCEPTION(scope, { });
        joiner.append(globalObject, element);
        RETURN_IF_EXCEPTION(scope, { });
    }
    RELEASE_AND_RETURN(scope, joiner.join(globalObject));
}

ALWAYS_INLINE JSValue fastJoin(JSGlobalObject* globalObject, JSObject* thisObject, StringView separator, unsigned length)
{
    bool sawHoles = false;
    bool genericCase = false;
    return fastJoin(globalObject, thisObject, separator, length, sawHoles, genericCase);
}

inline bool canUseDefaultArrayJoinForToString(JSObject* thisObject)
{
    JSGlobalObject* globalObject = thisObject->globalObject();

    if (globalObject->arrayJoinWatchpointSet().state() != IsWatched)
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
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    Integrity::auditStructureID(thisObject->structureID());
    if (UNLIKELY(!canUseDefaultArrayJoinForToString(thisObject))) {
        // 2. Let func be the result of calling the [[Get]] internal method of array with argument "join".
        JSValue function = thisObject->get(globalObject, vm.propertyNames->join);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());

        // 3. If IsCallable(func) is false, then let func be the standard built-in method Object.prototype.toString (15.2.4.2).
        auto callData = JSC::getCallData(function);
        if (UNLIKELY(callData.type == CallData::Type::None))
            RELEASE_AND_RETURN(scope, JSValue::encode(objectPrototypeToString(globalObject, thisObject)));

        // 4. Return the result of calling the [[Call]] internal method of func providing array as the this value and an empty arguments list.
        if (!isJSArray(thisObject) || callData.type != CallData::Type::Native || callData.native.function != arrayProtoFuncJoin)
            RELEASE_AND_RETURN(scope, JSValue::encode(call(globalObject, function, callData, thisObject, *vm.emptyList)));
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

        bool isCoW = isCopyOnWrite(thisArray->indexingMode());
        JSImmutableButterfly* immutableButterfly = nullptr;
        if (isCoW) {
            immutableButterfly = JSImmutableButterfly::fromButterfly(thisArray->butterfly());
            auto iter = vm.heap.immutableButterflyToStringCache.find(immutableButterfly);
            if (iter != vm.heap.immutableButterflyToStringCache.end())
                return JSValue::encode(iter->value);
        }

        bool sawHoles = false;
        bool genericCase = false;
        JSValue result = fastJoin(globalObject, thisArray, { &comma, 1 }, length, sawHoles, genericCase);
        RETURN_IF_EXCEPTION(scope, { });

        if (!sawHoles && !genericCase && result && isJSString(result) && isCoW) {
            ASSERT(JSImmutableButterfly::fromButterfly(thisArray->butterfly()) == immutableButterfly);
            vm.heap.immutableButterflyToStringCache.add(immutableButterfly, jsCast<JSString*>(result));
        }

        return JSValue::encode(result);
    }

    JSStringJoiner joiner(","_s);
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

static JSString* toLocaleString(JSGlobalObject* globalObject, JSValue value, JSValue locales, JSValue options)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue toLocaleStringMethod = value.get(globalObject, vm.propertyNames->toLocaleString);
    RETURN_IF_EXCEPTION(scope, { });

    auto callData = JSC::getCallData(toLocaleStringMethod);
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
    const LChar comma = ',';
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
    if (UNLIKELY(!thisObject))
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
        const LChar comma = ',';

        if (UNLIKELY(length > std::numeric_limits<unsigned>::max() || !canUseFastJoin(thisObject))) {
            JSString* jsSeparator = jsSingleCharacterString(vm, comma);
            RETURN_IF_EXCEPTION(scope, encodedJSValue());

            RELEASE_AND_RETURN(scope, JSValue::encode(slowJoin(globalObject, thisObject, jsSeparator, length)));
        }

        unsigned unsignedLength = static_cast<unsigned>(length);
        ASSERT(static_cast<double>(unsignedLength) == length);

        RELEASE_AND_RETURN(scope, JSValue::encode(fastJoin(globalObject, thisObject, { &comma, 1 }, unsignedLength)));
    }

    // 4. Let sep be ? ToString(separator).
    JSString* jsSeparator = separatorValue.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (UNLIKELY(length > std::numeric_limits<unsigned>::max() || !canUseFastJoin(thisObject)))
        RELEASE_AND_RETURN(scope, JSValue::encode(slowJoin(globalObject, thisObject, jsSeparator, length)));

    auto viewWithString = jsSeparator->viewWithUnderlyingString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    RELEASE_AND_RETURN(scope, JSValue::encode(fastJoin(globalObject, thisObject, viewWithString.view, length)));
}

inline EncodedJSValue createArrayIteratorObject(JSGlobalObject* globalObject, CallFrame* callFrame, IterationKind kind)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* thisObject = callFrame->thisValue().toThis(globalObject, ECMAMode::strict()).toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObject);
    UNUSED_PARAM(scope);
    if (UNLIKELY(!thisObject))
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

    if (isJSArray(thisValue))
        RELEASE_AND_RETURN(scope, JSValue::encode(asArray(thisValue)->pop(globalObject)));

    JSObject* thisObj = thisValue.toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObj);
    if (UNLIKELY(!thisObj))
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
    if (UNLIKELY(!success)) {
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
    uint64_t length = toLength(globalObject, thisObj);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    unsigned argCount = callFrame->argumentCount();

    if (UNLIKELY(length + argCount > static_cast<uint64_t>(maxSafeInteger())))
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
    if (UNLIKELY(!thisObject))
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
            if (UNLIKELY(!success)) {
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
            if (UNLIKELY(!success)) {
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
    if (UNLIKELY(!thisObj))
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
    if (UNLIKELY(!thisObj))
        return { };
    uint64_t length = toLength(globalObject, thisObj);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t begin = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(0), length);
    RETURN_IF_EXCEPTION(scope, { });
    uint64_t end = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(1), length, length);
    RETURN_IF_EXCEPTION(scope, { });
    if (end < begin)
        end = begin;

    std::pair<SpeciesConstructResult, JSObject*> speciesResult = speciesConstructArray(globalObject, thisObj, end - begin);
    // We can only get an exception if we call some user function.
    EXCEPTION_ASSERT(!!scope.exception() == (speciesResult.first == SpeciesConstructResult::Exception));
    if (UNLIKELY(speciesResult.first == SpeciesConstructResult::Exception))
        return { };

    if (LIKELY(speciesResult.first == SpeciesConstructResult::FastPath)) {
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
        if (UNLIKELY(end - begin > std::numeric_limits<uint32_t>::max())) {
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

JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncSplice, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    // 15.4.4.12

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* thisObj = callFrame->thisValue().toThis(globalObject, ECMAMode::strict()).toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObj);
    if (UNLIKELY(!thisObj))
        return encodedJSValue();
    uint64_t length = toLength(globalObject, thisObj);
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

    uint64_t actualStart = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(0), length);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    uint64_t actualDeleteCount = length - actualStart;
    if (callFrame->argumentCount() > 1) {
        double deleteCount = callFrame->uncheckedArgument(1).toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        if (deleteCount < 0)
            actualDeleteCount = 0;
        else if (deleteCount > length - actualStart)
            actualDeleteCount = length - actualStart;
        else
            actualDeleteCount = static_cast<uint64_t>(deleteCount);
    }
    unsigned itemCount = std::max<int>(callFrame->argumentCount() - 2, 0);
    if (UNLIKELY(length - actualDeleteCount + itemCount > static_cast<uint64_t>(maxSafeInteger())))
        return throwVMTypeError(globalObject, scope, "Splice cannot produce an array of length larger than (2 ** 53) - 1"_s);

    std::pair<SpeciesConstructResult, JSObject*> speciesResult = speciesConstructArray(globalObject, thisObj, actualDeleteCount);
    EXCEPTION_ASSERT(!!scope.exception() == (speciesResult.first == SpeciesConstructResult::Exception));
    if (speciesResult.first == SpeciesConstructResult::Exception)
        return JSValue::encode(jsUndefined());

    JSObject* result = nullptr;
    if (LIKELY(speciesResult.first == SpeciesConstructResult::FastPath)) {
        result = JSArray::fastSlice(globalObject, thisObj, actualStart, actualDeleteCount);
        RETURN_IF_EXCEPTION(scope, { });
    }

    if (!result) {
        if (speciesResult.first == SpeciesConstructResult::CreatedObject)
            result = speciesResult.second;
        else {
            if (UNLIKELY(actualDeleteCount > std::numeric_limits<uint32_t>::max())) {
                throwRangeError(globalObject, scope, LengthExceededTheMaximumArrayLengthError);
                return encodedJSValue();
            }
            result = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithUndecided), static_cast<uint32_t>(actualDeleteCount));
            if (UNLIKELY(!result)) {
                throwOutOfMemoryError(globalObject, scope);
                return encodedJSValue();
            }
        }
        for (uint64_t k = 0; k < actualDeleteCount; ++k) {
            JSValue v = getProperty(globalObject, thisObj, k + actualStart);
            RETURN_IF_EXCEPTION(scope, encodedJSValue());
            if (UNLIKELY(!v))
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
    if (UNLIKELY(!thisObj))
        return encodedJSValue();
    uint64_t length = toLength(globalObject, thisObj);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    unsigned nrArgs = callFrame->argumentCount();
    if (nrArgs) {
        if (UNLIKELY(length + nrArgs > static_cast<uint64_t>(maxSafeInteger())))
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
            if (searchElement.isObject()) {
                auto* result = bitwise_cast<const WriteBarrier<Unknown>*>(WTF::find64(bitwise_cast<const uint64_t*>(data + index), JSValue::encode(searchElement), length - index));
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

JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncIndexOf, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 15.4.4.14
    JSObject* thisObject = callFrame->thisValue().toThis(globalObject, ECMAMode::strict()).toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObject);
    if (UNLIKELY(!thisObject))
        return { };
    uint64_t length = toLength(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });
    if (!length)
        return JSValue::encode(jsNumber(-1));

    uint64_t index = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(1), length);
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

JSC_DEFINE_HOST_FUNCTION(arrayProtoFuncLastIndexOf, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 15.4.4.15
    JSObject* thisObject = callFrame->thisValue().toThis(globalObject, ECMAMode::strict()).toObject(globalObject);
    EXCEPTION_ASSERT(!!scope.exception() == !thisObject);
    if (UNLIKELY(!thisObject))
        return { };
    uint64_t length = toLength(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });
    if (!length)
        return JSValue::encode(jsNumber(-1));

    uint64_t index = length - 1;
    if (callFrame->argumentCount() >= 2) {
        JSValue fromValue = callFrame->uncheckedArgument(1);
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

enum class FillMode {
    Undefined,
    Empty,
};

template<FillMode fillMode>
static bool moveElements(JSGlobalObject* globalObject, VM& vm, JSArray* target, unsigned targetOffset, JSArray* source, unsigned sourceLength)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (LIKELY(!hasAnyArrayStorage(source->indexingType()) && !source->holesMustForwardToPrototype())) {
        for (unsigned i = 0; i < sourceLength; ++i) {
            JSValue value = source->tryGetIndexQuickly(i);
            if constexpr (fillMode == FillMode::Empty) {
                if (!value)
                    continue;
            } else {
                if (!value)
                    value = jsUndefined();
            }
            target->putDirectIndex(globalObject, targetOffset + i, value, 0, PutDirectIndexShouldThrow);
            RETURN_IF_EXCEPTION(scope, false);
        }
    } else {
        for (unsigned i = 0; i < sourceLength; ++i) {
            JSValue value = getProperty(globalObject, source, i);
            RETURN_IF_EXCEPTION(scope, false);
            if constexpr (fillMode == FillMode::Empty) {
                if (!value)
                    continue;
            } else {
                if (!value)
                    value = jsUndefined();
            }
            target->putDirectIndex(globalObject, targetOffset + i, value, 0, PutDirectIndexShouldThrow);
            RETURN_IF_EXCEPTION(scope, false);
        }
    }
    return true;
}

static JSValue concatAppendOne(JSGlobalObject* globalObject, VM& vm, JSArray* first, JSValue second)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(!isJSArray(second));
    ASSERT(!shouldUseSlowPut(first->indexingType()));
    Butterfly* firstButterfly = first->butterfly();
    unsigned firstArraySize = firstButterfly->publicLength();

    CheckedUint32 checkedResultSize = firstArraySize;
    checkedResultSize += 1;
    if (UNLIKELY(checkedResultSize.hasOverflowed())) {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    // This code still can see concat-spreadable items. It is ProxyObject and DerivedArray.
    // We should not use `isArray` check here since it is side-effectful for ProxyObject. Let's just bail out if it is ProxyObject or DerivedArray.
    if (second.isObject()) {
        JSType type = asObject(second)->type();
        if (UNLIKELY(type == ProxyObjectType || type == DerivedArrayType))
            return jsNull();
    }

    unsigned resultSize = checkedResultSize;
    bool allowPromotion = false;
    IndexingType type = first->mergeIndexingTypeForCopying(indexingTypeForValue(second) | IsArray, allowPromotion);
    
    if (type == NonArray)
        type = first->indexingType();

    Structure* resultStructure = globalObject->arrayStructureForIndexingTypeDuringAllocation(type);
    JSArray* result = JSArray::tryCreate(vm, resultStructure, resultSize);
    if (UNLIKELY(!result)) {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    bool success = result->appendMemcpy(globalObject, vm, 0, first);
    EXCEPTION_ASSERT(!scope.exception() || !success);
    if (!success) {
        RETURN_IF_EXCEPTION(scope, { });

        bool success = moveElements<FillMode::Empty>(globalObject, vm, result, 0, first, firstArraySize);
        EXCEPTION_ASSERT(!scope.exception() == success);
        if (UNLIKELY(!success))
            return { };
    }

    scope.release();
    result->putDirectIndex(globalObject, firstArraySize, second);
    return result;
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

template<FillMode fillMode, typename T, typename U>
ALWAYS_INLINE void copyElements(T* buffer, unsigned offset, U* source, unsigned sourceSize, IndexingType sourceType)
{
    if (sourceType == ArrayWithUndecided) {
        if constexpr (fillMode == FillMode::Empty) {
            for (unsigned i = 0; i < sourceSize; ++i)
                clearElement<T>(buffer[i + offset]);
        } else {
            for (unsigned i = 0; i < sourceSize; ++i)
                buffer[i + offset].setWithoutWriteBarrier(jsUndefined());
        }
        return;
    }

    if constexpr (std::is_same_v<T, U>) {
        if constexpr (fillMode == FillMode::Empty) {
            gcSafeMemcpy(buffer + offset, source, sizeof(JSValue) * sourceSize);
            return;
        } else {
            for (unsigned i = 0; i < sourceSize; ++i) {
                JSValue value = source[i].get();
                if (!value)
                    value = jsUndefined();
                buffer[i + offset].setWithoutWriteBarrier(value);
            }
        }
    } else if constexpr (std::is_same_v<T, double>) {
        ASSERT(sourceType == ArrayWithInt32);
        static_assert(fillMode == FillMode::Empty);
        for (unsigned i = 0; i < sourceSize; ++i) {
            JSValue value = source[i].get();
            if (value)
                buffer[i + offset] = value.asInt32();
            else
                buffer[i + offset] = PNaN;
        }
    } else {
        static_assert(std::is_same_v<U, double>);
        for (unsigned i = 0; i < sourceSize; ++i) {
            double value = source[i];
            if (value == value)
                buffer[i + offset].setWithoutWriteBarrier(JSValue(JSValue::EncodeAsDouble, value));
            else {
                if constexpr (fillMode == FillMode::Undefined)
                    buffer[i + offset].setWithoutWriteBarrier(jsUndefined());
                else
                    buffer[i + offset].clear();
            }
        }
    }
};

JSC_DEFINE_HOST_FUNCTION(arrayProtoPrivateFuncConcatMemcpy, (JSGlobalObject* globalObject, CallFrame* callFrame))
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
    bool isValid = arraySpeciesWatchpointIsValid(firstArray);
    RETURN_IF_EXCEPTION(scope, { });
    if (UNLIKELY(!isValid))
        return JSValue::encode(jsNull());

    JSValue second = callFrame->uncheckedArgument(1);
    if (!isJSArray(second))
        RELEASE_AND_RETURN(scope, JSValue::encode(concatAppendOne(globalObject, vm, firstArray, second)));

    JSArray* secondArray = jsCast<JSArray*>(second);
    
    Butterfly* firstButterfly = firstArray->butterfly();
    Butterfly* secondButterfly = secondArray->butterfly();

    unsigned firstArraySize = firstButterfly->publicLength();
    unsigned secondArraySize = secondButterfly->publicLength();

    CheckedUint32 checkedResultSize = firstArraySize;
    checkedResultSize += secondArraySize;

    if (UNLIKELY(checkedResultSize.hasOverflowed())) {
        throwOutOfMemoryError(globalObject, scope);
        return encodedJSValue();
    }

    unsigned resultSize = checkedResultSize;
    IndexingType firstType = firstArray->indexingType();
    IndexingType secondType = secondArray->indexingType();
    bool allowPromotion = true;
    IndexingType type = firstArray->mergeIndexingTypeForCopying(secondType, allowPromotion);
    if (type == NonArray || !firstArray->canFastCopy(secondArray) || resultSize >= MIN_SPARSE_ARRAY_INDEX) {
        JSArray* result = constructEmptyArray(globalObject, nullptr, resultSize);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());

        bool success = moveElements<FillMode::Empty>(globalObject, vm, result, 0, firstArray, firstArraySize);
        EXCEPTION_ASSERT(!scope.exception() == success);
        if (UNLIKELY(!success))
            return encodedJSValue();
        success = moveElements<FillMode::Empty>(globalObject, vm, result, firstArraySize, secondArray, secondArraySize);
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
        if (firstType == ArrayWithDouble)
            copyElements<FillMode::Empty>(buffer, 0, firstButterfly->contiguousDouble().data(), firstArraySize, firstType);
        else
            copyElements<FillMode::Empty>(buffer, 0, firstButterfly->contiguous().data(), firstArraySize, firstType);
        if (secondType == ArrayWithDouble)
            copyElements<FillMode::Empty>(buffer, firstArraySize, secondButterfly->contiguousDouble().data(), secondArraySize, secondType);
        else
            copyElements<FillMode::Empty>(buffer, firstArraySize, secondButterfly->contiguous().data(), secondArraySize, secondType);
    } else if (type != ArrayWithUndecided) {
        WriteBarrier<Unknown>* buffer = result->butterfly()->contiguous().data();
        copyElements<FillMode::Empty>(buffer, 0, firstButterfly->contiguous().data(), firstArraySize, firstType);
        copyElements<FillMode::Empty>(buffer, firstArraySize, secondButterfly->contiguous().data(), secondArraySize, secondType);
    }

    ASSERT(result->butterfly()->publicLength() == resultSize);
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(arrayProtoPrivateFuncAppendMemcpy, (JSGlobalObject* globalObject, CallFrame* callFrame))
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
    moveElements<FillMode::Empty>(globalObject, vm, resultArray, startIndex, otherArray, otherArray->length());
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(arrayProtoPrivateFuncFromFast, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue constructor = callFrame->uncheckedArgument(0);
    if (UNLIKELY(constructor != globalObject->arrayConstructor() && constructor.isObject()))
        return JSValue::encode(jsUndefined());

    JSValue arrayValue = callFrame->uncheckedArgument(1);
    if (UNLIKELY(!isJSArray(arrayValue)))
        return JSValue::encode(jsUndefined());

    auto* array = jsCast<JSArray*>(arrayValue);
    if (UNLIKELY(!array->isIteratorProtocolFastAndNonObservable()))
        return JSValue::encode(jsUndefined());

    IndexingType sourceType = array->indexingType();
    if (UNLIKELY(shouldUseSlowPut(sourceType) || sourceType == ArrayClass))
        return JSValue::encode(jsUndefined());

    Butterfly* butterfly= array->butterfly();
    unsigned resultSize = butterfly->publicLength();
    if (hasAnyArrayStorage(sourceType) || resultSize >= MIN_SPARSE_ARRAY_INDEX) {
        JSArray* result = constructEmptyArray(globalObject, nullptr, resultSize);
        RETURN_IF_EXCEPTION(scope, { });

        scope.release();
        moveElements<FillMode::Undefined>(globalObject, vm, result, 0, array, resultSize);
        return JSValue::encode(result);
    }

    ASSERT(sourceType == ArrayWithDouble || sourceType == ArrayWithInt32 || sourceType == ArrayWithContiguous || sourceType == ArrayWithUndecided);
    IndexingType resultType = sourceType;
    if (sourceType == ArrayWithDouble) {
        double* buffer = butterfly->contiguousDouble().data();
        for (unsigned i = 0; i < resultSize; ++i) {
            double value = buffer[i];
            if (std::isnan(value)) {
                resultType = ArrayWithContiguous;
                break;
            }
        }
    } else if (sourceType == ArrayWithInt32) {
        auto* buffer = butterfly->contiguous().data();
        if (UNLIKELY(WTF::find64(bitwise_cast<const uint64_t*>(buffer), JSValue::encode(JSValue()), resultSize)))
            resultType = ArrayWithContiguous;
    } else if (sourceType == ArrayWithUndecided && resultSize)
        resultType = ArrayWithContiguous;

    Structure* resultStructure = globalObject->arrayStructureForIndexingTypeDuringAllocation(resultType);
    if (UNLIKELY(hasAnyArrayStorage(resultStructure->indexingType())))
        return JSValue::encode(jsUndefined());

    ASSERT(!globalObject->isHavingABadTime());
    ObjectInitializationScope initializationScope(vm);
    JSArray* result = JSArray::tryCreateUninitializedRestricted(initializationScope, resultStructure, resultSize);
    if (UNLIKELY(!result)) {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }
    ASSERT(result->butterfly()->publicLength() == resultSize);

    if (resultType == ArrayWithUndecided) {
        ASSERT(!resultSize);
        return JSValue::encode(result);
    }

    if (resultType == ArrayWithDouble) {
        ASSERT(sourceType == ArrayWithDouble);
        double* buffer = result->butterfly()->contiguousDouble().data();
        copyElements<FillMode::Empty>(buffer, 0, butterfly->contiguousDouble().data(), resultSize, ArrayWithDouble);
        return JSValue::encode(result);
    }

    if (resultType == ArrayWithInt32) {
        ASSERT(sourceType == ArrayWithInt32);
        auto* buffer = result->butterfly()->contiguous().data();
        copyElements<FillMode::Empty>(buffer, 0, butterfly->contiguous().data(), resultSize, ArrayWithInt32);
        return JSValue::encode(result);
    }

    ASSERT(resultType == ArrayWithContiguous);
    auto* buffer = result->butterfly()->contiguous().data();
    if (sourceType == ArrayWithDouble)
        copyElements<FillMode::Undefined>(buffer, 0, butterfly->contiguousDouble().data(), resultSize, ArrayWithDouble);
    else
        copyElements<FillMode::Undefined>(buffer, 0, butterfly->contiguous().data(), resultSize, sourceType);
    return JSValue::encode(result);
}

} // namespace JSC

/*
 * Copyright (C) 2022 Apple, Inc. All rights reserved.
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

#include "ArrayConstructor.h"
#include "ArrayPrototype.h"
#include "CommonIdentifiers.h"
#include "Error.h"
#include "ExceptionHelpers.h"
#include "GetVM.h"
#include "JSGlobalObject.h"
#include "JSStringJoiner.h"
#include "JSStringInlines.h"
#include "ObjectPrototype.h"
#include "StringRecursionChecker.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

enum class SpeciesConstructResult : uint8_t {
    FastPath,
    Exception,
    CreatedObject
};

ALWAYS_INLINE bool arraySpeciesWatchpointIsValid(VM& vm, JSObject* thisObject)
{
    JSGlobalObject* globalObject = thisObject->globalObject();
    ArrayPrototype* arrayPrototype = globalObject->arrayPrototype();

    ASSERT(globalObject->arraySpeciesWatchpointSet().state() != ClearWatchpoint);
    if (arrayPrototype != thisObject->getPrototypeDirect())
        return false;

    if (globalObject->arraySpeciesWatchpointSet().state() != IsWatched)
        return false;

    if (!thisObject->hasCustomProperties())
        return true;

    return thisObject->getDirectOffset(vm, vm.propertyNames->constructor) == invalidOffset;
}

ALWAYS_INLINE bool arrayMissingIsConcatSpreadable(VM& vm, JSObject* thisObject)
{
    JSGlobalObject* globalObject = thisObject->globalObject();
    ASSERT(globalObject->arrayIsConcatSpreadableWatchpointSet().state() != ClearWatchpoint);
    if (globalObject->arrayIsConcatSpreadableWatchpointSet().state() != IsWatched)
        return false;

    if (isJSArray(thisObject)) {
        ArrayPrototype* arrayPrototype = globalObject->arrayPrototype();
        if (arrayPrototype != thisObject->getPrototypeDirect())
            return false;
    } else {
        if (globalObject->objectPrototype() != thisObject->getPrototypeDirect())
            return false;
    }

    if (!thisObject->hasCustomProperties())
        return true;

    return thisObject->getDirectOffset(vm, vm.propertyNames->isConcatSpreadableSymbol) == invalidOffset;
}

ALWAYS_INLINE std::pair<SpeciesConstructResult, JSObject*> speciesConstructArray(JSGlobalObject* globalObject, JSObject* thisObject, uint64_t length)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    constexpr std::pair<SpeciesConstructResult, JSObject*> exceptionResult { SpeciesConstructResult::Exception, nullptr };

    // ECMA 9.4.2.3: https://tc39.github.io/ecma262/#sec-arrayspeciescreate
    JSValue constructor = jsUndefined();
    bool thisIsArray = isArray(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, exceptionResult);
    if (thisIsArray) [[likely]] {
        // Fast path in the normal case where the user has not set an own constructor and the Array.prototype.constructor is normal.
        // We need prototype check for subclasses of Array, which are Array objects but have a different prototype by default.
        bool isValid = arraySpeciesWatchpointIsValid(vm, thisObject);
        RETURN_IF_EXCEPTION(scope, exceptionResult);
        if (isValid) [[likely]]
            return std::pair { SpeciesConstructResult::FastPath, nullptr };

        constructor = thisObject->get(globalObject, vm.propertyNames->constructor);
        RETURN_IF_EXCEPTION(scope, exceptionResult);
        if (constructor.isConstructor()) {
            JSObject* constructorObject = jsCast<JSObject*>(constructor);
            bool isArrayConstructorFromAnotherRealm = globalObject != constructorObject->globalObject()
                && constructorObject->inherits<ArrayConstructor>();
            if (isArrayConstructorFromAnotherRealm)
                return std::pair { SpeciesConstructResult::FastPath, nullptr };
        }
        if (constructor.isObject()) {
            constructor = constructor.get(globalObject, vm.propertyNames->speciesSymbol);
            RETURN_IF_EXCEPTION(scope, exceptionResult);
            if (constructor.isNull())
                return std::pair { SpeciesConstructResult::FastPath, nullptr };
        }
    } else {
        // If isArray is false, return ? ArrayCreate(length).
        return std::pair { SpeciesConstructResult::FastPath, nullptr };
    }

    if (constructor.isUndefined())
        return std::pair { SpeciesConstructResult::FastPath, nullptr };

    MarkedArgumentBuffer args;
    args.append(jsNumber(length));
    ASSERT(!args.hasOverflowed());
    JSObject* newObject = construct(globalObject, constructor, args, "Species construction did not get a valid constructor"_s);
    RETURN_IF_EXCEPTION(scope, exceptionResult);
    return std::pair { SpeciesConstructResult::CreatedObject, newObject };
}

ALWAYS_INLINE void setLength(JSGlobalObject* globalObject, VM& vm, JSObject* obj, uint64_t value)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    static constexpr bool throwException = true;
    if (isJSArray(obj)) [[likely]] {
        if (value > UINT32_MAX) [[unlikely]] {
            throwRangeError(globalObject, scope, "Invalid array length"_s);
            return;
        }
        scope.release();
        jsCast<JSArray*>(obj)->setLength(globalObject, static_cast<uint32_t>(value), throwException);
        return;
    }
    scope.release();
    PutPropertySlot slot(obj, throwException);
    obj->methodTable()->put(obj, globalObject, vm.propertyNames->length, jsNumber(value), slot);
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
void shift(JSGlobalObject* globalObject, JSObject* thisObj, uint64_t header, uint64_t currentCount, uint64_t resultCount, uint64_t length)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    RELEASE_ASSERT(currentCount > resultCount);
    uint64_t count = currentCount - resultCount;

    RELEASE_ASSERT(header <= length);
    RELEASE_ASSERT(currentCount <= (length - header));

    if (isJSArray(thisObj)) {
        JSArray* array = asArray(thisObj);
        uint32_t header32 = static_cast<uint32_t>(header);
        ASSERT(header32 == header);
        if (array->length() == length && array->shiftCount<shiftCountMode>(globalObject, header32, static_cast<uint32_t>(count)))
            return;
        header = header32;
    }

    for (uint64_t k = header; k < length - currentCount; ++k) {
        uint64_t from = k + currentCount;
        uint64_t to = k + resultCount;
        JSValue value = getProperty(globalObject, thisObj, from);
        RETURN_IF_EXCEPTION(scope, void());
        if (value) {
            thisObj->putByIndexInline(globalObject, to, value, true);
            RETURN_IF_EXCEPTION(scope, void());
        } else {
            bool success = thisObj->deleteProperty(globalObject, to);
            RETURN_IF_EXCEPTION(scope, void());
            if (!success) {
                throwTypeError(globalObject, scope, UnableToDeletePropertyError);
                return;
            }
        }
    }
    for (uint64_t k = length; k > length - count; --k) {
        bool success = thisObj->deleteProperty(globalObject, k - 1);
        RETURN_IF_EXCEPTION(scope, void());
        if (!success) {
            throwTypeError(globalObject, scope, UnableToDeletePropertyError);
            return;
        }
    }
}

inline void unshift(JSGlobalObject* globalObject, JSObject* thisObj, uint64_t header, uint64_t currentCount, uint64_t resultCount, uint64_t length)
{
    ASSERT(header <= maxSafeInteger());
    ASSERT(currentCount <= maxSafeInteger());
    ASSERT(resultCount <= maxSafeInteger());
    ASSERT(length <= maxSafeInteger());

    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    RELEASE_ASSERT(resultCount > currentCount);
    uint64_t count = resultCount - currentCount;

    RELEASE_ASSERT(header <= length);
    RELEASE_ASSERT(currentCount <= (length - header));

    if (isJSArray(thisObj)) {
        // Spec says if we would produce an array of this size, we must throw a range error.
        if (count + length > std::numeric_limits<uint32_t>::max()) {
            throwRangeError(globalObject, scope, LengthExceededTheMaximumArrayLengthError);
            return;
        }

        JSArray* array = asArray(thisObj);
        if (array->length() == length) {
            bool handled = array->unshiftCount(globalObject, static_cast<uint32_t>(header), static_cast<uint32_t>(count));
            EXCEPTION_ASSERT(!scope.exception() || handled);
            if (handled)
                return;
        }
    }

    for (uint64_t k = length - currentCount; k > header; --k) {
        uint64_t from = k + currentCount - 1;
        uint64_t to = k + resultCount - 1;
        JSValue value = getProperty(globalObject, thisObj, from);
        RETURN_IF_EXCEPTION(scope, void());
        if (value) {
            thisObj->putByIndexInline(globalObject, to, value, true);
            RETURN_IF_EXCEPTION(scope, void());
        } else {
            bool success = thisObj->deleteProperty(globalObject, to);
            RETURN_IF_EXCEPTION(scope, void());
            if (!success) [[unlikely]] {
                throwTypeError(globalObject, scope, UnableToDeletePropertyError);
                return;
            }
        }
    }
}

inline Structure* ArrayPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(DerivedArrayType, StructureFlags), info(), ArrayClass);
}

inline bool holesMustForwardToPrototype(JSObject* object)
{
    return object->structure()->holesMustForwardToPrototype(object);
}

inline bool canUseFastArrayJoin(const JSObject* thisObject)
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

// This is intentionally supporting non-JSArray as well.
ALWAYS_INLINE JSString* fastArrayJoin(JSGlobalObject* globalObject, JSObject* thisObject, StringView separator, unsigned length, bool& sawHoles, bool& genericCase)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSStringJoiner joiner(separator);

    unsigned i = 0;
    switch (thisObject->indexingType()) {
    case ALL_INT32_INDEXING_TYPES: {
        auto& butterfly = *thisObject->butterfly();
        if (length > butterfly.publicLength()) [[unlikely]]
            break;
        joiner.reserveCapacity(globalObject, length);
        RETURN_IF_EXCEPTION(scope, { });
        auto data = butterfly.contiguous().data();
        bool holesKnownToBeOK = false;
        for (; i < length; ++i) {
            JSValue value = data[i].get();
            if (value) [[likely]]
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
        unsigned originalLength = butterfly.publicLength();
        if (length > originalLength) [[unlikely]]
            break;
        auto data = butterfly.contiguous().data();
        bool holesKnownToBeOK = false;

        JSOnlyStringsAndInt32sJoiner onlyStringsJoiner(separator);
        if (auto joined = onlyStringsJoiner.tryJoin(globalObject, data, length))
            RELEASE_AND_RETURN(scope, joined);
        RETURN_IF_EXCEPTION(scope, { });

        for (; i < length; ++i) {
            if (JSValue value = data[i].get()) {
                bool withoutSideEffect = joiner.append(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
                if (!withoutSideEffect) {
                    genericCase = true;
                    if (thisObject->butterfly() == &butterfly && originalLength == butterfly.publicLength()) [[likely]]
                        continue;
                    ++i;
                    goto generalCase;
                }
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
        if (length > butterfly.publicLength()) [[unlikely]]
            break;
        joiner.reserveCapacity(globalObject, length);
        RETURN_IF_EXCEPTION(scope, { });
        auto data = butterfly.contiguousDouble().data();
        bool holesKnownToBeOK = false;
        for (; i < length; ++i) {
            double value = data[i];
            if (!isHole(value)) [[likely]]
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
                RELEASE_AND_RETURN(scope, repeatCharacter(globalObject, separator.span8().front(), length - 1));
            RELEASE_AND_RETURN(scope, repeatCharacter(globalObject, separator.span16().front(), length - 1));
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

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

/*
 * Copyright (C) 2015-2022 Apple Inc. All rights reserved.
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
/*
 * Part of TypedArray#fill code derived from bun, MIT licensed.
 * https://github.com/Jarred-Sumner/bun-releases-for-updater
 *
 * Copyright (C) 2022 Jarred Sumner. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/

#pragma once

#include "Error.h"
#include "JSArrayBufferViewInlines.h"
#include "JSCBuiltins.h"
#include "JSCJSValueInlines.h"
#include "JSFunction.h"
#include "JSGenericTypedArrayViewInlines.h"
#include "JSGenericTypedArrayViewPrototypeInlines.h"
#include "JSStringJoiner.h"
#include "StructureInlines.h"
#include "TypedArrayAdaptors.h"
#include "TypedArrayController.h"
#include <wtf/StdLibExtras.h>

#if OS(DARWIN)
#include <strings.h>
#endif

namespace JSC {

namespace JSGenericTypedArrayViewPrototypeFunctionsInternal {
static constexpr bool verbose = false;
}

template<typename ViewClass>
ALWAYS_INLINE bool speciesWatchpointIsValid(JSGlobalObject* globalObject, ViewClass* thisObject)
{
    auto* prototype = globalObject->typedArrayPrototype(ViewClass::TypedArrayStorageType);

    if (globalObject->typedArraySpeciesWatchpointSet(ViewClass::TypedArrayStorageType).state() == ClearWatchpoint) {
        globalObject->tryInstallTypedArraySpeciesWatchpoint(ViewClass::TypedArrayStorageType);
        ASSERT(globalObject->typedArraySpeciesWatchpointSet(ViewClass::TypedArrayStorageType).state() != ClearWatchpoint);
    }

    return !thisObject->hasCustomProperties()
        && prototype == thisObject->getPrototypeDirect()
        && globalObject->typedArraySpeciesWatchpointSet(ViewClass::TypedArrayStorageType).state() == IsWatched
        && globalObject->typedArrayConstructorSpeciesWatchpointSet().state() == IsWatched;
}

// This implements 22.2.4.7 TypedArraySpeciesCreate
// Note, that this function throws.
// https://tc39.es/ecma262/#typedarray-species-create
template<typename ViewClass, typename Functor>
inline JSArrayBufferView* speciesConstruct(JSGlobalObject* globalObject, ViewClass* exemplar, MarkedArgumentBuffer& args, const Functor& defaultConstructor)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool inSameRealm = exemplar->globalObject() == globalObject;
    if (LIKELY(inSameRealm)) {
        if (LIKELY(speciesWatchpointIsValid(globalObject, exemplar)))
            RELEASE_AND_RETURN(scope, defaultConstructor());
    }

    JSValue constructorValue = exemplar->get(globalObject, vm.propertyNames->constructor);
    RETURN_IF_EXCEPTION(scope, nullptr);

    if (constructorValue.isUndefined())
        RELEASE_AND_RETURN(scope, defaultConstructor());

    if (!constructorValue.isObject()) {
        throwTypeError(globalObject, scope, "constructor Property should not be null"_s);
        return nullptr;
    }

    // Even though exemplar is extended, still we can try to use watchpoints to avoid @@species lookup if the obtained constructor is ViewClass's constructor.
    JSObject* viewClassConstructor = globalObject->typedArrayConstructor(ViewClass::TypedArrayStorageType);
    JSObject* constructor = jsCast<JSObject*>(constructorValue);
    if (LIKELY(constructor == viewClassConstructor)) {
        if (LIKELY(inSameRealm && globalObject->typedArraySpeciesWatchpointSet(ViewClass::TypedArrayStorageType).state() == IsWatched && globalObject->typedArrayConstructorSpeciesWatchpointSet().state() == IsWatched))
            RELEASE_AND_RETURN(scope, defaultConstructor());
    }

    JSValue species = constructor->get(globalObject, vm.propertyNames->speciesSymbol);
    RETURN_IF_EXCEPTION(scope, nullptr);

    if (species.isUndefinedOrNull())
        RELEASE_AND_RETURN(scope, defaultConstructor());

    // If species constructor ends up the same to viewClassConstructor, let's use default fast path.
    if (species == viewClassConstructor)
        RELEASE_AND_RETURN(scope, defaultConstructor());

    JSValue result = construct(globalObject, species, args, "species is not a constructor"_s);
    RETURN_IF_EXCEPTION(scope, nullptr);

    if (JSArrayBufferView* view = jsDynamicCast<JSArrayBufferView*>(result)) {
        if (view->type() == DataViewType) {
            throwTypeError(globalObject, scope, "species constructor did not return a TypedArray View"_s);
            return nullptr;
        }

        if (LIKELY(!view->isDetached()))
            return view;

        throwTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);
        return nullptr;
    }

    throwTypeError(globalObject, scope, "species constructor did not return a TypedArray View"_s);
    return nullptr;
}

inline size_t argumentClampedIndexFromStartOrEnd(JSGlobalObject* globalObject, JSValue value, size_t length, size_t undefinedValue = 0)
{
    if (value.isUndefined())
        return undefinedValue;

    double indexDouble = value.toIntegerOrInfinity(globalObject);
    if (indexDouble < 0) {
        indexDouble += length;
        return indexDouble < 0 ? 0 : static_cast<size_t>(indexDouble);
    }
    return indexDouble > length ? length : static_cast<size_t>(indexDouble);
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewProtoFuncSet(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 22.2.3.22
    ViewClass* thisObject = jsCast<ViewClass*>(callFrame->thisValue());

    if (UNLIKELY(!callFrame->argumentCount()))
        return throwVMTypeError(globalObject, scope, "Expected at least one argument"_s);

    size_t offset;
    if (callFrame->argumentCount() >= 2) {
        double offsetNumber = callFrame->uncheckedArgument(1).toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        if (UNLIKELY(offsetNumber < 0))
            return throwVMRangeError(globalObject, scope, "Offset should not be negative"_s);
        if (offsetNumber <= maxSafeInteger() && offsetNumber <= static_cast<double>(std::numeric_limits<size_t>::max()))
            offset = offsetNumber;
        else
            offset = std::numeric_limits<size_t>::max();
    } else
        offset = 0;

    if (UNLIKELY(thisObject->isDetached()))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    JSObject* sourceArray = callFrame->uncheckedArgument(0).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    size_t length;
    if (isTypedView(sourceArray->type())) {
        JSArrayBufferView* sourceView = jsCast<JSArrayBufferView*>(sourceArray);
        if (UNLIKELY(sourceView->isDetached()))
            return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

        length = jsCast<JSArrayBufferView*>(sourceArray)->length();
    } else {
        JSValue lengthValue = sourceArray->get(globalObject, vm.propertyNames->length);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        length = lengthValue.toLength(globalObject);
    }

    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    scope.release();
    thisObject->set(globalObject, offset, sourceArray, 0, length, CopyType::Unobservable);
    return JSValue::encode(jsUndefined());
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewProtoFuncCopyWithin(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 22.2.3.5
    ViewClass* thisObject = jsCast<ViewClass*>(callFrame->thisValue());
    if (UNLIKELY(thisObject->isDetached()))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    size_t length = thisObject->length();
    size_t to = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(0), length);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    size_t from = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(1), length);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    size_t final = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(2), length, length);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (final < from)
        return JSValue::encode(callFrame->thisValue());

    ASSERT(to <= length);
    ASSERT(from <= length);
    size_t count = std::min(length - std::max(to, from), final - from);

    if (UNLIKELY(thisObject->isDetached()))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    typename ViewClass::ElementType* array = thisObject->typedVector();
    memmove(array + to, array + from, count * thisObject->elementSize);

    return JSValue::encode(callFrame->thisValue());
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewProtoFuncIncludes(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    ViewClass* thisObject = jsCast<ViewClass*>(callFrame->thisValue());
    if (UNLIKELY(thisObject->isDetached()))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    size_t length = thisObject->length();

    if (!length)
        return JSValue::encode(jsBoolean(false));

    JSValue valueToFind = callFrame->argument(0);

    size_t index = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(1), length);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (UNLIKELY(thisObject->isDetached()))
        return JSValue::encode(jsBoolean(valueToFind.isUndefined()));

    typename ViewClass::ElementType* array = thisObject->typedVector();
    auto targetOption = ViewClass::toAdaptorNativeFromValueWithoutCoercion(valueToFind);
    if (!targetOption)
        return JSValue::encode(jsBoolean(false));

    scope.assertNoExceptionExceptTermination();
    RELEASE_ASSERT(!thisObject->isDetached());

    if constexpr (ViewClass::Adaptor::isFloat) {
        if (std::isnan(static_cast<double>(*targetOption))) {
            for (; index < length; ++index) {
                if (std::isnan(static_cast<double>(array[index])))
                    return JSValue::encode(jsBoolean(true));
            }
        }
    }

    if constexpr (ViewClass::Adaptor::isInteger) {
        if constexpr (ViewClass::elementSize == 1) {
            if (index < length) {
                auto* result = static_cast<typename ViewClass::ElementType*>(memchr(array + index, targetOption.value(), length - index));
                return JSValue::encode(jsBoolean(!!result));
            }
            return JSValue::encode(jsBoolean(false));
        }

#if HAVE(MEMCHR16)
        if constexpr (ViewClass::elementSize == 2) {
            if (index < length) {
                auto* result = bitwise_cast<typename ViewClass::ElementType*>(WTF::memchr16(bitwise_cast<const uint16_t*>(array + index), targetOption.value(), length - index));
                return JSValue::encode(jsBoolean(!!result));
            }
            return JSValue::encode(jsBoolean(false));
        }
#endif
    }

    for (; index < length; ++index) {
        if (array[index] == targetOption.value())
            return JSValue::encode(jsBoolean(true));
    }

    return JSValue::encode(jsBoolean(false));
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewProtoFuncIndexOf(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 22.2.3.13
    ViewClass* thisObject = jsCast<ViewClass*>(callFrame->thisValue());
    if (UNLIKELY(thisObject->isDetached()))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    size_t length = thisObject->length();

    if (!length)
        return JSValue::encode(jsNumber(-1));

    JSValue valueToFind = callFrame->argument(0);
    size_t index = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(1), length);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (UNLIKELY(thisObject->isDetached()))
        return JSValue::encode(jsNumber(-1));

    typename ViewClass::ElementType* array = thisObject->typedVector();
    auto targetOption = ViewClass::toAdaptorNativeFromValueWithoutCoercion(valueToFind);
    if (!targetOption)
        return JSValue::encode(jsNumber(-1));
    scope.assertNoExceptionExceptTermination();
    RELEASE_ASSERT(!thisObject->isDetached());

    if constexpr (ViewClass::Adaptor::isInteger) {
        if constexpr (ViewClass::elementSize == 1) {
            if (index < length) {
                auto* result = static_cast<typename ViewClass::ElementType*>(memchr(array + index, targetOption.value(), length - index));
                if (result)
                    return JSValue::encode(jsNumber(result - array));
            }
            return JSValue::encode(jsNumber(-1));
        }

#if HAVE(MEMCHR16)
        if constexpr (ViewClass::elementSize == 2) {
            if (index < length) {
                auto* result = bitwise_cast<typename ViewClass::ElementType*>(WTF::memchr16(bitwise_cast<const uint16_t*>(array + index), targetOption.value(), length - index));
                if (result)
                    return JSValue::encode(jsNumber(result - array));
            }
            return JSValue::encode(jsNumber(-1));
        }
#endif
    }

    for (; index < length; ++index) {
        if (array[index] == targetOption.value())
            return JSValue::encode(jsNumber(index));
    }

    return JSValue::encode(jsNumber(-1));
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewProtoFuncJoin(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    ViewClass* thisObject = jsCast<ViewClass*>(callFrame->thisValue());
    if (UNLIKELY(thisObject->isDetached()))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    size_t length = thisObject->length();
    auto joinWithSeparator = [&] (StringView separator) -> EncodedJSValue {
        JSStringJoiner joiner(globalObject, separator, length);
        RETURN_IF_EXCEPTION(scope, { });
        if (LIKELY(!thisObject->isDetached())) {
            for (size_t i = 0; i < length; i++) {
                JSValue value;
                if constexpr (ViewClass::Adaptor::canConvertToJSQuickly)
                    value = thisObject->getIndexQuickly(i);
                else {
                    auto nativeValue = thisObject->getIndexQuicklyAsNativeValue(i);
                    value = ViewClass::Adaptor::toJSValue(globalObject, nativeValue);
                    RETURN_IF_EXCEPTION(scope, { });
                }
                joiner.append(globalObject, value);
                RETURN_IF_EXCEPTION(scope, { });
            }
        } else {
            for (size_t i = 0; i < length; i++)
                joiner.appendEmptyString();
        }
        RELEASE_AND_RETURN(scope, JSValue::encode(joiner.join(globalObject)));
    };

    JSValue separatorValue = callFrame->argument(0);
    if (separatorValue.isUndefined()) {
        const LChar* comma = reinterpret_cast<const LChar*>(",");
        return joinWithSeparator({ comma, 1 });
    }

    JSString* separatorString = separatorValue.toString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    auto viewWithString = separatorString->viewWithUnderlyingString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    return joinWithSeparator(viewWithString.view);
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewProtoFuncFill(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame)
{
    // https://tc39.es/ecma262/#sec-%typedarray%.prototype.fill
    auto scope = DECLARE_THROW_SCOPE(vm);

    ViewClass* thisObject = jsCast<ViewClass*>(callFrame->thisValue());
    if (UNLIKELY(thisObject->isDetached()))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    size_t length = thisObject->length();
    typename ViewClass::ElementType nativeValue = ViewClass::toAdaptorNativeFromValue(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    size_t start = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(1), length, 0);
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(start <= length);

    size_t end = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(2), length, length);
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(end <= length);

    if (UNLIKELY(thisObject->isDetached()))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    if (!(start < end))
        return JSValue::encode(thisObject);

    size_t count = end - start;
    typename ViewClass::ElementType* underlyingVector = thisObject->typedVector();
    ASSERT(count <= length);

#if OS(DARWIN)
    if constexpr (ViewClass::elementSize == 8) {
        static_assert(sizeof(decltype(nativeValue)) == 8);
        memset_pattern8(underlyingVector + start, &nativeValue, count * ViewClass::elementSize);
        return JSValue::encode(thisObject);
    }

    if constexpr (ViewClass::elementSize == 4) {
        static_assert(sizeof(decltype(nativeValue)) == 4);
        memset_pattern4(underlyingVector + start, &nativeValue, count * ViewClass::elementSize);
        return JSValue::encode(thisObject);
    }
#endif

    if constexpr (ViewClass::elementSize == 1) {
        static_assert(sizeof(decltype(nativeValue)) == 1);
        memset(underlyingVector + start, nativeValue, count * ViewClass::elementSize);
        return JSValue::encode(thisObject);
    }

    std::fill(underlyingVector + start, underlyingVector + end, nativeValue);
    return JSValue::encode(thisObject);
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewProtoFuncLastIndexOf(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 22.2.3.16
    ViewClass* thisObject = jsCast<ViewClass*>(callFrame->thisValue());
    if (UNLIKELY(thisObject->isDetached()))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    size_t length = thisObject->length();

    if (!length)
        return JSValue::encode(jsNumber(-1));

    JSValue valueToFind = callFrame->argument(0);

    size_t index = length - 1;
    if (callFrame->argumentCount() >= 2) {
        JSValue fromValue = callFrame->uncheckedArgument(1);
        double fromDouble = fromValue.toIntegerOrInfinity(globalObject);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        if (fromDouble < 0) {
            fromDouble += length;
            if (fromDouble < 0)
                return JSValue::encode(jsNumber(-1));
        }
        if (fromDouble < length)
            index = static_cast<size_t>(fromDouble);
    }

    if (UNLIKELY(thisObject->isDetached()))
        return JSValue::encode(jsNumber(-1));

    auto targetOption = ViewClass::toAdaptorNativeFromValueWithoutCoercion(valueToFind);
    if (!targetOption)
        return JSValue::encode(jsNumber(-1));

    typename ViewClass::ElementType* array = thisObject->typedVector();
    scope.assertNoExceptionExceptTermination();
    RELEASE_ASSERT(!thisObject->isDetached());

    // We always have at least one iteration, since we checked that length is different from 0 earlier.
    do {
        if (array[index] == targetOption.value())
            return JSValue::encode(jsNumber(index));
        if (!index)
            break;
        --index;
    } while (true);

    return JSValue::encode(jsNumber(-1));
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewProtoGetterFuncBuffer(VM&, JSGlobalObject* globalObject, CallFrame* callFrame)
{
    // 22.2.3.3
    ViewClass* thisObject = jsCast<ViewClass*>(callFrame->thisValue());

    return JSValue::encode(thisObject->possiblySharedJSBuffer(globalObject));
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewProtoGetterFuncLength(VM&, JSGlobalObject*, CallFrame* callFrame)
{
    // 22.2.3.17
    ViewClass* thisObject = jsCast<ViewClass*>(callFrame->thisValue());

    return JSValue::encode(jsNumber(thisObject->length()));
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewProtoGetterFuncByteLength(VM&, JSGlobalObject*, CallFrame* callFrame)
{
    // 22.2.3.2
    ViewClass* thisObject = jsCast<ViewClass*>(callFrame->thisValue());

    return JSValue::encode(jsNumber(thisObject->byteLength()));
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewProtoGetterFuncByteOffset(VM&, JSGlobalObject*, CallFrame* callFrame)
{
    // 22.2.3.3
    ViewClass* thisObject = jsCast<ViewClass*>(callFrame->thisValue());

    return JSValue::encode(jsNumber(thisObject->byteOffset()));
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewProtoFuncReverse(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 22.2.3.21
    ViewClass* thisObject = jsCast<ViewClass*>(callFrame->thisValue());
    if (UNLIKELY(thisObject->isDetached()))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    typename ViewClass::ElementType* array = thisObject->typedVector();
    std::reverse(array, array + thisObject->length());

    return JSValue::encode(thisObject);
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewPrivateFuncSort(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 22.2.3.25
    ViewClass* thisObject = jsCast<ViewClass*>(callFrame->argument(0));
    if (UNLIKELY(thisObject->isDetached()))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    thisObject->sort();

    return JSValue::encode(thisObject);
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewProtoFuncSlice(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 22.2.3.26

    ViewClass* thisObject = jsCast<ViewClass*>(callFrame->thisValue());
    if (UNLIKELY(thisObject->isDetached()))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    size_t thisLength = thisObject->length();

    size_t begin = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(0), thisLength);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    size_t end = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(1), thisLength, thisLength);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    if (UNLIKELY(thisObject->isDetached()))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    // Clamp end to begin.
    end = std::max(begin, end);

    ASSERT(end >= begin);
    size_t length = end - begin;

    MarkedArgumentBuffer args;
    args.append(jsNumber(length));
    ASSERT(!args.hasOverflowed());

    JSArrayBufferView* result = speciesConstruct(globalObject, thisObject, args, [&]() {
        Structure* structure = globalObject->typedArrayStructure(ViewClass::TypedArrayStorageType);
        return ViewClass::createUninitialized(globalObject, structure, length);
    });
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    ASSERT(!result->isDetached());

    // https://tc39.es/ecma262/#typedarray-species-create
    // If result.[[ContentType]] ≠ exemplar.[[ContentType]], throw a TypeError exception.
    if (contentType(result->type()) != ViewClass::contentType)
        return throwVMTypeError(globalObject, scope, "Content types of source and created typed arrays are different"_s);

    // We return early here since we don't allocate a backing store if length is 0 and memmove does not like nullptrs
    if (!length)
        return JSValue::encode(result);

    if (UNLIKELY(thisObject->isDetached()))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    // The species constructor may return an array with any arbitrary length.
    if (result->length() < length)
        return throwVMTypeError(globalObject, scope, "TypedArray.prototype.slice constructed typed array of insufficient length"_s);

    switch (result->type()) {
    case Int8ArrayType:
        scope.release();
        jsCast<JSInt8Array*>(result)->set(globalObject, 0, thisObject, begin, length, CopyType::LeftToRight);
        return JSValue::encode(result);
    case Int16ArrayType:
        scope.release();
        jsCast<JSInt16Array*>(result)->set(globalObject, 0, thisObject, begin, length, CopyType::LeftToRight);
        return JSValue::encode(result);
    case Int32ArrayType:
        scope.release();
        jsCast<JSInt32Array*>(result)->set(globalObject, 0, thisObject, begin, length, CopyType::LeftToRight);
        return JSValue::encode(result);
    case Uint8ArrayType:
        scope.release();
        jsCast<JSUint8Array*>(result)->set(globalObject, 0, thisObject, begin, length, CopyType::LeftToRight);
        return JSValue::encode(result);
    case Uint8ClampedArrayType:
        scope.release();
        jsCast<JSUint8ClampedArray*>(result)->set(globalObject, 0, thisObject, begin, length, CopyType::LeftToRight);
        return JSValue::encode(result);
    case Uint16ArrayType:
        scope.release();
        jsCast<JSUint16Array*>(result)->set(globalObject, 0, thisObject, begin, length, CopyType::LeftToRight);
        return JSValue::encode(result);
    case Uint32ArrayType:
        scope.release();
        jsCast<JSUint32Array*>(result)->set(globalObject, 0, thisObject, begin, length, CopyType::LeftToRight);
        return JSValue::encode(result);
    case Float32ArrayType:
        scope.release();
        jsCast<JSFloat32Array*>(result)->set(globalObject, 0, thisObject, begin, length, CopyType::LeftToRight);
        return JSValue::encode(result);
    case Float64ArrayType:
        scope.release();
        jsCast<JSFloat64Array*>(result)->set(globalObject, 0, thisObject, begin, length, CopyType::LeftToRight);
        return JSValue::encode(result);
    case BigInt64ArrayType:
        scope.release();
        jsCast<JSBigInt64Array*>(result)->set(globalObject, 0, thisObject, begin, length, CopyType::LeftToRight);
        return JSValue::encode(result);
    case BigUint64ArrayType:
        scope.release();
        jsCast<JSBigUint64Array*>(result)->set(globalObject, 0, thisObject, begin, length, CopyType::LeftToRight);
        return JSValue::encode(result);
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewPrivateFuncSubarrayCreate(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame)
{
    DeferTermination deferScope(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 22.2.3.23

    ViewClass* thisObject = jsCast<ViewClass*>(callFrame->thisValue());
    if (UNLIKELY(thisObject->isDetached()))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    // Get the length here; later assert that the length didn't change.
    size_t thisLength = thisObject->length();

    // I would assert that the arguments are integers here but that's not true since
    // https://tc39.github.io/ecma262/#sec-tointeger allows the result of the operation
    // to be +/- Infinity and -0.
    ASSERT(callFrame->argument(0).isNumber());
    ASSERT(callFrame->argument(1).isUndefined() || callFrame->argument(1).isNumber());
    size_t begin = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(0), thisLength);
    scope.assertNoException();
    size_t end = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(1), thisLength, thisLength);
    scope.assertNoException();

    RELEASE_ASSERT(!thisObject->isDetached());

    // Clamp end to begin.
    end = std::max(begin, end);

    ASSERT(end >= begin);
    size_t offset = begin;
    size_t length = end - begin;

    RefPtr<ArrayBuffer> arrayBuffer = thisObject->possiblySharedBuffer();
    if (UNLIKELY(!arrayBuffer)) {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }
    RELEASE_ASSERT(thisLength == thisObject->length());

    size_t newByteOffset = thisObject->byteOffset() + offset * ViewClass::elementSize;

    JSObject* defaultConstructor = globalObject->typedArrayConstructor(ViewClass::TypedArrayStorageType);
    JSValue species = callFrame->uncheckedArgument(2);
    if (species == defaultConstructor) {
        Structure* structure = globalObject->typedArrayStructure(ViewClass::TypedArrayStorageType);

        RELEASE_AND_RETURN(scope, JSValue::encode(ViewClass::create(
            globalObject, structure, WTFMove(arrayBuffer),
            thisObject->byteOffset() + offset * ViewClass::elementSize,
            length)));
    }

    MarkedArgumentBuffer args;
    args.append(vm.m_typedArrayController->toJS(globalObject, thisObject->globalObject(), arrayBuffer.get()));
    args.append(jsNumber(newByteOffset));
    args.append(jsNumber(length));
    ASSERT(!args.hasOverflowed());

    JSObject* result = construct(globalObject, species, args, "species is not a constructor"_s);
    RETURN_IF_EXCEPTION(scope, { });

    JSArrayBufferView* validated = validateTypedArray(globalObject, result);
    RETURN_IF_EXCEPTION(scope, { });
    if (contentType(validated->type()) != ViewClass::contentType)
        return throwVMTypeError(globalObject, scope, "TypedArray.prototype.subarray constructed typed array of different content type from |this|"_s);

    return JSValue::encode(validated);
}

} // namespace JSC

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
template<typename ViewClass, typename Functor, typename SlowPathArgsConstructor>
inline JSArrayBufferView* speciesConstruct(JSGlobalObject* globalObject, ViewClass* exemplar, const Functor& defaultConstructor, const SlowPathArgsConstructor& constructArgs, std::optional<size_t> length)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool inSameRealm = exemplar->globalObject() == globalObject;
    if (LIKELY(inSameRealm)) {
        bool isValid = speciesWatchpointIsValid(globalObject, exemplar);
        RETURN_IF_EXCEPTION(scope, nullptr);
        if (LIKELY(isValid))
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

    MarkedArgumentBuffer args;
    constructArgs(args);
    RETURN_IF_EXCEPTION(scope, nullptr);

    JSValue result = construct(globalObject, species, args, "species is not a constructor"_s);
    RETURN_IF_EXCEPTION(scope, nullptr);

    if (JSArrayBufferView* view = jsDynamicCast<JSArrayBufferView*>(result)) {
        if (view->type() == DataViewType) {
            throwTypeError(globalObject, scope, "species constructor did not return a TypedArray View"_s);
            return nullptr;
        }

        validateTypedArray(globalObject, view);
        RETURN_IF_EXCEPTION(scope, nullptr);

        // https://tc39.es/ecma262/#typedarray-create
        // 3. If argumentList is a List of a single Number, then
        // a. If newTypedArray.[[ArrayLength]] < R(argumentList[0]), throw a TypeError exception.
        if (length) {
            if (UNLIKELY(view->length() < length.value())) {
                throwTypeError(globalObject, scope, "TypedArray.prototype.slice constructed typed array of insufficient length"_s);
                return nullptr;
            }
        }

        // https://tc39.es/ecma262/#typedarray-species-create
        // If result.[[ContentType]] ≠ exemplar.[[ContentType]], throw a TypeError exception.
        if (UNLIKELY(contentType(view->type()) != ViewClass::contentType)) {
            throwTypeError(globalObject, scope, "Content types of source and created typed arrays are different"_s);
            return nullptr;
        }

        return view;
    }

    throwTypeError(globalObject, scope, "species constructor did not return a TypedArray View"_s);
    return nullptr;
}

inline size_t argumentClampedIndexFromStartOrEnd(JSGlobalObject* globalObject, JSValue value, size_t length, size_t undefinedValue = 0)
{
    if (value.isUndefined())
        return undefinedValue;

    if (LIKELY(value.isInt32())) {
        int64_t indexInt = value.asInt32();
        if (indexInt < 0) {
            indexInt += length;
            return indexInt < 0 ? 0 : static_cast<size_t>(indexInt);
        }
        return static_cast<size_t>(indexInt) > length ? length : static_cast<size_t>(indexInt);
    }

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
        RETURN_IF_EXCEPTION(scope, { });
        if (UNLIKELY(offsetNumber < 0))
            return throwVMRangeError(globalObject, scope, "Offset should not be negative"_s);
        if (offsetNumber <= maxSafeInteger() && offsetNumber <= static_cast<double>(std::numeric_limits<size_t>::max()))
            offset = offsetNumber;
        else
            offset = std::numeric_limits<size_t>::max();
    } else
        offset = 0;

    validateTypedArray(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });

    JSValue source = callFrame->uncheckedArgument(0);

    if (source.isObject() && isTypedView(asObject(source)->type())) {
        JSArrayBufferView* sourceView = jsCast<JSArrayBufferView*>(source);
        IdempotentArrayBufferByteLengthGetter<std::memory_order_seq_cst> getter;
        auto lengthValue = integerIndexedObjectLength(sourceView, getter);
        if (UNLIKELY(!lengthValue))
            return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);
        scope.release();
        thisObject->setFromTypedArray(globalObject, offset, sourceView, 0, lengthValue.value(), CopyType::Unobservable);
        return JSValue::encode(jsUndefined());
    }

    scope.release();
    thisObject->setFromArrayLike(globalObject, offset, source);
    return JSValue::encode(jsUndefined());
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewProtoFuncCopyWithin(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 22.2.3.5
    ViewClass* thisObject = jsCast<ViewClass*>(callFrame->thisValue());
    validateTypedArray(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });

    size_t length = thisObject->length();
    size_t to = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(0), length);
    RETURN_IF_EXCEPTION(scope, { });
    size_t from = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(1), length);
    RETURN_IF_EXCEPTION(scope, { });
    size_t final = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(2), length, length);
    RETURN_IF_EXCEPTION(scope, { });

    if (final < from)
        return JSValue::encode(callFrame->thisValue());

    ASSERT(to <= length);
    ASSERT(from <= length);
    size_t count = std::min(length - std::max(to, from), final - from);

    if (count > 0) {
        IdempotentArrayBufferByteLengthGetter<std::memory_order_seq_cst> getter;
        auto updatedLength = integerIndexedObjectLength(thisObject, getter);
        if (UNLIKELY(!updatedLength))
            return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

        // ResizableArrayBuffer can shrink the length. Thus, we need to check again to see whether we can copy things.
        // https://tc39.es/proposal-resizablearraybuffer/#sec-%typedarray%.prototype.copywithin
        if (updatedLength.value() != length) {
            length = updatedLength.value();
            if (std::max(to, from) + count > length)
                count = length - std::max(to, from);
        }

        typename ViewClass::ElementType* array = thisObject->typedVector();
        memmove(array + to, array + from, count * thisObject->elementSize);
    }

    return JSValue::encode(callFrame->thisValue());
}

template<typename ViewClass>
static ALWAYS_INLINE size_t typedArrayIndexOfImpl(typename ViewClass::ElementType* array, size_t length, typename ViewClass::ElementType target, size_t index)
{
    if (index >= length)
        return WTF::notFound;

    if constexpr (ViewClass::Adaptor::isInteger) {
        if constexpr (ViewClass::elementSize == 1) {
            auto* result = bitwise_cast<typename ViewClass::ElementType*>(WTF::find8(bitwise_cast<const uint8_t*>(array + index), target, length - index));
            if (result)
                return result - array;
            return WTF::notFound;
        }

        if constexpr (ViewClass::elementSize == 2) {
            auto* result = bitwise_cast<typename ViewClass::ElementType*>(WTF::find16(bitwise_cast<const uint16_t*>(array + index), target, length - index));
            if (result)
                return result - array;
            return WTF::notFound;
        }

        if constexpr (ViewClass::elementSize == 4) {
            auto* result = bitwise_cast<typename ViewClass::ElementType*>(WTF::find32(bitwise_cast<const uint32_t*>(array + index), target, length - index));
            if (result)
                return result - array;
            return WTF::notFound;
        }

        if constexpr (ViewClass::elementSize == 8) {
            auto* result = bitwise_cast<typename ViewClass::ElementType*>(WTF::find64(bitwise_cast<const uint64_t*>(array + index), target, length - index));
            if (result)
                return result - array;
            return WTF::notFound;
        }
    }

    if constexpr (ViewClass::Adaptor::isFloat) {
        if constexpr (ViewClass::elementSize == 4) {
            auto* result = bitwise_cast<typename ViewClass::ElementType*>(WTF::findFloat(bitwise_cast<const float*>(array + index), target, length - index));
            if (result)
                return result - array;
            return WTF::notFound;
        }

        if constexpr (ViewClass::elementSize == 8) {
            auto* result = bitwise_cast<typename ViewClass::ElementType*>(WTF::findDouble(bitwise_cast<const double*>(array + index), target, length - index));
            if (result)
                return result - array;
            return WTF::notFound;
        }
    }

    ASSERT_NOT_REACHED();
    return WTF::notFound;
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewProtoFuncIncludes(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    ViewClass* thisObject = jsCast<ViewClass*>(callFrame->thisValue());
    validateTypedArray(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });

    size_t length = thisObject->length();

    if (!length)
        return JSValue::encode(jsBoolean(false));

    JSValue valueToFind = callFrame->argument(0);

    size_t index = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(1), length);
    RETURN_IF_EXCEPTION(scope, { });

    size_t updatedLength = 0;
    {
        IdempotentArrayBufferByteLengthGetter<std::memory_order_seq_cst> getter;
        auto lengthValue = integerIndexedObjectLength(thisObject, getter);
        if (UNLIKELY(!lengthValue))
            return JSValue::encode(jsBoolean(valueToFind.isUndefined()));

        updatedLength = lengthValue.value();
    }

    typename ViewClass::ElementType* array = thisObject->typedVector();
    auto targetOption = ViewClass::toAdaptorNativeFromValueWithoutCoercion(valueToFind);
    if (!targetOption) {
        // Even though our TypedArray's length is updated, we iterate up to `length`.
        // So, if `updatedLength` is smaller than `length`, we will see undefined after that.
        return JSValue::encode(jsBoolean(valueToFind.isUndefined() && length > updatedLength));
    }

    scope.assertNoExceptionExceptTermination();
    RELEASE_ASSERT(!thisObject->isDetached());

    size_t searchLength = std::min<size_t>(length, updatedLength);
    if constexpr (ViewClass::Adaptor::isFloat) {
        if (std::isnan(static_cast<double>(*targetOption))) {
            for (; index < searchLength; ++index) {
                if (std::isnan(static_cast<double>(array[index])))
                    return JSValue::encode(jsBoolean(true));
            }
            return JSValue::encode(jsBoolean(false));
        }
    }

    size_t result = typedArrayIndexOfImpl<ViewClass>(array, searchLength, targetOption.value(), index);
    return JSValue::encode(jsBoolean(result != WTF::notFound));
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewProtoFuncIndexOf(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 22.2.3.13
    ViewClass* thisObject = jsCast<ViewClass*>(callFrame->thisValue());
    validateTypedArray(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });

    size_t length = thisObject->length();

    if (!length)
        return JSValue::encode(jsNumber(-1));

    JSValue valueToFind = callFrame->argument(0);
    size_t index = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(1), length);
    RETURN_IF_EXCEPTION(scope, { });

    size_t updatedLength = 0;
    {
        IdempotentArrayBufferByteLengthGetter<std::memory_order_seq_cst> getter;
        auto lengthValue = integerIndexedObjectLength(thisObject, getter);
        if (UNLIKELY(!lengthValue)) {
            // indexOf only sees elements when HasProperty passed. Thus, even though length gets smaller, the trailing undefineds are not checked.
            return JSValue::encode(jsNumber(-1));
        }

        updatedLength = lengthValue.value();
    }

    typename ViewClass::ElementType* array = thisObject->typedVector();
    auto targetOption = ViewClass::toAdaptorNativeFromValueWithoutCoercion(valueToFind);
    if (!targetOption)
        return JSValue::encode(jsNumber(-1));
    scope.assertNoExceptionExceptTermination();
    RELEASE_ASSERT(!thisObject->isDetached());

    size_t searchLength = std::min<size_t>(length, updatedLength);
    size_t result = typedArrayIndexOfImpl<ViewClass>(array, searchLength, targetOption.value(), index);
    if (result == WTF::notFound)
        return JSValue::encode(jsNumber(-1));
    return JSValue::encode(jsNumber(result));
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewProtoFuncJoin(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    ViewClass* thisObject = jsCast<ViewClass*>(callFrame->thisValue());
    validateTypedArray(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });

    size_t length = thisObject->length();
    auto joinWithSeparator = [&] (StringView separator) -> EncodedJSValue {
        IdempotentArrayBufferByteLengthGetter<std::memory_order_seq_cst> getter;
        auto updatedLength = integerIndexedObjectLength(thisObject, getter);
        if (UNLIKELY(!updatedLength)) {
            JSStringJoiner joiner(globalObject, separator, length);
            RETURN_IF_EXCEPTION(scope, { });
            for (size_t i = 0; i < length; i++)
                joiner.appendEmptyString();
            RELEASE_AND_RETURN(scope, JSValue::encode(joiner.join(globalObject)));
        }

        JSStringJoiner joiner(globalObject, separator, length);
        RETURN_IF_EXCEPTION(scope, { });

        size_t accessibleLength = std::min(length, updatedLength.value());

        for (size_t i = 0; i < accessibleLength; i++) {
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

        for (size_t i = accessibleLength; i < length; i++)
            joiner.appendEmptyString();

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
    validateTypedArray(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });

    size_t length = thisObject->length();
    typename ViewClass::ElementType nativeValue = ViewClass::toAdaptorNativeFromValue(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });

    size_t start = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(1), length, 0);
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(start <= length);

    size_t end = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(2), length, length);
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(end <= length);

    // ResizableArrayBuffer can shrink the length. Thus, we need to check again to see whether we can copy things.
    // https://tc39.es/proposal-resizablearraybuffer/#sec-%typedarray%.prototype.fill
    IdempotentArrayBufferByteLengthGetter<std::memory_order_seq_cst> getter;
    auto updatedLength = integerIndexedObjectLength(thisObject, getter);
    if (UNLIKELY(!updatedLength))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    end = std::min(end, updatedLength.value());

    if (!(start < end))
        return JSValue::encode(thisObject);

    size_t count = end - start;
    typename ViewClass::ElementType* underlyingVector = thisObject->typedVector();
    ASSERT_UNUSED(count, count <= length);

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
    validateTypedArray(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });

    size_t length = thisObject->length();

    if (!length)
        return JSValue::encode(jsNumber(-1));

    JSValue valueToFind = callFrame->argument(0);

    size_t index = length - 1;
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
            index = static_cast<size_t>(fromDouble);
    }

    {
        IdempotentArrayBufferByteLengthGetter<std::memory_order_seq_cst> getter;
        auto lengthValue = integerIndexedObjectLength(thisObject, getter);
        if (UNLIKELY(!lengthValue))
            return JSValue::encode(jsNumber(-1));

        length = lengthValue.value();
        if (!length)
            return JSValue::encode(jsNumber(-1));
        index = std::min(length - 1, index);
    }

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
    validateTypedArray(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });

    typename ViewClass::ElementType* array = thisObject->typedVector();
    std::reverse(array, array + thisObject->length());

    return JSValue::encode(thisObject);
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewProtoFuncToReversed(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame)
{
    // https://tc39.es/proposal-change-array-by-copy/#sec-%typedarray%.prototype.toReversed

    auto scope = DECLARE_THROW_SCOPE(vm);

    ViewClass* thisObject = jsCast<ViewClass*>(callFrame->thisValue());
    validateTypedArray(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });

    size_t length = thisObject->length();

    bool isResizableOrGrowableShared = false;
    Structure* structure = globalObject->typedArrayStructure(ViewClass::TypedArrayStorageType, isResizableOrGrowableShared);
    ViewClass* result = ViewClass::createUninitialized(globalObject, structure, length);
    RETURN_IF_EXCEPTION(scope, { });

    const typename ViewClass::ElementType* from = thisObject->typedVector();
    typename ViewClass::ElementType* to = result->typedVector();

    memmove(to, from, length * ViewClass::elementSize);
    std::reverse(to, to + length);

    return JSValue::encode(result);
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewPrivateFuncClone(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    ViewClass* thisObject = jsCast<ViewClass*>(callFrame->thisValue());
    validateTypedArray(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });

    size_t length = thisObject->length();
    bool isResizableOrGrowableShared = false;
    Structure* structure = globalObject->typedArrayStructure(ViewClass::TypedArrayStorageType, isResizableOrGrowableShared);
    ViewClass* result = ViewClass::createUninitialized(globalObject, structure, length);
    RETURN_IF_EXCEPTION(scope, { });

    typename ViewClass::ElementType* from = thisObject->typedVector();
    typename ViewClass::ElementType* to = result->typedVector();
    memmove(to, from, length * ViewClass::elementSize);
    return JSValue::encode(result);
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewPrivateFuncSort(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 22.2.3.25
    ViewClass* thisObject = jsCast<ViewClass*>(callFrame->argument(0));
    validateTypedArray(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (UNLIKELY(!thisObject->sort()))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    return JSValue::encode(thisObject);
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewPrivateFuncFromFast(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue arrayLike = callFrame->uncheckedArgument(1);
    JSArrayBufferView* items = jsDynamicCast<JSArrayBufferView*>(arrayLike);
    if (!items) {
        // Converting Double or Int32 to BigInt throws an error.
        if constexpr (ViewClass::TypedArrayStorageType == TypeBigInt64 || ViewClass::TypedArrayStorageType == TypeBigUint64)
            return JSValue::encode(jsUndefined());

        // TypedArray.from(Array) case.
        JSArray* array = jsDynamicCast<JSArray*>(arrayLike);
        if (!array)
            return JSValue::encode(jsUndefined());

        if (!array->isIteratorProtocolFastAndNonObservable())
            return JSValue::encode(jsUndefined());

        IndexingType indexingType = array->indexingType() & IndexingShapeMask;
        if (indexingType != Int32Shape && indexingType != DoubleShape)
            return JSValue::encode(jsUndefined());

        size_t length = array->length();

        bool isResizableOrGrowableShared = false;
        Structure* structure = globalObject->typedArrayStructure(ViewClass::TypedArrayStorageType, isResizableOrGrowableShared);
        ViewClass* result = ViewClass::createUninitialized(globalObject, structure, length);
        RETURN_IF_EXCEPTION(scope, { });

        if (indexingType == Int32Shape) {
            for (unsigned i = 0; i < length; i++) {
                JSValue value = array->butterfly()->contiguous().at(array, i).get();
                if (LIKELY(!!value))
                    result->setIndexQuicklyToNativeValue(i, ViewClass::Adaptor::toNativeFromInt32(value.asInt32()));
                else
                    result->setIndexQuicklyToNativeValue(i, ViewClass::Adaptor::toNativeFromUndefined());
            }
        } else {
            ASSERT(indexingType == DoubleShape);
            for (unsigned i = 0; i < length; i++) {
                double d = array->butterfly()->contiguousDouble().at(array, i);
                result->setIndexQuicklyToNativeValue(i, ViewClass::Adaptor::toNativeFromDouble(d));
            }
        }
        return JSValue::encode(result);
    }

    if (!items->isIteratorProtocolFastAndNonObservable())
        return JSValue::encode(jsUndefined());

    IdempotentArrayBufferByteLengthGetter<std::memory_order_seq_cst> getter;
    auto lengthValue = integerIndexedObjectLength(items, getter);
    if (UNLIKELY(!lengthValue))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);
    size_t length = lengthValue.value();

    bool isResizableOrGrowableShared = false;
    Structure* structure = globalObject->typedArrayStructure(ViewClass::TypedArrayStorageType, isResizableOrGrowableShared);
    ViewClass* result = ViewClass::createUninitialized(globalObject, structure, length);
    RETURN_IF_EXCEPTION(scope, { });

    scope.release();
    result->setFromTypedArray(globalObject, 0, items, 0, length, CopyType::Unobservable);
    return JSValue::encode(result);
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewProtoFuncSlice(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 22.2.3.26

    ViewClass* thisObject = jsCast<ViewClass*>(callFrame->thisValue());
    validateTypedArray(globalObject, thisObject);
    RETURN_IF_EXCEPTION(scope, { });

    size_t thisLength = 0;
    {
        IdempotentArrayBufferByteLengthGetter<std::memory_order_seq_cst> getter;
        auto lengthValue = integerIndexedObjectLength(thisObject, getter);
        if (UNLIKELY(!lengthValue))
            return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);
        thisLength = lengthValue.value();
    }

    size_t begin = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(0), thisLength);
    RETURN_IF_EXCEPTION(scope, { });
    size_t end = argumentClampedIndexFromStartOrEnd(globalObject, callFrame->argument(1), thisLength, thisLength);
    RETURN_IF_EXCEPTION(scope, { });

    // Clamp end to begin.
    end = std::max(begin, end);

    ASSERT(end >= begin);
    size_t length = end - begin;

    JSArrayBufferView* result = speciesConstruct(globalObject, thisObject, [&]() {
        bool isResizableOrGrowableShared = false;
        Structure* structure = globalObject->typedArrayStructure(ViewClass::TypedArrayStorageType, isResizableOrGrowableShared);

        // If the source TypedArray is resizable, length can be changed.
        // In that case, it is possible that we will have some holes which is not initialized to the zero values.
        // We use initialized TypedArray if source TypedArray is resizable.
        // Note that regardless of the source TypedArray's resizability, resulted TypedArray should be unresizable.
        if (UNLIKELY(thisObject->isResizableOrGrowableShared()))
            return ViewClass::create(globalObject, structure, length);

        return ViewClass::createUninitialized(globalObject, structure, length);
    }, [&](MarkedArgumentBuffer& args) {
        args.append(jsNumber(length));
        ASSERT(!args.hasOverflowed());
    }, length);
    RETURN_IF_EXCEPTION(scope, { });

    // We return early here since we don't allocate a backing store if length is 0 and memmove does not like nullptrs
    if (!length)
        return JSValue::encode(result);

    {
        IdempotentArrayBufferByteLengthGetter<std::memory_order_seq_cst> getter;
        auto updatedLength = integerIndexedObjectLength(thisObject, getter);
        if (UNLIKELY(!updatedLength))
            return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);
        end = std::min(updatedLength.value(), end);
    }

    // It is possible that |begin| becomes larger than |end| at this point. In this case, we do nothing.
    if (begin >= end)
        return JSValue::encode(result);

    ASSERT(end > begin);
    // This length is always smaller than the previous length.
    length = end - begin;
    ASSERT(result->length() >= length);

    switch (result->type()) {
    case Int8ArrayType:
        scope.release();
        jsCast<JSInt8Array*>(result)->setFromTypedArray(globalObject, 0, thisObject, begin, length, CopyType::LeftToRight);
        return JSValue::encode(result);
    case Int16ArrayType:
        scope.release();
        jsCast<JSInt16Array*>(result)->setFromTypedArray(globalObject, 0, thisObject, begin, length, CopyType::LeftToRight);
        return JSValue::encode(result);
    case Int32ArrayType:
        scope.release();
        jsCast<JSInt32Array*>(result)->setFromTypedArray(globalObject, 0, thisObject, begin, length, CopyType::LeftToRight);
        return JSValue::encode(result);
    case Uint8ArrayType:
        scope.release();
        jsCast<JSUint8Array*>(result)->setFromTypedArray(globalObject, 0, thisObject, begin, length, CopyType::LeftToRight);
        return JSValue::encode(result);
    case Uint8ClampedArrayType:
        scope.release();
        jsCast<JSUint8ClampedArray*>(result)->setFromTypedArray(globalObject, 0, thisObject, begin, length, CopyType::LeftToRight);
        return JSValue::encode(result);
    case Uint16ArrayType:
        scope.release();
        jsCast<JSUint16Array*>(result)->setFromTypedArray(globalObject, 0, thisObject, begin, length, CopyType::LeftToRight);
        return JSValue::encode(result);
    case Uint32ArrayType:
        scope.release();
        jsCast<JSUint32Array*>(result)->setFromTypedArray(globalObject, 0, thisObject, begin, length, CopyType::LeftToRight);
        return JSValue::encode(result);
    case Float32ArrayType:
        scope.release();
        jsCast<JSFloat32Array*>(result)->setFromTypedArray(globalObject, 0, thisObject, begin, length, CopyType::LeftToRight);
        return JSValue::encode(result);
    case Float64ArrayType:
        scope.release();
        jsCast<JSFloat64Array*>(result)->setFromTypedArray(globalObject, 0, thisObject, begin, length, CopyType::LeftToRight);
        return JSValue::encode(result);
    case BigInt64ArrayType:
        scope.release();
        jsCast<JSBigInt64Array*>(result)->setFromTypedArray(globalObject, 0, thisObject, begin, length, CopyType::LeftToRight);
        return JSValue::encode(result);
    case BigUint64ArrayType:
        scope.release();
        jsCast<JSBigUint64Array*>(result)->setFromTypedArray(globalObject, 0, thisObject, begin, length, CopyType::LeftToRight);
        return JSValue::encode(result);
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewProtoFuncSubarray(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame)
{
    // https://tc39.es/ecma262/#sec-%typedarray%.prototype.subarray

    auto scope = DECLARE_THROW_SCOPE(vm);

    ViewClass* thisObject = jsCast<ViewClass*>(callFrame->thisValue());

    size_t thisLength = thisObject->length();

    JSValue start = callFrame->argument(0);
    if (UNLIKELY(!start.isInt32())) {
        start = jsNumber(start.toIntegerOrInfinity(globalObject));
        RETURN_IF_EXCEPTION(scope, { });
    }

    JSValue finish = callFrame->argument(1);
    if (!finish.isUndefined()) {
        if (UNLIKELY(!finish.isInt32())) {
            finish = jsNumber(finish.toIntegerOrInfinity(globalObject));
            RETURN_IF_EXCEPTION(scope, { });
        }
    }

    ASSERT(start.isNumber());
    ASSERT(finish.isUndefined() || finish.isNumber());
    size_t begin = argumentClampedIndexFromStartOrEnd(globalObject, start, thisLength);
    RETURN_IF_EXCEPTION(scope, { });

    std::optional<size_t> count;
    if (!(thisObject->isAutoLength() && finish.isUndefined())) {
        size_t end = argumentClampedIndexFromStartOrEnd(globalObject, finish, thisLength, thisLength);
        RETURN_IF_EXCEPTION(scope, { });

        // Clamp end to begin.
        end = std::max(begin, end);

        ASSERT(end >= begin);
        count = end - begin;
    }

    RefPtr<ArrayBuffer> arrayBuffer = thisObject->possiblySharedBuffer();
    if (UNLIKELY(!arrayBuffer)) {
        throwOutOfMemoryError(globalObject, scope);
        return { };
    }

    size_t newByteOffset = thisObject->byteOffsetRaw() + begin * ViewClass::elementSize;

    scope.release();
    return JSValue::encode(speciesConstruct(globalObject, thisObject, [&]() {
        Structure* structure = globalObject->typedArrayStructure(ViewClass::TypedArrayStorageType, arrayBuffer->isResizableOrGrowableShared());
        return ViewClass::create(globalObject, structure, WTFMove(arrayBuffer), newByteOffset, count);
    }, [&](MarkedArgumentBuffer& args) {
        args.append(vm.m_typedArrayController->toJS(globalObject, thisObject->globalObject(), arrayBuffer.get()));
        args.append(jsNumber(newByteOffset));
        if (count)
            args.append(jsNumber(count.value()));
        ASSERT(!args.hasOverflowed());
    }, std::nullopt));
}

template<typename ViewClass>
static inline void validateIntegerIndex(JSGlobalObject* globalObject, ViewClass* thisObject, double index)
{
    // https://tc39.es/proposal-resizablearraybuffer/#sec-isvalidintegerindex
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());

    if (UNLIKELY(!isInteger(index))) {
        throwVMRangeError(globalObject, scope, "index should be integer"_s);
        return;
    }
    if (UNLIKELY(index == 0 && std::signbit(index))) {
        throwVMRangeError(globalObject, scope, "index should not be negative zero"_s);
        return;
    }

    IdempotentArrayBufferByteLengthGetter<std::memory_order_relaxed> getter;
    auto length = integerIndexedObjectLength(thisObject, getter);
    if (UNLIKELY(!length || index < 0 || index >= length.value())) {
        throwVMRangeError(globalObject, scope, "index is out of range"_s);
        return;
    }
}

template<typename ViewClass>
ALWAYS_INLINE EncodedJSValue genericTypedArrayViewProtoFuncWith(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame)
{
    // https://tc39.es/proposal-change-array-by-copy/#sec-%typedarray%.prototype.with

    auto scope = DECLARE_THROW_SCOPE(vm);

    ViewClass* thisObject = jsCast<ViewClass*>(callFrame->thisValue());
    IdempotentArrayBufferByteLengthGetter<std::memory_order_seq_cst> getter;
    auto length = integerIndexedObjectLength(thisObject, getter);
    if (UNLIKELY(!length))
        return throwVMTypeError(globalObject, scope, typedArrayBufferHasBeenDetachedErrorMessage);

    size_t thisLength = length.value();

    double relativeIndex = callFrame->argument(0).toIntegerOrInfinity(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    double actualIndex = 0;
    if (relativeIndex >= 0)
        actualIndex = relativeIndex;
    else
        actualIndex = thisLength + relativeIndex;

    typename ViewClass::ElementType nativeValue = ViewClass::toAdaptorNativeFromValue(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    validateIntegerIndex(globalObject, thisObject, actualIndex);
    RETURN_IF_EXCEPTION(scope, { });
    ASSERT(!thisObject->isDetached());
    size_t replaceIndex = static_cast<size_t>(actualIndex);

    bool isResizableOrGrowableShared = false;
    Structure* structure = globalObject->typedArrayStructure(ViewClass::TypedArrayStorageType, isResizableOrGrowableShared);
    ViewClass* result = ViewClass::createUninitialized(globalObject, structure, thisLength);
    RETURN_IF_EXCEPTION(scope, { });

    size_t updatedLength = thisObject->length();
    if (UNLIKELY(thisLength != updatedLength)) {
        // If TypedArray is shrunk, remaining part will be filled with NativeValue(undefined).
        // But BigInt64Array / BigUint64Array throws a TypeError since undefined cannot be converted to BigInt.
        if constexpr (ViewClass::Adaptor::isBigInt) {
            if (thisLength > updatedLength)
                return throwVMTypeError(globalObject, scope, "Cannot convert undefined to BigInt"_s);
        }

        for (size_t index = 0; index < thisLength; ++index) {
            typename ViewClass::ElementType fromValue = 0;
            if constexpr (ViewClass::Adaptor::isFloat)
                fromValue = PNaN;
            if (index == replaceIndex)
                fromValue = nativeValue;
            else if (thisObject->canGetIndexQuickly(index))
                fromValue = thisObject->getIndexQuicklyAsNativeValue(index);
            result->setIndexQuicklyToNativeValue(index, fromValue);
        }
    } else {
        typename ViewClass::ElementType* from = thisObject->typedVector();
        typename ViewClass::ElementType* to = result->typedVector();
        memmove(to, from, thisLength * ViewClass::elementSize);
        to[replaceIndex] = nativeValue;
    }

    return JSValue::encode(result);
}

} // namespace JSC

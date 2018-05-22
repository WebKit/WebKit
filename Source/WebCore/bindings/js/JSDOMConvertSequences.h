/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "IDLTypes.h"
#include "JSDOMConvertBase.h"
#include "JSDOMConvertNumbers.h"
#include "JSDOMGlobalObject.h"
#include <JavaScriptCore/IteratorOperations.h>
#include <JavaScriptCore/JSArray.h>
#include <JavaScriptCore/JSGlobalObjectInlines.h>
#include <JavaScriptCore/ObjectConstructor.h>

namespace WebCore {

namespace Detail {

template<typename IDLType>
struct GenericSequenceConverter {
    using ReturnType = Vector<typename IDLType::ImplementationType>;

    static ReturnType convert(JSC::ExecState& state, JSC::JSObject* object)
    {
        return convert(state, object, ReturnType());
    }

    static ReturnType convert(JSC::ExecState& state, JSC::JSObject* object, ReturnType&& result)
    {
        forEachInIterable(&state, object, [&result](JSC::VM& vm, JSC::ExecState* state, JSC::JSValue nextValue) {
            auto scope = DECLARE_THROW_SCOPE(vm);

            auto convertedValue = Converter<IDLType>::convert(*state, nextValue);
            if (UNLIKELY(scope.exception()))
                return;
            result.append(WTFMove(convertedValue));
        });
        return WTFMove(result);
    }

    static ReturnType convert(JSC::ExecState& state, JSC::JSObject* object, JSC::JSValue method)
    {
        return convert(state, object, method, ReturnType());
    }

    static ReturnType convert(JSC::ExecState& state, JSC::JSObject* object, JSC::JSValue method, ReturnType&& result)
    {
        forEachInIterable(state, object, method, [&result](JSC::VM& vm, JSC::ExecState& state, JSC::JSValue nextValue) {
            auto scope = DECLARE_THROW_SCOPE(vm);

            auto convertedValue = Converter<IDLType>::convert(state, nextValue);
            if (UNLIKELY(scope.exception()))
                return;
            result.append(WTFMove(convertedValue));
        });
        return WTFMove(result);
    }
};

// Specialization for numeric types
// FIXME: This is only implemented for the IDLFloatingPointTypes and IDLLong. To add
// support for more numeric types, add an overload of Converter<IDLType>::convert that
// takes an ExecState, ThrowScope, double as its arguments.
template<typename IDLType>
struct NumericSequenceConverter {
    using GenericConverter = GenericSequenceConverter<IDLType>;
    using ReturnType = typename GenericConverter::ReturnType;

    static ReturnType convertArray(JSC::ExecState& state, JSC::ThrowScope& scope, JSC::JSArray* array, unsigned length, JSC::IndexingType indexingType, ReturnType&& result)
    {
        if (indexingType == JSC::Int32Shape) {
            for (unsigned i = 0; i < length; i++) {
                auto indexValue = array->butterfly()->contiguousInt32().at(array, i).get();
                ASSERT(!indexValue || indexValue.isInt32());
                if (!indexValue)
                    result.uncheckedAppend(0);
                else
                    result.uncheckedAppend(indexValue.asInt32());
            }
            return WTFMove(result);
        }

        ASSERT(indexingType == JSC::DoubleShape);
        for (unsigned i = 0; i < length; i++) {
            double doubleValue = array->butterfly()->contiguousDouble().at(array, i);
            if (std::isnan(doubleValue))
                result.uncheckedAppend(0);
            else {
                auto convertedValue = Converter<IDLType>::convert(state, scope, doubleValue);
                RETURN_IF_EXCEPTION(scope, { });

                result.uncheckedAppend(convertedValue);
            }
        }
        return WTFMove(result);
    }

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        auto& vm = state.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (!value.isObject()) {
            throwSequenceTypeError(state, scope);
            return { };
        }

        JSC::JSObject* object = JSC::asObject(value);
        if (!JSC::isJSArray(object))
            return GenericConverter::convert(state, object);

        JSC::JSArray* array = JSC::asArray(object);
        if (!array->isIteratorProtocolFastAndNonObservable())
            return GenericConverter::convert(state, object);

        unsigned length = array->length();
        ReturnType result;
        // If we're not an int32/double array, it's possible that converting a
        // JSValue to a number could cause the iterator protocol to change, hence,
        // we may need more capacity, or less. In such cases, we use the length
        // as a proxy for the capacity we will most likely need (it's unlikely that 
        // a program is written with a valueOf that will augment the iterator protocol).
        // If we are an int32/double array, then length is precisely the capacity we need.
        if (!result.tryReserveCapacity(length)) {
            // FIXME: Is the right exception to throw?
            throwTypeError(&state, scope);
            return { };
        }
        
        JSC::IndexingType indexingType = array->indexingType() & JSC::IndexingShapeMask;
        if (indexingType != JSC::Int32Shape && indexingType != JSC::DoubleShape)
            return GenericConverter::convert(state, object, WTFMove(result));

        return convertArray(state, scope, array, length, indexingType, WTFMove(result));
    }

    static ReturnType convert(JSC::ExecState& state, JSC::JSObject* object, JSC::JSValue method)
    {
        auto& vm = state.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (!JSC::isJSArray(object))
            return GenericConverter::convert(state, object, method);

        JSC::JSArray* array = JSC::asArray(object);
        if (!array->isIteratorProtocolFastAndNonObservable())
            return GenericConverter::convert(state, object, method);

        unsigned length = array->length();
        ReturnType result;
        // If we're not an int32/double array, it's possible that converting a
        // JSValue to a number could cause the iterator protocol to change, hence,
        // we may need more capacity, or less. In such cases, we use the length
        // as a proxy for the capacity we will most likely need (it's unlikely that 
        // a program is written with a valueOf that will augment the iterator protocol).
        // If we are an int32/double array, then length is precisely the capacity we need.
        if (!result.tryReserveCapacity(length)) {
            // FIXME: Is the right exception to throw?
            throwTypeError(&state, scope);
            return { };
        }
        
        JSC::IndexingType indexingType = array->indexingType() & JSC::IndexingShapeMask;
        if (indexingType != JSC::Int32Shape && indexingType != JSC::DoubleShape)
            return GenericConverter::convert(state, object, method, WTFMove(result));

        return convertArray(state, scope, array, length, indexingType, WTFMove(result));
    }
};

template<typename IDLType>
struct SequenceConverter {
    using GenericConverter = GenericSequenceConverter<IDLType>;
    using ReturnType = typename GenericConverter::ReturnType;

    static ReturnType convertArray(JSC::ExecState& state, JSC::ThrowScope& scope, JSC::JSArray* array)
    {
        unsigned length = array->length();

        ReturnType result;
        if (!result.tryReserveCapacity(length)) {
            // FIXME: Is the right exception to throw?
            throwTypeError(&state, scope);
            return { };
        }

        JSC::IndexingType indexingType = array->indexingType() & JSC::IndexingShapeMask;

        if (indexingType == JSC::ContiguousShape) {
            for (unsigned i = 0; i < length; i++) {
                auto indexValue = array->butterfly()->contiguous().at(array, i).get();
                if (!indexValue)
                    indexValue = JSC::jsUndefined();

                auto convertedValue = Converter<IDLType>::convert(state, indexValue);
                RETURN_IF_EXCEPTION(scope, { });

                result.uncheckedAppend(convertedValue);
            }
            return result;
        }

        for (unsigned i = 0; i < length; i++) {
            auto indexValue = array->getDirectIndex(&state, i);
            RETURN_IF_EXCEPTION(scope, { });

            if (!indexValue)
                indexValue = JSC::jsUndefined();

            auto convertedValue = Converter<IDLType>::convert(state, indexValue);
            RETURN_IF_EXCEPTION(scope, { });
            
            result.uncheckedAppend(convertedValue);
        }
        return result;
    }

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        auto& vm = state.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (!value.isObject()) {
            throwSequenceTypeError(state, scope);
            return { };
        }

        JSC::JSObject* object = JSC::asObject(value);
        if (Converter<IDLType>::conversionHasSideEffects)
            return GenericConverter::convert(state, object);

        if (!JSC::isJSArray(object))
            return GenericConverter::convert(state, object);

        JSC::JSArray* array = JSC::asArray(object);
        if (!array->isIteratorProtocolFastAndNonObservable())
            return GenericConverter::convert(state, object);
        
        return convertArray(state, scope, array);
    }

    static ReturnType convert(JSC::ExecState& state, JSC::JSObject* object, JSC::JSValue method)
    {
        auto& vm = state.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (Converter<IDLType>::conversionHasSideEffects)
            return GenericConverter::convert(state, object, method);

        if (!JSC::isJSArray(object))
            return GenericConverter::convert(state, object, method);

        JSC::JSArray* array = JSC::asArray(object);
        if (!array->isIteratorProtocolFastAndNonObservable())
            return GenericConverter::convert(state, object, method);

        return convertArray(state, scope, array);
    }
};

template<>
struct SequenceConverter<IDLLong> {
    using ReturnType = typename GenericSequenceConverter<IDLLong>::ReturnType;

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return NumericSequenceConverter<IDLLong>::convert(state, value);
    }

    static ReturnType convert(JSC::ExecState& state, JSC::JSObject* object, JSC::JSValue method)
    {
        return NumericSequenceConverter<IDLLong>::convert(state, object, method);
    }
};

template<>
struct SequenceConverter<IDLFloat> {
    using ReturnType = typename GenericSequenceConverter<IDLFloat>::ReturnType;

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return NumericSequenceConverter<IDLFloat>::convert(state, value);
    }

    static ReturnType convert(JSC::ExecState& state, JSC::JSObject* object, JSC::JSValue method)
    {
        return NumericSequenceConverter<IDLFloat>::convert(state, object, method);
    }
};

template<>
struct SequenceConverter<IDLUnrestrictedFloat> {
    using ReturnType = typename GenericSequenceConverter<IDLUnrestrictedFloat>::ReturnType;

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return NumericSequenceConverter<IDLUnrestrictedFloat>::convert(state, value);
    }

    static ReturnType convert(JSC::ExecState& state, JSC::JSObject* object, JSC::JSValue method)
    {
        return NumericSequenceConverter<IDLUnrestrictedFloat>::convert(state, object, method);
    }
};

template<>
struct SequenceConverter<IDLDouble> {
    using ReturnType = typename GenericSequenceConverter<IDLDouble>::ReturnType;

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return NumericSequenceConverter<IDLDouble>::convert(state, value);
    }

    static ReturnType convert(JSC::ExecState& state, JSC::JSObject* object, JSC::JSValue method)
    {
        return NumericSequenceConverter<IDLDouble>::convert(state, object, method);
    }
};

template<>
struct SequenceConverter<IDLUnrestrictedDouble> {
    using ReturnType = typename GenericSequenceConverter<IDLUnrestrictedDouble>::ReturnType;

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return NumericSequenceConverter<IDLUnrestrictedDouble>::convert(state, value);
    }

    static ReturnType convert(JSC::ExecState& state, JSC::JSObject* object, JSC::JSValue method)
    {
        return NumericSequenceConverter<IDLUnrestrictedDouble>::convert(state, object, method);
    }
};

}

template<typename T> struct Converter<IDLSequence<T>> : DefaultConverter<IDLSequence<T>> {
    using ReturnType = typename Detail::SequenceConverter<T>::ReturnType;

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return Detail::SequenceConverter<T>::convert(state, value);
    }

    static ReturnType convert(JSC::ExecState& state, JSC::JSObject* object, JSC::JSValue method)
    {
        return Detail::SequenceConverter<T>::convert(state, object, method);
    }
};

template<typename T> struct JSConverter<IDLSequence<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = true;

    template<typename U, size_t inlineCapacity>
    static JSC::JSValue convert(JSC::ExecState& exec, JSDOMGlobalObject& globalObject, const Vector<U, inlineCapacity>& vector)
    {
        JSC::VM& vm = exec.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        JSC::MarkedArgumentBuffer list;
        for (auto& element : vector)
            list.append(toJS<T>(exec, globalObject, element));
        if (UNLIKELY(list.hasOverflowed())) {
            throwOutOfMemoryError(&exec, scope);
            return { };
        }
        return JSC::constructArray(&exec, nullptr, &globalObject, list);
    }
};

template<typename T> struct Converter<IDLFrozenArray<T>> : DefaultConverter<IDLFrozenArray<T>> {
    using ReturnType = typename Detail::SequenceConverter<T>::ReturnType;

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return Detail::SequenceConverter<T>::convert(state, value);
    }

    static ReturnType convert(JSC::ExecState& state, JSC::JSObject* object, JSC::JSValue method)
    {
        return Detail::SequenceConverter<T>::convert(state, object, method);
    }
};

template<typename T> struct JSConverter<IDLFrozenArray<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = true;

    template<typename U, size_t inlineCapacity>
    static JSC::JSValue convert(JSC::ExecState& exec, JSDOMGlobalObject& globalObject, const Vector<U, inlineCapacity>& vector)
    {
        JSC::VM& vm = exec.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        JSC::MarkedArgumentBuffer list;
        for (auto& element : vector)
            list.append(toJS<T>(exec, globalObject, element));
        if (UNLIKELY(list.hasOverflowed())) {
            throwOutOfMemoryError(&exec, scope);
            return { };
        }
        auto* array = JSC::constructArray(&exec, nullptr, &globalObject, list);
        return JSC::objectConstructorFreeze(&exec, array);
    }
};

} // namespace WebCore


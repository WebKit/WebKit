/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include <runtime/IteratorOperations.h>
#include <runtime/JSArray.h>
#include <runtime/JSGlobalObjectInlines.h>

namespace WebCore {

namespace Detail {

template<typename IDLType>
struct GenericSequenceConverter {
    using ReturnType = Vector<typename IDLType::ImplementationType>;

    static ReturnType convert(JSC::ExecState& state, JSC::JSObject* jsObject)
    {
        ReturnType result;
        forEachInIterable(&state, jsObject, [&result](JSC::VM& vm, JSC::ExecState* state, JSC::JSValue jsValue) {
            auto scope = DECLARE_THROW_SCOPE(vm);

            auto convertedValue = Converter<IDLType>::convert(*state, jsValue);
            if (UNLIKELY(scope.exception()))
                return;
            result.append(WTFMove(convertedValue));
        });
        return result;
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
        if (!array->globalObject()->isArrayIteratorProtocolFastAndNonObservable())
            return GenericConverter::convert(state, object);

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
                auto indexValue = array->butterfly()->contiguous()[i].get();
                if (!indexValue)
                    result.uncheckedAppend(0);
                else {
                    auto convertedValue = Converter<IDLType>::convert(state, indexValue);
                    RETURN_IF_EXCEPTION(scope, { });

                    result.uncheckedAppend(convertedValue);
                }
            }
            return result;
        }
        
        if (indexingType == JSC::Int32Shape) {
            for (unsigned i = 0; i < length; i++) {
                auto indexValue = array->butterfly()->contiguousInt32()[i].get();
                ASSERT(!indexValue || indexValue.isInt32());
                if (!indexValue)
                    result.uncheckedAppend(0);
                else
                    result.uncheckedAppend(indexValue.asInt32());
            }
            return result;
        }

        if (indexingType == JSC::DoubleShape) {
            for (unsigned i = 0; i < length; i++) {
                auto doubleValue = array->butterfly()->contiguousDouble()[i];
                if (std::isnan(doubleValue))
                    result.uncheckedAppend(0);
                else {
                    auto convertedValue = Converter<IDLType>::convert(state, scope, doubleValue);
                    RETURN_IF_EXCEPTION(scope, { });

                    result.uncheckedAppend(convertedValue);
                }
            }
            return result;
        }

        for (unsigned i = 0; i < length; i++) {
            auto indexValue = array->getDirectIndex(&state, i);
            RETURN_IF_EXCEPTION(scope, { });
            
            if (!indexValue)
                result.uncheckedAppend(0);
            else {
                auto convertedValue = Converter<IDLType>::convert(state, indexValue);
                RETURN_IF_EXCEPTION(scope, { });
                
                result.uncheckedAppend(convertedValue);
            }
        }
        return result;
    }
};

template<typename IDLType>
struct SequenceConverter {
    using GenericConverter = GenericSequenceConverter<IDLType>;
    using ReturnType = typename GenericConverter::ReturnType;

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
        if (!array->globalObject()->isArrayIteratorProtocolFastAndNonObservable())
            return GenericConverter::convert(state, object);

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
                auto indexValue = array->butterfly()->contiguous()[i].get();
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
};

template<>
struct SequenceConverter<IDLLong> {
    using ReturnType = typename GenericSequenceConverter<IDLLong>::ReturnType;

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return NumericSequenceConverter<IDLLong>::convert(state, value);
    }
};

template<>
struct SequenceConverter<IDLFloat> {
    using ReturnType = typename GenericSequenceConverter<IDLFloat>::ReturnType;

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return NumericSequenceConverter<IDLFloat>::convert(state, value);
    }
};

template<>
struct SequenceConverter<IDLUnrestrictedFloat> {
    using ReturnType = typename GenericSequenceConverter<IDLUnrestrictedFloat>::ReturnType;

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return NumericSequenceConverter<IDLUnrestrictedFloat>::convert(state, value);
    }
};

template<>
struct SequenceConverter<IDLDouble> {
    using ReturnType = typename GenericSequenceConverter<IDLDouble>::ReturnType;

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return NumericSequenceConverter<IDLDouble>::convert(state, value);
    }
};

template<>
struct SequenceConverter<IDLUnrestrictedDouble> {
    using ReturnType = typename GenericSequenceConverter<IDLUnrestrictedDouble>::ReturnType;

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return NumericSequenceConverter<IDLUnrestrictedDouble>::convert(state, value);
    }
};

}

template<typename T> struct Converter<IDLSequence<T>> : DefaultConverter<IDLSequence<T>> {
    using ReturnType = typename Detail::SequenceConverter<T>::ReturnType;

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return Detail::SequenceConverter<T>::convert(state, value);
    }
};

template<typename T> struct JSConverter<IDLSequence<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = true;

    template<typename U, size_t inlineCapacity>
    static JSC::JSValue convert(JSC::ExecState& exec, JSDOMGlobalObject& globalObject, const Vector<U, inlineCapacity>& vector)
    {
        JSC::MarkedArgumentBuffer list;
        for (auto& element : vector)
            list.append(toJS<T>(exec, globalObject, element));
        return JSC::constructArray(&exec, nullptr, &globalObject, list);
    }
};

template<typename T> struct Converter<IDLFrozenArray<T>> : DefaultConverter<IDLFrozenArray<T>> {
    using ReturnType = typename Detail::SequenceConverter<T>::ReturnType;

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return Detail::SequenceConverter<T>::convert(state, value);
    }
};

template<typename T> struct JSConverter<IDLFrozenArray<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = true;

    template<typename U, size_t inlineCapacity>
    static JSC::JSValue convert(JSC::ExecState& exec, JSDOMGlobalObject& globalObject, const Vector<U, inlineCapacity>& vector)
    {
        JSC::MarkedArgumentBuffer list;
        for (auto& element : vector)
            list.append(toJS<T>(exec, globalObject, element));
        auto* array = JSC::constructArray(&exec, nullptr, &globalObject, list);
        return JSC::objectConstructorFreeze(&exec, array);
    }
};

} // namespace WebCore

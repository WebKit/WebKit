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
#include "JSDOMBinding.h"
#include "JSDOMConvertBase.h"
#include <runtime/IteratorOperations.h>

namespace WebCore {

template<typename ReturnType, typename T, bool enabled>
struct ConditionalConverter;

template<typename ReturnType, typename T>
struct ConditionalConverter<ReturnType, T, true> {
    static std::optional<ReturnType> convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return ReturnType(Converter<T>::convert(state, value));
    }
};

template<typename ReturnType, typename T>
struct ConditionalConverter<ReturnType, T, false> {
    static std::optional<ReturnType> convert(JSC::ExecState&, JSC::JSValue)
    {
        return std::nullopt;
    }
};

namespace Detail {

template<typename List, bool condition>
struct ConditionalFront;

template<typename List>
struct ConditionalFront<List, true> {
    using type = brigand::front<List>;
};

template<typename List>
struct ConditionalFront<List, false> {
    using type = void;
};

}

template<typename List, bool condition>
using ConditionalFront = typename Detail::ConditionalFront<List, condition>::type;

template<typename... T> struct Converter<IDLUnion<T...>> : DefaultConverter<IDLUnion<T...>> {
    using Type = IDLUnion<T...>;
    using TypeList = typename Type::TypeList;
    using ReturnType = typename Type::ImplementationType;

    using NumericTypeList = brigand::filter<TypeList, IsIDLNumber<brigand::_1>>;
    static constexpr size_t numberOfNumericTypes = brigand::size<NumericTypeList>::value;
    static_assert(numberOfNumericTypes == 0 || numberOfNumericTypes == 1, "There can be 0 or 1 numeric types in an IDLUnion.");
    using NumericType = ConditionalFront<NumericTypeList, numberOfNumericTypes != 0>;

    // FIXME: This should also check for IDLEnumeration<T>.
    using StringTypeList = brigand::filter<TypeList, std::is_base_of<IDLString, brigand::_1>>;
    static constexpr size_t numberOfStringTypes = brigand::size<StringTypeList>::value;
    static_assert(numberOfStringTypes == 0 || numberOfStringTypes == 1, "There can be 0 or 1 string types in an IDLUnion.");
    using StringType = ConditionalFront<StringTypeList, numberOfStringTypes != 0>;

    using SequenceTypeList = brigand::filter<TypeList, IsIDLSequence<brigand::_1>>;
    static constexpr size_t numberOfSequenceTypes = brigand::size<SequenceTypeList>::value;
    static_assert(numberOfSequenceTypes == 0 || numberOfSequenceTypes == 1, "There can be 0 or 1 sequence types in an IDLUnion.");
    using SequenceType = ConditionalFront<SequenceTypeList, numberOfSequenceTypes != 0>;

    using FrozenArrayTypeList = brigand::filter<TypeList, IsIDLFrozenArray<brigand::_1>>;
    static constexpr size_t numberOfFrozenArrayTypes = brigand::size<FrozenArrayTypeList>::value;
    static_assert(numberOfFrozenArrayTypes == 0 || numberOfFrozenArrayTypes == 1, "There can be 0 or 1 FrozenArray types in an IDLUnion.");
    using FrozenArrayType = ConditionalFront<FrozenArrayTypeList, numberOfFrozenArrayTypes != 0>;

    using DictionaryTypeList = brigand::filter<TypeList, IsIDLDictionary<brigand::_1>>;
    static constexpr size_t numberOfDictionaryTypes = brigand::size<DictionaryTypeList>::value;
    static_assert(numberOfDictionaryTypes == 0 || numberOfDictionaryTypes == 1, "There can be 0 or 1 dictionary types in an IDLUnion.");
    static constexpr bool hasDictionaryType = numberOfDictionaryTypes != 0;
    using DictionaryType = ConditionalFront<DictionaryTypeList, hasDictionaryType>;

    using RecordTypeList = brigand::filter<TypeList, IsIDLRecord<brigand::_1>>;
    static constexpr size_t numberOfRecordTypes = brigand::size<RecordTypeList>::value;
    static_assert(numberOfRecordTypes == 0 || numberOfRecordTypes == 1, "There can be 0 or 1 record types in an IDLUnion.");
    static constexpr bool hasRecordType = numberOfRecordTypes != 0;
    using RecordType = ConditionalFront<RecordTypeList, hasRecordType>;

    static constexpr bool hasObjectType = (numberOfSequenceTypes + numberOfFrozenArrayTypes + numberOfDictionaryTypes + numberOfRecordTypes) > 0;

    using InterfaceTypeList = brigand::filter<TypeList, IsIDLInterface<brigand::_1>>;

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        JSC::VM& vm = state.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        // 1. If the union type includes a nullable type and V is null or undefined, then return the IDL value null.
        constexpr bool hasNullType = brigand::any<TypeList, std::is_same<IDLNull, brigand::_1>>::value;
        if (hasNullType) {
            if (value.isUndefinedOrNull())
                return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, IDLNull, hasNullType>::convert(state, value).value());
        }
        
        // 2. Let types be the flattened member types of the union type.
        // NOTE: Union is expected to be pre-flattented.
        
        // 3. If V is null or undefined then:
        if (hasDictionaryType || hasRecordType) {
            if (value.isUndefinedOrNull()) {
                //     1. If types includes a dictionary type, then return the result of converting V to that dictionary type.
                if (hasDictionaryType)
                    return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, DictionaryType, hasDictionaryType>::convert(state, value).value());
                
                //     2. If types includes a record type, then return the result of converting V to that record type.
                if (hasRecordType)
                    return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, RecordType, hasRecordType>::convert(state, value).value());
            }
        }

        // 4. If V is a platform object, then:
        //     1. If types includes an interface type that V implements, then return the IDL value that is a reference to the object V.
        //     2. If types includes object, then return the IDL value that is a reference to the object V.
        //         (FIXME: Add support for object and step 4.2)
        if (brigand::any<TypeList, IsIDLInterface<brigand::_1>>::value) {
            std::optional<ReturnType> returnValue;
            brigand::for_each<InterfaceTypeList>([&](auto&& type) {
                if (returnValue)
                    return;
                
                using Type = typename WTF::RemoveCVAndReference<decltype(type)>::type::type;
                using ImplementationType = typename Type::ImplementationType;
                using RawType = typename Type::RawType;
                using WrapperType = typename JSDOMWrapperConverterTraits<RawType>::WrapperClass;

                auto castedValue = WrapperType::toWrapped(vm, value);
                if (!castedValue)
                    return;
                
                returnValue = ReturnType(ImplementationType(castedValue));
            });

            if (returnValue)
                return WTFMove(returnValue.value());
        }
        
        // FIXME: Add support for steps 5 - 10.

        // 11. If V is any kind of object, then:
        if (hasObjectType) {
            if (value.isCell()) {
                JSC::JSCell* cell = value.asCell();
                if (cell->isObject()) {
                    // FIXME: We should be able to optimize the following code by making use
                    // of the fact that we have proved that the value is an object. 
                
                    //     1. If types includes a sequence type, then:
                    //         1. Let method be the result of GetMethod(V, @@iterator).
                    //         2. ReturnIfAbrupt(method).
                    //         3. If method is not undefined, return the result of creating a
                    //            sequence of that type from V and method.        
                    constexpr bool hasSequenceType = numberOfSequenceTypes != 0;
                    if (hasSequenceType) {
                        bool hasIterator = JSC::hasIteratorMethod(state, value);
                        RETURN_IF_EXCEPTION(scope, ReturnType());
                        if (hasIterator)
                            return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, SequenceType, hasSequenceType>::convert(state, value).value());
                    }

                    //     2. If types includes a frozen array type, then:
                    //         1. Let method be the result of GetMethod(V, @@iterator).
                    //         2. ReturnIfAbrupt(method).
                    //         3. If method is not undefined, return the result of creating a
                    //            frozen array of that type from V and method.
                    constexpr bool hasFrozenArrayType = numberOfFrozenArrayTypes != 0;
                    if (hasFrozenArrayType) {
                        bool hasIterator = JSC::hasIteratorMethod(state, value);
                        RETURN_IF_EXCEPTION(scope, ReturnType());
                        if (hasIterator)
                            return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, FrozenArrayType, hasFrozenArrayType>::convert(state, value).value());
                    }

                    //     3. If types includes a dictionary type, then return the result of
                    //        converting V to that dictionary type.
                    if (hasDictionaryType)
                        return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, DictionaryType, hasDictionaryType>::convert(state, value).value());

                    //     4. If types includes a record type, then return the result of converting V to that record type.
                    if (hasRecordType)
                        return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, RecordType, hasRecordType>::convert(state, value).value());

                    //     5. If types includes a callback interface type, then return the result of converting V to that interface type.
                    //         (FIXME: Add support for callback interface type and step 12.5)
                    //     6. If types includes object, then return the IDL value that is a reference to the object V.
                    //         (FIXME: Add support for object and step 12.6)
                }
            }
        }

        // 12. If V is a Boolean value, then:
        //     1. If types includes a boolean, then return the result of converting V to boolean.
        constexpr bool hasBooleanType = brigand::any<TypeList, std::is_same<IDLBoolean, brigand::_1>>::value;
        if (hasBooleanType) {
            if (value.isBoolean())
                return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, IDLBoolean, hasBooleanType>::convert(state, value).value());
        }
        
        // 13. If V is a Number value, then:
        //     1. If types includes a numeric type, then return the result of converting V to that numeric type.
        constexpr bool hasNumericType = brigand::size<NumericTypeList>::value != 0;
        if (hasNumericType) {
            if (value.isNumber())
                return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, NumericType, hasNumericType>::convert(state, value).value());
        }
        
        // 14. If types includes a string type, then return the result of converting V to that type.
        constexpr bool hasStringType = brigand::size<StringTypeList>::value != 0;
        if (hasStringType)
            return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, StringType, hasStringType>::convert(state, value).value());

        // 15. If types includes a numeric type, then return the result of converting V to that numeric type.
        if (hasNumericType)
            return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, NumericType, hasNumericType>::convert(state, value).value());

        // 16. If types includes a boolean, then return the result of converting V to boolean.
        if (hasBooleanType)
            return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, IDLBoolean, hasBooleanType>::convert(state, value).value());

        // 17. Throw a TypeError.
        throwTypeError(&state, scope);
        return ReturnType();
    }
};

template<typename... T> struct JSConverter<IDLUnion<T...>> {
    using Type = IDLUnion<T...>;
    using TypeList = typename Type::TypeList;
    using ImplementationType = typename Type::ImplementationType;

    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = true;

    using Sequence = brigand::make_sequence<brigand::ptrdiff_t<0>, WTF::variant_size<ImplementationType>::value>;

    static JSC::JSValue convert(JSC::ExecState& state, JSDOMGlobalObject& globalObject, const ImplementationType& variant)
    {
        auto index = variant.index();

        std::optional<JSC::JSValue> returnValue;
        brigand::for_each<Sequence>([&](auto&& type) {
            using I = typename WTF::RemoveCVAndReference<decltype(type)>::type::type;
            if (I::value == index) {
                ASSERT(!returnValue);
                returnValue = toJS<brigand::at<TypeList, I>>(state, globalObject, WTF::get<I::value>(variant));
            }
        });

        ASSERT(returnValue);
        return returnValue.value();
    }
};

} // namespace WebCore

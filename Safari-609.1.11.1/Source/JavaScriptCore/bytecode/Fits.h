/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#include "GetPutInfo.h"
#include "Interpreter.h"
#include "Label.h"
#include "OpcodeSize.h"
#include "ProfileTypeBytecodeFlag.h"
#include "PutByIdFlags.h"
#include "ResultType.h"
#include "SymbolTableOrScopeDepth.h"
#include "VirtualRegister.h"
#include <type_traits>

namespace JSC {

enum FitsAssertion {
    Assert,
    NoAssert
};

// Fits template
template<typename, OpcodeSize, typename = std::true_type>
struct Fits;

// Implicit conversion for types of the same size
template<typename T, OpcodeSize size>
struct Fits<T, size, std::enable_if_t<sizeof(T) == size, std::true_type>> {
    using TargetType = typename TypeBySize<size>::unsignedType;

    static bool check(T) { return true; }

    static TargetType convert(T t) { return bitwise_cast<TargetType>(t); }

    template<class T1 = T, OpcodeSize size1 = size, typename = std::enable_if_t<!std::is_same<T1, TargetType>::value, std::true_type>>
    static T1 convert(TargetType t) { return bitwise_cast<T1>(t); }
};

template<typename T, OpcodeSize size>
struct Fits<T, size, std::enable_if_t<std::is_integral<T>::value && sizeof(T) != size && !std::is_same<bool, T>::value, std::true_type>> {
    using TargetType = std::conditional_t<std::is_unsigned<T>::value, typename TypeBySize<size>::unsignedType, typename TypeBySize<size>::signedType>;

    static bool check(T t)
    {
        return t >= std::numeric_limits<TargetType>::min() && t <= std::numeric_limits<TargetType>::max();
    }

    static TargetType convert(T t)
    {
        ASSERT(check(t));
        return static_cast<TargetType>(t);
    }

    template<class T1 = T, OpcodeSize size1 = size, typename TargetType1 = TargetType, typename = std::enable_if_t<!std::is_same<T1, TargetType1>::value, std::true_type>>
    static T1 convert(TargetType1 t) { return static_cast<T1>(t); }
};

template<OpcodeSize size>
struct Fits<bool, size, std::enable_if_t<size != sizeof(bool), std::true_type>> : public Fits<uint8_t, size> {
    using Base = Fits<uint8_t, size>;

    static bool check(bool e) { return Base::check(static_cast<uint8_t>(e)); }

    static typename Base::TargetType convert(bool e)
    {
        return Base::convert(static_cast<uint8_t>(e));
    }

    static bool convert(typename Base::TargetType e)
    {
        return Base::convert(e);
    }
};

template<OpcodeSize size>
struct FirstConstant;

template<>
struct FirstConstant<OpcodeSize::Narrow> {
    static constexpr int index = FirstConstantRegisterIndex8;
};

template<>
struct FirstConstant<OpcodeSize::Wide16> {
    static constexpr int index = FirstConstantRegisterIndex16;
};

template<OpcodeSize size>
struct Fits<VirtualRegister, size, std::enable_if_t<size != OpcodeSize::Wide32, std::true_type>> {
    // Narrow:
    // -128..-1  local variables
    //    0..15  arguments
    //   16..127 constants
    //
    // Wide16:
    // -2**15..-1  local variables
    //      0..64  arguments
    //     64..2**15-1 constants

    using TargetType = typename TypeBySize<size>::signedType;

    static constexpr int s_firstConstantIndex = FirstConstant<size>::index;
    static bool check(VirtualRegister r)
    {
        if (r.isConstant())
            return (s_firstConstantIndex + r.toConstantIndex()) <= std::numeric_limits<TargetType>::max();
        return r.offset() >= std::numeric_limits<TargetType>::min() && r.offset() < s_firstConstantIndex;
    }

    static TargetType convert(VirtualRegister r)
    {
        ASSERT(check(r));
        if (r.isConstant())
            return static_cast<TargetType>(s_firstConstantIndex + r.toConstantIndex());
        return static_cast<TargetType>(r.offset());
    }

    static VirtualRegister convert(TargetType u)
    {
        int i = static_cast<int>(static_cast<TargetType>(u));
        if (i >= s_firstConstantIndex)
            return VirtualRegister { (i - s_firstConstantIndex) + FirstConstantRegisterIndex };
        return VirtualRegister { i };
    }
};

template<OpcodeSize size>
struct Fits<SymbolTableOrScopeDepth, size, std::enable_if_t<size != OpcodeSize::Wide32, std::true_type>> : public Fits<unsigned, size> {
    static_assert(sizeof(SymbolTableOrScopeDepth) == sizeof(unsigned));
    using TargetType = typename TypeBySize<size>::unsignedType;
    using Base = Fits<unsigned, size>;

    static bool check(SymbolTableOrScopeDepth u) { return Base::check(u.raw()); }

    static TargetType convert(SymbolTableOrScopeDepth u)
    {
        return Base::convert(u.raw());
    }

    static SymbolTableOrScopeDepth convert(TargetType u)
    {
        return SymbolTableOrScopeDepth::raw(Base::convert(u));
    }
};

template<OpcodeSize size>
struct Fits<GetPutInfo, size, std::enable_if_t<size != OpcodeSize::Wide32, std::true_type>> {
    using TargetType = typename TypeBySize<size>::unsignedType;

    // 13 Resolve Types
    // 3 Initialization Modes
    // 2 Resolve Modes
    //
    // Try to encode encode as
    //
    //           initialization mode
    //                    v
    // free bit-> 0|0000|00|0
    //                 ^    ^
    //    resolve  type   resolve mode
    static constexpr int s_resolveTypeMax = 1 << 4;
    static constexpr int s_initializationModeMax = 1 << 2;
    static constexpr int s_resolveModeMax = 1 << 1;

    static constexpr int s_resolveTypeBits = (s_resolveTypeMax - 1) << 3;
    static constexpr int s_initializationModeBits = (s_initializationModeMax - 1) << 1;
    static constexpr int s_resolveModeBits = (s_resolveModeMax - 1);

    static_assert(!(s_resolveTypeBits & s_initializationModeBits & s_resolveModeBits), "There should be no intersection between ResolveMode, ResolveType and InitializationMode");

    static bool check(GetPutInfo gpi)
    {
        auto resolveType = static_cast<int>(gpi.resolveType());
        auto initializationMode = static_cast<int>(gpi.initializationMode());
        auto resolveMode = static_cast<int>(gpi.resolveMode());
        return resolveType < s_resolveTypeMax && initializationMode < s_initializationModeMax && resolveMode < s_resolveModeMax;
    }

    static TargetType convert(GetPutInfo gpi)
    {
        ASSERT(check(gpi));
        auto resolveType = static_cast<uint8_t>(gpi.resolveType());
        auto initializationMode = static_cast<uint8_t>(gpi.initializationMode());
        auto resolveMode = static_cast<uint8_t>(gpi.resolveMode());
        return (resolveType << 3) | (initializationMode << 1) | resolveMode;
    }

    static GetPutInfo convert(TargetType gpi)
    {
        auto resolveType = static_cast<ResolveType>((gpi & s_resolveTypeBits) >> 3);
        auto initializationMode = static_cast<InitializationMode>((gpi & s_initializationModeBits) >> 1);
        auto resolveMode = static_cast<ResolveMode>(gpi & s_resolveModeBits);
        return GetPutInfo(resolveMode, resolveType, initializationMode);
    }
};

template<typename E, OpcodeSize size>
struct Fits<E, size, std::enable_if_t<sizeof(E) != size && std::is_enum<E>::value, std::true_type>> : public Fits<std::underlying_type_t<E>, size> {
    using Base = Fits<std::underlying_type_t<E>, size>;

    static bool check(E e) { return Base::check(static_cast<std::underlying_type_t<E>>(e)); }

    static typename Base::TargetType convert(E e)
    {
        return Base::convert(static_cast<std::underlying_type_t<E>>(e));
    }

    static E convert(typename Base::TargetType e)
    {
        return static_cast<E>(Base::convert(e));
    }
};

template<OpcodeSize size>
struct Fits<ResultType, size, std::enable_if_t<sizeof(ResultType) != size, std::true_type>> : public Fits<uint8_t, size> {
    static_assert(sizeof(ResultType) == sizeof(uint8_t));
    using Base = Fits<uint8_t, size>;

    static bool check(ResultType type) { return Base::check(type.bits()); }

    static typename Base::TargetType convert(ResultType type) { return Base::convert(type.bits()); }

    static ResultType convert(typename Base::TargetType type) { return ResultType(Base::convert(type)); }
};

template<OpcodeSize size>
struct Fits<OperandTypes, size, std::enable_if_t<sizeof(OperandTypes) != size, std::true_type>> {
    static_assert(sizeof(OperandTypes) == sizeof(uint16_t));
    using TargetType = typename TypeBySize<size>::unsignedType;

    // a pair of (ResultType::Type, ResultType::Type) - try to fit each type into 4 bits
    // additionally, encode unknown types as 0 rather than the | of all types
    static constexpr unsigned typeWidth = 4;
    static constexpr unsigned maxType = (1 << typeWidth) - 1;

    static bool check(OperandTypes types)
    {
        if (size == OpcodeSize::Narrow) {
            auto first = types.first().bits();
            auto second = types.second().bits();
            if (first == ResultType::unknownType().bits())
                first = 0;
            if (second == ResultType::unknownType().bits())
                second = 0;
            return first <= maxType && second <= maxType;
        }
        return true;
    }

    static TargetType convert(OperandTypes types)
    {
        if (size == OpcodeSize::Narrow) {
            ASSERT(check(types));
            auto first = types.first().bits();
            auto second = types.second().bits();
            if (first == ResultType::unknownType().bits())
                first = 0;
            if (second == ResultType::unknownType().bits())
                second = 0;
            return (first << typeWidth) | second;
        }
        return static_cast<TargetType>(types.bits());
    }

    static OperandTypes convert(TargetType types)
    {
        if (size == OpcodeSize::Narrow) {
            auto first = types >> typeWidth;
            auto second = types & maxType;
            if (!first)
                first = ResultType::unknownType().bits();
            if (!second)
                second = ResultType::unknownType().bits();
            return OperandTypes(ResultType(first), ResultType(second));
        }
        return OperandTypes::fromBits(static_cast<uint16_t>(types));
    }
};

template<OpcodeSize size, typename GeneratorTraits>
struct Fits<GenericBoundLabel<GeneratorTraits>, size> : public Fits<int, size> {
    // This is a bit hacky: we need to delay computing jump targets, since we
    // might have to emit `nop`s to align the instructions stream. Additionally,
    // we have to compute the target before we start writing to the instruction
    // stream, since the offset is computed from the start of the bytecode. We
    // achieve this by computing the target when we `check` and saving it, then
    // later we use the saved target when we call convert.

    using Base = Fits<int, size>;
    static bool check(GenericBoundLabel<GeneratorTraits>& label)
    {
        return Base::check(label.saveTarget());
    }

    static typename Base::TargetType convert(GenericBoundLabel<GeneratorTraits>& label)
    {
        return Base::convert(label.commitTarget());
    }

    static GenericBoundLabel<GeneratorTraits> convert(typename Base::TargetType target)
    {
        return GenericBoundLabel<GeneratorTraits>(Base::convert(target));
    }
};

} // namespace JSC

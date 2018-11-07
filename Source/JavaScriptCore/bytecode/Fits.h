/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "ResultType.h"
#include "SpecialPointer.h"
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
    static bool check(T) { return true; }

    static typename TypeBySize<size>::type convert(T t) { return *reinterpret_cast<typename TypeBySize<size>::type*>(&t); }

    template<class T1 = T, OpcodeSize size1 = size, typename = std::enable_if_t<!std::is_same<T1, typename TypeBySize<size1>::type>::value, std::true_type>>
    static T1 convert(typename TypeBySize<size1>::type t) { return *reinterpret_cast<T1*>(&t); }
};

template<typename T, OpcodeSize size>
struct Fits<T, size, std::enable_if_t<sizeof(T) < size, std::true_type>> {
    static bool check(T) { return true; }

    static typename TypeBySize<size>::type convert(T t) { return static_cast<typename TypeBySize<size>::type>(t); }

    template<class T1 = T, OpcodeSize size1 = size, typename = std::enable_if_t<!std::is_same<T1, typename TypeBySize<size1>::type>::value, std::true_type>>
    static T1 convert(typename TypeBySize<size1>::type t) { return static_cast<T1>(t); }
};

template<>
struct Fits<uint32_t, OpcodeSize::Narrow> {
    static bool check(unsigned u) { return u <= UINT8_MAX; }

    static uint8_t convert(unsigned u)
    {
        ASSERT(check(u));
        return static_cast<uint8_t>(u);
    }
    static unsigned convert(uint8_t u)
    {
        return u;
    }
};

template<>
struct Fits<int, OpcodeSize::Narrow> {
    static bool check(int i)
    {
        return i >= INT8_MIN && i <= INT8_MAX;
    }

    static uint8_t convert(int i)
    {
        ASSERT(check(i));
        return static_cast<uint8_t>(i);
    }

    static int convert(uint8_t i)
    {
        return static_cast<int8_t>(i);
    }
};

template<>
struct Fits<VirtualRegister, OpcodeSize::Narrow> {
    // -128..-1  local variables
    //    0..15  arguments
    //   16..127 constants
    static constexpr int s_firstConstantIndex = 16;
    static bool check(VirtualRegister r)
    {
        if (r.isConstant())
            return (s_firstConstantIndex + r.toConstantIndex()) <= INT8_MAX;
        return r.offset() >= INT8_MIN && r.offset() < s_firstConstantIndex;
    }

    static uint8_t convert(VirtualRegister r)
    {
        ASSERT(check(r));
        if (r.isConstant())
            return static_cast<int8_t>(s_firstConstantIndex + r.toConstantIndex());
        return static_cast<int8_t>(r.offset());
    }

    static VirtualRegister convert(uint8_t u)
    {
        int i = static_cast<int>(static_cast<int8_t>(u));
        if (i >= s_firstConstantIndex)
            return VirtualRegister { (i - s_firstConstantIndex) + FirstConstantRegisterIndex };
        return VirtualRegister { i };
    }
};

template<>
struct Fits<Special::Pointer, OpcodeSize::Narrow> : Fits<int, OpcodeSize::Narrow> {
    using Base = Fits<int, OpcodeSize::Narrow>;
    static bool check(Special::Pointer sp) { return Base::check(static_cast<int>(sp)); }
    static uint8_t convert(Special::Pointer sp)
    {
        return Base::convert(static_cast<int>(sp));
    }
    static Special::Pointer convert(uint8_t sp)
    {
        return static_cast<Special::Pointer>(Base::convert(sp));
    }
};

template<>
struct Fits<GetPutInfo, OpcodeSize::Narrow> {
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

    static uint8_t convert(GetPutInfo gpi)
    {
        ASSERT(check(gpi));
        auto resolveType = static_cast<uint8_t>(gpi.resolveType());
        auto initializationMode = static_cast<uint8_t>(gpi.initializationMode());
        auto resolveMode = static_cast<uint8_t>(gpi.resolveMode());
        return (resolveType << 3) | (initializationMode << 1) | resolveMode;
    }

    static GetPutInfo convert(uint8_t gpi)
    {
        auto resolveType = static_cast<ResolveType>((gpi & s_resolveTypeBits) >> 3);
        auto initializationMode = static_cast<InitializationMode>((gpi & s_initializationModeBits) >> 1);
        auto resolveMode = static_cast<ResolveMode>(gpi & s_resolveModeBits);
        return GetPutInfo(resolveMode, resolveType, initializationMode);
    }
};

template<>
struct Fits<DebugHookType, OpcodeSize::Narrow> : Fits<int, OpcodeSize::Narrow> {
    using Base = Fits<int, OpcodeSize::Narrow>;
    static bool check(DebugHookType dht) { return Base::check(static_cast<int>(dht)); }
    static uint8_t convert(DebugHookType dht)
    {
        return Base::convert(static_cast<int>(dht));
    }
    static DebugHookType convert(uint8_t dht)
    {
        return static_cast<DebugHookType>(Base::convert(dht));
    }
};

template<>
struct Fits<ProfileTypeBytecodeFlag, OpcodeSize::Narrow> : Fits<int, OpcodeSize::Narrow> {
    using Base = Fits<int, OpcodeSize::Narrow>;
    static bool check(ProfileTypeBytecodeFlag ptbf) { return Base::check(static_cast<int>(ptbf)); }
    static uint8_t convert(ProfileTypeBytecodeFlag ptbf)
    {
        return Base::convert(static_cast<int>(ptbf));
    }
    static ProfileTypeBytecodeFlag convert(uint8_t ptbf)
    {
        return static_cast<ProfileTypeBytecodeFlag>(Base::convert(ptbf));
    }
};

template<>
struct Fits<ResolveType, OpcodeSize::Narrow> : Fits<int, OpcodeSize::Narrow> {
    using Base = Fits<int, OpcodeSize::Narrow>;
    static bool check(ResolveType rt) { return Base::check(static_cast<int>(rt)); }
    static uint8_t convert(ResolveType rt)
    {
        return Base::convert(static_cast<int>(rt));
    }

    static ResolveType convert(uint8_t rt)
    {
        return static_cast<ResolveType>(Base::convert(rt));
    }
};

template<>
struct Fits<OperandTypes, OpcodeSize::Narrow> {
    // a pair of (ResultType::Type, ResultType::Type) - try to fit each type into 4 bits
    // additionally, encode unknown types as 0 rather than the | of all types
    static constexpr int s_maxType = 0x10;

    static bool check(OperandTypes types)
    {
        auto first = types.first().bits();
        auto second = types.second().bits();
        if (first == ResultType::unknownType().bits())
            first = 0;
        if (second == ResultType::unknownType().bits())
            second = 0;
        return first < s_maxType && second < s_maxType;
    }

    static uint8_t convert(OperandTypes types)
    {
        ASSERT(check(types));
        auto first = types.first().bits();
        auto second = types.second().bits();
        if (first == ResultType::unknownType().bits())
            first = 0;
        if (second == ResultType::unknownType().bits())
            second = 0;
        return (first << 4) | second;
    }

    static OperandTypes convert(uint8_t types)
    {
        auto first = (types & (0xf << 4)) >> 4;
        auto second = (types & 0xf);
        if (!first)
            first = ResultType::unknownType().bits();
        if (!second)
            second = ResultType::unknownType().bits();
        return OperandTypes(ResultType(first), ResultType(second));
    }
};

template<>
struct Fits<PutByIdFlags, OpcodeSize::Narrow> : Fits<int, OpcodeSize::Narrow> {
    // only ever encoded in the bytecode stream as 0 or 1, so the trivial encoding should be good enough
    using Base = Fits<int, OpcodeSize::Narrow>;
    static bool check(PutByIdFlags flags) { return Base::check(static_cast<int>(flags)); }
    static uint8_t convert(PutByIdFlags flags)
    {
        return Base::convert(static_cast<int>(flags));
    }

    static PutByIdFlags convert(uint8_t flags)
    {
        return static_cast<PutByIdFlags>(Base::convert(flags));
    }
};

template<OpcodeSize size>
struct Fits<BoundLabel, size> : Fits<int, size> {
    // This is a bit hacky: we need to delay computing jump targets, since we
    // might have to emit `nop`s to align the instructions stream. Additionally,
    // we have to compute the target before we start writing to the instruction
    // stream, since the offset is computed from the start of the bytecode. We
    // achieve this by computing the target when we `check` and saving it, then
    // later we use the saved target when we call convert.

    using Base = Fits<int, size>;
    static bool check(BoundLabel& label)
    {
        return Base::check(label.saveTarget());
    }

    static typename TypeBySize<size>::type convert(BoundLabel& label)
    {
        return Base::convert(label.commitTarget());
    }

    static BoundLabel convert(typename TypeBySize<size>::type target)
    {
        return BoundLabel(Base::convert(target));
    }
};

} // namespace JSC

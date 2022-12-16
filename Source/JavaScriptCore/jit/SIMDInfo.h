/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include <wtf/PrintStream.h>

namespace JSC {

typedef union v128_u {
    float f32x4[4];
    double f64x2[2];
    uint8_t u8x16[16];
    uint16_t u16x8[8];
    uint32_t u32x4[4];
    uint64_t u64x2[2] = { 0, 0 };
    v128_u() = default;
} v128_t;

// Comparing based on bits. Not using float/double comparison.
inline bool bitEquals(const v128_t& lhs, const v128_t rhs)
{
    return lhs.u64x2[0] == rhs.u64x2[0] && lhs.u64x2[1] == rhs.u64x2[1];
}

enum class SIMDLane : uint8_t {
    v128,
    i8x16,
    i16x8,
    i32x4,
    i64x2,
    f32x4,
    f64x2
};

enum class SIMDSignMode : uint8_t {
    None, 
    Signed,
    Unsigned
};

struct SIMDInfo {
    SIMDLane lane { SIMDLane::v128 };
    SIMDSignMode signMode { SIMDSignMode::None };
};

constexpr uint8_t elementCount(SIMDLane lane)
{
    switch (lane) {
    case SIMDLane::i8x16:
        return 16;
    case SIMDLane::i16x8:
        return 8;
    case SIMDLane::f32x4:
    case SIMDLane::i32x4:
        return 4;
    case SIMDLane::f64x2:
    case SIMDLane::i64x2:
        return 2;
    case SIMDLane::v128:
        RELEASE_ASSERT_NOT_REACHED();
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr bool scalarTypeIsFloatingPoint(SIMDLane lane)
{
    switch (lane) {
    case SIMDLane::f32x4:
    case SIMDLane::f64x2:
        return true;
    default:
        return false;
    }
}

constexpr bool scalarTypeIsIntegral(SIMDLane lane)
{
    switch (lane) {
    case SIMDLane::i8x16:
    case SIMDLane::i16x8:
    case SIMDLane::i32x4:
    case SIMDLane::i64x2:
        return true;
    default:
        return false;
    }
}

constexpr SIMDLane narrowedLane(SIMDLane lane)
{
    switch (lane) {
    case SIMDLane::v128:
    case SIMDLane::i8x16:
    case SIMDLane::f32x4:
        RELEASE_ASSERT_NOT_REACHED();
        return lane;
    case SIMDLane::i16x8:
        return SIMDLane::i8x16;
    case SIMDLane::i32x4:
        return SIMDLane::i16x8;
    case SIMDLane::i64x2:
        return SIMDLane::i32x4;
    case SIMDLane::f64x2:
        return SIMDLane::f32x4;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr SIMDLane promotedLane(SIMDLane lane)
{
    switch (lane) {
    case SIMDLane::v128:
    case SIMDLane::i64x2:
    case SIMDLane::f64x2:
        RELEASE_ASSERT_NOT_REACHED();
        return lane;
    case SIMDLane::i8x16:
        return SIMDLane::i16x8;
    case SIMDLane::i16x8:
        return SIMDLane::i32x4;
    case SIMDLane::i32x4:
        return SIMDLane::i64x2;
    case SIMDLane::f32x4:
        return SIMDLane::f64x2;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

constexpr unsigned elementByteSize(SIMDLane simdLane)
{
    switch (simdLane) {
    case SIMDLane::i8x16:
        return 1;
    case SIMDLane::i16x8:
        return 2;
    case SIMDLane::i32x4:
    case SIMDLane::f32x4:
        return 4;
    case SIMDLane::i64x2:
    case SIMDLane::f64x2:
        return 8;
    case SIMDLane::v128:
        return 16;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace JSC

namespace WTF {

inline void printInternal(PrintStream& out, JSC::SIMDLane lane)
{
    switch (lane) {
    case JSC::SIMDLane::i8x16:
        out.print("i8x16");
        break;
    case JSC::SIMDLane::i16x8:
        out.print("i16x8");
        break;
    case JSC::SIMDLane::i32x4:
        out.print("i32x4");
        break;
    case JSC::SIMDLane::f32x4:
        out.print("f32x4");
        break;
    case JSC::SIMDLane::i64x2:
        out.print("i64x2");
        break;
    case JSC::SIMDLane::f64x2:
        out.print("f64x2");
        break;
    case JSC::SIMDLane::v128:
        out.print("v128");
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

inline void printInternal(PrintStream& out, JSC::SIMDSignMode mode)
{
    switch (mode) {
    case JSC::SIMDSignMode::None:
        out.print("SignMode::None");
        break;
    case JSC::SIMDSignMode::Signed:
        out.print("SignMode::Signed");
        break;
    case JSC::SIMDSignMode::Unsigned:
        out.print("SignMode::Unsigned");
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

} // namespace WTF

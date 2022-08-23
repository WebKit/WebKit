/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
} v128_t;

enum class SimdLane : uint8_t {
    v128,
    i8x16,
    i16x8,
    i32x4,
    i64x2,
    f32x4,
    f64x2
};

enum class SimdSignMode : uint8_t {
    None, 
    Signed,
    Unsigned
};

struct SimdInfo {
    SimdLane lane { SimdLane::v128 };
    SimdSignMode signMode { SimdSignMode::None };
};

constexpr size_t elementByteSize(SimdLane simdLane)
{
    switch (simdLane) {
    case SimdLane::i8x16:
        return 1;
    case SimdLane::i16x8:
        return 2;
    case SimdLane::i32x4:
    case SimdLane::f32x4:
        return 4;
    case SimdLane::i64x2:
    case SimdLane::f64x2:
        return 8;
    case SimdLane::v128:
        return 16;
    }
}

constexpr uint8_t simdLaneCount(SimdLane lane)
{
    switch (lane) {
    case SimdLane::i8x16:
        return 16;
    case SimdLane::i16x8:
        return 8;
    case SimdLane::f32x4:
    case SimdLane::i32x4:
        return 4;
    case SimdLane::f64x2:
    case SimdLane::i64x2:
        return 2;
    case SimdLane::v128:
        RELEASE_ASSERT_NOT_REACHED();
        return 0;
    }
}

constexpr bool scalarTypeIsFloatingPoint(SimdLane lane)
{
    switch (lane) {
    case SimdLane::f32x4:
    case SimdLane::f64x2:
        return true;
    default:
        return false;
    }
}

constexpr bool scalarTypeIsIntegral(SimdLane lane)
{
    switch (lane) {
    case SimdLane::i8x16:
    case SimdLane::i16x8:
    case SimdLane::i32x4:
    case SimdLane::i64x2:
        return true;
    default:
        return false;
    }
}

inline SimdLane narrowedLane(SimdLane lane)
{
    switch (lane) {
    case SimdLane::v128:
    case SimdLane::i8x16:
    case SimdLane::f32x4:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    case SimdLane::i16x8:
        return SimdLane::i8x16;
    case SimdLane::i32x4:
        return SimdLane::i16x8;
    case SimdLane::i64x2:
        return SimdLane::i32x4;
    case SimdLane::f64x2:
        return SimdLane::f32x4;
    }
}

inline SimdLane promotedLane(SimdLane lane)
{
    switch (lane) {
    case SimdLane::v128:
    case SimdLane::i64x2:
    case SimdLane::f64x2:
        RELEASE_ASSERT_NOT_REACHED();
    case SimdLane::i8x16:
        return SimdLane::i16x8;
    case SimdLane::i16x8:
        return SimdLane::i32x4;
    case SimdLane::i32x4:
        return SimdLane::i64x2;
    case SimdLane::f32x4:
        return SimdLane::f64x2;
    }
}

} // namespace JSC

namespace WTF {

inline void printInternal(PrintStream& out, ::JSC::SimdLane lane)
{
    switch (lane) {
    case ::JSC::SimdLane::i8x16:
        out.print("i8x16");
        break;
    case ::JSC::SimdLane::i16x8:
        out.print("i16x8");
        break;
    case ::JSC::SimdLane::i32x4:
        out.print("i32x4");
        break;
    case ::JSC::SimdLane::f32x4:
        out.print("f32x4");
        break;
    case ::JSC::SimdLane::i64x2:
        out.print("i64x2");
        break;
    case ::JSC::SimdLane::f64x2:
        out.print("f64x2");
        break;
    case ::JSC::SimdLane::v128:
        out.print("v128");
        break;
    }
}

inline void printInternal(PrintStream& out, ::JSC::SimdSignMode mode)
{
    switch (mode) {
    case ::JSC::SimdSignMode::None:
        out.print("SignMode::None");
        break;
    case ::JSC::SimdSignMode::Signed:
        out.print("SignMode::Signed");
        break;
    case ::JSC::SimdSignMode::Unsigned:
        out.print("SignMode::Unsigned");
        break;
    }
}

} // namespace WTF

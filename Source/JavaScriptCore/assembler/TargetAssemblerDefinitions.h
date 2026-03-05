/*
 * Copyright (C) 2008-2024 Apple Inc. All rights reserved.
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

#include <wtf/Compiler.h>
#include <wtf/Platform.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#if ENABLE(ASSEMBLER)

#include <JavaScriptCore/JSCJSValue.h>

#define DEFINE_SIMD_FUNC(name, func, lane) \
    template <typename ...Args> \
    void name(Args&&... args) { func(lane, std::forward<Args>(args)...); }

#define DEFINE_SIMD_FUNC_WITH_SIGN_EXTEND_MODE(name, func, lane, mode) \
    template <typename ...Args> \
    void name(Args&&... args) { func(lane, mode, std::forward<Args>(args)...); }

#define DEFINE_SIMD_FUNCS(name) \
    DEFINE_SIMD_FUNC(name##Int8, name, SIMDLane::i8x16) \
    DEFINE_SIMD_FUNC(name##Int16, name, SIMDLane::i16x8) \
    DEFINE_SIMD_FUNC(name##Int32, name, SIMDLane::i32x4) \
    DEFINE_SIMD_FUNC(name##Int64, name, SIMDLane::i64x2) \
    DEFINE_SIMD_FUNC(name##Float32, name, SIMDLane::f32x4) \
    DEFINE_SIMD_FUNC(name##Float64, name, SIMDLane::f64x2)

#define DEFINE_SIGNED_SIMD_FUNCS(name) \
    DEFINE_SIMD_FUNC_WITH_SIGN_EXTEND_MODE(name##SignedInt8, name, SIMDLane::i8x16, SIMDSignMode::Signed) \
    DEFINE_SIMD_FUNC_WITH_SIGN_EXTEND_MODE(name##UnsignedInt8, name, SIMDLane::i8x16, SIMDSignMode::Unsigned) \
    DEFINE_SIMD_FUNC_WITH_SIGN_EXTEND_MODE(name##SignedInt16, name, SIMDLane::i16x8, SIMDSignMode::Signed) \
    DEFINE_SIMD_FUNC_WITH_SIGN_EXTEND_MODE(name##UnsignedInt16, name, SIMDLane::i16x8, SIMDSignMode::Unsigned) \
    DEFINE_SIMD_FUNC_WITH_SIGN_EXTEND_MODE(name##Int32, name, SIMDLane::i32x4, SIMDSignMode::None) \
    DEFINE_SIMD_FUNC_WITH_SIGN_EXTEND_MODE(name##Int64, name, SIMDLane::i64x2, SIMDSignMode::None) \
    DEFINE_SIMD_FUNC(name##Float32, name, SIMDLane::f32x4) \
    DEFINE_SIMD_FUNC(name##Float64, name, SIMDLane::f64x2)

#if CPU(ADDRESS64)
#define DEFINE_PTR_FUNC(name) \
    template <typename ...Args> \
    auto name##Ptr(Args&&... args) -> decltype(auto) { return name##64(std::forward<Args>(args)...); }
#else
#define DEFINE_PTR_FUNC(name) \
    template <typename ...Args> \
    auto name##Ptr(Args&&... args) -> decltype(auto) { return name##32(std::forward<Args>(args)...); }
#endif

#if CPU(ARM_THUMB2)
#define TARGET_ASSEMBLER ARMv7Assembler

#elif CPU(ARM64E)
#define TARGET_ASSEMBLER ARM64EAssembler

#elif CPU(ARM64)
#define TARGET_ASSEMBLER ARM64Assembler

#elif CPU(X86_64)
#define TARGET_ASSEMBLER X86Assembler

#elif CPU(RISCV64)
#define TARGET_ASSEMBLER RISCV64Assembler

#else
#error "The MacroAssembler is not supported on this platform."
#endif

#endif // ENABLE(ASSEMBLER)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

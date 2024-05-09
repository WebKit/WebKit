/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <bit>
#include <optional>
#include <wtf/StdLibExtras.h>
#include <wtf/simde/simde.h>

namespace WTF::SIMD {

constexpr simde_uint8x16_t splat(uint8_t code)
{
    return simde_uint8x16_t { code, code, code, code, code, code, code, code, code, code, code, code, code, code, code, code };
}

constexpr simde_uint16x8_t splat(uint16_t code)
{
    return simde_uint16x8_t { code, code, code, code, code, code, code, code };
}

ALWAYS_INLINE simde_uint8x16_t load(const uint8_t* ptr)
{
    return simde_vld1q_u8(ptr);
}

ALWAYS_INLINE simde_uint16x8_t load(const uint16_t* ptr)
{
    return simde_vld1q_u16(ptr);
}

ALWAYS_INLINE void store(simde_uint8x16_t value, uint8_t* ptr)
{
    return simde_vst1q_u8(ptr, value);
}

ALWAYS_INLINE void store(simde_uint16x8_t value, uint16_t* ptr)
{
    return simde_vst1q_u16(ptr, value);
}

ALWAYS_INLINE simde_uint8x16_t merge(simde_uint8x16_t accumulated, simde_uint8x16_t input)
{
    return simde_vorrq_u8(accumulated, input);
}

ALWAYS_INLINE simde_uint16x8_t merge(simde_uint16x8_t accumulated, simde_uint16x8_t input)
{
    return simde_vorrq_u16(accumulated, input);
}

ALWAYS_INLINE bool isNonZero(simde_uint8x16_t accumulated)
{
#if CPU(X86_64)
    auto raw = simde_uint8x16_to_m128i(accumulated);
    return !simde_mm_test_all_zeros(raw, raw);
#else
    return simde_vmaxvq_u8(accumulated);
#endif
}

ALWAYS_INLINE bool isNonZero(simde_uint16x8_t accumulated)
{
#if CPU(X86_64)
    auto raw = simde_uint16x8_to_m128i(accumulated);
    return !simde_mm_test_all_zeros(raw, raw);
#else
    return simde_vmaxvq_u16(accumulated);
#endif
}

ALWAYS_INLINE std::optional<uint8_t> findFirstNonZeroIndex(simde_uint8x16_t value)
{
#if CPU(X86_64)
    auto raw = simde_uint8x16_to_m128i(value);
    uint16_t mask = simde_mm_movemask_epi8(raw);
    if (!mask)
        return std::nullopt;
    return std::countr_zero(mask);
#else
    if (!isNonZero(value))
        return std::nullopt;
    constexpr simde_uint8x16_t indexMask { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    return simde_vminvq_u8(simde_vornq_u8(indexMask, value));
#endif
}

ALWAYS_INLINE std::optional<uint8_t> findFirstNonZeroIndex(simde_uint16x8_t value)
{
#if CPU(X86_64)
    auto raw = simde_uint16x8_to_m128i(value);
    uint16_t mask = simde_mm_movemask_epi8(raw);
    if (!mask)
        return std::nullopt;
    return std::countr_zero(mask) >> 1;
#else
    if (!isNonZero(value))
        return std::nullopt;
    constexpr simde_uint16x8_t indexMask { 0, 1, 2, 3, 4, 5, 6, 7 };
    return simde_vminvq_u16(simde_vornq_u16(indexMask, value));
#endif
}

template<LChar character, LChar... characters>
ALWAYS_INLINE simde_uint8x16_t equal(simde_uint8x16_t input)
{
    auto result = simde_vceqq_u8(input, simde_vmovq_n_u8(character));
    if constexpr (!sizeof...(characters))
        return result;
    else
        return merge(result, equal<characters...>(input));
}

template<UChar character, UChar... characters>
ALWAYS_INLINE simde_uint16x8_t equal(simde_uint16x8_t input)
{
    auto result = simde_vceqq_u16(input, simde_vmovq_n_u16(character));
    if constexpr (!sizeof...(characters))
        return result;
    else
        return merge(result, equal<characters...>(input));
}

ALWAYS_INLINE simde_uint8x16_t equal(simde_uint8x16_t lhs, simde_uint8x16_t rhs)
{
    return simde_vceqq_u8(lhs, rhs);
}

ALWAYS_INLINE simde_uint16x8_t equal(simde_uint16x8_t lhs, simde_uint16x8_t rhs)
{
    return simde_vceqq_u16(lhs, rhs);
}

ALWAYS_INLINE simde_uint8x16_t lessThan(simde_uint8x16_t lhs, simde_uint8x16_t rhs)
{
    return simde_vcltq_u8(lhs, rhs);
}

ALWAYS_INLINE simde_uint16x8_t lessThan(simde_uint16x8_t lhs, simde_uint16x8_t rhs)
{
    return simde_vcltq_u16(lhs, rhs);
}

}

namespace SIMD = WTF::SIMD;

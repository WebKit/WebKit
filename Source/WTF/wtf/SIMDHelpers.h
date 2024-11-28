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

#include <wtf/Compiler.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include <bit>
#include <optional>
#include <wtf/StdLibExtras.h>
#include <wtf/simde/simde.h>

namespace WTF::SIMD {

template<typename LaneType>
struct LaneToVector;

template<>
struct LaneToVector<uint8_t> {
    using Type = simde_uint8x16_t;
};

template<>
struct LaneToVector<uint16_t> {
    using Type = simde_uint16x8_t;
};

template<>
struct LaneToVector<uint32_t> {
    using Type = simde_uint32x4_t;
};

template<>
struct LaneToVector<uint64_t> {
    using Type = simde_uint64x2_t;
};

template<typename LaneType>
using VectorType = typename LaneToVector<LaneType>::Type;


template<typename LaneType>
inline constexpr size_t stride = 16 / sizeof(LaneType);

constexpr simde_uint8x16_t splat8(uint8_t code)
{
    return simde_uint8x16_t { code, code, code, code, code, code, code, code, code, code, code, code, code, code, code, code };
}

constexpr simde_uint16x8_t splat16(uint16_t code)
{
    return simde_uint16x8_t { code, code, code, code, code, code, code, code };
}

constexpr simde_uint32x4_t splat32(uint32_t code)
{
    return simde_uint32x4_t { code, code, code, code };
}

constexpr simde_uint64x2_t splat64(uint64_t code)
{
    return simde_uint64x2_t { code, code };
}

template<typename LaneType>
ALWAYS_INLINE constexpr decltype(auto) splat(LaneType lane)
{
    if constexpr (sizeof(LaneType) == sizeof(uint8_t))
        return splat8(static_cast<uint8_t>(lane));
    else if constexpr (sizeof(LaneType) == sizeof(uint16_t))
        return splat16(static_cast<uint16_t>(lane));
    else if constexpr (sizeof(LaneType) == sizeof(uint32_t))
        return splat32(static_cast<uint32_t>(lane));
    else {
        static_assert(sizeof(LaneType) == sizeof(uint64_t));
        return splat64(static_cast<uint64_t>(lane));
    }
}

ALWAYS_INLINE simde_uint8x16_t load(const uint8_t* ptr)
{
    return simde_vld1q_u8(ptr);
}

ALWAYS_INLINE simde_uint16x8_t load(const uint16_t* ptr)
{
    return simde_vld1q_u16(ptr);
}

ALWAYS_INLINE simde_uint32x4_t load(const uint32_t* ptr)
{
    return simde_vld1q_u32(ptr);
}

ALWAYS_INLINE simde_uint64x2_t load(const uint64_t* ptr)
{
    return simde_vld1q_u64(ptr);
}

ALWAYS_INLINE void store(simde_uint8x16_t value, uint8_t* ptr)
{
    return simde_vst1q_u8(ptr, value);
}

ALWAYS_INLINE void store(simde_uint16x8_t value, uint16_t* ptr)
{
    return simde_vst1q_u16(ptr, value);
}

ALWAYS_INLINE void store(simde_uint32x4_t value, uint32_t* ptr)
{
    return simde_vst1q_u32(ptr, value);
}

ALWAYS_INLINE void store(simde_uint64x2_t value, uint64_t* ptr)
{
    return simde_vst1q_u64(ptr, value);
}

ALWAYS_INLINE simde_uint8x16_t merge2(simde_uint8x16_t accumulated, simde_uint8x16_t input)
{
    return simde_vorrq_u8(accumulated, input);
}

ALWAYS_INLINE simde_uint16x8_t merge2(simde_uint16x8_t accumulated, simde_uint16x8_t input)
{
    return simde_vorrq_u16(accumulated, input);
}

ALWAYS_INLINE simde_uint32x4_t merge2(simde_uint32x4_t accumulated, simde_uint32x4_t input)
{
    return simde_vorrq_u32(accumulated, input);
}

ALWAYS_INLINE simde_uint64x2_t merge2(simde_uint64x2_t accumulated, simde_uint64x2_t input)
{
    return simde_vorrq_u64(accumulated, input);
}

ALWAYS_INLINE simde_uint8x16_t bitOr2(simde_uint8x16_t accumulated, simde_uint8x16_t input)
{
    return simde_vorrq_u8(accumulated, input);
}

ALWAYS_INLINE simde_uint16x8_t bitOr2(simde_uint16x8_t accumulated, simde_uint16x8_t input)
{
    return simde_vorrq_u16(accumulated, input);
}

ALWAYS_INLINE simde_uint32x4_t bitOr2(simde_uint32x4_t accumulated, simde_uint32x4_t input)
{
    return simde_vorrq_u32(accumulated, input);
}

ALWAYS_INLINE simde_uint64x2_t bitOr2(simde_uint64x2_t accumulated, simde_uint64x2_t input)
{
    return simde_vorrq_u64(accumulated, input);
}

ALWAYS_INLINE simde_uint8x16_t bitAnd2(simde_uint8x16_t accumulated, simde_uint8x16_t input)
{
    return simde_vandq_u8(accumulated, input);
}

ALWAYS_INLINE simde_uint16x8_t bitAnd2(simde_uint16x8_t accumulated, simde_uint16x8_t input)
{
    return simde_vandq_u16(accumulated, input);
}

ALWAYS_INLINE simde_uint32x4_t bitAnd2(simde_uint32x4_t accumulated, simde_uint32x4_t input)
{
    return simde_vandq_u32(accumulated, input);
}

ALWAYS_INLINE simde_uint64x2_t bitAnd2(simde_uint64x2_t accumulated, simde_uint64x2_t input)
{
    return simde_vandq_u64(accumulated, input);
}

template<typename VectorType, typename... Args>
ALWAYS_INLINE decltype(auto) merge(VectorType a0, VectorType a1, Args... args)
{
    if constexpr (!sizeof...(args))
        return merge2(a0, a1);
    else
        return merge2(a0, merge(a1, std::forward<Args>(args)...));
}

template<typename VectorType, typename... Args>
ALWAYS_INLINE decltype(auto) bitOr(VectorType a0, VectorType a1, Args... args)
{
    if constexpr (!sizeof...(args))
        return bitOr2(a0, a1);
    else
        return bitOr2(a0, bitOr(a1, std::forward<Args>(args)...));
}

template<typename VectorType, typename... Args>
ALWAYS_INLINE decltype(auto) bitAnd(VectorType a0, VectorType a1, Args... args)
{
    if constexpr (!sizeof...(args))
        return bitAnd2(a0, a1);
    else
        return bitAnd2(a0, bitAnd(a1, std::forward<Args>(args)...));
}

ALWAYS_INLINE simde_uint8x16_t bitNot(simde_uint8x16_t input)
{
    return simde_vmvnq_u8(input);
}

ALWAYS_INLINE simde_uint16x8_t bitNot(simde_uint16x8_t input)
{
    return simde_vmvnq_u16(input);
}

ALWAYS_INLINE simde_uint32x4_t bitNot(simde_uint32x4_t input)
{
    return simde_vmvnq_u32(input);
}

ALWAYS_INLINE simde_uint64x2_t bitNot(simde_uint64x2_t input)
{
    return simde_vreinterpretq_u64_u32(simde_vmvnq_u32(simde_vreinterpretq_u32_u64(input)));
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

ALWAYS_INLINE bool isNonZero(simde_uint32x4_t accumulated)
{
#if CPU(X86_64)
    auto raw = simde_uint32x4_to_m128i(accumulated);
    return !simde_mm_test_all_zeros(raw, raw);
#else
    return simde_vmaxvq_u32(accumulated);
#endif
}

ALWAYS_INLINE bool isNonZero(simde_uint64x2_t accumulated)
{
#if CPU(X86_64)
    auto raw = simde_uint64x2_to_m128i(accumulated);
    return !simde_mm_test_all_zeros(raw, raw);
#else
    // There is no simde_vmaxvq_u64, so using simde_vmaxvq_u8.
    // But this is fine since it only just checks if the input is all-zeros.
    return simde_vmaxvq_u32(simde_vreinterpretq_u32_u64(accumulated));
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
    // Incoming value is a comparison result, where each vector element is either all 1s or 0s.
    if (!isNonZero(value))
        return std::nullopt;
    constexpr simde_uint16x8_t indexMask { 0, 1, 2, 3, 4, 5, 6, 7 };
    // Found elements are all-1 and the other elements are 0. But it is possible that this vector
    // includes multiple found characters. We perform [0, 1, 2, 3, 4, 5, 6, 7] OR-NOT with this value,
    // to assign the index to found characters.
    // Find the smallest value. Because of [0, 1, 2, 3, 4, 5, 6, 7], the value should be index in this vector.
    // If the index less than length, it is within the requested pointer. Otherwise, nullptr.
    //
    // Example
    //     value       |0|0|0|X|0|X|0|0| (X is all-one)
    //     not-value   |X|X|X|0|X|0|X|X|
    //     index-value |0|1|2|3|4|5|6|7|
    //     ranked      |X|X|X|3|X|5|X|X|
    //     index       3, the smallest number from this vector, and it is the same to the index.
    return simde_vminvq_u16(simde_vornq_u16(indexMask, value));
#endif
}

ALWAYS_INLINE std::optional<uint8_t> findFirstNonZeroIndex(simde_uint32x4_t value)
{
#if CPU(X86_64)
    auto raw = simde_uint32x4_to_m128i(value);
    uint16_t mask = simde_mm_movemask_epi8(raw);
    if (!mask)
        return std::nullopt;
    return std::countr_zero(mask) >> 2;
#else
    if (!isNonZero(value))
        return std::nullopt;
    constexpr simde_uint32x4_t indexMask { 0, 1, 2, 3 };
    return simde_vminvq_u32(simde_vornq_u32(indexMask, value));
#endif
}

ALWAYS_INLINE std::optional<uint8_t> findFirstNonZeroIndex(simde_uint64x2_t value)
{
#if CPU(X86_64)
    auto raw = simde_uint64x2_to_m128i(value);
    uint16_t mask = simde_mm_movemask_epi8(raw);
    if (!mask)
        return std::nullopt;
    return std::countr_zero(mask) >> 3;
#else
    simde_uint32x2_t reducedMask = simde_vmovn_u64(value);
    if (!simde_vget_lane_u64(simde_vreinterpret_u64_u32(reducedMask), 0))
        return std::nullopt;
    constexpr simde_uint32x2_t indexMask { 0, 1 }; // It is intentionally uint32x2_t.
    return simde_vminv_u32(simde_vorn_u32(indexMask, reducedMask));
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

ALWAYS_INLINE simde_uint32x4_t equal(simde_uint32x4_t lhs, simde_uint32x4_t rhs)
{
    return simde_vceqq_u32(lhs, rhs);
}

ALWAYS_INLINE simde_uint64x2_t equal(simde_uint64x2_t lhs, simde_uint64x2_t rhs)
{
    return simde_vceqq_u64(lhs, rhs);
}

ALWAYS_INLINE simde_uint8x16_t lessThan(simde_uint8x16_t lhs, simde_uint8x16_t rhs)
{
    return simde_vcltq_u8(lhs, rhs);
}

ALWAYS_INLINE simde_uint16x8_t lessThan(simde_uint16x8_t lhs, simde_uint16x8_t rhs)
{
    return simde_vcltq_u16(lhs, rhs);
}

ALWAYS_INLINE simde_uint32x4_t lessThan(simde_uint32x4_t lhs, simde_uint32x4_t rhs)
{
    return simde_vcltq_u32(lhs, rhs);
}

ALWAYS_INLINE simde_uint64x2_t lessThan(simde_uint64x2_t lhs, simde_uint64x2_t rhs)
{
    return simde_vcltq_u64(lhs, rhs);
}

ALWAYS_INLINE simde_uint8x16_t lessThanOrEqual(simde_uint8x16_t lhs, simde_uint8x16_t rhs)
{
    return simde_vcleq_u8(lhs, rhs);
}

ALWAYS_INLINE simde_uint16x8_t lessThanOrEqual(simde_uint16x8_t lhs, simde_uint16x8_t rhs)
{
    return simde_vcleq_u16(lhs, rhs);
}

ALWAYS_INLINE simde_uint32x4_t lessThanOrEqual(simde_uint32x4_t lhs, simde_uint32x4_t rhs)
{
    return simde_vcleq_u32(lhs, rhs);
}

ALWAYS_INLINE simde_uint64x2_t lessThanOrEqual(simde_uint64x2_t lhs, simde_uint64x2_t rhs)
{
    return simde_vcleq_u64(lhs, rhs);
}

ALWAYS_INLINE simde_uint8x16_t greaterThan(simde_uint8x16_t lhs, simde_uint8x16_t rhs)
{
    return simde_vcgtq_u8(lhs, rhs);
}

ALWAYS_INLINE simde_uint16x8_t greaterThan(simde_uint16x8_t lhs, simde_uint16x8_t rhs)
{
    return simde_vcgtq_u16(lhs, rhs);
}

ALWAYS_INLINE simde_uint32x4_t greaterThan(simde_uint32x4_t lhs, simde_uint32x4_t rhs)
{
    return simde_vcgtq_u32(lhs, rhs);
}

ALWAYS_INLINE simde_uint64x2_t greaterThan(simde_uint64x2_t lhs, simde_uint64x2_t rhs)
{
    return simde_vcgtq_u64(lhs, rhs);
}

ALWAYS_INLINE simde_uint8x16_t greaterThanOrEqual(simde_uint8x16_t lhs, simde_uint8x16_t rhs)
{
    return simde_vcgeq_u8(lhs, rhs);
}

ALWAYS_INLINE simde_uint16x8_t greaterThanOrEqual(simde_uint16x8_t lhs, simde_uint16x8_t rhs)
{
    return simde_vcgeq_u16(lhs, rhs);
}

ALWAYS_INLINE simde_uint32x4_t greaterThanOrEqual(simde_uint32x4_t lhs, simde_uint32x4_t rhs)
{
    return simde_vcgeq_u32(lhs, rhs);
}

ALWAYS_INLINE simde_uint64x2_t greaterThanOrEqual(simde_uint64x2_t lhs, simde_uint64x2_t rhs)
{
    return simde_vcgeq_u64(lhs, rhs);
}

template<typename CharacterType, size_t threshold = SIMD::stride<CharacterType>>
ALWAYS_INLINE const CharacterType* find(std::span<const CharacterType> span, const auto& vectorMatch, const auto& scalarMatch)
{
    constexpr size_t stride = SIMD::stride<CharacterType>;
    using UnsignedType = std::make_unsigned_t<CharacterType>;
    static_assert(threshold >= stride);
    const auto* cursor = span.data();
    const auto* end = span.data() + span.size();
    if (span.size() >= threshold) {
        for (; cursor + (stride - 1) < end; cursor += stride) {
            if (auto index = vectorMatch(SIMD::load(std::bit_cast<const UnsignedType*>(cursor))))
                return cursor + index.value();
        }
        if (cursor < end) {
            if (auto index = vectorMatch(SIMD::load(std::bit_cast<const UnsignedType*>(end - stride))))
                return end - stride + index.value();
        }
        return end;
    }

    for (; cursor != end; ++cursor) {
        auto character = *cursor;
        if (scalarMatch(character))
            return cursor;
    }
    return end;
}

template<typename CharacterType, size_t threshold = SIMD::stride<CharacterType> * 2>
requires(sizeof(CharacterType) == 2)
ALWAYS_INLINE const CharacterType* findInterleaved(std::span<const CharacterType> span, const auto& vectorMatch, const auto& scalarMatch)
{
    constexpr size_t stride = SIMD::stride<CharacterType> * 2;
    static_assert(threshold >= stride);
    const auto* cursor = span.data();
    const auto* end = span.data() + span.size();
    if (span.size() >= threshold) {
        for (; cursor + (stride - 1) < end; cursor += stride) {
            if (auto index = vectorMatch(simde_vld2q_u8(std::bit_cast<const uint8_t*>(cursor))))
                return cursor + index.value();
        }
        if (cursor < end) {
            if (auto index = vectorMatch(simde_vld2q_u8(std::bit_cast<const uint8_t*>(end - stride))))
                return end - stride + index.value();
        }
        return end;
    }

    for (; cursor != end; ++cursor) {
        auto character = *cursor;
        if (scalarMatch(character))
            return cursor;
    }
    return end;
}

}

namespace SIMD = WTF::SIMD;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

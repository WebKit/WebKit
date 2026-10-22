/*
 * Copyright (C) 2016, 2026 Apple Inc. All rights reserved.
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

#include <algorithm>
#include <bit>
#include <cstddef>
#include <limits.h>
#include <optional>
#include <span>
#include <utility>
#include <wtf/Assertions.h>
#include <wtf/Compiler.h>
#include <wtf/StdLibExtras.h>

// This file contains a bunch of helper functions for decoding LEB numbers.
// See https://en.wikipedia.org/wiki/LEB128 for more information about the
// LEB format.

// The decoders first try to decode as a single byte since that's the most common case. When that
// fails we check at least 8 bytes remain, These 8 bytes are decoded in one 64-bit word without
// any branches:
//   1. Isolate the lowest byte with bit 7 clear (`~window & 0x8080808080808080ULL`, then `t &= -t`)
//      to find the terminating byte and the encoded length (LEBWindow).
//   2. Mask `window` with `terminators - 1` to zero all bytes after the terminating byte (and for
//      signed numbers, replicate the terminating byte's sign bit into all bits above the number).
//   3. Shift and mask each byte's 7-bit payload into its contiguous position in the 56-bit result.
// Inputs near the end of the buffer, 9-10 byte numbers, and encodings that fail terminal-byte
// overflow checks fall back to the byte-by-byte loop.
// FIXME: We could consider having the 9-10 byte fallthrough from the multi-byte case if we found
// those were popular enough.

namespace WTF { namespace LEBDecoder {

template<typename T>
constexpr size_t maxByteLength()
{
    constexpr size_t numBits = sizeof(T) * CHAR_BIT;
    return (numBits - 1) / 7 + 1; // numBits / 7 rounding up.
}

template<typename T>
constexpr unsigned lastByteMask()
{
    constexpr size_t numBits = sizeof(T) * CHAR_BIT;
    static_assert(numBits % 7);
    return ~((1U << (numBits % 7)) - 1);
}

template<typename T>
constexpr bool isValidSignedLastByte(uint8_t byte)
{
    if (!(byte & 0x40)) {
        // This is a non-sign-extended, positive number, so the remaining bits should be (lastByteMask<T>() >> 1).
        // For example, in the int32_t case, the last byte should be less than 0b00000111, since 7 * 4 + 3 = 31.
        return !(byte & (lastByteMask<T>() >> 1));
    }
    // This is a sign-extended, negative number, so zero should not exist in (lastByteMask<T>() >> 1) bits except for the top bit.
    // For example, in the int32_t case, the last byte should be 0b01111XXX and 1 part must be 1. Since we already checked 0x40 is 1,
    // middle [3,5] bits must be zero (e.g. 0b01000111 is invalid). We convert 0b01111XXX =(| 0x80)=> 0b11111XXX =(~)=> 0b00000YYY.
    // And check that we do not have 1 in upper 5 bits.
    return !(static_cast<uint8_t>(~(byte | 0x80)) & (lastByteMask<T>() >> 1));
}

struct LEBWindow {
    uint64_t bytes;
    size_t length;
};

template<typename T>
inline std::optional<LEBWindow> loadLEBWindow(std::span<const uint8_t> bytes, size_t offset)
{
    if constexpr (std::endian::native != std::endian::little)
        return std::nullopt;
    if (bytes.size() - offset < sizeof(uint64_t))
        return std::nullopt;
    uint64_t window;
    memcpySpan(asMutableByteSpan(window), bytes.subspan(offset, sizeof(window)));
    uint64_t terminators = ~window & 0x8080808080808080ULL;
    if (!terminators)
        return std::nullopt;
    terminators &= -terminators;
    size_t length = (std::countr_zero(terminators) + 1) / 8;
    if constexpr (maxByteLength<T>() <= sizeof(uint64_t)) {
        if (length > maxByteLength<T>())
            return std::nullopt;
        if (length == maxByteLength<T>()) {
            uint8_t lastByte = static_cast<uint8_t>(window >> (8 * (maxByteLength<T>() - 1)));
            bool isValid;
            if constexpr (std::is_signed_v<T>)
                isValid = isValidSignedLastByte<T>(lastByte);
            else
                isValid = !(lastByte & lastByteMask<T>());
            if (!isValid)
                return std::nullopt;
        }
    }
    uint64_t payloadBytes = window & (terminators - 1);
    if constexpr (std::is_signed_v<T>) {
        // Fill the bits above the number with its sign bit.
        payloadBytes |= -(window & (terminators >> 1));
    }
    return LEBWindow { payloadBytes, length };
}

constexpr uint64_t gatherLEBPayloads(uint64_t bytes)
{
    uint64_t result = 0;
    for (unsigned i = 0; i < sizeof(uint64_t); ++i)
        result |= (bytes >> i) & (0x7fULL << (7 * i));
    return result;
}

template<typename T>
[[nodiscard]] inline bool decodeUIntByteLoop(std::span<const uint8_t> bytes, size_t& offset, T& result)
{
    result = 0;
    unsigned shift = 0;
    size_t last = std::min(maxByteLength<T>(), bytes.size() - offset) - 1;
    for (unsigned i = 0; true; ++i) {
        uint8_t byte = bytes[offset++];
        result |= static_cast<T>(byte & 0x7f) << shift;
        shift += 7;
        if (!(byte & 0x80))
            return !(((maxByteLength<T>() - 1) == i && (byte & lastByteMask<T>())));
        if (i == last)
            return false;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return true;
}

// Returning the offset in a register, rather than through a reference, keeps callers' offsets out of memory.
template<typename T>
[[nodiscard]] NEVER_INLINE std::pair<std::optional<T>, size_t> decodeUIntMultiByte(std::span<const uint8_t> bytes, size_t offset)
{
    auto window = loadLEBWindow<T>(bytes, offset);
    // The byte loop reports invalid numbers, leaving offset where callers' error messages expect it.
    if (!window) {
        T result;
        if (!decodeUIntByteLoop(bytes, offset, result))
            return { std::nullopt, offset };
        return { result, offset };
    }
    return { static_cast<T>(gatherLEBPayloads(window->bytes)), offset + window->length };
}

template<typename T>
[[nodiscard]] ALWAYS_INLINE bool decodeUInt(std::span<const uint8_t> bytes, size_t& offset, T& result)
{
    static_assert(std::is_unsigned_v<T>);
    if (bytes.size() <= offset)
        return false;
    uint8_t firstByte = bytes[offset];
    if (!(firstByte & 0x80)) {
        result = firstByte;
        ++offset;
        return true;
    }
    auto [multiByteResult, newOffset] = decodeUIntMultiByte<T>(bytes, offset);
    offset = newOffset;
    if (!multiByteResult)
        return false;
    result = *multiByteResult;
    return true;
}

template<typename T>
[[nodiscard]] inline bool decodeIntByteLoop(std::span<const uint8_t> bytes, size_t& offset, T& result)
{
    using UnsignedT = std::make_unsigned_t<T>;
    result = 0;
    unsigned shift = 0;
    size_t last = std::min(maxByteLength<T>(), bytes.size() - offset) - 1;
    uint8_t byte;
    for (unsigned i = 0; true; ++i) {
        byte = bytes[offset++];
        result |= static_cast<T>(static_cast<UnsignedT>(byte & 0x7f) << shift);
        shift += 7;
        if (!(byte & 0x80)) {
            if ((maxByteLength<T>() - 1) == i && !isValidSignedLastByte<T>(byte))
                return false;
            break;
        }
        if (i == last)
            return false;
    }

    const size_t numBits = sizeof(T) * CHAR_BIT;
    if (shift < numBits && (byte & 0x40))
        result = static_cast<T>(static_cast<UnsignedT>(result) | (static_cast<UnsignedT>(-1) << shift));
    return true;
}

template<typename T>
[[nodiscard]] NEVER_INLINE std::pair<std::optional<T>, size_t> decodeIntMultiByte(std::span<const uint8_t> bytes, size_t offset)
{
    auto window = loadLEBWindow<T>(bytes, offset);
    if (!window) {
        T result;
        if (!decodeIntByteLoop(bytes, offset, result))
            return { std::nullopt, offset };
        return { result, offset };
    }
    int64_t value = static_cast<int64_t>(gatherLEBPayloads(window->bytes) << 8) >> 8;
    return { static_cast<T>(value), offset + window->length };
}

template<typename T>
[[nodiscard]] ALWAYS_INLINE bool decodeInt(std::span<const uint8_t> bytes, size_t& offset, T& result)
{
    static_assert(std::is_signed_v<T>);
    if (bytes.size() <= offset)
        return false;
    uint8_t firstByte = bytes[offset];
    if (!(firstByte & 0x80)) {
        result = static_cast<T>(static_cast<int8_t>(firstByte << 1) >> 1);
        ++offset;
        return true;
    }
    auto [multiByteResult, newOffset] = decodeIntMultiByte<T>(bytes, offset);
    offset = newOffset;
    if (!multiByteResult)
        return false;
    result = *multiByteResult;
    return true;
}

[[nodiscard]] inline bool decodeUInt32(std::span<const uint8_t> bytes, size_t& offset, uint32_t& result)
{
    return decodeUInt<uint32_t>(bytes, offset, result);
}

[[nodiscard]] inline bool decodeUInt64(std::span<const uint8_t> bytes, size_t& offset, uint64_t& result)
{
    return decodeUInt<uint64_t>(bytes, offset, result);
}

[[nodiscard]] inline bool decodeInt32(std::span<const uint8_t> bytes, size_t& offset, int32_t& result)
{
    return decodeInt<int32_t>(bytes, offset, result);
}

[[nodiscard]] inline bool decodeInt64(std::span<const uint8_t> bytes, size_t& offset, int64_t& result)
{
    return decodeInt<int64_t>(bytes, offset, result);
}

// For streams this process encoded itself, where a malformed number is a bug rather than input.
template<typename T>
inline T decodeUIntOrCrash(std::span<const uint8_t> bytes, size_t& offset)
{
    T result = 0;
    bool success = decodeUInt<T>(bytes, offset, result);
    RELEASE_ASSERT(success);
    return result;
}

template<typename T>
inline T decodeIntOrCrash(std::span<const uint8_t> bytes, size_t& offset)
{
    T result = 0;
    bool success = decodeInt<T>(bytes, offset, result);
    RELEASE_ASSERT(success);
    return result;
}

inline uint32_t decodeUInt32OrCrash(std::span<const uint8_t> bytes, size_t& offset)
{
    return decodeUIntOrCrash<uint32_t>(bytes, offset);
}

inline uint64_t decodeUInt64OrCrash(std::span<const uint8_t> bytes, size_t& offset)
{
    return decodeUIntOrCrash<uint64_t>(bytes, offset);
}

inline int32_t decodeInt32OrCrash(std::span<const uint8_t> bytes, size_t& offset)
{
    return decodeIntOrCrash<int32_t>(bytes, offset);
}

inline int64_t decodeInt64OrCrash(std::span<const uint8_t> bytes, size_t& offset)
{
    return decodeIntOrCrash<int64_t>(bytes, offset);
}

} } // WTF::LEBDecoder

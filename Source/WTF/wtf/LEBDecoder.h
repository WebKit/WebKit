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
#include <limits.h>
#include <wtf/Compiler.h>

// This file contains a bunch of helper functions for decoding LEB numbers.
// See https://en.wikipedia.org/wiki/LEB128 for more information about the
// LEB format.

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
inline bool WARN_UNUSED_RETURN decodeUInt(const uint8_t* bytes, size_t length, size_t& offset, T& result)
{
    static_assert(std::is_unsigned_v<T>);
    if (length <= offset)
        return false;
    result = 0;
    unsigned shift = 0;
    size_t last = std::min(maxByteLength<T>(), length - offset) - 1;
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

template<typename T>
inline bool WARN_UNUSED_RETURN decodeInt(const uint8_t* bytes, size_t length, size_t& offset, T& result)
{
    static_assert(std::is_signed_v<T>);
    if (length <= offset)
        return false;
    using UnsignedT = typename std::make_unsigned<T>::type;
    result = 0;
    unsigned shift = 0;
    size_t last = std::min(maxByteLength<T>(), length - offset) - 1;
    uint8_t byte;
    for (unsigned i = 0; true; ++i) {
        byte = bytes[offset++];
        result |= static_cast<T>(static_cast<UnsignedT>(byte & 0x7f) << shift);
        shift += 7;
        if (!(byte & 0x80)) {
            if ((maxByteLength<T>() - 1) == i) {
                if (!(byte & 0x40)) {
                    // This is a non-sign-extended, positive number. Then, the remaining bits should be (lastByteMask<T>() >> 1).
                    // For example, in the int32_t case, the last byte should be less than 0b00000111, since 7 * 4 + 3 = 31.
                    if (byte & (lastByteMask<T>() >> 1))
                        return false;
                } else {
                    // This is sign-extended, negative number. Then, zero should not exists in (lastByteMask<T>() >> 1) bits except for the top bit.
                    // For example, in the int32_t case, the last byte should be 0b01111XXX and 1 part must be 1. Since we already checked 0x40 is 1,
                    // middle [3,5] bits must be zero (e.g. 0b01000111 is invalid). We convert 0b01111XXX =(| 0x80)=> 0b11111XXX =(~)=> 0b00000YYY.
                    // And check that we do not have 1 in upper 5 bits.
                    if (static_cast<uint8_t>(~(byte | 0x80)) & (lastByteMask<T>() >> 1))
                        return false;
                }
            }
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

inline bool WARN_UNUSED_RETURN decodeUInt32(const uint8_t* bytes, size_t length, size_t& offset, uint32_t& result)
{
    return decodeUInt<uint32_t>(bytes, length, offset, result);
}

inline bool WARN_UNUSED_RETURN decodeUInt64(const uint8_t* bytes, size_t length, size_t& offset, uint64_t& result)
{
    return decodeUInt<uint64_t>(bytes, length, offset, result);
}

inline bool WARN_UNUSED_RETURN decodeInt32(const uint8_t* bytes, size_t length, size_t& offset, int32_t& result)
{
    return decodeInt<int32_t>(bytes, length, offset, result);
}

inline bool WARN_UNUSED_RETURN decodeInt64(const uint8_t* bytes, size_t length, size_t& offset, int64_t& result)
{
    return decodeInt<int64_t>(bytes, length, offset, result);
}

} } // WTF::LEBDecoder

/*
 * Copyright (C) 2026 Anthropic PBC.
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

#include <type_traits>
#include <wtf/Vector.h>

// Encodes LEB128 numbers into a byte vector; the counterpart of LEBDecoder.h.
// See https://en.wikipedia.org/wiki/LEB128 for more information about the
// LEB format.

namespace WTF { namespace LEBEncoder {

template<typename T, typename VectorType>
inline void encodeUInt(VectorType& bytes, T value)
{
    static_assert(std::is_unsigned_v<T>);
    while (value >= 0x80) {
        bytes.append(static_cast<uint8_t>(value | 0x80));
        value >>= 7;
    }
    bytes.append(static_cast<uint8_t>(value));
}

template<typename T, typename VectorType>
inline void encodeInt(VectorType& bytes, T value)
{
    static_assert(std::is_signed_v<T>);
    while (true) {
        uint8_t byte = value & 0x7f;
        value >>= 7;
        if ((!value && !(byte & 0x40)) || (value == -1 && (byte & 0x40))) {
            bytes.append(byte);
            return;
        }
        bytes.append(static_cast<uint8_t>(byte | 0x80));
    }
}

template<typename VectorType>
inline void encodeUInt32(VectorType& bytes, uint32_t value)
{
    encodeUInt<uint32_t>(bytes, value);
}

template<typename VectorType>
inline void encodeUInt64(VectorType& bytes, uint64_t value)
{
    encodeUInt<uint64_t>(bytes, value);
}

template<typename VectorType>
inline void encodeInt32(VectorType& bytes, int32_t value)
{
    encodeInt<int32_t>(bytes, value);
}

template<typename VectorType>
inline void encodeInt64(VectorType& bytes, int64_t value)
{
    encodeInt<int64_t>(bytes, value);
}

} } // WTF::LEBEncoder

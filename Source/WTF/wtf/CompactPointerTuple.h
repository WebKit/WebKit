/*
 * Copyright (C) 2018 Yusuke Suzuki <utatane.tea@gmail.com>.
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

#include <type_traits>
#include <wtf/StdLibExtras.h>

namespace WTF {

// The goal of this class is folding a pointer and 1 byte value into 8 bytes in both 32bit and 64bit architectures.
// 32bit architecture just has a pair of byte and pointer, which should be 8 bytes.
// In 64bit, we use the upper 5 bits and lower 3 bits (zero due to alignment) since these bits are safe to use even
// with 5-level page tables where the effective pointer width is 57bits.
template<typename PointerType, typename Type>
class CompactPointerTuple final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static_assert(sizeof(Type) == 1, "");
    static_assert(std::is_pointer<PointerType>::value, "");
    static_assert(std::is_integral<Type>::value || std::is_enum<Type>::value, "");

    CompactPointerTuple() = default;

#if CPU(ADDRESS64)
public:
    static constexpr uint64_t encodeType(uint8_t type)
    {
        // Encode 8bit type UUUDDDDD into 64bit data DDDDD..56bit..UUU.
        return (static_cast<uint64_t>(type) << 59) | (static_cast<uint64_t>(type) >> 5);
    }
    static constexpr uint8_t decodeType(uint64_t value)
    {
        // Decode 64bit data DDDDD..56bit..UUU into 8bit type UUUDDDDD.
        return static_cast<uint8_t>((value >> 59) | (value << 5));
    }

    static constexpr uint64_t typeMask = encodeType(UINT8_MAX);
    static_assert(0xF800000000000007ULL == typeMask, "");
    static constexpr uint64_t pointerMask = ~typeMask;

    CompactPointerTuple(PointerType pointer, Type type)
        : m_data(bitwise_cast<uint64_t>(pointer) | encodeType(static_cast<uint8_t>(type)))
    {
        ASSERT((bitwise_cast<uint64_t>(pointer) & 0b111) == 0x0);
    }

    PointerType pointer() const { return bitwise_cast<PointerType>(m_data & pointerMask); }
    void setPointer(PointerType pointer)
    {
        static_assert(alignof(typename std::remove_pointer<PointerType>::type) >= alignof(void*), "");
        ASSERT((bitwise_cast<uint64_t>(pointer) & 0b111) == 0x0);
        m_data = CompactPointerTuple(pointer, type()).m_data;
    }

    Type type() const { return static_cast<Type>(decodeType(m_data)); }
    void setType(Type type)
    {
        m_data = CompactPointerTuple(pointer(), type).m_data;
    }

private:
    uint64_t m_data { 0 };
#else
public:
    CompactPointerTuple(PointerType pointer, Type type)
        : m_pointer(pointer)
        , m_type(type)
    {
    }

    PointerType pointer() const { return m_pointer; }
    void setPointer(PointerType pointer) { m_pointer = pointer; }
    Type type() const { return m_type; }
    void setType(Type type) { m_type = type; }

private:
    PointerType m_pointer { nullptr };
    Type m_type { 0 };
#endif
};

} // namespace WTF

using WTF::CompactPointerTuple;

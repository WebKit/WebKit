/*
 * Copyright (C) 2018 Yusuke Suzuki <utatane.tea@gmail.com>.
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include <wtf/FastMalloc.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

// The goal of this class is folding a pointer and 2 bytes value into 8 bytes in both 32bit and 64bit architectures.
// 32bit architecture just has a pair of byte and pointer, which should be 8 bytes.
// We are assuming 48bit pointers here, which is also assumed in JSValue anyway.
template<typename PointerType, typename Type>
class CompactPointerTuple final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static_assert(sizeof(Type) <= 2, "");
    static_assert(std::is_pointer<PointerType>::value, "");
    static_assert(std::is_integral<Type>::value || std::is_enum<Type>::value, "");
    using UnsignedType = std::make_unsigned_t<std::conditional_t<std::is_same_v<Type, bool>, uint8_t, Type>>;
    static_assert(sizeof(UnsignedType) == sizeof(Type));

    CompactPointerTuple() = default;

#if CPU(ADDRESS64)
public:
    static constexpr unsigned maxNumberOfBitsInPointer = 48;
    static_assert(OS_CONSTANT(EFFECTIVE_ADDRESS_WIDTH) <= maxNumberOfBitsInPointer);

#if CPU(LITTLE_ENDIAN)
    static ptrdiff_t offsetOfType()
    {
        return maxNumberOfBitsInPointer / 8;
    }
#endif

    static constexpr uint64_t pointerMask = (1ULL << maxNumberOfBitsInPointer) - 1;

    CompactPointerTuple(PointerType pointer, Type type)
        : m_data(encode(pointer, type))
    {
        ASSERT(this->type() == type);
        ASSERT(this->pointer() == pointer);
    }

    template<typename OtherPointerType, typename = std::enable_if<std::is_pointer<PointerType>::value && std::is_convertible<OtherPointerType, PointerType>::value>>
    CompactPointerTuple(CompactPointerTuple<OtherPointerType, Type>&& other)
        : m_data { std::exchange(other.m_data, { }) }
    {
    }

    PointerType pointer() const { return bitwise_cast<PointerType>(m_data & pointerMask); }
    void setPointer(PointerType pointer)
    {
        m_data = encode(pointer, type());
        ASSERT(this->pointer() == pointer);
    }

    Type type() const { return decodeType(m_data); }
    void setType(Type type)
    {
        m_data = encode(pointer(), type);
        ASSERT(this->type() == type);
    }

    uint64_t data() const { return m_data; }

private:
    static constexpr uint64_t encodeType(Type type)
    {
        return static_cast<uint64_t>(static_cast<UnsignedType>(type)) << maxNumberOfBitsInPointer;
    }
    static constexpr Type decodeType(uint64_t value)
    {
        return static_cast<Type>(static_cast<UnsignedType>(value >> maxNumberOfBitsInPointer));
    }

    static uint64_t encode(PointerType pointer, Type type)
    {
        return bitwise_cast<uint64_t>(pointer) | encodeType(type);
    }

    uint64_t m_data { 0 };
#else
public:
    CompactPointerTuple(PointerType pointer, Type type)
        : m_pointer(pointer)
        , m_type(type)
    {
    }

    template<typename OtherPointerType, typename = std::enable_if<std::is_pointer<PointerType>::value && std::is_convertible<OtherPointerType, PointerType>::value>>
    CompactPointerTuple(CompactPointerTuple<OtherPointerType, Type>&& other)
        : m_pointer { std::exchange(other.m_pointer, { }) }
        , m_type { std::exchange(other.m_type, { }) }
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

    template<typename, typename> friend class CompactPointerTuple;
};

} // namespace WTF

using WTF::CompactPointerTuple;

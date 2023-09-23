/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include <span>
#include <wtf/EnumTraits.h>
#include <wtf/SHA1.h>
#include <wtf/Vector.h>
#include <wtf/persistence/PersistentCoders.h>

namespace WTF::Persistence {

template<typename> struct Coder;

class Encoder {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WTF_EXPORT_PRIVATE Encoder();
    WTF_EXPORT_PRIVATE ~Encoder();

    WTF_EXPORT_PRIVATE void encodeChecksum();
    WTF_EXPORT_PRIVATE void encodeFixedLengthData(std::span<const uint8_t>);

    template<typename T, std::enable_if_t<std::is_enum<T>::value>* = nullptr>
    Encoder& operator<<(const T& t)
    {
        static_assert(sizeof(T) <= sizeof(uint64_t), "Enum type must not be larger than 64 bits.");
        return *this << static_cast<uint64_t>(t);
    }

    template<typename T, std::enable_if_t<!std::is_enum<T>::value && !std::is_arithmetic<typename std::remove_const<T>>::value>* = nullptr>
    Encoder& operator<<(const T& t)
    {
        Coder<T>::encodeForPersistence(*this, t);
        return *this;
    }

    WTF_EXPORT_PRIVATE Encoder& operator<<(bool);
    WTF_EXPORT_PRIVATE Encoder& operator<<(uint8_t);
    WTF_EXPORT_PRIVATE Encoder& operator<<(uint16_t);
    WTF_EXPORT_PRIVATE Encoder& operator<<(uint32_t);
    WTF_EXPORT_PRIVATE Encoder& operator<<(uint64_t);
    WTF_EXPORT_PRIVATE Encoder& operator<<(int16_t);
    WTF_EXPORT_PRIVATE Encoder& operator<<(int32_t);
    WTF_EXPORT_PRIVATE Encoder& operator<<(int64_t);
    WTF_EXPORT_PRIVATE Encoder& operator<<(float);
    WTF_EXPORT_PRIVATE Encoder& operator<<(double);

    const uint8_t* buffer() const { return m_buffer.data(); }
    size_t bufferSize() const { return m_buffer.size(); }

    WTF_EXPORT_PRIVATE static void updateChecksumForData(SHA1&, std::span<const uint8_t>);
    template <typename Type> static void updateChecksumForNumber(SHA1&, Type);

    static constexpr bool isIPCEncoder = false;

private:

    template<typename Type> Encoder& encodeNumber(Type);

    uint8_t* grow(size_t);

    template <typename Type> struct Salt;

    Vector<uint8_t, 4096> m_buffer;
    SHA1 m_sha1;
};

template <> struct Encoder::Salt<bool> { static constexpr unsigned value = 3; };
template <> struct Encoder::Salt<uint8_t> { static constexpr  unsigned value = 5; };
template <> struct Encoder::Salt<uint16_t> { static constexpr unsigned value = 7; };
template <> struct Encoder::Salt<uint32_t> { static constexpr unsigned value = 11; };
template <> struct Encoder::Salt<uint64_t> { static constexpr unsigned value = 13; };
template <> struct Encoder::Salt<int32_t> { static constexpr unsigned value = 17; };
template <> struct Encoder::Salt<int64_t> { static constexpr unsigned value = 19; };
template <> struct Encoder::Salt<float> { static constexpr unsigned value = 23; };
template <> struct Encoder::Salt<double> { static constexpr unsigned value = 29; };
template <> struct Encoder::Salt<uint8_t*> { static constexpr unsigned value = 101; };
template <> struct Encoder::Salt<int16_t> { static constexpr unsigned value = 103; };

template <typename Type>
void Encoder::updateChecksumForNumber(SHA1& sha1, Type value)
{
    auto typeSalt = Salt<Type>::value;
    sha1.addBytes(reinterpret_cast<uint8_t*>(&typeSalt), sizeof(typeSalt));
    sha1.addBytes(reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

}

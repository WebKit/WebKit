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

#include <optional>
#include <span>
#include <wtf/EnumTraits.h>

namespace IPC {

class Decoder;
class Encoder;

template<typename T, typename = void> struct ArgumentCoder;

template<>
struct ArgumentCoder<bool> {
    // Boolean type is considered to be arithmetic, but its size isn't guaranteed to be a single byte.
    // This specialization converts booleans to and from uint8_t to ensure minimum possible size.

    template<typename Encoder>
    static void encode(Encoder& encoder, bool value)
    {
        // When converting to an integer type, boolean false/true will be converted to that type's 0/1 value.
        encoder << static_cast<uint8_t>(value);
    }

    template<typename Decoder>
    static std::optional<bool> decode(Decoder& decoder)
    {
        auto data = decoder.template decode<uint8_t>();
        if (!data)
            return std::nullopt;
        // When converting to a boolean, a zero integer value will be converted to false, and anything else to true.
        return static_cast<bool>(*data);
    }
};

template<typename T>
struct ArgumentCoder<T, typename std::enable_if_t<std::is_arithmetic_v<T>>> {
    template<typename Encoder>
    static void encode(Encoder& encoder, T value)
    {
        encoder.encodeObject(value);
    }

    template<typename Decoder>
    static std::optional<T> decode(Decoder& decoder)
    {
        return decoder.template decodeObject<T>();
    }
};

template<typename T>
struct ArgumentCoder<T, typename std::enable_if_t<std::is_enum_v<T>>> {
    template<typename Encoder>
    static void encode(Encoder& encoder, T value)
    {
        ASSERT(WTF::isValidEnum<T>(WTF::enumToUnderlyingType<T>(value)));
        encoder << WTF::enumToUnderlyingType<T>(value);
    }

    template<typename Decoder>
    static std::optional<T> decode(Decoder& decoder)
    {
        std::optional<std::underlying_type_t<T>> value;
        decoder >> value;
        if (value && WTF::isValidEnum<T>(*value))
            return static_cast<T>(*value);
        return std::nullopt;
    }
};

}

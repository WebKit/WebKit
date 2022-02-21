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

#include "Decoder.h"
#include "Encoder.h"
#include <wtf/EnumTraits.h>
#include <wtf/Span.h>

namespace IPC {
namespace Detail {
template<typename T, typename I> auto TestLegacyDecoder(int) -> decltype(I::decode(std::declval<Decoder&>(), std::declval<T&>()), std::true_type { });
template<typename T, typename I> auto TestLegacyDecoder(long) -> std::false_type;
template<typename T, typename I> auto TestModernDecoder(int) -> decltype(I::decode(std::declval<Decoder&>()), std::true_type { });
template<typename T, typename I> auto TestModernDecoder(long) -> std::false_type;
}
template<typename T, typename I = T> struct HasLegacyDecoder : decltype(Detail::TestLegacyDecoder<T, I>(0)) { };
template<typename T, typename I = T> inline constexpr bool HasLegacyDecoderV = HasLegacyDecoder<T, I>::value;
template<typename T, typename I = T> struct HasModernDecoder : decltype(Detail::TestModernDecoder<T, I>(0)) { };
template<typename T, typename I = T> inline constexpr bool HasModernDecoderV = HasModernDecoder<T, I>::value;

template<typename T, typename = void> struct ArgumentCoder {
    template<typename Encoder>
    static void encode(Encoder& encoder, const T& t)
    {
        t.encode(encoder);
    }

    template<typename Decoder>
    static std::optional<T> decode(Decoder& decoder)
    {
        if constexpr(HasModernDecoderV<T>)
            return T::decode(decoder);
        else {
            T t;
            if (T::decode(decoder, t))
                return t;
            return std::nullopt;
        }
    }

    static WARN_UNUSED_RETURN bool decode(Decoder& decoder, T& t)
    {
        if constexpr(HasLegacyDecoderV<T>)
            return T::decode(decoder, t);
        else {
            std::optional<T> optional = T::decode(decoder);
            if (!optional)
                return false;
            t = WTFMove(*optional);
            return true;
        }
    }
};

template<>
struct ArgumentCoder<bool> {
    template<typename Encoder>
    static void encode(Encoder& encoder, bool value)
    {
        uint8_t data = value ? 1 : 0;
        encoder << data;
    }

    template<typename Decoder>
    static std::optional<bool> decode(Decoder& decoder)
    {
        uint8_t data;
        if (decoder.decodeFixedLengthData(&data, sizeof(uint8_t), alignof(uint8_t)))
            return !!data; // This ensures that only the lower bit is set in a boolean for IPC messages
        return std::nullopt;
    }
};

template<typename T>
struct ArgumentCoder<T, typename std::enable_if_t<std::is_arithmetic_v<T>>> {
    template<typename Encoder>
    static void encode(Encoder& encoder, T value)
    {
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(&value), sizeof(T), alignof(T));
    }

    template<typename Decoder>
    static std::optional<T> decode(Decoder& decoder)
    {
        T result;
        if (decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(&result), sizeof(T), alignof(T)))
            return result;
        return std::nullopt;
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

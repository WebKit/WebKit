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

#include <wtf/Optional.h>

namespace WebCore {
class IntConstraint;
class DoubleConstraint;
class ResourceResponse;
struct ViewportArguments;
}

namespace IPC {

class Decoder;
class Encoder;

template<typename> struct ArgumentCoder;

template<typename> class UsesModernDecoder;

template<typename U>
class UsesModernDecoder {
private:
    template<typename T, T> struct Helper;
    template<typename T> static uint8_t check(Helper<Optional<U> (*)(Decoder&), &T::decode>*);
    template<typename T> static uint16_t check(...);
    template<typename T> static uint8_t checkArgumentCoder(Helper<Optional<U> (*)(Decoder&), &ArgumentCoder<T>::decode>*);
    template<typename T> static uint16_t checkArgumentCoder(...);
public:
    static constexpr bool argumentCoderValue = sizeof(check<U>(nullptr)) == sizeof(uint8_t);
    static constexpr bool value = argumentCoderValue || sizeof(checkArgumentCoder<U>(nullptr)) == sizeof(uint8_t);
};
    
template<typename... Types>
class UsesModernDecoder<std::tuple<Types...>> {
public:
    static constexpr bool value = true;
    static constexpr bool argumentCoderValue = true;
};

template<typename U>
class UsesLegacyDecoder {
private:
    template<typename T, T> struct Helper;
    template<typename T> static uint8_t check(Helper<bool (*)(Decoder&, U&), &T::decode>*);
    template<typename T> static uint16_t check(...);
    template<typename T> static uint8_t checkArgumentCoder(Helper<bool (*)(Decoder&, U&), &ArgumentCoder<T>::decode>*);
    template<typename T> static uint16_t checkArgumentCoder(...);
public:
    static constexpr bool argumentCoderValue = sizeof(check<U>(nullptr)) == sizeof(uint8_t);
    static constexpr bool value = argumentCoderValue || sizeof(checkArgumentCoder<U>(nullptr)) == sizeof(uint8_t);
};

template<typename BoolType>
class DefaultDecoderValues {
public:
    static constexpr bool argumentCoderValue = BoolType::value;
    static constexpr bool value = BoolType::value;
};

// ResourceResponseBase has the legacy decode template, not ResourceResponse.
template<> class UsesModernDecoder<WebCore::ResourceResponse> : public DefaultDecoderValues<std::false_type> { };
template<> class UsesLegacyDecoder<WebCore::ResourceResponse> : public DefaultDecoderValues<std::true_type> { };

// IntConstraint and DoubleConstraint have their legacy decoder templates in NumericConstraint.
template<> class UsesModernDecoder<WebCore::IntConstraint> : public DefaultDecoderValues<std::false_type> { };
template<> class UsesLegacyDecoder<WebCore::IntConstraint> : public DefaultDecoderValues<std::true_type> { };
template<> class UsesModernDecoder<WebCore::DoubleConstraint> : public DefaultDecoderValues<std::false_type> { };
template<> class UsesLegacyDecoder<WebCore::DoubleConstraint> : public DefaultDecoderValues<std::true_type> { };

template<typename T> struct ArgumentCoder {
    static void encode(Encoder& encoder, const T& t)
    {
        t.encode(encoder);
    }

    template<typename U = T, std::enable_if_t<UsesLegacyDecoder<U>::argumentCoderValue>* = nullptr>
    static bool decode(Decoder& decoder, U& u)
    {
        return U::decode(decoder, u);
    }

    template<typename U = T, std::enable_if_t<UsesModernDecoder<U>::argumentCoderValue>* = nullptr>
    static Optional<U> decode(Decoder& decoder)
    {
        return U::decode(decoder);
    }
};

}

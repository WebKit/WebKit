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

namespace IPC {

class Decoder;
class Encoder;

template<typename U>
class UsesModernDecoder {
private:
    template<typename T, T> struct Helper;
    template<typename T> static uint8_t check(Helper<std::optional<U> (*)(Decoder&), &T::decode>*);
    template<typename T> static uint16_t check(...);
public:
    static constexpr bool value = sizeof(check<U>(0)) == sizeof(uint8_t);
};

template<typename T> struct ArgumentCoder {
    static void encode(Encoder& encoder, const T& t)
    {
        t.encode(encoder);
    }

    template<typename U = T, std::enable_if_t<!UsesModernDecoder<U>::value>* = nullptr>
    static bool decode(Decoder& decoder, U& u)
    {
        return U::decode(decoder, u);
    }

    template<typename U = T, std::enable_if_t<UsesModernDecoder<U>::value>* = nullptr>
    static std::optional<U> decode(Decoder& decoder)
    {
        return U::decode(decoder);
    }
};

}

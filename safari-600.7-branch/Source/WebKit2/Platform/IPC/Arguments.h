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

#ifndef Arguments_h
#define Arguments_h

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"

namespace IPC {

template<size_t index, typename... Elements>
struct TupleCoder {
    static void encode(ArgumentEncoder& encoder, const std::tuple<Elements...>& tuple)
    {
        encoder << std::get<sizeof...(Elements) - index>(tuple);
        TupleCoder<index - 1, Elements...>::encode(encoder, tuple);
    }

    static bool decode(ArgumentDecoder& decoder, std::tuple<Elements...>& tuple)
    {
        if (!decoder.decode(std::get<sizeof...(Elements) - index>(tuple)))
            return false;
        return TupleCoder<index - 1, Elements...>::decode(decoder, tuple);
    }
};

template<typename... Elements>
struct TupleCoder<0, Elements...> {
    static void encode(ArgumentEncoder&, const std::tuple<Elements...>&)
    {
    }

    static bool decode(ArgumentDecoder&, std::tuple<Elements...>&)
    {
        return true;
    }
};

template<typename... Elements> struct ArgumentCoder<std::tuple<Elements...>> {
    static void encode(ArgumentEncoder& encoder, const std::tuple<Elements...>& tuple)
    {
        TupleCoder<sizeof...(Elements), Elements...>::encode(encoder, tuple);
    }

    static bool decode(ArgumentDecoder& decoder, std::tuple<Elements...>& tuple)
    {
        return TupleCoder<sizeof...(Elements), Elements...>::decode(decoder, tuple);
    }
};

template<typename... Types>
struct Arguments {
    typedef std::tuple<typename std::decay<Types>::type...> ValueType;

    Arguments(Types&&... values)
        : arguments(std::forward<Types>(values)...)
    {
    }

    void encode(ArgumentEncoder& encoder) const
    {
        ArgumentCoder<std::tuple<Types...>>::encode(encoder, arguments);
    }

    static bool decode(ArgumentDecoder& decoder, Arguments& result)
    {
        return ArgumentCoder<std::tuple<Types...>>::decode(decoder, result.arguments);
    }

    std::tuple<Types...> arguments;
};

} // namespace IPC

#endif // Arguments_h

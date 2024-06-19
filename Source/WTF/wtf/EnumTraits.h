/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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

#include <algorithm>
#include <type_traits>
#include <utility>

namespace WTF {

template<typename> struct EnumTraits;
template<typename> struct EnumTraitsForPersistence;

template<typename E, E...> struct EnumValues;

template<typename E, E... values>
struct EnumValues {
    static constexpr E max = std::max({ values... });
    static constexpr E min = std::min({ values... });
    static constexpr size_t count = sizeof...(values);

    template <typename Callable>
    static void forEach(Callable&& c)
    {
        for (auto value : { values... })
            c(value);
    }
};

template<typename T, typename E> struct EnumValueChecker;

template<typename T, typename E, E e, E... es>
struct EnumValueChecker<T, EnumValues<E, e, es...>> {
    static constexpr bool isValidEnum(T t)
    {
        return (static_cast<T>(e) == t) ? true : EnumValueChecker<T, EnumValues<E, es...>>::isValidEnum(t);
    }
};

template<typename T, typename E>
struct EnumValueChecker<T, EnumValues<E>> {
    static constexpr bool isValidEnum(T)
    {
        return false;
    }
};

template<typename E, typename = std::enable_if_t<!std::is_same_v<std::underlying_type_t<E>, bool>>>
bool isValidEnum(std::underlying_type_t<E> t)
{
    return EnumValueChecker<std::underlying_type_t<E>, typename EnumTraits<E>::values>::isValidEnum(t);
}

template<typename E, typename = std::enable_if_t<std::is_same_v<std::underlying_type_t<E>, bool>>>
constexpr bool isValidEnum(bool t)
{
    return !t || t == 1;
}

template<typename E, typename = std::enable_if_t<!std::is_same_v<std::underlying_type_t<E>, bool>>>
bool isValidEnumForPersistence(std::underlying_type_t<E> t)
{
    return EnumValueChecker<std::underlying_type_t<E>, typename EnumTraitsForPersistence<E>::values>::isValidEnum(t);
}

template<typename E, typename = std::enable_if_t<std::is_same_v<std::underlying_type_t<E>, bool>>>
constexpr bool isValidEnumForPersistence(bool t)
{
    return !t || t == 1;
}

template<typename T, typename E> struct ZeroBasedContiguousEnumChecker;

template<typename T, typename E, E e, E... es>
struct ZeroBasedContiguousEnumChecker<T, EnumValues<E, e, es...>> {
    template<size_t INDEX = 0>
    static constexpr bool isZeroBasedContiguousEnum()
    {
        return (std::to_underlying(e) == INDEX) ? ZeroBasedContiguousEnumChecker<T, EnumValues<E, es...>>::template isZeroBasedContiguousEnum<INDEX + 1>() : false;
    }
};

template<typename T, typename E>
struct ZeroBasedContiguousEnumChecker<T, EnumValues<E>> {
    template<size_t>
    static constexpr bool isZeroBasedContiguousEnum()
    {
        return true;
    }
};

template<typename E, typename = std::enable_if_t<!std::is_same_v<std::underlying_type_t<E>, bool>>>
constexpr bool isZeroBasedContiguousEnum()
{
    return ZeroBasedContiguousEnumChecker<std::underlying_type_t<E>, typename EnumTraits<E>::values>::isZeroBasedContiguousEnum();
}

} // namespace WTF

using WTF::isValidEnum;
using WTF::isZeroBasedContiguousEnum;

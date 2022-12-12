/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

namespace WTF {

template<typename> struct EnumTraits;
template<typename> struct EnumTraitsForPersistence;

template<typename E, E...> struct EnumValues;

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


template<typename E>
constexpr auto enumToUnderlyingType(E e)
{
    return static_cast<std::underlying_type_t<E>>(e);
}

}

using WTF::enumToUnderlyingType;
using WTF::isValidEnum;

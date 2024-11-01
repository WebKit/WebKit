/*
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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

//  __  __             _        ______                          _____
// |  \/  |           (_)      |  ____|                        / ____|_     _
// | \  / | __ _  __ _ _  ___  | |__   _ __  _   _ _ __ ___   | |   _| |_ _| |_
// | |\/| |/ _` |/ _` | |/ __| |  __| | '_ \| | | | '_ ` _ \  | |  |_   _|_   _|
// | |  | | (_| | (_| | | (__  | |____| | | | |_| | | | | | | | |____|_|   |_|
// |_|  |_|\__,_|\__, |_|\___| |______|_| |_|\__,_|_| |_| |_|  \_____|
//                __/ | https://github.com/Neargye/magic_enum
//               |___/  version 0.9.5
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2019 - 2024 Daniil Goncharov <neargye@gmail.com>.
//
// Permission is hereby  granted, free of charge, to any  person obtaining a copy
// of this software and associated  documentation files (the "Software"), to deal
// in the Software  without restriction, including without  limitation the rights
// to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
// copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
// IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
// FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
// AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
// LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <algorithm>
#include <span>
#include <type_traits>
#include <wtf/Compiler.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

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

template<typename E>
constexpr auto enumToUnderlyingType(E e)
{
    return static_cast<std::underlying_type_t<E>>(e);
}

template<typename T, typename E> struct ZeroBasedContiguousEnumChecker;

template<typename T, typename E, E e, E... es>
struct ZeroBasedContiguousEnumChecker<T, EnumValues<E, e, es...>> {
    template<size_t INDEX = 0>
    static constexpr bool isZeroBasedContiguousEnum()
    {
        return (enumToUnderlyingType(e) == INDEX) ? ZeroBasedContiguousEnumChecker<T, EnumValues<E, es...>>::template isZeroBasedContiguousEnum<INDEX + 1>() : false;
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

#if COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#pragma clang diagnostic ignored "-Wenum-constexpr-conversion"
#endif

template<typename E>
constexpr std::span<const char> enumTypeNameImpl()
{
    static_assert(std::is_enum_v<E>, "enumTypeName requires an enum type.");

#if COMPILER(CLANG)
    const size_t prefix = sizeof("std::span<const char> WTF::enumTypeNameImpl() [E = ") - 1;
    const size_t suffix = sizeof("]") - 1;
    std::span<const char> name { __PRETTY_FUNCTION__ + prefix, sizeof(__PRETTY_FUNCTION__) - prefix - suffix - 1 };
#elif COMPILER(GCC)
    const size_t prefix = sizeof("constexpr std::span<const char> WTF::enumTypeNameImpl() [with auto V = ") - 1;
    const size_t suffix = sizeof("]") - 1;
    std::span<const char> name { __PRETTY_FUNCTION__ + prefix, sizeof(__PRETTY_FUNCTION__) - prefix - suffix - 1 };
#else
#error "Unsupported compiler"
#endif
    for (size_t i = name.size(); i--;) {
        if (name[i] == ':')
            return name.subspan(i + 1);
    }
    return name;
}
template<typename E>
constexpr std::span<const char> enumTypeName()
{
    constexpr auto result = enumTypeNameImpl<std::decay_t<E>>(); // Force the span to be generated at compile time.
    return result;
}

template<auto V>
constexpr std::span<const char> enumNameImpl()
{
    static_assert(std::is_enum_v<decltype(V)>, "enumName requires an enum type.");

#if COMPILER(CLANG)
    const size_t prefix = sizeof("std::span<const char> WTF::enumNameImpl() [V = ") - 1;
    const size_t suffix = sizeof("]") - 1;
    std::span<const char> name { __PRETTY_FUNCTION__ + prefix, sizeof(__PRETTY_FUNCTION__) - prefix - suffix - 1 };
    if (name[0] == '(' || name[0] == '-' || ('0' <= name[0] && name[0] <= '9'))
        return { };
#elif COMPILER(GCC)
    const size_t prefix = sizeof("constexpr std::span<const char> WTF::enumNameImpl() [with auto V = ") - 1;
    const size_t suffix = sizeof("]") - 1;
    std::span<const char> name { __PRETTY_FUNCTION__ + prefix, sizeof(__PRETTY_FUNCTION__) - prefix - suffix - 1 };
    if (name[0] == '(')
        name = { };
#else
#error "Unsupported compiler"
#endif
    for (std::size_t i = name.size(); i--;) {
        if (name[i] == ':')
            return name.subspan(i + 1);
    }
    return name;
}

template<auto V>
constexpr std::span<const char> enumName()
{
    constexpr auto result = enumNameImpl<V>(); // Force the span to be generated at compile time.
    return result;
}

namespace detail {

template<size_t i, size_t end>
constexpr void forConstexpr(const auto& func)
{
    if constexpr (i < end) {
        func(std::integral_constant<size_t, i>());
        forConstexpr<i + 1, end>(func);
    }
}

}

template<typename E, size_t limit = 256>
constexpr std::array<std::span<const char>, limit> enumNames()
{
    std::array<std::span<const char>, limit> names;

    detail::forConstexpr<0, limit>([&] (auto i) {
        names[i] = enumName<static_cast<E>(static_cast<unsigned>(i))>();
    });
    return names;
}


template<typename T>
constexpr std::span<const char> enumName(T v)
{
    constexpr auto names = enumNames<std::decay_t<T>>();
    if (static_cast<size_t>(v) >= names.size())
        return { "enum out of range" };
    return names[static_cast<uint8_t>(v)];
}

#if COMPILER(CLANG)
#pragma clang diagnostic pop
#endif

} // namespace WTF

using WTF::enumToUnderlyingType;
using WTF::isValidEnum;
using WTF::isZeroBasedContiguousEnum;
using WTF::enumTypeName;
using WTF::enumNames;
using WTF::enumName;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

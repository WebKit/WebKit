/*
 * Copyright (C) 2018 Yusuke Suzuki <yusukesuzuki@slowstart.org>.
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
// The idea of Markable<T> is derived from markable<T> at
// https://github.com/akrzemi1/markable.
//
// Copyright (C) 2015-2018 Andrzej Krzemienski.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <type_traits>
#include <wtf/Optional.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

// Example:
//     enum class Type { Value1, Value2, Value3 };
//     Markable<Type, EnumMarkableTraits<Type, 42>> optional;
template<
    typename EnumType,
    typename std::underlying_type<EnumType>::type constant = std::numeric_limits<typename std::underlying_type<EnumType>::type>::max()>
struct EnumMarkableTraits {
    static_assert(std::is_enum<EnumType>::value, "");
    using UnderlyingType = typename std::underlying_type<EnumType>::type;

    constexpr static bool isEmptyValue(EnumType value)
    {
        return static_cast<UnderlyingType>(value) == constant;
    }

    constexpr static EnumType emptyValue()
    {
        return static_cast<EnumType>(constant);
    }
};

template<typename IntegralType, IntegralType constant = 0>
struct IntegralMarkableTraits {
    static_assert(std::is_integral<IntegralType>::value, "");
    constexpr static bool isEmptyValue(IntegralType value)
    {
        return value == constant;
    }

    constexpr static IntegralType emptyValue()
    {
        return constant;
    }
};

// The goal of Markable is offering Optional without sacrificing storage efficiency.
// Markable takes Traits, which should have isEmptyValue and emptyValue functions. By using
// one value of T as an empty value, we can remove bool flag in Optional. This strategy is
// similar to WTF::HashTable, which uses two values of T as an empty value and a deleted value.
// This class is intended to be used as a member of a class to compact the size of the class.
// Otherwise, you should use Optional.
template<typename T, typename Traits>
class Markable {
    WTF_MAKE_FAST_ALLOCATED;
public:
    constexpr Markable()
        : m_value(Traits::emptyValue())
    { }

    constexpr Markable(WTF::nullopt_t)
        : Markable()
    { }

    constexpr Markable(T&& value)
        : m_value(WTFMove(value))
    { }

    constexpr Markable(const T& value)
        : m_value(value)
    { }

    template<typename... Args>
    constexpr explicit Markable(std::in_place_t, Args&&... args)
        : m_value(std::forward<Args>(args)...)
    { }

    constexpr Markable(const Optional<T>& value)
        : m_value(bool(value) ? *value : Traits::emptyValue())
    { }

    constexpr Markable(Optional<T>&& value)
        : m_value(bool(value) ? WTFMove(*value) : Traits::emptyValue())
    { }

    constexpr explicit operator bool() const { return !Traits::isEmptyValue(m_value); }

    void reset() { m_value = Traits::emptyValue(); }

    constexpr const T& value() const& { return m_value; }
    constexpr T& value() & { return m_value; }
    constexpr T&& value() && { return WTFMove(m_value); }

    constexpr const T* operator->() const { return std::addressof(m_value); }
    constexpr T* operator->() { return std::addressof(m_value); }

    constexpr const T& operator*() const& { return m_value; }
    constexpr T& operator*() & { return m_value; }

    operator Optional<T>() &&
    {
        if (bool(*this))
            return WTFMove(m_value);
        return WTF::nullopt;
    }

    operator Optional<T>() const&
    {
        if (bool(*this))
            return m_value;
        return WTF::nullopt;
    }

    Optional<T> asOptional() const
    {
        return Optional<T>(*this);
    }

private:
    T m_value;
};

template <typename T, typename Traits> constexpr bool operator==(const Markable<T, Traits>& x, const Markable<T, Traits>& y)
{
    if (bool(x) != bool(y))
        return false;
    if (!bool(x))
        return true;
    return x.value() == y.value();
}
template <typename T, typename Traits> constexpr bool operator==(const Markable<T, Traits>& x, const T& v) { return bool(x) && x.value() == v; }
template <typename T, typename Traits> constexpr bool operator==(const T& v, const Markable<T, Traits>& x) { return bool(x) && v == x.value(); }

template <typename T, typename Traits> constexpr bool operator!=(const Markable<T, Traits>& x, const Markable<T, Traits>& y) { return !(x == y); }
template <typename T, typename Traits> constexpr bool operator!=(const Markable<T, Traits>& x, const T& v) { return !(x == v); }
template <typename T, typename Traits> constexpr bool operator!=(const T& v, const Markable<T, Traits>& x) { return !(v == x); }

} // namespace WTF

using WTF::Markable;
using WTF::IntegralMarkableTraits;
using WTF::EnumMarkableTraits;

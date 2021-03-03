/*
 * Copyright (C) 2017, 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <algorithm>
#include <array>
#include <math.h>
#include <tuple>

namespace WebCore {

template<typename T>
struct ColorComponents {
    constexpr static size_t Size = 4;

    constexpr ColorComponents(T a = 0, T b = 0, T c = 0, T d = 0)
        : components { a, b, c, d }
    {
    }

    template<typename F>
    constexpr auto map(F&& function) const -> ColorComponents<decltype(function(std::declval<T>()))>;

    constexpr ColorComponents& operator+=(const ColorComponents&);

    constexpr ColorComponents operator+(T) const;
    constexpr ColorComponents operator/(T) const;
    constexpr ColorComponents operator*(T) const;

    constexpr ColorComponents abs() const;

    constexpr T& operator[](size_t i) { return components[i]; }
    constexpr const T& operator[](size_t i) const { return components[i]; }

    template<std::size_t N>
    constexpr T get() const;

    std::array<T, Size> components;
};

template<typename F, typename T, typename... Ts>
constexpr auto mapColorComponents(F&& function, T component, Ts... components) -> ColorComponents<decltype(function(component[0], components[0]...))>
{
    static_assert(std::conjunction_v<std::bool_constant<Ts::Size == T::Size>...>, "All ColorComponents passed to mapColorComponents must have the same size");

    ColorComponents<decltype(function(component[0], components[0]...))> result;
    for (std::remove_const_t<decltype(T::Size)> i = 0; i < T::Size; ++i)
        result[i] = function(component[i], components[i]...);
    return result;
}

template<typename T>
template<typename F>
constexpr auto ColorComponents<T>::map(F&& function) const -> ColorComponents<decltype(function(std::declval<T>()))>
{
    return mapColorComponents(std::forward<F>(function), *this);
}

template<typename T>
constexpr ColorComponents<T>& ColorComponents<T>::operator+=(const ColorComponents& rhs)
{
    *this = mapColorComponents([](T c1, T c2) { return c1 + c2; }, *this, rhs);
    return *this;
}

template<typename T>
constexpr ColorComponents<T> ColorComponents<T>::operator+(T rhs) const
{
    return map([rhs](T c) { return c + rhs; });
}

template<typename T>
constexpr ColorComponents<T> ColorComponents<T>::operator/(T denominator) const
{
    return map([denominator](T c) { return c / denominator; });
}

template<typename T>
constexpr ColorComponents<T> ColorComponents<T>::operator*(T factor) const
{
    return map([factor](T c) { return c * factor; });
}

template<typename T>
constexpr ColorComponents<T> ColorComponents<T>::abs() const
{
    return map([](T c) { return std::abs(c); });
}

template<typename T>
template<std::size_t N>
constexpr T ColorComponents<T>::get() const
{
    return components[N];
}

template<typename T>
constexpr ColorComponents<T> perComponentMax(const ColorComponents<T>& a, const ColorComponents<T>& b)
{
    return mapColorComponents([](T c1, T c2) { return std::max(c1, c2); }, a, b);
}

template<typename T>
constexpr ColorComponents<T> perComponentMin(const ColorComponents<T>& a, const ColorComponents<T>& b)
{
    return mapColorComponents([](T c1, T c2) { return std::min(c1, c2); }, a, b);
}

template<typename T>
constexpr bool operator==(const ColorComponents<T>& a, const ColorComponents<T>& b)
{
    return a.components == b.components;
}

template<typename T>
constexpr bool operator!=(const ColorComponents<T>& a, const ColorComponents<T>& b)
{
    return !(a == b);
}

} // namespace WebCore

namespace std {

template<typename T>
class tuple_size<WebCore::ColorComponents<T>> : public std::integral_constant<std::size_t, WebCore::ColorComponents<T>::Size> {
};

template<std::size_t N, typename T>
class tuple_element<N, WebCore::ColorComponents<T>> {
public:
    using type = T;
};

}

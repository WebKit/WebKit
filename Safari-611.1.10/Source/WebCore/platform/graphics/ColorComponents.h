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

template<typename T>
constexpr ColorComponents<T>& ColorComponents<T>::operator+=(const ColorComponents& rhs)
{
    components[0] += rhs[0];
    components[1] += rhs[1];
    components[2] += rhs[2];
    components[3] += rhs[3];
    return *this;
}

template<typename T>
constexpr ColorComponents<T> ColorComponents<T>::operator+(T rhs) const
{
    return {
        components[0] + rhs,
        components[1] + rhs,
        components[2] + rhs,
        components[3] + rhs
    };
}

template<typename T>
constexpr ColorComponents<T> ColorComponents<T>::operator/(T denominator) const
{
    return {
        components[0] / denominator,
        components[1] / denominator,
        components[2] / denominator,
        components[3] / denominator
    };
}

template<typename T>
constexpr ColorComponents<T> ColorComponents<T>::operator*(T factor) const
{
    return {
        components[0] * factor,
        components[1] * factor,
        components[2] * factor,
        components[3] * factor
    };
}

template<typename T>
constexpr ColorComponents<T> ColorComponents<T>::abs() const
{
    return {
        std::abs(components[0]),
        std::abs(components[1]),
        std::abs(components[2]),
        std::abs(components[3])
    };
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
    return {
        std::max(a[0], b[0]),
        std::max(a[1], b[1]),
        std::max(a[2], b[2]),
        std::max(a[3], b[3])
    };
}

template<typename T>
constexpr ColorComponents<T> perComponentMin(const ColorComponents<T>& a, const ColorComponents<T>& b)
{
    return {
        std::min(a[0], b[0]),
        std::min(a[1], b[1]),
        std::min(a[2], b[2]),
        std::min(a[3], b[3])
    };
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

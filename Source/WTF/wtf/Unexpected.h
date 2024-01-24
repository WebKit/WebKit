/*
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

/*
 * <expected> is a C++23 feature, so it cannot be used in WebKit yet.
 * Portions copied (and modified) from:
 *   https://github.com/llvm/llvm-project/tree/main/libcxx/include/__expected
 * as of:
 *   https://github.com/llvm/llvm-project/commit/134c91595568ea1335b22e559f20c1a488ea270e
 * license:
 *   Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 *   See https://llvm.org/LICENSE.txt for license information.
 *   SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

#pragma once

#include <cstdlib>
#include <type_traits>
#include <utility>
#include <wtf/StdLibExtras.h>

namespace WTF::detail_from_clang_libstdcpp {

template <class _Err>
class unexpected;

namespace detail {

template <class _Tp>
struct is_std_unexpected : std::false_type { };

template <class _Err>
struct is_std_unexpected<unexpected<_Err>> : std::true_type { };

template <class _Tp>
using valid_std_unexpected = std::bool_constant<
    std::is_object_v<_Tp>
    && !std::is_array_v<_Tp>
    && !is_std_unexpected<_Tp>::value
    && !std::is_const_v<_Tp>
    && !std::is_volatile_v<_Tp>>;

} // namespace detail

template <class Err>
class unexpected {
    WTF_MAKE_FAST_ALLOCATED;
    static_assert(detail::valid_std_unexpected<Err>::value,
        "[expected.un.general] states a program that instantiates std::unexpected for a non-object type, an "
        "array type, a specialization of unexpected, or a cv-qualified type is ill-formed."); // NOLINT

public:
    constexpr unexpected(const unexpected&) = default;
    constexpr unexpected(unexpected&&)      = default;

    template <class Error = Err>
        requires(!std::is_same_v<std::remove_cvref_t<Error>, unexpected>
            && !std::is_same_v<std::remove_cvref_t<Error>, std::in_place_t>
            && std::is_constructible_v<Err, Error>)
    constexpr explicit unexpected(Error&& error)
        : m_unex(std::forward<Error>(error)) { }

    template <class... Args>
        requires std::is_constructible_v<Err, Args...>
    constexpr explicit unexpected(std::in_place_t, Args&&... args)
        : m_unex(std::forward<Args>(args)...) { }

    template <class Up, class... Args>
        requires std::is_constructible_v<Err, std::initializer_list<Up>&, Args...>
    constexpr explicit unexpected(std::in_place_t, std::initializer_list<Up> il, Args&&... args)
        : m_unex(il, std::forward<Args>(args)...) { }

    constexpr unexpected& operator=(const unexpected&) = default;
    constexpr unexpected& operator=(unexpected&&)      = default;

    constexpr const Err& error() const& { return m_unex; }
    constexpr Err& error() & { return m_unex; }
    constexpr const Err&& error() const&& { return WTFMove(m_unex); }
    constexpr Err&& error() && { return WTFMove(m_unex); }

    constexpr void swap(unexpected& other)
    {
        static_assert(std::is_swappable_v<Err>, "unexpected::swap requires is_swappable_v<E> to be true");
        using std::swap; // NOLINT
        swap(m_unex, other.m_unex);
    }

    friend constexpr void swap(unexpected& x, unexpected& y)
        requires std::is_swappable_v<Err>
    { // NOLINT
        x.swap(y);
    }

    template <class Err2>
    friend constexpr bool operator==(const unexpected& x, const unexpected<Err2>& y)
    {
        return x.m_unex == y.m_unex;
    }

private:
    Err m_unex;
};

template <class Err>
unexpected(Err) -> unexpected<Err>;

} // namespace WTF::detail_from_clang_libstdcpp

template<class E> using Unexpected = WTF::detail_from_clang_libstdcpp::unexpected<E>;

// Not in the std::expected spec, but useful to work around lack of C++17 deduction guides.
template<class E> constexpr Unexpected<std::decay_t<E>> makeUnexpected(E&& v) { return Unexpected<typename std::decay<E>::type>(std::forward<E>(v)); }

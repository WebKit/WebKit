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

// Implementation of Library Fundamentals v3's std::expected, as described here: http://wg21.link/p0323r4

#pragma once

/*
    unexpected synopsis

namespace std {
namespace experimental {
inline namespace fundamentals_v3 {
    // ?.?.3, Unexpected object type
    template <class E>
      class unexpected;

    // ?.?.4, Unexpected relational operators
    template <class E>
        constexpr bool
        operator==(const unexpected<E>&, const unexpected<E>&);
    template <class E>
        constexpr bool
        operator!=(const unexpected<E>&, const unexpected<E>&);

    template <class E>
    class unexpected {
    public:
        unexpected() = delete;
        template <class U = E>
          constexpr explicit unexpected(E&&);
        constexpr const E& value() const &;
        constexpr E& value() &;
        constexpr E&& value() &&;
        constexpr E const&& value() const&&;
    private:
        E val; // exposition only
    };

}}}

*/

#include <cstdlib>
#include <utility>
#include <wtf/StdLibExtras.h>

namespace std {
namespace experimental {
inline namespace fundamentals_v3 {

template<class E>
class unexpected {
    WTF_MAKE_FAST_ALLOCATED;
public:
    unexpected() = delete;
    template <class U = E>
    constexpr explicit unexpected(U&& u) : val(std::forward<U>(u)) { }
    constexpr const E& value() const & { return val; }
    constexpr E& value() & { return val; }
    constexpr E&& value() && { return WTFMove(val); }
    constexpr const E&& value() const && { return WTFMove(val); }

private:
    E val;
};

template<class E> constexpr bool operator==(const unexpected<E>& lhs, const unexpected<E>& rhs) { return lhs.value() == rhs.value(); }
template<class E> constexpr bool operator!=(const unexpected<E>& lhs, const unexpected<E>& rhs) { return lhs.value() != rhs.value(); }

}}} // namespace std::experimental::fundamentals_v3

template<class E> using Unexpected = std::experimental::unexpected<E>;

// Not in the std::expected spec, but useful to work around lack of C++17 deduction guides.
template<class E> constexpr Unexpected<std::decay_t<E>> makeUnexpected(E&& v) { return Unexpected<typename std::decay<E>::type>(std::forward<E>(v)); }

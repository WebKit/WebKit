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
#include <functional>
#include <initializer_list>
#include <type_traits>
#include <utility>
#include <wtf/Assertions.h>
#include <wtf/Compiler.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Unexpected.h>

namespace WTF::detail_from_clang_libstdcpp {

namespace detail {
template <template <class...> class Func, class ...Args>
struct Lazy : Func<Args...> { };
}

struct unexpect_t {
    explicit unexpect_t() = default; // NOLINT
};

inline constexpr unexpect_t unexpect { };

template <class Tp, class Err>
class expected;

namespace detail {

template <class Tp>
struct is_std_expected : std::false_type { };

template <class Tp, class Err>
struct is_std_expected<expected<Tp, Err>> : std::true_type { };

struct expected_construct_in_place_from_invoke_tag { };
struct expected_construct_unexpected_from_invoke_tag { };

// FIXME: Remove specializations for trivially copy/mov-able once all compilers support
// "conditional trivial special member functions" (__cpp_concepts >= 202002L).

enum class IsTriviallyCopyConstructible { No, Yes };
enum class IsTriviallyMoveConstructible { No, Yes };
enum class IsTriviallyDestructible { No, Yes };

template <typename Tp, typename Err, IsTriviallyCopyConstructible, IsTriviallyMoveConstructible, IsTriviallyDestructible>
struct Storage;

template <typename Tp, typename Err>
struct Storage<Tp, Err, IsTriviallyCopyConstructible::No, IsTriviallyMoveConstructible::No, IsTriviallyDestructible::No> {
    union Union {
        constexpr Union() : m_empty() { }

        template <class... Args>
        constexpr explicit Union(std::in_place_t, Args&&... args)
            : m_val(std::forward<Args>(args)...) { }

        template <class... Args>
        constexpr explicit Union(unexpect_t, Args&&... args)
            : m_unex(std::forward<Args>(args)...) { }

        template <class Func, class... Args>
        constexpr explicit Union(
            expected_construct_in_place_from_invoke_tag, Func&& f, Args&&... args)
            : m_val(std::invoke(std::forward<Func>(f), std::forward<Args>(args)...)) { }

        template <class Func, class... Args>
        constexpr explicit Union(
            expected_construct_unexpected_from_invoke_tag, Func&& f, Args&&... args)
            : m_unex(std::invoke(std::forward<Func>(f), std::forward<Args>(args)...)) { }

        // the Storage's destructor handles this
        constexpr ~Union() { }

        struct Empty { };
        Empty m_empty;
        Tp m_val;
        Err m_unex;
    };

    NO_UNIQUE_ADDRESS Union m_union;
    bool m_hasVal;

    template <class... Args>
    constexpr explicit Storage(std::in_place_t tag, Args&&... args)
        : m_union(tag, std::forward<Args>(args)...), m_hasVal(true) { }

    template <class... Args>
    constexpr explicit Storage(unexpect_t tag, Args&&... args)
        : m_union(tag, std::forward<Args>(args)...), m_hasVal(false) { }

    template <class Func, class... Args>
    constexpr explicit Storage(
        expected_construct_in_place_from_invoke_tag tag, Func&& f, Args&&... args)
        : m_union(tag, std::forward<Func>(f), std::forward<Args>(args)...), m_hasVal(true) { }

    template <class Func, class... Args>
    constexpr explicit Storage(
        expected_construct_unexpected_from_invoke_tag tag, Func&& f, Args&&... args)
        : m_union(tag, std::forward<Func>(f), std::forward<Args>(args)...), m_hasVal(false) { }

    constexpr Storage(const Storage& other) requires(std::is_copy_constructible_v<Tp> && std::is_copy_constructible_v<Err>)
    {
        if (other.m_hasVal)
            std::construct_at(std::addressof(m_union.m_val), other.m_union.m_val);
        else
            std::construct_at(std::addressof(m_union.m_unex), other.m_union.m_unex);
        m_hasVal = other.m_hasVal;
    }

    constexpr Storage(Storage&& other) requires(std::is_move_constructible_v<Tp> && std::is_move_constructible_v<Err>)
    {
        if (other.m_hasVal)
            std::construct_at(std::addressof(m_union.m_val), WTFMove(other.m_union.m_val));
        else
            std::construct_at(std::addressof(m_union.m_unex), WTFMove(other.m_union.m_unex));
        m_hasVal = other.m_hasVal;
    }

    constexpr ~Storage()
    {
        if (m_hasVal)
            std::destroy_at(std::addressof(m_union.m_val));
        else
            std::destroy_at(std::addressof(m_union.m_unex));
    }
};

template <typename Tp, typename Err>
struct Storage<Tp, Err, IsTriviallyCopyConstructible::Yes, IsTriviallyMoveConstructible::No, IsTriviallyDestructible::No> {
    union Union {
        constexpr Union() : m_empty() { }

        constexpr Union(const Union&) = default;
        constexpr Union& operator=(const Union&) = default;

        template <class... Args>
        constexpr explicit Union(std::in_place_t, Args&&... args)
            : m_val(std::forward<Args>(args)...) { }

        template <class... Args>
        constexpr explicit Union(unexpect_t, Args&&... args)
            : m_unex(std::forward<Args>(args)...) { }

        template <class Func, class... Args>
        constexpr explicit Union(
            expected_construct_in_place_from_invoke_tag, Func&& f, Args&&... args)
            : m_val(std::invoke(std::forward<Func>(f), std::forward<Args>(args)...)) { }

        template <class Func, class... Args>
        constexpr explicit Union(
            expected_construct_unexpected_from_invoke_tag, Func&& f, Args&&... args)
            : m_unex(std::invoke(std::forward<Func>(f), std::forward<Args>(args)...)) { }

        // the Storage's destructor handles this
        constexpr ~Union() { }

        struct Empty { };
        Empty m_empty;
        Tp m_val;
        Err m_unex;
    };

    NO_UNIQUE_ADDRESS Union m_union;
    bool m_hasVal;

    template <class... Args>
    constexpr explicit Storage(std::in_place_t tag, Args&&... args)
        : m_union(tag, std::forward<Args>(args)...), m_hasVal(true) { }

    template <class... Args>
    constexpr explicit Storage(unexpect_t tag, Args&&... args)
        : m_union(tag, std::forward<Args>(args)...), m_hasVal(false) { }

    template <class Func, class... Args>
    constexpr explicit Storage(
        expected_construct_in_place_from_invoke_tag tag, Func&& f, Args&&... args)
        : m_union(tag, std::forward<Func>(f), std::forward<Args>(args)...), m_hasVal(true) { }

    template <class Func, class... Args>
    constexpr explicit Storage(
        expected_construct_unexpected_from_invoke_tag tag, Func&& f, Args&&... args)
        : m_union(tag, std::forward<Func>(f), std::forward<Args>(args)...), m_hasVal(false) { }

    constexpr Storage(const Storage& other) = default;

    constexpr Storage(Storage&& other) requires(std::is_move_constructible_v<Tp> && std::is_move_constructible_v<Err>)
    {
        if (other.m_hasVal)
            std::construct_at(std::addressof(m_union.m_val), WTFMove(other.m_union.m_val));
        else
            std::construct_at(std::addressof(m_union.m_unex), WTFMove(other.m_union.m_unex));
        m_hasVal = other.m_hasVal;
    }

    constexpr ~Storage()
    {
        if (m_hasVal)
            std::destroy_at(std::addressof(m_union.m_val));
        else
            std::destroy_at(std::addressof(m_union.m_unex));
    }
};

template <typename Tp, typename Err>
struct Storage<Tp, Err, IsTriviallyCopyConstructible::No, IsTriviallyMoveConstructible::Yes, IsTriviallyDestructible::No> {
    union Union {
        constexpr Union() : m_empty() { }

        constexpr Union(const Union&) = default;
        constexpr Union& operator=(const Union&) = default;
        constexpr Union(Union&&) = default;
        constexpr Union& operator=(Union&&) = default;

        template <class... Args>
        constexpr explicit Union(std::in_place_t, Args&&... args)
            : m_val(std::forward<Args>(args)...) { }

        template <class... Args>
        constexpr explicit Union(unexpect_t, Args&&... args)
            : m_unex(std::forward<Args>(args)...) { }

        template <class Func, class... Args>
        constexpr explicit Union(
            expected_construct_in_place_from_invoke_tag, Func&& f, Args&&... args)
            : m_val(std::invoke(std::forward<Func>(f), std::forward<Args>(args)...)) { }

        template <class Func, class... Args>
        constexpr explicit Union(
            expected_construct_unexpected_from_invoke_tag, Func&& f, Args&&... args)
            : m_unex(std::invoke(std::forward<Func>(f), std::forward<Args>(args)...)) { }

        // the Storage's destructor handles this
        constexpr ~Union() { }

        struct Empty { };
        NO_UNIQUE_ADDRESS Empty m_empty;
        NO_UNIQUE_ADDRESS Tp m_val;
        NO_UNIQUE_ADDRESS Err m_unex;
    };

    NO_UNIQUE_ADDRESS Union m_union;
    bool m_hasVal;

    template <class... Args>
    constexpr explicit Storage(std::in_place_t tag, Args&&... args)
        : m_union(tag, std::forward<Args>(args)...), m_hasVal(true) { }

    template <class... Args>
    constexpr explicit Storage(unexpect_t tag, Args&&... args)
        : m_union(tag, std::forward<Args>(args)...), m_hasVal(false) { }

    template <class Func, class... Args>
    constexpr explicit Storage(
        expected_construct_in_place_from_invoke_tag tag, Func&& f, Args&&... args)
        : m_union(tag, std::forward<Func>(f), std::forward<Args>(args)...), m_hasVal(true) { }

    template <class Func, class... Args>
    constexpr explicit Storage(
        expected_construct_unexpected_from_invoke_tag tag, Func&& f, Args&&... args)
        : m_union(tag, std::forward<Func>(f), std::forward<Args>(args)...), m_hasVal(false) { }

    constexpr Storage(const Storage& other) requires(std::is_copy_constructible_v<Tp> && std::is_copy_constructible_v<Err>)
    {
        if (other.m_hasVal)
            std::construct_at(std::addressof(m_union.m_val), other.m_union.m_val);
        else
            std::construct_at(std::addressof(m_union.m_unex), other.m_union.m_unex);
        m_hasVal = other.m_hasVal;
    }

    constexpr Storage(Storage&& other) = default;

    constexpr ~Storage()
    {
        if (m_hasVal)
            std::destroy_at(std::addressof(m_union.m_val));
        else
            std::destroy_at(std::addressof(m_union.m_unex));
    }
};

template <typename Tp, typename Err>
struct Storage<Tp, Err, IsTriviallyCopyConstructible::Yes, IsTriviallyMoveConstructible::Yes, IsTriviallyDestructible::No> {
    union Union {
        constexpr Union(const Union&) = default;
        constexpr Union& operator=(const Union&) = default;
        constexpr Union(Union&&) = default;
        constexpr Union& operator=(Union&&) = default;

        template <class... Args>
        constexpr explicit Union(std::in_place_t, Args&&... args)
            : m_val(std::forward<Args>(args)...) { }

        template <class... Args>
        constexpr explicit Union(unexpect_t, Args&&... args)
            : m_unex(std::forward<Args>(args)...) { }

        template <class Func, class... Args>
        constexpr explicit Union(
            expected_construct_in_place_from_invoke_tag, Func&& f, Args&&... args)
            : m_val(std::invoke(std::forward<Func>(f), std::forward<Args>(args)...)) { }

        template <class Func, class... Args>
        constexpr explicit Union(
            expected_construct_unexpected_from_invoke_tag, Func&& f, Args&&... args)
            : m_unex(std::invoke(std::forward<Func>(f), std::forward<Args>(args)...)) { }

        // the Storage's destructor handles this
        constexpr ~Union() { }

        NO_UNIQUE_ADDRESS Tp m_val;
        NO_UNIQUE_ADDRESS Err m_unex;
    };

    NO_UNIQUE_ADDRESS Union m_union;
    bool m_hasVal;

    template <class... Args>
    constexpr explicit Storage(std::in_place_t tag, Args&&... args)
        : m_union(tag, std::forward<Args>(args)...), m_hasVal(true) { }

    template <class... Args>
    constexpr explicit Storage(unexpect_t tag, Args&&... args)
        : m_union(tag, std::forward<Args>(args)...), m_hasVal(false) { }

    template <class Func, class... Args>
    constexpr explicit Storage(
        expected_construct_in_place_from_invoke_tag tag, Func&& f, Args&&... args)
        : m_union(tag, std::forward<Func>(f), std::forward<Args>(args)...), m_hasVal(true) { }

    template <class Func, class... Args>
    constexpr explicit Storage(
        expected_construct_unexpected_from_invoke_tag tag, Func&& f, Args&&... args)
        : m_union(tag, std::forward<Func>(f), std::forward<Args>(args)...), m_hasVal(false) { }

    constexpr Storage(const Storage& other) = default;

    constexpr Storage(Storage&& other) = default;

    constexpr ~Storage()
    {
        if (m_hasVal)
            std::destroy_at(std::addressof(m_union.m_val));
        else
            std::destroy_at(std::addressof(m_union.m_unex));
    }
};

template <typename Tp, typename Err>
struct Storage<Tp, Err, IsTriviallyCopyConstructible::No, IsTriviallyMoveConstructible::No, IsTriviallyDestructible::Yes> {
    union Union {
        constexpr Union() : m_empty() { }

        template <class... Args>
        constexpr explicit Union(std::in_place_t, Args&&... args)
            : m_val(std::forward<Args>(args)...) { }

        template <class... Args>
        constexpr explicit Union(unexpect_t, Args&&... args)
            : m_unex(std::forward<Args>(args)...) { }

        template <class Func, class... Args>
        constexpr explicit Union(
            expected_construct_in_place_from_invoke_tag, Func&& f, Args&&... args)
            : m_val(std::invoke(std::forward<Func>(f), std::forward<Args>(args)...)) { }

        template <class Func, class... Args>
        constexpr explicit Union(
            expected_construct_unexpected_from_invoke_tag, Func&& f, Args&&... args)
            : m_unex(std::invoke(std::forward<Func>(f), std::forward<Args>(args)...)) { }

        struct Empty { };
        Empty m_empty;
        Tp m_val;
        Err m_unex;
    };

    NO_UNIQUE_ADDRESS Union m_union;
    bool m_hasVal;

    template <class... Args>
    constexpr explicit Storage(std::in_place_t tag, Args&&... args)
        : m_union(tag, std::forward<Args>(args)...), m_hasVal(true) { }

    template <class... Args>
    constexpr explicit Storage(unexpect_t tag, Args&&... args)
        : m_union(tag, std::forward<Args>(args)...), m_hasVal(false) { }

    template <class Func, class... Args>
    constexpr explicit Storage(
        expected_construct_in_place_from_invoke_tag tag, Func&& f, Args&&... args)
        : m_union(tag, std::forward<Func>(f), std::forward<Args>(args)...), m_hasVal(true) { }

    template <class Func, class... Args>
    constexpr explicit Storage(
        expected_construct_unexpected_from_invoke_tag tag, Func&& f, Args&&... args)
        : m_union(tag, std::forward<Func>(f), std::forward<Args>(args)...), m_hasVal(false) { }

    constexpr Storage(const Storage& other) requires(std::is_copy_constructible_v<Tp> && std::is_copy_constructible_v<Err>)
    {
        if (other.m_hasVal)
            std::construct_at(std::addressof(m_union.m_val), other.m_union.m_val);
        else
            std::construct_at(std::addressof(m_union.m_unex), other.m_union.m_unex);
        m_hasVal = other.m_hasVal;
    }

    constexpr Storage(Storage&& other) requires(std::is_move_constructible_v<Tp> && std::is_move_constructible_v<Err>)
    {
        if (other.m_hasVal)
            std::construct_at(std::addressof(m_union.m_val), WTFMove(other.m_union.m_val));
        else
            std::construct_at(std::addressof(m_union.m_unex), WTFMove(other.m_union.m_unex));
        m_hasVal = other.m_hasVal;
    }
};

template <typename Tp, typename Err>
struct Storage<Tp, Err, IsTriviallyCopyConstructible::Yes, IsTriviallyMoveConstructible::No, IsTriviallyDestructible::Yes> {
    union Union {
        constexpr Union() : m_empty() { }

        constexpr Union(const Union&) = default;
        constexpr Union& operator=(const Union&) = default;

        template <class... Args>
        constexpr explicit Union(std::in_place_t, Args&&... args)
            : m_val(std::forward<Args>(args)...) { }

        template <class... Args>
        constexpr explicit Union(unexpect_t, Args&&... args)
            : m_unex(std::forward<Args>(args)...) { }

        template <class Func, class... Args>
        constexpr explicit Union(
            expected_construct_in_place_from_invoke_tag, Func&& f, Args&&... args)
            : m_val(std::invoke(std::forward<Func>(f), std::forward<Args>(args)...)) { }

        template <class Func, class... Args>
        constexpr explicit Union(
            expected_construct_unexpected_from_invoke_tag, Func&& f, Args&&... args)
            : m_unex(std::invoke(std::forward<Func>(f), std::forward<Args>(args)...)) { }

        struct Empty { };
        Empty m_empty;
        Tp m_val;
        Err m_unex;
    };

    NO_UNIQUE_ADDRESS Union m_union;
    bool m_hasVal;

    template <class... Args>
    constexpr explicit Storage(std::in_place_t tag, Args&&... args)
        : m_union(tag, std::forward<Args>(args)...), m_hasVal(true) { }

    template <class... Args>
    constexpr explicit Storage(unexpect_t tag, Args&&... args)
        : m_union(tag, std::forward<Args>(args)...), m_hasVal(false) { }

    template <class Func, class... Args>
    constexpr explicit Storage(
        expected_construct_in_place_from_invoke_tag tag, Func&& f, Args&&... args)
        : m_union(tag, std::forward<Func>(f), std::forward<Args>(args)...), m_hasVal(true) { }

    template <class Func, class... Args>
    constexpr explicit Storage(
        expected_construct_unexpected_from_invoke_tag tag, Func&& f, Args&&... args)
        : m_union(tag, std::forward<Func>(f), std::forward<Args>(args)...), m_hasVal(false) { }

    constexpr Storage(const Storage& other) = default;

    constexpr Storage(Storage&& other) requires(std::is_move_constructible_v<Tp> && std::is_move_constructible_v<Err>)
    {
        if (other.m_hasVal)
            std::construct_at(std::addressof(m_union.m_val), WTFMove(other.m_union.m_val));
        else
            std::construct_at(std::addressof(m_union.m_unex), WTFMove(other.m_union.m_unex));
        m_hasVal = other.m_hasVal;
    }
};

template <typename Tp, typename Err>
struct Storage<Tp, Err, IsTriviallyCopyConstructible::No, IsTriviallyMoveConstructible::Yes, IsTriviallyDestructible::Yes> {
    union Union {
        constexpr Union() : m_empty() { }

        constexpr Union(const Union&) = default;
        constexpr Union& operator=(const Union&) = default;
        constexpr Union(Union&&) = default;
        constexpr Union& operator=(Union&&) = default;

        template <class... Args>
        constexpr explicit Union(std::in_place_t, Args&&... args)
            : m_val(std::forward<Args>(args)...) { }

        template <class... Args>
        constexpr explicit Union(unexpect_t, Args&&... args)
            : m_unex(std::forward<Args>(args)...) { }

        template <class Func, class... Args>
        constexpr explicit Union(
            expected_construct_in_place_from_invoke_tag, Func&& f, Args&&... args)
            : m_val(std::invoke(std::forward<Func>(f), std::forward<Args>(args)...)) { }

        template <class Func, class... Args>
        constexpr explicit Union(
            expected_construct_unexpected_from_invoke_tag, Func&& f, Args&&... args)
            : m_unex(std::invoke(std::forward<Func>(f), std::forward<Args>(args)...)) { }

        struct Empty { };
        NO_UNIQUE_ADDRESS Empty m_empty;
        NO_UNIQUE_ADDRESS Tp m_val;
        NO_UNIQUE_ADDRESS Err m_unex;
    };

    NO_UNIQUE_ADDRESS Union m_union;
    bool m_hasVal;

    template <class... Args>
    constexpr explicit Storage(std::in_place_t tag, Args&&... args)
        : m_union(tag, std::forward<Args>(args)...), m_hasVal(true) { }

    template <class... Args>
    constexpr explicit Storage(unexpect_t tag, Args&&... args)
        : m_union(tag, std::forward<Args>(args)...), m_hasVal(false) { }

    template <class Func, class... Args>
    constexpr explicit Storage(
        expected_construct_in_place_from_invoke_tag tag, Func&& f, Args&&... args)
        : m_union(tag, std::forward<Func>(f), std::forward<Args>(args)...), m_hasVal(true) { }

    template <class Func, class... Args>
    constexpr explicit Storage(
        expected_construct_unexpected_from_invoke_tag tag, Func&& f, Args&&... args)
        : m_union(tag, std::forward<Func>(f), std::forward<Args>(args)...), m_hasVal(false) { }

    constexpr Storage(const Storage& other) requires(std::is_copy_constructible_v<Tp> && std::is_copy_constructible_v<Err>)
    {
        if (other.m_hasVal)
            std::construct_at(std::addressof(m_union.m_val), other.m_union.m_val);
        else
            std::construct_at(std::addressof(m_union.m_unex), other.m_union.m_unex);
        m_hasVal = other.m_hasVal;
    }

    constexpr Storage(Storage&& other) = default;
};

template <typename Tp, typename Err>
struct Storage<Tp, Err, IsTriviallyCopyConstructible::Yes, IsTriviallyMoveConstructible::Yes, IsTriviallyDestructible::Yes> {
    union Union {
        constexpr Union(const Union&) = default;
        constexpr Union& operator=(const Union&) = default;
        constexpr Union(Union&&) = default;
        constexpr Union& operator=(Union&&) = default;

        template <class... Args>
        constexpr explicit Union(std::in_place_t, Args&&... args)
            : m_val(std::forward<Args>(args)...) { }

        template <class... Args>
        constexpr explicit Union(unexpect_t, Args&&... args)
            : m_unex(std::forward<Args>(args)...) { }

        template <class Func, class... Args>
        constexpr explicit Union(
            expected_construct_in_place_from_invoke_tag, Func&& f, Args&&... args)
            : m_val(std::invoke(std::forward<Func>(f), std::forward<Args>(args)...)) { }

        template <class Func, class... Args>
        constexpr explicit Union(
            expected_construct_unexpected_from_invoke_tag, Func&& f, Args&&... args)
            : m_unex(std::invoke(std::forward<Func>(f), std::forward<Args>(args)...)) { }

        NO_UNIQUE_ADDRESS Tp m_val;
        NO_UNIQUE_ADDRESS Err m_unex;
    };

    NO_UNIQUE_ADDRESS Union m_union;
    bool m_hasVal;

    template <class... Args>
    constexpr explicit Storage(std::in_place_t tag, Args&&... args)
        : m_union(tag, std::forward<Args>(args)...), m_hasVal(true) { }

    template <class... Args>
    constexpr explicit Storage(unexpect_t tag, Args&&... args)
        : m_union(tag, std::forward<Args>(args)...), m_hasVal(false) { }

    template <class Func, class... Args>
    constexpr explicit Storage(
        expected_construct_in_place_from_invoke_tag tag, Func&& f, Args&&... args)
        : m_union(tag, std::forward<Func>(f), std::forward<Args>(args)...), m_hasVal(true) { }

    template <class Func, class... Args>
    constexpr explicit Storage(
        expected_construct_unexpected_from_invoke_tag tag, Func&& f, Args&&... args)
        : m_union(tag, std::forward<Func>(f), std::forward<Args>(args)...), m_hasVal(false) { }

    constexpr Storage(const Storage& other) = default;

    constexpr Storage(Storage&& other) = default;
};

} // namespace detail

template <class Tp, class Err>
class expected {
    WTF_MAKE_FAST_ALLOCATED;
    static_assert(
        !std::is_reference_v<Tp>
        && !std::is_function_v<Tp>
        && !std::is_same_v<std::remove_cv_t<Tp>, std::in_place_t>
        && !std::is_same_v<std::remove_cv_t<Tp>, unexpect_t>
        && !detail::is_std_unexpected<std::remove_cv_t<Tp>>::value
        && detail::valid_std_unexpected<Err>::value,
        "[expected.object.general] A program that instantiates the definition of template expected<T, E> for a "
        "reference type, a function type, or for possibly cv-qualified types std::std::in_place_t, unexpect_t, or a "
        "specialization of unexpected for the T parameter is ill-formed. A program that instantiates the "
        "definition of the template expected<T, E> with a type for the E parameter that is not a valid "
        "template argument for unexpected is ill-formed."); // NOLINT

    template <class Up, class OtherErr>
    friend class expected;

public:
    using value_type      = Tp;
    using error_type      = Err;
    using unexpected_type = unexpected<Err>;

    template <class Up>
    using rebind = expected<Up, error_type>;

    // [expected.object.ctor], constructors
    constexpr expected() requires std::is_default_constructible_v<Tp>
        : m_storage(std::in_place) { }

    constexpr expected(const expected&) = delete;

    constexpr expected(const expected&)
        requires(std::is_copy_constructible_v<Tp> && std::is_copy_constructible_v<Err>)
    = default;

    constexpr expected(expected&&)
        requires(std::is_move_constructible_v<Tp> && std::is_move_constructible_v<Err>)
    = default;

private:
    template <class Up, class OtherErr, class UfQual, class OtherErrQual>
    using CanConvert =
        std::conjunction<
            std::is_constructible<Tp, UfQual>,
            std::is_constructible<Err, OtherErrQual>,
            std::conditional_t<
                std::negation<std::is_same<std::remove_cv_t<Tp>, bool>>::value,
                std::conjunction<
                    std::negation<std::is_constructible<Tp, expected<Up, OtherErr>&>>,
                    std::negation<std::is_constructible<Tp, expected<Up, OtherErr>>>,
                    std::negation<std::is_constructible<Tp, const expected<Up, OtherErr>&>>,
                    std::negation<std::is_constructible<Tp, const expected<Up, OtherErr>>>,
                    std::negation<std::is_convertible<expected<Up, OtherErr>&, Tp>>,
                    std::negation<std::is_convertible<expected<Up, OtherErr>&&, Tp>>,
                    std::negation<std::is_convertible<const expected<Up, OtherErr>&, Tp>>,
                    std::negation<std::is_convertible<const expected<Up, OtherErr>&&, Tp>>>,
                std::true_type>,
            std::negation<std::is_constructible<unexpected<Err>, expected<Up, OtherErr>&>>,
            std::negation<std::is_constructible<unexpected<Err>, expected<Up, OtherErr>>>,
            std::negation<std::is_constructible<unexpected<Err>, const expected<Up, OtherErr>&>>,
            std::negation<std::is_constructible<unexpected<Err>, const expected<Up, OtherErr>>>>;

    template <class Func, class... Args>
    constexpr explicit expected(
        detail::expected_construct_in_place_from_invoke_tag tag, Func&& f, Args&&... args)
        : m_storage(tag, std::forward<Func>(f), std::forward<Args>(args)...) { }

    template <class Func, class... Args>
    constexpr explicit expected(
        detail::expected_construct_unexpected_from_invoke_tag tag, Func&& f, Args&&... args)
        : m_storage(tag, std::forward<Func>(f), std::forward<Args>(args)...) { }

public:
    template <class Up, class OtherErr>
        requires CanConvert<Up, OtherErr, const Up&, const OtherErr&>::value
    constexpr explicit(!std::is_convertible_v<const Up&, Tp> || !std::is_convertible_v<const OtherErr&, Err>)
    expected(const expected<Up, OtherErr>& other)
        : m_storage(other.m_storage) { }

    template <class Up, class OtherErr>
        requires CanConvert<Up, OtherErr, Up, OtherErr>::value
    constexpr explicit(!std::is_convertible_v<Up, Tp> || !std::is_convertible_v<OtherErr, Err>)
    expected(expected<Up, OtherErr>&& other)
        : m_storage(other.m_storage) { }

    template <class Up = Tp>
        requires(!std::is_same_v<std::remove_cvref_t<Up>, std::in_place_t> && !std::is_same_v<expected, std::remove_cvref_t<Up>>
            && std::is_constructible_v<Tp, Up> && !detail::is_std_unexpected<std::remove_cvref_t<Up>>::value
            && (!std::is_same_v<std::remove_cv_t<Tp>, bool> || !detail::is_std_expected<std::remove_cvref_t<Up>>::value))
    constexpr explicit(!std::is_convertible_v<Up, Tp>) expected(Up&& u)
        : m_storage(std::in_place, std::forward<Up>(u)) { }

    template <class OtherErr>
        requires std::is_constructible_v<Err, const OtherErr&>
    constexpr explicit(!std::is_convertible_v<const OtherErr&, Err>)
    expected(const unexpected<OtherErr>& unex)
        : m_storage(unexpect, unex.error()) { }

    template <class OtherErr>
        requires std::is_constructible_v<Err, OtherErr>
    constexpr explicit(!std::is_convertible_v<OtherErr, Err>)
    expected(unexpected<OtherErr>&& unex)
        : m_storage(unexpect, WTFMove(unex.error())) { }

    template <class... Args>
        requires std::is_constructible_v<Tp, Args...>
    constexpr explicit expected(std::in_place_t tag, Args&&... args)
        : m_storage(tag, std::forward<Args>(args)...) { }

    template <class Up, class... Args>
        requires std::is_constructible_v< Tp, std::initializer_list<Up>&, Args... >
    constexpr explicit
    expected(std::in_place_t tag, std::initializer_list<Up> il, Args&&... args)
        : m_storage(tag, il, std::forward<Args>(args)...) { }

    template <class... Args>
        requires std::is_constructible_v<Err, Args...>
    constexpr explicit expected(unexpect_t tag, Args&&... args)
        : m_storage(tag, std::forward<Args>(args)...) { }

    template <class Up, class... Args>
        requires std::is_constructible_v< Err, std::initializer_list<Up>&, Args... >
    constexpr explicit
    expected(unexpect_t tag, std::initializer_list<Up> il, Args&&... args)
        : m_storage(tag, il, std::forward<Args>(args)...) { }

private:
    template <class T1, class T2, class... Args>
    static constexpr void reinitExpected(T1& newval, T2& oldval, Args&&... args)
    {
        if constexpr (std::is_constructible_v<T1, Args...>) {
            std::destroy_at(std::addressof(oldval));
            std::construct_at(std::addressof(newval), std::forward<Args>(args)...);
        } else if constexpr (std::is_move_constructible_v<T1>) {
            T1 tmp(std::forward<Args>(args)...);
            std::destroy_at(std::addressof(oldval));
            std::construct_at(std::addressof(newval), WTFMove(tmp));
        } else {
            static_assert(std::is_move_constructible_v<T2>);
            T2 tmp(WTFMove(oldval));
            std::destroy_at(std::addressof(oldval));
            std::construct_at(std::addressof(newval), std::forward<Args>(args)...);
            (void)tmp;
        }
    }

public:
    // [expected.object.assign], assignment
    constexpr expected& operator=(const expected&) = delete;

    constexpr expected& operator=(const expected& rhs)
        requires(std::is_copy_assignable_v<Tp>
            && std::is_copy_constructible_v<Tp>
            && std::is_copy_assignable_v<Err>
            && std::is_copy_constructible_v<Err>
            && (std::is_move_constructible_v<Tp> || std::is_move_constructible_v<Err>))
    {
        if (m_storage.m_hasVal && rhs.m_storage.m_hasVal)
            m_storage.m_union.m_val = rhs.m_storage.m_union.m_val;
        else if (m_storage.m_hasVal)
            reinitExpected(m_storage.m_union.m_unex, m_storage.m_union.m_val, rhs.m_storage.m_union.m_unex);
        else if (rhs.m_storage.m_hasVal)
            reinitExpected(m_storage.m_union.m_val, m_storage.m_union.m_unex, rhs.m_storage.m_union.m_val);
        else
            m_storage.m_union.m_unex = rhs.m_storage.m_union.m_unex;
        m_storage.m_hasVal = rhs.m_storage.m_hasVal;
        return *this;
    }

    constexpr expected& operator=(expected&& rhs)
        requires(std::is_move_constructible_v<Tp>
            && std::is_move_assignable_v<Tp>
            && std::is_move_constructible_v<Err>
            && std::is_move_assignable_v<Err>
            && (std::is_move_constructible_v<Tp> || std::is_move_constructible_v<Err>))
    {
        if (m_storage.m_hasVal && rhs.m_storage.m_hasVal)
            m_storage.m_union.m_val = WTFMove(rhs.m_storage.m_union.m_val);
        else if (m_storage.m_hasVal)
            reinitExpected(m_storage.m_union.m_unex, m_storage.m_union.m_val, WTFMove(rhs.m_storage.m_union.m_unex));
        else if (rhs.m_storage.m_hasVal)
            reinitExpected(m_storage.m_union.m_val, m_storage.m_union.m_unex, WTFMove(rhs.m_storage.m_union.m_val));
        else
            m_storage.m_union.m_unex = WTFMove(rhs.m_storage.m_union.m_unex);
        m_storage.m_hasVal = rhs.m_storage.m_hasVal;
        return *this;
    }

    template <class Up = Tp>
    constexpr expected& operator=(Up&& v)
        requires(!std::is_same_v<expected, std::remove_cvref_t<Up>>
            && !detail::is_std_unexpected<std::remove_cvref_t<Up>>::value
            && std::is_constructible_v<Tp, Up>
            && std::is_assignable_v<Tp&, Up>
            && (std::is_constructible_v<Tp, Up> || std::is_move_constructible_v<Tp> || std::is_move_constructible_v<Err>))
    {
        if (m_storage.m_hasVal)
            m_storage.m_union.m_val = std::forward<Up>(v);
        else {
            reinitExpected(m_storage.m_union.m_val, m_storage.m_union.m_unex, std::forward<Up>(v));
            m_storage.m_hasVal = true;
        }
        return *this;
    }

private:
    template <class OtherErrQual>
    static constexpr bool canAssignFromUnexpected =
        std::conjunction<
            std::is_constructible<Err, OtherErrQual>,
            std::is_assignable<Err&, OtherErrQual>,
            detail::Lazy<
                std::disjunction,
                std::is_constructible<Err, OtherErrQual>,
                std::is_move_constructible<Tp>,
                std::is_move_constructible<Err>>>::value;

public:
    template <class OtherErr>
        requires(canAssignFromUnexpected<const OtherErr&>)
    constexpr expected& operator=(const unexpected<OtherErr>& un)
    {
        if (m_storage.m_hasVal) {
            reinitExpected(m_storage.m_union.m_unex, m_storage.m_union.m_val, un.error());
            m_storage.m_hasVal = false;
        } else
            m_storage.m_union.m_unex = un.error();
        return *this;
    }

    template <class OtherErr>
        requires(canAssignFromUnexpected<OtherErr>)
    constexpr expected& operator=(unexpected<OtherErr>&& un)
    {
        if (m_storage.m_hasVal) {
            reinitExpected(m_storage.m_union.m_unex, m_storage.m_union.m_val, WTFMove(un.error()));
            m_storage.m_hasVal = false;
        } else
            m_storage.m_union.m_unex = WTFMove(un.error());
        return *this;
    }

    template <class... Args>
        requires std::is_constructible_v<Tp, Args...>
    constexpr Tp& emplace(Args&&... args)
    {
        if (m_storage.m_hasVal)
            std::destroy_at(std::addressof(m_storage.m_union.m_val));
        else {
            std::destroy_at(std::addressof(m_storage.m_union.m_unex));
            m_storage.m_hasVal = true;
        }
        return *std::construct_at(std::addressof(m_storage.m_union.m_val), std::forward<Args>(args)...);
    }

    template <class Up, class... Args>
        requires std::is_constructible_v< Tp, std::initializer_list<Up>&, Args... >
    constexpr Tp& emplace(std::initializer_list<Up> il, Args&&... args)
    {
        if (m_storage.m_hasVal)
            std::destroy_at(std::addressof(m_storage.m_union.m_val));
        else {
            std::destroy_at(std::addressof(m_storage.m_union.m_unex));
            m_storage.m_hasVal = true;
        }
        return *std::construct_at(std::addressof(m_storage.m_union.m_val), il, std::forward<Args>(args)...);
    }

public:
    // [expected.object.swap], swap
    constexpr void swap(expected& rhs)
        requires(std::is_swappable_v<Tp>
            && std::is_swappable_v<Err>
            && std::is_move_constructible_v<Tp>
            && std::is_move_constructible_v<Err>
            && (std::is_move_constructible_v<Tp> || std::is_move_constructible_v<Err>))
    {
        auto swapValUnexImpl = [&](expected& withVal, expected& withErr) {
            if constexpr (std::is_move_constructible_v<Err>) {
                Err tmp(WTFMove(withErr.m_storage.m_union.m_unex));
                std::destroy_at(std::addressof(withErr.m_storage.m_union.m_unex));
                std::construct_at(std::addressof(withErr.m_storage.m_union.m_val), WTFMove(withVal.m_storage.m_union.m_val));
                std::destroy_at(std::addressof(withVal.m_storage.m_union.m_val));
                std::construct_at(std::addressof(withVal.m_storage.m_union.m_unex), WTFMove(tmp));
            } else {
                static_assert(std::is_move_constructible_v<Tp>);
                Tp tmp(WTFMove(withVal.m_storage.m_union.m_val));
                std::destroy_at(std::addressof(withVal.m_storage.m_union.m_val));
                std::construct_at(std::addressof(withVal.m_storage.m_union.m_unex), WTFMove(withErr.m_storage.m_union.m_unex));
                std::destroy_at(std::addressof(withErr.m_storage.m_union.m_unex));
                std::construct_at(std::addressof(withErr.m_storage.m_union.m_val), WTFMove(tmp));
            }
            withVal.m_storage.m_hasVal = false;
            withErr.m_storage.m_hasVal = true;
        };

        if (m_storage.m_hasVal) {
            if (rhs.m_storage.m_hasVal) {
                using std::swap; // NOLINT
                swap(m_storage.m_union.m_val, rhs.m_storage.m_union.m_val);
            } else
                swapValUnexImpl(*this, rhs);
        } else {
            if (rhs.m_storage.m_hasVal)
                swapValUnexImpl(rhs, *this);
            else {
                using std::swap; // NOLINT
                swap(m_storage.m_union.m_unex, rhs.m_storage.m_union.m_unex);
            }
        }
    }

    friend constexpr void swap(expected& x, expected& y)
        requires(std::is_swappable_v<Tp>
            && std::is_swappable_v<Err>
            && std::is_move_constructible_v<Tp>
            && std::is_move_constructible_v<Err>
            && (std::is_move_constructible_v<Tp> || std::is_move_constructible_v<Err>))
    {
        x.swap(y);
    }

    // [expected.object.obs], observers
    constexpr const Tp* operator->() const
    {
        RELEASE_ASSERT(m_storage.m_hasVal, "expected::operator-> requires the expected to contain a value");
        return std::addressof(m_storage.m_union.m_val);
    }

    constexpr Tp* operator->()
    {
        RELEASE_ASSERT(m_storage.m_hasVal, "expected::operator-> requires the expected to contain a value");
        return std::addressof(m_storage.m_union.m_val);
    }

    constexpr const Tp& operator*() const&
    { // NOLINT
        RELEASE_ASSERT(m_storage.m_hasVal, "expected::operator* requires the expected to contain a value");
        return m_storage.m_union.m_val;
    }

    constexpr Tp& operator*() &
    { // NOLINT
        RELEASE_ASSERT(m_storage.m_hasVal, "expected::operator* requires the expected to contain a value");
        return m_storage.m_union.m_val;
    }

    constexpr const Tp&& operator*() const&&
    { // NOLINT
        RELEASE_ASSERT(m_storage.m_hasVal, "expected::operator* requires the expected to contain a value");
        return WTFMove(m_storage.m_union.m_val);
    }

    constexpr Tp&& operator*() && // NOLINT
    { // NOLINT
        RELEASE_ASSERT(m_storage.m_hasVal, "expected::operator* requires the expected to contain a value");
        return WTFMove(m_storage.m_union.m_val);
    }

    constexpr explicit operator bool() const { return m_storage.m_hasVal; }

    constexpr bool has_value() const { return m_storage.m_hasVal; }

    constexpr const Tp& value() const&
    { // NOLINT
        RELEASE_ASSERT(m_storage.m_hasVal);
        return m_storage.m_union.m_val;
    }

    constexpr Tp& value() &
    { // NOLINT
        RELEASE_ASSERT(m_storage.m_hasVal);
        return m_storage.m_union.m_val;
    }

    constexpr const Tp&& value() const&&
    { // NOLINT
        RELEASE_ASSERT(m_storage.m_hasVal);
        return WTFMove(m_storage.m_union.m_val);
    }

    constexpr Tp&& value() && // NOLINT
    { // NOLINT
        RELEASE_ASSERT(m_storage.m_hasVal);
        return WTFMove(m_storage.m_union.m_val);
    }

    constexpr const Err& error() const&
    { // NOLINT
        RELEASE_ASSERT(!m_storage.m_hasVal, "expected::error requires the expected to contain an error");
        return m_storage.m_union.m_unex;
    }

    constexpr Err& error() &
    { // NOLINT
        RELEASE_ASSERT(!m_storage.m_hasVal, "expected::error requires the expected to contain an error");
        return m_storage.m_union.m_unex;
    }

    constexpr const Err&& error() const&&
    { // NOLINT
        RELEASE_ASSERT(!m_storage.m_hasVal, "expected::error requires the expected to contain an error");
        return WTFMove(m_storage.m_union.m_unex);
    }

    constexpr Err&& error() && // NOLINT
    { // NOLINT
        RELEASE_ASSERT(!m_storage.m_hasVal, "expected::error requires the expected to contain an error");
        return WTFMove(m_storage.m_union.m_unex);
    }

    template <class Up>
    constexpr Tp value_or(Up&& v) const&
    { // NOLINT
        static_assert(std::is_copy_constructible_v<Tp>, "value_type has to be copy constructible");
        static_assert(std::is_convertible_v<Up, Tp>, "argument has to be convertible to value_type");
        return m_storage.m_hasVal ? m_storage.m_union.m_val : static_cast<Tp>(std::forward<Up>(v));
    }

    template <class Up>
    constexpr Tp value_or(Up&& v) && // NOLINT
    { // NOLINT
        static_assert(std::is_move_constructible_v<Tp>, "value_type has to be move constructible");
        static_assert(std::is_convertible_v<Up, Tp>, "argument has to be convertible to value_type");
        return m_storage.m_hasVal ? WTFMove(m_storage.m_union.m_val) : static_cast<Tp>(std::forward<Up>(v));
    }

    template <class Up = Err>
    constexpr Err error_or(Up&& err) const&
    { // NOLINT
        static_assert(std::is_copy_constructible_v<Err>, "error_type has to be copy constructible");
        static_assert(std::is_convertible_v<Up, Err>, "argument has to be convertible to error_type");
        if (has_value())
            return std::forward<Up>(err);
        return error();
    }

    template <class Up = Err>
    constexpr Err error_or(Up&& err) && // NOLINT
    { // NOLINT
        static_assert(std::is_move_constructible_v<Err>, "error_type has to be move constructible");
        static_assert(std::is_convertible_v<Up, Err>, "argument has to be convertible to error_type");
        if (has_value())
            return std::forward<Up>(err);
        return WTFMove(error());
    }

    // [expected.void.monadic], monadic
    template <class Func>
        requires std::is_constructible_v<Err, Err&>
    constexpr auto and_then(Func&& f) &
    { // NOLINT
        using Up = std::remove_cvref_t<std::invoke_result_t<Func, Tp&>>;
        static_assert(detail::is_std_expected<Up>::value, "The result of f(**this) must be a specialization of std::expected");
        static_assert(std::is_same_v<typename Up::error_type, Err>,
            "The result of f(**this) must have the same error_type as this expected");
        if (has_value())
            return std::invoke(std::forward<Func>(f), m_storage.m_union.m_val);
        return Up(unexpect, error());
    }

    template <class Func>
        requires std::is_constructible_v<Err, const Err&>
    constexpr auto and_then(Func&& f) const&
    { // NOLINT
        using Up = std::remove_cvref_t<std::invoke_result_t<Func, const Tp&>>;
        static_assert(detail::is_std_expected<Up>::value, "The result of f(**this) must be a specialization of std::expected");
        static_assert(std::is_same_v<typename Up::error_type, Err>,
            "The result of f(**this) must have the same error_type as this expected");
        if (has_value())
            return std::invoke(std::forward<Func>(f), m_storage.m_union.m_val);
        return Up(unexpect, error());
    }

    template <class Func>
        requires std::is_constructible_v<Err, Err&&>
    constexpr auto and_then(Func&& f) && // NOLINT
    { // NOLINT
        using Up = std::remove_cvref_t<std::invoke_result_t<Func, Tp&&>>;
        static_assert(
            detail::is_std_expected<Up>::value, "The result of f(WTFMove(**this)) must be a specialization of std::expected");
        static_assert(std::is_same_v<typename Up::error_type, Err>,
            "The result of f(WTFMove(**this)) must have the same error_type as this expected");
        if (has_value())
            return std::invoke(std::forward<Func>(f), WTFMove(m_storage.m_union.m_val));
        return Up(unexpect, WTFMove(error()));
    }

    template <class Func>
        requires std::is_constructible_v<Err, const Err&&>
    constexpr auto and_then(Func&& f) const&&
    { // NOLINT
        using Up = std::remove_cvref_t<std::invoke_result_t<Func, const Tp&&>>;
        static_assert(
            detail::is_std_expected<Up>::value, "The result of f(WTFMove(**this)) must be a specialization of std::expected");
        static_assert(std::is_same_v<typename Up::error_type, Err>,
            "The result of f(WTFMove(**this)) must have the same error_type as this expected");
        if (has_value())
            return std::invoke(std::forward<Func>(f), WTFMove(m_storage.m_union.m_val));
        return Up(unexpect, WTFMove(error()));
    }

    template <class Func>
        requires std::is_constructible_v<Tp, Tp&>
    constexpr auto or_else(Func&& f) &
    { // NOLINT
        using Gp = std::remove_cvref_t<std::invoke_result_t<Func, Err&>>;
        static_assert(detail::is_std_expected<Gp>::value, "The result of f(error()) must be a specialization of std::expected");
        static_assert(std::is_same_v<typename Gp::value_type, Tp>,
            "The result of f(error()) must have the same value_type as this expected");
        if (has_value())
            return Gp(std::in_place, m_storage.m_union.m_val);
        return std::invoke(std::forward<Func>(f), error());
    }

    template <class Func>
        requires std::is_constructible_v<Tp, const Tp&>
    constexpr auto or_else(Func&& f) const&
    { // NOLINT
        using Gp = std::remove_cvref_t<std::invoke_result_t<Func, const Err&>>;
        static_assert(detail::is_std_expected<Gp>::value, "The result of f(error()) must be a specialization of std::expected");
        static_assert(std::is_same_v<typename Gp::value_type, Tp>,
            "The result of f(error()) must have the same value_type as this expected");
        if (has_value())
            return Gp(std::in_place, m_storage.m_union.m_val);
        return std::invoke(std::forward<Func>(f), error());
    }

    template <class Func>
        requires std::is_constructible_v<Tp, Tp&&>
    constexpr auto or_else(Func&& f) && // NOLINT
    { // NOLINT
        using Gp = std::remove_cvref_t<std::invoke_result_t<Func, Err&&>>;
        static_assert(
            detail::is_std_expected<Gp>::value, "The result of f(WTFMove(error())) must be a specialization of std::expected");
        static_assert(std::is_same_v<typename Gp::value_type, Tp>,
            "The result of f(WTFMove(error())) must have the same value_type as this expected");
        if (has_value())
            return Gp(std::in_place, WTFMove(m_storage.m_union.m_val));
        return std::invoke(std::forward<Func>(f), WTFMove(error()));
    }

    template <class Func>
        requires std::is_constructible_v<Tp, const Tp&&>
    constexpr auto or_else(Func&& f) const&&
    { // NOLINT
        using Gp = std::remove_cvref_t<std::invoke_result_t<Func, const Err&&>>;
        static_assert(
            detail::is_std_expected<Gp>::value, "The result of f(WTFMove(error())) must be a specialization of std::expected");
        static_assert(std::is_same_v<typename Gp::value_type, Tp>,
            "The result of f(WTFMove(error())) must have the same value_type as this expected");
        if (has_value())
            return Gp(std::in_place, WTFMove(m_storage.m_union.m_val));
        return std::invoke(std::forward<Func>(f), WTFMove(error()));
    }

    template <class Func>
        requires std::is_constructible_v<Err, Err&>
    constexpr auto transform(Func&& f) &
    { // NOLINT
        using Up = std::remove_cv_t<std::invoke_result_t<Func, Tp&>>;
        if (!has_value())
            return expected<Up, Err>(unexpect, error());
        if constexpr (!std::is_void_v<Up>)
            return expected<Up, Err>(detail::expected_construct_in_place_from_invoke_tag { }, std::forward<Func>(f), m_storage.m_union.m_val);
        else {
            std::invoke(std::forward<Func>(f), m_storage.m_union.m_val);
            return expected<Up, Err>();
        }
    }

    template <class Func>
        requires std::is_constructible_v<Err, const Err&>
    constexpr auto transform(Func&& f) const&
    { // NOLINT
        using Up = std::remove_cv_t<std::invoke_result_t<Func, const Tp&>>;
        if (!has_value())
            return expected<Up, Err>(unexpect, error());
        if constexpr (!std::is_void_v<Up>)
            return expected<Up, Err>(detail::expected_construct_in_place_from_invoke_tag { }, std::forward<Func>(f), m_storage.m_union.m_val);
        else {
            std::invoke(std::forward<Func>(f), m_storage.m_union.m_val);
            return expected<Up, Err>();
        }
    }

    template <class Func>
        requires std::is_constructible_v<Err, Err&&>
    constexpr auto transform(Func&& f) && // NOLINT
    { // NOLINT
        using Up = std::remove_cv_t<std::invoke_result_t<Func, Tp&&>>;
        if (!has_value())
            return expected<Up, Err>(unexpect, WTFMove(error()));
        if constexpr (!std::is_void_v<Up>)
            return expected<Up, Err>(detail::expected_construct_in_place_from_invoke_tag { }, std::forward<Func>(f), WTFMove(m_storage.m_union.m_val));
        else {
            std::invoke(std::forward<Func>(f), WTFMove(m_storage.m_union.m_val));
            return expected<Up, Err>();
        }
    }

    template <class Func>
        requires std::is_constructible_v<Err, const Err&&>
    constexpr auto transform(Func&& f) const&&
    { // NOLINT
        using Up = std::remove_cv_t<std::invoke_result_t<Func, const Tp&&>>;
        if (!has_value())
            return expected<Up, Err>(unexpect, WTFMove(error()));
        if constexpr (!std::is_void_v<Up>)
            return expected<Up, Err>(detail::expected_construct_in_place_from_invoke_tag { }, std::forward<Func>(f), WTFMove(m_storage.m_union.m_val));
        else {
            std::invoke(std::forward<Func>(f), WTFMove(m_storage.m_union.m_val));
            return expected<Up, Err>();
        }
    }

    template <class Func>
        requires std::is_constructible_v<Tp, Tp&>
    constexpr auto transform_error(Func&& f) &
    { // NOLINT
        using Gp = std::remove_cv_t<std::invoke_result_t<Func, Err&>>;
        static_assert(detail::valid_std_unexpected<Gp>::value,
            "The result of f(error()) must be a valid template argument for unexpected");
        if (has_value())
            return expected<Tp, Gp>(std::in_place, m_storage.m_union.m_val);
        return expected<Tp, Gp>(detail::expected_construct_unexpected_from_invoke_tag { }, std::forward<Func>(f), error());
    }

    template <class Func>
        requires std::is_constructible_v<Tp, const Tp&>
    constexpr auto transform_error(Func&& f) const&
    { // NOLINT
        using Gp = std::remove_cv_t<std::invoke_result_t<Func, const Err&>>;
        static_assert(detail::valid_std_unexpected<Gp>::value,
            "The result of f(error()) must be a valid template argument for unexpected");
        if (has_value())
            return expected<Tp, Gp>(std::in_place, m_storage.m_union.m_val);
        return expected<Tp, Gp>(detail::expected_construct_unexpected_from_invoke_tag { }, std::forward<Func>(f), error());
    }

    template <class Func>
        requires std::is_constructible_v<Tp, Tp&&>
    constexpr auto transform_error(Func&& f) && // NOLINT
    { // NOLINT
        using Gp = std::remove_cv_t<std::invoke_result_t<Func, Err&&>>;
        static_assert(detail::valid_std_unexpected<Gp>::value,
            "The result of f(WTFMove(error())) must be a valid template argument for unexpected");
        if (has_value())
            return expected<Tp, Gp>(std::in_place, WTFMove(m_storage.m_union.m_val));
        return expected<Tp, Gp>(detail::expected_construct_unexpected_from_invoke_tag { }, std::forward<Func>(f), WTFMove(error()));
    }

    template <class Func>
        requires std::is_constructible_v<Tp, const Tp&&>
    constexpr auto transform_error(Func&& f) const&&
    { // NOLINT
        using Gp = std::remove_cv_t<std::invoke_result_t<Func, const Err&&>>;
        static_assert(detail::valid_std_unexpected<Gp>::value,
            "The result of f(WTFMove(error())) must be a valid template argument for unexpected");
        if (has_value())
            return expected<Tp, Gp>(std::in_place, WTFMove(m_storage.m_union.m_val));
        return expected<Tp, Gp>(detail::expected_construct_unexpected_from_invoke_tag { }, std::forward<Func>(f), WTFMove(error()));
    }

    // [expected.object.eq], equality operators
    template <class T2, class E2>
        requires(!std::is_void_v<T2>)
    friend constexpr bool operator==(const expected& x, const expected<T2, E2>& y)
    {
        if (x.m_storage.m_hasVal != y.m_storage.m_hasVal)
            return false;
        if (x.m_storage.m_hasVal)
            return x.m_storage.m_union.m_val == y.m_storage.m_union.m_val;
        return x.m_storage.m_union.m_unex == y.m_storage.m_union.m_unex;
    }

    template <class T2>
    friend constexpr bool operator==(const expected& x, const T2& v)
    {
        return x.m_storage.m_hasVal && static_cast<bool>(x.m_storage.m_union.m_val == v);
    }

    template <class E2>
    friend constexpr bool operator==(const expected& x, const unexpected<E2>& e)
    {
        return !x.m_storage.m_hasVal && static_cast<bool>(x.m_storage.m_union.m_unex == e.error());
    }

private:
    detail::Storage<
        Tp,
        Err,
        (std::is_trivially_copy_constructible_v<Tp> && std::is_trivially_copy_constructible_v<Err>) ? detail::IsTriviallyCopyConstructible::Yes : detail::IsTriviallyCopyConstructible::No,
        (std::is_trivially_move_constructible_v<Tp> && std::is_trivially_move_constructible_v<Err>) ? detail::IsTriviallyMoveConstructible::Yes : detail::IsTriviallyMoveConstructible::No,
        (std::is_trivially_destructible_v<Tp> && std::is_trivially_destructible_v<Err>) ? detail::IsTriviallyDestructible::Yes : detail::IsTriviallyDestructible::No
    > m_storage;
};

template <class Tp, class Err>
    requires std::is_void_v<Tp>
class expected<Tp, Err> {
    WTF_MAKE_FAST_ALLOCATED;
    static_assert(detail::valid_std_unexpected<Err>::value,
        "[expected.void.general] A program that instantiates expected<T, E> with a E that is not a "
        "valid argument for unexpected<E> is ill-formed"); // NOLINT

    template <class, class>
    friend class expected;

    template <class Up, class OtherErr, class OtherErrQual>
    using CanConvert =
        std::conjunction<
            std::is_void<Up>,
            std::is_constructible<Err, OtherErrQual>,
            std::negation<std::is_constructible<unexpected<Err>, expected<Up, OtherErr>&>>,
            std::negation<std::is_constructible<unexpected<Err>, expected<Up, OtherErr>>>,
            std::negation<std::is_constructible<unexpected<Err>, const expected<Up, OtherErr>&>>,
            std::negation<std::is_constructible<unexpected<Err>, const expected<Up, OtherErr>>>>;

public:
    using value_type      = Tp;
    using error_type      = Err;
    using unexpected_type = unexpected<Err>;

    template <class Up>
    using rebind = expected<Up, error_type>;

    // [expected.void.ctor], constructors
    constexpr expected()
        : m_storage(std::in_place) { }

    constexpr expected(const expected&) = delete;

    constexpr expected(const expected&)
        requires(std::is_copy_constructible_v<Err>)
    = default;

    constexpr expected(expected&&)
        requires(std::is_move_constructible_v<Err>)
    = default;

    template <class Up, class OtherErr>
        requires CanConvert<Up, OtherErr, const OtherErr&>::value
    constexpr explicit(!std::is_convertible_v<const OtherErr&, Err>)
    expected(const expected<Up, OtherErr>& other)
        : m_storage(other.m_storage) { }

    template <class Up, class OtherErr>
        requires CanConvert<Up, OtherErr, OtherErr>::value
    constexpr explicit(!std::is_convertible_v<OtherErr, Err>)
    expected(expected<Up, OtherErr>&& other)
        : m_storage(other.m_storage) { }

    template <class OtherErr>
        requires std::is_constructible_v<Err, const OtherErr&>
    constexpr explicit(!std::is_convertible_v<const OtherErr&, Err>)
    expected(const unexpected<OtherErr>& unex)
        : m_storage(unexpect, unex.error()) { }

    template <class OtherErr>
        requires std::is_constructible_v<Err, OtherErr>
    constexpr explicit(!std::is_convertible_v<OtherErr, Err>)
    expected(unexpected<OtherErr>&& unex)
        : m_storage(unexpect, WTFMove(unex.error())) { }

    constexpr explicit expected(std::in_place_t tag)
        : m_storage(tag) { }

    template <class... Args>
        requires std::is_constructible_v<Err, Args...>
    constexpr explicit expected(unexpect_t, Args&&... args)
        : m_storage(unexpect, std::forward<Args>(args)...) { }

    template <class Up, class... Args>
        requires std::is_constructible_v< Err, std::initializer_list<Up>&, Args... >
    constexpr explicit expected(unexpect_t, std::initializer_list<Up> il, Args&&... args)
        : m_storage(unexpect, il, std::forward<Args>(args)...) { }

private:
    template <class Func>
    constexpr explicit expected(detail::expected_construct_in_place_from_invoke_tag, Func&& f)
        : m_storage(std::in_place)
    {
        std::invoke(std::forward<Func>(f));
    }

    template <class Func, class... Args>
    constexpr explicit expected(
        detail::expected_construct_unexpected_from_invoke_tag tag, Func&& f, Args&&... args)
        : m_storage(tag, std::forward<Func>(f), std::forward<Args>(args)...) { }

public:
    // [expected.void.assign], assignment

    constexpr expected& operator=(const expected&) = delete;

    constexpr expected& operator=(const expected& rhs)
        requires(std::is_copy_assignable_v<Err> && std::is_copy_constructible_v<Err>)
    {
        if (m_storage.m_hasVal) {
            if (!rhs.m_storage.m_hasVal) {
                std::construct_at(std::addressof(m_storage.m_union.m_unex), rhs.m_storage.m_union.m_unex);
                m_storage.m_hasVal = false;
            }
        } else {
            if (rhs.m_storage.m_hasVal) {
                std::destroy_at(std::addressof(m_storage.m_union.m_unex));
                m_storage.m_hasVal = true;
            } else
                m_storage.m_union.m_unex = rhs.m_storage.m_union.m_unex;
        }
        return *this;
    }

    constexpr expected& operator=(expected&&) = delete;

    constexpr expected& operator=(expected&& rhs)
        requires(std::is_move_assignable_v<Err> && std::is_move_constructible_v<Err>)
    {
        if (m_storage.m_hasVal) {
            if (!rhs.m_storage.m_hasVal) {
                std::construct_at(std::addressof(m_storage.m_union.m_unex), WTFMove(rhs.m_storage.m_union.m_unex));
                m_storage.m_hasVal = false;
            }
        } else {
            if (rhs.m_storage.m_hasVal) {
                std::destroy_at(std::addressof(m_storage.m_union.m_unex));
                m_storage.m_hasVal = true;
            } else
                m_storage.m_union.m_unex = WTFMove(rhs.m_storage.m_union.m_unex);
        }
        return *this;
    }

    template <class OtherErr>
        requires(std::is_constructible_v<Err, const OtherErr&> && std::is_assignable_v<Err&, const OtherErr&>)
    constexpr expected& operator=(const unexpected<OtherErr>& un)
    {
        if (m_storage.m_hasVal) {
            std::construct_at(std::addressof(m_storage.m_union.m_unex), un.error());
            m_storage.m_hasVal = false;
        } else
            m_storage.m_union.m_unex = un.error();
        return *this;
    }

    template <class OtherErr>
        requires(std::is_constructible_v<Err, OtherErr> && std::is_assignable_v<Err&, OtherErr>)
    constexpr expected& operator=(unexpected<OtherErr>&& un)
    {
        if (m_storage.m_hasVal) {
            std::construct_at(std::addressof(m_storage.m_union.m_unex), WTFMove(un.error()));
            m_storage.m_hasVal = false;
        } else
            m_storage.m_union.m_unex = WTFMove(un.error());
        return *this;
    }

    constexpr void emplace()
    {
        if (!m_storage.m_hasVal) {
            std::destroy_at(std::addressof(m_storage.m_union.m_unex));
            m_storage.m_hasVal = true;
        }
    }

    // [expected.void.swap], swap
    constexpr void swap(expected& rhs)
        requires(std::is_swappable_v<Err> && std::is_move_constructible_v<Err>)
    {
        auto swapValUnexImpl = [&](expected& withVal, expected& withErr) {
        std::construct_at(std::addressof(withVal.m_storage.m_union.m_unex), WTFMove(withErr.m_storage.m_union.m_unex));
        std::destroy_at(std::addressof(withErr.m_storage.m_union.m_unex));
        withVal.m_storage.m_hasVal = false;
        withErr.m_storage.m_hasVal = true;
        };

        if (m_storage.m_hasVal) {
            if (!rhs.m_storage.m_hasVal)
                swapValUnexImpl(*this, rhs);
        } else {
            if (rhs.m_storage.m_hasVal)
                swapValUnexImpl(rhs, *this);
            else {
                using std::swap; // NOLINT
                swap(m_storage.m_union.m_unex, rhs.m_storage.m_union.m_unex);
            }
        }
    }

    friend constexpr void swap(expected& x, expected& y)
        requires(std::is_swappable_v<Err> && std::is_move_constructible_v<Err>)
    {
        x.swap(y);
    }

    // [expected.void.obs], observers
    constexpr explicit operator bool() const { return m_storage.m_hasVal; }

    constexpr bool has_value() const { return m_storage.m_hasVal; }

    constexpr void operator*() const
    {
        RELEASE_ASSERT(m_storage.m_hasVal, "expected::operator* requires the expected to contain a value");
    }

    constexpr void value() const&
    { // NOLINT
        RELEASE_ASSERT(m_storage.m_hasVal);
    }

    constexpr void value() && // NOLINT
    { // NOLINT
        RELEASE_ASSERT(m_storage.m_hasVal);
    }

    constexpr const Err& error() const&
    { // NOLINT
        RELEASE_ASSERT(!m_storage.m_hasVal);
        return m_storage.m_union.m_unex;
    }

    constexpr Err& error() &
    { // NOLINT
        RELEASE_ASSERT(!m_storage.m_hasVal);
        return m_storage.m_union.m_unex;
    }

    constexpr const Err&& error() const&&
    { // NOLINT
        RELEASE_ASSERT(!m_storage.m_hasVal);
        return WTFMove(m_storage.m_union.m_unex);
    }

    constexpr Err&& error() && // NOLINT
    { // NOLINT
        RELEASE_ASSERT(!m_storage.m_hasVal);
        return WTFMove(m_storage.m_union.m_unex);
    }

    template <class Up = Err>
    constexpr Err error_or(Up&& err) const&
    { // NOLINT
        static_assert(std::is_copy_constructible_v<Err>, "error_type has to be copy constructible");
        static_assert(std::is_convertible_v<Up, Err>, "argument has to be convertible to error_type");
        if (has_value())
            return std::forward<Up>(err);
        return error();
    }

    template <class Up = Err>
    constexpr Err error_or(Up&& err) && // NOLINT
    { // NOLINT
        static_assert(std::is_move_constructible_v<Err>, "error_type has to be move constructible");
        static_assert(std::is_convertible_v<Up, Err>, "argument has to be convertible to error_type");
        if (has_value())
            return std::forward<Up>(err);
        return WTFMove(error());
    }

    // [expected.void.monadic], monadic
    template <class Func>
        requires std::is_constructible_v<Err, Err&>
    constexpr auto and_then(Func&& f) &
    { // NOLINT
        using Up = std::remove_cvref_t<std::invoke_result_t<Func>>;
        static_assert(detail::is_std_expected<Up>::value, "The result of f() must be a specialization of std::expected");
        static_assert(
            std::is_same_v<typename Up::error_type, Err>, "The result of f() must have the same error_type as this expected");
        if (has_value())
            return std::invoke(std::forward<Func>(f));
        return Up(unexpect, error());
    }

    template <class Func>
        requires std::is_constructible_v<Err, const Err&>
    constexpr auto and_then(Func&& f) const&
    { // NOLINT
        using Up = std::remove_cvref_t<std::invoke_result_t<Func>>;
        static_assert(detail::is_std_expected<Up>::value, "The result of f() must be a specialization of std::expected");
        static_assert(
            std::is_same_v<typename Up::error_type, Err>, "The result of f() must have the same error_type as this expected");
        if (has_value())
            return std::invoke(std::forward<Func>(f));
        return Up(unexpect, error());
    }

    template <class Func>
        requires std::is_constructible_v<Err, Err&&>
    constexpr auto and_then(Func&& f) && // NOLINT
    { // NOLINT
        using Up = std::remove_cvref_t<std::invoke_result_t<Func>>;
        static_assert(detail::is_std_expected<Up>::value, "The result of f() must be a specialization of std::expected");
        static_assert(
            std::is_same_v<typename Up::error_type, Err>, "The result of f() must have the same error_type as this expected");
        if (has_value())
            return std::invoke(std::forward<Func>(f));
        return Up(unexpect, WTFMove(error()));
    }

    template <class Func>
        requires std::is_constructible_v<Err, const Err&&>
    constexpr auto and_then(Func&& f) const&&
    { // NOLINT
        using Up = std::remove_cvref_t<std::invoke_result_t<Func>>;
        static_assert(detail::is_std_expected<Up>::value, "The result of f() must be a specialization of std::expected");
        static_assert(
            std::is_same_v<typename Up::error_type, Err>, "The result of f() must have the same error_type as this expected");
        if (has_value())
            return std::invoke(std::forward<Func>(f));
        return Up(unexpect, WTFMove(error()));
    }

    template <class Func>
    constexpr auto or_else(Func&& f) &
    { // NOLINT
        using Gp = std::remove_cvref_t<std::invoke_result_t<Func, Err&>>;
        static_assert(detail::is_std_expected<Gp>::value, "The result of f(error()) must be a specialization of std::expected");
        static_assert(std::is_same_v<typename Gp::value_type, Tp>,
            "The result of f(error()) must have the same value_type as this expected");
        if (has_value())
            return Gp();
        return std::invoke(std::forward<Func>(f), error());
    }

    template <class Func>
    constexpr auto or_else(Func&& f) const&
    { // NOLINT
        using Gp = std::remove_cvref_t<std::invoke_result_t<Func, const Err&>>;
        static_assert(detail::is_std_expected<Gp>::value, "The result of f(error()) must be a specialization of std::expected");
        static_assert(std::is_same_v<typename Gp::value_type, Tp>,
            "The result of f(error()) must have the same value_type as this expected");
        if (has_value())
            return Gp();
        return std::invoke(std::forward<Func>(f), error());
    }

    template <class Func>
    constexpr auto or_else(Func&& f) && // NOLINT
    { // NOLINT
        using Gp = std::remove_cvref_t<std::invoke_result_t<Func, Err&&>>;
        static_assert(detail::is_std_expected<Gp>::value,
            "The result of f(WTFMove(error())) must be a specialization of std::expected");
        static_assert(std::is_same_v<typename Gp::value_type, Tp>,
            "The result of f(WTFMove(error())) must have the same value_type as this expected");
        if (has_value())
            return Gp();
        return std::invoke(std::forward<Func>(f), WTFMove(error()));
    }

    template <class Func>
    constexpr auto or_else(Func&& f) const&&
    { // NOLINT
        using Gp = std::remove_cvref_t<std::invoke_result_t<Func, const Err&&>>;
        static_assert(detail::is_std_expected<Gp>::value,
            "The result of f(WTFMove(error())) must be a specialization of std::expected");
        static_assert(std::is_same_v<typename Gp::value_type, Tp>,
            "The result of f(WTFMove(error())) must have the same value_type as this expected");
        if (has_value())
            return Gp();
        return std::invoke(std::forward<Func>(f), WTFMove(error()));
    }

    template <class Func>
        requires std::is_constructible_v<Err, Err&>
    constexpr auto transform(Func&& f) &
    { // NOLINT
        using Up = std::remove_cv_t<std::invoke_result_t<Func>>;
        if (!has_value())
            return expected<Up, Err>(unexpect, error());
        if constexpr (!std::is_void_v<Up>)
            return expected<Up, Err>(detail::expected_construct_in_place_from_invoke_tag { }, std::forward<Func>(f));
        else {
            std::invoke(std::forward<Func>(f));
            return expected<Up, Err>();
        }
    }

    template <class Func>
        requires std::is_constructible_v<Err, const Err&>
    constexpr auto transform(Func&& f) const&
    { // NOLINT
        using Up = std::remove_cv_t<std::invoke_result_t<Func>>;
        if (!has_value())
            return expected<Up, Err>(unexpect, error());
        if constexpr (!std::is_void_v<Up>)
            return expected<Up, Err>(detail::expected_construct_in_place_from_invoke_tag { }, std::forward<Func>(f));
        else {
            std::invoke(std::forward<Func>(f));
            return expected<Up, Err>();
        }
    }

    template <class Func>
        requires std::is_constructible_v<Err, Err&&>
    constexpr auto transform(Func&& f) && // NOLINT
    { // NOLINT
        using Up = std::remove_cv_t<std::invoke_result_t<Func>>;
        if (!has_value())
            return expected<Up, Err>(unexpect, WTFMove(error()));
        if constexpr (!std::is_void_v<Up>)
            return expected<Up, Err>(detail::expected_construct_in_place_from_invoke_tag { }, std::forward<Func>(f));
        else {
            std::invoke(std::forward<Func>(f));
            return expected<Up, Err>();
        }
    }

    template <class Func>
        requires std::is_constructible_v<Err, const Err&&>
    constexpr auto transform(Func&& f) const&&
    { // NOLINT
        using Up = std::remove_cv_t<std::invoke_result_t<Func>>;
        if (!has_value())
            return expected<Up, Err>(unexpect, WTFMove(error()));
        if constexpr (!std::is_void_v<Up>)
            return expected<Up, Err>(detail::expected_construct_in_place_from_invoke_tag { }, std::forward<Func>(f));
        else {
            std::invoke(std::forward<Func>(f));
            return expected<Up, Err>();
        }
    }

    template <class Func>
    constexpr auto transform_error(Func&& f) &
    { // NOLINT
        using Gp = std::remove_cv_t<std::invoke_result_t<Func, Err&>>;
        static_assert(detail::valid_std_unexpected<Gp>::value,
            "The result of f(error()) must be a valid template argument for unexpected");
        if (has_value())
            return expected<Tp, Gp>();
        return expected<Tp, Gp>(detail::expected_construct_unexpected_from_invoke_tag { }, std::forward<Func>(f), error());
    }

    template <class Func>
    constexpr auto transform_error(Func&& f) const&
    { // NOLINT
        using Gp = std::remove_cv_t<std::invoke_result_t<Func, const Err&>>;
        static_assert(detail::valid_std_unexpected<Gp>::value,
            "The result of f(error()) must be a valid template argument for unexpected");
        if (has_value())
            return expected<Tp, Gp>();
        return expected<Tp, Gp>(detail::expected_construct_unexpected_from_invoke_tag { }, std::forward<Func>(f), error());
    }

    template <class Func>
    constexpr auto transform_error(Func&& f) && // NOLINT
    { // NOLINT
        using Gp = std::remove_cv_t<std::invoke_result_t<Func, Err&&>>;
        static_assert(detail::valid_std_unexpected<Gp>::value,
            "The result of f(WTFMove(error())) must be a valid template argument for unexpected");
        if (has_value())
            return expected<Tp, Gp>();
        return expected<Tp, Gp>(detail::expected_construct_unexpected_from_invoke_tag { }, std::forward<Func>(f), WTFMove(error()));
    }

    template <class Func>
    constexpr auto transform_error(Func&& f) const&&
    { // NOLINT
        using Gp = std::remove_cv_t<std::invoke_result_t<Func, const Err&&>>;
        static_assert(detail::valid_std_unexpected<Gp>::value,
            "The result of f(WTFMove(error())) must be a valid template argument for unexpected");
        if (has_value())
            return expected<Tp, Gp>();
        return expected<Tp, Gp>(
            detail::expected_construct_unexpected_from_invoke_tag { }, std::forward<Func>(f), WTFMove(error()));
    }

    // [expected.void.eq], equality operators
    template <class T2, class E2>
        requires std::is_void_v<T2>
    friend constexpr bool operator==(const expected& x, const expected<T2, E2>& y)
    {
        if (x.m_storage.m_hasVal != y.m_storage.m_hasVal)
            return false;
        return x.m_storage.m_hasVal || static_cast<bool>(x.m_storage.m_union.m_unex == y.m_storage.m_union.m_unex);
    }

    template <class E2>
    friend constexpr bool operator==(const expected& x, const unexpected<E2>& y)
    {
        return !x.m_storage.m_hasVal && static_cast<bool>(x.m_storage.m_union.m_unex == y.error());
    }

private:
    struct VoidPlaceholder { };

    detail::Storage<
        VoidPlaceholder,
        Err,
        std::is_trivially_copy_constructible_v<Err> ? detail::IsTriviallyCopyConstructible::Yes : detail::IsTriviallyCopyConstructible::No,
        std::is_trivially_move_constructible_v<Err> ? detail::IsTriviallyMoveConstructible::Yes : detail::IsTriviallyMoveConstructible::No,
        std::is_trivially_destructible_v<Err> ? detail::IsTriviallyDestructible::Yes : detail::IsTriviallyDestructible::No> m_storage;
};

} // namespace WTF::detail_from_clang_libstdcpp

inline constexpr auto& unexpect = WTF::detail_from_clang_libstdcpp::unexpect;
template<class T, class E> using Expected = WTF::detail_from_clang_libstdcpp::expected<T, E>;

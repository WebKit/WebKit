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

#include "config.h"

#include "RefLogger.h"

#include <memory>
#include <string>

#include <wtf/Expected.h>
#include <wtf/Unexpected.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Ref.h>

namespace std {

template<typename T0, typename T1> std::ostream& operator<<(std::ostream& os, const std::pair<T0, T1>& p)
{
    return os << '(' << p.first << ", " << p.second << ')';
}

namespace experimental {
inline namespace fundamentals_v3 {

template<class E> std::ostream& operator<<(std::ostream& os, const Unexpected<E>& u)
{
    return os << u.value();
}

template<class T, class E> std::ostream& operator<<(std::ostream& os, const Expected<T, E>& e)
{
    if (e.has_value())
        return os << e.value();
    return os << e.error();
}

template<class E> std::ostream& operator<<(std::ostream& os, const Expected<void, E>& e)
{
    if (e.has_value())
        return os << "";
    return os << e.error();
}

}}} // namespace std::experimental::fundamentals_v3


namespace TestWebKitAPI {

constexpr const char* oops = "oops";
constexpr const char* foof = "foof";
constexpr const char* otherError = "err";
constexpr const void* otherErrorV = static_cast<const void*>("errV");

TEST(WTF_Expected, Unexpected)
{
    {
        auto u = Unexpected<int>(42);
        EXPECT_EQ(u.error(), 42);
        constexpr auto c = makeUnexpected(42);
        EXPECT_EQ(c.error(), 42);
        EXPECT_EQ(u, c);
        EXPECT_FALSE(u != c);
    }
    {
        auto c = makeUnexpected(oops);
        EXPECT_EQ(c.error(), oops);
    }
    {
        auto s = makeUnexpected(std::string(oops));
        EXPECT_EQ(s.error(), oops);
    }
    {
        constexpr auto s0 = makeUnexpected(oops);
        constexpr auto s1(s0);
        EXPECT_EQ(s0, s1);
    }
}

struct foo {
    int v;
    foo(int v)
        : v(v)
    { }
    ~foo() { }
    bool operator==(const foo& y) const { return v == y.v; }
    friend std::ostream& operator<<(std::ostream&, const foo&);
};
std::ostream& operator<<(std::ostream& os, const foo& f) { return os << f.v; }

TEST(WTF_Expected, expected)
{
    typedef Expected<int, const char*> E;
    typedef Expected<int, const void*> EV;
    typedef Expected<foo, const char*> FooChar;
    typedef Expected<foo, std::string> FooString;
    {
        auto e = E();
        EXPECT_TRUE(e.has_value());
        EXPECT_EQ(e.value(), 0);
        EXPECT_EQ(e.value_or(3.14), 0);
        EXPECT_EQ(e.error_or(otherError), otherError);
    }
    {
        constexpr E e;
        EXPECT_TRUE(e.has_value());
        EXPECT_EQ(e.value(), 0);
        EXPECT_EQ(e.value_or(3.14), 0);
        EXPECT_EQ(e.error_or(otherError), otherError);
    }
    {
        auto e = E(42);
        EXPECT_TRUE(e.has_value());
        EXPECT_EQ(e.value(), 42);
        EXPECT_EQ(e.value_or(3.14), 42);
        EXPECT_EQ(e.error_or(otherError), otherError);
        const auto e2(e);
        EXPECT_TRUE(e2.has_value());
        EXPECT_EQ(e2.value(), 42);
        EXPECT_EQ(e2.value_or(3.14), 42);
        EXPECT_EQ(e2.error_or(otherError), otherError);
        E e3;
        e3 = e2;
        EXPECT_TRUE(e3.has_value());
        EXPECT_EQ(e3.value(), 42);
        EXPECT_EQ(e3.value_or(3.14), 42);
        EXPECT_EQ(e3.error_or(otherError), otherError);
        const E e4 = e2;
        EXPECT_TRUE(e4.has_value());
        EXPECT_EQ(e4.value(), 42);
        EXPECT_EQ(e4.value_or(3.14), 42);
        EXPECT_EQ(e4.error_or(otherError), otherError);
    }
    {
        constexpr E c(42);
        EXPECT_TRUE(c.has_value());
        EXPECT_EQ(c.value(), 42);
        EXPECT_EQ(c.value_or(3.14), 42);
        EXPECT_EQ(c.error_or(otherError), otherError);
        constexpr const auto c2(c);
        EXPECT_TRUE(c2.has_value());
        EXPECT_EQ(c2.value(), 42);
        EXPECT_EQ(c2.value_or(3.14), 42);
        EXPECT_EQ(c2.error_or(otherError), otherError);
    }
    {
        auto u = E(makeUnexpected(oops));
        EXPECT_FALSE(u.has_value());
        EXPECT_EQ(u.error(), oops);
        EXPECT_EQ(u.value_or(3.14), 3);
        EXPECT_EQ(u.error_or(otherError), oops);
    }
    {
        auto u = E(unexpect, oops);
        EXPECT_FALSE(u.has_value());
        EXPECT_EQ(u.error(), oops);
        EXPECT_EQ(u.value_or(3.14), 3);
        EXPECT_EQ(u.error_or(otherError), oops);
    }
    {
        auto uv = EV(makeUnexpected(oops));
        EXPECT_FALSE(uv.has_value());
        EXPECT_EQ(uv.error(), oops);
        EXPECT_EQ(uv.value_or(3.14), 3);
        EXPECT_EQ(uv.error_or(otherErrorV), oops);
    }
    {
        auto uv = EV(unexpect, oops);
        EXPECT_FALSE(uv.has_value());
        EXPECT_EQ(uv.error(), oops);
        EXPECT_EQ(uv.value_or(3.14), 3);
        EXPECT_EQ(uv.error_or(otherErrorV), oops);
    }
    {
        E e = makeUnexpected(oops);
        EXPECT_FALSE(e.has_value());
        EXPECT_EQ(e.error(), oops);
        EXPECT_EQ(e.value_or(3.14), 3);
        EXPECT_EQ(e.error_or(otherError), oops);
    }
    {
        auto e = FooChar(42);
        EXPECT_EQ(e->v, 42);
        EXPECT_EQ((*e).v, 42);
    }
    {
        auto e0 = E(42);
        auto e1 = E(1024);
        swap(e0, e1);
        EXPECT_TRUE(e0.has_value());
        EXPECT_EQ(e0.value(), 1024);
        EXPECT_EQ(e0.value_or(3.14), 1024);
        EXPECT_EQ(e0.error_or(otherError), otherError);
        EXPECT_TRUE(e1.has_value());
        EXPECT_EQ(e1.value(), 42);
        EXPECT_EQ(e1.value_or(3.14), 42);
        EXPECT_EQ(e1.error_or(otherError), otherError);
    }
    {
        auto e0 = E(makeUnexpected(oops));
        auto e1 = E(makeUnexpected(foof));
        swap(e0, e1);
        EXPECT_FALSE(e0.has_value());
        EXPECT_EQ(e0.error(), foof);
        EXPECT_EQ(e0.value_or(3.14), 3);
        EXPECT_EQ(e0.error_or(otherError), foof);
        EXPECT_FALSE(e1.has_value());
        EXPECT_EQ(e1.error(), oops);
        EXPECT_EQ(e1.value_or(3.14), 3);
        EXPECT_EQ(e1.error_or(otherError), oops);
    }
    {
        auto e0 = E(42);
        auto e1 = E(makeUnexpected(foof));
        swap(e0, e1);
        EXPECT_FALSE(e0.has_value());
        EXPECT_EQ(e0.error(), foof);
        EXPECT_EQ(e0.value_or(3.14), 3);
        EXPECT_EQ(e0.error_or(otherError), foof);
        EXPECT_TRUE(e1.has_value());
        EXPECT_EQ(e1.value(), 42);
        EXPECT_EQ(e1.value_or(3.14), 42);
        EXPECT_EQ(e1.error_or(otherError), otherError);
    }
    {
        FooChar c(foo(42));
        EXPECT_EQ(c->v, 42);
        EXPECT_EQ((*c).v, 42);
    }
    {
        FooString s(foo(42));
        EXPECT_EQ(s->v, 42);
        EXPECT_EQ((*s).v, 42);
        const char* message = "very long failure string, for very bad failure cases";
        FooString e0(makeUnexpected<std::string>(message));
        FooString e1(makeUnexpected<std::string>(message));
        FooString e2(makeUnexpected<std::string>(std::string()));
        EXPECT_EQ(e0.error(), std::string(message));
        EXPECT_EQ(e0.error_or(std::string(otherError)), std::string(message));
        EXPECT_EQ(e0, e1);
        EXPECT_NE(e0, e2);
        FooString* e4 = new FooString(makeUnexpected<std::string>(message));
        FooString* e5 = new FooString(*e4);
        EXPECT_EQ(e0, *e4);
        delete e4;
        EXPECT_EQ(e0, *e5);
        delete e5;
    }
}

TEST(WTF_Expected, void)
{
    typedef Expected<void, const char*> E;
    typedef Expected<void, const void*> EV;
    typedef Expected<void, std::string> String;
    {
        auto e = E();
        EXPECT_TRUE(e.has_value());
        EXPECT_EQ(e.error_or(otherError), otherError);
        const auto e2(e);
        EXPECT_TRUE(e2.has_value());
        EXPECT_EQ(e, e2);
        E e3;
        e3 = e2;
        EXPECT_TRUE(e3.has_value());
        EXPECT_EQ(e, e3);
    }
    {
        constexpr E c;
        EXPECT_TRUE(c.has_value());
        constexpr const auto c2(c);
        EXPECT_TRUE(c2.has_value());
        EXPECT_EQ(c, c2);
    }
    {
        auto u = E(makeUnexpected(oops));
        EXPECT_FALSE(u.has_value());
        EXPECT_EQ(u.error(), oops);
        EXPECT_EQ(u.error_or(otherError), oops);
    }
    {
        auto uv = EV(makeUnexpected(oops));
        EXPECT_FALSE(uv.has_value());
        EXPECT_EQ(uv.error(), oops);
        EXPECT_EQ(uv.error_or(otherError), oops);
    }
    {
        E e = makeUnexpected(oops);
        EXPECT_FALSE(e.has_value());
        EXPECT_EQ(e.error(), oops);
        EXPECT_EQ(e.error_or(otherError), oops);
    }
    {
        auto e0 = E();
        auto e1 = E();
        swap(e0, e1);
        EXPECT_EQ(e0, e1);
    }
    {
        auto e0 = E(makeUnexpected(oops));
        auto e1 = E(makeUnexpected(foof));
        swap(e0, e1);
        EXPECT_FALSE(e0.has_value());
        EXPECT_EQ(e0.error(), foof);
        EXPECT_EQ(e0.error_or(otherError), foof);
        EXPECT_FALSE(e0.has_value());
        EXPECT_EQ(e1.error(), oops);
        EXPECT_EQ(e1.error_or(otherError), oops);
    }
    {
        auto e0 = E();
        auto e1 = E(makeUnexpected(foof));
        swap(e0, e1);
        EXPECT_FALSE(e0.has_value());
        EXPECT_EQ(e0.error(), foof);
        EXPECT_EQ(e0.error_or(otherError), foof);
        EXPECT_TRUE(e1.has_value());
        EXPECT_EQ(e1.error_or(otherError), otherError);
    }
    {
        const char* message = "very long failure string, for very bad failure cases";
        String e0(makeUnexpected<std::string>(message));
        String e1(makeUnexpected<std::string>(message));
        String e2(makeUnexpected<std::string>(std::string()));
        EXPECT_EQ(e0.error(), std::string(message));
        EXPECT_EQ(e0.error_or(otherError), std::string(message));
        EXPECT_EQ(e0, e1);
        EXPECT_NE(e0, e2);
        String* e4 = new String(makeUnexpected<std::string>(message));
        String* e5 = new String(*e4);
        EXPECT_EQ(e0, *e4);
        delete e4;
        EXPECT_EQ(e0, *e5);
        delete e5;
    }
    {
        typedef Expected<std::pair<int, int>, std::string> Et;
        EXPECT_EQ(Et(std::in_place, 1, 2), Et(std::in_place, 1, 2));
    }
}

TEST(WTF_Expected, monadic)
{
    using EIC = Expected<int, const char*>;
    using EII = Expected<int, int>;
    using EFC = Expected<foo, const char*>;
    using EVC = Expected<void, const char*>;
    using EVI = Expected<void, int>;
    {
        auto e = EIC(42);
        EXPECT_TRUE(e.has_value());
        EXPECT_EQ(e.and_then([](auto&& v) {
            static_assert(std::is_same_v<decltype(v), int&>);
            EXPECT_EQ(v, 42);
            return EFC(v + 1);
        }), EFC(43));
        EXPECT_EQ(e.and_then([](auto&& v) {
            static_assert(std::is_same_v<decltype(v), int&>);
            EXPECT_EQ(v, 42);
            return EFC(makeUnexpected(foof));
        }), EFC(makeUnexpected(foof)));
        EXPECT_EQ(e.or_else([](auto&& e) {
            static_assert(std::is_same_v<decltype(e), const char*&>);
            ASSERT_NOT_REACHED();
            return EII(makeUnexpected(123));
        }), EII(42));
        EXPECT_EQ(e.transform([](auto&& v) {
            static_assert(std::is_same_v<decltype(v), int&>);
            EXPECT_EQ(v, 42);
            return foo(v + 1);
        }), EFC(43));
        EXPECT_EQ(e.transform_error([](auto&& e) {
            static_assert(std::is_same_v<decltype(e), const char*&>);
            ASSERT_NOT_REACHED();
            return 123;
        }), EII(42));
    }
    {
        auto e = EIC(makeUnexpected(oops));
        EXPECT_FALSE(e.has_value());
        EXPECT_EQ(e.and_then([](auto&& v) {
            static_assert(std::is_same_v<decltype(v), int&>);
            ASSERT_NOT_REACHED();
            return EFC(42);
        }), EFC(makeUnexpected(oops)));
        EXPECT_EQ(e.or_else([](auto&& e) {
            static_assert(std::is_same_v<decltype(e), const char*&>);
            EXPECT_EQ(e, oops);
            return EII(42);
        }), EII(42));
        EXPECT_EQ(e.or_else([](auto&& e) {
            static_assert(std::is_same_v<decltype(e), const char*&>);
            EXPECT_EQ(e, oops);
            return EII(makeUnexpected(43));
        }), EII(makeUnexpected(43)));
        EXPECT_EQ(e.transform([](auto&& v) {
            static_assert(std::is_same_v<decltype(v), int&>);
            ASSERT_NOT_REACHED();
            return foo(v + 1);
        }), EFC(makeUnexpected(oops)));
        EXPECT_EQ(e.transform_error([](auto&& e) {
            static_assert(std::is_same_v<decltype(e), const char*&>);
            EXPECT_EQ(e, oops);
            return 123;
        }), EII(makeUnexpected(123)));
    }
    {
        auto e = EVC();
        EXPECT_TRUE(e.has_value());
        EXPECT_EQ(e.and_then([]() {
            return EFC(42);
        }), EFC(42));
        EXPECT_EQ(e.and_then([]() {
            return EFC(makeUnexpected(foof));
        }), EFC(makeUnexpected(foof)));
        EXPECT_EQ(e.or_else([](auto&& e) {
            static_assert(std::is_same_v<decltype(e), const char*&>);
            ASSERT_NOT_REACHED();
            return EVI(makeUnexpected(123));
        }), EVI());
        EXPECT_EQ(e.transform([]() { return foo(42); }), EFC(42));
        EXPECT_EQ(e.transform_error([](auto&& e) {
            static_assert(std::is_same_v<decltype(e), const char*&>);
            ASSERT_NOT_REACHED();
            return 123;
        }), EVI());
    }
    {
        auto e = EVC(makeUnexpected(oops));
        EXPECT_FALSE(e.has_value());
        EXPECT_EQ(e.and_then([]() {
            ASSERT_NOT_REACHED();
            return EIC(42);
        }), EIC(makeUnexpected(oops)));
        EXPECT_EQ(e.or_else([](auto&& e) {
            static_assert(std::is_same_v<decltype(e), const char*&>);
            EXPECT_EQ(e, oops);
            return EVI();
        }), EVI());
        EXPECT_EQ(e.or_else([](auto&& e) {
            static_assert(std::is_same_v<decltype(e), const char*&>);
            EXPECT_EQ(e, oops);
            return EVI(makeUnexpected(43));
        }), EVI(makeUnexpected(43)));
        EXPECT_EQ(e.transform([]() {
            ASSERT_NOT_REACHED();
            return foo(42);
        }), EFC(makeUnexpected(oops)));
        EXPECT_EQ(e.transform_error([](auto&& e) {
            static_assert(std::is_same_v<decltype(e), const char*&>);
            EXPECT_EQ(e, oops);
            return 123;
        }), EVI(makeUnexpected(123)));
    }
}

static_assert(std::is_copy_constructible_v<int>);
static_assert(std::is_copy_constructible_v<Expected<int, int>>);
static_assert(std::is_copy_constructible_v<Expected<void, int>>);
static_assert(std::is_trivially_copy_constructible_v<int>);
static_assert(std::is_trivially_copy_constructible_v<Expected<int, int>>);
static_assert(std::is_trivially_copy_constructible_v<Expected<void, int>>);

static_assert(std::is_copy_assignable_v<int>);
static_assert(std::is_copy_assignable_v<Expected<int, int>>);
static_assert(std::is_copy_assignable_v<Expected<void, int>>);

static_assert(std::is_move_constructible_v<int>);
static_assert(std::is_move_constructible_v<Expected<int, int>>);
static_assert(std::is_move_constructible_v<Expected<void, int>>);
static_assert(std::is_trivially_move_constructible_v<int>);
static_assert(std::is_trivially_move_constructible_v<Expected<int, int>>);
static_assert(std::is_trivially_move_constructible_v<Expected<void, int>>);

static_assert(std::is_move_assignable_v<int>);
static_assert(std::is_move_assignable_v<Expected<int, int>>);
static_assert(std::is_move_assignable_v<Expected<void, int>>);

struct Copyable {
    Copyable(const Copyable&) = default;
    Copyable& operator=(const Copyable&) = default;
    Copyable(Copyable&&) = default;
    Copyable& operator=(Copyable&&) = default;
};

static_assert(std::is_copy_constructible_v<Copyable>);
static_assert(std::is_copy_constructible_v<Expected<Copyable, int>>);
static_assert(std::is_copy_constructible_v<Expected<int, Copyable>>);
static_assert(std::is_copy_constructible_v<Expected<Copyable, Copyable>>);
static_assert(std::is_copy_constructible_v<Expected<void, Copyable>>);
static_assert(std::is_trivially_copy_constructible_v<Copyable>);
static_assert(std::is_trivially_copy_constructible_v<Expected<Copyable, int>>);
static_assert(std::is_trivially_copy_constructible_v<Expected<int, Copyable>>);
static_assert(std::is_trivially_copy_constructible_v<Expected<Copyable, Copyable>>);
static_assert(std::is_trivially_copy_constructible_v<Expected<void, Copyable>>);

static_assert(std::is_copy_assignable_v<Copyable>);
static_assert(std::is_copy_assignable_v<Expected<Copyable, int>>);
static_assert(std::is_copy_assignable_v<Expected<int, Copyable>>);
static_assert(std::is_copy_assignable_v<Expected<Copyable, Copyable>>);
static_assert(std::is_copy_assignable_v<Expected<void, Copyable>>);

static_assert(std::is_move_constructible_v<Copyable>);
static_assert(std::is_move_constructible_v<Expected<Copyable, int>>);
static_assert(std::is_move_constructible_v<Expected<int, Copyable>>);
static_assert(std::is_move_constructible_v<Expected<Copyable, Copyable>>);
static_assert(std::is_move_constructible_v<Expected<void, Copyable>>);
static_assert(std::is_trivially_move_constructible_v<Copyable>);
static_assert(std::is_trivially_move_constructible_v<Expected<Copyable, int>>);
static_assert(std::is_trivially_move_constructible_v<Expected<int, Copyable>>);
static_assert(std::is_trivially_move_constructible_v<Expected<Copyable, Copyable>>);
static_assert(std::is_trivially_move_constructible_v<Expected<void, Copyable>>);

static_assert(std::is_move_assignable_v<Copyable>);
static_assert(std::is_move_assignable_v<Expected<Copyable, int>>);
static_assert(std::is_move_assignable_v<Expected<int, Copyable>>);
static_assert(std::is_move_assignable_v<Expected<Copyable, Copyable>>);
static_assert(std::is_move_assignable_v<Expected<void, Copyable>>);

struct CopyableNonTrivially {
    CopyableNonTrivially(const CopyableNonTrivially&) { }
    CopyableNonTrivially& operator=(const CopyableNonTrivially&) { return *this; }
    CopyableNonTrivially(CopyableNonTrivially&&) { }
    CopyableNonTrivially& operator=(CopyableNonTrivially&&) { return *this; }
};

static_assert(std::is_copy_constructible_v<CopyableNonTrivially>);
static_assert(std::is_copy_constructible_v<Expected<CopyableNonTrivially, int>>);
static_assert(std::is_copy_constructible_v<Expected<int, CopyableNonTrivially>>);
static_assert(std::is_copy_constructible_v<Expected<CopyableNonTrivially, CopyableNonTrivially>>);
static_assert(std::is_copy_constructible_v<Expected<void, CopyableNonTrivially>>);
static_assert(!std::is_trivially_copy_constructible_v<CopyableNonTrivially>);
static_assert(!std::is_trivially_copy_constructible_v<Expected<CopyableNonTrivially, int>>);
static_assert(!std::is_trivially_copy_constructible_v<Expected<int, CopyableNonTrivially>>);
static_assert(!std::is_trivially_copy_constructible_v<Expected<CopyableNonTrivially, CopyableNonTrivially>>);
static_assert(!std::is_trivially_copy_constructible_v<Expected<void, CopyableNonTrivially>>);
static_assert(!std::is_trivially_copy_constructible_v<Expected<CopyableNonTrivially, Copyable>>);
static_assert(!std::is_trivially_copy_constructible_v<Expected<Copyable, CopyableNonTrivially>>);

static_assert(std::is_copy_assignable_v<CopyableNonTrivially>);
static_assert(std::is_copy_assignable_v<Expected<CopyableNonTrivially, int>>);
static_assert(std::is_copy_assignable_v<Expected<int, CopyableNonTrivially>>);
static_assert(std::is_copy_assignable_v<Expected<CopyableNonTrivially, CopyableNonTrivially>>);
static_assert(std::is_copy_assignable_v<Expected<void, CopyableNonTrivially>>);

static_assert(std::is_move_constructible_v<CopyableNonTrivially>);
static_assert(std::is_move_constructible_v<Expected<CopyableNonTrivially, int>>);
static_assert(std::is_move_constructible_v<Expected<int, CopyableNonTrivially>>);
static_assert(std::is_move_constructible_v<Expected<CopyableNonTrivially, CopyableNonTrivially>>);
static_assert(std::is_move_constructible_v<Expected<void, CopyableNonTrivially>>);
static_assert(!std::is_trivially_move_constructible_v<CopyableNonTrivially>);
static_assert(!std::is_trivially_move_constructible_v<Expected<CopyableNonTrivially, int>>);
static_assert(!std::is_trivially_move_constructible_v<Expected<int, CopyableNonTrivially>>);
static_assert(!std::is_trivially_move_constructible_v<Expected<CopyableNonTrivially, CopyableNonTrivially>>);
static_assert(!std::is_trivially_move_constructible_v<Expected<void, CopyableNonTrivially>>);

static_assert(std::is_move_assignable_v<CopyableNonTrivially>);
static_assert(std::is_move_assignable_v<Expected<CopyableNonTrivially, int>>);
static_assert(std::is_move_assignable_v<Expected<int, CopyableNonTrivially>>);
static_assert(std::is_move_assignable_v<Expected<CopyableNonTrivially, CopyableNonTrivially>>);
static_assert(std::is_move_assignable_v<Expected<void, CopyableNonTrivially>>);

template<typename T>
struct OnlyMovable {
    OnlyMovable(T&& t)
        : value(std::forward<T>(t)) { }
    OnlyMovable(const OnlyMovable&) = delete;
    OnlyMovable& operator=(const OnlyMovable&) = delete;
    OnlyMovable(OnlyMovable&&) = default;
    OnlyMovable& operator=(OnlyMovable&&) = default;
    bool operator==(const OnlyMovable<T>& other) const { return value == other.value; }
    T value;
};

static_assert(!std::is_copy_constructible_v<OnlyMovable<int>>);
static_assert(!std::is_copy_constructible_v<Expected<OnlyMovable<int>, int>>);
static_assert(!std::is_copy_constructible_v<Expected<int, OnlyMovable<int>>>);
static_assert(!std::is_copy_constructible_v<Expected<OnlyMovable<int>, OnlyMovable<int>>>);
static_assert(!std::is_copy_constructible_v<Expected<void, OnlyMovable<int>>>);

static_assert(!std::is_copy_assignable_v<OnlyMovable<int>>);
static_assert(!std::is_copy_assignable_v<Expected<OnlyMovable<int>, int>>);
static_assert(!std::is_copy_assignable_v<Expected<int, OnlyMovable<int>>>);
static_assert(!std::is_copy_assignable_v<Expected<OnlyMovable<int>, OnlyMovable<int>>>);
static_assert(!std::is_copy_assignable_v<Expected<void, OnlyMovable<int>>>);

static_assert(std::is_move_constructible_v<OnlyMovable<int>>);
static_assert(std::is_move_constructible_v<Expected<OnlyMovable<int>, int>>);
static_assert(std::is_move_constructible_v<Expected<int, OnlyMovable<int>>>);
static_assert(std::is_move_constructible_v<Expected<OnlyMovable<int>, OnlyMovable<int>>>);
static_assert(std::is_move_constructible_v<Expected<void, OnlyMovable<int>>>);
static_assert(std::is_trivially_move_constructible_v<OnlyMovable<int>>);
static_assert(std::is_trivially_move_constructible_v<Expected<OnlyMovable<int>, int>>);
static_assert(std::is_trivially_move_constructible_v<Expected<int, OnlyMovable<int>>>);
static_assert(std::is_trivially_move_constructible_v<Expected<OnlyMovable<int>, OnlyMovable<int>>>);
static_assert(std::is_trivially_move_constructible_v<Expected<void, OnlyMovable<int>>>);

static_assert(std::is_move_assignable_v<OnlyMovable<int>>);
static_assert(std::is_move_assignable_v<Expected<OnlyMovable<int>, int>>);
static_assert(std::is_move_assignable_v<Expected<int, OnlyMovable<int>>>);
static_assert(std::is_move_assignable_v<Expected<OnlyMovable<int>, OnlyMovable<int>>>);
static_assert(std::is_move_assignable_v<Expected<void, OnlyMovable<int>>>);

template<typename T>
struct OnlyMovableNonTrivially {
    OnlyMovableNonTrivially(T&& t)
        : value(std::forward<T>(t)) { }
    OnlyMovableNonTrivially(const OnlyMovableNonTrivially&) = delete;
    OnlyMovableNonTrivially& operator=(const OnlyMovableNonTrivially&) = delete;
    OnlyMovableNonTrivially(OnlyMovableNonTrivially&& other)
        : value(WTFMove(other.value))
    { }
    OnlyMovableNonTrivially& operator=(OnlyMovableNonTrivially&& other) { value = WTFMove(other.value); return *this; }
    bool operator==(const OnlyMovableNonTrivially<T>& other) const { return value == other.value; }
    T value;
};

static_assert(!std::is_copy_constructible_v<OnlyMovableNonTrivially<int>>);
static_assert(!std::is_copy_constructible_v<Expected<OnlyMovableNonTrivially<int>, int>>);
static_assert(!std::is_copy_constructible_v<Expected<int, OnlyMovableNonTrivially<int>>>);
static_assert(!std::is_copy_constructible_v<Expected<OnlyMovableNonTrivially<int>, OnlyMovableNonTrivially<int>>>);
static_assert(!std::is_copy_constructible_v<Expected<void, OnlyMovableNonTrivially<int>>>);

static_assert(!std::is_copy_assignable_v<OnlyMovableNonTrivially<int>>);
static_assert(!std::is_copy_assignable_v<Expected<OnlyMovableNonTrivially<int>, int>>);
static_assert(!std::is_copy_assignable_v<Expected<int, OnlyMovableNonTrivially<int>>>);
static_assert(!std::is_copy_assignable_v<Expected<OnlyMovableNonTrivially<int>, OnlyMovableNonTrivially<int>>>);
static_assert(!std::is_copy_assignable_v<Expected<void, OnlyMovableNonTrivially<int>>>);

static_assert(std::is_move_constructible_v<OnlyMovableNonTrivially<int>>);
static_assert(std::is_move_constructible_v<Expected<OnlyMovableNonTrivially<int>, int>>);
static_assert(std::is_move_constructible_v<Expected<int, OnlyMovableNonTrivially<int>>>);
static_assert(std::is_move_constructible_v<Expected<OnlyMovableNonTrivially<int>, OnlyMovableNonTrivially<int>>>);
static_assert(std::is_move_constructible_v<Expected<void, OnlyMovableNonTrivially<int>>>);
static_assert(!std::is_trivially_move_constructible_v<OnlyMovableNonTrivially<int>>);
static_assert(!std::is_trivially_move_constructible_v<Expected<OnlyMovableNonTrivially<int>, int>>);
static_assert(!std::is_trivially_move_constructible_v<Expected<int, OnlyMovableNonTrivially<int>>>);
static_assert(!std::is_trivially_move_constructible_v<Expected<OnlyMovableNonTrivially<int>, OnlyMovableNonTrivially<int>>>);
static_assert(!std::is_trivially_move_constructible_v<Expected<void, OnlyMovableNonTrivially<int>>>);
static_assert(!std::is_trivially_move_constructible_v<Expected<OnlyMovableNonTrivially<int>, OnlyMovable<int>>>);
static_assert(!std::is_trivially_move_constructible_v<Expected<OnlyMovable<int>, OnlyMovableNonTrivially<int>>>);

static_assert(std::is_move_assignable_v<OnlyMovableNonTrivially<int>>);
static_assert(std::is_move_assignable_v<Expected<OnlyMovableNonTrivially<int>, int>>);
static_assert(std::is_move_assignable_v<Expected<int, OnlyMovableNonTrivially<int>>>);
static_assert(std::is_move_assignable_v<Expected<OnlyMovableNonTrivially<int>, OnlyMovableNonTrivially<int>>>);
static_assert(std::is_move_assignable_v<Expected<void, OnlyMovableNonTrivially<int>>>);

TEST(WTF_Expected, comparison)
{
    typedef Expected<int, const char*> Ex;
    typedef Expected<int, int> Er;

    // Two Expected, no errors.
    EXPECT_EQ(Ex(42), Ex(42));
    EXPECT_NE(Ex(42), Ex(1024));

    EXPECT_FALSE(Ex(42) == Ex(1024));
    EXPECT_FALSE(Ex(42) != Ex(42));

    // Two Expected, half errors.
    EXPECT_FALSE(Ex(42) == Ex(makeUnexpected(oops)));
    EXPECT_NE(Ex(42), Ex(makeUnexpected(oops)));

    EXPECT_FALSE(Ex(makeUnexpected(oops)) == Ex(42));
    EXPECT_NE(Ex(makeUnexpected(oops)), Ex(42));

    // Two Expected, all errors.
    EXPECT_EQ(Er(42), Er(42));
    EXPECT_NE(Er(42), Er(1024));

    EXPECT_FALSE(Er(42) == Er(1024));
    EXPECT_FALSE(Er(42) != Er(42));

    // One Expected, one value.
    EXPECT_EQ(Ex(42), 42);
    EXPECT_NE(Ex(42), 0);

    EXPECT_FALSE(Ex(42) == 0);
    EXPECT_FALSE(Ex(42) != 42);

    EXPECT_EQ(42, Ex(42));
    EXPECT_NE(42, Ex(1024));

    EXPECT_FALSE(42 == Ex(1024));
    EXPECT_FALSE(42 != Ex(42));

    // One Expected, one unexpected.
    EXPECT_FALSE(Ex(42) == makeUnexpected(oops));
    EXPECT_NE(Ex(42), makeUnexpected(oops));

    EXPECT_FALSE(makeUnexpected(oops) == Ex(42));
    EXPECT_NE(makeUnexpected(oops), Ex(42));

    {
        OnlyMovable<int> a { 5 };
        OnlyMovable<int> b { 6 };
        Unexpected<OnlyMovable<double>> c { makeUnexpected(OnlyMovable<double> { 5.0 }) };
        Expected<OnlyMovable<int>, OnlyMovable<double>> d { OnlyMovable<int> { 5 } };
        Expected<OnlyMovable<int>, OnlyMovable<double>> e { makeUnexpected(OnlyMovable<double> { 5.0 }) };

        EXPECT_TRUE(a != e);
        EXPECT_TRUE(e != a);
        EXPECT_FALSE(a == e);
        EXPECT_FALSE(e == a);

        EXPECT_TRUE(b != e);
        EXPECT_TRUE(e != b);
        EXPECT_FALSE(b == e);
        EXPECT_FALSE(e == b);

        EXPECT_TRUE(c != d);
        EXPECT_TRUE(d != c);
        EXPECT_FALSE(c == d);
        EXPECT_FALSE(d == c);

        EXPECT_TRUE(c == e);
        EXPECT_TRUE(e == c);
        EXPECT_FALSE(c != e);
        EXPECT_FALSE(e != c);
    }

    {
        OnlyMovableNonTrivially<int> a { 5 };
        OnlyMovableNonTrivially<int> b { 6 };
        Unexpected<OnlyMovableNonTrivially<double>> c { makeUnexpected(OnlyMovableNonTrivially<double> { 5.0 }) };
        Expected<OnlyMovableNonTrivially<int>, OnlyMovableNonTrivially<double>> d { OnlyMovableNonTrivially<int> { 5 } };
        Expected<OnlyMovableNonTrivially<int>, OnlyMovableNonTrivially<double>> e { makeUnexpected(OnlyMovableNonTrivially<double> { 5.0 }) };

        EXPECT_TRUE(a != e);
        EXPECT_TRUE(e != a);
        EXPECT_FALSE(a == e);
        EXPECT_FALSE(e == a);

        EXPECT_TRUE(b != e);
        EXPECT_TRUE(e != b);
        EXPECT_FALSE(b == e);
        EXPECT_FALSE(e == b);

        EXPECT_TRUE(c != d);
        EXPECT_TRUE(d != c);
        EXPECT_FALSE(c == d);
        EXPECT_FALSE(d == c);

        EXPECT_TRUE(c == e);
        EXPECT_TRUE(e == c);
        EXPECT_FALSE(c != e);
        EXPECT_FALSE(e != c);
    }
}

static_assert(std::is_trivially_destructible_v<int>);
static_assert(std::is_trivially_destructible_v<Expected<int, int>>);
static_assert(std::is_trivially_destructible_v<Expected<void, int>>);

struct TrivialDtor {
    ~TrivialDtor() = default;
};

static_assert(std::is_trivially_destructible_v<TrivialDtor>);
static_assert(std::is_trivially_destructible_v<Expected<TrivialDtor, int>>);
static_assert(std::is_trivially_destructible_v<Expected<int, TrivialDtor>>);
static_assert(std::is_trivially_destructible_v<Expected<TrivialDtor, TrivialDtor>>);
static_assert(std::is_trivially_destructible_v<Expected<void, TrivialDtor>>);

struct NonTrivialDtor {
    ~NonTrivialDtor() { ++count; }
    static int count;
};
int NonTrivialDtor::count = 0;

static_assert(!std::is_trivially_destructible_v<NonTrivialDtor>);
static_assert(!std::is_trivially_destructible_v<Expected<NonTrivialDtor, int>>);
static_assert(!std::is_trivially_destructible_v<Expected<int, NonTrivialDtor>>);
static_assert(!std::is_trivially_destructible_v<Expected<NonTrivialDtor, NonTrivialDtor>>);
static_assert(!std::is_trivially_destructible_v<Expected<void, NonTrivialDtor>>);

TEST(WTF_Expected, destructors)
{
    typedef Expected<NonTrivialDtor, const char*> NT;
    typedef Expected<const char*, NonTrivialDtor> TN;
    typedef Expected<NonTrivialDtor, NonTrivialDtor> NN;
    typedef Expected<void, NonTrivialDtor> VN;
    EXPECT_EQ(NonTrivialDtor::count, 0);
    { NT nt; }
    EXPECT_EQ(NonTrivialDtor::count, 1);
    { NT nt = makeUnexpected(oops); }
    EXPECT_EQ(NonTrivialDtor::count, 1);
    { TN tn; }
    EXPECT_EQ(NonTrivialDtor::count, 1);
    { TN tn = makeUnexpected(NonTrivialDtor()); }
    EXPECT_EQ(NonTrivialDtor::count, 4);
    { NN nn; }
    EXPECT_EQ(NonTrivialDtor::count, 5);
    { NN nn = makeUnexpected(NonTrivialDtor()); }
    EXPECT_EQ(NonTrivialDtor::count, 8);
    { VN vn; }
    EXPECT_EQ(NonTrivialDtor::count, 8);
    { VN vn = makeUnexpected(NonTrivialDtor()); }
    EXPECT_EQ(NonTrivialDtor::count, 11);
}

static int snowflakes = 0;
static int melted = 0;
struct snowflake {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    static void reset() { snowflakes = melted = 0; }
    snowflake() { ++snowflakes; }
    ~snowflake() { ++melted; }
};

TEST(WTF_Expected, unique_ptr)
{
    // Unique snowflakes cannot be copied.
    {
        auto s = makeUnexpected(makeUnique<snowflake>());
        EXPECT_EQ(snowflakes, 1);
        EXPECT_EQ(melted, 0);
    }
    EXPECT_EQ(snowflakes, 1);
    EXPECT_EQ(melted, 1);
    snowflake::reset();

    {
        auto s = makeUnexpected(makeUnique<snowflake>());
        Unexpected<std::unique_ptr<snowflake>> c(WTFMove(s));
        EXPECT_EQ(snowflakes, 1);
        EXPECT_EQ(melted, 0);
    }
    EXPECT_EQ(snowflakes, 1);
    EXPECT_EQ(melted, 1);
    snowflake::reset();

    auto plow = [] (std::unique_ptr<snowflake>&& s)
    {
        {
            std::unique_ptr<snowflake> moved = WTFMove(s);
            EXPECT_EQ(snowflakes, 1);
            EXPECT_EQ(melted, 0);
        }
        EXPECT_EQ(snowflakes, 1);
        EXPECT_EQ(melted, 1);
    };

    {
        Expected<std::unique_ptr<snowflake>, int> s(makeUnique<snowflake>());
        plow(WTFMove(s).value());
    }
    EXPECT_EQ(snowflakes, 1);
    EXPECT_EQ(melted, 1);
    snowflake::reset();

    {
        Expected<int, std::unique_ptr<snowflake>> s(makeUnexpected(makeUnique<snowflake>()));
        plow(WTFMove(s).error());
    }
    EXPECT_EQ(snowflakes, 1);
    EXPECT_EQ(melted, 1);
    snowflake::reset();

    {
        Expected<void, std::unique_ptr<snowflake>> s(makeUnexpected(makeUnique<snowflake>()));
        plow(WTFMove(s).error());
    }
    EXPECT_EQ(snowflakes, 1);
    EXPECT_EQ(melted, 1);
    snowflake::reset();
}

TEST(WTF_Expected, Ref)
{
    {
        RefLogger a("a");
        Expected<Ref<RefLogger>, int> expected = Ref<RefLogger>(a);
        EXPECT_TRUE(expected.has_value());
        EXPECT_EQ(&a, expected.value().ptr());
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefLogger a("a");
        Expected<Ref<RefLogger>, int> expected = Expected<Ref<RefLogger>, int>(Ref<RefLogger>(a));
        EXPECT_TRUE(expected.has_value());
        EXPECT_EQ(&a, expected.value().ptr());
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefLogger a("a");
        Expected<int, Ref<RefLogger>> expected = makeUnexpected(Ref<RefLogger>(a));
        EXPECT_FALSE(expected.has_value());
        EXPECT_EQ(&a, expected.error().ptr());
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefLogger a("a");
        Expected<void, Ref<RefLogger>> expected = makeUnexpected(Ref<RefLogger>(a));
        EXPECT_FALSE(expected.has_value());
        EXPECT_EQ(&a, expected.error().ptr());
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

class NeedsStdAddress {
public:
    NeedsStdAddress(NeedsStdAddress&& other)
        : m_ptr(WTFMove(other.m_ptr)) { }
    NeedsStdAddress(int& other)
        : m_ptr(&other) { }
    int* operator&() { ASSERT_NOT_REACHED(); return nullptr; }
private:
    std::unique_ptr<int> m_ptr;
};

TEST(WTF_Expected, Address)
{
    NeedsStdAddress a(*new int(3));
    Expected<NeedsStdAddress, float> b(WTFMove(a));
    Expected<NeedsStdAddress, float> c(WTFMove(b));
    (void)c;
}

} // namespace TestWebkitAPI

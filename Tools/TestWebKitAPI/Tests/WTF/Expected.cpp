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

TEST(WTF_Expected, Unexpected)
{
    {
        auto u = Unexpected<int>(42);
        EXPECT_EQ(u.value(), 42);
        constexpr auto c = makeUnexpected(42);
        EXPECT_EQ(c.value(), 42);
        EXPECT_EQ(u, c);
        EXPECT_FALSE(u != c);
    }
    {
        auto c = makeUnexpected(oops);
        EXPECT_EQ(c.value(), oops);
    }
    {
        auto s = makeUnexpected(std::string(oops));
        EXPECT_EQ(s.value(), oops);
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
    }
    {
        constexpr E e;
        EXPECT_TRUE(e.has_value());
        EXPECT_EQ(e.value(), 0);
        EXPECT_EQ(e.value_or(3.14), 0);
    }
    {
        auto e = E(42);
        EXPECT_TRUE(e.has_value());
        EXPECT_EQ(e.value(), 42);
        EXPECT_EQ(e.value_or(3.14), 42);
        const auto e2(e);
        EXPECT_TRUE(e2.has_value());
        EXPECT_EQ(e2.value(), 42);
        EXPECT_EQ(e2.value_or(3.14), 42);
        E e3;
        e3 = e2;
        EXPECT_TRUE(e3.has_value());
        EXPECT_EQ(e3.value(), 42);
        EXPECT_EQ(e3.value_or(3.14), 42);
        const E e4 = e2;
        EXPECT_TRUE(e4.has_value());
        EXPECT_EQ(e4.value(), 42);
        EXPECT_EQ(e4.value_or(3.14), 42);
    }
    {
        constexpr E c(42);
        EXPECT_TRUE(c.has_value());
        EXPECT_EQ(c.value(), 42);
        EXPECT_EQ(c.value_or(3.14), 42);
        constexpr const auto c2(c);
        EXPECT_TRUE(c2.has_value());
        EXPECT_EQ(c2.value(), 42);
        EXPECT_EQ(c2.value_or(3.14), 42);
    }
    {
        auto u = E(makeUnexpected(oops));
        EXPECT_FALSE(u.has_value());
        EXPECT_EQ(u.error(), oops);
        EXPECT_EQ(u.value_or(3.14), 3);
    }
    {
        auto uv = EV(makeUnexpected(oops));
        EXPECT_FALSE(uv.has_value());
        EXPECT_EQ(uv.error(), oops);
        EXPECT_EQ(uv.value_or(3.14), 3);
    }
    {
        E e = makeUnexpected(oops);
        EXPECT_FALSE(e.has_value());
        EXPECT_EQ(e.error(), oops);
        EXPECT_EQ(e.value_or(3.14), 3);
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
        EXPECT_EQ(e0.value(), 1024);
        EXPECT_EQ(e1.value(), 42);
    }
    {
        auto e0 = E(makeUnexpected(oops));
        auto e1 = E(makeUnexpected(foof));
        swap(e0, e1);
        EXPECT_EQ(e0.error(), foof);
        EXPECT_EQ(e1.error(), oops);
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
    }
    {
        auto uv = EV(makeUnexpected(oops));
        EXPECT_FALSE(uv.has_value());
        EXPECT_EQ(uv.error(), oops);
    }
    {
        E e = makeUnexpected(oops);
        EXPECT_FALSE(e.has_value());
        EXPECT_EQ(e.error(), oops);
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
        EXPECT_EQ(e0.error(), foof);
        EXPECT_EQ(e1.error(), oops);
    }
    {
        const char* message = "very long failure string, for very bad failure cases";
        String e0(makeUnexpected<std::string>(message));
        String e1(makeUnexpected<std::string>(message));
        String e2(makeUnexpected<std::string>(std::string()));
        EXPECT_EQ(e0.error(), std::string(message));
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
}

struct NonTrivialDtor {
    ~NonTrivialDtor() { ++count; }
    static int count;
};
int NonTrivialDtor::count = 0;

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
    static void reset() { snowflakes = melted = 0; }
    snowflake() { ++snowflakes; }
    ~snowflake() { ++melted; }
};

TEST(WTF_Expected, unique_ptr)
{
    // Unique snowflakes cannot be copied.
    {
        auto s = makeUnexpected(std::make_unique<snowflake>());
        EXPECT_EQ(snowflakes, 1);
        EXPECT_EQ(melted, 0);
    }
    EXPECT_EQ(snowflakes, 1);
    EXPECT_EQ(melted, 1);
    snowflake::reset();

    {
        auto s = makeUnexpected(std::make_unique<snowflake>());
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
        Expected<std::unique_ptr<snowflake>, int> s(std::make_unique<snowflake>());
        plow(WTFMove(s).value());
    }
    EXPECT_EQ(snowflakes, 1);
    EXPECT_EQ(melted, 1);
    snowflake::reset();

    {
        Expected<int, std::unique_ptr<snowflake>> s(makeUnexpected(std::make_unique<snowflake>()));
        plow(WTFMove(s).error());
    }
    EXPECT_EQ(snowflakes, 1);
    EXPECT_EQ(melted, 1);
    snowflake::reset();

    {
        Expected<void, std::unique_ptr<snowflake>> s(makeUnexpected(std::make_unique<snowflake>()));
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

/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2018 Yusuke Suzuki <yusukesuzuki@slowstart.org>.
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

#include <wtf/Markable.h>

namespace TestWebKitAPI {

TEST(WTF_Markable, Disengaged)
{
    {
        Markable<int, IntegralMarkableTraits<int, 42>> optional;

        EXPECT_FALSE(static_cast<bool>(optional));
    }

    {
        Markable<int, IntegralMarkableTraits<int, 42>> optional { WTF::nullopt };

        EXPECT_FALSE(static_cast<bool>(optional));
    }
}

TEST(WTF_Markable, Engaged)
{
    {
        Markable<int, IntegralMarkableTraits<int, 42>> optional { 10 };

        EXPECT_TRUE(static_cast<bool>(optional));
        EXPECT_EQ(10, optional.value());

        optional = 41;
        EXPECT_TRUE(static_cast<bool>(optional));
        EXPECT_EQ(41, optional.value());

        optional = WTF::nullopt;
        EXPECT_FALSE(static_cast<bool>(optional));

        optional = 42;
        EXPECT_FALSE(static_cast<bool>(optional));
    }
    {
        Markable<int, IntegralMarkableTraits<int, 42>> optional { 42 };

        EXPECT_FALSE(static_cast<bool>(optional));

        optional = 41;

        EXPECT_TRUE(static_cast<bool>(optional));
        EXPECT_EQ(41, optional.value());
    }
}

TEST(WTF_Markable, Destructor)
{
    static bool didCallDestructor = false;
    struct A {
        explicit A(int value)
            : m_value(value)
        { }

        ~A()
        {
            EXPECT_FALSE(didCallDestructor);
            didCallDestructor = true;
        }

        int m_value { 42 };
    };

    struct ATraits {
        static bool isEmptyValue(const A& value)
        {
            return value.m_value == 42;
        }
        static A emptyValue()
        {
            return A(42);
        }
    };

    didCallDestructor = false;
    {
        Markable<A, ATraits> optional { std::in_place, 20 };
        EXPECT_TRUE(static_cast<bool>(optional));
    }
    EXPECT_TRUE(didCallDestructor);

    didCallDestructor = false;
    {
        Markable<A, ATraits> optional { std::in_place, 42 };
        EXPECT_FALSE(static_cast<bool>(optional));
    }
    EXPECT_TRUE(didCallDestructor);
}

TEST(WTF_Markable, FromOptional)
{
    {
        Optional<int> from;
        EXPECT_FALSE(static_cast<bool>(from));
        Markable<int, IntegralMarkableTraits<int, 42>> optional = from;
        EXPECT_FALSE(static_cast<bool>(optional));
    }
    {
        Optional<int> from { 42 };
        EXPECT_TRUE(static_cast<bool>(from));
        Markable<int, IntegralMarkableTraits<int, 42>> optional = from;
        // We convert this to nullopt.
        EXPECT_FALSE(static_cast<bool>(optional));
    }
    {
        Optional<int> from { 43 };
        EXPECT_TRUE(static_cast<bool>(from));
        Markable<int, IntegralMarkableTraits<int, 42>> optional = from;
        EXPECT_TRUE(static_cast<bool>(optional));
        EXPECT_EQ(optional.value(), 43);
    }
    {
        Optional<int> from;
        EXPECT_FALSE(static_cast<bool>(from));
        Markable<int, IntegralMarkableTraits<int, 42>> optional { WTFMove(from) };
        EXPECT_FALSE(static_cast<bool>(optional));
    }
    {
        Optional<int> from { 42 };
        EXPECT_TRUE(static_cast<bool>(from));
        Markable<int, IntegralMarkableTraits<int, 42>> optional { WTFMove(from) };
        // We convert this to nullopt.
        EXPECT_FALSE(static_cast<bool>(optional));
    }
    {
        Optional<int> from { 43 };
        EXPECT_TRUE(static_cast<bool>(from));
        Markable<int, IntegralMarkableTraits<int, 42>> optional { WTFMove(from) };
        EXPECT_TRUE(static_cast<bool>(optional));
        EXPECT_EQ(optional.value(), 43);
    }
}

TEST(WTF_Markable, ToOptional)
{
    {
        Markable<int, IntegralMarkableTraits<int, 42>> optional;
        EXPECT_FALSE(static_cast<bool>(optional));
        Optional<int> to = optional;
        EXPECT_FALSE(static_cast<bool>(to));
    }
    {
        Markable<int, IntegralMarkableTraits<int, 42>> optional { 42 };
        EXPECT_FALSE(static_cast<bool>(optional));
        // We convert this to nullopt.
        Optional<int> to = optional;
        EXPECT_FALSE(static_cast<bool>(to));
    }
    {
        Markable<int, IntegralMarkableTraits<int, 42>> optional { 43 };
        EXPECT_TRUE(static_cast<bool>(optional));
        Optional<int> to = optional;
        EXPECT_TRUE(static_cast<bool>(to));
        EXPECT_EQ(to.value(), 43);
    }
    {
        Markable<int, IntegralMarkableTraits<int, 42>> optional;
        EXPECT_FALSE(static_cast<bool>(optional));
        Optional<int> to { WTFMove(optional) };
        EXPECT_FALSE(static_cast<bool>(to));
    }
    {
        Markable<int, IntegralMarkableTraits<int, 42>> optional { 42 };
        EXPECT_FALSE(static_cast<bool>(optional));
        // We convert this to nullopt.
        Optional<int> to { WTFMove(optional) };
        EXPECT_FALSE(static_cast<bool>(to));
    }
    {
        Markable<int, IntegralMarkableTraits<int, 42>> optional { 43 };
        EXPECT_TRUE(static_cast<bool>(optional));
        Optional<int> to { WTFMove(optional) };
        EXPECT_TRUE(static_cast<bool>(to));
        EXPECT_EQ(to.value(), 43);
    }
}

TEST(WTF_Markable, MoveOptional)
{
    class OnlyMovable {
    public:
        constexpr explicit OnlyMovable(int value)
            : m_value(value)
        {
        }

        int value() { return m_value; }

        OnlyMovable& operator=(OnlyMovable&& other)
        {
            m_value = other.m_value;
            other.m_value = 42;
            return *this;
        }

        OnlyMovable(OnlyMovable&& other)
            : m_value(other.m_value)
        {
            other.m_value = 42;
        }

        static OnlyMovable emptyValue()
        {
            return OnlyMovable(42);
        }

        static constexpr bool isEmptyValue(const OnlyMovable& value)
        {
            return value.m_value == 42;
        }

    private:
        OnlyMovable(const OnlyMovable&) = delete;
        OnlyMovable& operator=(const OnlyMovable&) = delete;

        int m_value;
    };
    {
        Optional<OnlyMovable> from { std::in_place, 20 };
        EXPECT_TRUE(static_cast<bool>(from));
        EXPECT_EQ(from.value().value(), 20);
        Markable<OnlyMovable, OnlyMovable> compact = WTFMove(from);
        EXPECT_TRUE(static_cast<bool>(compact));
        EXPECT_EQ(compact.value().value(), 20);
        Optional<OnlyMovable> to = WTFMove(compact);
        EXPECT_TRUE(static_cast<bool>(to));
        EXPECT_EQ(to.value().value(), 20);
    }
}

TEST(WTF_Markable, Equality)
{
    Markable<int, IntegralMarkableTraits<int, 42>> unengaged1;
    Markable<int, IntegralMarkableTraits<int, 42>> unengaged2;

    Markable<int, IntegralMarkableTraits<int, 42>> engaged1 { 1 };
    Markable<int, IntegralMarkableTraits<int, 42>> engaged2 { 2 };
    Markable<int, IntegralMarkableTraits<int, 42>> engagedx2 { 2 };

    EXPECT_TRUE(unengaged1 == unengaged2);
    EXPECT_FALSE(engaged1 == engaged2);
    EXPECT_FALSE(engaged1 == unengaged1);
    EXPECT_TRUE(engaged2 == engagedx2);

    EXPECT_FALSE(unengaged1 == 42);
    EXPECT_FALSE(engaged1 == 42);
    EXPECT_FALSE(42 == unengaged1);
    EXPECT_FALSE(42 == engaged1);

    EXPECT_TRUE(engaged1 == 1);
    EXPECT_FALSE(engaged1 == 2);
    EXPECT_TRUE(1 == engaged1);
    EXPECT_FALSE(2 == engaged1);

    EXPECT_FALSE(unengaged1 == 1);
    EXPECT_FALSE(1 == unengaged1);
}

TEST(WTF_Markable, Inequality)
{
    Markable<int, IntegralMarkableTraits<int, 42>> unengaged1;
    Markable<int, IntegralMarkableTraits<int, 42>> unengaged2;

    Markable<int, IntegralMarkableTraits<int, 42>> engaged1 { 1 };
    Markable<int, IntegralMarkableTraits<int, 42>> engaged2 { 2 };
    Markable<int, IntegralMarkableTraits<int, 42>> engagedx2 { 2 };

    EXPECT_FALSE(unengaged1 != unengaged2);
    EXPECT_TRUE(engaged1 != engaged2);
    EXPECT_TRUE(engaged1 != unengaged1);
    EXPECT_FALSE(engaged2 != engagedx2);

    EXPECT_TRUE(unengaged1 != 42);
    EXPECT_TRUE(engaged1 != 42);
    EXPECT_TRUE(42 != unengaged1);
    EXPECT_TRUE(42 != engaged1);

    EXPECT_FALSE(engaged1 != 1);
    EXPECT_TRUE(engaged1 != 2);
    EXPECT_FALSE(1 != engaged1);
    EXPECT_TRUE(2 != engaged1);

    EXPECT_TRUE(unengaged1 != 1);
    EXPECT_TRUE(1 != unengaged1);
}

} // namespace TestWebKitAPI

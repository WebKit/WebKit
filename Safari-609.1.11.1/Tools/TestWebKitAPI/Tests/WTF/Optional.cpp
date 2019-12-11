/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include <wtf/Optional.h>

namespace TestWebKitAPI {

TEST(WTF_Optional, Disengaged)
{
    {
        Optional<int> optional;

        EXPECT_FALSE(static_cast<bool>(optional));
    }

    {
        Optional<int> optional { WTF::nullopt };

        EXPECT_FALSE(static_cast<bool>(optional));
    }
}

TEST(WTF_Optional, Engaged)
{
    Optional<int> optional { 10 };

    EXPECT_TRUE(static_cast<bool>(optional));
    EXPECT_EQ(10, optional.value());
}

TEST(WTF_Optional, Destructor)
{
    static bool didCallDestructor = false;
    struct A {
        ~A()
        {
            EXPECT_FALSE(didCallDestructor);
            didCallDestructor = true;
        }
    };

    {
        Optional<A> optional { std::in_place };

        EXPECT_TRUE(static_cast<bool>(optional));
    }

    EXPECT_TRUE(didCallDestructor);
}

TEST(WTF_Optional, Callback)
{
    bool called = false;
    Optional<int> a;
    int result = valueOrCompute(a, [&] {
        called = true;
        return 300;
    });
    EXPECT_TRUE(called);
    EXPECT_EQ(result, 300);

    a = 250;
    called = false;
    result = valueOrCompute(a, [&] {
        called = true;
        return 300;
    });
    EXPECT_FALSE(called);
    EXPECT_EQ(result, 250);
}

TEST(WTF_Optional, Equality)
{
    Optional<int> unengaged1;
    Optional<int> unengaged2;

    Optional<int> engaged1 { 1 };
    Optional<int> engaged2 { 2 };
    Optional<int> engagedx2 { 2 };

    EXPECT_TRUE(unengaged1 == unengaged2);
    EXPECT_FALSE(engaged1 == engaged2);
    EXPECT_FALSE(engaged1 == unengaged1);
    EXPECT_TRUE(engaged2 == engagedx2);

    EXPECT_TRUE(unengaged1 == WTF::nullopt);
    EXPECT_FALSE(engaged1 == WTF::nullopt);
    EXPECT_TRUE(WTF::nullopt == unengaged1);
    EXPECT_FALSE(WTF::nullopt == engaged1);

    EXPECT_TRUE(engaged1 == 1);
    EXPECT_TRUE(1 == engaged1);
    EXPECT_FALSE(unengaged1 == 1);
    EXPECT_FALSE(1 == unengaged1);
}

TEST(WTF_Optional, Inequality)
{
    Optional<int> unengaged1;
    Optional<int> unengaged2;

    Optional<int> engaged1 { 1 };
    Optional<int> engaged2 { 2 };
    Optional<int> engagedx2 { 2 };

    EXPECT_FALSE(unengaged1 != unengaged2);
    EXPECT_TRUE(engaged1 != engaged2);
    EXPECT_TRUE(engaged1 != unengaged1);
    EXPECT_FALSE(engaged2 != engagedx2);

    EXPECT_FALSE(unengaged1 != WTF::nullopt);
    EXPECT_TRUE(engaged1 != WTF::nullopt);
    EXPECT_FALSE(WTF::nullopt != unengaged1);
    EXPECT_TRUE(WTF::nullopt != engaged1);

    EXPECT_FALSE(engaged1 != 1);
    EXPECT_TRUE(engaged1 != 2);
    EXPECT_FALSE(1 != engaged1);
    EXPECT_TRUE(2 != engaged1);

    EXPECT_TRUE(unengaged1 != 1);
    EXPECT_TRUE(1 != unengaged1);
}

TEST(WTF_Optional, MoveSemanticsConstructor)
{
    {
        Optional<int> unengaged1 { };
        EXPECT_FALSE(!!unengaged1);

        Optional<int> unengaged2(std::move(unengaged1));
        EXPECT_FALSE(!!unengaged1);
        EXPECT_FALSE(!!unengaged2);
    }

    {
        Optional<int> engagedToUnengaged { 2 };
        EXPECT_TRUE(!!engagedToUnengaged);
        EXPECT_EQ(2, engagedToUnengaged.value());

        Optional<int> engaged(std::move(engagedToUnengaged));
        EXPECT_FALSE(!!engagedToUnengaged);
        EXPECT_TRUE(!!engaged);
        EXPECT_EQ(2, engaged.value());
    }
}

TEST(WTF_Optional, MoveSemanticsAssignment)
{
    {
        Optional<int> unengaged1 { };
        EXPECT_FALSE(!!unengaged1);

        Optional<int> unengaged2;
        EXPECT_FALSE(!!unengaged2);

        unengaged2 = std::move(unengaged1);
        EXPECT_FALSE(!!unengaged1);
        EXPECT_FALSE(!!unengaged2);
    }

    {
        Optional<int> engagedToUnengaged { 3 };
        EXPECT_TRUE(!!engagedToUnengaged);
        EXPECT_EQ(3, engagedToUnengaged.value());

        engagedToUnengaged = std::move(WTF::nullopt);
        EXPECT_FALSE(!!engagedToUnengaged);
    }

    {
        Optional<int> engagedToUnengaged { 4 };
        EXPECT_TRUE(!!engagedToUnengaged);
        EXPECT_EQ(4, engagedToUnengaged.value());

        Optional<int> unengagedToEngaged;
        EXPECT_FALSE(!!unengagedToEngaged);

        unengagedToEngaged = std::move(engagedToUnengaged);
        EXPECT_FALSE(!!engagedToUnengaged);
        EXPECT_TRUE(!!unengagedToEngaged);
        EXPECT_EQ(4, unengagedToEngaged.value());
    }

    {
        Optional<int> engagedToUnengaged { 5 };
        EXPECT_TRUE(!!engagedToUnengaged);
        EXPECT_EQ(5, engagedToUnengaged.value());

        Optional<int> unengaged;
        EXPECT_FALSE(!!unengaged);

        engagedToUnengaged = std::move(unengaged);
        EXPECT_FALSE(!!engagedToUnengaged);
        EXPECT_FALSE(!!unengaged);
    }

    {
        Optional<int> engagedToUnengaged { 6 };
        EXPECT_TRUE(!!engagedToUnengaged);
        EXPECT_EQ(6, engagedToUnengaged.value());

        Optional<int> engagedToEngaged { 7 };
        EXPECT_TRUE(!!engagedToEngaged);
        EXPECT_EQ(7, engagedToEngaged.value());

        engagedToEngaged = std::move(engagedToUnengaged);
        EXPECT_FALSE(!!engagedToUnengaged);
        EXPECT_TRUE(!!engagedToEngaged);
        EXPECT_EQ(6, engagedToEngaged.value());
    }
}

} // namespace TestWebKitAPI

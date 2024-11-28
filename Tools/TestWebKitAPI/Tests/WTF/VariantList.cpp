/*
 * Copyright (C) 2011-2022 Apple Inc. All rights reserved.
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

#include "MoveOnly.h"
#include <wtf/VariantList.h>

namespace TestWebKitAPI {

TEST(WTF_VariantList, Basic)
{
    WTF::VariantList<std::variant<int, float>> variantList;
    EXPECT_TRUE(variantList.isEmpty());

    variantList.append(0);
    EXPECT_FALSE(variantList.isEmpty());

    unsigned iterations = 0;
    for (auto proxy : variantList) {
        EXPECT_TRUE(proxy.holds_alternative<int>());

        proxy.visit(
            [&](int value) { EXPECT_EQ(0, value); },
            [&](float) { FAIL(); }
        );

        WTF::switchOn(proxy,
            [&](int value) { EXPECT_EQ(0, value); },
            [&](float) { FAIL(); }
        );

        WTF::switchOn(proxy.asVariant(),
            [&](int value) { EXPECT_EQ(0, value); },
            [&](float) { FAIL(); }
        );

        ++iterations;
    }

    EXPECT_EQ(1u, iterations);
}

TEST(WTF_VariantList, Basic_InlineCapacity)
{
    WTF::VariantList<std::variant<int, float>, 8> variantList;
    EXPECT_TRUE(variantList.isEmpty());

    variantList.append(0);
    EXPECT_FALSE(variantList.isEmpty());

    unsigned iterations = 0;
    for (auto proxy : variantList) {
        EXPECT_TRUE(proxy.holds_alternative<int>());

        proxy.visit(
            [&](int value) { EXPECT_EQ(0, value); },
            [&](float) { FAIL(); }
        );

        WTF::switchOn(proxy,
            [&](int value) { EXPECT_EQ(0, value); },
            [&](float) { FAIL(); }
        );

        WTF::switchOn(proxy.asVariant(),
            [&](int value) { EXPECT_EQ(0, value); },
            [&](float) { FAIL(); }
        );

        ++iterations;
    }

    EXPECT_EQ(1u, iterations);
}

TEST(WTF_VariantList, MoveOnly)
{
    WTF::VariantList<std::variant<int, MoveOnly, float>> variantList;

    variantList.append(MoveOnly(0));
    variantList.append(MoveOnly(1));

    unsigned iterations = 0;

    for (auto proxy : variantList) {
        EXPECT_TRUE(proxy.holds_alternative<MoveOnly>());

        proxy.visit(
            [&](int) { FAIL(); },
            [&](const MoveOnly& value) { EXPECT_EQ(iterations, value.value()); },
            [&](float) { FAIL(); }
        );

        ++iterations;
    }

    EXPECT_EQ(2u, iterations);
}

TEST(WTF_VariantList, EmptyMove)
{
    WTF::VariantList<std::variant<int, MoveOnly, float>> variantList;
    EXPECT_TRUE(variantList.isEmpty());

    auto moved = WTFMove(variantList);
    EXPECT_TRUE(moved.isEmpty());
}

TEST(WTF_VariantList, EmptyCopy)
{
    WTF::VariantList<std::variant<int, float>> variantList;
    EXPECT_TRUE(variantList.isEmpty());

    auto copied = variantList;
    EXPECT_TRUE(copied.isEmpty());
    EXPECT_TRUE(variantList.isEmpty());
}

TEST(WTF_VariantList, MoveWithItems)
{
    WTF::VariantList<std::variant<int, MoveOnly, float>> variantList;
    variantList.append(0);
    variantList.append(MoveOnly(1u));
    variantList.append(2.0f);

    auto moved = WTFMove(variantList);

    unsigned iterations = 0;

    for (auto proxy : moved) {
        proxy.visit(
            [&](int value) { EXPECT_EQ(0, value); },
            [&](const MoveOnly& value) { EXPECT_EQ(1u, value.value()); },
            [&](float value) { EXPECT_EQ(2.0f, value); }
        );

        ++iterations;
    }

    EXPECT_EQ(3u, iterations);
}

TEST(WTF_VariantList, CopyWithItems)
{
    WTF::VariantList<std::variant<int, float>> variantList;
    variantList.append(0);
    variantList.append(1.0f);

    auto copied = variantList;

    unsigned iterations = 0;

    for (auto proxy : variantList) {
        proxy.visit(
            [&](int value) { EXPECT_EQ(0, value); },
            [&](float value) { EXPECT_EQ(1.0f, value); }
        );

        ++iterations;
    }

    EXPECT_EQ(2u, iterations);

    iterations = 0;

    for (auto proxy : copied) {
        proxy.visit(
            [&](int value) { EXPECT_EQ(0, value); },
            [&](float value) { EXPECT_EQ(1.0f, value); }
        );

        ++iterations;
    }

    EXPECT_EQ(2u, iterations);
}

TEST(WTF_VariantList, Equality)
{
    WTF::VariantList<std::variant<int, float, double>> a;
    WTF::VariantList<std::variant<int, float, double>> b;

    // Test self equality when empty.
    EXPECT_EQ(a, a);

    // Test equality when both empty.
    EXPECT_EQ(a, b);

    a.append(1);
    a.append(2.0);

    // Test self equality when NON-empty.
    EXPECT_EQ(a, a);

    // Test NON-equality when one is empty.
    EXPECT_NE(a, b);

    b.append(1);
    b.append(2.0);

    // Test equality when NOT empty.
    EXPECT_EQ(a, b);

    b.append(3.0f);

    // Test NON-equality when both NON-empty.
    EXPECT_NE(a, b);
}

TEST(WTF_VariantList, Grow)
{
    WTF::VariantList<std::variant<int, float, double>> variantList;
    EXPECT_EQ(0u, variantList.capacityInBytes());

    constexpr auto firstCapacity = 32u;

    variantList.append(0.0);

    EXPECT_EQ(firstCapacity, variantList.capacityInBytes());

    for (unsigned i = 0; i < 5; ++i)
        variantList.append(0.0);

    EXPECT_NE(firstCapacity, variantList.capacityInBytes());
}

TEST(WTF_VariantList, Sizer)
{
    using List = WTF::VariantList<std::variant<int, float, double>>;

    // Create a sizer for the list.
    auto sizer = List::Sizer { };

    // Emulate appending a few items.
    sizer.append<double>();
    sizer.append<float>();
    sizer.append<double>();
    sizer.append<int>();

    // Create a list, passing the sizer in to initialize the capacity.
    auto list = List(sizer);

    // Record the capacity.
    auto initialListCapacity = list.capacityInBytes();
    EXPECT_EQ(0u, list.sizeInBytes());

    // Append the same series of item types to the actual list.
    list.append(1.0);
    list.append(2.0f);
    list.append(3.0);
    list.append(4);

    // Check that the size has grown.
    EXPECT_NE(0u, list.sizeInBytes());

    // Check that the capacity has not changed.
    EXPECT_EQ(initialListCapacity, list.capacityInBytes());

    // Check that the capacity is exactly equal to the size.
    EXPECT_EQ(list.capacityInBytes(), list.sizeInBytes());
}

TEST(WTF_VariantList, InlineCapacity)
{
    using List = WTF::VariantList<std::variant<int, double>, 8>;

    List list;
    EXPECT_EQ(8u, list.capacityInBytes());
    EXPECT_EQ(0u, list.sizeInBytes());

    list.append(1);
    EXPECT_EQ(8u, list.capacityInBytes());
    EXPECT_NE(0u, list.sizeInBytes());

    list.append(2.0);
    EXPECT_NE(8u, list.capacityInBytes());
    EXPECT_NE(0u, list.sizeInBytes());
}

} // namespace TestWebKitAPI

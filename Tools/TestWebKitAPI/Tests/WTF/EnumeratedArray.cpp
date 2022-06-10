/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include <wtf/EnumeratedArray.h>

namespace TestWebKitAPI {

enum class Foo {
    One,
    Two,
    Three,
};

TEST(WTF_EnumeratedArray, Basic)
{
    EnumeratedArray<Foo, int, Foo::Three> array;
    array[Foo::Three] = 17;
    EXPECT_EQ(array[Foo::Three], 17);
    EXPECT_EQ(array.at(Foo::Three), 17);
    array[Foo::One] = 3;
    EXPECT_EQ(array.front(), 3);
    EXPECT_EQ(array.back(), 17);
    auto* data = array.data();
    EXPECT_EQ(data[static_cast<size_t>(Foo::One)], 3);
    EXPECT_EQ(data[static_cast<size_t>(Foo::Three)], 17);
    array[Foo::Two] = 10;
    EXPECT_FALSE(array.empty());
    EnumeratedArray<Foo, int, Foo::Three> array2;
    array2.fill(4);
    EXPECT_EQ(array2.at(Foo::One), 4);
    EXPECT_EQ(array2.at(Foo::Two), 4);
    EXPECT_EQ(array2.at(Foo::Three), 4);
    array.swap(array2);
    EXPECT_EQ(array.at(Foo::One), 4);
    EXPECT_EQ(array.at(Foo::Two), 4);
    EXPECT_EQ(array.at(Foo::Three), 4);
    EXPECT_EQ(array2.at(Foo::One), 3);
    EXPECT_EQ(array2.at(Foo::Two), 10);
    EXPECT_EQ(array2.at(Foo::Three), 17);
    EXPECT_EQ(array.size(), static_cast<size_t>(3));
    EXPECT_EQ(array.max_size(), static_cast<size_t>(3));
}

TEST(WTF_EnumeratedArray, Comparison)
{
    EnumeratedArray<Foo, int, Foo::Three> array { { 3, 10, 17 } };
    auto array2 = array;
    EXPECT_EQ(array, array2);
    EXPECT_LE(array, array2);
    EXPECT_GE(array, array2);
    array[Foo::Three] = 18;
    EXPECT_GT(array, array2);
    EXPECT_GE(array, array2);
    EXPECT_LT(array2, array);
    EXPECT_LE(array2, array);
    array[Foo::Three] = 15;
    EXPECT_LT(array, array2);
    EXPECT_LE(array, array2);
    EXPECT_GT(array2, array);
    EXPECT_GE(array2, array);
}

TEST(WTF_EnumeratedArray, Construction)
{
    EnumeratedArray<Foo, int, Foo::Three> array1 { { 3, 10, 17 } };
    EXPECT_EQ(array1.front(), 3);
    EXPECT_EQ(array1.back(), 17);
    EnumeratedArray<Foo, int, Foo::Three> array2(array1);
    EXPECT_EQ(array2.front(), 3);
    EXPECT_EQ(array2.back(), 17);
    EnumeratedArray<Foo, int, Foo::Three> array3(WTFMove(array1));
    EXPECT_EQ(array3.front(), 3);
    EXPECT_EQ(array3.back(), 17);
    EnumeratedArray<Foo, int, Foo::Three> array4;
    array4 = array2;
    EXPECT_EQ(array4.front(), 3);
    EXPECT_EQ(array4.back(), 17);
    EnumeratedArray<Foo, int, Foo::Three> array5;
    array5 = WTFMove(array2);
    EXPECT_EQ(array5.front(), 3);
    EXPECT_EQ(array5.back(), 17);
}

TEST(WTF_EnumeratedArray, Iteration)
{
    EnumeratedArray<Foo, int, Foo::Three> array { { 3, 10, 17 } };
    int index = 0;
    for (int value : array) {
        switch (index) {
        case 0:
            EXPECT_EQ(value, 3);
            break;
        case 1:
            EXPECT_EQ(value, 10);
            break;
        case 2:
            EXPECT_EQ(value, 17);
            break;
        default:
            FAIL();
            break;
        }
        ++index;
    }
    index = 0;
    for (auto iter = array.rbegin(); iter != array.rend(); ++iter) {
        int value = *iter;
        switch (index) {
        case 0:
            EXPECT_EQ(value, 17);
            break;
        case 1:
            EXPECT_EQ(value, 10);
            break;
        case 2:
            EXPECT_EQ(value, 3);
            break;
        default:
            FAIL();
            break;
        }
        ++index;
    }
}

} // namespace TestWebKitAPI

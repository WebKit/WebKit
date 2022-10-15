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
#include <wtf/UniqueRefVector.h>

#include <gtest/gtest.h>
#include <wtf/UniqueRef.h>

#define MAKE(x) makeUniqueRefWithoutFastMallocCheck<int>(x)

namespace TestWebKitAPI {

TEST(WTF_UniqueRefVector, Basic)
{
    UniqueRefVector<int> intRefVector;
    EXPECT_TRUE(intRefVector.isEmpty());
    EXPECT_EQ(0U, intRefVector.size());
    EXPECT_EQ(0U, intRefVector.capacity());
}

TEST(WTF_UniqueRefVector, Iterator)
{
    UniqueRefVector<int> intRefVector;
    intRefVector.append(MAKE(10));
    intRefVector.append(MAKE(11));
    intRefVector.append(MAKE(12));
    intRefVector.append(MAKE(13));

    UniqueRefVector<int>::iterator it = intRefVector.begin();
    UniqueRefVector<int>::iterator end = intRefVector.end();
    EXPECT_NE(it, end);

    EXPECT_EQ(10, *it);
    ++it;
    EXPECT_EQ(11, *it);
    ++it;
    EXPECT_EQ(12, *it);
    ++it;
    EXPECT_EQ(13, *it);
    ++it;

    EXPECT_EQ(end, it);
}

TEST(WTF_UniqueRefVector, ConstructWithFrom)
{
    auto vector = UniqueRefVector<int>::from(MAKE(1), MAKE(2), MAKE(3), MAKE(4), MAKE(5));
    EXPECT_EQ(vector.size(), 5U);
    EXPECT_EQ(vector.capacity(), 5U);

    EXPECT_EQ(vector[0], 1);
    EXPECT_EQ(vector[1], 2);
    EXPECT_EQ(vector[2], 3);
    EXPECT_EQ(vector[3], 4);
    EXPECT_EQ(vector[4], 5);
}

TEST(WTF_UniqueRefVector, Reverse)
{
    UniqueRefVector<int> intRefVector;
    intRefVector.append(MAKE(10));
    intRefVector.append(MAKE(11));
    intRefVector.append(MAKE(12));
    intRefVector.append(MAKE(13));
    intRefVector.reverse();

    EXPECT_EQ(13, intRefVector[0]);
    EXPECT_EQ(12, intRefVector[1]);
    EXPECT_EQ(11, intRefVector[2]);
    EXPECT_EQ(10, intRefVector[3]);

    intRefVector.append(MAKE(9));
    intRefVector.reverse();

    EXPECT_EQ(9, intRefVector[0]);
    EXPECT_EQ(10, intRefVector[1]);
    EXPECT_EQ(11, intRefVector[2]);
    EXPECT_EQ(12, intRefVector[3]);
    EXPECT_EQ(13, intRefVector[4]);
}

TEST(WTF_UniqueRefVector, ReverseIterator)
{
    UniqueRefVector<int> intRefVector;
    intRefVector.append(MAKE(10));
    intRefVector.append(MAKE(11));
    intRefVector.append(MAKE(12));
    intRefVector.append(MAKE(13));

    UniqueRefVector<int>::reverse_iterator it = intRefVector.rbegin();
    UniqueRefVector<int>::reverse_iterator end = intRefVector.rend();
    EXPECT_NE(end, it);

    EXPECT_EQ(13, *it);
    ++it;
    EXPECT_EQ(12, *it);
    ++it;
    EXPECT_EQ(11, *it);
    ++it;
    EXPECT_EQ(10, *it);
    ++it;

    EXPECT_EQ(end, it);
}

TEST(WTF_UniqueRefVector, UncheckedAppend)
{
    UniqueRefVector<int> intRefVector;

    intRefVector.reserveInitialCapacity(100);
    for (int i = 0; i < 100; ++i) {
        auto moveOnly = MAKE(i);
        intRefVector.uncheckedAppend(WTFMove(moveOnly));
        EXPECT_EQ(moveOnly.moveToUniquePtr(), nullptr);
    }

    for (int i = 0; i < 100; ++i)
        EXPECT_EQ(i, intRefVector[i]);
}

TEST(WTF_UniqueRefVector, Insert)
{
    UniqueRefVector<int> intRefVector;

    for (int i = 0; i < 100; ++i) {
        auto moveOnly = MAKE(i);
        intRefVector.insert(0, WTFMove(moveOnly));
        EXPECT_EQ(moveOnly.moveToUniquePtr(), nullptr);
    }

    EXPECT_EQ(intRefVector.size(), 100U);
    for (int i = 0; i < 100; ++i)
        EXPECT_EQ(99 - i, intRefVector[i]);

    for (int i = 0; i < 200; i += 2) {
        auto moveOnly = MAKE(1000 + i);
        intRefVector.insert(i, WTFMove(moveOnly));
        EXPECT_EQ(moveOnly.moveToUniquePtr(), nullptr);
    }

    EXPECT_EQ(200U, intRefVector.size());
    for (int i = 0; i < 200; ++i) {
        if (i % 2)
            EXPECT_EQ(99 - i / 2, intRefVector[i]);
        else
            EXPECT_EQ(1000 + i, intRefVector[i]);
    }
}

TEST(WTF_UniqueRefVector, TakeLast)
{
    UniqueRefVector<int> intRefVector;

    for (int i = 0; i < 100; ++i) {
        auto moveOnly = MAKE(i);
        intRefVector.append(WTFMove(moveOnly));
        EXPECT_EQ(moveOnly.moveToUniquePtr(), nullptr);
    }

    EXPECT_EQ(100U, intRefVector.size());
    for (int i = 0; i < 100; ++i)
        EXPECT_EQ(99 - i, intRefVector.takeLast());

    EXPECT_EQ(0U, intRefVector.size());
}

TEST(WTF_UniqueRefVector, FindIfEmpty)
{
    UniqueRefVector<int> v;
    EXPECT_EQ(v.findIf([](int) { return false; }), notFound);
    EXPECT_EQ(v.findIf([](int) { return true; }), notFound);
}

TEST(WTF_UniqueRefVector, FindIf)
{
    Vector<UniqueRef<int>> v = UniqueRefVector<int>::from(
        MAKE(3), MAKE(1), MAKE(2), MAKE(1), MAKE(2), MAKE(1),
        MAKE(2), MAKE(2), MAKE(1), MAKE(1), MAKE(1), MAKE(3));

    EXPECT_EQ(v.findIf([](int value) { return value > 3; }), notFound);
    EXPECT_EQ(v.findIf([](int) { return false; }), notFound);
    EXPECT_EQ(0U, v.findIf([](int) { return true; }));
    EXPECT_EQ(0U, v.findIf([](int value) { return value <= 3; }));
    EXPECT_EQ(1U, v.findIf([](int value) { return value < 3; }));
    EXPECT_EQ(2U, v.findIf([](int value) { return value == 2; }));
}

} // namespace TestWebKitAPI

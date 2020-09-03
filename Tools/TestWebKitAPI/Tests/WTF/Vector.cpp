/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>
#include <wtf/Vector.h>
#include <wtf/VectorHash.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

TEST(WTF_Vector, Basic)
{
    Vector<int> intVector;
    EXPECT_TRUE(intVector.isEmpty());
    EXPECT_EQ(0U, intVector.size());
    EXPECT_EQ(0U, intVector.capacity());
}

TEST(WTF_Vector, Iterator)
{
    Vector<int> intVector;
    intVector.append(10);
    intVector.append(11);
    intVector.append(12);
    intVector.append(13);

    Vector<int>::iterator it = intVector.begin();
    Vector<int>::iterator end = intVector.end();
    EXPECT_TRUE(end != it);

    EXPECT_EQ(10, *it);
    ++it;
    EXPECT_EQ(11, *it);
    ++it;
    EXPECT_EQ(12, *it);
    ++it;
    EXPECT_EQ(13, *it);
    ++it;

    EXPECT_TRUE(end == it);
}

TEST(WTF_Vector, OverloadedOperatorAmpersand)
{
    struct Test {
    private:
        Test* operator&() = delete;
    };

    Vector<Test> vector;
    vector.append(Test());
}

TEST(WTF_Vector, AppendLast)
{
    Vector<unsigned> vector;
    vector.append(0);

    // FIXME: This test needs to be run with GuardMalloc to show the bug.
    for (size_t i = 0; i < 100; ++i)
        vector.append(const_cast<const unsigned&>(vector.last()));
}

TEST(WTF_Vector, InitializerList)
{
    Vector<int> vector = { 1, 2, 3, 4 };
    EXPECT_EQ(4U, vector.size());

    EXPECT_EQ(1, vector[0]);
    EXPECT_EQ(2, vector[1]);
    EXPECT_EQ(3, vector[2]);
    EXPECT_EQ(4, vector[3]);
}

TEST(WTF_Vector, ConstructWithFrom)
{
    auto vector = Vector<int>::from(1, 2, 3, 4, 5);
    EXPECT_EQ(5U, vector.size());
    EXPECT_EQ(5U, vector.capacity());

    EXPECT_EQ(1, vector[0]);
    EXPECT_EQ(2, vector[1]);
    EXPECT_EQ(3, vector[2]);
    EXPECT_EQ(4, vector[3]);
    EXPECT_EQ(5, vector[4]);
}

TEST(WTF_Vector, ConstructWithFromString)
{
    String s1 = "s1";
    String s2 = "s2";
    String s3 = s1;
    auto vector = Vector<String>::from(s1, s2, WTFMove(s3));
    EXPECT_EQ(3U, vector.size());
    EXPECT_EQ(3U, vector.capacity());

    EXPECT_TRUE(s1 == vector[0]);
    EXPECT_TRUE(s2 == vector[1]);
    EXPECT_TRUE(s1 == vector[2]);
    EXPECT_TRUE(s3.isNull());
}

TEST(WTF_Vector, IsolateCopy)
{
    String s1 = "s1";
    String s2 = "s2";

    auto vector1 = Vector<String>::from(WTFMove(s1), s2);
    EXPECT_EQ(2U, vector1.size());
    EXPECT_EQ(2U, vector1.capacity());

    auto* data1 = vector1[0].impl();
    auto* data2 = vector1[1].impl();

    auto vector2 = vector1.isolatedCopy();

    EXPECT_TRUE("s1" == vector2[0]);
    EXPECT_TRUE("s2" == vector2[1]);

    EXPECT_FALSE(data1 == vector2[0].impl());
    EXPECT_FALSE(data2 == vector2[1].impl());

    auto vector3 = WTFMove(vector1).isolatedCopy();
    EXPECT_EQ(0U, vector1.size());

    EXPECT_TRUE("s1" == vector3[0]);
    EXPECT_TRUE("s2" == vector3[1]);

    EXPECT_TRUE(data1 == vector3[0].impl());
    EXPECT_FALSE(data2 == vector3[1].impl());
}

TEST(WTF_Vector, ConstructWithFromMoveOnly)
{
    auto vector1 = Vector<MoveOnly>::from(MoveOnly(1));
    auto vector3 = Vector<MoveOnly>::from(MoveOnly(1), MoveOnly(2), MoveOnly(3));

    EXPECT_EQ(1U, vector1.size());
    EXPECT_EQ(1U, vector1.capacity());

    EXPECT_EQ(3U, vector3.size());
    EXPECT_EQ(3U, vector3.capacity());

    EXPECT_EQ(1U, vector1[0].value());
    EXPECT_EQ(1U, vector3[0].value());
    EXPECT_EQ(2U, vector3[1].value());
    EXPECT_EQ(3U, vector3[2].value());
}

TEST(WTF_Vector, InitializeFromOtherInitialCapacity)
{
    Vector<int, 3> vector = { 1, 3, 2, 4 };
    Vector<int, 5> vectorCopy(vector);
    EXPECT_EQ(4U, vector.size());
    EXPECT_EQ(4U, vectorCopy.size());
    EXPECT_EQ(5U, vectorCopy.capacity());

    EXPECT_EQ(1, vectorCopy[0]);
    EXPECT_EQ(3, vectorCopy[1]);
    EXPECT_EQ(2, vectorCopy[2]);
    EXPECT_EQ(4, vectorCopy[3]);

    EXPECT_TRUE(vector == vectorCopy);
    EXPECT_FALSE(vector != vectorCopy);
}

TEST(WTF_Vector, CopyFromOtherInitialCapacity)
{
    Vector<int, 3> vector = { 1, 3, 2, 4 };
    Vector<int, 5> vectorCopy { 0 };
    EXPECT_EQ(4U, vector.size());
    EXPECT_EQ(1U, vectorCopy.size());

    vectorCopy = vector;

    EXPECT_EQ(4U, vector.size());
    EXPECT_EQ(4U, vectorCopy.size());
    EXPECT_EQ(5U, vectorCopy.capacity());

    EXPECT_EQ(1, vectorCopy[0]);
    EXPECT_EQ(3, vectorCopy[1]);
    EXPECT_EQ(2, vectorCopy[2]);
    EXPECT_EQ(4, vectorCopy[3]);

    EXPECT_TRUE(vector == vectorCopy);
    EXPECT_FALSE(vector != vectorCopy);
}

TEST(WTF_Vector, InitializeFromOtherOverflowBehavior)
{
    Vector<int, 7, WTF::CrashOnOverflow> vector = { 4, 3, 2, 1 };
    Vector<int, 7, UnsafeVectorOverflow> vectorCopy(vector);
    EXPECT_EQ(4U, vector.size());
    EXPECT_EQ(4U, vectorCopy.size());

    EXPECT_EQ(4, vectorCopy[0]);
    EXPECT_EQ(3, vectorCopy[1]);
    EXPECT_EQ(2, vectorCopy[2]);
    EXPECT_EQ(1, vectorCopy[3]);

    EXPECT_TRUE(vector == vectorCopy);
    EXPECT_FALSE(vector != vectorCopy);
}

TEST(WTF_Vector, CopyFromOtherOverflowBehavior)
{
    Vector<int, 7, WTF::CrashOnOverflow> vector = { 4, 3, 2, 1 };
    Vector<int, 7, UnsafeVectorOverflow> vectorCopy = { 0, 0, 0 };

    EXPECT_EQ(4U, vector.size());
    EXPECT_EQ(3U, vectorCopy.size());

    vectorCopy = vector;

    EXPECT_EQ(4U, vector.size());
    EXPECT_EQ(4U, vectorCopy.size());

    EXPECT_EQ(4, vectorCopy[0]);
    EXPECT_EQ(3, vectorCopy[1]);
    EXPECT_EQ(2, vectorCopy[2]);
    EXPECT_EQ(1, vectorCopy[3]);

    EXPECT_TRUE(vector == vectorCopy);
    EXPECT_FALSE(vector != vectorCopy);
}

TEST(WTF_Vector, InitializeFromOtherMinCapacity)
{
    Vector<int, 7, WTF::CrashOnOverflow, 1> vector = { 3, 4, 2, 1 };
    Vector<int, 7, WTF::CrashOnOverflow, 50> vectorCopy(vector);
    EXPECT_EQ(4U, vector.size());
    EXPECT_EQ(4U, vectorCopy.size());

    EXPECT_EQ(3, vectorCopy[0]);
    EXPECT_EQ(4, vectorCopy[1]);
    EXPECT_EQ(2, vectorCopy[2]);
    EXPECT_EQ(1, vectorCopy[3]);

    EXPECT_TRUE(vector == vectorCopy);
    EXPECT_FALSE(vector != vectorCopy);
}

TEST(WTF_Vector, CopyFromOtherMinCapacity)
{
    Vector<int, 7, WTF::CrashOnOverflow, 1> vector = { 3, 4, 2, 1 };
    Vector<int, 7, WTF::CrashOnOverflow, 50> vectorCopy;

    EXPECT_EQ(4U, vector.size());
    EXPECT_EQ(0U, vectorCopy.size());

    vectorCopy = vector;

    EXPECT_EQ(4U, vector.size());
    EXPECT_EQ(4U, vectorCopy.size());

    EXPECT_EQ(3, vectorCopy[0]);
    EXPECT_EQ(4, vectorCopy[1]);
    EXPECT_EQ(2, vectorCopy[2]);
    EXPECT_EQ(1, vectorCopy[3]);

    EXPECT_TRUE(vector == vectorCopy);
    EXPECT_FALSE(vector != vectorCopy);
}

TEST(WTF_Vector, Reverse)
{
    Vector<int> intVector;
    intVector.append(10);
    intVector.append(11);
    intVector.append(12);
    intVector.append(13);
    intVector.reverse();

    EXPECT_EQ(13, intVector[0]);
    EXPECT_EQ(12, intVector[1]);
    EXPECT_EQ(11, intVector[2]);
    EXPECT_EQ(10, intVector[3]);

    intVector.append(9);
    intVector.reverse();

    EXPECT_EQ(9, intVector[0]);
    EXPECT_EQ(10, intVector[1]);
    EXPECT_EQ(11, intVector[2]);
    EXPECT_EQ(12, intVector[3]);
    EXPECT_EQ(13, intVector[4]);
}

TEST(WTF_Vector, ReverseIterator)
{
    Vector<int> intVector;
    intVector.append(10);
    intVector.append(11);
    intVector.append(12);
    intVector.append(13);

    Vector<int>::reverse_iterator it = intVector.rbegin();
    Vector<int>::reverse_iterator end = intVector.rend();
    EXPECT_TRUE(end != it);

    EXPECT_EQ(13, *it);
    ++it;
    EXPECT_EQ(12, *it);
    ++it;
    EXPECT_EQ(11, *it);
    ++it;
    EXPECT_EQ(10, *it);
    ++it;

    EXPECT_TRUE(end == it);
}

TEST(WTF_Vector, MoveOnly_UncheckedAppend)
{
    Vector<MoveOnly> vector;

    vector.reserveInitialCapacity(100);
    for (size_t i = 0; i < 100; ++i) {
        MoveOnly moveOnly(i);
        vector.uncheckedAppend(WTFMove(moveOnly));
        EXPECT_EQ(0U, moveOnly.value());
    }

    for (size_t i = 0; i < 100; ++i)
        EXPECT_EQ(i, vector[i].value());
}

TEST(WTF_Vector, MoveOnly_Append)
{
    Vector<MoveOnly> vector;

    for (size_t i = 0; i < 100; ++i) {
        MoveOnly moveOnly(i);
        vector.append(WTFMove(moveOnly));
        EXPECT_EQ(0U, moveOnly.value());
    }

    for (size_t i = 0; i < 100; ++i)
        EXPECT_EQ(i, vector[i].value());

    for (size_t i = 0; i < 16; ++i) {
        Vector<MoveOnly> vector;

        vector.append(i);

        for (size_t j = 0; j < i; ++j)
            vector.append(j);
        vector.append(WTFMove(vector[0]));

        EXPECT_EQ(0U, vector[0].value());

        for (size_t j = 0; j < i; ++j)
            EXPECT_EQ(j, vector[j + 1].value());
        EXPECT_EQ(i, vector.last().value());
    }
}

TEST(WTF_Vector, MoveOnly_Insert)
{
    Vector<MoveOnly> vector;

    for (size_t i = 0; i < 100; ++i) {
        MoveOnly moveOnly(i);
        vector.insert(0, WTFMove(moveOnly));
        EXPECT_EQ(0U, moveOnly.value());
    }

    EXPECT_EQ(vector.size(), 100U);
    for (size_t i = 0; i < 100; ++i)
        EXPECT_EQ(99 - i, vector[i].value());

    for (size_t i = 0; i < 200; i += 2) {
        MoveOnly moveOnly(1000 + i);
        vector.insert(i, WTFMove(moveOnly));
        EXPECT_EQ(0U, moveOnly.value());
    }

    EXPECT_EQ(200U, vector.size());
    for (size_t i = 0; i < 200; ++i) {
        if (i % 2)
            EXPECT_EQ(99 - i / 2, vector[i].value());
        else
            EXPECT_EQ(1000 + i, vector[i].value());
    }
}

TEST(WTF_Vector, MoveOnly_TakeLast)
{
    Vector<MoveOnly> vector;

    for (size_t i = 0; i < 100; ++i) {
        MoveOnly moveOnly(i);
        vector.append(WTFMove(moveOnly));
        EXPECT_EQ(0U, moveOnly.value());
    }

    EXPECT_EQ(100U, vector.size());
    for (size_t i = 0; i < 100; ++i)
        EXPECT_EQ(99 - i, vector.takeLast().value());

    EXPECT_EQ(0U, vector.size());
}

TEST(WTF_Vector, VectorOfVectorsOfVectorsInlineCapacitySwap)
{
    Vector<Vector<Vector<int, 1>, 1>, 1> a;
    Vector<Vector<Vector<int, 1>, 1>, 1> b;
    Vector<Vector<Vector<int, 1>, 1>, 1> c;

    EXPECT_EQ(0U, a.size());
    EXPECT_EQ(0U, b.size());
    EXPECT_EQ(0U, c.size());

    Vector<int, 1> x;
    x.append(42);

    EXPECT_EQ(1U, x.size());
    EXPECT_EQ(42, x[0]);

    Vector<Vector<int, 1>, 1> y;
    y.append(x);

    EXPECT_EQ(1U, x.size());
    EXPECT_EQ(42, x[0]);
    EXPECT_EQ(1U, y.size());
    EXPECT_EQ(1U, y[0].size());
    EXPECT_EQ(42, y[0][0]);

    a.append(y);

    EXPECT_EQ(1U, x.size());
    EXPECT_EQ(42, x[0]);
    EXPECT_EQ(1U, y.size());
    EXPECT_EQ(1U, y[0].size());
    EXPECT_EQ(42, y[0][0]);
    EXPECT_EQ(1U, a.size());
    EXPECT_EQ(1U, a[0].size());
    EXPECT_EQ(1U, a[0][0].size());
    EXPECT_EQ(42, a[0][0][0]);

    a.swap(b);

    EXPECT_EQ(0U, a.size());
    EXPECT_EQ(1U, x.size());
    EXPECT_EQ(42, x[0]);
    EXPECT_EQ(1U, y.size());
    EXPECT_EQ(1U, y[0].size());
    EXPECT_EQ(42, y[0][0]);
    EXPECT_EQ(1U, b.size());
    EXPECT_EQ(1U, b[0].size());
    EXPECT_EQ(1U, b[0][0].size());
    EXPECT_EQ(42, b[0][0][0]);

    b.swap(c);

    EXPECT_EQ(0U, a.size());
    EXPECT_EQ(0U, b.size());
    EXPECT_EQ(1U, x.size());
    EXPECT_EQ(42, x[0]);
    EXPECT_EQ(1U, y.size());
    EXPECT_EQ(1U, y[0].size());
    EXPECT_EQ(42, y[0][0]);
    EXPECT_EQ(1U, c.size());
    EXPECT_EQ(1U, c[0].size());
    EXPECT_EQ(1U, c[0][0].size());
    EXPECT_EQ(42, c[0][0][0]);

    y[0][0] = 24;

    EXPECT_EQ(1U, x.size());
    EXPECT_EQ(42, x[0]);
    EXPECT_EQ(1U, y.size());
    EXPECT_EQ(1U, y[0].size());
    EXPECT_EQ(24, y[0][0]);

    a.append(y);

    EXPECT_EQ(1U, x.size());
    EXPECT_EQ(42, x[0]);
    EXPECT_EQ(1U, y.size());
    EXPECT_EQ(1U, y[0].size());
    EXPECT_EQ(24, y[0][0]);
    EXPECT_EQ(1U, a.size());
    EXPECT_EQ(1U, a[0].size());
    EXPECT_EQ(1U, a[0][0].size());
    EXPECT_EQ(24, a[0][0][0]);
    EXPECT_EQ(1U, c.size());
    EXPECT_EQ(1U, c[0].size());
    EXPECT_EQ(1U, c[0][0].size());
    EXPECT_EQ(42, c[0][0][0]);
    EXPECT_EQ(0U, b.size());
}

TEST(WTF_Vector, RemoveFirst)
{
    Vector<int> v;
    EXPECT_TRUE(v.isEmpty());
    EXPECT_FALSE(v.removeFirst(1));
    EXPECT_FALSE(v.removeFirst(-1));
    EXPECT_TRUE(v.isEmpty());

    v.fill(2, 10);
    EXPECT_EQ(10U, v.size());
    EXPECT_FALSE(v.removeFirst(1));
    EXPECT_EQ(10U, v.size());
    v.clear();

    v.fill(1, 10);
    EXPECT_EQ(10U, v.size());
    EXPECT_TRUE(v.removeFirst(1));
    EXPECT_TRUE(v == Vector<int>({1, 1, 1, 1, 1, 1, 1, 1, 1}));
    EXPECT_EQ(9U, v.size());
    EXPECT_FALSE(v.removeFirst(2));
    EXPECT_EQ(9U, v.size());
    EXPECT_TRUE(v == Vector<int>({1, 1, 1, 1, 1, 1, 1, 1, 1}));

    unsigned removed = 0;
    while (v.removeFirst(1))
        ++removed;
    EXPECT_EQ(9U, removed);
    EXPECT_TRUE(v.isEmpty());

    v.resize(1);
    EXPECT_EQ(1U, v.size());
    EXPECT_TRUE(v.removeFirst(1));
    EXPECT_EQ(0U, v.size());
    EXPECT_TRUE(v.isEmpty());
}

TEST(WTF_Vector, RemoveAll)
{
    // Using a memcpy-able type.
    static_assert(VectorTraits<int>::canMoveWithMemcpy, "Should use a memcpy-able type");
    Vector<int> v;
    EXPECT_TRUE(v.isEmpty());
    EXPECT_FALSE(v.removeAll(1));
    EXPECT_FALSE(v.removeAll(-1));
    EXPECT_TRUE(v.isEmpty());

    v.fill(1, 10);
    EXPECT_EQ(10U, v.size());
    EXPECT_EQ(10U, v.removeAll(1));
    EXPECT_TRUE(v.isEmpty());

    v.fill(2, 10);
    EXPECT_EQ(10U, v.size());
    EXPECT_EQ(0U, v.removeAll(1));
    EXPECT_EQ(10U, v.size());

    v = {1, 2, 1, 2, 1, 2, 2, 1, 1, 1};
    EXPECT_EQ(10U, v.size());
    EXPECT_EQ(6U, v.removeAll(1));
    EXPECT_EQ(4U, v.size());
    EXPECT_TRUE(v == Vector<int>({2, 2, 2, 2}));
    EXPECT_TRUE(v.find(1) == notFound);
    EXPECT_EQ(4U, v.removeAll(2));
    EXPECT_TRUE(v.isEmpty());

    v = {3, 1, 2, 1, 2, 1, 2, 2, 1, 1, 1, 3};
    EXPECT_EQ(12U, v.size());
    EXPECT_EQ(6U, v.removeAll(1));
    EXPECT_EQ(6U, v.size());
    EXPECT_TRUE(v.find(1) == notFound);
    EXPECT_TRUE(v == Vector<int>({3, 2, 2, 2, 2, 3}));

    EXPECT_EQ(4U, v.removeAll(2));
    EXPECT_EQ(2U, v.size());
    EXPECT_TRUE(v.find(2) == notFound);
    EXPECT_TRUE(v == Vector<int>({3, 3}));

    EXPECT_EQ(2U, v.removeAll(3));
    EXPECT_TRUE(v.isEmpty());

    v = {1, 1, 1, 3, 2, 4, 2, 2, 2, 4, 4, 3};
    EXPECT_EQ(12U, v.size());
    EXPECT_EQ(3U, v.removeAll(1));
    EXPECT_EQ(9U, v.size());
    EXPECT_TRUE(v.find(1) == notFound);
    EXPECT_TRUE(v == Vector<int>({3, 2, 4, 2, 2, 2, 4, 4, 3}));

    // Using a non memcpy-able type.
    static_assert(!VectorTraits<CString>::canMoveWithMemcpy, "Should use a non memcpy-able type");
    Vector<CString> vExpected;
    Vector<CString> v2;
    EXPECT_TRUE(v2.isEmpty());
    EXPECT_FALSE(v2.removeAll("1"));
    EXPECT_TRUE(v2.isEmpty());

    v2.fill("1", 10);
    EXPECT_EQ(10U, v2.size());
    EXPECT_EQ(10U, v2.removeAll("1"));
    EXPECT_TRUE(v2.isEmpty());

    v2.fill("2", 10);
    EXPECT_EQ(10U, v2.size());
    EXPECT_EQ(0U, v2.removeAll("1"));
    EXPECT_EQ(10U, v2.size());

    v2 = {"1", "2", "1", "2", "1", "2", "2", "1", "1", "1"};
    EXPECT_EQ(10U, v2.size());
    EXPECT_EQ(6U, v2.removeAll("1"));
    EXPECT_EQ(4U, v2.size());
    EXPECT_TRUE(v2.find("1") == notFound);
    EXPECT_EQ(4U, v2.removeAll("2"));
    EXPECT_TRUE(v2.isEmpty());

    v2 = {"3", "1", "2", "1", "2", "1", "2", "2", "1", "1", "1", "3"};
    EXPECT_EQ(12U, v2.size());
    EXPECT_EQ(6U, v2.removeAll("1"));
    EXPECT_EQ(6U, v2.size());
    EXPECT_TRUE(v2.find("1") == notFound);
    vExpected = {"3", "2", "2", "2", "2", "3"};
    EXPECT_TRUE(v2 == vExpected);

    EXPECT_EQ(4U, v2.removeAll("2"));
    EXPECT_EQ(2U, v2.size());
    EXPECT_TRUE(v2.find("2") == notFound);
    vExpected = {"3", "3"};
    EXPECT_TRUE(v2 == vExpected);

    EXPECT_EQ(2U, v2.removeAll("3"));
    EXPECT_TRUE(v2.isEmpty());

    v2 = {"1", "1", "1", "3", "2", "4", "2", "2", "2", "4", "4", "3"};
    EXPECT_EQ(12U, v2.size());
    EXPECT_EQ(3U, v2.removeAll("1"));
    EXPECT_EQ(9U, v2.size());
    EXPECT_TRUE(v2.find("1") == notFound);
    vExpected = {"3", "2", "4", "2", "2", "2", "4", "4", "3"};
    EXPECT_TRUE(v2 == vExpected);
}

TEST(WTF_Vector, FindMatching)
{
    Vector<int> v;
    EXPECT_TRUE(v.findMatching([](int) { return false; }) == notFound);
    EXPECT_TRUE(v.findMatching([](int) { return true; }) == notFound);

    v = {3, 1, 2, 1, 2, 1, 2, 2, 1, 1, 1, 3};
    EXPECT_TRUE(v.findMatching([](int value) { return value > 3; }) == notFound);
    EXPECT_TRUE(v.findMatching([](int) { return false; }) == notFound);
    EXPECT_EQ(0U, v.findMatching([](int) { return true; }));
    EXPECT_EQ(0U, v.findMatching([](int value) { return value <= 3; }));
    EXPECT_EQ(1U, v.findMatching([](int value) { return value < 3; }));
    EXPECT_EQ(2U, v.findMatching([](int value) { return value == 2; }));
}

TEST(WTF_Vector, RemoveFirstMatching)
{
    Vector<int> v;
    EXPECT_TRUE(v.isEmpty());
    EXPECT_FALSE(v.removeFirstMatching([] (int value) { return value > 0; }));
    EXPECT_FALSE(v.removeFirstMatching([] (int) { return true; }));
    EXPECT_FALSE(v.removeFirstMatching([] (int) { return false; }));

    v = {3, 1, 2, 1, 2, 1, 2, 2, 1, 1, 1, 3};
    EXPECT_EQ(12U, v.size());
    EXPECT_FALSE(v.removeFirstMatching([] (int) { return false; }));
    EXPECT_EQ(12U, v.size());
    EXPECT_FALSE(v.removeFirstMatching([] (int value) { return value < 0; }));
    EXPECT_EQ(12U, v.size());
    EXPECT_TRUE(v.removeFirstMatching([] (int value) { return value < 3; }));
    EXPECT_EQ(11U, v.size());
    EXPECT_TRUE(v == Vector<int>({3, 2, 1, 2, 1, 2, 2, 1, 1, 1, 3}));
    EXPECT_TRUE(v.removeFirstMatching([] (int value) { return value > 2; }));
    EXPECT_EQ(10U, v.size());
    EXPECT_TRUE(v == Vector<int>({2, 1, 2, 1, 2, 2, 1, 1, 1, 3}));
    EXPECT_TRUE(v.removeFirstMatching([] (int value) { return value > 2; }));
    EXPECT_EQ(9U, v.size());
    EXPECT_TRUE(v == Vector<int>({2, 1, 2, 1, 2, 2, 1, 1, 1}));
    EXPECT_TRUE(v.removeFirstMatching([] (int value) { return value == 1; }, 1));
    EXPECT_EQ(8U, v.size());
    EXPECT_TRUE(v == Vector<int>({2, 2, 1, 2, 2, 1, 1, 1}));
    EXPECT_TRUE(v.removeFirstMatching([] (int value) { return value == 1; }, 3));
    EXPECT_EQ(7U, v.size());
    EXPECT_TRUE(v == Vector<int>({2, 2, 1, 2, 2, 1, 1}));
    EXPECT_FALSE(v.removeFirstMatching([] (int value) { return value == 1; }, 7));
    EXPECT_EQ(7U, v.size());
    EXPECT_FALSE(v.removeFirstMatching([] (int value) { return value == 1; }, 10));
    EXPECT_EQ(7U, v.size());
}

TEST(WTF_Vector, RemoveAllMatching)
{
    Vector<int> v;
    EXPECT_TRUE(v.isEmpty());
    EXPECT_FALSE(v.removeAllMatching([] (int value) { return value > 0; }));
    EXPECT_FALSE(v.removeAllMatching([] (int) { return true; }));
    EXPECT_FALSE(v.removeAllMatching([] (int) { return false; }));

    v = {3, 1, 2, 1, 2, 1, 2, 2, 1, 1, 1, 3};
    EXPECT_EQ(12U, v.size());
    EXPECT_EQ(0U, v.removeAllMatching([] (int) { return false; }));
    EXPECT_EQ(12U, v.size());
    EXPECT_EQ(0U, v.removeAllMatching([] (int value) { return value < 0; }));
    EXPECT_EQ(12U, v.size());
    EXPECT_EQ(12U, v.removeAllMatching([] (int value) { return value > 0; }));
    EXPECT_TRUE(v.isEmpty());

    v = {3, 1, 2, 1, 2, 1, 3, 2, 2, 1, 1, 1, 3};
    EXPECT_EQ(13U, v.size());
    EXPECT_EQ(3U, v.removeAllMatching([] (int value) { return value > 2; }));
    EXPECT_EQ(10U, v.size());
    EXPECT_TRUE(v == Vector<int>({1, 2, 1, 2, 1, 2, 2, 1, 1, 1}));
    EXPECT_EQ(6U, v.removeAllMatching([] (int value) { return value != 2; }));
    EXPECT_EQ(4U, v.size());
    EXPECT_TRUE(v == Vector<int>({2, 2, 2, 2}));
    EXPECT_EQ(4U, v.removeAllMatching([] (int value) { return value == 2; }));
    EXPECT_TRUE(v.isEmpty());

    v = {3, 1, 2, 1, 2, 1, 3, 2, 2, 1, 1, 1, 3};
    EXPECT_EQ(13U, v.size());
    EXPECT_EQ(0U, v.removeAllMatching([] (int value) { return value > 0; }, 13));
    EXPECT_EQ(13U, v.size());
    EXPECT_EQ(0U, v.removeAllMatching([] (int value) { return value > 0; }, 20));
    EXPECT_EQ(13U, v.size());
    EXPECT_EQ(5U, v.removeAllMatching([] (int value) { return value > 1; }, 3));
    EXPECT_EQ(8U, v.size());
    EXPECT_TRUE(v == Vector<int>({3, 1, 2, 1, 1, 1, 1, 1}));
    EXPECT_EQ(8U, v.removeAllMatching([] (int value) { return value > 0; }, 0));
    EXPECT_EQ(0U, v.size());
}

static int multiplyByTwo(int value)
{
    return 2 * value;
}

TEST(WTF_Vector, MapStaticFunction)
{
    Vector<int> vector { 2, 3, 4 };

    auto mapped = WTF::map(vector, multiplyByTwo);

    EXPECT_EQ(3U, mapped.size());
    EXPECT_EQ(4, mapped[0]);
    EXPECT_EQ(6, mapped[1]);
    EXPECT_EQ(8, mapped[2]);
}

static MoveOnly multiplyByTwoMoveOnly(const MoveOnly& value)
{
    return MoveOnly(2 * value.value());
}

TEST(WTF_Vector, MapStaticFunctionMoveOnly)
{
    Vector<MoveOnly> vector;

    vector.reserveInitialCapacity(3);
    for (unsigned i = 0; i < 3; ++i)
        vector.uncheckedAppend(MoveOnly { i });

    auto mapped = WTF::map(vector, multiplyByTwoMoveOnly);

    EXPECT_EQ(3U, mapped.size());
    EXPECT_EQ(0U, mapped[0].value());
    EXPECT_EQ(2U, mapped[1].value());
    EXPECT_EQ(4U, mapped[2].value());
}

TEST(WTF_Vector, MapLambda)
{
    Vector<int> vector { 2, 3, 4 };

    int counter = 0;
    auto mapped = WTF::map(vector, [&] (int item) {
        counter += 2;
        return counter <= item;
    });

    EXPECT_EQ(3U, mapped.size());
    EXPECT_TRUE(mapped[0]);
    EXPECT_FALSE(mapped[1]);
    EXPECT_FALSE(mapped[2]);
}

TEST(WTF_Vector, MapLambdaMove)
{
    Vector<MoveOnly> vector;

    vector.reserveInitialCapacity(3);
    for (unsigned i = 0; i < 3; ++i)
        vector.uncheckedAppend(MoveOnly { i });


    unsigned counter = 0;
    auto mapped = WTF::map(WTFMove(vector), [&] (MoveOnly&& item) {
        item = item.value() + ++counter;
        return WTFMove(item);
    });

    EXPECT_EQ(3U, mapped.size());
    EXPECT_EQ(1U, mapped[0].value());
    EXPECT_EQ(3U, mapped[1].value());
    EXPECT_EQ(5U, mapped[2].value());
}

TEST(WTF_Vector, MapFromHashMap)
{
    HashMap<String, String> map;
    map.set(String { "k1" }, String { "v1" });
    map.set(String { "k2" }, String { "v2" });
    map.set(String { "k3" }, String { "v3" });

    auto mapped = WTF::map(map, [&] (KeyValuePair<String, String>& pair) -> String {
        return pair.value;
    });
    std::sort(mapped.begin(), mapped.end(), WTF::codePointCompareLessThan);

    EXPECT_EQ(3U, mapped.size());
    EXPECT_TRUE(mapped[0] == "v1");
    EXPECT_TRUE(mapped[1] == "v2");
    EXPECT_TRUE(mapped[2] == "v3");

    mapped = WTF::map(map, [&] (const auto& pair) -> String {
        return pair.key;
    });
    std::sort(mapped.begin(), mapped.end(), WTF::codePointCompareLessThan);

    EXPECT_EQ(3U, mapped.size());
    EXPECT_TRUE(mapped[0] == "k1");
    EXPECT_TRUE(mapped[1] == "k2");
    EXPECT_TRUE(mapped[2] == "k3");

    mapped = WTF::map(WTFMove(map), [&] (KeyValuePair<String, String>&& pair) -> String {
        return WTFMove(pair.value);
    });
    std::sort(mapped.begin(), mapped.end(), WTF::codePointCompareLessThan);

    EXPECT_EQ(3U, mapped.size());
    EXPECT_TRUE(mapped[0] == "v1");
    EXPECT_TRUE(mapped[1] == "v2");
    EXPECT_TRUE(mapped[2] == "v3");

    EXPECT_TRUE(map.contains("k1"));
    EXPECT_TRUE(map.contains("k2"));
    EXPECT_TRUE(map.contains("k3"));

    EXPECT_TRUE(map.get("k1").isNull());
    EXPECT_TRUE(map.get("k2").isNull());
    EXPECT_TRUE(map.get("k3").isNull());

}

WTF::Optional<int> evenMultipliedByFive(int input)
{
    if (input % 2)
        return WTF::nullopt;
    return input * 5;
}

TEST(WTF_Vector, CompactMapStaticFunctionReturnOptional)
{
    Vector<int> vector { 1, 2, 3, 4 };

    static_assert(std::is_same<decltype(WTF::compactMap(vector, evenMultipliedByFive)), typename WTF::Vector<int>>::value,
        "WTF::compactMap returns Vector<Ref<>> when the mapped function returns Optional<>");
    auto mapped = WTF::compactMap(vector, evenMultipliedByFive);

    EXPECT_EQ(2U, mapped.size());
    EXPECT_EQ(10, mapped[0]);
    EXPECT_EQ(20, mapped[1]);
}

struct RefCountedObject : public RefCounted<RefCountedObject> {
public:
    static Ref<RefCountedObject> create(int value) { return adoptRef(*new RefCountedObject(value)); }

    void ref() const
    {
        RefCounted<RefCountedObject>::ref();
        ++s_totalRefCount;
    }

    int value { 0 };

    static unsigned s_totalRefCount;

private:
    RefCountedObject(int value)
        : value(value)
    { }
};

unsigned RefCountedObject::s_totalRefCount = 0;

RefPtr<RefCountedObject> createRefCountedForOdd(int input)
{
    if (input % 2)
        return RefCountedObject::create(input);
    return nullptr;
}

TEST(WTF_Vector, CompactMapStaticFunctionReturnRefPtr)
{
    Vector<int> vector { 1, 2, 3, 4 };

    RefCountedObject::s_totalRefCount = 0;
    static_assert(std::is_same<decltype(WTF::compactMap(vector, createRefCountedForOdd)), typename WTF::Vector<Ref<RefCountedObject>>>::value,
        "WTF::compactMap returns Vector<int> when the mapped function returns Optional<int>");
    auto mapped = WTF::compactMap(vector, createRefCountedForOdd);

    EXPECT_EQ(0U, RefCountedObject::s_totalRefCount);
    EXPECT_EQ(2U, mapped.size());
    EXPECT_EQ(1, mapped[0]->value);
    EXPECT_EQ(3, mapped[1]->value);
}

Optional<Ref<RefCountedObject>> createRefCountedForEven(int input)
{
    if (input % 2)
        return WTF::nullopt;
    return RefCountedObject::create(input);
}

TEST(WTF_Vector, CompactMapStaticFunctionReturnOptionalRef)
{
    Vector<int> vector { 1, 2, 3, 4 };

    RefCountedObject::s_totalRefCount = 0;
    static_assert(std::is_same<decltype(WTF::compactMap(vector, createRefCountedForEven)), typename WTF::Vector<Ref<RefCountedObject>>>::value,
        "WTF::compactMap returns Vector<Ref<>> when the mapped function returns RefPtr<>");
    auto mapped = WTF::compactMap(vector, createRefCountedForEven);

    EXPECT_EQ(0U, RefCountedObject::s_totalRefCount);
    EXPECT_EQ(2U, mapped.size());
    EXPECT_EQ(2, mapped[0]->value);
    EXPECT_EQ(4, mapped[1]->value);
}

Optional<RefPtr<RefCountedObject>> createRefCountedWhenDivisibleByThree(int input)
{
    if (input % 3)
        return WTF::nullopt;
    if (input % 2)
        return RefPtr<RefCountedObject>();
    return RefPtr<RefCountedObject>(RefCountedObject::create(input));
}

TEST(WTF_Vector, CompactMapStaticFunctionReturnOptionalRefPtr)
{
    Vector<int> vector { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

    RefCountedObject::s_totalRefCount = 0;
    static_assert(std::is_same<decltype(WTF::compactMap(vector, createRefCountedWhenDivisibleByThree)), typename WTF::Vector<RefPtr<RefCountedObject>>>::value,
        "WTF::compactMap returns Vector<RefPtr<>> when the mapped function returns Optional<RefPtr<>>");
    auto mapped = WTF::compactMap(vector, createRefCountedWhenDivisibleByThree);

    EXPECT_EQ(0U, RefCountedObject::s_totalRefCount);
    EXPECT_EQ(3U, mapped.size());
    EXPECT_EQ(nullptr, mapped[0]);
    EXPECT_EQ(6, mapped[1]->value);
    EXPECT_EQ(nullptr, mapped[2]);
}

TEST(WTF_Vector, CompactMapLambdaReturnOptional)
{
    Vector<String> vector { "a", "b", "hello", "ciao", "world", "webkit" };

    auto function = [](const String& value) -> Optional<String> {
        if (value.length() < 5)
            return WTF::nullopt;
        return value.convertToASCIIUppercase();
    };
    static_assert(std::is_same<decltype(WTF::compactMap(vector, function)), typename WTF::Vector<String>>::value,
        "WTF::compactMap returns Vector<String> when the mapped function returns Optional<String>");
    auto mapped = WTF::compactMap(vector, function);

    EXPECT_EQ(3U, mapped.size());
    EXPECT_STREQ("HELLO", mapped[0].ascii().data());
    EXPECT_STREQ("WORLD", mapped[1].ascii().data());
    EXPECT_STREQ("WEBKIT", mapped[2].ascii().data());
}

class CountedObject {
public:
    explicit CountedObject(int value)
        : m_value(value)
    { ++s_count; }

    CountedObject(const CountedObject& other)
        : m_value(other.m_value)
    { ++s_count; }

    CountedObject(CountedObject&& other)
        : m_value(other.m_value)
    { }

    int value() const { return m_value; }

    static unsigned& count() { return s_count; }

private:
    int m_value;

    static unsigned s_count;
};

unsigned CountedObject::s_count = 0;

TEST(WTF_Vector, CompactMapLambdaCopyVectorReturnOptionalCountedObject)
{
    Vector<CountedObject> vector { CountedObject(1), CountedObject(2), CountedObject(3), CountedObject(4) };

    CountedObject::count() = 0;

    auto function = [](const CountedObject& object) -> Optional<CountedObject> {
        if (object.value() % 2)
            return object;
        return WTF::nullopt;
    };

    static_assert(std::is_same<decltype(WTF::compactMap(vector, function)), typename WTF::Vector<CountedObject>>::value,
        "WTF::compactMap returns Vector<CountedObject> when the lambda returns Optional<CountedObject>");
    auto mapped = WTF::compactMap(vector, function);

    EXPECT_EQ(2U, CountedObject::count());

    EXPECT_EQ(2U, mapped.size());
    EXPECT_EQ(1, mapped[0].value());
    EXPECT_EQ(3, mapped[1].value());
}

TEST(WTF_Vector, CompactMapLambdaMoveVectorReturnOptionalCountedObject)
{
    Vector<CountedObject> vector { CountedObject(1), CountedObject(2), CountedObject(3), CountedObject(4) };

    CountedObject::count() = 0;

    auto function = [](CountedObject&& object) -> Optional<CountedObject> {
        if (object.value() % 2)
            return WTFMove(object);
        return WTF::nullopt;
    };

    RefCountedObject::s_totalRefCount = 0;
    static_assert(std::is_same<decltype(WTF::compactMap(WTFMove(vector), function)), typename WTF::Vector<CountedObject>>::value,
        "WTF::compactMap returns Vector<CountedObject> when the lambda returns Optional<CountedObject>");
    auto mapped = WTF::compactMap(WTFMove(vector), function);

    EXPECT_EQ(0U, CountedObject::count());

    EXPECT_EQ(2U, mapped.size());
    EXPECT_EQ(1, mapped[0].value());
    EXPECT_EQ(3, mapped[1].value());
}

TEST(WTF_Vector, CompactMapLambdaReturnRefPtr)
{
    Vector<int> vector { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

    auto function = [](int value) -> RefPtr<RefCountedObject> {
        if (value % 3)
            return nullptr;
        return RefCountedObject::create(value);
    };

    RefCountedObject::s_totalRefCount = 0;
    static_assert(std::is_same<decltype(WTF::compactMap(vector, function)), typename WTF::Vector<Ref<RefCountedObject>>>::value,
        "WTF::compactMap returns Vector<Ref<RefCountedObject>> when the lambda returns RefPtr<RefCountedObject>");
    auto mapped = WTF::compactMap(vector, function);

    EXPECT_EQ(0U, RefCountedObject::s_totalRefCount);
    EXPECT_EQ(3U, mapped.size());
    EXPECT_EQ(3, mapped[0]->value);
    EXPECT_EQ(6, mapped[1]->value);
    EXPECT_EQ(9, mapped[2]->value);
}

TEST(WTF_Vector, CompactMapLambdaReturnRefPtrFromMovedRef)
{
    Vector<int> vector { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

    auto countedObjects = WTF::map(vector, [](int value) -> Ref<RefCountedObject> {
        return RefCountedObject::create(value);
    });

    auto function = [](Ref<RefCountedObject>&& object) -> RefPtr<RefCountedObject> {
        if (object->value % 3)
            return nullptr;
        return WTFMove(object);
    };

    RefCountedObject::s_totalRefCount = 0;
    static_assert(std::is_same<decltype(WTF::compactMap(WTFMove(countedObjects), function)), typename WTF::Vector<Ref<RefCountedObject>>>::value,
        "WTF::compactMap returns Vector<Ref<RefCountedObject>> when the lambda returns RefPtr<RefCountedObject>");
    auto mapped = WTF::compactMap(WTFMove(countedObjects), function);

    EXPECT_EQ(0U, RefCountedObject::s_totalRefCount);
    EXPECT_EQ(3U, mapped.size());
    EXPECT_EQ(3, mapped[0]->value);
    EXPECT_EQ(6, mapped[1]->value);
    EXPECT_EQ(9, mapped[2]->value);
}

TEST(WTF_Vector, CompactMapLambdaReturnOptionalRefPtr)
{
    Vector<int> vector { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

    RefCountedObject::s_totalRefCount = 0;

    auto function = [&](int value) -> Optional<RefPtr<RefCountedObject>> {
        if (!(value % 2))
            return WTF::nullopt;
        if (!(value % 3))
            return RefPtr<RefCountedObject>();
        return RefPtr<RefCountedObject>(RefCountedObject::create(value));
    };

    static_assert(std::is_same<decltype(WTF::compactMap(vector, function)), typename WTF::Vector<RefPtr<RefCountedObject>>>::value,
        "WTF::compactMap returns Vector<RefPtr<RefCountedObject>> when the lambda returns Optional<RefPtr<RefCountedObject>>");
    Vector<RefPtr<RefCountedObject>> mapped = WTF::compactMap(vector, function);

    EXPECT_EQ(0U, RefCountedObject::s_totalRefCount);
    EXPECT_EQ(5U, mapped.size());
    EXPECT_EQ(1, mapped[0]->value);
    EXPECT_EQ(nullptr, mapped[1]);
    EXPECT_EQ(5, mapped[2]->value);
    EXPECT_EQ(7, mapped[3]->value);
    EXPECT_EQ(nullptr, mapped[4]);
}

TEST(WTF_Vector, CopyToVector)
{
    HashSet<int> intSet { 1, 2, 3 };

    auto vector = copyToVector(intSet);
    EXPECT_EQ(3U, vector.size());

    std::sort(vector.begin(), vector.end());
    EXPECT_EQ(1, vector[0]);
    EXPECT_EQ(2, vector[1]);
    EXPECT_EQ(3, vector[2]);
}

TEST(WTF_Vector, CopyToVectorOrderPreserving)
{
    ListHashSet<int> orderedIntSet;
    orderedIntSet.add(1);
    orderedIntSet.add(2);
    orderedIntSet.add(3);

    auto vector = copyToVector(orderedIntSet);
    EXPECT_EQ(3U, vector.size());

    EXPECT_EQ(1, vector[0]);
    EXPECT_EQ(2, vector[1]);
    EXPECT_EQ(3, vector[2]);
}

TEST(WTF_Vector, CopyToVectorOf)
{
    HashSet<int> intSet { 1, 2, 3 };

    Vector<float> vector = copyToVectorOf<float>(intSet);
    EXPECT_EQ(3U, vector.size());

    std::sort(vector.begin(), vector.end());
    EXPECT_FLOAT_EQ(1, vector[0]);
    EXPECT_FLOAT_EQ(2, vector[1]);
    EXPECT_FLOAT_EQ(3, vector[2]);
}

TEST(WTF_Vector, CopyToVectorSizeRangeIterator)
{
    HashMap<int, float> map {
        { 1, 9 },
        { 2, 8 },
        { 3, 7 }
    };

    auto keysVector = copyToVector(map.keys());
    EXPECT_EQ(3U, keysVector.size());

    std::sort(keysVector.begin(), keysVector.end());
    EXPECT_EQ(1, keysVector[0]);
    EXPECT_EQ(2, keysVector[1]);
    EXPECT_EQ(3, keysVector[2]);

    auto valuesVector = copyToVector(map.values());
    EXPECT_EQ(3U, valuesVector.size());

    std::sort(valuesVector.begin(), valuesVector.end());
    EXPECT_FLOAT_EQ(7, valuesVector[0]);
    EXPECT_FLOAT_EQ(8, valuesVector[1]);
    EXPECT_FLOAT_EQ(9, valuesVector[2]);
}

TEST(WTF_Vector, StringComparison)
{
    Vector<String> a = {{ "a" }};
    Vector<String> b = {{ "a" }};
    EXPECT_TRUE(a == b);
}

TEST(WTF_Vector, HashKey)
{
    Vector<int> a = { 1 };
    Vector<int> b = { 1, 2 };

    HashSet<Vector<int>> hash;

    EXPECT_FALSE(hash.contains(a));
    EXPECT_FALSE(hash.contains(b));

    hash.add(a);

    EXPECT_TRUE(hash.contains(a));
    EXPECT_FALSE(hash.contains(b));

    hash.add(a);

    EXPECT_TRUE(hash.contains(a));
    EXPECT_FALSE(hash.contains(b));

    hash.add(b);

    EXPECT_TRUE(hash.contains(a));
    EXPECT_TRUE(hash.contains(b));

    hash.remove(a);

    EXPECT_FALSE(hash.contains(a));
    EXPECT_TRUE(hash.contains(b));

    hash.remove(b);

    EXPECT_FALSE(hash.contains(a));
    EXPECT_FALSE(hash.contains(b));
    EXPECT_TRUE(hash.isEmpty());

    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 10; ++j) {
            hash.add({ i, j });
            hash.add({ i, j, i });
        }
    }

    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 10; ++j) {
            EXPECT_TRUE(hash.contains({ i, j }));
            EXPECT_TRUE(hash.contains({ i, j, i }));
            EXPECT_FALSE(hash.contains({ 10, j }));
            EXPECT_FALSE(hash.contains({ i, j, 10 }));
        }
    }

    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 10; ++j) {
            hash.remove({ i, j });
            hash.remove({ i, j, i });
        }
    }

    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 10; ++j) {
            EXPECT_EQ(hash.contains({ i, j }), i > 4);
            EXPECT_EQ(hash.contains({ i, j, i }), i > 4);
        }
    }

    for (int i = 5; i < 10; ++i) {
        for (int j = 0; j < 10; ++j) {
            hash.remove({ i, j });
            hash.remove({ i, j, i });
        }
    }

    EXPECT_TRUE(hash.isEmpty());
}

TEST(WTF_Vector, HashKeyInlineCapacity)
{
    Vector<int, 1> a = { 1 };
    Vector<int, 1> b = { 1, 2 };

    HashSet<Vector<int, 1>> hash;

    EXPECT_FALSE(hash.contains(a));
    EXPECT_FALSE(hash.contains(b));

    hash.add(a);

    EXPECT_TRUE(hash.contains(a));
    EXPECT_FALSE(hash.contains(b));

    hash.add(a);

    EXPECT_TRUE(hash.contains(a));
    EXPECT_FALSE(hash.contains(b));

    hash.add(b);

    EXPECT_TRUE(hash.contains(a));
    EXPECT_TRUE(hash.contains(b));

    hash.remove(a);

    EXPECT_FALSE(hash.contains(a));
    EXPECT_TRUE(hash.contains(b));

    hash.remove(b);

    EXPECT_FALSE(hash.contains(a));
    EXPECT_FALSE(hash.contains(b));
    EXPECT_TRUE(hash.isEmpty());
}

TEST(WTF_Vector, HashKeyString)
{
    Vector<String> a = { "a" };
    Vector<String> b = { "a", "b" };

    HashSet<Vector<String>> hash;

    EXPECT_FALSE(hash.contains(a));
    EXPECT_FALSE(hash.contains(b));

    hash.add(a);

    EXPECT_TRUE(hash.contains(a));
    EXPECT_FALSE(hash.contains(b));

    hash.add(a);

    EXPECT_TRUE(hash.contains(a));
    EXPECT_FALSE(hash.contains(b));

    hash.add(b);

    EXPECT_TRUE(hash.contains(a));
    EXPECT_TRUE(hash.contains(b));

    hash.remove(a);

    EXPECT_FALSE(hash.contains(a));
    EXPECT_TRUE(hash.contains(b));

    hash.remove(b);

    EXPECT_FALSE(hash.contains(a));
    EXPECT_FALSE(hash.contains(b));

    for (auto i : { "a", "b", "c", "d", "e" }) {
        for (auto j : { "a", "b", "c", "d", "e" }) {
            hash.add({ i, j });
            hash.add({ i, j, i, j });
        }
    }

    for (auto i : { "a", "b", "c", "d", "e" }) {
        for (auto j : { "a", "b", "c", "d", "e" }) {
            EXPECT_TRUE(hash.contains({ i, j }));
            EXPECT_TRUE(hash.contains({ i, j, i, j }));
            EXPECT_FALSE(hash.contains({ i, j, i }));
            EXPECT_FALSE(hash.contains({ "x", j }));
            EXPECT_FALSE(hash.contains({ i, j, "x", j }));
        }
    }

    for (auto i : { "d", "e" }) {
        for (auto j : { "a", "b", "c", "d", "e" }) {
            hash.remove({ i, j });
            hash.remove({ i, j, i, j });
        }
    }

    for (auto i : { "d", "e" }) {
        for (auto j : { "a", "b", "c", "d", "e" }) {
            EXPECT_FALSE(hash.contains({ i, j }));
            EXPECT_FALSE(hash.contains({ i, j, i, j }));
        }
    }

    for (auto i : { "a", "b", "c" }) {
        for (auto j : { "a", "b", "c", "d", "e" }) {
            EXPECT_TRUE(hash.contains({ i, j }));
            EXPECT_TRUE(hash.contains({ i, j, i, j }));
        }
    }

    for (auto i : { "a", "b", "c" }) {
        for (auto j : { "a", "b", "c", "d", "e" }) {
            hash.remove({ i, j });
            hash.remove({ i, j, i, j });
        }
    }

    EXPECT_TRUE(hash.isEmpty());
}
    
} // namespace TestWebKitAPI

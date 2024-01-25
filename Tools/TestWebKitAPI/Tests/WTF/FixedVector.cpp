/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
#include <wtf/FixedVector.h>
#include <wtf/VectorHash.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

TEST(WTF_FixedVector, Empty)
{
    FixedVector<unsigned> intVector;
    EXPECT_TRUE(intVector.isEmpty());
    EXPECT_EQ(0U, intVector.size());
}

TEST(WTF_FixedVector, Iterator)
{
    FixedVector<unsigned> intVector(4);
    intVector[0] = 10;
    intVector[1] = 11;
    intVector[2] = 12;
    intVector[3] = 13;

    FixedVector<unsigned>::iterator it = intVector.begin();
    FixedVector<unsigned>::iterator end = intVector.end();
    EXPECT_TRUE(end != it);

    EXPECT_EQ(10U, *it);
    ++it;
    EXPECT_EQ(11U, *it);
    ++it;
    EXPECT_EQ(12U, *it);
    ++it;
    EXPECT_EQ(13U, *it);
    ++it;

    EXPECT_TRUE(end == it);
}

TEST(WTF_FixedVector, OverloadedOperatorAmpersand)
{
    struct Test {
    private:
        Test* operator&() = delete;
    };

    FixedVector<Test> vector(1);
    vector[0] = Test();
}

TEST(WTF_FixedVector, ClearContainsAndFind)
{
    FixedVector<int> v;
    EXPECT_TRUE(v.isEmpty());
    v.clear();
    EXPECT_TRUE(v.isEmpty());

    v = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    EXPECT_EQ(10U, v.size());
    EXPECT_FALSE(v.contains(1));
    EXPECT_EQ(notFound, v.find(1));
    v.clear();
    EXPECT_TRUE(v.isEmpty());
    EXPECT_FALSE(v.contains(1));
    EXPECT_EQ(notFound, v.find(1));

    v = { 3, 1, 2, 1, 1, 4, 2, 2, 1, 1, 3, 5 };
    EXPECT_EQ(12U, v.size());
    EXPECT_TRUE(v.contains(3));
    EXPECT_TRUE(v.contains(4));
    EXPECT_TRUE(v.contains(5));
    EXPECT_FALSE(v.contains(6));
    EXPECT_EQ(0U, v.find(3));
    EXPECT_EQ(5U, v.find(4));
    EXPECT_EQ(11U, v.find(5));
    EXPECT_EQ(notFound, v.find(6));
    EXPECT_TRUE(v == FixedVector<int>({ 3, 1, 2, 1, 1, 4, 2, 2, 1, 1, 3, 5 }));
    v.clear();
    EXPECT_EQ(0U, v.size());
    EXPECT_FALSE(v.contains(3));
    EXPECT_FALSE(v.contains(4));
    EXPECT_FALSE(v.contains(5));
    EXPECT_FALSE(v.contains(6));
    EXPECT_EQ(notFound, v.find(3));
    EXPECT_EQ(notFound, v.find(4));
    EXPECT_EQ(notFound, v.find(5));
    EXPECT_EQ(notFound, v.find(6));
    EXPECT_TRUE(v == FixedVector<int>({ }));
}

TEST(WTF_FixedVector, Copy)
{
    FixedVector<unsigned> vec1(3);
    vec1[0] = 0;
    vec1[1] = 1;
    vec1[2] = 2;

    FixedVector<unsigned> vec2(vec1);
    EXPECT_EQ(3U, vec1.size());
    EXPECT_EQ(3U, vec2.size());
    for (unsigned i = 0; i < vec1.size(); ++i) {
        EXPECT_EQ(i, vec1[i]);
        EXPECT_EQ(i, vec2[i]);
    }
    vec1[2] = 42;
    EXPECT_EQ(42U, vec1[2]);
    EXPECT_EQ(2U, vec2[2]);
}

TEST(WTF_FixedVector, CopyAssign)
{
    FixedVector<unsigned> vec1(3);
    vec1[0] = 0;
    vec1[1] = 1;
    vec1[2] = 2;

    FixedVector<unsigned> vec2;
    vec2 = vec1;
    EXPECT_EQ(3U, vec1.size());
    EXPECT_EQ(3U, vec2.size());
    for (unsigned i = 0; i < vec1.size(); ++i) {
        EXPECT_EQ(i, vec1[i]);
        EXPECT_EQ(i, vec2[i]);
    }
    vec1[2] = 42;
    EXPECT_EQ(42U, vec1[2]);
    EXPECT_EQ(2U, vec2[2]);
}

TEST(WTF_FixedVector, FindIf)
{
    FixedVector<int> v;
    EXPECT_TRUE(v.findIf([](int) { return false; }) == notFound);
    EXPECT_TRUE(v.findIf([](int) { return true; }) == notFound);

    v = { 3, 1, 2, 1, 2, 1, 2, 2, 1, 1, 1, 3 };
    EXPECT_TRUE(v.findIf([](int value) { return value > 3; }) == notFound);
    EXPECT_TRUE(v.findIf([](int) { return false; }) == notFound);
    EXPECT_EQ(0U, v.findIf([](int) { return true; }));
    EXPECT_EQ(0U, v.findIf([](int value) { return value <= 3; }));
    EXPECT_EQ(1U, v.findIf([](int value) { return value < 3; }));
    EXPECT_EQ(2U, v.findIf([](int value) { return value == 2; }));
}

TEST(WTF_FixedVector, Move)
{
    FixedVector<unsigned> vec1(3);
    vec1[0] = 0;
    vec1[1] = 1;
    vec1[2] = 2;

    FixedVector<unsigned> vec2(WTFMove(vec1));
    IGNORE_CLANG_STATIC_ANALYZER_USE_AFTER_MOVE_ATTRIBUTE
    EXPECT_EQ(0U, vec1.size());
    EXPECT_EQ(3U, vec2.size());
    for (unsigned i = 0; i < vec2.size(); ++i)
        EXPECT_EQ(i, vec2[i]);
}

TEST(WTF_FixedVector, MoveAssign)
{
    FixedVector<unsigned> vec1(3);
    vec1[0] = 0;
    vec1[1] = 1;
    vec1[2] = 2;

    FixedVector<unsigned> vec2;
    vec2 = WTFMove(vec1);
    IGNORE_CLANG_STATIC_ANALYZER_USE_AFTER_MOVE_ATTRIBUTE
    EXPECT_EQ(0U, vec1.size());
    EXPECT_EQ(3U, vec2.size());
    for (unsigned i = 0; i < vec2.size(); ++i)
        EXPECT_EQ(i, vec2[i]);
}

TEST(WTF_FixedVector, MoveVector)
{
    auto vec1 = Vector<MoveOnly>::from(MoveOnly(0), MoveOnly(1), MoveOnly(2), MoveOnly(3));
    EXPECT_EQ(4U, vec1.size());
    FixedVector<MoveOnly> vec2(WTFMove(vec1));
    IGNORE_CLANG_STATIC_ANALYZER_USE_AFTER_MOVE_ATTRIBUTE
    EXPECT_EQ(0U, vec1.size());
    EXPECT_EQ(4U, vec2.size());
    for (unsigned index = 0; index < vec2.size(); ++index)
        EXPECT_EQ(index, vec2[index].value());
}

TEST(WTF_FixedVector, MoveAssignVector)
{
    FixedVector<MoveOnly> vec2;
    {
        auto vec1 = Vector<MoveOnly>::from(MoveOnly(0), MoveOnly(1), MoveOnly(2), MoveOnly(3));
        EXPECT_EQ(4U, vec1.size());
        vec2 = WTFMove(vec1);
        IGNORE_CLANG_STATIC_ANALYZER_USE_AFTER_MOVE_ATTRIBUTE
        EXPECT_EQ(0U, vec1.size());
    }
    EXPECT_EQ(4U, vec2.size());
    for (unsigned index = 0; index < vec2.size(); ++index)
        EXPECT_EQ(index, vec2[index].value());
}

TEST(WTF_FixedVector, Swap)
{
    FixedVector<unsigned> vec1(3);
    vec1[0] = 0;
    vec1[1] = 1;
    vec1[2] = 2;

    FixedVector<unsigned> vec2;
    vec2.swap(vec1);
    EXPECT_EQ(0U, vec1.size());
    EXPECT_EQ(3U, vec2.size());
    for (unsigned i = 0; i < vec2.size(); ++i)
        EXPECT_EQ(i, vec2[i]);
}

TEST(WTF_FixedVector, IteratorFor)
{
    FixedVector<unsigned> vec1(3);
    vec1[0] = 0;
    vec1[1] = 1;
    vec1[2] = 2;

    unsigned index = 0;
    for (auto iter = vec1.begin(); iter != vec1.end(); ++iter) {
        EXPECT_EQ(index, *iter);
        ++index;
    }
}

TEST(WTF_FixedVector, Reverse)
{
    FixedVector<unsigned> vec1(3);
    vec1[0] = 0;
    vec1[1] = 1;
    vec1[2] = 2;

    unsigned index = 0;
    for (auto iter = vec1.rbegin(); iter != vec1.rend(); ++iter) {
        ++index;
        EXPECT_EQ(3U - index, *iter);
    }
}

TEST(WTF_FixedVector, Fill)
{
    FixedVector<unsigned> vec1(3);
    vec1.fill(42);

    for (auto& value : vec1)
        EXPECT_EQ(42U, value);
}

struct DestructorObserver {
    DestructorObserver() = default;

    DestructorObserver(bool* destructed)
        : destructed(destructed)
    {
    }

    ~DestructorObserver()
    {
        if (destructed)
            *destructed = true;
    }

    DestructorObserver(DestructorObserver&& other)
        : destructed(other.destructed)
    {
        other.destructed = nullptr;
    }

    DestructorObserver& operator=(DestructorObserver&& other)
    {
        destructed = other.destructed;
        other.destructed = nullptr;
        return *this;
    }

    bool* destructed { nullptr };
};

TEST(WTF_FixedVector, Destructor)
{
    Vector<bool> flags(3, false);
    {
        FixedVector<DestructorObserver> vector(flags.size());
        for (unsigned i = 0; i < flags.size(); ++i)
            vector[i] = DestructorObserver(&flags[i]);
        for (unsigned i = 0; i < flags.size(); ++i)
            EXPECT_FALSE(flags[i]);
    }
    for (unsigned i = 0; i < flags.size(); ++i)
        EXPECT_TRUE(flags[i]);
}

TEST(WTF_FixedVector, DestructorAfterMove)
{
    Vector<bool> flags(3, false);
    {
        FixedVector<DestructorObserver> outerVector;
        {
            FixedVector<DestructorObserver> vector(flags.size());
            for (unsigned i = 0; i < flags.size(); ++i)
                vector[i] = DestructorObserver(&flags[i]);
            for (unsigned i = 0; i < flags.size(); ++i)
                EXPECT_FALSE(flags[i]);
            outerVector = WTFMove(vector);
        }
        for (unsigned i = 0; i < flags.size(); ++i)
            EXPECT_FALSE(flags[i]);
    }
    for (unsigned i = 0; i < flags.size(); ++i)
        EXPECT_TRUE(flags[i]);
}

TEST(WTF_FixedVector, MoveKeepsData)
{
    {
        FixedVector<unsigned> vec1(3);
        auto* data1 = vec1.data();
        FixedVector<unsigned> vec2(WTFMove(vec1));
        EXPECT_EQ(data1, vec2.data());
    }
    {
        FixedVector<unsigned> vec1(3);
        auto* data1 = vec1.data();
        FixedVector<unsigned> vec2;
        vec2 = WTFMove(vec1);
        EXPECT_EQ(data1, vec2.data());
    }
}

TEST(WTF_FixedVector, MoveKeepsDataNested)
{
    {
        Vector<FixedVector<unsigned>> vec1;
        vec1.append(FixedVector<unsigned>(3));
        auto* data1 = vec1[0].data();
        FixedVector<FixedVector<unsigned>> vec2(WTFMove(vec1));
        EXPECT_EQ(1U, vec2.size());
        EXPECT_EQ(data1, vec2[0].data());
    }
    {
        Vector<FixedVector<unsigned>> vec1;
        vec1.append(FixedVector<unsigned>(3));
        auto* data1 = vec1[0].data();
        FixedVector<FixedVector<unsigned>> vec2;
        vec2 = WTFMove(vec1);
        EXPECT_EQ(1U, vec2.size());
        EXPECT_EQ(data1, vec2[0].data());
    }
}

TEST(WTF_FixedVector, Offset)
{
    EXPECT_EQ(0, FixedVector<unsigned>::offsetOfStorage());
    EXPECT_EQ(4, FixedVector<unsigned>::Storage::offsetOfData());
    EXPECT_EQ(0, FixedVector<void*>::offsetOfStorage());
    EXPECT_EQ(sizeof(void*), static_cast<unsigned>(FixedVector<void*>::Storage::offsetOfData()));
}

TEST(WTF_FixedVector, Equal)
{
    {
        FixedVector<unsigned> vec1(10);
        FixedVector<unsigned> vec2(10);
        for (unsigned i = 0; i < 10; ++i) {
            vec1[i] = i;
            vec2[i] = i;
        }
        EXPECT_EQ(vec2, vec1);
        vec1[0] = 42;
        EXPECT_NE(vec1, vec2);
    }
    {
        FixedVector<unsigned> vec1;
        FixedVector<unsigned> vec2;
        FixedVector<unsigned> vec3(0);
        EXPECT_EQ(vec2, vec1);
        EXPECT_EQ(vec3, vec1);
    }
}

} // namespace TestWebKitAPI

/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include <array>
#include <wtf/EmbeddedFixedVector.h>

namespace TestWebKitAPI {

TEST(WTF_EmbeddedFixedVector, Empty)
{
    auto vector = EmbeddedFixedVector<unsigned>::create(0);
    EXPECT_TRUE(vector->isEmpty());
    EXPECT_EQ(0U, vector->size());
}

TEST(WTF_EmbeddedFixedVector, Iterator)
{
    auto intVector = EmbeddedFixedVector<unsigned>::create(4);
    intVector.get()[0] = 10;
    intVector.get()[1] = 11;
    intVector.get()[2] = 12;
    intVector.get()[3] = 13;

    auto it = intVector->begin();
    auto end = intVector->end();
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

TEST(WTF_EmbeddedFixedVector, OverloadedOperatorAmpersand)
{
    struct Test {
    private:
        Test* operator&() = delete;
    };

    auto vector = EmbeddedFixedVector<Test>::create(1);
    vector.get()[0] = Test();
}

TEST(WTF_EmbeddedFixedVector, Copy)
{
    auto vec1 = EmbeddedFixedVector<unsigned>::create(3);
    vec1.get()[0] = 0;
    vec1.get()[1] = 1;
    vec1.get()[2] = 2;

    auto vec2 = EmbeddedFixedVector<unsigned>::create(vec1->begin(), vec1->end());
    EXPECT_EQ(3U, vec1->size());
    EXPECT_EQ(3U, vec2->size());
    for (unsigned i = 0; i < vec1->size(); ++i) {
        EXPECT_EQ(i, vec1.get()[i]);
        EXPECT_EQ(i, vec2.get()[i]);
    }
    vec1.get()[2] = 42;
    EXPECT_EQ(42U, vec1.get()[2]);
    EXPECT_EQ(2U, vec2.get()[2]);
}

TEST(WTF_EmbeddedFixedVector, CopyVector)
{
    auto vec1 = Vector<unsigned>::from(0, 1, 2, 3);
    EXPECT_EQ(4U, vec1.size());
    auto vec2 = EmbeddedFixedVector<unsigned>::createFromVector(vec1);
    EXPECT_EQ(4U, vec1.size());
    EXPECT_EQ(4U, vec2->size());
    for (unsigned i = 0; i < vec1.size(); ++i) {
        EXPECT_EQ(i, vec1[i]);
        EXPECT_EQ(i, vec2.get()[i]);
    }
    vec1[2] = 42;
    EXPECT_EQ(42U, vec1[2]);
    EXPECT_EQ(2U, vec2.get()[2]);
}

TEST(WTF_EmbeddedFixedVector, MoveVector)
{
    auto vec1 = Vector<MoveOnly>::from(MoveOnly(0), MoveOnly(1), MoveOnly(2), MoveOnly(3));
    EXPECT_EQ(4U, vec1.size());
    auto vec2 = EmbeddedFixedVector<MoveOnly>::createFromVector(WTFMove(vec1));
    EXPECT_EQ(0U, vec1.size());
    EXPECT_EQ(4U, vec2->size());
    for (unsigned index = 0; index < vec2->size(); ++index)
        EXPECT_EQ(index, vec2.get()[index].value());
}

TEST(WTF_EmbeddedFixedVector, IteratorFor)
{
    auto vec1 = EmbeddedFixedVector<unsigned>::create(3);
    vec1.get()[0] = 0;
    vec1.get()[1] = 1;
    vec1.get()[2] = 2;

    unsigned index = 0;
    for (auto iter = vec1->begin(); iter != vec1->end(); ++iter) {
        EXPECT_EQ(index, *iter);
        ++index;
    }
}

TEST(WTF_EmbeddedFixedVector, Reverse)
{
    auto vec1 = EmbeddedFixedVector<unsigned>::create(3);
    vec1.get()[0] = 0;
    vec1.get()[1] = 1;
    vec1.get()[2] = 2;

    unsigned index = 0;
    for (auto iter = vec1->rbegin(); iter != vec1->rend(); ++iter) {
        ++index;
        EXPECT_EQ(3U - index, *iter);
    }
}

TEST(WTF_EmbeddedFixedVector, Fill)
{
    auto vec1 = EmbeddedFixedVector<unsigned>::create(3);
    vec1->fill(42);

    for (auto& value : vec1.get())
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

TEST(WTF_EmbeddedFixedVector, Destructor)
{
    Vector<bool> flags(3, false);
    {
        auto vector = EmbeddedFixedVector<DestructorObserver>::create(flags.size());
        for (unsigned i = 0; i < flags.size(); ++i)
            vector.get()[i] = DestructorObserver(&flags[i]);
        for (unsigned i = 0; i < flags.size(); ++i)
            EXPECT_FALSE(flags[i]);
    }
    for (unsigned i = 0; i < flags.size(); ++i)
        EXPECT_TRUE(flags[i]);
}

TEST(WTF_EmbeddedFixedVector, DestructorAfterMove)
{
    Vector<bool> flags(3, false);
    {
        std::unique_ptr<EmbeddedFixedVector<DestructorObserver>> outerVector;
        {
            auto vector = EmbeddedFixedVector<DestructorObserver>::create(flags.size());
            for (unsigned i = 0; i < flags.size(); ++i)
                vector.get()[i] = DestructorObserver(&flags[i]);
            for (unsigned i = 0; i < flags.size(); ++i)
                EXPECT_FALSE(flags[i]);
            outerVector = vector.moveToUniquePtr();
        }
        for (unsigned i = 0; i < flags.size(); ++i)
            EXPECT_FALSE(flags[i]);
    }
    for (unsigned i = 0; i < flags.size(); ++i)
        EXPECT_TRUE(flags[i]);
}

TEST(WTF_EmbeddedFixedVector, Basic)
{
    std::array<unsigned, 4> array { {
        0, 1, 2, 3
    } };
    {
        auto vector = EmbeddedFixedVector<unsigned>::create(array.begin(), array.end());
        EXPECT_EQ(4U, vector->size());
        EXPECT_EQ(0U, vector->at(0));
        EXPECT_EQ(1U, vector->at(1));
        EXPECT_EQ(2U, vector->at(2));
        EXPECT_EQ(3U, vector->at(3));
    }
}

TEST(WTF_EmbeddedFixedVector, Clone)
{
    std::array<unsigned, 4> array { {
        0, 1, 2, 3
    } };
    {
        auto vector = EmbeddedFixedVector<unsigned>::create(array.begin(), array.end());
        EXPECT_EQ(4U, vector->size());
        EXPECT_EQ(0U, vector->at(0));
        EXPECT_EQ(1U, vector->at(1));
        EXPECT_EQ(2U, vector->at(2));
        EXPECT_EQ(3U, vector->at(3));

        auto cloned = vector->clone();
        EXPECT_EQ(4U, cloned->size());
        EXPECT_EQ(0U, cloned->at(0));
        EXPECT_EQ(1U, cloned->at(1));
        EXPECT_EQ(2U, cloned->at(2));
        EXPECT_EQ(3U, cloned->at(3));
    }
}

TEST(WTF_EmbeddedFixedVector, Equal)
{
    auto vec1 = EmbeddedFixedVector<unsigned>::create(10);
    auto vec2 = EmbeddedFixedVector<unsigned>::create(10);
    for (unsigned i = 0; i < 10; ++i) {
        vec1.get()[i] = i;
        vec2.get()[i] = i;
    }
    EXPECT_EQ(vec1.get(), vec2.get());
    vec1.get()[0] = 42;
    EXPECT_NE(vec1.get(), vec2.get());
}

}

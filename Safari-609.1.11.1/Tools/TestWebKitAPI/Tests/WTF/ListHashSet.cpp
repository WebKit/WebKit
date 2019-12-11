/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "Counters.h"
#include "MoveOnly.h"
#include <wtf/ListHashSet.h>

namespace TestWebKitAPI {

TEST(WTF_ListHashSet, RemoveFirst)
{
    ListHashSet<int> list;
    list.add(1);
    list.add(2);
    list.add(3);

    ASSERT_EQ(1, list.first());

    list.removeFirst();
    ASSERT_EQ(2, list.first());

    list.removeFirst();
    ASSERT_EQ(3, list.first());

    list.removeFirst();
    ASSERT_TRUE(list.isEmpty());
}

TEST(WTF_ListHashSet, RemoveLast)
{
    ListHashSet<int> list;
    list.add(1);
    list.add(2);
    list.add(3);

    ASSERT_EQ(3, list.last());

    list.removeLast();
    ASSERT_EQ(2, list.last());

    list.removeLast();
    ASSERT_EQ(1, list.last());

    list.removeLast();
    ASSERT_TRUE(list.isEmpty());
}

TEST(WTF_ListHashSet, AppendOrMoveToLastNewItems)
{
    ListHashSet<int> list;
    ListHashSet<int>::AddResult result = list.appendOrMoveToLast(1);
    ASSERT_TRUE(result.isNewEntry);
    result = list.add(2);
    ASSERT_TRUE(result.isNewEntry);
    result = list.appendOrMoveToLast(3);
    ASSERT_TRUE(result.isNewEntry);

    ASSERT_EQ(list.size(), 3u);

    // The list should be in order 1, 2, 3.
    ListHashSet<int>::iterator iterator = list.begin();
    ASSERT_EQ(1, *iterator);
    ++iterator;
    ASSERT_EQ(2, *iterator);
    ++iterator;
    ASSERT_EQ(3, *iterator);
    ++iterator;
}

TEST(WTF_ListHashSet, AppendOrMoveToLastWithDuplicates)
{
    ListHashSet<int> list;

    // Add a single element twice.
    ListHashSet<int>::AddResult result = list.add(1);
    ASSERT_TRUE(result.isNewEntry);
    result = list.appendOrMoveToLast(1);
    ASSERT_FALSE(result.isNewEntry);
    ASSERT_EQ(1u, list.size());

    list.add(2);
    list.add(3);
    ASSERT_EQ(3u, list.size());

    // Appending 2 move it to the end.
    ASSERT_EQ(3, list.last());
    result = list.appendOrMoveToLast(2);
    ASSERT_FALSE(result.isNewEntry);
    ASSERT_EQ(2, list.last());

    // Inverse the list by moving each element to end end.
    result = list.appendOrMoveToLast(3);
    ASSERT_FALSE(result.isNewEntry);
    result = list.appendOrMoveToLast(2);
    ASSERT_FALSE(result.isNewEntry);
    result = list.appendOrMoveToLast(1);
    ASSERT_FALSE(result.isNewEntry);
    ASSERT_EQ(3u, list.size());

    ListHashSet<int>::iterator iterator = list.begin();
    ASSERT_EQ(3, *iterator);
    ++iterator;
    ASSERT_EQ(2, *iterator);
    ++iterator;
    ASSERT_EQ(1, *iterator);
    ++iterator;
}

TEST(WTF_ListHashSet, PrependOrMoveToLastNewItems)
{
    ListHashSet<int> list;
    ListHashSet<int>::AddResult result = list.prependOrMoveToFirst(1);
    ASSERT_TRUE(result.isNewEntry);
    result = list.add(2);
    ASSERT_TRUE(result.isNewEntry);
    result = list.prependOrMoveToFirst(3);
    ASSERT_TRUE(result.isNewEntry);

    ASSERT_EQ(list.size(), 3u);

    // The list should be in order 3, 1, 2.
    ListHashSet<int>::iterator iterator = list.begin();
    ASSERT_EQ(3, *iterator);
    ++iterator;
    ASSERT_EQ(1, *iterator);
    ++iterator;
    ASSERT_EQ(2, *iterator);
    ++iterator;
}

TEST(WTF_ListHashSet, PrependOrMoveToLastWithDuplicates)
{
    ListHashSet<int> list;

    // Add a single element twice.
    ListHashSet<int>::AddResult result = list.add(1);
    ASSERT_TRUE(result.isNewEntry);
    result = list.prependOrMoveToFirst(1);
    ASSERT_FALSE(result.isNewEntry);
    ASSERT_EQ(1u, list.size());

    list.add(2);
    list.add(3);
    ASSERT_EQ(3u, list.size());

    // Prepending 2 move it to the beginning.
    ASSERT_EQ(1, list.first());
    result = list.prependOrMoveToFirst(2);
    ASSERT_FALSE(result.isNewEntry);
    ASSERT_EQ(2, list.first());

    // Inverse the list by moving each element to the first position.
    result = list.prependOrMoveToFirst(1);
    ASSERT_FALSE(result.isNewEntry);
    result = list.prependOrMoveToFirst(2);
    ASSERT_FALSE(result.isNewEntry);
    result = list.prependOrMoveToFirst(3);
    ASSERT_FALSE(result.isNewEntry);
    ASSERT_EQ(3u, list.size());

    ListHashSet<int>::iterator iterator = list.begin();
    ASSERT_EQ(3, *iterator);
    ++iterator;
    ASSERT_EQ(2, *iterator);
    ++iterator;
    ASSERT_EQ(1, *iterator);
    ++iterator;
}

TEST(WTF_ListHashSet, ReverseIterator)
{
    ListHashSet<int> list;

    list.add(1);
    list.add(2);
    list.add(3);

    auto it = list.rbegin();
    ASSERT_EQ(3, *it);
    ++it;
    ASSERT_EQ(2, *it);
    ++it;
    ASSERT_EQ(1, *it);
    ++it;
    ASSERT_TRUE(it == list.rend());

    const auto& listHashSet = list;

    auto constIt = listHashSet.rbegin();
    ASSERT_EQ(3, *constIt);
    ++constIt;
    ASSERT_EQ(2, *constIt);
    ++constIt;
    ASSERT_EQ(1, *constIt);
    ++constIt;
    ASSERT_TRUE(constIt == listHashSet.rend());
}

TEST(WTF_ListHashSet, MoveOnly)
{
    ListHashSet<MoveOnly> list;
    list.add(MoveOnly(2));
    list.add(MoveOnly(4));

    // { 2, 4 }
    ASSERT_EQ(2U, list.first().value());
    ASSERT_EQ(4U, list.last().value());

    list.appendOrMoveToLast(MoveOnly(3));

    // { 2, 4, 3 }
    ASSERT_EQ(3U, list.last().value());

    // { 4, 3, 2 }
    list.appendOrMoveToLast(MoveOnly(2));
    ASSERT_EQ(4U, list.first().value());
    ASSERT_EQ(2U, list.last().value());

    list.prependOrMoveToFirst(MoveOnly(5));

    // { 5, 2, 4, 3 }
    ASSERT_EQ(5U, list.first().value());

    list.prependOrMoveToFirst(MoveOnly(3));

    // { 3, 5, 4, 2 }
    ASSERT_EQ(3U, list.first().value());
    ASSERT_EQ(2U, list.last().value());

    list.insertBefore(MoveOnly(4), MoveOnly(1));
    list.insertBefore(list.end(), MoveOnly(6));

    // { 3, 5, 1, 4, 2, 6 }
    ASSERT_EQ(3U, list.takeFirst().value());
    ASSERT_EQ(5U, list.takeFirst().value());
    ASSERT_EQ(1U, list.takeFirst().value());

    // { 4, 2, 6 }
    ASSERT_EQ(6U, list.takeLast().value());
    ASSERT_EQ(2U, list.takeLast().value());
    ASSERT_EQ(4U, list.takeLast().value());

    ASSERT_TRUE(list.isEmpty());
}

TEST(WTF_ListHashSet, MoveConstructor)
{
    ListHashSet<int> list;
    list.add(1);
    list.add(2);
    list.add(3);

    ASSERT_EQ(3U, list.size());
    auto iterator = list.begin();
    ASSERT_EQ(1, *iterator);
    ++iterator;
    ASSERT_EQ(2, *iterator);
    ++iterator;
    ASSERT_EQ(3, *iterator);
    ++iterator;

    ListHashSet<int> list2(WTFMove(list));
    ASSERT_EQ(3U, list2.size());
    auto iterator2 = list2.begin();
    ASSERT_EQ(1, *iterator2);
    ++iterator2;
    ASSERT_EQ(2, *iterator2);
    ++iterator2;
    ASSERT_EQ(3, *iterator2);
    ++iterator2;

    ASSERT_EQ(0U, list.size());
    ASSERT_TRUE(list.begin() == list.end());
    list.add(4);
    list.add(5);
    list.add(6);
    iterator = list.begin();
    ASSERT_EQ(4, *iterator);
    ++iterator;
    ASSERT_EQ(5, *iterator);
    ++iterator;
    ASSERT_EQ(6, *iterator);
    ++iterator;
}

TEST(WTF_ListHashSet, MoveAssignment)
{
    ListHashSet<int> list;
    list.add(1);
    list.add(2);
    list.add(3);

    ASSERT_EQ(3U, list.size());
    auto iterator = list.begin();
    ASSERT_EQ(1, *iterator);
    ++iterator;
    ASSERT_EQ(2, *iterator);
    ++iterator;
    ASSERT_EQ(3, *iterator);
    ++iterator;

    ListHashSet<int> list2;
    list2.add(10);
    list2 = (WTFMove(list));
    ASSERT_EQ(3U, list2.size());
    auto iterator2 = list2.begin();
    ASSERT_EQ(1, *iterator2);
    ++iterator2;
    ASSERT_EQ(2, *iterator2);
    ++iterator2;
    ASSERT_EQ(3, *iterator2);
    ++iterator2;

    ASSERT_EQ(0U, list.size());
    ASSERT_TRUE(list.begin() == list.end());
    list.add(4);
    list.add(5);
    list.add(6);
    iterator = list.begin();
    ASSERT_EQ(4, *iterator);
    ++iterator;
    ASSERT_EQ(5, *iterator);
    ++iterator;
    ASSERT_EQ(6, *iterator);
    ++iterator;
}

TEST(WTF_ListHashSet, InitializerListConstuctor)
{
    ListHashSet<int> set { 1, 2, 3 };
    ASSERT_EQ(3U, set.size());
    auto iterator = set.begin();
    ASSERT_EQ(1, *iterator);
    ++iterator;
    ASSERT_EQ(2, *iterator);
    ++iterator;
    ASSERT_EQ(3, *iterator);
    ++iterator;

    set = { 4, 5, 6, 7 };
    ASSERT_EQ(4U, set.size());
    iterator = set.begin();
    ASSERT_EQ(4, *iterator);
    ++iterator;
    ASSERT_EQ(5, *iterator);
    ++iterator;
    ASSERT_EQ(6, *iterator);
    ++iterator;
    ASSERT_EQ(7, *iterator);
    ++iterator;
}

TEST(WTF_ListHashSet, UniquePtrKey)
{
    ConstructorDestructorCounter::TestingScope scope;

    ListHashSet<std::unique_ptr<ConstructorDestructorCounter>> list;

    auto uniquePtr = makeUnique<ConstructorDestructorCounter>();
    list.add(WTFMove(uniquePtr));

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(0u, ConstructorDestructorCounter::destructionCount);

    list.clear();

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(1u, ConstructorDestructorCounter::destructionCount);
}

TEST(WTF_ListHashSet, UniquePtrKey_FindUsingRawPointer)
{
    ListHashSet<std::unique_ptr<int>> list;

    auto uniquePtr = makeUniqueWithoutFastMallocCheck<int>(5);
    auto ptr = uniquePtr.get();
    list.add(WTFMove(uniquePtr));

    auto it = list.find(ptr);
    ASSERT_TRUE(it != list.end());
    EXPECT_EQ(ptr, it->get());
    EXPECT_EQ(5, *it->get());
}

TEST(WTF_ListHashSet, UniquePtrKey_ContainsUsingRawPointer)
{
    ListHashSet<std::unique_ptr<int>> list;

    auto uniquePtr = makeUniqueWithoutFastMallocCheck<int>(5);
    auto ptr = uniquePtr.get();
    list.add(WTFMove(uniquePtr));

    EXPECT_EQ(true, list.contains(ptr));
}

TEST(WTF_ListHashSet, UniquePtrKey_InsertBeforeUsingRawPointer)
{
    ListHashSet<std::unique_ptr<int>> list;

    auto uniquePtrWith2 = makeUniqueWithoutFastMallocCheck<int>(2);
    auto ptrWith2 = uniquePtrWith2.get();
    auto uniquePtrWith4 = makeUniqueWithoutFastMallocCheck<int>(4);
    auto ptrWith4 = uniquePtrWith4.get();

    list.add(WTFMove(uniquePtrWith2));
    list.add(WTFMove(uniquePtrWith4));

    // { 2, 4 }
    ASSERT_EQ(ptrWith2, list.first().get());
    ASSERT_EQ(2, *list.first().get());
    ASSERT_EQ(ptrWith4, list.last().get());
    ASSERT_EQ(4, *list.last().get());

    auto uniquePtrWith3 = makeUniqueWithoutFastMallocCheck<int>(3);
    auto ptrWith3 = uniquePtrWith3.get();

    list.insertBefore(ptrWith4, WTFMove(uniquePtrWith3));
    
    // { 2, 3, 4 }
    auto firstWith2 = list.takeFirst();
    ASSERT_EQ(ptrWith2, firstWith2.get());
    ASSERT_EQ(2, *firstWith2);

    auto firstWith3 = list.takeFirst();
    ASSERT_EQ(ptrWith3, firstWith3.get());
    ASSERT_EQ(3, *firstWith3);

    auto firstWith4 = list.takeFirst();
    ASSERT_EQ(ptrWith4, firstWith4.get());
    ASSERT_EQ(4, *firstWith4);

    ASSERT_TRUE(list.isEmpty());
}

TEST(WTF_ListHashSet, UniquePtrKey_RemoveUsingRawPointer)
{
    ConstructorDestructorCounter::TestingScope scope;

    ListHashSet<std::unique_ptr<ConstructorDestructorCounter>> list;

    auto uniquePtr = makeUnique<ConstructorDestructorCounter>();
    auto* ptr = uniquePtr.get();
    list.add(WTFMove(uniquePtr));

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(0u, ConstructorDestructorCounter::destructionCount);

    bool result = list.remove(ptr);
    EXPECT_EQ(true, result);

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(1u, ConstructorDestructorCounter::destructionCount);
}

} // namespace TestWebKitAPI

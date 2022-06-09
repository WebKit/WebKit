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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/SentinelLinkedList.h>

#include <wtf/Bag.h>

namespace TestWebKitAPI {

class TestNode : public BasicRawSentinelNode<TestNode> {
public:
    TestNode(int value)
        : m_value(value)
    {
    }

    int value() const { return m_value; }

private:
    int m_value { 0 };
};

TEST(WTF_SentinelLinkedList, Basic)
{
    Bag<TestNode> bag;
    SentinelLinkedList<TestNode> list;
    EXPECT_TRUE(list.isEmpty());

    auto* first = bag.add(0);
    list.push(first);
    EXPECT_EQ(first, list.begin());
    EXPECT_EQ(0, list.begin()->value());

    auto* second = bag.add(1);
    list.push(second);
    EXPECT_EQ(second, list.begin());
    EXPECT_EQ(1, list.begin()->value());
    EXPECT_EQ(first, list.begin()->next());
    EXPECT_EQ(0, list.begin()->next()->value());
    EXPECT_EQ(list.end(), list.begin()->next()->next());

    auto* third = bag.add(2);
    first->prepend(third);
    EXPECT_EQ(second, list.begin());
    EXPECT_EQ(1, list.begin()->value());
    EXPECT_EQ(third, list.begin()->next());
    EXPECT_EQ(2, list.begin()->next()->value());
    EXPECT_EQ(first, list.begin()->next()->next());
    EXPECT_EQ(0, list.begin()->next()->next()->value());
    EXPECT_EQ(list.end(), list.begin()->next()->next()->next());

    auto* fourth = bag.add(3);
    first->append(fourth);
    EXPECT_EQ(second, list.begin());
    EXPECT_EQ(1, list.begin()->value());
    EXPECT_EQ(third, list.begin()->next());
    EXPECT_EQ(2, list.begin()->next()->value());
    EXPECT_EQ(first, list.begin()->next()->next());
    EXPECT_EQ(0, list.begin()->next()->next()->value());
    EXPECT_EQ(fourth, list.begin()->next()->next()->next());
    EXPECT_EQ(3, list.begin()->next()->next()->next()->value());
    EXPECT_EQ(list.end(), list.begin()->next()->next()->next()->next());

    EXPECT_TRUE(first->isOnList());
    EXPECT_TRUE(second->isOnList());
    EXPECT_TRUE(third->isOnList());
    EXPECT_TRUE(fourth->isOnList());

    first->remove();
    second->remove();
    third->remove();
    fourth->remove();

    EXPECT_TRUE(!first->isOnList());
    EXPECT_TRUE(!second->isOnList());
    EXPECT_TRUE(!third->isOnList());
    EXPECT_TRUE(!fourth->isOnList());

    EXPECT_TRUE(list.isEmpty());
}

TEST(WTF_SentinelLinkedList, Remove)
{
    Bag<TestNode> bag;
    SentinelLinkedList<TestNode> list;
    EXPECT_TRUE(list.isEmpty());

    for (int i = 0; i < 10; ++i)
        list.append(bag.add(i));

    int i = 0;
    while (!list.isEmpty()) {
        auto* node = list.begin();
        EXPECT_EQ(i++, node->value());
        node->remove();
    }
    EXPECT_EQ(10, i);
}

TEST(WTF_SentinelLinkedList, TakeFrom)
{
    Bag<TestNode> bag;
    SentinelLinkedList<TestNode> list;
    SentinelLinkedList<TestNode> from;
    EXPECT_TRUE(list.isEmpty());
    EXPECT_TRUE(from.isEmpty());

    list.takeFrom(from);
    EXPECT_TRUE(list.isEmpty());
    EXPECT_TRUE(from.isEmpty());

    list.append(bag.add(0));
    list.takeFrom(from);
    EXPECT_TRUE(!list.isEmpty());
    EXPECT_EQ(0, list.begin()->value());
    EXPECT_TRUE(from.isEmpty());

    from.append(bag.add(1));
    from.append(bag.add(2));
    EXPECT_TRUE(!list.isEmpty());
    EXPECT_TRUE(!from.isEmpty());
    list.takeFrom(from);
    EXPECT_TRUE(from.isEmpty());
    EXPECT_EQ(0, list.begin()->value());
    EXPECT_EQ(1, list.begin()->next()->value());
    EXPECT_EQ(2, list.begin()->next()->next()->value());
    EXPECT_EQ(list.end(), list.begin()->next()->next()->next());
}

TEST(WTF_SentinelLinkedList, ForEach)
{
    Bag<TestNode> bag;
    SentinelLinkedList<TestNode> list;
    EXPECT_TRUE(list.isEmpty());

    bool executed = false;
    list.forEach([&](auto*) {
        executed = true;
    });
    EXPECT_FALSE(executed);

    for (int i = 0; i < 10; ++i)
        list.append(bag.add(i));

    int i = 0;
    list.forEach([&](auto* node) {
        EXPECT_EQ(i++, node->value());
    });
    EXPECT_EQ(10, i);
}

} // namespace TestWebKitAPI

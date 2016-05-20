/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "Test.h"
#include <wtf/WeakPtr.h>

namespace TestWebKitAPI {

TEST(WTF_WeakPtr, Basic)
{
    int dummy = 5;
    WeakPtrFactory<int>* factory = new WeakPtrFactory<int>(&dummy);
    WeakPtr<int> weakPtr1 = factory->createWeakPtr();
    WeakPtr<int> weakPtr2 = factory->createWeakPtr();
    WeakPtr<int> weakPtr3 = factory->createWeakPtr();
    EXPECT_EQ(weakPtr1.get(), &dummy);
    EXPECT_EQ(weakPtr2.get(), &dummy);
    EXPECT_EQ(weakPtr3.get(), &dummy);
    EXPECT_TRUE(weakPtr1);
    EXPECT_TRUE(weakPtr2);
    EXPECT_TRUE(weakPtr3);
    EXPECT_TRUE(weakPtr1 == weakPtr2);
    EXPECT_TRUE(weakPtr1 == &dummy);
    EXPECT_TRUE(&dummy == weakPtr2);
    delete factory;
    EXPECT_NULL(weakPtr1.get());
    EXPECT_NULL(weakPtr2.get());
    EXPECT_NULL(weakPtr3.get());
    EXPECT_FALSE(weakPtr1);
    EXPECT_FALSE(weakPtr2);
    EXPECT_FALSE(weakPtr3);
}

TEST(WTF_WeakPtr, Assignment)
{
    int dummy = 5;
    WeakPtr<int> weakPtr;
    {
        WeakPtrFactory<int> factory(&dummy);
        EXPECT_NULL(weakPtr.get());
        weakPtr = factory.createWeakPtr();
        EXPECT_EQ(weakPtr.get(), &dummy);
    }
    EXPECT_NULL(weakPtr.get());
}

TEST(WTF_WeakPtr, MultipleFactories)
{
    int dummy1 = 5;
    int dummy2 = 7;
    WeakPtrFactory<int>* factory1 = new WeakPtrFactory<int>(&dummy1);
    WeakPtrFactory<int>* factory2 = new WeakPtrFactory<int>(&dummy2);
    WeakPtr<int> weakPtr1 = factory1->createWeakPtr();
    WeakPtr<int> weakPtr2 = factory2->createWeakPtr();
    EXPECT_EQ(weakPtr1.get(), &dummy1);
    EXPECT_EQ(weakPtr2.get(), &dummy2);
    EXPECT_TRUE(weakPtr1 != weakPtr2);
    EXPECT_TRUE(weakPtr1 != &dummy2);
    EXPECT_TRUE(&dummy1 != weakPtr2);
    delete factory1;
    EXPECT_NULL(weakPtr1.get());
    EXPECT_EQ(weakPtr2.get(), &dummy2);
    delete factory2;
    EXPECT_NULL(weakPtr2.get());
}

TEST(WTF_WeakPtr, RevokeAll)
{
    int dummy = 5;
    WeakPtrFactory<int> factory(&dummy);
    WeakPtr<int> weakPtr1 = factory.createWeakPtr();
    WeakPtr<int> weakPtr2 = factory.createWeakPtr();
    WeakPtr<int> weakPtr3 = factory.createWeakPtr();
    EXPECT_EQ(weakPtr1.get(), &dummy);
    EXPECT_EQ(weakPtr2.get(), &dummy);
    EXPECT_EQ(weakPtr3.get(), &dummy);
    factory.revokeAll();
    EXPECT_NULL(weakPtr1.get());
    EXPECT_NULL(weakPtr2.get());
    EXPECT_NULL(weakPtr3.get());
}

TEST(WTF_WeakPtr, NullFactory)
{
    WeakPtrFactory<int> factory(nullptr);
    WeakPtr<int> weakPtr = factory.createWeakPtr();
    EXPECT_NULL(weakPtr.get());
    factory.revokeAll();
    EXPECT_NULL(weakPtr.get());
}

struct Foo {
    void bar() { };
};

TEST(WTF_WeakPtr, Dereference)
{
    Foo f;
    WeakPtrFactory<Foo> factory(&f);
    WeakPtr<Foo> weakPtr = factory.createWeakPtr();
    weakPtr->bar();
}

TEST(WTF_WeakPtr, Forget)
{
    int dummy = 5;
    int dummy2 = 7;

    WeakPtrFactory<int> outerFactory(&dummy2);
    WeakPtr<int> weakPtr1, weakPtr2, weakPtr3, weakPtr4;
    {
        WeakPtrFactory<int> innerFactory(&dummy);
        weakPtr1 = innerFactory.createWeakPtr();
        weakPtr2 = innerFactory.createWeakPtr();
        weakPtr3 = innerFactory.createWeakPtr();
        EXPECT_EQ(weakPtr1.get(), &dummy);
        EXPECT_EQ(weakPtr2.get(), &dummy);
        EXPECT_EQ(weakPtr3.get(), &dummy);
        weakPtr1.clear();
        weakPtr3 = nullptr;
        EXPECT_NULL(weakPtr1.get());
        EXPECT_EQ(weakPtr2.get(), &dummy);
        EXPECT_NULL(weakPtr3.get());
        weakPtr1.clear();
        weakPtr3.clear();
        EXPECT_NULL(weakPtr1.get());
        EXPECT_EQ(weakPtr2.get(), &dummy);
        EXPECT_NULL(weakPtr3.get());
        weakPtr3 = nullptr;
        EXPECT_NULL(weakPtr1.get());
        EXPECT_EQ(weakPtr2.get(), &dummy);
        EXPECT_NULL(weakPtr3.get());
        
        weakPtr4 = weakPtr2;
        EXPECT_EQ(weakPtr2.get(), &dummy);
        EXPECT_EQ(weakPtr4.get(), &dummy);

        WeakPtr<int> weakPtr5 = weakPtr2;
        EXPECT_EQ(weakPtr2.get(), &dummy);
        EXPECT_EQ(weakPtr5.get(), &dummy);
        weakPtr5.clear();
        EXPECT_NULL(weakPtr5.get());
        EXPECT_EQ(weakPtr2.get(), &dummy);

        weakPtr4 = outerFactory.createWeakPtr();
        EXPECT_EQ(weakPtr2.get(), &dummy);
        EXPECT_EQ(weakPtr4.get(), &dummy2);
    }

    EXPECT_NULL(weakPtr1.get());
    EXPECT_NULL(weakPtr2.get());
    EXPECT_EQ(weakPtr4.get(), &dummy2);

    WeakPtr<int> weakPtr5 = weakPtr4;
    EXPECT_EQ(weakPtr4.get(), &dummy2);
    EXPECT_EQ(weakPtr5.get(), &dummy2);
    weakPtr5.clear();
    EXPECT_NULL(weakPtr5.get());
    WeakPtr<int> weakPtr6 = weakPtr5;
    EXPECT_NULL(weakPtr6.get());
    EXPECT_EQ(weakPtr5.get(), weakPtr6.get());

    WeakPtr<int> weakPtr7 = outerFactory.createWeakPtr();
    EXPECT_EQ(weakPtr7.get(), &dummy2);
    weakPtr7 = nullptr;
    EXPECT_NULL(weakPtr7.get());
}
    
} // namespace TestWebKitAPI

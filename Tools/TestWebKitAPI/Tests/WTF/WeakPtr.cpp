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
#include <wtf/HashSet.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>

static unsigned s_baseWeakReferences = 0;

namespace TestWebKitAPI {

class Base {
public:
    Base() { }

    int foo()
    {
        return 0;
    }

    auto& weakPtrFactory() const { return m_weakPtrFactory; }

private:
    WeakPtrFactory<Base> m_weakPtrFactory;
};

class Derived : public Base {
public:
    Derived() { }

    int foo()
    {
        return 1;
    }
};

}

namespace WTF {

template<>
WeakReference<TestWebKitAPI::Base>::WeakReference(TestWebKitAPI::Base* ptr)
    : m_ptr(ptr)
{
    ++s_baseWeakReferences;
}
template<>
WeakReference<TestWebKitAPI::Base>::~WeakReference()
{
    --s_baseWeakReferences;
}

}

namespace TestWebKitAPI {

TEST(WTF_WeakPtr, Basic)
{
    int dummy = 5;
    WeakPtrFactory<int>* factory = new WeakPtrFactory<int>();
    WeakPtr<int> weakPtr1 = factory->createWeakPtr(dummy);
    WeakPtr<int> weakPtr2 = factory->createWeakPtr(dummy);
    WeakPtr<int> weakPtr3 = factory->createWeakPtr(dummy);
    EXPECT_EQ(weakPtr1.get(), &dummy);
    EXPECT_EQ(weakPtr2.get(), &dummy);
    EXPECT_EQ(weakPtr3.get(), &dummy);
    EXPECT_TRUE(!!weakPtr1);
    EXPECT_TRUE(!!weakPtr2);
    EXPECT_TRUE(!!weakPtr3);
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
        WeakPtrFactory<int> factory;
        EXPECT_NULL(weakPtr.get());
        weakPtr = factory.createWeakPtr(dummy);
        EXPECT_EQ(weakPtr.get(), &dummy);
    }
    EXPECT_NULL(weakPtr.get());
}

TEST(WTF_WeakPtr, MultipleFactories)
{
    int dummy1 = 5;
    int dummy2 = 7;
    WeakPtrFactory<int>* factory1 = new WeakPtrFactory<int>();
    WeakPtrFactory<int>* factory2 = new WeakPtrFactory<int>();
    WeakPtr<int> weakPtr1 = factory1->createWeakPtr(dummy1);
    WeakPtr<int> weakPtr2 = factory2->createWeakPtr(dummy2);
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
    WeakPtrFactory<int> factory;
    WeakPtr<int> weakPtr1 = factory.createWeakPtr(dummy);
    WeakPtr<int> weakPtr2 = factory.createWeakPtr(dummy);
    WeakPtr<int> weakPtr3 = factory.createWeakPtr(dummy);
    EXPECT_EQ(weakPtr1.get(), &dummy);
    EXPECT_EQ(weakPtr2.get(), &dummy);
    EXPECT_EQ(weakPtr3.get(), &dummy);
    factory.revokeAll();
    EXPECT_NULL(weakPtr1.get());
    EXPECT_NULL(weakPtr2.get());
    EXPECT_NULL(weakPtr3.get());
}

struct Foo {
    void bar() { };
};

TEST(WTF_WeakPtr, Dereference)
{
    Foo f;
    WeakPtrFactory<Foo> factory;
    WeakPtr<Foo> weakPtr = factory.createWeakPtr(f);
    weakPtr->bar();
}

TEST(WTF_WeakPtr, Operators)
{
    Foo f;
    WeakPtrFactory<Foo> factory;
    WeakPtr<Foo> weakPtr = factory.createWeakPtr(f);

    WeakPtr<Foo> weakPtr2 = weakPtr;
    EXPECT_EQ(weakPtr2.get(), &f);

    WeakPtr<Foo> weakPtr3;
    weakPtr3 = weakPtr;
    EXPECT_EQ(weakPtr3.get(), &f);

    WeakPtr<Foo> weakPtr4 = WTFMove(weakPtr);
    EXPECT_EQ(weakPtr4.get(), &f);
    EXPECT_FALSE(weakPtr);
}

TEST(WTF_WeakPtr, Forget)
{
    int dummy = 5;
    int dummy2 = 7;

    WeakPtrFactory<int> outerFactory;
    WeakPtr<int> weakPtr1, weakPtr2, weakPtr3, weakPtr4;
    {
        WeakPtrFactory<int> innerFactory;
        weakPtr1 = innerFactory.createWeakPtr(dummy);
        weakPtr2 = innerFactory.createWeakPtr(dummy);
        weakPtr3 = innerFactory.createWeakPtr(dummy);
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

        weakPtr4 = outerFactory.createWeakPtr(dummy2);
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

    WeakPtr<int> weakPtr7 = outerFactory.createWeakPtr(dummy2);
    EXPECT_EQ(weakPtr7.get(), &dummy2);
    weakPtr7 = nullptr;
    EXPECT_NULL(weakPtr7.get());
}

TEST(WTF_WeakPtr, Downcasting)
{
    int dummy0 = 0;
    int dummy1 = 1;

    WeakPtr<Base> baseWeakPtr;
    WeakPtr<Derived> derivedWeakPtr;

    {
        Derived object;
        Derived* derivedPtr = &object;
        Base* basePtr = static_cast<Base*>(&object);

        baseWeakPtr = object.weakPtrFactory().createWeakPtr(object);
        EXPECT_EQ(basePtr->foo(), dummy0);
        EXPECT_EQ(baseWeakPtr->foo(), basePtr->foo());
        EXPECT_EQ(baseWeakPtr.get()->foo(), basePtr->foo());

        derivedWeakPtr = makeWeakPtr(object);
        EXPECT_EQ(derivedWeakPtr->foo(), dummy1);
        EXPECT_EQ(derivedWeakPtr->foo(), derivedPtr->foo());
        EXPECT_EQ(derivedWeakPtr.get()->foo(), derivedPtr->foo());

        EXPECT_EQ(baseWeakPtr.get(), derivedWeakPtr.get());
    }

    EXPECT_NULL(baseWeakPtr.get());
    EXPECT_NULL(derivedWeakPtr.get());
}

TEST(WTF_WeakPtr, DerivedConstructAndAssign)
{
    Derived derived;
    {
        WeakPtr<Derived> derivedWeakPtr = makeWeakPtr(derived);
        WeakPtr<Base> baseWeakPtr { WTFMove(derivedWeakPtr) };
        EXPECT_EQ(baseWeakPtr.get(), &derived);
        EXPECT_NULL(derivedWeakPtr.get());
    }

    {
        WeakPtr<Derived> derivedWeakPtr = makeWeakPtr(derived);
        WeakPtr<Base> baseWeakPtr { derivedWeakPtr };
        EXPECT_EQ(baseWeakPtr.get(), &derived);
        EXPECT_EQ(derivedWeakPtr.get(), &derived);
    }

    {
        WeakPtr<Derived> derivedWeakPtr = makeWeakPtr(derived);
        WeakPtr<Base> baseWeakPtr;
        baseWeakPtr = WTFMove(derivedWeakPtr);
        EXPECT_EQ(baseWeakPtr.get(), &derived);
        EXPECT_NULL(derivedWeakPtr.get());
    }

    {
        WeakPtr<Derived> derivedWeakPtr = makeWeakPtr(derived);
        WeakPtr<Base> baseWeakPtr;
        baseWeakPtr = derivedWeakPtr;
        EXPECT_EQ(baseWeakPtr.get(), &derived);
        EXPECT_EQ(derivedWeakPtr.get(), &derived);
    }
}

TEST(WTF_WeakPtr, DerivedConstructAndAssignConst)
{
    const Derived derived;
    {
        auto derivedWeakPtr = makeWeakPtr(derived);
        WeakPtr<const Base> baseWeakPtr { WTFMove(derivedWeakPtr) };
        EXPECT_EQ(baseWeakPtr.get(), &derived);
        EXPECT_NULL(derivedWeakPtr.get());
    }

    {
        auto derivedWeakPtr = makeWeakPtr(derived);
        WeakPtr<const Base> baseWeakPtr { derivedWeakPtr };
        EXPECT_EQ(baseWeakPtr.get(), &derived);
        EXPECT_EQ(derivedWeakPtr.get(), &derived);
    }

    {
        auto derivedWeakPtr = makeWeakPtr(derived);
        WeakPtr<const Base> baseWeakPtr;
        baseWeakPtr = WTFMove(derivedWeakPtr);
        EXPECT_EQ(baseWeakPtr.get(), &derived);
        EXPECT_NULL(derivedWeakPtr.get());
    }

    {
        auto derivedWeakPtr = makeWeakPtr(derived);
        WeakPtr<const Base> baseWeakPtr;
        baseWeakPtr = derivedWeakPtr;
        EXPECT_EQ(baseWeakPtr.get(), &derived);
        EXPECT_EQ(derivedWeakPtr.get(), &derived);
    }
}

template <typename T>
unsigned computeSizeOfWeakHashSet(const HashSet<WeakPtr<T>>& set)
{
    unsigned size = 0;
    for (auto& item : set) {
        UNUSED_PARAM(item);
        size++;
    }
    return size;
}

template <typename T>
unsigned computeSizeOfWeakHashSet(const WeakHashSet<T>& set)
{
    unsigned size = 0;
    for (auto& item : set) {
        UNUSED_PARAM(item);
        size++;
    }
    return size;
}

TEST(WTF_WeakPtr, WeakHashSetBasic)
{
    {
        WeakHashSet<Base> weakHashSet;
        Base object;
        EXPECT_FALSE(weakHashSet.contains(object));
        EXPECT_EQ(s_baseWeakReferences, 0u);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 0u);
        weakHashSet.add(object);
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 1u);
        EXPECT_TRUE(weakHashSet.contains(object));
        weakHashSet.add(object);
        EXPECT_TRUE(weakHashSet.contains(object));
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 1u);
        weakHashSet.checkConsistency();
    }
    EXPECT_EQ(s_baseWeakReferences, 0u);

    {
        WeakHashSet<Base> weakHashSet;
        Derived object;
        EXPECT_FALSE(weakHashSet.contains(object));
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 0u);
        EXPECT_EQ(s_baseWeakReferences, 0u);
        weakHashSet.add(object);
        EXPECT_TRUE(weakHashSet.contains(object));
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 1u);
        weakHashSet.add(object);
        EXPECT_TRUE(weakHashSet.contains(object));
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 1u);
        weakHashSet.checkConsistency();
    }
    EXPECT_EQ(s_baseWeakReferences, 0u);

    {
        WeakHashSet<Base> weakHashSet;
        {
            Base object;
            EXPECT_FALSE(weakHashSet.contains(object));
            EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 0u);
            EXPECT_EQ(s_baseWeakReferences, 0u);
            weakHashSet.add(object);
            EXPECT_TRUE(weakHashSet.contains(object));
            EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 1u);
            EXPECT_EQ(s_baseWeakReferences, 1u);
        }
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 0u);
        weakHashSet.checkConsistency();
    }
    EXPECT_EQ(s_baseWeakReferences, 0u);

    {
        WeakHashSet<Base> weakHashSet;
        {
            Base object1;
            Base object2;
            EXPECT_FALSE(weakHashSet.contains(object1));
            EXPECT_FALSE(weakHashSet.contains(object2));
            EXPECT_EQ(s_baseWeakReferences, 0u);
            EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 0u);
            weakHashSet.add(object1);
            EXPECT_TRUE(weakHashSet.contains(object1));
            EXPECT_FALSE(weakHashSet.contains(object2));
            EXPECT_EQ(s_baseWeakReferences, 1u);
            EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 1u);
            weakHashSet.add(object2);
            EXPECT_TRUE(weakHashSet.contains(object1));
            EXPECT_TRUE(weakHashSet.contains(object2));
            EXPECT_EQ(s_baseWeakReferences, 2u);
            EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 2u);
            weakHashSet.remove(object1);
            EXPECT_FALSE(weakHashSet.contains(object1));
            EXPECT_TRUE(weakHashSet.contains(object2));
            EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 1u);
        }
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 0u);
        weakHashSet.checkConsistency();
    }
    EXPECT_EQ(s_baseWeakReferences, 0u);

    {
        WeakHashSet<Base> weakHashSet;
        Base object1;
        Base object2;
        Base object3;
        EXPECT_FALSE(weakHashSet.contains(object1));
        EXPECT_FALSE(weakHashSet.contains(object2));
        EXPECT_FALSE(weakHashSet.contains(object3));
        EXPECT_EQ(s_baseWeakReferences, 0u);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 0u);
        weakHashSet.add(object1);
        weakHashSet.add(object2);
        EXPECT_TRUE(weakHashSet.contains(object1));
        EXPECT_TRUE(weakHashSet.contains(object2));
        EXPECT_FALSE(weakHashSet.contains(object3));
        EXPECT_EQ(s_baseWeakReferences, 2u);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 2u);
        weakHashSet.remove(object1);
        EXPECT_FALSE(weakHashSet.contains(object1));
        EXPECT_TRUE(weakHashSet.contains(object2));
        EXPECT_FALSE(weakHashSet.contains(object3));
        EXPECT_EQ(s_baseWeakReferences, 2u); // Because object2 holds onto WeakReference.
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 1u);
        weakHashSet.remove(object3);
        EXPECT_FALSE(weakHashSet.contains(object1));
        EXPECT_TRUE(weakHashSet.contains(object2));
        EXPECT_FALSE(weakHashSet.contains(object3));
        EXPECT_EQ(s_baseWeakReferences, 2u);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 1u);
        weakHashSet.add(object2);
        EXPECT_FALSE(weakHashSet.contains(object1));
        EXPECT_TRUE(weakHashSet.contains(object2));
        EXPECT_FALSE(weakHashSet.contains(object3));
        EXPECT_EQ(s_baseWeakReferences, 2u);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 1u);
        weakHashSet.checkConsistency();
    }
    EXPECT_EQ(s_baseWeakReferences, 0u);
}

TEST(WTF_WeakPtr, WeakHashSetExpansion)
{
    unsigned initialCapacity;
    const static unsigned maxLoadCap = 3;
    {
        WeakHashSet<Base> weakHashSet;
        Base object;
        EXPECT_EQ(s_baseWeakReferences, 0u);
        weakHashSet.add(object);
        EXPECT_EQ(s_baseWeakReferences, 1u);
        initialCapacity = weakHashSet.capacity();
    }
    EXPECT_EQ(s_baseWeakReferences, 0u);

    for (unsigned i = 0; i < 1; ++i) {
        WeakHashSet<Base> weakHashSet;
        Vector<std::unique_ptr<Base>> objects;
        Vector<std::unique_ptr<Base>> otherObjects;

        EXPECT_EQ(weakHashSet.capacity(), 0u);
        EXPECT_TRUE(initialCapacity / maxLoadCap);
        for (unsigned i = 0; i < initialCapacity / maxLoadCap; ++i) {
            auto object = std::make_unique<Base>();
            weakHashSet.add(*object);
            objects.append(WTFMove(object));
            otherObjects.append(std::make_unique<Base>());
            weakHashSet.checkConsistency();
        }
        EXPECT_EQ(s_baseWeakReferences, otherObjects.size());
        EXPECT_EQ(weakHashSet.capacity(), initialCapacity);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), objects.size());
        for (unsigned i = 0; i < otherObjects.size(); ++i) {
            EXPECT_TRUE(weakHashSet.contains(*objects[i]));
            EXPECT_FALSE(weakHashSet.contains(*otherObjects[i]));
        }
        objects.clear();
        weakHashSet.checkConsistency();
        EXPECT_EQ(s_baseWeakReferences, otherObjects.size());
        EXPECT_EQ(weakHashSet.capacity(), initialCapacity);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 0u);
        for (auto& object : otherObjects)
            EXPECT_FALSE(weakHashSet.contains(*object));
        for (auto& object : otherObjects) {
            weakHashSet.add(*object);
            weakHashSet.checkConsistency();
        }
        EXPECT_EQ(weakHashSet.capacity(), initialCapacity);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), otherObjects.size());
        for (auto& object : otherObjects)
            EXPECT_TRUE(weakHashSet.contains(*object));
    }
    EXPECT_EQ(s_baseWeakReferences, 0u);

    for (unsigned i = 0; i < 10; ++i) {
        WeakHashSet<Base> weakHashSet;
        Vector<std::unique_ptr<Base>> objects;
        EXPECT_EQ(weakHashSet.capacity(), 0u);
        unsigned objectCount = initialCapacity * 2;
        for (unsigned i = 0; i < objectCount; ++i) {
            auto object = std::make_unique<Base>();
            weakHashSet.add(*object);
            objects.append(WTFMove(object));
            weakHashSet.checkConsistency();
        }
        unsigned originalCapacity = weakHashSet.capacity();
        EXPECT_EQ(s_baseWeakReferences, objects.size());
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), objects.size());
        for (auto& object : objects)
            EXPECT_TRUE(weakHashSet.contains(*object));
        objects.clear();
        weakHashSet.checkConsistency();
        EXPECT_EQ(s_baseWeakReferences, objectCount);
        EXPECT_EQ(weakHashSet.capacity(), originalCapacity);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 0u);
    }
}

} // namespace TestWebKitAPI

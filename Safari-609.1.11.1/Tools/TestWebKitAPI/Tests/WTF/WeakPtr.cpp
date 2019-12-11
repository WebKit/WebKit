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

static unsigned s_baseWeakReferences = 0;

#define DID_CREATE_WEAK_PTR_IMPL(p) do { \
    ++s_baseWeakReferences; \
} while (0);

#define WILL_DESTROY_WEAK_PTR_IMPL(p) do { \
    --s_baseWeakReferences; \
} while (0);

#include "Test.h"
#include <wtf/HashSet.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>

namespace TestWebKitAPI {

struct Int : public CanMakeWeakPtr<Int> {
    Int(int i) : m_i(i) { }
    operator int() const { return m_i; }
    bool operator==(const Int& other) const { return m_i == other.m_i; }
    int m_i;
};

class Base : public CanMakeWeakPtr<Base> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Base() { }

    int foo()
    {
        return 0;
    }

    int dummy; // Prevent empty base class optimization, to make testing more interesting.
};

class Derived : public Base {
public:
    Derived() { }

    virtual ~Derived() { } // Force a pointer fixup when casting Base <-> Derived

    int foo()
    {
        return 1;
    }
};

}

namespace TestWebKitAPI {

TEST(WTF_WeakPtr, Basic)
{
    Int dummy(5);
    WeakPtrFactory<Int>* factory = new WeakPtrFactory<Int>();
    WeakPtr<Int> weakPtr1 = factory->createWeakPtr(dummy);
    WeakPtr<Int> weakPtr2 = factory->createWeakPtr(dummy);
    WeakPtr<Int> weakPtr3 = factory->createWeakPtr(dummy);
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
    Int dummy(5);
    WeakPtr<Int> weakPtr;
    {
        WeakPtrFactory<Int> factory;
        EXPECT_NULL(weakPtr.get());
        weakPtr = factory.createWeakPtr(dummy);
        EXPECT_EQ(weakPtr.get(), &dummy);
    }
    EXPECT_NULL(weakPtr.get());
}

TEST(WTF_WeakPtr, MultipleFactories)
{
    Int dummy1(5);
    Int dummy2(7);
    WeakPtrFactory<Int>* factory1 = new WeakPtrFactory<Int>();
    WeakPtrFactory<Int>* factory2 = new WeakPtrFactory<Int>();
    WeakPtr<Int> weakPtr1 = factory1->createWeakPtr(dummy1);
    WeakPtr<Int> weakPtr2 = factory2->createWeakPtr(dummy2);
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
    Int dummy(5);
    WeakPtrFactory<Int> factory;
    WeakPtr<Int> weakPtr1 = factory.createWeakPtr(dummy);
    WeakPtr<Int> weakPtr2 = factory.createWeakPtr(dummy);
    WeakPtr<Int> weakPtr3 = factory.createWeakPtr(dummy);
    EXPECT_EQ(weakPtr1.get(), &dummy);
    EXPECT_EQ(weakPtr2.get(), &dummy);
    EXPECT_EQ(weakPtr3.get(), &dummy);
    factory.revokeAll();
    EXPECT_NULL(weakPtr1.get());
    EXPECT_NULL(weakPtr2.get());
    EXPECT_NULL(weakPtr3.get());
}

struct Foo : public CanMakeWeakPtr<Foo> {
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
    Int dummy(5);
    Int dummy2(7);

    WeakPtrFactory<Int> outerFactory;
    WeakPtr<Int> weakPtr1, weakPtr2, weakPtr3, weakPtr4;
    {
        WeakPtrFactory<Int> innerFactory;
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

        WeakPtr<Int> weakPtr5 = weakPtr2;
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

    WeakPtr<Int> weakPtr5 = weakPtr4;
    EXPECT_EQ(weakPtr4.get(), &dummy2);
    EXPECT_EQ(weakPtr5.get(), &dummy2);
    weakPtr5.clear();
    EXPECT_NULL(weakPtr5.get());
    WeakPtr<Int> weakPtr6 = weakPtr5;
    EXPECT_NULL(weakPtr6.get());
    EXPECT_EQ(weakPtr5.get(), weakPtr6.get());

    WeakPtr<Int> weakPtr7 = outerFactory.createWeakPtr(dummy2);
    EXPECT_EQ(weakPtr7.get(), &dummy2);
    weakPtr7 = nullptr;
    EXPECT_NULL(weakPtr7.get());
}

TEST(WTF_WeakPtr, Downcasting)
{
    int dummy0(0);
    int dummy1(1);

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

TEST(WTF_WeakPtr, WeakHashSetConstObjects)
{
    {
        WeakHashSet<Base> weakHashSet;
        const Base object;
        EXPECT_FALSE(weakHashSet.contains(object));
        EXPECT_EQ(s_baseWeakReferences, 0u);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 0u);
        weakHashSet.add(object);
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 1u);
        EXPECT_TRUE(weakHashSet.contains(object));
        weakHashSet.checkConsistency();
        weakHashSet.add(object);
        EXPECT_TRUE(weakHashSet.contains(object));
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 1u);
        weakHashSet.checkConsistency();
        weakHashSet.remove(object);
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 0u);
    }

    {
        WeakHashSet<Base> weakHashSet;
        const Derived object;
        EXPECT_FALSE(weakHashSet.contains(object));
        EXPECT_EQ(s_baseWeakReferences, 0u);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 0u);
        weakHashSet.add(object);
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 1u);
        EXPECT_TRUE(weakHashSet.contains(object));
        weakHashSet.checkConsistency();
        weakHashSet.add(object);
        EXPECT_TRUE(weakHashSet.contains(object));
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 1u);
        weakHashSet.checkConsistency();
        weakHashSet.remove(object);
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 0u);
    }

    {
        WeakHashSet<Derived> weakHashSet;
        const Derived object;
        EXPECT_FALSE(weakHashSet.contains(object));
        EXPECT_EQ(s_baseWeakReferences, 0u);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 0u);
        weakHashSet.add(object);
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 1u);
        EXPECT_TRUE(weakHashSet.contains(object));
        weakHashSet.checkConsistency();
        weakHashSet.add(object);
        EXPECT_TRUE(weakHashSet.contains(object));
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 1u);
        weakHashSet.checkConsistency();
        weakHashSet.remove(object);
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), 0u);
    }
}

TEST(WTF_WeakPtr, WeakHashSetExpansion)
{
    unsigned initialCapacity;
    static constexpr unsigned maxLoadCap = 3;
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
            auto object = makeUnique<Base>();
            weakHashSet.add(*object);
            objects.append(WTFMove(object));
            otherObjects.append(makeUnique<Base>());
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
            auto object = makeUnique<Base>();
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

TEST(WTF_WeakPtr, WeakHashSetComputesEmpty)
{
    {
        WeakHashSet<Base> weakHashSet;
        {
            Base object;
            EXPECT_EQ(s_baseWeakReferences, 0u);
            weakHashSet.add(object);
            EXPECT_FALSE(weakHashSet.computesEmpty());
        }
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_TRUE(weakHashSet.computesEmpty());
    }

    {
        WeakHashSet<Base> weakHashSet;
        Base object1;
        EXPECT_EQ(s_baseWeakReferences, 0u);
        weakHashSet.add(object1);
        EXPECT_EQ(s_baseWeakReferences, 1u);
        {
            Base object2;
            weakHashSet.add(object2);
            EXPECT_FALSE(weakHashSet.computesEmpty());
        }
        EXPECT_EQ(s_baseWeakReferences, 2u);
        EXPECT_FALSE(weakHashSet.computesEmpty());
        weakHashSet.remove(object1);
        EXPECT_TRUE(weakHashSet.computesEmpty());
    }

    {
        WeakHashSet<Base> weakHashSet;
        Vector<std::unique_ptr<Base>> objects;
        auto firstObject = makeUnique<Base>();
        weakHashSet.add(*firstObject);
        do {
            auto object = makeUnique<Base>();
            weakHashSet.add(*object);
            objects.append(WTFMove(object));
        } while (weakHashSet.begin().get() == firstObject.get());

        EXPECT_EQ(s_baseWeakReferences, objects.size() + 1);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), objects.size() + 1);
        EXPECT_FALSE(weakHashSet.computesEmpty());
        firstObject = nullptr;
        EXPECT_FALSE(weakHashSet.computesEmpty());
        EXPECT_EQ(s_baseWeakReferences, objects.size() + 1);
        EXPECT_EQ(computeSizeOfWeakHashSet(weakHashSet), objects.size());
    }
}

TEST(WTF_WeakPtr, WeakHashSetComputeSize)
{
    {
        WeakHashSet<Base> weakHashSet;
        {
            Base object;
            EXPECT_EQ(s_baseWeakReferences, 0u);
            weakHashSet.add(object);
            EXPECT_EQ(s_baseWeakReferences, 1u);
            EXPECT_EQ(weakHashSet.computeSize(), 1u);
            weakHashSet.checkConsistency();
        }
        EXPECT_TRUE(weakHashSet.computesEmpty());
        EXPECT_EQ(weakHashSet.computeSize(), 0u);
        EXPECT_EQ(s_baseWeakReferences, 0u);
        EXPECT_TRUE(weakHashSet.computesEmpty());
        weakHashSet.checkConsistency();
    }

    {
        WeakHashSet<Base> weakHashSet;
        {
            Base object1;
            EXPECT_EQ(s_baseWeakReferences, 0u);
            weakHashSet.add(object1);
            EXPECT_EQ(s_baseWeakReferences, 1u);
            {
                Base object2;
                weakHashSet.add(object2);
                EXPECT_EQ(s_baseWeakReferences, 2u);
                EXPECT_EQ(weakHashSet.computeSize(), 2u);
                weakHashSet.checkConsistency();
            }
            EXPECT_EQ(s_baseWeakReferences, 2u);
            EXPECT_EQ(weakHashSet.computeSize(), 1u);
            EXPECT_EQ(s_baseWeakReferences, 1u);
            weakHashSet.checkConsistency();
            weakHashSet.remove(object1);
            EXPECT_EQ(s_baseWeakReferences, 1u);
            EXPECT_EQ(weakHashSet.computeSize(), 0u);
            EXPECT_EQ(s_baseWeakReferences, 1u);
            weakHashSet.checkConsistency();
        }
        EXPECT_EQ(s_baseWeakReferences, 0u);
        weakHashSet.checkConsistency();
    }

    while (1) {
        WeakHashSet<Base> weakHashSet;
        auto firstObject = makeUnique<Base>();
        auto lastObject = makeUnique<Base>();
        weakHashSet.add(*firstObject);
        weakHashSet.add(*lastObject);
        if (weakHashSet.begin().get() != firstObject.get())
            continue;
        EXPECT_EQ(s_baseWeakReferences, 2u);
        EXPECT_EQ(weakHashSet.computeSize(), 2u);
        EXPECT_EQ(s_baseWeakReferences, 2u);
        weakHashSet.checkConsistency();
        firstObject = nullptr;
        EXPECT_EQ(weakHashSet.computeSize(), 1u);
        EXPECT_EQ(s_baseWeakReferences, 1u);
        weakHashSet.checkConsistency();
        lastObject = nullptr;
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(weakHashSet.computeSize(), 0u);
        EXPECT_EQ(s_baseWeakReferences, 0u);
        weakHashSet.checkConsistency();
        break;
    }

    {
        WeakHashSet<Base> weakHashSet;
        Vector<std::unique_ptr<Base>> objects;
        auto nonFirstObject = makeUnique<Base>();
        weakHashSet.add(*nonFirstObject);
        do {
            auto object = makeUnique<Base>();
            weakHashSet.add(*object);
            objects.append(WTFMove(object));
        } while (weakHashSet.begin().get() == nonFirstObject.get());

        unsigned objectsCount = objects.size();
        EXPECT_EQ(s_baseWeakReferences, objectsCount + 1);
        EXPECT_EQ(weakHashSet.computeSize(), objectsCount + 1);
        EXPECT_EQ(s_baseWeakReferences, objectsCount + 1);
        weakHashSet.checkConsistency();
        nonFirstObject = nullptr;
        EXPECT_EQ(weakHashSet.computeSize(), objectsCount);
        EXPECT_EQ(s_baseWeakReferences, objectsCount);
        weakHashSet.checkConsistency();
        objects.clear();
        EXPECT_EQ(s_baseWeakReferences, objectsCount);
        EXPECT_EQ(weakHashSet.computeSize(), 0u);
        EXPECT_EQ(s_baseWeakReferences, 0u);
    }
}

} // namespace TestWebKitAPI

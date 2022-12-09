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
#include <thread>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakHashSet.h>

namespace TestWebKitAPI {

static unsigned s_baseWeakReferences = 0;

class WeakPtrImplWithCounter final : public WTF::WeakPtrImplBase<WeakPtrImplWithCounter> {
public:
    template<typename T>
    WeakPtrImplWithCounter(T* ptr)
        : WTF::WeakPtrImplBase<WeakPtrImplWithCounter>(ptr)
    {
        increment();
    }

    ~WeakPtrImplWithCounter()
    {
        decrement();
    }

    static void increment() { ++s_baseWeakReferences; }
    static void decrement() { --s_baseWeakReferences; }
};

template<typename T> using CanMakeWeakPtr = WTF::CanMakeWeakPtr<T, WeakPtrFactoryInitialization::Lazy, WeakPtrImplWithCounter>;
template<typename T, typename U> using WeakHashMap = WTF::WeakHashMap<T, U, WeakPtrImplWithCounter>;
template<typename T> using WeakHashSet = WTF::WeakHashSet<T, WeakPtrImplWithCounter>;
template<typename T> using WeakPtr = WTF::WeakPtr<T, WeakPtrImplWithCounter>;
template<typename T> using WeakPtrFactory = WTF::WeakPtrFactory<T, WeakPtrImplWithCounter>;

// FIXME: Drop when we support C++20. C++17 does not support template parameter deduction for aliases and WeakPtr is an alias in this file.
template<typename T, typename = std::enable_if_t<!WTF::IsSmartPtr<T>::value>> inline auto makeWeakPtr(T& object, EnableWeakPtrThreadingAssertions enableWeakPtrThreadingAssertions = EnableWeakPtrThreadingAssertions::Yes)
{
    return object.weakPtrFactory().template createWeakPtr<T>(object, enableWeakPtrThreadingAssertions);
}

// FIXME: Drop when we support C++20. C++17 does not support template parameter deduction for aliases and WeakPtr is an alias in this file.
template<typename T, typename = std::enable_if_t<!WTF::IsSmartPtr<T>::value>> inline auto makeWeakPtr(T* ptr, EnableWeakPtrThreadingAssertions enableWeakPtrThreadingAssertions = EnableWeakPtrThreadingAssertions::Yes) -> decltype(makeWeakPtr(*ptr))
{
    if (!ptr)
        return { };
    return makeWeakPtr(*ptr, enableWeakPtrThreadingAssertions);
}

// FIXME: Drop when we support C++20. C++17 does not support template parameter deduction for aliases and WeakPtr is an alias in this file.
template<typename T, typename = std::enable_if_t<!WTF::IsSmartPtr<T>::value>> inline auto makeWeakPtr(const Ref<T>& object, EnableWeakPtrThreadingAssertions enableWeakPtrThreadingAssertions = EnableWeakPtrThreadingAssertions::Yes)
{
    return makeWeakPtr(object.get(), enableWeakPtrThreadingAssertions);
}

// FIXME: Drop when we support C++20. C++17 does not support template parameter deduction for aliases and WeakPtr is an alias in this file.
template<typename T, typename = std::enable_if_t<!WTF::IsSmartPtr<T>::value>> inline auto makeWeakPtr(const RefPtr<T>& object, EnableWeakPtrThreadingAssertions enableWeakPtrThreadingAssertions = EnableWeakPtrThreadingAssertions::Yes)
{
    return makeWeakPtr(object.get(), enableWeakPtrThreadingAssertions);
}

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

    virtual ~Base() = default;

    int foo()
    {
        return 0;
    }

    int dummy; // Prevent empty base class optimization, to make testing more interesting.
};

class Derived : public Base {
public:
    Derived() { }

    ~Derived() override { } // Force a pointer fixup when casting Base <-> Derived

    int foo()
    {
        return 1;
    }
};

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

class BaseObjectWithRefAndWeakPtr : public RefCounted<BaseObjectWithRefAndWeakPtr>, public CanMakeWeakPtr<BaseObjectWithRefAndWeakPtr> {
public:
    static Ref<BaseObjectWithRefAndWeakPtr> create() { return adoptRef(*new BaseObjectWithRefAndWeakPtr()); }

    virtual ~BaseObjectWithRefAndWeakPtr() = default;
    void someFunction() { }

protected:
    BaseObjectWithRefAndWeakPtr() = default;
};

class DerivedObjectWithRefAndWeakPtr : public BaseObjectWithRefAndWeakPtr {
public:
    static Ref<DerivedObjectWithRefAndWeakPtr> create() { return adoptRef(*new DerivedObjectWithRefAndWeakPtr()); }

private:
    DerivedObjectWithRefAndWeakPtr() = default;
    virtual ~DerivedObjectWithRefAndWeakPtr() = default;
};

TEST(WTF_WeakPtr, MakeWeakPtrTakesRef)
{
    Ref baseObject = BaseObjectWithRefAndWeakPtr::create();
    EXPECT_EQ(baseObject->refCount(), 1U);
    EXPECT_EQ(baseObject->weakPtrFactory().weakPtrCount(), 0U);
    {
        auto baseObjectWeakPtr = makeWeakPtr(baseObject);
        EXPECT_EQ(baseObject->refCount(), 1U);
        EXPECT_EQ(baseObject->weakPtrFactory().weakPtrCount(), 1U);
        EXPECT_EQ(baseObjectWeakPtr.get(), baseObject.ptr());
    }
    EXPECT_EQ(baseObject->refCount(), 1U);
    EXPECT_EQ(baseObject->weakPtrFactory().weakPtrCount(), 0U);

    WeakPtr<BaseObjectWithRefAndWeakPtr> baseWeakPtr;
    {
        Ref derivedObject = DerivedObjectWithRefAndWeakPtr::create();
        EXPECT_EQ(derivedObject->refCount(), 1U);
        EXPECT_EQ(derivedObject->weakPtrFactory().weakPtrCount(), 0U);
        {
            WeakPtr<DerivedObjectWithRefAndWeakPtr> derivedObjectWeakPtr = makeWeakPtr(derivedObject);
            EXPECT_EQ(derivedObject->refCount(), 1U);
            EXPECT_EQ(derivedObject->weakPtrFactory().weakPtrCount(), 1U);
            EXPECT_EQ(derivedObjectWeakPtr.get(), derivedObject.ptr());
        }
        EXPECT_EQ(derivedObject->refCount(), 1U);
        EXPECT_EQ(derivedObject->weakPtrFactory().weakPtrCount(), 0U);
        {
            Ref<BaseObjectWithRefAndWeakPtr> baseRefPtr = derivedObject;
            EXPECT_EQ(derivedObject->refCount(), 2U);
            EXPECT_EQ(derivedObject->weakPtrFactory().weakPtrCount(), 0U);
            baseWeakPtr = makeWeakPtr(baseRefPtr);
            EXPECT_EQ(derivedObject->refCount(), 2U);
            EXPECT_EQ(derivedObject->weakPtrFactory().weakPtrCount(), 1U);
            EXPECT_EQ(baseWeakPtr.get(), derivedObject.ptr());
        }
        EXPECT_EQ(derivedObject->refCount(), 1U);
        EXPECT_EQ(derivedObject->weakPtrFactory().weakPtrCount(), 1U);
        EXPECT_EQ(baseWeakPtr.get(), derivedObject.ptr());
    }
    EXPECT_EQ(baseWeakPtr.get(), nullptr);
}

TEST(WTF_WeakPtr, MakeWeakPtrTakesRefPtr)
{
    RefPtr<BaseObjectWithRefAndWeakPtr> baseObject = BaseObjectWithRefAndWeakPtr::create();
    EXPECT_EQ(baseObject->refCount(), 1U);
    EXPECT_EQ(baseObject->weakPtrFactory().weakPtrCount(), 0U);
    {
        auto baseObjectWeakPtr = makeWeakPtr(baseObject);
        EXPECT_EQ(baseObject->refCount(), 1U);
        EXPECT_EQ(baseObject->weakPtrFactory().weakPtrCount(), 1U);
        EXPECT_EQ(baseObjectWeakPtr.get(), baseObject.get());
        baseObject = nullptr;
        EXPECT_EQ(baseObjectWeakPtr.get(), nullptr);
    }

    RefPtr<DerivedObjectWithRefAndWeakPtr> derivedObject = DerivedObjectWithRefAndWeakPtr::create();
    EXPECT_EQ(derivedObject->refCount(), 1U);
    EXPECT_EQ(derivedObject->weakPtrFactory().weakPtrCount(), 0U);
    {
        WeakPtr<DerivedObjectWithRefAndWeakPtr> derivedObjectWeakPtr = makeWeakPtr(derivedObject);
        EXPECT_EQ(derivedObject->refCount(), 1U);
        EXPECT_EQ(derivedObject->weakPtrFactory().weakPtrCount(), 1U);
        EXPECT_EQ(derivedObjectWeakPtr.get(), derivedObject.get());

        WeakPtr<BaseObjectWithRefAndWeakPtr> baseObjectWeakPtr = makeWeakPtr<BaseObjectWithRefAndWeakPtr>(derivedObject);
        EXPECT_EQ(derivedObject->refCount(), 1U);
        EXPECT_EQ(derivedObject->weakPtrFactory().weakPtrCount(), 2U);
        EXPECT_EQ(baseObjectWeakPtr.get(), derivedObject.get());
    }
    EXPECT_EQ(derivedObject->refCount(), 1U);
    EXPECT_EQ(derivedObject->weakPtrFactory().weakPtrCount(), 0U);
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

    {
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

template <typename T, typename U>
unsigned computeSizeOfWeakHashMap(const WeakHashMap<T, U>& map)
{
    unsigned size = 0;
    for (auto item : map) {
        UNUSED_PARAM(item);
        size++;
    }
    return size;
}

struct ValueObject : public RefCounted<ValueObject> {
public:

    static Ref<ValueObject> create(int value) { return adoptRef(*new ValueObject(value)); }

    ~ValueObject() { ASSERT(s_count); --s_count; }

    int value;

    static unsigned s_count;

private:
    ValueObject(unsigned short value)
        : value(value)
    {
        ++s_count;
    }
};

unsigned ValueObject::s_count = 0;

TEST(WTF_WeakPtr, WeakHashMapBasic)
{
    {
        WeakHashMap<Base, int> weakHashMap;
        Base object;
        EXPECT_FALSE(weakHashMap.contains(object));
        EXPECT_EQ(s_baseWeakReferences, 0u);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 0u);
        weakHashMap.set(object, 34);
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 1u);
        EXPECT_TRUE(weakHashMap.contains(object));
        EXPECT_EQ(weakHashMap.get(object), 34);
        weakHashMap.add(object, 12);
        EXPECT_TRUE(weakHashMap.contains(object));
        EXPECT_EQ(weakHashMap.get(object), 34);
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 1u);
        weakHashMap.set(object, 7);
        EXPECT_TRUE(weakHashMap.contains(object));
        EXPECT_EQ(weakHashMap.get(object), 7);
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 1u);
        weakHashMap.checkConsistency();
    }
    EXPECT_EQ(s_baseWeakReferences, 0u);

    {
        WeakHashMap<Base, int> weakHashMap;
        {
            Base object;
            EXPECT_FALSE(weakHashMap.contains(object));
            EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 0u);
            EXPECT_EQ(s_baseWeakReferences, 0u);
            weakHashMap.add(object, 7);
            EXPECT_TRUE(weakHashMap.contains(object));
            EXPECT_EQ(weakHashMap.get(object), 7);
            EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 1u);
            EXPECT_EQ(s_baseWeakReferences, 1u);
        }
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 0u);
        weakHashMap.checkConsistency();
    }
    EXPECT_EQ(s_baseWeakReferences, 0u);

    {
        WeakHashMap<Base, int> weakHashMap;
        {
            Base object1;
            Base object2;
            EXPECT_FALSE(weakHashMap.contains(object1));
            EXPECT_EQ(weakHashMap.get(object1), 0);
            EXPECT_FALSE(weakHashMap.contains(object2));
            EXPECT_EQ(weakHashMap.get(object2), 0);
            EXPECT_EQ(s_baseWeakReferences, 0u);
            EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 0u);
            weakHashMap.add(object1, 3);
            EXPECT_TRUE(weakHashMap.contains(object1));
            EXPECT_EQ(weakHashMap.get(object1), 3);
            EXPECT_FALSE(weakHashMap.contains(object2));
            EXPECT_EQ(weakHashMap.get(object2), 0);
            EXPECT_EQ(s_baseWeakReferences, 1u);
            EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 1u);
            weakHashMap.add(object2, 24);
            EXPECT_TRUE(weakHashMap.contains(object1));
            EXPECT_EQ(weakHashMap.get(object1), 3);
            EXPECT_TRUE(weakHashMap.contains(object2));
            EXPECT_EQ(weakHashMap.get(object2), 24);
            EXPECT_EQ(s_baseWeakReferences, 2u);
            EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 2u);
            weakHashMap.remove(object1);
            EXPECT_FALSE(weakHashMap.contains(object1));
            EXPECT_EQ(weakHashMap.get(object1), 0);
            EXPECT_TRUE(weakHashMap.contains(object2));
            EXPECT_EQ(weakHashMap.get(object2), 24);
            EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 1u);
        }
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 0u);
        weakHashMap.checkConsistency();
    }
    EXPECT_EQ(s_baseWeakReferences, 0u);

    {
        WeakHashMap<Base, Ref<ValueObject>> weakHashMap;
        Base object1;
        Base object2;
        Base object3;
        EXPECT_FALSE(weakHashMap.contains(object1));
        EXPECT_EQ(weakHashMap.get(object1), nullptr);
        EXPECT_FALSE(weakHashMap.contains(object2));
        EXPECT_EQ(weakHashMap.get(object2), nullptr);
        EXPECT_FALSE(weakHashMap.contains(object3));
        EXPECT_EQ(weakHashMap.get(object3), nullptr);
        EXPECT_EQ(s_baseWeakReferences, 0u);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 0u);
        weakHashMap.add(object1, ValueObject::create(100));
        weakHashMap.add(object2, ValueObject::create(200));
        EXPECT_EQ(s_baseWeakReferences, 2u);
        EXPECT_EQ(ValueObject::s_count, 2u);
        EXPECT_TRUE(weakHashMap.contains(object1));
        EXPECT_EQ(weakHashMap.get(object1)->value, 100);
        EXPECT_TRUE(weakHashMap.contains(object2));
        EXPECT_EQ(weakHashMap.get(object2)->value, 200);
        EXPECT_FALSE(weakHashMap.contains(object3));
        EXPECT_EQ(weakHashMap.get(object3), nullptr);
        EXPECT_EQ(s_baseWeakReferences, 2u);
        EXPECT_EQ(ValueObject::s_count, 2u);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 2u);
        weakHashMap.remove(object1);
        EXPECT_FALSE(weakHashMap.contains(object1));
        EXPECT_EQ(weakHashMap.get(object1), nullptr);
        EXPECT_TRUE(weakHashMap.contains(object2));
        EXPECT_EQ(weakHashMap.get(object2)->value, 200);
        EXPECT_FALSE(weakHashMap.contains(object3));
        EXPECT_EQ(weakHashMap.get(object3), nullptr);
        EXPECT_EQ(s_baseWeakReferences, 2u); // Because object2 holds onto WeakReference.
        EXPECT_EQ(ValueObject::s_count, 1u);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 1u);
        weakHashMap.remove(object3);
        EXPECT_FALSE(weakHashMap.contains(object1));
        EXPECT_EQ(weakHashMap.get(object1), nullptr);
        EXPECT_TRUE(weakHashMap.contains(object2));
        EXPECT_EQ(weakHashMap.get(object2)->value, 200);
        EXPECT_FALSE(weakHashMap.contains(object3));
        EXPECT_EQ(weakHashMap.get(object3), nullptr);
        EXPECT_EQ(s_baseWeakReferences, 2u);
        EXPECT_EQ(ValueObject::s_count, 1u);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 1u);
        weakHashMap.add(object2, ValueObject::create(210));
        EXPECT_FALSE(weakHashMap.contains(object1));
        EXPECT_EQ(weakHashMap.get(object1), nullptr);
        EXPECT_TRUE(weakHashMap.contains(object2));
        EXPECT_EQ(weakHashMap.get(object2)->value, 200);
        EXPECT_FALSE(weakHashMap.contains(object3));
        EXPECT_EQ(weakHashMap.get(object3), nullptr);
        EXPECT_EQ(s_baseWeakReferences, 2u);
        EXPECT_EQ(ValueObject::s_count, 1u);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 1u);
        weakHashMap.set(object2, ValueObject::create(220));
        EXPECT_FALSE(weakHashMap.contains(object1));
        EXPECT_EQ(weakHashMap.get(object1), nullptr);
        EXPECT_TRUE(weakHashMap.contains(object2));
        EXPECT_EQ(weakHashMap.get(object2)->value, 220);
        EXPECT_FALSE(weakHashMap.contains(object3));
        EXPECT_EQ(weakHashMap.get(object3), nullptr);
        EXPECT_EQ(s_baseWeakReferences, 2u);
        EXPECT_EQ(ValueObject::s_count, 1u);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 1u);
        weakHashMap.remove(object2);
        EXPECT_FALSE(weakHashMap.contains(object1));
        EXPECT_EQ(weakHashMap.get(object1), nullptr);
        EXPECT_FALSE(weakHashMap.contains(object2));
        EXPECT_EQ(weakHashMap.get(object2), nullptr);
        EXPECT_FALSE(weakHashMap.contains(object3));
        EXPECT_EQ(weakHashMap.get(object3), nullptr);
        EXPECT_EQ(s_baseWeakReferences, 2u);
        EXPECT_EQ(ValueObject::s_count, 0u);
        weakHashMap.checkConsistency();
    }
    EXPECT_EQ(s_baseWeakReferences, 0u);
    EXPECT_EQ(ValueObject::s_count, 0u);
}

TEST(WTF_WeakPtr, WeakHashMapConstObjects)
{
    {
        WeakHashMap<Base, Ref<ValueObject>> weakHashMap;
        const Base object;
        EXPECT_FALSE(weakHashMap.contains(object));
        EXPECT_EQ(s_baseWeakReferences, 0u);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 0u);
        weakHashMap.add(object, ValueObject::create(3));
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 1u);
        EXPECT_TRUE(weakHashMap.contains(object));
        EXPECT_EQ(weakHashMap.get(object)->value, 3);
        weakHashMap.checkConsistency();
        weakHashMap.add(object, ValueObject::create(7));
        EXPECT_TRUE(weakHashMap.contains(object));
        EXPECT_EQ(weakHashMap.get(object)->value, 3);
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 1u);
        weakHashMap.checkConsistency();
        weakHashMap.set(object, ValueObject::create(11));
        EXPECT_TRUE(weakHashMap.contains(object));
        EXPECT_EQ(weakHashMap.get(object)->value, 11);
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 1u);
        weakHashMap.checkConsistency();
        weakHashMap.remove(object);
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 0u);
        EXPECT_FALSE(weakHashMap.contains(object));
        EXPECT_EQ(weakHashMap.get(object), nullptr);
    }

    {
        WeakHashMap<Base, String> weakHashMap;
        const Derived object;
        EXPECT_FALSE(weakHashMap.contains(object));
        EXPECT_TRUE(weakHashMap.get(object).isNull());
        EXPECT_EQ(s_baseWeakReferences, 0u);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 0u);
        weakHashMap.add(object, "hello"_s);
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 1u);
        EXPECT_TRUE(weakHashMap.contains(object));
        EXPECT_STREQ(weakHashMap.get(object).utf8().data(), "hello");
        weakHashMap.checkConsistency();
        weakHashMap.add(object, "world"_s);
        EXPECT_TRUE(weakHashMap.contains(object));
        EXPECT_STREQ(weakHashMap.get(object).utf8().data(), "hello");
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 1u);
        weakHashMap.checkConsistency();
        weakHashMap.set(object, "WebKit"_s);
        EXPECT_TRUE(weakHashMap.contains(object));
        EXPECT_STREQ(weakHashMap.get(object).utf8().data(), "WebKit");
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 1u);
        weakHashMap.checkConsistency();
        weakHashMap.remove(object);
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 0u);
        EXPECT_FALSE(weakHashMap.contains(object));
        EXPECT_TRUE(weakHashMap.get(object).isNull());
    }

    {
        WeakHashMap<Base, int> weakHashMap;
        const Derived object;
        EXPECT_FALSE(weakHashMap.contains(object));
        EXPECT_EQ(weakHashMap.get(object), 0);
        EXPECT_EQ(s_baseWeakReferences, 0u);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 0u);
        weakHashMap.checkConsistency();
        weakHashMap.add(object, 3);
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 1u);
        EXPECT_TRUE(weakHashMap.contains(object));
        EXPECT_EQ(weakHashMap.get(object), 3);
        weakHashMap.checkConsistency();
        weakHashMap.add(object, 7);
        EXPECT_TRUE(weakHashMap.contains(object));
        EXPECT_EQ(weakHashMap.get(object), 3);
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 1u);
        weakHashMap.checkConsistency();
        weakHashMap.set(object, 11);
        EXPECT_TRUE(weakHashMap.contains(object));
        EXPECT_EQ(weakHashMap.get(object), 11);
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 1u);
        weakHashMap.checkConsistency();
        weakHashMap.remove(object);
        EXPECT_FALSE(weakHashMap.contains(object));
        EXPECT_EQ(weakHashMap.get(object), 0);
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 0u);
    }
}

TEST(WTF_WeakPtr, WeakHashMapRemoveIterator)
{
    WeakHashMap<Base, int> weakHashMap;
    Vector<std::unique_ptr<Base>> objects;
    for (unsigned i = 0; i < 13; ++i) {
        auto object = makeUnique<Base>();
        weakHashMap.add(*object, 0);
        objects.append(WTFMove(object));
    }
    while (!objects.isEmpty()) {
        auto it = weakHashMap.find(*objects.last());
        objects.remove(0);
        weakHashMap.remove(it);
        weakHashMap.checkConsistency();
    }
}

TEST(WTF_WeakPtr, WeakHashMapExpansion)
{
    unsigned initialCapacity;
    static constexpr unsigned maxLoadCap = 3;
    {
        WeakHashMap<Base, int> weakHashMap;
        Base object;
        EXPECT_EQ(s_baseWeakReferences, 0u);
        weakHashMap.add(object, 1);
        EXPECT_EQ(s_baseWeakReferences, 1u);
        initialCapacity = weakHashMap.capacity();
    }
    EXPECT_EQ(s_baseWeakReferences, 0u);

    for (unsigned testCount = 0; testCount < 10; ++testCount) {
        WeakHashMap<Base, unsigned> weakHashMap;
        Vector<std::unique_ptr<Base>> objects;
        Vector<std::unique_ptr<Base>> otherObjects;

        EXPECT_EQ(weakHashMap.capacity(), 0u);
        EXPECT_TRUE(initialCapacity / maxLoadCap);
        for (unsigned i = 0; i < initialCapacity / maxLoadCap; ++i) {
            auto object = makeUnique<Base>();
            weakHashMap.add(*object, i);
            objects.append(WTFMove(object));
            otherObjects.append(makeUnique<Base>());
            weakHashMap.checkConsistency();
        }
        EXPECT_EQ(s_baseWeakReferences, otherObjects.size());
        EXPECT_EQ(weakHashMap.capacity(), initialCapacity);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), objects.size());
        for (unsigned i = 0; i < otherObjects.size(); ++i) {
            EXPECT_TRUE(weakHashMap.contains(*objects[i]));
            EXPECT_EQ(weakHashMap.get(*objects[i]), i);
            EXPECT_FALSE(weakHashMap.contains(*otherObjects[i]));
        }
        objects.clear();
        weakHashMap.checkConsistency();
        EXPECT_EQ(s_baseWeakReferences, otherObjects.size());
        EXPECT_EQ(weakHashMap.capacity(), initialCapacity);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 0u);
        for (auto& object : otherObjects)
            EXPECT_FALSE(weakHashMap.contains(*object));
        unsigned i = 100;
        for (auto& object : otherObjects) {
            weakHashMap.add(*object, i);
            weakHashMap.checkConsistency();
            ++i;
        }
        EXPECT_EQ(weakHashMap.capacity(), initialCapacity);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), otherObjects.size());
        i = 100;
        for (auto& object : otherObjects) {
            EXPECT_TRUE(weakHashMap.contains(*object));
            EXPECT_EQ(weakHashMap.get(*object), i);
            ++i;
        }
    }
    EXPECT_EQ(s_baseWeakReferences, 0u);

    for (unsigned testCount = 0; testCount < 10; ++testCount) {
        WeakHashMap<Base, Ref<ValueObject>> weakHashMap;
        Vector<std::unique_ptr<Base>> objects;
        EXPECT_EQ(weakHashMap.capacity(), 0u);
        unsigned objectCount = initialCapacity * 2;
        for (unsigned i = 0; i < objectCount; ++i) {
            auto object = makeUnique<Base>();
            weakHashMap.set(*object, ValueObject::create(100 + i));
            objects.append(WTFMove(object));
            weakHashMap.checkConsistency();
        }
        unsigned originalCapacity = weakHashMap.capacity();
        EXPECT_EQ(s_baseWeakReferences, objects.size());
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), objects.size());

        int i = 100;
        for (auto& object : objects) {
            EXPECT_TRUE(weakHashMap.contains(*object));
            EXPECT_EQ(weakHashMap.get(*object)->value, i);
            ++i;
        }

        objects.clear();
        weakHashMap.checkConsistency();
        EXPECT_EQ(s_baseWeakReferences, objectCount);
        EXPECT_EQ(weakHashMap.capacity(), originalCapacity);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), 0u);
    }
}

TEST(WTF_WeakPtr, WeakHashMapIsEmptyIgnoringNullReferences)
{
    {
        WeakHashMap<Base, int> weakHashMap;
        {
            Base object;
            EXPECT_EQ(s_baseWeakReferences, 0u);
            EXPECT_FALSE(weakHashMap.hasNullReferences());
            EXPECT_TRUE(weakHashMap.isEmptyIgnoringNullReferences());
            weakHashMap.add(object, 1);
            EXPECT_EQ(s_baseWeakReferences, 1u);
            EXPECT_FALSE(weakHashMap.hasNullReferences());
            EXPECT_FALSE(weakHashMap.isEmptyIgnoringNullReferences());
            weakHashMap.checkConsistency();
            weakHashMap.set(object, 2);
            EXPECT_EQ(s_baseWeakReferences, 1u);
            EXPECT_FALSE(weakHashMap.hasNullReferences());
            EXPECT_FALSE(weakHashMap.isEmptyIgnoringNullReferences());
            weakHashMap.checkConsistency();
            weakHashMap.add(object, 3);
            EXPECT_EQ(s_baseWeakReferences, 1u);
            EXPECT_FALSE(weakHashMap.hasNullReferences());
            EXPECT_FALSE(weakHashMap.isEmptyIgnoringNullReferences());
            weakHashMap.checkConsistency();
        }
        EXPECT_TRUE(weakHashMap.hasNullReferences());
        EXPECT_TRUE(weakHashMap.isEmptyIgnoringNullReferences());
        EXPECT_EQ(s_baseWeakReferences, 0u);
        EXPECT_FALSE(weakHashMap.hasNullReferences());
        weakHashMap.checkConsistency();
    }

    {
        WeakHashMap<Base, int> weakHashMap;
        Base object1;
        EXPECT_EQ(s_baseWeakReferences, 0u);
        EXPECT_FALSE(weakHashMap.hasNullReferences());
        EXPECT_TRUE(weakHashMap.isEmptyIgnoringNullReferences());
        weakHashMap.checkConsistency();
        weakHashMap.add(object1, 100);
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_FALSE(weakHashMap.hasNullReferences());
        EXPECT_FALSE(weakHashMap.isEmptyIgnoringNullReferences());
        weakHashMap.checkConsistency();
        {
            Base object2;
            weakHashMap.add(object2, 200);
            EXPECT_EQ(s_baseWeakReferences, 2u);
            EXPECT_FALSE(weakHashMap.hasNullReferences());
            EXPECT_FALSE(weakHashMap.isEmptyIgnoringNullReferences());
            weakHashMap.checkConsistency();
        }
        EXPECT_EQ(s_baseWeakReferences, 2u);
        EXPECT_TRUE(weakHashMap.hasNullReferences());
        EXPECT_FALSE(weakHashMap.isEmptyIgnoringNullReferences());
        weakHashMap.checkConsistency();
        weakHashMap.remove(object1);
        EXPECT_TRUE(weakHashMap.isEmptyIgnoringNullReferences());
        weakHashMap.checkConsistency();
    }

    {
        WeakHashMap<Base, Ref<ValueObject>> weakHashMap;
        Vector<std::unique_ptr<Base>> objects;
        auto firstObject = makeUnique<Base>();
        int value = 1;
        weakHashMap.set(*firstObject, ValueObject::create(value++));
        do {
            auto object = makeUnique<Base>();
            weakHashMap.set(*object, ValueObject::create(value++));
            objects.append(WTFMove(object));
        } while (&weakHashMap.begin()->key == firstObject.get());

        EXPECT_EQ(ValueObject::s_count, objects.size() + 1);
        EXPECT_EQ(s_baseWeakReferences, objects.size() + 1);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), objects.size() + 1);
        EXPECT_FALSE(weakHashMap.hasNullReferences());
        EXPECT_FALSE(weakHashMap.isEmptyIgnoringNullReferences());
        firstObject = nullptr;
        EXPECT_TRUE(weakHashMap.hasNullReferences());
        EXPECT_FALSE(weakHashMap.isEmptyIgnoringNullReferences());
        EXPECT_EQ(s_baseWeakReferences, objects.size() + 1);
        EXPECT_EQ(computeSizeOfWeakHashMap(weakHashMap), objects.size());
        objects.clear();
        EXPECT_TRUE(weakHashMap.hasNullReferences());
        EXPECT_TRUE(weakHashMap.isEmptyIgnoringNullReferences());
        weakHashMap.clear();
        EXPECT_FALSE(weakHashMap.hasNullReferences());
        EXPECT_TRUE(weakHashMap.isEmptyIgnoringNullReferences());
    }
}

TEST(WTF_WeakPtr, WeakHashMapRemoveNullReferences)
{
    {
        WeakHashMap<Base, int> weakHashMap;
        {
            Base object;
            weakHashMap.checkConsistency();
            EXPECT_EQ(s_baseWeakReferences, 0u);
            EXPECT_FALSE(weakHashMap.hasNullReferences());

            weakHashMap.removeNullReferences();
            weakHashMap.checkConsistency();
            EXPECT_EQ(s_baseWeakReferences, 0u);
            EXPECT_FALSE(weakHashMap.hasNullReferences());

            weakHashMap.add(object, 1);
            weakHashMap.checkConsistency();
            EXPECT_EQ(s_baseWeakReferences, 1u);
            EXPECT_FALSE(weakHashMap.hasNullReferences());

            weakHashMap.removeNullReferences();
            weakHashMap.checkConsistency();
            EXPECT_EQ(s_baseWeakReferences, 1u);
            EXPECT_FALSE(weakHashMap.hasNullReferences());

            weakHashMap.set(object, 2);
            weakHashMap.checkConsistency();
            EXPECT_EQ(s_baseWeakReferences, 1u);
            EXPECT_FALSE(weakHashMap.hasNullReferences());

            weakHashMap.removeNullReferences();
            weakHashMap.checkConsistency();
            EXPECT_EQ(s_baseWeakReferences, 1u);
            EXPECT_FALSE(weakHashMap.hasNullReferences());

            weakHashMap.set(object, 3);
            weakHashMap.checkConsistency();
            EXPECT_EQ(s_baseWeakReferences, 1u);
            EXPECT_FALSE(weakHashMap.hasNullReferences());
        }
        weakHashMap.checkConsistency();
        EXPECT_EQ(s_baseWeakReferences, 1u);
        EXPECT_TRUE(weakHashMap.hasNullReferences());

        weakHashMap.removeNullReferences();
        weakHashMap.checkConsistency();
        EXPECT_EQ(s_baseWeakReferences, 0u);
        EXPECT_FALSE(weakHashMap.hasNullReferences());
    }

    {
        WeakHashMap<Base, RefPtr<ValueObject>> weakHashMap;
        Vector<std::unique_ptr<Derived>> objects;

        for (unsigned i = 0; i < 50; ++i) {
            auto key = makeUnique<Derived>();
            weakHashMap.add(*key, ValueObject::create(i + 100));
            objects.append(WTFMove(key));
        }
        EXPECT_EQ(ValueObject::s_count, 50U);
        EXPECT_EQ(s_baseWeakReferences, 50U);
        EXPECT_FALSE(weakHashMap.hasNullReferences());
        weakHashMap.checkConsistency();

        weakHashMap.removeNullReferences();
        EXPECT_EQ(ValueObject::s_count, 50U);
        EXPECT_EQ(s_baseWeakReferences, 50U);
        EXPECT_FALSE(weakHashMap.hasNullReferences());
        weakHashMap.checkConsistency();

        for (unsigned i = 0; i < 50; ++i) {
            if (i % 2)
                objects[i] = nullptr;
        }

        weakHashMap.removeNullReferences();
        EXPECT_EQ(ValueObject::s_count, 25U);
        EXPECT_EQ(s_baseWeakReferences, 25U);
        EXPECT_FALSE(weakHashMap.hasNullReferences());
        weakHashMap.checkConsistency();

        bool seen[50] = { false };
        for (auto keyValue : weakHashMap) {
            int rawValue = keyValue.value->value;
            EXPECT_TRUE(rawValue >= 100 && rawValue < 150);
            EXPECT_EQ((rawValue - 100) % 2, 0);
            unsigned index = rawValue - 100;
            EXPECT_FALSE(seen[index]);
            seen[index] = true;
            EXPECT_EQ(&keyValue.key, objects[index].get());
            EXPECT_EQ(weakHashMap.get(keyValue.key), keyValue.value.get());
        }
        for (unsigned i = 0; i < 50; ++i) {
            bool shouldBePresent = !(i % 2);
            EXPECT_EQ(seen[i], shouldBePresent);
        }

        objects.clear();
        weakHashMap.removeNullReferences();
        EXPECT_EQ(ValueObject::s_count, 0U);
        EXPECT_EQ(s_baseWeakReferences, 0U);
        EXPECT_FALSE(weakHashMap.hasNullReferences());
        weakHashMap.checkConsistency();
    }
}

template <typename KeyType, typename ValueType, typename Map>
auto collectKeyValuePairsUsingIterators(Map& map)
{
    HashSet<std::pair<KeyType, ValueType>> pairs;
    for (auto it = map.begin(); it != map.end(); ++it) {
        std::pair keyValuePair { &it->key, it.get()->value };
        EXPECT_FALSE(pairs.contains(keyValuePair));
        pairs.add(keyValuePair);
    }
    return pairs;
};

TEST(WTF_WeakPtr, WeakHashMapIterators)
{
    {
        WeakHashMap<Base, int> weakHashMap;
        {
            Base object1;
            Derived object2;
            Base object3;
            Derived object4;
            weakHashMap.checkConsistency();
            weakHashMap.add(object1, 1);
            weakHashMap.add(object2, 2);
            weakHashMap.add(object3, 3);
            weakHashMap.add(object4, 4);
            EXPECT_EQ(s_baseWeakReferences, 4u);
            EXPECT_EQ(weakHashMap.computeSize(), 4u);
            EXPECT_FALSE(weakHashMap.hasNullReferences());

            const auto& constMap = weakHashMap;
            auto pairs = collectKeyValuePairsUsingIterators<Base*, int>(constMap);
            EXPECT_EQ(pairs.size(), 4u);
            EXPECT_TRUE(pairs.contains(std::pair { &object1, 1 }));
            EXPECT_TRUE(pairs.contains(std::pair { &object2, 2 }));
            EXPECT_TRUE(pairs.contains(std::pair { &object3, 3 }));
            EXPECT_TRUE(pairs.contains(std::pair { &object4, 4 }));

            weakHashMap.remove(object2);
            weakHashMap.remove(object3);
            EXPECT_EQ(s_baseWeakReferences, 4u);
            EXPECT_FALSE(weakHashMap.hasNullReferences());

            pairs = collectKeyValuePairsUsingIterators<Base*, int>(constMap);
            EXPECT_EQ(pairs.size(), 2u);
            EXPECT_TRUE(pairs.contains(std::pair { &object1, 1 }));
            EXPECT_FALSE(pairs.contains(std::pair { &object2, 2 }));
            EXPECT_FALSE(pairs.contains(std::pair { &object3, 3 }));
            EXPECT_TRUE(pairs.contains(std::pair { &object4, 4 }));
        }
        EXPECT_EQ(s_baseWeakReferences, 2u);
        EXPECT_TRUE(weakHashMap.hasNullReferences());

        auto pairs = collectKeyValuePairsUsingIterators<Base*, int>(weakHashMap);
        EXPECT_EQ(pairs.size(), 0u);
    }

    {
        WeakHashMap<Base, int> weakHashMap;
        {
            Vector<std::unique_ptr<Base>> objects;
            for (unsigned i = 0; i < 50; ++i) {
                if (i % 2)
                    objects.append(makeUnique<Derived>());
                else
                    objects.append(makeUnique<Base>());
                weakHashMap.set(*objects.last(), i);
            }
            weakHashMap.checkConsistency();
            EXPECT_EQ(s_baseWeakReferences, 50u);
            EXPECT_FALSE(weakHashMap.hasNullReferences());
            EXPECT_EQ(weakHashMap.computeSize(), 50u);

            for (auto it = weakHashMap.begin(); it != weakHashMap.end(); ++it) {
                auto keyValue = *it;
                if (keyValue.value % 6)
                    continue;
                keyValue.value *= 51;
            }

            weakHashMap.removeIf([](auto& keyValue) { return !(keyValue.value % 5); });
            weakHashMap.checkConsistency();
            EXPECT_EQ(s_baseWeakReferences, 50u);
            EXPECT_FALSE(weakHashMap.hasNullReferences());

            auto pairs = collectKeyValuePairsUsingIterators<Base*, int>(weakHashMap);
            ASSERT(pairs.size(), 40U);
            for (unsigned i = 0; i < 50; ++i) {
                if (!(i % 5))
                    EXPECT_TRUE(WTF::allOf(pairs, [&](auto& item) { return item.first != objects[i].get(); }));
                else if (!(i % 6))
                    EXPECT_TRUE(pairs.contains(std::pair { objects[i].get(), i * 51 }));
                else
                    EXPECT_TRUE(pairs.contains(std::pair { objects[i].get(), i }));
            }

            for (unsigned i = 0; i < 50; ++i) {
                if (!(i % 5) || !(i % 9))
                    objects[i] = nullptr;
            }
            EXPECT_EQ(s_baseWeakReferences, 40u);
            weakHashMap.checkConsistency();

            pairs = collectKeyValuePairsUsingIterators<Base*, int>(weakHashMap);
            ASSERT(pairs.size(), 36U);
            for (unsigned i = 0; i < 50; ++i) {
                if (!(i % 5) || !(i % 9))
                    EXPECT_TRUE(WTF::allOf(pairs, [&](auto& item) { return item.first != objects[i].get(); }));
                else if (!(i % 6))
                    EXPECT_TRUE(pairs.contains(std::pair { objects[i].get(), i * 51 }));
                else
                    EXPECT_TRUE(pairs.contains(std::pair { objects[i].get(), i }));
            }
            weakHashMap.removeNullReferences();
            EXPECT_EQ(s_baseWeakReferences, 36u);
            EXPECT_FALSE(weakHashMap.hasNullReferences());
            ASSERT(pairs.size(), weakHashMap.computeSize());
            weakHashMap.checkConsistency();
        }
        EXPECT_EQ(s_baseWeakReferences, 36u);
        weakHashMap.checkConsistency();
        EXPECT_TRUE(weakHashMap.hasNullReferences());

        auto pairs = collectKeyValuePairsUsingIterators<Base*, int>(weakHashMap);
        EXPECT_EQ(pairs.size(), 0u);

        EXPECT_EQ(s_baseWeakReferences, 36u);
        weakHashMap.checkConsistency();
        EXPECT_TRUE(weakHashMap.hasNullReferences());
    }

    {
        WeakHashMap<Base, RefPtr<ValueObject>> weakHashMap;
        {
            Vector<std::pair<std::unique_ptr<Base>, RefPtr<ValueObject>>> objects;
            for (unsigned i = 0; i < 50; ++i) {
                if (i % 2)
                    objects.append(std::pair { makeUnique<Derived>(), ValueObject::create(i * 83) });
                else
                    objects.append(std::pair { makeUnique<Base>(), ValueObject::create(i) });
                objects.last().first->dummy = 0;
                if (i < 25)
                    weakHashMap.add(*objects.last().first, objects.last().second);
            }
            weakHashMap.checkConsistency();
            EXPECT_EQ(s_baseWeakReferences, 25u);
            EXPECT_FALSE(weakHashMap.hasNullReferences());
            EXPECT_EQ(weakHashMap.computeSize(), 25u);

            for (unsigned i = 0; i < 50; ++i) {
                auto it = weakHashMap.find(*objects[i].first);
                if (i < 25) {
                    EXPECT_TRUE(it != weakHashMap.end());
                    EXPECT_EQ(it->key.dummy, 0);
                    if (i % 9)
                        continue;
                } else
                    EXPECT_TRUE(it == weakHashMap.end());
                weakHashMap.remove(it);
                weakHashMap.checkConsistency();
            }

            EXPECT_EQ(s_baseWeakReferences, 25u);
            EXPECT_FALSE(weakHashMap.hasNullReferences());
            EXPECT_EQ(weakHashMap.computeSize(), 22u);
            for (int i = 0; i < 25; ++i) {
                auto it = weakHashMap.find(*objects[i].first);
                if (!(i % 9)) {
                    EXPECT_TRUE(it == weakHashMap.end());
                    continue;
                }

                EXPECT_EQ(&it.get()->key, objects[i].first.get());
                EXPECT_EQ(it.get()->key.dummy, 0);
                if (i % 2) {
                    it.get()->key.dummy = i * 11;
                    EXPECT_EQ(it.get()->value->value, i * 83);
                } else
                    EXPECT_EQ(it.get()->value->value, i);

                if (!(i % 4))
                    objects[i].first = nullptr;
            }

            weakHashMap.checkConsistency();
            EXPECT_EQ(s_baseWeakReferences, 25u);
            EXPECT_TRUE(weakHashMap.hasNullReferences());
            for (int i = 0; i < 25; ++i) {
                if (!(i % 4))
                    continue;
                auto it = weakHashMap.find(*objects[i].first);
                if (!(i % 9)) {
                    EXPECT_TRUE(it == weakHashMap.end());
                    continue;
                }
                EXPECT_EQ(&(*it).key, objects[i].first.get());
                if (i % 2) {
                    EXPECT_EQ((*it).key.dummy, i * 11);
                    EXPECT_EQ((*it).value->value, i * 83);
                } else {
                    EXPECT_EQ((*it).key.dummy, 0);
                    it->key.dummy = i * 31;
                    EXPECT_EQ((*it).value->value, i);
                }
            }
            weakHashMap.checkConsistency();
            EXPECT_EQ(weakHashMap.computeSize(), 16u);
            for (int i = 0; i < 25; ++i) {
                if (!(i % 4))
                    continue;
                auto it = weakHashMap.find(*objects[i].first);
                if (!(i % 9)) {
                    EXPECT_TRUE(it == weakHashMap.end());
                    continue;
                }
                EXPECT_EQ(&it->key, objects[i].first.get());
                auto keyValue = it.get();
                if (i % 2) {
                    EXPECT_EQ(keyValue->key.dummy, i * 11);
                    EXPECT_EQ(keyValue->value->value, i * 83);
                } else {
                    EXPECT_EQ((*keyValue).key.dummy, i * 31);
                    EXPECT_EQ((*keyValue).value->value, i);
                }
            }
        }
        EXPECT_EQ(s_baseWeakReferences, 16u);
        EXPECT_TRUE(weakHashMap.hasNullReferences());
        auto pairs = collectKeyValuePairsUsingIterators<Base*, RefPtr<ValueObject>>(weakHashMap);
        EXPECT_EQ(pairs.size(), 0U);
        weakHashMap.removeNullReferences();
        EXPECT_FALSE(weakHashMap.hasNullReferences());
        EXPECT_EQ(s_baseWeakReferences, 0U);
    }
}

TEST(WTF_WeakPtr, WeakHashMapAmortizedCleanup)
{
    {
        WeakHashMap<Base, int> weakHashMap;
        {
            Vector<std::unique_ptr<Base>> objects;
            for (unsigned i = 0; i < 50; ++i) {
                if (i % 2)
                    objects.append(makeUnique<Derived>());
                else
                    objects.append(makeUnique<Base>());
                weakHashMap.set(*objects.last(), i);
            }
            for (unsigned i = 0; i < 4; ++i)
                collectKeyValuePairsUsingIterators<Base*, int>(weakHashMap);
            weakHashMap.checkConsistency();
            EXPECT_EQ(s_baseWeakReferences, 50u);
            EXPECT_FALSE(weakHashMap.hasNullReferences());
            EXPECT_EQ(weakHashMap.computeSize(), 50u);
            
            for (unsigned i = 0; i < 50; ++i) {
                if (!(i % 5))
                    objects[i] = nullptr;
            }
            weakHashMap.checkConsistency();
            EXPECT_EQ(s_baseWeakReferences, 50u);
            EXPECT_TRUE(weakHashMap.hasNullReferences());

            for (unsigned i = 0; i < 4; ++i)
                collectKeyValuePairsUsingIterators<Base*, int>(weakHashMap);

            weakHashMap.checkConsistency();
            EXPECT_EQ(s_baseWeakReferences, 50u);
            EXPECT_TRUE(weakHashMap.hasNullReferences());

            for (unsigned i = 0; i < 50; ++i) {
                if (!(i % 9))
                    objects[i] = nullptr;
            }

            weakHashMap.checkConsistency();
            EXPECT_EQ(s_baseWeakReferences, 40u);
            EXPECT_TRUE(weakHashMap.hasNullReferences());

            for (unsigned i = 0; i < 4; ++i)
                collectKeyValuePairsUsingIterators<Base*, int>(weakHashMap);

            weakHashMap.checkConsistency();
            EXPECT_EQ(s_baseWeakReferences, 40u);
            EXPECT_TRUE(weakHashMap.hasNullReferences());
        }
    }

    {
        WeakHashMap<Base, Ref<ValueObject>> weakHashMap;
        {
            Vector<std::unique_ptr<Base>> objects;
            for (unsigned i = 0; i < 50; ++i) {
                objects.append(makeUnique<Derived>());
                weakHashMap.set(*objects.last(), ValueObject::create(i));
            }
            weakHashMap.checkConsistency();
            EXPECT_EQ(s_baseWeakReferences, 50u);
            EXPECT_EQ(ValueObject::s_count, 50u);
            EXPECT_FALSE(weakHashMap.hasNullReferences());
            EXPECT_EQ(weakHashMap.computeSize(), 50u);
            
            for (unsigned i = 0; i < 50; ++i) {
                if (!(i % 7))
                    objects[i] = nullptr;
            }
            weakHashMap.checkConsistency();
            EXPECT_TRUE(weakHashMap.hasNullReferences());
            EXPECT_EQ(s_baseWeakReferences, 50u);
            EXPECT_EQ(ValueObject::s_count, 50u);

            for (unsigned i = 0; i < 10; ++i) {
                for (auto& ptr : objects) {
                    if (!ptr)
                        continue;
                    auto it = weakHashMap.find(*ptr);
                    EXPECT_TRUE(it != weakHashMap.end());
                }
            }
            weakHashMap.checkConsistency();
            EXPECT_EQ(s_baseWeakReferences, 50u);
            EXPECT_EQ(ValueObject::s_count, 50u);
            EXPECT_TRUE(weakHashMap.hasNullReferences());

            for (unsigned i = 0; i < 50; ++i) {
                if (!(i % 3))
                    objects[i] = nullptr;
            }
            weakHashMap.checkConsistency();
            EXPECT_EQ(s_baseWeakReferences, 42u);
            EXPECT_EQ(ValueObject::s_count, 42u);
            EXPECT_TRUE(weakHashMap.hasNullReferences());

            for (unsigned i = 0; i < 4; ++i) {
                int objectIndex = 0;
                for (auto& ptr : objects) {
                    if (ptr) {
                        auto result = weakHashMap.add(*ptr, ValueObject::create(objectIndex * 51));
                        EXPECT_EQ(result.iterator->value->value, objectIndex);
                    }
                    ++objectIndex;
                }
            }
            weakHashMap.checkConsistency();
            EXPECT_EQ(s_baseWeakReferences, 28u);
            EXPECT_EQ(ValueObject::s_count, 28u);
            EXPECT_FALSE(weakHashMap.hasNullReferences());
            EXPECT_EQ(weakHashMap.computeSize(), 28u);
        }
        weakHashMap.checkConsistency();
        EXPECT_EQ(s_baseWeakReferences, 28u);
        EXPECT_EQ(ValueObject::s_count, 28u);
        EXPECT_TRUE(weakHashMap.hasNullReferences());

        EXPECT_TRUE(weakHashMap.isEmptyIgnoringNullReferences());
        weakHashMap.checkConsistency();
        EXPECT_EQ(s_baseWeakReferences, 0u);
        EXPECT_EQ(ValueObject::s_count, 0u);
        EXPECT_FALSE(weakHashMap.hasNullReferences());
    }
}

TEST(WTF_WeakPtr, WeakHashMap_iterator_destruction)
{
    constexpr unsigned objectCount = 10;
    WeakHashMap<Base, unsigned> weakHashMap;
    Vector<std::unique_ptr<Base>> objects;
    objects.reserveInitialCapacity(objectCount);
    for (unsigned i = 0; i < objectCount; ++i) {
        auto object = makeUnique<Base>();
        weakHashMap.add(*object, i);
        objects.uncheckedAppend(WTFMove(object));
    }

    auto a = objects.takeLast();
    auto b = objects.takeLast();

    auto aIterator = weakHashMap.find(*a);
    objects.clear();
    for (unsigned i = 0; i < 20; ++i) {
        auto bIterator = weakHashMap.find(*b);
        EXPECT_EQ(bIterator->value, objectCount - 2);
        EXPECT_EQ(aIterator->value, objectCount - 1);
    }
}

class MultipleInheritanceBase1 : public CanMakeWeakPtr<MultipleInheritanceBase1> {
public:
    virtual void meow() = 0;

    int dummy; // Prevent empty base class optimization, to make testing more interesting.
};

class MultipleInheritanceBase2 : public CanMakeWeakPtr<MultipleInheritanceBase2> {
public:
    virtual void woof() = 0;

    int dummy; // Prevent empty base class optimization, to make testing more interesting.
};

class MultipleInheritanceDerived : public MultipleInheritanceBase1, public MultipleInheritanceBase2 {
public:
    bool meowCalled() const
    {
        return m_meowCalled;
    }
    bool woofCalled() const
    {
        return m_woofCalled;
    }

private:
    void meow() final
    {
        m_meowCalled = true;
    }

    void woof() final
    {
        m_woofCalled = true;
    }

    bool m_meowCalled { false };
    bool m_woofCalled { false };
};

TEST(WTF_WeakPtr, MultipleInheritance)
{
    WeakHashSet<MultipleInheritanceBase1> base1Set;
    WeakHashSet<MultipleInheritanceBase2> base2Set;
    {
        MultipleInheritanceDerived derived;
        base1Set.add(derived);
        base2Set.add(derived);
        for (MultipleInheritanceBase1& base1 : base1Set)
            base1.meow();
        for (MultipleInheritanceBase2& base2 : base2Set)
            base2.woof();
        EXPECT_TRUE(derived.meowCalled());
        EXPECT_TRUE(derived.woofCalled());
    }
    EXPECT_TRUE(base1Set.computesEmpty());
    EXPECT_TRUE(base2Set.computesEmpty());
}

struct ThreadSafeInstanceCounter : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<ThreadSafeInstanceCounter> {
    ThreadSafeInstanceCounter() { instanceCount++; }
    ~ThreadSafeInstanceCounter() { instanceCount--; }
    static std::atomic<size_t> instanceCount;
};

std::atomic<size_t> ThreadSafeInstanceCounter::instanceCount;

TEST(WTF_ThreadSafeWeakPtr, ThreadSafety)
{
    RefPtr counter = adoptRef(*new ThreadSafeInstanceCounter());
    ThreadSafeWeakPtr<ThreadSafeInstanceCounter> weakPtr(counter);
    EXPECT_NOT_NULL(weakPtr.get());

    std::atomic<size_t> readyThreads { 0 };
    EXPECT_EQ(ThreadSafeInstanceCounter::instanceCount, 1u);
    std::array<std::thread, 3> threads { std::thread { [&, counter] () mutable {
        Vector<RefPtr<ThreadSafeInstanceCounter>> strongReferences;
        strongReferences.reserveInitialCapacity(101);
        readyThreads++;
        while (readyThreads < 3) { }
        for (size_t i = 0; i < 100; i++) {
            strongReferences.uncheckedAppend(counter);
            strongReferences.uncheckedAppend(counter);
            strongReferences.takeLast();
        }
        counter = nullptr;
    } }, std::thread { [&, counter] () mutable {
        Vector<ThreadSafeWeakPtr<ThreadSafeInstanceCounter>> weakReferences;
        weakReferences.reserveInitialCapacity(101);
        readyThreads++;
        while (readyThreads < 3) { }
        for (size_t i = 0; i < 100; i++) {
            weakReferences.uncheckedAppend(counter);
            weakReferences.uncheckedAppend(counter);
            weakReferences.takeLast();
        }
        counter = nullptr;
    } }, std::thread { [&, counter] () mutable {
        Vector<RefPtr<ThreadSafeInstanceCounter>> strongReferences;
        Vector<ThreadSafeWeakPtr<ThreadSafeInstanceCounter>> weakReferences;
        strongReferences.reserveInitialCapacity(51);
        weakReferences.reserveInitialCapacity(51);
        readyThreads++;
        while (readyThreads < 3) { }
        for (size_t i = 0; i < 50; i++) {
            strongReferences.uncheckedAppend(counter.get());
            weakReferences.uncheckedAppend(counter);
            weakReferences.uncheckedAppend(counter);
            strongReferences.uncheckedAppend(weakReferences.takeLast().get());
            strongReferences.takeLast();
        }
        counter = nullptr;
    } } };

    counter = nullptr;
    for (auto& thread : threads)
        thread.join();
    EXPECT_NULL(weakPtr.get());
    EXPECT_EQ(ThreadSafeInstanceCounter::instanceCount, 0u);
}

TEST(WTF_ThreadSafeWeakPtr, UseAfterMoveResistance)
{
    auto counter = adoptRef(*new ThreadSafeInstanceCounter());
    auto weakPtr = ThreadSafeWeakPtr { counter.get() };
    auto movedTo = WTFMove(weakPtr);
    EXPECT_NULL(weakPtr.get());
    EXPECT_NOT_NULL(movedTo.get());
    ThreadSafeWeakPtr<ThreadSafeInstanceCounter> emptyConstructor;
    EXPECT_NULL(emptyConstructor.get());
}

} // namespace TestWebKitAPI

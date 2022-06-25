/*
 * Copyright (C) 2012-2021 Apple Inc. All rights reserved.
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
#include "DeletedAddressOfOperator.h"
#include "MoveOnly.h"
#include "RefLogger.h"
#include "Test.h"
#include <functional>
#include <wtf/RefPtr.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/StringHash.h>
#include <wtf/DataLog.h>
#include <wtf/Stopwatch.h>

namespace TestWebKitAPI {

template<int initialCapacity>
struct InitialCapacityTestHashTraits : public WTF::UnsignedWithZeroKeyHashTraits<int> {
    static const int minimumTableSize = initialCapacity;
};

template<typename T>
struct RobinHoodHash : public DefaultHash<T> {
    static constexpr bool hasHashInValue = true;
};

static constexpr unsigned capacityForSize(unsigned size)
{
    unsigned capacity = WTF::roundUpToPowerOfTwo(size);
    if (size * 100 >= capacity * 95)
        return capacity * 2;
    return capacity;
}

template<unsigned size>
void testInitialCapacity()
{
    constexpr unsigned initialCapacity = capacityForSize(size);
    MemoryCompactLookupOnlyRobinHoodHashSet<int, RobinHoodHash<int>, InitialCapacityTestHashTraits<initialCapacity>> testSet;

    // Initial capacity is null.
    ASSERT_EQ(0u, testSet.capacity());

    // Adding items up to size should never change the capacity.
    for (size_t i = 0; i < size; ++i) {
        testSet.add(i);
        ASSERT_EQ(initialCapacity, static_cast<unsigned>(testSet.capacity()));
    }

    // Adding items up to more than 95% of the capacity should not change the capacity.
    unsigned capacityLimit = static_cast<unsigned>(std::ceil(initialCapacity * 0.95));
    for (size_t i = size; i < capacityLimit; ++i) {
        testSet.add(i);
        ASSERT_EQ(initialCapacity, static_cast<unsigned>(testSet.capacity()));
    }

    // Adding one more item increase the capacity.
    testSet.add(initialCapacity);
    EXPECT_GT(static_cast<unsigned>(testSet.capacity()), initialCapacity);
}

template<unsigned size> inline void generateTestCapacityUpToSize();
template<> inline void generateTestCapacityUpToSize<0>()
{
}
template<unsigned size> inline void generateTestCapacityUpToSize()
{
    generateTestCapacityUpToSize<size - 1>();
    testInitialCapacity<size>();
}

TEST(WTF_RobinHoodHashSet, InitialCapacity)
{
    generateTestCapacityUpToSize<128>();
}

TEST(WTF_RobinHoodHashSet, MoveOnly)
{
    MemoryCompactLookupOnlyRobinHoodHashSet<MoveOnly> hashSet;

    for (size_t i = 0; i < 100; ++i) {
        MoveOnly moveOnly(i + 1);
        hashSet.add(WTFMove(moveOnly));
    }

    for (size_t i = 0; i < 100; ++i)
        EXPECT_TRUE(hashSet.contains(MoveOnly(i + 1)));

    for (size_t i = 0; i < 100; ++i)
        EXPECT_TRUE(hashSet.remove(MoveOnly(i + 1)));

    EXPECT_TRUE(hashSet.isEmpty());

    for (size_t i = 0; i < 100; ++i)
        hashSet.add(MoveOnly(i + 1));

    for (size_t i = 0; i < 100; ++i)
        EXPECT_TRUE(hashSet.take(MoveOnly(i + 1)) == MoveOnly(i + 1));

    EXPECT_TRUE(hashSet.isEmpty());

    for (size_t i = 0; i < 100; ++i)
        hashSet.add(MoveOnly(i + 1));

    MemoryCompactLookupOnlyRobinHoodHashSet<MoveOnly> secondSet;

    for (size_t i = 0; i < 100; ++i)
        secondSet.add(hashSet.takeAny());

    EXPECT_TRUE(hashSet.isEmpty());

    for (size_t i = 0; i < 100; ++i)
        EXPECT_TRUE(secondSet.contains(MoveOnly(i + 1)));
}


TEST(WTF_RobinHoodHashSet, UniquePtrKey)
{
    ConstructorDestructorCounter::TestingScope scope;

    MemoryCompactLookupOnlyRobinHoodHashSet<std::unique_ptr<ConstructorDestructorCounter>, RobinHoodHash<std::unique_ptr<ConstructorDestructorCounter>>> set;

    auto uniquePtr = makeUnique<ConstructorDestructorCounter>();
    set.add(WTFMove(uniquePtr));

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(0u, ConstructorDestructorCounter::destructionCount);

    set.clear();

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(1u, ConstructorDestructorCounter::destructionCount);
}

TEST(WTF_RobinHoodHashSet, UniquePtrKey_FindUsingRawPointer)
{
    MemoryCompactLookupOnlyRobinHoodHashSet<std::unique_ptr<int>, RobinHoodHash<std::unique_ptr<int>>> set;

    auto uniquePtr = makeUniqueWithoutFastMallocCheck<int>(5);
    int* ptr = uniquePtr.get();
    set.add(WTFMove(uniquePtr));

    auto it = set.find(ptr);
    ASSERT_TRUE(it != set.end());
    EXPECT_EQ(ptr, it->get());
    EXPECT_EQ(5, *it->get());
}

TEST(WTF_RobinHoodHashSet, UniquePtrKey_ContainsUsingRawPointer)
{
    MemoryCompactLookupOnlyRobinHoodHashSet<std::unique_ptr<int>, RobinHoodHash<std::unique_ptr<int>>> set;

    auto uniquePtr = makeUniqueWithoutFastMallocCheck<int>(5);
    int* ptr = uniquePtr.get();
    set.add(WTFMove(uniquePtr));

    EXPECT_EQ(true, set.contains(ptr));
}

TEST(WTF_RobinHoodHashSet, UniquePtrKey_RemoveUsingRawPointer)
{
    ConstructorDestructorCounter::TestingScope scope;

    MemoryCompactLookupOnlyRobinHoodHashSet<std::unique_ptr<ConstructorDestructorCounter>, RobinHoodHash<std::unique_ptr<ConstructorDestructorCounter>>> set;
    auto uniquePtr = makeUnique<ConstructorDestructorCounter>();
    ConstructorDestructorCounter* ptr = uniquePtr.get();
    set.add(WTFMove(uniquePtr));

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(0u, ConstructorDestructorCounter::destructionCount);

    bool result = set.remove(ptr);
    EXPECT_EQ(true, result);

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(1u, ConstructorDestructorCounter::destructionCount);
}

TEST(WTF_RobinHoodHashSet, UniquePtrKey_TakeUsingRawPointer)
{
    ConstructorDestructorCounter::TestingScope scope;

    MemoryCompactLookupOnlyRobinHoodHashSet<std::unique_ptr<ConstructorDestructorCounter>, RobinHoodHash<std::unique_ptr<ConstructorDestructorCounter>>> set;

    auto uniquePtr = makeUnique<ConstructorDestructorCounter>();
    ConstructorDestructorCounter* ptr = uniquePtr.get();
    set.add(WTFMove(uniquePtr));

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(0u, ConstructorDestructorCounter::destructionCount);

    auto result = set.take(ptr);
    EXPECT_EQ(ptr, result.get());

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(0u, ConstructorDestructorCounter::destructionCount);

    result = nullptr;

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(1u, ConstructorDestructorCounter::destructionCount);
}

TEST(WTF_RobinHoodHashSet, CopyEmpty)
{
    {
        MemoryCompactLookupOnlyRobinHoodHashSet<unsigned, RobinHoodHash<unsigned>> foo;
        MemoryCompactLookupOnlyRobinHoodHashSet<unsigned, RobinHoodHash<unsigned>> bar(foo);

        EXPECT_EQ(0u, bar.capacity());
        EXPECT_EQ(0u, bar.size());
    }
    {
        MemoryCompactLookupOnlyRobinHoodHashSet<unsigned, RobinHoodHash<unsigned>> foo({ 1, 5, 64, 42 });
        EXPECT_EQ(4u, foo.size());
        foo.remove(1);
        foo.remove(5);
        foo.remove(42);
        foo.remove(64);
        MemoryCompactLookupOnlyRobinHoodHashSet<unsigned, RobinHoodHash<unsigned>> bar(foo);

        EXPECT_EQ(0u, bar.capacity());
        EXPECT_EQ(0u, bar.size());
    }
}

TEST(WTF_RobinHoodHashSet, CopyAllocateAtLeastMinimumCapacity)
{
    MemoryCompactLookupOnlyRobinHoodHashSet<unsigned, RobinHoodHash<unsigned>> foo({ 42 });
    EXPECT_EQ(1u, foo.size());
    MemoryCompactLookupOnlyRobinHoodHashSet<unsigned, RobinHoodHash<unsigned>> bar(foo);

    EXPECT_EQ(8u, bar.capacity());
    EXPECT_EQ(1u, bar.size());
}

TEST(WTF_RobinHoodHashSet, CopyCapacityIsNotOnBoundary)
{
    for (unsigned size = 4; size < 100; ++size) {
        MemoryCompactLookupOnlyRobinHoodHashSet<unsigned, RobinHoodHash<unsigned>> source;
        for (unsigned i = 1; i < size + 1; ++i)
            source.add(i);

        MemoryCompactLookupOnlyRobinHoodHashSet<unsigned, RobinHoodHash<unsigned>> copy1(source);
        MemoryCompactLookupOnlyRobinHoodHashSet<unsigned, RobinHoodHash<unsigned>> copy2(source);
        MemoryCompactLookupOnlyRobinHoodHashSet<unsigned, RobinHoodHash<unsigned>> copy3(source);

        EXPECT_EQ(size, copy1.size());
        EXPECT_EQ(size, copy2.size());
        EXPECT_EQ(size, copy3.size());
        for (unsigned i = 1; i < size + 1; ++i) {
            EXPECT_TRUE(copy1.contains(i)) << i;
            EXPECT_TRUE(copy2.contains(i)) << i;
            EXPECT_TRUE(copy3.contains(i)) << i;
        }
        EXPECT_FALSE(copy1.contains(size + 2));
        EXPECT_FALSE(copy2.contains(size + 2));
        EXPECT_FALSE(copy3.contains(size + 2));

        EXPECT_TRUE(copy2.remove(1));
        EXPECT_EQ(copy1.capacity(), copy2.capacity());
        EXPECT_FALSE(copy2.contains(1));

        EXPECT_TRUE(copy3.add(size + 2).isNewEntry);
        if ((copy1.capacity() * 0.95) <= copy1.size())
            EXPECT_EQ(copy1.capacity() * 2, copy3.capacity()) << copy1.size();
        else
            EXPECT_EQ(copy1.capacity(), copy3.capacity()) << copy1.size();
        EXPECT_TRUE(copy3.contains(size + 2));
    }
}

struct DerefObserver {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    NEVER_INLINE void ref()
    {
        ++count;
    }
    NEVER_INLINE void deref()
    {
        --count;
        observedBucket = bucketAddress->get();
    }
    unsigned count { 1 };
    const RefPtr<DerefObserver>* bucketAddress { nullptr };
    const DerefObserver* observedBucket { nullptr };
};

TEST(WTF_RobinHoodHashSet, RefPtrNotZeroedBeforeDeref)
{
    auto observer = makeUnique<DerefObserver>();

    MemoryCompactLookupOnlyRobinHoodHashSet<RefPtr<DerefObserver>, RobinHoodHash<RefPtr<DerefObserver>>> set;
    set.add(adoptRef(observer.get()));

    auto iterator = set.find(observer.get());
    EXPECT_TRUE(iterator != set.end());

    observer->bucketAddress = iterator.get();

    EXPECT_TRUE(observer->observedBucket == nullptr);
    EXPECT_TRUE(set.remove(observer.get()));

    // It if fine to either leave the old value intact at deletion or already set it to the deleted
    // value.
    // A zero would be a incorrect outcome as it would mean we nulled the bucket before an opaque
    // call.
    EXPECT_TRUE(observer->observedBucket == observer.get() || observer->observedBucket == RefPtr<DerefObserver>::PtrTraits::hashTableDeletedValue());
    EXPECT_EQ(observer->count, 0u);
}


TEST(WTF_RobinHoodHashSet, UniquePtrNotZeroedBeforeDestructor)
{
    struct DestructorObserver {
        ~DestructorObserver()
        {
            observe();
        }
        std::function<void()> observe;
    };

    const std::unique_ptr<DestructorObserver>* bucketAddress = nullptr;
    const DestructorObserver* observedBucket = nullptr;
    bool destructorCalled = false;
    std::unique_ptr<DestructorObserver> observer(new DestructorObserver { [&]() {
        observedBucket = bucketAddress->get();
        destructorCalled = true;
    }});

    const DestructorObserver* observerAddress = observer.get();

    MemoryCompactLookupOnlyRobinHoodHashSet<std::unique_ptr<DestructorObserver>, RobinHoodHash<std::unique_ptr<DestructorObserver>>> set;
    auto addResult = set.add(WTFMove(observer));

    EXPECT_TRUE(addResult.isNewEntry);
    EXPECT_TRUE(observedBucket == nullptr);
    EXPECT_FALSE(destructorCalled);

    bucketAddress = addResult.iterator.get();

    EXPECT_TRUE(observedBucket == nullptr);
    EXPECT_FALSE(destructorCalled);
    EXPECT_TRUE(set.remove(*addResult.iterator));

    EXPECT_TRUE(destructorCalled);
    EXPECT_TRUE(observedBucket == observerAddress || observedBucket == reinterpret_cast<const DestructorObserver*>(-1));
}

TEST(WTF_RobinHoodHashSet, Ref)
{
    {
        RefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashSet<Ref<RefLogger>, RobinHoodHash<Ref<RefLogger>>> set;

        Ref<RefLogger> ref(a);
        set.add(WTFMove(ref));
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashSet<Ref<RefLogger>, RobinHoodHash<Ref<RefLogger>>> set;

        Ref<RefLogger> ref(a);
        set.add(ref.copyRef());
    }

    ASSERT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());

    {
        RefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashSet<Ref<RefLogger>, RobinHoodHash<Ref<RefLogger>>> set;

        Ref<RefLogger> ref(a);
        set.add(WTFMove(ref));
        set.remove(&a);
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashSet<Ref<RefLogger>, RobinHoodHash<Ref<RefLogger>>> set;

        Ref<RefLogger> ref(a);
        set.add(WTFMove(ref));

        auto aOut = set.take(&a);
        ASSERT_TRUE(static_cast<bool>(aOut));
        ASSERT_EQ(&a, aOut.get());
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashSet<Ref<RefLogger>, RobinHoodHash<Ref<RefLogger>>> set;

        Ref<RefLogger> ref(a);
        set.add(WTFMove(ref));

        auto aOut = set.takeAny();
        ASSERT_TRUE(static_cast<bool>(aOut));
        ASSERT_EQ(&a, aOut.get());
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        MemoryCompactLookupOnlyRobinHoodHashSet<Ref<RefLogger>, RobinHoodHash<Ref<RefLogger>>> set;
        auto emptyTake = set.takeAny();
        ASSERT_FALSE(static_cast<bool>(emptyTake));
    }

    {
        RefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashSet<Ref<RefLogger>, RobinHoodHash<Ref<RefLogger>>> set;

        Ref<RefLogger> ref(a);
        set.add(WTFMove(ref));

        ASSERT_TRUE(set.contains(&a));
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        MemoryCompactLookupOnlyRobinHoodHashSet<Ref<RefLogger>, RobinHoodHash<Ref<RefLogger>>> set;
        for (int i = 0; i < 64; ++i) {
            // FIXME: All of these RefLogger objects leak. No big deal for a test I guess.
            Ref<RefLogger> ref = adoptRef(*new RefLogger("a"));
            auto* pointer = ref.ptr();
            set.add(WTFMove(ref));
            ASSERT_TRUE(set.contains(pointer));
        }
    }
    ASSERT_STREQ("deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) ", takeLogStr().c_str());

    {
        RefLogger a("a");

        MemoryCompactLookupOnlyRobinHoodHashSet<Ref<RefLogger>, RobinHoodHash<Ref<RefLogger>>> set;
        Ref<RefLogger> ref(a);
        set.add(WTFMove(ref));
        MemoryCompactLookupOnlyRobinHoodHashSet<Ref<RefLogger>, RobinHoodHash<Ref<RefLogger>>> set2(set);
    }
    ASSERT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_RobinHoodHashSet, DeletedAddressOfOperator)
{
    MemoryCompactLookupOnlyRobinHoodHashSet<DeletedAddressOfOperator> set1;
    set1.add(10);

    set1.remove(10);
}

TEST(WTF_RobinHoodHashSet, RemoveRandom)
{
    MemoryCompactLookupOnlyRobinHoodHashSet<unsigned, RobinHoodHash<unsigned>> set1 { 1, 2, 3 };
    set1.remove(set1.random());
    set1.remove(set1.random());
    set1.remove(set1.random());
    ASSERT_TRUE(set1.isEmpty());
}

TEST(WTF_RobinHoodHashSet, ReserveInitialCapacity)
{
    MemoryCompactLookupOnlyRobinHoodHashSet<String> set;
    EXPECT_EQ(0u, set.size());
    EXPECT_EQ(0u, set.capacity());

    set.reserveInitialCapacity(9999);
    EXPECT_EQ(0u, set.size());
    EXPECT_EQ(16384u, set.capacity());

    for (int i = 0; i < 9999; ++i)
        set.add(makeString("foo", i));
    EXPECT_EQ(9999u, set.size());
    EXPECT_EQ(16384u, set.capacity());
    EXPECT_TRUE(set.contains("foo3"_str));

    for (int i = 0; i < 9999; ++i)
        set.add(makeString("excess", i));
    EXPECT_EQ(9999u + 9999u, set.size());
    EXPECT_EQ(32768u, set.capacity());

    for (int i = 0; i < 9999; ++i)
        EXPECT_TRUE(set.remove(makeString("foo", i)));
    EXPECT_EQ(9999u, set.size());
    EXPECT_EQ(32768u, set.capacity());

    for (int i = 0; i < 9999; ++i)
        EXPECT_TRUE(set.remove(makeString("excess", i)));
    EXPECT_EQ(0u, set.size());
    EXPECT_EQ(8u, set.capacity());

    MemoryCompactLookupOnlyRobinHoodHashSet<String> set2;
    set2.reserveInitialCapacity(9999);
    EXPECT_FALSE(set2.remove("foo1"_s));

    for (int i = 0; i < 2000; ++i)
        set2.add(makeString("foo", i));
    EXPECT_EQ(2000u, set2.size());
    EXPECT_EQ(16384u, set2.capacity());

    for (int i = 0; i < 2000; ++i)
        EXPECT_TRUE(set2.remove(makeString("foo", i)));
    EXPECT_EQ(0u, set2.size());
    EXPECT_EQ(8u, set2.capacity());
}

TEST(WTF_RobinHoodHashSet, AddAndIdentity)
{
    MemoryCompactLookupOnlyRobinHoodHashSet<RefPtr<StringImpl>> set;
    for (unsigned index = 0; index < 1e3; ++index) {
        auto string = String::number(index);
        auto result = set.add(string.impl());
        EXPECT_TRUE(result.isNewEntry);
        EXPECT_EQ(*result.iterator, string.impl());
    }
    for (unsigned index = 0; index < 1e3; ++index) {
        auto string = String::number(index);
        auto result = set.add(string.impl());
        EXPECT_FALSE(result.isNewEntry);
        EXPECT_NE(*result.iterator, string.impl());
    }
}

TEST(WTF_RobinHoodHashSet, LargeRemoval)
{
    MemoryCompactLookupOnlyRobinHoodHashSet<RefPtr<StringImpl>> set;
    for (unsigned index = 0; index < 1e4; ++index) {
        auto string = String::number(index);
        set.add(string.impl());
    }
    EXPECT_EQ(set.size(), 1e4);
    for (unsigned index = 0; index < 1e4; ++index) {
        if (index & 0x1) {
            auto string = String::number(index);
            bool removed = set.remove(string.impl());
            EXPECT_TRUE(removed);
        }
    }
    EXPECT_EQ(set.size(), 1e4 / 2);
    for (unsigned index = 0; index < 1e4; ++index) {
        auto string = String::number(index);
        if (index & 0x1)
            EXPECT_FALSE(set.contains(string.impl()));
        else
            EXPECT_TRUE(set.contains(string.impl()));
    }

    for (unsigned index = 0; index < 1e4; ++index) {
        if (!(index & 0x1)) {
            auto string = String::number(index);
            bool removed = set.remove(string.impl());
            EXPECT_TRUE(removed);
        }
    }
    EXPECT_EQ(set.size(), 0u);
    EXPECT_EQ(set.capacity(), 8u);
}

} // namespace TestWebKitAPI

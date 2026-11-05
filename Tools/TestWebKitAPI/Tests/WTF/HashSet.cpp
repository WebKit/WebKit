/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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

#include "Helpers/Counters.h"
#include "DeletedAddressOfOperator.h"
#include "MoveOnly.h"
#include "RefLogger.h"
#include "Helpers/Test.h"
#include <functional>
#include <wtf/HashSet.h>
#include <wtf/InlineWeakPtr.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/RefCountedWithInlineWeakPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringHash.h>

namespace {

class InlineWeakPtrObject : public RefCountedWithInlineWeakPtr<InlineWeakPtrObject> {
public:
    static Ref<InlineWeakPtrObject> create()
    {
        return createRefCountedWithInlineWeakPtr<InlineWeakPtrObject>();
    }
};

}

namespace TestWebKitAPI {

template<int initialCapacity>
struct InitialCapacityTestHashTraits : public WTF::UnsignedWithZeroKeyHashTraits<int> {
    static const int minimumTableSize = initialCapacity;
};

template<unsigned size>
void testInitialCapacity()
{
    // HashTable requires minimumTableSize >= 8.
    const unsigned initialCapacity = std::max(8u, WTF::HashTableCapacityForSize<size>::value);
    HashSet<int, DefaultHash<int>, InitialCapacityTestHashTraits<initialCapacity> > testSet;

    // Initial capacity is null.
    ASSERT_EQ(0u, testSet.capacity());

    // Adding items up to size should never change the capacity.
    for (size_t i = 0; i < size; ++i) {
        testSet.add(i);
        ASSERT_EQ(initialCapacity, static_cast<unsigned>(testSet.capacity()));
    }

    // Adding items up to less than 3/4 of the capacity should not change the capacity.
    unsigned capacityLimit = initialCapacity * 3 / 4 - 1;
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

TEST(WTF_HashSet, InitialCapacity)
{
    generateTestCapacityUpToSize<128>();
}

TEST(WTF_HashSet, MoveOnly)
{
    HashSet<MoveOnly> hashSet;

    for (size_t i = 0; i < 100; ++i) {
        MoveOnly moveOnly(i + 1);
        hashSet.add(WTF::move(moveOnly));
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

    HashSet<MoveOnly> secondSet;

    for (size_t i = 0; i < 100; ++i)
        secondSet.add(hashSet.takeAny());

    EXPECT_TRUE(hashSet.isEmpty());

    for (size_t i = 0; i < 100; ++i)
        EXPECT_TRUE(secondSet.contains(MoveOnly(i + 1)));
}


TEST(WTF_HashSet, UniquePtrKey)
{
    ConstructorDestructorCounter::TestingScope scope;

    HashSet<std::unique_ptr<ConstructorDestructorCounter>> set;

    auto uniquePtr = makeUnique<ConstructorDestructorCounter>();
    set.add(WTF::move(uniquePtr));

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(0u, ConstructorDestructorCounter::destructionCount);

    set.clear();

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(1u, ConstructorDestructorCounter::destructionCount);
}

TEST(WTF_HashSet, UniquePtrKey_FindUsingRawPointer)
{
    HashSet<std::unique_ptr<int>> set;

    auto uniquePtr = makeUniqueWithoutFastMallocCheck<int>(5);
    int* ptr = uniquePtr.get();
    set.add(WTF::move(uniquePtr));

    auto it = set.find(ptr);
    ASSERT_TRUE(it != set.end());
    EXPECT_EQ(ptr, it->get());
    EXPECT_EQ(5, *it->get());
}

TEST(WTF_HashSet, UniquePtrKey_ContainsUsingRawPointer)
{
    HashSet<std::unique_ptr<int>> set;

    auto uniquePtr = makeUniqueWithoutFastMallocCheck<int>(5);
    int* ptr = uniquePtr.get();
    set.add(WTF::move(uniquePtr));

    EXPECT_EQ(true, set.contains(ptr));
}

TEST(WTF_HashSet, UniquePtrKey_RemoveUsingRawPointer)
{
    ConstructorDestructorCounter::TestingScope scope;

    HashSet<std::unique_ptr<ConstructorDestructorCounter>> set;
#if !CHECK_HASHTABLE_ITERATORS &&!DUMP_HASHTABLE_STATS_PER_TABLE
    static_assert(sizeof(set) == sizeof(void*));
#endif

    auto uniquePtr = makeUnique<ConstructorDestructorCounter>();
    ConstructorDestructorCounter* ptr = uniquePtr.get();
    set.add(WTF::move(uniquePtr));

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(0u, ConstructorDestructorCounter::destructionCount);

    bool result = set.remove(ptr);
    EXPECT_EQ(true, result);

    EXPECT_EQ(1u, ConstructorDestructorCounter::constructionCount);
    EXPECT_EQ(1u, ConstructorDestructorCounter::destructionCount);
}

TEST(WTF_HashSet, UniquePtrKey_TakeUsingRawPointer)
{
    ConstructorDestructorCounter::TestingScope scope;

    HashSet<std::unique_ptr<ConstructorDestructorCounter>> set;

    auto uniquePtr = makeUnique<ConstructorDestructorCounter>();
    ConstructorDestructorCounter* ptr = uniquePtr.get();
    set.add(WTF::move(uniquePtr));

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

TEST(WTF_HashSet, CopyEmpty)
{
    {
        HashSet<unsigned> foo;
        HashSet<unsigned> bar(foo);

        EXPECT_EQ(0u, bar.capacity());
        EXPECT_EQ(0u, bar.size());
    }
    {
        HashSet<unsigned> foo({ 1, 5, 64, 42 });
        EXPECT_EQ(4u, foo.size());
        foo.remove(1);
        foo.remove(5);
        foo.remove(42);
        foo.remove(64);
        HashSet<unsigned> bar(foo);

        EXPECT_EQ(0u, bar.capacity());
        EXPECT_EQ(0u, bar.size());
    }
}

TEST(WTF_HashSet, CopyAllocateAtLeastMinimumCapacity)
{
    HashSet<unsigned> foo({ 42 });
    EXPECT_EQ(1u, foo.size());
    HashSet<unsigned> bar(foo);

    EXPECT_EQ(8u, bar.capacity());
    EXPECT_EQ(1u, bar.size());
}

TEST(WTF_HashSet, CopyCapacityIsNotOnBoundary)
{
    // Starting at 4 because the minimum size is 8.
    // With a size of 8, a medium load can be up to 3.3333->3.
    // Adding 1 to 3 would reach max load.
    // While correct, that's not really what we care about here.
    for (unsigned size = 4; size < 100; ++size) {
        HashSet<unsigned> source;
        for (unsigned i = 1; i < size + 1; ++i)
            source.add(i);

        HashSet<unsigned> copy1(source);
        HashSet<unsigned> copy2(source);
        HashSet<unsigned> copy3(source);

        EXPECT_EQ(size, copy1.size());
        EXPECT_EQ(size, copy2.size());
        EXPECT_EQ(size, copy3.size());
        for (unsigned i = 1; i < size + 1; ++i) {
            EXPECT_TRUE(copy1.contains(i));
            EXPECT_TRUE(copy2.contains(i));
            EXPECT_TRUE(copy3.contains(i));
        }
        EXPECT_FALSE(copy1.contains(size + 2));
        EXPECT_FALSE(copy2.contains(size + 2));
        EXPECT_FALSE(copy3.contains(size + 2));

        EXPECT_TRUE(copy2.remove(1));
        EXPECT_EQ(copy1.capacity(), copy2.capacity());
        EXPECT_FALSE(copy2.contains(1));

        EXPECT_TRUE(copy3.add(size + 2).isNewEntry);
        EXPECT_EQ(copy1.capacity(), copy3.capacity());
        EXPECT_TRUE(copy3.contains(size + 2));
    }
}

namespace {

struct DerefObserver {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(DerefObserver);
    NEVER_INLINE void ref()
    {
        ++count;
    }
    NEVER_INLINE void deref()
    {
        --count;
        if (!set)
            return;
        observedDeref = true;
        observedSetSize = set->size();
        observedContains = set->contains(this);
    }
    unsigned count { 1 };
    const HashSet<RefPtr<DerefObserver>>* set { nullptr };
    bool observedDeref { false };
    unsigned observedSetSize { 0 };
    bool observedContains { true };
};

} // namespace

TEST(WTF_HashSet, RefPtrRemovedBeforeDeref)
{
    auto observer = makeUniqueWithoutRefCountedCheck<DerefObserver>();

    HashSet<RefPtr<DerefObserver>> set;
    set.add(adoptRef(observer.get()));
    EXPECT_TRUE(set.contains(observer.get()));

    observer->set = &set;
    EXPECT_TRUE(set.remove(observer.get()));

    EXPECT_TRUE(observer->observedDeref);
    EXPECT_EQ(observer->observedSetSize, 0u);
    EXPECT_FALSE(observer->observedContains);
    EXPECT_EQ(observer->count, 0u);
}

TEST(WTF_HashSet, UniquePtrRemovedBeforeDestructor)
{
    struct DestructorObserver {
        ~DestructorObserver()
        {
            observe();
        }
        std::function<void()> observe;
    };

    HashSet<std::unique_ptr<DestructorObserver>> set;
    DestructorObserver* observerAddress = nullptr;
    bool observedDestructor = false;
    unsigned observedSetSize = 0;
    bool observedContains = true;
    std::unique_ptr<DestructorObserver> observer(new DestructorObserver { [&]() {
        observedDestructor = true;
        observedSetSize = set.size();
        observedContains = set.contains(observerAddress);
    }});
    observerAddress = observer.get();

    auto addResult = set.add(WTF::move(observer));
    EXPECT_TRUE(addResult.isNewEntry);
    EXPECT_FALSE(observedDestructor);

    EXPECT_TRUE(set.remove(*addResult.iterator));

    EXPECT_TRUE(observedDestructor);
    EXPECT_EQ(observedSetSize, 0u);
    EXPECT_FALSE(observedContains);
}

TEST(WTF_HashSet, Ref)
{
    {
        RefLogger a("a");

        HashSet<Ref<RefLogger>> set;

        Ref<RefLogger> ref(a);
        set.add(WTF::move(ref));
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefLogger a("a");

        HashSet<Ref<RefLogger>> set;

        Ref<RefLogger> ref(a);
        set.add(ref.copyRef());
    }

    ASSERT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());

    {
        RefLogger a("a");

        HashSet<Ref<RefLogger>> set;

        Ref<RefLogger> ref(a);
        set.add(WTF::move(ref));
        set.remove(&a);
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefLogger a("a");

        HashSet<Ref<RefLogger>> set;

        Ref<RefLogger> ref(a);
        set.add(WTF::move(ref));

        auto aOut = set.take(&a);
        ASSERT_TRUE(static_cast<bool>(aOut));
        ASSERT_EQ(&a, aOut.get());
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefLogger a("a");

        HashSet<Ref<RefLogger>> set;

        Ref<RefLogger> ref(a);
        set.add(WTF::move(ref));

        auto aOut = set.takeAny();
        ASSERT_TRUE(static_cast<bool>(aOut));
        ASSERT_EQ(&a, aOut.get());
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        HashSet<Ref<RefLogger>> set;
        auto emptyTake = set.takeAny();
        ASSERT_FALSE(static_cast<bool>(emptyTake));
    }

    {
        RefLogger a("a");

        HashSet<Ref<RefLogger>> set;

        Ref<RefLogger> ref(a);
        set.add(WTF::move(ref));
        
        ASSERT_TRUE(set.contains(&a));
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        HashSet<Ref<RefLogger>> set;
        for (int i = 0; i < 64; ++i) {
            // FIXME: All of these RefLogger objects leak. No big deal for a test I guess.
            Ref<RefLogger> ref = adoptRef(*new RefLogger("a"));
            auto* pointer = ref.ptr();
            set.add(WTF::move(ref));
            ASSERT_TRUE(set.contains(pointer));
        }
    }
    ASSERT_STREQ("deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) deref(a) ", takeLogStr().c_str());

    {
        RefLogger a("a");

        HashSet<Ref<RefLogger>> set;
        Ref<RefLogger> ref(a);
        set.add(WTF::move(ref));
        HashSet<Ref<RefLogger>> set2(set);
    }
    ASSERT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_HashSet, DeletedAddressOfOperator)
{
    HashSet<DeletedAddressOfOperator> set1;
    set1.add(10);

    set1.remove(10);
}

TEST(WTF_HashSet, RemoveRandom)
{
    HashSet<unsigned> set1 { 1, 2, 3 };
    set1.remove(set1.random());
    set1.remove(set1.random());
    set1.remove(set1.random());
    ASSERT_TRUE(set1.isEmpty());
}

TEST(WTF_HashSet, RemoveIf)
{
    HashSet<unsigned> set1 { 1, 2, 3, 4, 5 };
    ASSERT_EQ(set1.size(), 5u);
    set1.removeIf([] (unsigned item) { return item % 2;  });
    set1.checkConsistency();
    ASSERT_TRUE(!set1.contains(1));
    ASSERT_TRUE(set1.contains(2));
    ASSERT_TRUE(!set1.contains(3));
    ASSERT_TRUE(set1.contains(4));
    ASSERT_TRUE(!set1.contains(5));
    ASSERT_EQ(set1.size(), 2u);
}

TEST(WTF_HashSet, RemoveIfShrinkToBestSize)
{
    HashSet<unsigned> set1;
    set1.add(1);
    unsigned originalCapacity = set1.capacity();
    while (set1.capacity() < originalCapacity * 4)
        set1.add(set1.size() + 1);
    set1.removeIf([] (unsigned item) { return item != 1; });
    set1.checkConsistency();
    ASSERT_EQ(set1.size(), 1u);
    ASSERT_EQ(set1.capacity(), originalCapacity);

    set1.clear();
    set1.checkConsistency();
    while (set1.capacity() < originalCapacity * 8)
        set1.add(set1.size() + 1);
    set1.removeIf([originalCapacity] (unsigned item) { return item >= originalCapacity / 2; });
    set1.checkConsistency();
    ASSERT_EQ(set1.size(), originalCapacity / 2 - 1);
    ASSERT_EQ(set1.capacity(), originalCapacity);
}

TEST(WTF_HashSet, ReserveInitialCapacity)
{
    HashSet<String> set;
    EXPECT_EQ(0u, set.size());
    EXPECT_EQ(0u, set.capacity());

    set.reserveInitialCapacity(9999);
    EXPECT_EQ(0u, set.size());
    EXPECT_EQ(32768u, set.capacity());

    for (int i = 0; i < 9999; ++i)
        set.add(makeString("foo"_s, i));
    EXPECT_EQ(9999u, set.size());
    EXPECT_EQ(32768u, set.capacity());
    EXPECT_TRUE(set.contains("foo3"_str));

    for (int i = 0; i < 9999; ++i)
        set.add(makeString("excess"_s, i));
    EXPECT_EQ(9999u + 9999u, set.size());
    EXPECT_EQ(32768u + 32768u, set.capacity());

    for (int i = 0; i < 9999; ++i)
        EXPECT_TRUE(set.remove(makeString("foo"_s, i)));
    EXPECT_EQ(9999u, set.size());
    EXPECT_EQ(32768u, set.capacity());

    for (int i = 0; i < 9999; ++i)
        EXPECT_TRUE(set.remove(makeString("excess"_s, i)));
    EXPECT_EQ(0u, set.size());
    EXPECT_EQ(8u, set.capacity());

    HashSet<String> set2;
    set2.reserveInitialCapacity(9999);
    EXPECT_FALSE(set2.remove("foo1"_s));

    for (int i = 0; i < 2000; ++i)
        set2.add(makeString("foo"_s, i));
    EXPECT_EQ(2000u, set2.size());
    EXPECT_EQ(32768u, set2.capacity());

    for (int i = 0; i < 2000; ++i)
        EXPECT_TRUE(set2.remove(makeString("foo"_s, i)));
    EXPECT_EQ(0u, set2.size());
    EXPECT_EQ(8u, set2.capacity());
}

TEST(WTF_HashSet, UnionWith)
{
    HashSet<int> emptySet;
    HashSet<int> set1 { 1, 2, 3 };
    HashSet<int> set2 { 2, 3, 4 };
    HashSet<int> set3 { 1, 2, 3, 4 };
    Vector<int> sequence { 2, 3, 4 };

    {
        auto result = emptySet.unionWith(set1);
        EXPECT_EQ(result, set1);
    }

    {
        auto result = set1.unionWith(emptySet);
        EXPECT_EQ(result, set1);
    }

    {
        auto result = emptySet.unionWith(emptySet);
        EXPECT_EQ(result, emptySet);
    }

    {
        auto result = set1.unionWith(set1);
        EXPECT_EQ(result, set1);
    }

    {
        auto result = set1.unionWith(set2);
        EXPECT_EQ(result, set3);
    }

    {
        auto result = set2.unionWith(set1);
        EXPECT_EQ(result, set3);
    }

    {
        auto result = set1.unionWith(sequence);
        EXPECT_EQ(result, set3);
    }
}

TEST(WTF_HashSet, FormUnion)
{
    HashSet<int> emptySet;
    HashSet<int> set1 { 1, 2, 3 };
    HashSet<int> set2 { 2, 3, 4 };
    HashSet<int> set3 { 1, 2, 3, 4 };
    Vector<int> sequence { 2, 3, 4 };

    {
        auto result = emptySet;
        result.addAll(set1);
        EXPECT_EQ(result, set1);
    }

    {
        auto result = set1;
        result.addAll(emptySet);
        EXPECT_EQ(result, set1);
    }

    {
        auto result = emptySet;
        result.addAll(emptySet);
        EXPECT_EQ(result, emptySet);
    }

    {
        auto result = set1;
        result.addAll(set1);
        EXPECT_EQ(result, set1);
    }

    {
        auto result = set1;
        result.addAll(set2);
        EXPECT_EQ(result, set3);
    }

    {
        auto result = set2;
        result.addAll(set1);
        EXPECT_EQ(result, set3);
    }

    {
        auto result = set1;
        result.addAll(sequence);
        EXPECT_EQ(result, set3);
    }
}

TEST(WTF_HashSet, IntersectionWith)
{
    HashSet<int> emptySet;
    HashSet<int> set1 { 1, 2, 3 };
    HashSet<int> set2 { 2, 3, 4 };
    HashSet<int> set3 { 2, 3 };
    Vector<int> sequence { 2, 3, 4 };

    {
        auto result = emptySet.intersectionWith(set1);
        EXPECT_EQ(result, emptySet);
    }

    {
        auto result = set1.intersectionWith(emptySet);
        EXPECT_EQ(result, emptySet);
    }

    {
        auto result = emptySet.intersectionWith(emptySet);
        EXPECT_EQ(result, emptySet);
    }

    {
        auto result = set1.intersectionWith(set1);
        EXPECT_EQ(result, set1);
    }

    {
        auto result = set1.intersectionWith(set2);
        EXPECT_EQ(result, set3);
    }

    {
        auto result = set2.intersectionWith(set1);
        EXPECT_EQ(result, set3);
    }

    {
        auto result = set1.intersectionWith(sequence);
        EXPECT_EQ(result, set3);
    }
}

TEST(WTF_HashSet, FormIntersection)
{
    HashSet<int> emptySet;
    HashSet<int> set1 { 1, 2, 3 };
    HashSet<int> set2 { 2, 3, 4 };
    HashSet<int> set3 { 2, 3 };
    Vector<int> sequence { 2, 3, 4 };

    {
        auto result = emptySet;
        result.formIntersection(set1);
        EXPECT_EQ(result, emptySet);
    }

    {
        auto result = set1;
        result.formIntersection(emptySet);
        EXPECT_EQ(result, emptySet);
    }

    {
        auto result = emptySet;
        result.formIntersection(emptySet);
        EXPECT_EQ(result, emptySet);
    }

    {
        auto result = set1;
        result.formIntersection(set1);
        EXPECT_EQ(result, set1);
    }

    {
        auto result = set1;
        result.formIntersection(set2);
        EXPECT_EQ(result, set3);
    }

    {
        auto result = set2;
        result.formIntersection(set1);
        EXPECT_EQ(result, set3);
    }

    {
        auto result = set1;
        result.formIntersection(sequence);
        EXPECT_EQ(result, set3);
    }
}

TEST(WTF_HashSet, DifferenceWith)
{
    HashSet<int> emptySet;
    HashSet<int> set1 { 1, 2, 3 };
    HashSet<int> set2 { 2, 3, 4 };
    Vector<int> sequence { 2, 3, 4 };

    {
        auto result = emptySet.differenceWith(set1);
        EXPECT_EQ(result, emptySet);
    }

    {
        auto result = set1.differenceWith(emptySet);
        EXPECT_EQ(result, set1);
    }

    {
        auto result = emptySet.differenceWith(emptySet);
        EXPECT_EQ(result, emptySet);
    }

    {
        auto result = set1.differenceWith(set1);
        EXPECT_EQ(result, emptySet);
    }

    {
        auto result = set1.differenceWith(set2);
        EXPECT_EQ(result, HashSet<int> { 1 });
    }

    {
        auto result = set2.differenceWith(set1);
        EXPECT_EQ(result, HashSet<int> { 4 });
    }

    {
        auto result = set1.differenceWith(sequence);
        EXPECT_EQ(result, HashSet<int> { 1 });
    }
}

TEST(WTF_HashSet, FormDifference)
{
    HashSet<int> emptySet;
    HashSet<int> set1 { 1, 2, 3 };
    HashSet<int> set2 { 2, 3, 4 };
    Vector<int> sequence { 2, 3, 4 };

    {
        auto result = emptySet;
        result.formDifference(set1);
        EXPECT_EQ(result, emptySet);
    }

    {
        auto result = set1;
        result.formDifference(emptySet);
        EXPECT_EQ(result, set1);
    }

    {
        auto result = emptySet;
        result.formDifference(emptySet);
        EXPECT_EQ(result, emptySet);
    }

    {
        auto result = set1;
        result.formDifference(set1);
        EXPECT_EQ(result, emptySet);
    }

    {
        auto result = set1;
        result.formDifference(set2);
        EXPECT_EQ(result, HashSet<int> { 1 });
    }

    {
        auto result = set2;
        result.formDifference(set1);
        EXPECT_EQ(result, HashSet<int> { 4 });
    }

    {
        auto result = set1;
        result.formDifference(sequence);
        EXPECT_EQ(result, HashSet<int> { 1 });
    }
}

TEST(WTF_HashSet, SymmetricDifferenceWith)
{
    HashSet<int> emptySet;
    HashSet<int> set1 { 1, 2, 3 };
    HashSet<int> set2 { 2, 3, 4 };
    HashSet<int> set3 { 1, 4 };
    Vector<int> sequence { 2, 3, 4 };

    {
        auto result = emptySet.symmetricDifferenceWith(set1);
        EXPECT_EQ(result, set1);
    }

    {
        auto result = set1.symmetricDifferenceWith(emptySet);
        EXPECT_EQ(result, set1);
    }

    {
        auto result = emptySet.symmetricDifferenceWith(emptySet);
        EXPECT_EQ(result, emptySet);
    }

    {
        auto result = set1.symmetricDifferenceWith(set1);
        EXPECT_EQ(result, emptySet);
    }

    {
        auto result = set1.symmetricDifferenceWith(set2);
        EXPECT_EQ(result, set3);
    }

    {
        auto result = set2.symmetricDifferenceWith(set1);
        EXPECT_EQ(result, set3);
    }

    {
        auto result = set1.symmetricDifferenceWith(sequence);
        EXPECT_EQ(result, set3);
    }
}

TEST(WTF_HashSet, FormSymmetricDifference)
{
    HashSet<int> emptySet;
    HashSet<int> set1 { 1, 2, 3 };
    HashSet<int> set2 { 2, 3, 4 };
    HashSet<int> set3 { 1, 4 };
    Vector<int> sequence { 2, 3, 4 };

    {
        auto result = emptySet;
        result.formSymmetricDifference(set1);
        EXPECT_EQ(result, set1);
    }

    {
        auto result = set1;
        result.formSymmetricDifference(emptySet);
        EXPECT_EQ(result, set1);
    }

    {
        auto result = emptySet;
        result.formSymmetricDifference(emptySet);
        EXPECT_EQ(result, emptySet);
    }

    {
        auto result = set1;
        result.formSymmetricDifference(set1);
        EXPECT_EQ(result, emptySet);
    }

    {
        auto result = set1;
        result.formSymmetricDifference(set2);
        EXPECT_EQ(result, set3);
    }

    {
        auto result = set2;
        result.formSymmetricDifference(set1);
        EXPECT_EQ(result, set3);
    }

    {
        auto result = set1;
        result.formSymmetricDifference(sequence);
        EXPECT_EQ(result, set3);
    }
}

TEST(WTF_HashSet, RangesAllAnyNoneOf)
{
    HashSet<int> set1 { 1, 2, 3 };
    EXPECT_TRUE(std::ranges::all_of(set1, [] (int el) {
        return el < 4;
    }));
    EXPECT_TRUE(std::ranges::none_of(set1, [] (int el) {
        return el > 4;
    }));
    EXPECT_TRUE(std::ranges::any_of(set1, [] (int el) {
        return el < 2;
    }));
}

class Object : public WTF::RefCountedAndCanMakeWeakPtr<Object> {
public:
    static Ref<Object> create() { return adoptRef(*new Object); }

private:
    Object() = default;
};

TEST(WTF_HashSet, WeakPtr)
{
    HashSet<WeakPtr<Object>> set;

    RefPtr object1 = Object::create();
    set.add(object1.get());

    Ref object2 = Object::create();

    // Present when live
    EXPECT_TRUE(set.contains(object1.get()));
    EXPECT_EQ(set.find(object1.get())->get(), object1.get());
    EXPECT_EQ(1u, set.size());
    for (auto& entry : set)
        EXPECT_EQ(entry, object1.get());

    EXPECT_FALSE(set.contains(&object2.get()));
    EXPECT_EQ(set.find(&object2.get()), set.end());

    Object* rawObject1 = object1.get();
    object1 = nullptr;

    // Absent when dead
    EXPECT_FALSE(set.contains(rawObject1));
    EXPECT_EQ(set.find(rawObject1), set.end());
    EXPECT_EQ(set.begin(), set.end());

    // Accurate size after removing weak nulls
    EXPECT_EQ(1u, set.size());
    set.removeWeakNullEntries();
    EXPECT_EQ(0u, set.size());

    // Bounded growth as added objects die
    for (size_t i = 0; i < 128; ++i)
        set.add(&Object::create().get());
    EXPECT_LT(set.size(), 16u);
}

TEST(WTF_HashSet, InlineWeakPtr)
{
    HashSet<InlineWeakPtr<InlineWeakPtrObject>> set;

    RefPtr object1 = InlineWeakPtrObject::create();
    set.add(object1.get());

    Ref object2 = InlineWeakPtrObject::create();

    // Present when live
    EXPECT_TRUE(set.contains(object1.get()));
    EXPECT_EQ(set.find(object1.get())->get(), object1.get());
    EXPECT_EQ(1u, set.size());
    for (auto& entry : set)
        EXPECT_EQ(entry, object1.get());

    EXPECT_FALSE(set.contains(&object2.get()));
    EXPECT_EQ(set.find(&object2.get()), set.end());

    InlineWeakPtrObject* rawObject1 = object1.get();
    object1 = nullptr;

    // Absent when dead
    EXPECT_FALSE(set.contains(rawObject1));
    EXPECT_EQ(set.find(rawObject1), set.end());
    EXPECT_EQ(set.begin(), set.end());

    // Accurate size after removing weak nulls
    EXPECT_EQ(1u, set.size());
    set.removeWeakNullEntries();
    EXPECT_EQ(0u, set.size());

    // Bounded growth as added objects die
    for (size_t i = 0; i < 128; ++i)
        set.add(&InlineWeakPtrObject::create().get());
    EXPECT_LT(set.size(), 16u);
}

} // namespace TestWebKitAPI

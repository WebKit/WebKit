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

#include "Utilities.h"
#include <wtf/CheckedPtr.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>
#include <wtf/WallTime.h>

namespace TestWebKitAPI {

class CheckedObject : public CanMakeCheckedPtr<CheckedObject> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(CheckedObject);
public:
    int someFunction() const { return -7; }
};

class DerivedCheckedObject : public CheckedObject {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(DerivedCheckedObject);
};

TEST(WTF_CheckedPtr, Basic)
{
    {
        auto checkedObject = makeUnique<CheckedObject>();
        EXPECT_EQ(checkedObject->ptrCount(), 0u);
    }

    {
        auto checkedObject = makeUnique<CheckedObject>();
        EXPECT_EQ(checkedObject->ptrCount(), 0u);
        {
            CheckedPtr ptr { checkedObject.get() };
            EXPECT_TRUE(!!ptr);
            EXPECT_EQ(ptr.get(), checkedObject.get());
            EXPECT_EQ(ptr->someFunction(), -7);
            EXPECT_EQ(checkedObject->ptrCount(), 1u);
        }
        EXPECT_EQ(checkedObject->ptrCount(), 0u);
    }

    {
        auto checkedObject = makeUnique<CheckedObject>();
        EXPECT_EQ(checkedObject->ptrCount(), 0u);

        CheckedPtr ptr = { checkedObject.get() };
        EXPECT_TRUE(!!ptr);
        EXPECT_EQ(ptr.get(), checkedObject.get());
        EXPECT_EQ(ptr->someFunction(), -7);
        EXPECT_EQ(checkedObject->ptrCount(), 1u);
        ptr = nullptr;

        EXPECT_FALSE(!!ptr);
        EXPECT_EQ(ptr.get(), nullptr);
        EXPECT_EQ(checkedObject->ptrCount(), 0u);
    }

    {
        auto checkedObject = makeUnique<CheckedObject>();
        EXPECT_EQ(checkedObject->ptrCount(), 0u);

        CheckedPtr ptr1 { checkedObject.get() };
        EXPECT_TRUE(!!ptr1);
        EXPECT_EQ(ptr1.get(), checkedObject.get());
        EXPECT_EQ(ptr1->someFunction(), -7);
        EXPECT_EQ(checkedObject->ptrCount(), 1u);

        const CheckedPtr ptr2 { checkedObject.get() };
        EXPECT_TRUE(!!ptr2);
        EXPECT_EQ(ptr2.get(), checkedObject.get());
        EXPECT_EQ(ptr2->someFunction(), -7);
        EXPECT_EQ(checkedObject->ptrCount(), 2u);

        CheckedPtr ptr3 = ptr2;
        EXPECT_TRUE(!!ptr3);
        EXPECT_EQ(ptr3.get(), checkedObject.get());
        EXPECT_EQ(checkedObject->ptrCount(), 3u);

        ptr1 = nullptr;
        EXPECT_EQ(checkedObject->ptrCount(), 2u);
        EXPECT_EQ(ptr1.get(), nullptr);
        EXPECT_EQ(ptr2.get(), checkedObject.get());
        EXPECT_EQ(ptr3.get(), checkedObject.get());

        ptr1 = WTFMove(ptr3);
        EXPECT_EQ(checkedObject->ptrCount(), 2u);
        EXPECT_EQ(ptr1.get(), checkedObject.get());
        EXPECT_EQ(ptr2.get(), checkedObject.get());
        SUPPRESS_USE_AFTER_MOVE EXPECT_EQ(ptr3.get(), nullptr);
    }
}

TEST(WTF_CheckedPtr, CheckedRef)
{
    {
        auto checkedObject = makeUnique<CheckedObject>();
        EXPECT_EQ(checkedObject->ptrCount(), 0u);
        {
            CheckedRef ref { *checkedObject };
            EXPECT_EQ(ref.ptr(), checkedObject.get());
            EXPECT_EQ(ref->someFunction(), -7);
            EXPECT_EQ(checkedObject->ptrCount(), 1u);
            CheckedPtr ptr { ref };
            EXPECT_EQ(ref.ptr(), checkedObject.get());
            EXPECT_EQ(ref->someFunction(), -7);
            EXPECT_EQ(ptr.get(), checkedObject.get());
            EXPECT_EQ(ptr->someFunction(), -7);
            EXPECT_EQ(checkedObject->ptrCount(), 2u);
        }
        EXPECT_EQ(checkedObject->ptrCount(), 0u);
    }

    {
        auto checkedObject = makeUnique<DerivedCheckedObject>();
        EXPECT_EQ(checkedObject->ptrCount(), 0u);
        {
            CheckedRef<DerivedCheckedObject> ref { *checkedObject };
            EXPECT_EQ(ref.ptr(), checkedObject.get());
            EXPECT_EQ(ref->someFunction(), -7);
            EXPECT_EQ(checkedObject->ptrCount(), 1u);
            CheckedPtr<CheckedObject> ptr { ref };
            EXPECT_EQ(ref.ptr(), checkedObject.get());
            EXPECT_EQ(ref->someFunction(), -7);
            EXPECT_EQ(ptr.get(), checkedObject.get());
            EXPECT_EQ(ptr->someFunction(), -7);
            EXPECT_EQ(checkedObject->ptrCount(), 2u);
        }
        EXPECT_EQ(checkedObject->ptrCount(), 0u);
    }

    {
        auto checkedObject = makeUnique<CheckedObject>();
        EXPECT_EQ(checkedObject->ptrCount(), 0u);
        {
            CheckedRef ref { *checkedObject };
            EXPECT_EQ(ref.ptr(), checkedObject.get());
            EXPECT_EQ(ref->someFunction(), -7);
            EXPECT_EQ(checkedObject->ptrCount(), 1u);
            CheckedPtr ptr { WTFMove(ref) };
            EXPECT_EQ(ptr.get(), checkedObject.get());
            EXPECT_EQ(ptr->someFunction(), -7);
            EXPECT_EQ(checkedObject->ptrCount(), 1u);
        }
        EXPECT_EQ(checkedObject->ptrCount(), 0u);
    }

    {
        auto checkedObject = makeUnique<DerivedCheckedObject>();
        EXPECT_EQ(checkedObject->ptrCount(), 0u);
        {
            CheckedRef<DerivedCheckedObject> ref { *checkedObject };
            EXPECT_EQ(ref.ptr(), checkedObject.get());
            EXPECT_EQ(ref->someFunction(), -7);
            EXPECT_EQ(checkedObject->ptrCount(), 1u);
            CheckedPtr<CheckedObject> ptr { WTFMove(ref) };
            EXPECT_EQ(ptr.get(), checkedObject.get());
            EXPECT_EQ(ptr->someFunction(), -7);
            EXPECT_EQ(checkedObject->ptrCount(), 1u);
        }
        EXPECT_EQ(checkedObject->ptrCount(), 0u);
    }
}

TEST(WTF_CheckedPtr, DerivedClass)
{
    {
        auto checkedObject = makeUnique<DerivedCheckedObject>();
        EXPECT_EQ(checkedObject->ptrCount(), 0u);
    }

    {
        auto checkedObject = makeUnique<DerivedCheckedObject>();
        EXPECT_EQ(checkedObject->ptrCount(), 0u);
        {
            CheckedPtr ptr = { checkedObject.get() };
            EXPECT_TRUE(!!ptr);
            EXPECT_EQ(ptr.get(), checkedObject.get());
            EXPECT_EQ(ptr->someFunction(), -7);
            EXPECT_EQ(checkedObject->ptrCount(), 1u);
        }
        EXPECT_EQ(checkedObject->ptrCount(), 0u);
    }

    {
        auto checkedObject = makeUnique<CheckedObject>();
        EXPECT_EQ(checkedObject->ptrCount(), 0u);

        CheckedPtr<CheckedObject> ptr { checkedObject.get() };
        EXPECT_TRUE(!!ptr);
        EXPECT_EQ(ptr.get(), checkedObject.get());
        EXPECT_EQ(ptr->someFunction(), -7);
        EXPECT_EQ(checkedObject->ptrCount(), 1u);
        ptr = nullptr;

        EXPECT_FALSE(!!ptr);
        EXPECT_EQ(ptr.get(), nullptr);
        EXPECT_EQ(checkedObject->ptrCount(), 0u);
    }

    {
        auto checkedObject = makeUnique<DerivedCheckedObject>();
        EXPECT_EQ(checkedObject->ptrCount(), 0u);

        CheckedPtr<DerivedCheckedObject> ptr1 { checkedObject.get() };
        EXPECT_TRUE(!!ptr1);
        EXPECT_EQ(ptr1.get(), checkedObject.get());
        EXPECT_EQ(ptr1->someFunction(), -7);
        EXPECT_EQ(checkedObject->ptrCount(), 1u);

        const CheckedPtr<CheckedObject> ptr2 = ptr1;
        EXPECT_TRUE(!!ptr2);
        EXPECT_EQ(ptr2.get(), checkedObject.get());
        EXPECT_EQ(ptr2->someFunction(), -7);
        EXPECT_EQ(checkedObject->ptrCount(), 2u);

        CheckedPtr<CheckedObject> ptr3 = ptr1;
        EXPECT_TRUE(!!ptr3);
        EXPECT_EQ(ptr3.get(), checkedObject.get());
        EXPECT_EQ(checkedObject->ptrCount(), 3u);

        ptr1 = nullptr;
        EXPECT_EQ(checkedObject->ptrCount(), 2u);
        EXPECT_EQ(ptr1.get(), nullptr);
        EXPECT_EQ(ptr2.get(), checkedObject.get());
        EXPECT_EQ(ptr3.get(), checkedObject.get());

        CheckedPtr<CheckedObject> ptr4 = WTFMove(ptr3);
        EXPECT_EQ(checkedObject->ptrCount(), 2u);
        EXPECT_EQ(ptr1.get(), nullptr);
        EXPECT_EQ(ptr2.get(), checkedObject.get());
        SUPPRESS_USE_AFTER_MOVE EXPECT_EQ(ptr3.get(), nullptr);
        EXPECT_EQ(ptr4.get(), checkedObject.get());
    }
}

TEST(WTF_CheckedPtr, HashSet)
{
    {
        auto checkedObject = makeUnique<CheckedObject>();
        EXPECT_EQ(checkedObject->ptrCount(), 0u);

        CheckedPtr ptr = { checkedObject.get() };
        EXPECT_EQ(ptr.get(), checkedObject.get());
        EXPECT_EQ(checkedObject->ptrCount(), 1u);

        HashSet<CheckedPtr<CheckedObject>> set;
        set.add(ptr);
        EXPECT_EQ(checkedObject->ptrCount(), 2u);

        ptr = nullptr;
        EXPECT_EQ(checkedObject->ptrCount(), 1u);
    }

    {
        auto object1 = makeUnique<CheckedObject>();
        auto object2 = makeUnique<DerivedCheckedObject>();
        EXPECT_EQ(object1->ptrCount(), 0u);
        EXPECT_EQ(object2->ptrCount(), 0u);

        HashSet<CheckedPtr<CheckedObject>> set;
        set.add(object1.get());
        EXPECT_EQ(object1->ptrCount(), 1u);
        EXPECT_EQ(object2->ptrCount(), 0u);

        set.add(object1.get());
        EXPECT_EQ(object1->ptrCount(), 1u);
        EXPECT_EQ(object2->ptrCount(), 0u);

        CheckedPtr<DerivedCheckedObject> ptr { object2.get() };
        set.add(ptr);
        EXPECT_EQ(object1->ptrCount(), 1u);
        EXPECT_EQ(object2->ptrCount(), 2u);
        ptr = nullptr;

        EXPECT_EQ(object1->ptrCount(), 1u);
        EXPECT_EQ(object2->ptrCount(), 1u);

        set.remove(object1.get());
        EXPECT_EQ(object1->ptrCount(), 0u);
        EXPECT_EQ(object2->ptrCount(), 1u);
    }

    {
        Vector<std::unique_ptr<CheckedObject>> objects;
        objects.append(makeUnique<CheckedObject>());
        EXPECT_EQ(objects[0]->ptrCount(), 0u);

        HashSet<CheckedPtr<CheckedObject>> set;
        set.add(objects[0].get());
        auto initialCapacity = set.capacity();

        for (unsigned i = 0; set.capacity() == initialCapacity; ++i) {
            if (i % 2)
                objects.append(makeUnique<DerivedCheckedObject>());
            else
                objects.append(makeUnique<CheckedObject>());
            set.add(objects.last().get());
        }

        for (auto& object : objects)
            EXPECT_EQ(object->ptrCount(), 1u);

        auto setVector = WTF::copyToVector(set);

        for (auto& object : objects)
            EXPECT_EQ(object->ptrCount(), 2u);
    }
}

TEST(WTF_CheckedPtr, ReferenceCountLimit)
{
    auto object = makeUnique<CheckedObject>();
    constexpr unsigned count = 256 * 1024;
    Vector<CheckedPtr<CheckedObject>> ptrs;
    ptrs.fill(object.get(), count);
    EXPECT_EQ(object->ptrCount(), count);
}

class ThreadSafeCheckedPtrObject final : public CanMakeThreadSafeCheckedPtr<ThreadSafeCheckedPtrObject> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ThreadSafeCheckedPtrObject);
public:
    std::atomic<unsigned> value { 0 };
};

TEST(WTF_CheckedPtr, CanMakeThreadSafeCheckedPtr)
{
    constexpr unsigned threadCount = 20;
    Vector<Ref<Thread>> threads;
    auto object = makeUnique<ThreadSafeCheckedPtrObject>();

    std::atomic<bool> allThreadsHaveStarted = false;
    Seconds startingTime;

    threads.reserveInitialCapacity(threadCount);
    for (unsigned i = 0; i < threadCount; ++i) {
        threads.append(Thread::create("CheckedPtr testing thread"_s, [&]() mutable {
            CheckedPtr ptr = object.get();
            do {
                for (unsigned i = 0; i < 1000; ++i) {
                    CheckedRef ref { *ptr };
                    ptr = nullptr;
                    ++ref->value;
                    ptr = ref;
                }
            } while (!allThreadsHaveStarted || WallTime::now().secondsSinceEpoch() - startingTime < Seconds::fromMilliseconds(5));
        }));
    }

    startingTime = WallTime::now().secondsSinceEpoch();
    allThreadsHaveStarted = true;

    for (auto& thread : threads)
        thread->waitForCompletion();
}

} // namespace TestWebKitAPI

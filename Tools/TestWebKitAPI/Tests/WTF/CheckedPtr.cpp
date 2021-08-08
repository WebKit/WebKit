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
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

class CheckedObject : public CanMakeCheckedPtr {
public:
    int someFunction() const { return -7; }
};

class DerivedCheckedObject : public CheckedObject {
};

TEST(WTF_CheckedPtr, Basic)
{
    {
        CheckedObject checkedObject;
        EXPECT_EQ(checkedObject.ptrCount(), 0);
    }

    {
        CheckedObject checkedObject;
        EXPECT_EQ(checkedObject.ptrCount(), 0);
        {
            CheckedPtr ptr { &checkedObject };
            EXPECT_TRUE(!!ptr);
            EXPECT_EQ(ptr.get(), &checkedObject);
            EXPECT_EQ(ptr->someFunction(), -7);
            EXPECT_EQ(checkedObject.ptrCount(), 1);
        }
        EXPECT_EQ(checkedObject.ptrCount(), 0);
    }

    {
        CheckedObject checkedObject;
        EXPECT_EQ(checkedObject.ptrCount(), 0);

        CheckedPtr ptr = { &checkedObject };
        EXPECT_TRUE(!!ptr);
        EXPECT_EQ(ptr.get(), &checkedObject);
        EXPECT_EQ(ptr->someFunction(), -7);
        EXPECT_EQ(checkedObject.ptrCount(), 1);
        ptr = nullptr;

        EXPECT_FALSE(!!ptr);
        EXPECT_EQ(ptr.get(), nullptr);
        EXPECT_EQ(checkedObject.ptrCount(), 0);
    }

    {
        CheckedObject checkedObject;
        EXPECT_EQ(checkedObject.ptrCount(), 0);

        CheckedPtr ptr1 { &checkedObject };
        EXPECT_TRUE(!!ptr1);
        EXPECT_EQ(ptr1.get(), &checkedObject);
        EXPECT_EQ(ptr1->someFunction(), -7);
        EXPECT_EQ(checkedObject.ptrCount(), 1);

        const CheckedPtr ptr2 { &checkedObject };
        EXPECT_TRUE(!!ptr2);
        EXPECT_EQ(ptr2.get(), &checkedObject);
        EXPECT_EQ(ptr2->someFunction(), -7);
        EXPECT_EQ(checkedObject.ptrCount(), 2);

        CheckedPtr ptr3 = ptr2;
        EXPECT_TRUE(!!ptr3);
        EXPECT_EQ(ptr3.get(), &checkedObject);
        EXPECT_EQ(checkedObject.ptrCount(), 3);

        ptr1 = nullptr;
        EXPECT_EQ(checkedObject.ptrCount(), 2);
        EXPECT_EQ(ptr1.get(), nullptr);
        EXPECT_EQ(ptr2.get(), &checkedObject);
        EXPECT_EQ(ptr3.get(), &checkedObject);

        ptr1 = WTFMove(ptr3);
        EXPECT_EQ(checkedObject.ptrCount(), 2);
        EXPECT_EQ(ptr1.get(), &checkedObject);
        EXPECT_EQ(ptr2.get(), &checkedObject);
        EXPECT_EQ(ptr3.get(), nullptr);
    }
}

TEST(WTF_CheckedPtr, CheckedRef)
{
    {
        CheckedObject checkedObject;
        EXPECT_EQ(checkedObject.ptrCount(), 0);
        {
            CheckedRef ref { checkedObject };
            EXPECT_EQ(ref.ptr(), &checkedObject);
            EXPECT_EQ(ref->someFunction(), -7);
            EXPECT_EQ(checkedObject.ptrCount(), 1);
            CheckedPtr ptr { ref };
            EXPECT_EQ(ref.ptr(), &checkedObject);
            EXPECT_EQ(ref->someFunction(), -7);
            EXPECT_EQ(ptr.get(), &checkedObject);
            EXPECT_EQ(ptr->someFunction(), -7);
            EXPECT_EQ(checkedObject.ptrCount(), 2);
        }
        EXPECT_EQ(checkedObject.ptrCount(), 0);
    }

    {
        DerivedCheckedObject checkedObject;
        EXPECT_EQ(checkedObject.ptrCount(), 0);
        {
            CheckedRef<DerivedCheckedObject> ref { checkedObject };
            EXPECT_EQ(ref.ptr(), &checkedObject);
            EXPECT_EQ(ref->someFunction(), -7);
            EXPECT_EQ(checkedObject.ptrCount(), 1);
            CheckedPtr<CheckedObject> ptr { ref };
            EXPECT_EQ(ref.ptr(), &checkedObject);
            EXPECT_EQ(ref->someFunction(), -7);
            EXPECT_EQ(ptr.get(), &checkedObject);
            EXPECT_EQ(ptr->someFunction(), -7);
            EXPECT_EQ(checkedObject.ptrCount(), 2);
        }
        EXPECT_EQ(checkedObject.ptrCount(), 0);
    }

    {
        CheckedObject checkedObject;
        EXPECT_EQ(checkedObject.ptrCount(), 0);
        {
            CheckedRef ref { checkedObject };
            EXPECT_EQ(ref.ptr(), &checkedObject);
            EXPECT_EQ(ref->someFunction(), -7);
            EXPECT_EQ(checkedObject.ptrCount(), 1);
            CheckedPtr ptr { WTFMove(ref) };
            EXPECT_EQ(ref.ptr(), nullptr);
            EXPECT_EQ(ptr.get(), &checkedObject);
            EXPECT_EQ(ptr->someFunction(), -7);
            EXPECT_EQ(checkedObject.ptrCount(), 1);
        }
        EXPECT_EQ(checkedObject.ptrCount(), 0);
    }

    {
        DerivedCheckedObject checkedObject;
        EXPECT_EQ(checkedObject.ptrCount(), 0);
        {
            CheckedRef<DerivedCheckedObject> ref { checkedObject };
            EXPECT_EQ(ref.ptr(), &checkedObject);
            EXPECT_EQ(ref->someFunction(), -7);
            EXPECT_EQ(checkedObject.ptrCount(), 1);
            CheckedPtr<CheckedObject> ptr { WTFMove(ref) };
            EXPECT_EQ(ref.ptr(), nullptr);
            EXPECT_EQ(ptr.get(), &checkedObject);
            EXPECT_EQ(ptr->someFunction(), -7);
            EXPECT_EQ(checkedObject.ptrCount(), 1);
        }
        EXPECT_EQ(checkedObject.ptrCount(), 0);
    }
}

TEST(WTF_CheckedPtr, DerivedClass)
{
    {
        DerivedCheckedObject checkedObject;
        EXPECT_EQ(checkedObject.ptrCount(), 0);
    }

    {
        DerivedCheckedObject checkedObject;
        EXPECT_EQ(checkedObject.ptrCount(), 0);
        {
            CheckedPtr ptr = { &checkedObject };
            EXPECT_TRUE(!!ptr);
            EXPECT_EQ(ptr.get(), &checkedObject);
            EXPECT_EQ(ptr->someFunction(), -7);
            EXPECT_EQ(checkedObject.ptrCount(), 1);
        }
        EXPECT_EQ(checkedObject.ptrCount(), 0);
    }

    {
        CheckedObject checkedObject;
        EXPECT_EQ(checkedObject.ptrCount(), 0);

        CheckedPtr<CheckedObject> ptr { &checkedObject };
        EXPECT_TRUE(!!ptr);
        EXPECT_EQ(ptr.get(), &checkedObject);
        EXPECT_EQ(ptr->someFunction(), -7);
        EXPECT_EQ(checkedObject.ptrCount(), 1);
        ptr = nullptr;

        EXPECT_FALSE(!!ptr);
        EXPECT_EQ(ptr.get(), nullptr);
        EXPECT_EQ(checkedObject.ptrCount(), 0);
    }

    {
        DerivedCheckedObject checkedObject;
        EXPECT_EQ(checkedObject.ptrCount(), 0);

        CheckedPtr<DerivedCheckedObject> ptr1 { &checkedObject };
        EXPECT_TRUE(!!ptr1);
        EXPECT_EQ(ptr1.get(), &checkedObject);
        EXPECT_EQ(ptr1->someFunction(), -7);
        EXPECT_EQ(checkedObject.ptrCount(), 1);

        const CheckedPtr<CheckedObject> ptr2 = ptr1;
        EXPECT_TRUE(!!ptr2);
        EXPECT_EQ(ptr2.get(), &checkedObject);
        EXPECT_EQ(ptr2->someFunction(), -7);
        EXPECT_EQ(checkedObject.ptrCount(), 2);

        CheckedPtr<CheckedObject> ptr3 = ptr1;
        EXPECT_TRUE(!!ptr3);
        EXPECT_EQ(ptr3.get(), &checkedObject);
        EXPECT_EQ(checkedObject.ptrCount(), 3);

        ptr1 = nullptr;
        EXPECT_EQ(checkedObject.ptrCount(), 2);
        EXPECT_EQ(ptr1.get(), nullptr);
        EXPECT_EQ(ptr2.get(), &checkedObject);
        EXPECT_EQ(ptr3.get(), &checkedObject);

        CheckedPtr<CheckedObject> ptr4 = WTFMove(ptr3);
        EXPECT_EQ(checkedObject.ptrCount(), 2);
        EXPECT_EQ(ptr1.get(), nullptr);
        EXPECT_EQ(ptr2.get(), &checkedObject);
        EXPECT_EQ(ptr3.get(), nullptr);
        EXPECT_EQ(ptr4.get(), &checkedObject);
    }
}

TEST(WTF_CheckedPtr, HashSet)
{
    {
        CheckedObject checkedObject;
        EXPECT_EQ(checkedObject.ptrCount(), 0);

        CheckedPtr ptr = { &checkedObject };
        EXPECT_EQ(ptr.get(), &checkedObject);
        EXPECT_EQ(checkedObject.ptrCount(), 1);

        HashSet<CheckedPtr<CheckedObject>> set;
        set.add(ptr);
        EXPECT_EQ(checkedObject.ptrCount(), 2);

        ptr = nullptr;
        EXPECT_EQ(checkedObject.ptrCount(), 1);
    }

    {
        CheckedObject object1;
        DerivedCheckedObject object2;
        EXPECT_EQ(object1.ptrCount(), 0);
        EXPECT_EQ(object2.ptrCount(), 0);

        HashSet<CheckedPtr<CheckedObject>> set;
        set.add(&object1);
        EXPECT_EQ(object1.ptrCount(), 1);
        EXPECT_EQ(object2.ptrCount(), 0);

        set.add(&object1);
        EXPECT_EQ(object1.ptrCount(), 1);
        EXPECT_EQ(object2.ptrCount(), 0);

        CheckedPtr<DerivedCheckedObject> ptr { &object2 };
        set.add(ptr);
        EXPECT_EQ(object1.ptrCount(), 1);
        EXPECT_EQ(object2.ptrCount(), 2);
        ptr = nullptr;

        EXPECT_EQ(object1.ptrCount(), 1);
        EXPECT_EQ(object2.ptrCount(), 1);

        set.remove(&object1);
        EXPECT_EQ(object1.ptrCount(), 0);
        EXPECT_EQ(object2.ptrCount(), 1);
    }

    {
        Vector<std::unique_ptr<CheckedObject>> objects;
        objects.append(makeUniqueWithoutFastMallocCheck<CheckedObject>());
        EXPECT_EQ(objects[0]->ptrCount(), 0);

        HashSet<CheckedPtr<CheckedObject>> set;
        set.add(objects[0].get());
        auto initialCapacity = set.capacity();

        for (unsigned i = 0; set.capacity() == initialCapacity; ++i) {
            if (i % 2)
                objects.append(makeUniqueWithoutFastMallocCheck<DerivedCheckedObject>());
            else
                objects.append(makeUniqueWithoutFastMallocCheck<CheckedObject>());
            set.add(objects.last().get());
        }

        for (auto& object : objects)
            EXPECT_EQ(object->ptrCount(), 1);

        auto setVector = WTF::copyToVector(set);

        for (auto& object : objects)
            EXPECT_EQ(object->ptrCount(), 2);
    }
}

} // namespace TestWebKitAPI

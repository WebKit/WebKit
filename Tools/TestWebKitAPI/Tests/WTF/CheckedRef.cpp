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
#include <wtf/CheckedRef.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

namespace {

class CheckedObject : public CanMakeCheckedPtr {
    WTF_MAKE_NONCOPYABLE(CheckedObject);
public:
    CheckedObject(int value = -7)
        : m_value(value)
    { }
    
    ~CheckedObject() = default;

    int someFunction() const { return m_value; }

private:
    int m_value;
};

class DerivedCheckedObject : public CheckedObject {
public:
    DerivedCheckedObject(int value = -11)
        : CheckedObject(value)
    { }
};

TEST(WTF_CheckedRef, Basic)
{
    {
        CheckedObject checkedObject;
        EXPECT_EQ(checkedObject.ptrCount(), 0);
    }

    {
        CheckedObject checkedObject;
        EXPECT_EQ(checkedObject.ptrCount(), 0);
        {
            CheckedRef ref = checkedObject;
            EXPECT_EQ(&ref.get(), &checkedObject);
            EXPECT_EQ(ref.ptr(), &checkedObject);
            EXPECT_EQ(ref->someFunction(), -7);
            EXPECT_EQ(static_cast<CheckedObject&>(ref).someFunction(), -7);
            EXPECT_EQ(checkedObject.ptrCount(), 1);
        }
        EXPECT_EQ(checkedObject.ptrCount(), 0);
    }

    {
        CheckedObject checkedObject;
        CheckedObject anotherObject;
        EXPECT_EQ(checkedObject.ptrCount(), 0);

        CheckedRef ref = checkedObject;
        EXPECT_EQ(ref.ptr(), &checkedObject);
        EXPECT_EQ(ref->someFunction(), -7);
        EXPECT_EQ(static_cast<CheckedObject&>(ref).someFunction(), -7);
        EXPECT_EQ(checkedObject.ptrCount(), 1);

        ref = anotherObject;
        EXPECT_EQ(ref.ptr(), &anotherObject);
        EXPECT_EQ(checkedObject.ptrCount(), 0);
        EXPECT_EQ(anotherObject.ptrCount(), 1);
    }

    {
        CheckedObject checkedObject;
        CheckedObject anotherObject;
        EXPECT_EQ(checkedObject.ptrCount(), 0);

        CheckedRef ref1 = checkedObject;
        EXPECT_EQ(ref1.ptr(), &checkedObject);
        EXPECT_EQ(ref1->someFunction(), -7);
        EXPECT_EQ(static_cast<CheckedObject&>(ref1).someFunction(), -7);
        EXPECT_EQ(checkedObject.ptrCount(), 1);

        const CheckedRef ref2 = checkedObject;
        EXPECT_EQ(ref2.ptr(), &checkedObject);
        EXPECT_EQ(ref2.get().someFunction(), -7);
        EXPECT_EQ(static_cast<const CheckedObject&>(ref2).someFunction(), -7);
        EXPECT_EQ(checkedObject.ptrCount(), 2);

        CheckedRef ref3 = ref2;
        EXPECT_EQ(ref3.ptr(), &checkedObject);
        EXPECT_EQ(checkedObject.ptrCount(), 3);

        ref1 = anotherObject;
        EXPECT_EQ(checkedObject.ptrCount(), 2);
        EXPECT_EQ(anotherObject.ptrCount(), 1);
        EXPECT_EQ(ref1.ptr(), &anotherObject);
        EXPECT_EQ(ref2.ptr(), &checkedObject);
        EXPECT_EQ(ref3.ptr(), &checkedObject);

        ref1 = WTFMove(ref3);
        EXPECT_EQ(checkedObject.ptrCount(), 2);
        EXPECT_EQ(anotherObject.ptrCount(), 0);
        EXPECT_EQ(ref1.ptr(), &checkedObject);
        EXPECT_EQ(ref2.ptr(), &checkedObject);
    }
}

TEST(WTF_CheckedRef, DerivedClass)
{
    {
        DerivedCheckedObject checkedObject;
        EXPECT_EQ(checkedObject.ptrCount(), 0);
        {
            CheckedRef ref = checkedObject;
            EXPECT_EQ(&ref.get(), &checkedObject);
            EXPECT_EQ(ref.ptr(), &checkedObject);
            EXPECT_EQ(static_cast<DerivedCheckedObject&>(ref).someFunction(), -11);
            EXPECT_EQ(ref->someFunction(), -11);
            EXPECT_EQ(checkedObject.ptrCount(), 1);
        }
        EXPECT_EQ(checkedObject.ptrCount(), 0);
    }

    {
        DerivedCheckedObject checkedObject;
        DerivedCheckedObject anotherObject;
        EXPECT_EQ(checkedObject.ptrCount(), 0);
        EXPECT_EQ(anotherObject.ptrCount(), 0);

        CheckedRef<CheckedObject> ref = checkedObject;
        EXPECT_EQ(ref.ptr(), &checkedObject);
        EXPECT_EQ(ref->someFunction(), -11);
        EXPECT_EQ(static_cast<CheckedObject&>(ref).someFunction(), -11);
        EXPECT_EQ(checkedObject.ptrCount(), 1);
        EXPECT_EQ(anotherObject.ptrCount(), 0);

        ref = anotherObject;
        EXPECT_EQ(ref.ptr(), &anotherObject);
        EXPECT_EQ(checkedObject.ptrCount(), 0);
        EXPECT_EQ(anotherObject.ptrCount(), 1);
    }

    {
        DerivedCheckedObject checkedObject { 10 };
        DerivedCheckedObject anotherObject { 20 };
        EXPECT_EQ(checkedObject.ptrCount(), 0);
        EXPECT_EQ(anotherObject.ptrCount(), 0);

        CheckedRef<DerivedCheckedObject> ref1 = checkedObject;
        EXPECT_EQ(ref1.ptr(), &checkedObject);
        EXPECT_EQ(ref1->someFunction(), 10);
        EXPECT_EQ(static_cast<CheckedObject&>(ref1).someFunction(), 10);
        EXPECT_EQ(checkedObject.ptrCount(), 1);
        EXPECT_EQ(anotherObject.ptrCount(), 0);

        const CheckedRef<CheckedObject> ref2 = ref1;
        EXPECT_EQ(ref2.ptr(), &checkedObject);
        EXPECT_EQ(ref2->someFunction(), 10);
        EXPECT_EQ(static_cast<const CheckedObject&>(ref2).someFunction(), 10);
        EXPECT_EQ(checkedObject.ptrCount(), 2);
        EXPECT_EQ(anotherObject.ptrCount(), 0);

        CheckedRef<CheckedObject> ref3 = ref1;
        EXPECT_EQ(ref3.ptr(), &checkedObject);
        EXPECT_EQ(static_cast<CheckedObject&>(ref3).someFunction(), 10);
        EXPECT_EQ(checkedObject.ptrCount(), 3);
        EXPECT_EQ(anotherObject.ptrCount(), 0);

        ref1 = anotherObject;
        EXPECT_EQ(checkedObject.ptrCount(), 2);
        EXPECT_EQ(anotherObject.ptrCount(), 1);
        EXPECT_EQ(ref1.ptr(), &anotherObject);
        EXPECT_EQ(ref1->someFunction(), 20);
        EXPECT_EQ(static_cast<CheckedObject&>(ref1).someFunction(), 20);
        EXPECT_EQ(ref2.ptr(), &checkedObject);
        EXPECT_EQ(ref3.ptr(), &checkedObject);

        CheckedRef<CheckedObject> ref4 = WTFMove(ref1);
        EXPECT_EQ(checkedObject.ptrCount(), 2);
        EXPECT_EQ(anotherObject.ptrCount(), 1);
        EXPECT_EQ(ref2.ptr(), &checkedObject);
        EXPECT_EQ(ref3.ptr(), &checkedObject);
        EXPECT_EQ(ref4.ptr(), &anotherObject);
        EXPECT_EQ(static_cast<CheckedObject&>(ref4).someFunction(), 20);
        EXPECT_EQ(ref4->someFunction(), 20);

        ref1 = checkedObject;
        EXPECT_EQ(checkedObject.ptrCount(), 3);
        EXPECT_EQ(anotherObject.ptrCount(), 1);
        EXPECT_EQ(ref1.ptr(), &checkedObject);
        EXPECT_EQ(ref1->someFunction(), 10);
        EXPECT_EQ(ref2.ptr(), &checkedObject);
        EXPECT_EQ(ref3.ptr(), &checkedObject);
    }
}

TEST(WTF_CheckedRef, HashSet)
{
    {
        CheckedObject checkedObject;
        EXPECT_EQ(checkedObject.ptrCount(), 0);

        HashSet<CheckedRef<CheckedObject>> set;
        {
            CheckedRef ref = checkedObject;
            EXPECT_EQ(ref.ptr(), &checkedObject);
            EXPECT_EQ(checkedObject.ptrCount(), 1);

            set.add(ref);
            EXPECT_EQ(checkedObject.ptrCount(), 2);
        }
        EXPECT_EQ(checkedObject.ptrCount(), 1);
    }

    {
        CheckedObject object1;
        DerivedCheckedObject object2;
        EXPECT_EQ(object1.ptrCount(), 0);
        EXPECT_EQ(object2.ptrCount(), 0);

        HashSet<CheckedRef<CheckedObject>> set;
        set.add(object1);
        EXPECT_EQ(object1.ptrCount(), 1);
        EXPECT_EQ(object2.ptrCount(), 0);
        EXPECT_TRUE(set.contains(object1));
        EXPECT_FALSE(set.contains(object2));

        set.add(object1);
        EXPECT_EQ(object1.ptrCount(), 1);
        EXPECT_EQ(object2.ptrCount(), 0);
        EXPECT_TRUE(set.contains(object1));
        EXPECT_FALSE(set.contains(object2));

        {
            CheckedRef<DerivedCheckedObject> ref { object2 };
            set.add(ref);
            EXPECT_EQ(object1.ptrCount(), 1);
            EXPECT_EQ(object2.ptrCount(), 2);
            EXPECT_TRUE(set.contains(object1));
            EXPECT_TRUE(set.contains(object2));
        }

        EXPECT_EQ(object1.ptrCount(), 1);
        EXPECT_EQ(object2.ptrCount(), 1);
        EXPECT_TRUE(set.contains(object1));
        EXPECT_TRUE(set.contains(object2));

        set.remove(object1);
        EXPECT_EQ(object1.ptrCount(), 0);
        EXPECT_EQ(object2.ptrCount(), 1);
        EXPECT_FALSE(set.contains(object1));
        EXPECT_TRUE(set.contains(object2));
    }

    {
        Vector<std::unique_ptr<CheckedObject>> objects;
        objects.append(makeUniqueWithoutFastMallocCheck<CheckedObject>(0));
        EXPECT_EQ(objects[0]->ptrCount(), 0);

        HashSet<CheckedRef<CheckedObject>> set;
        set.add(*objects[0].get());
        auto initialCapacity = set.capacity();

        for (unsigned i = 0; set.capacity() == initialCapacity; ++i) {
            if (i % 2)
                objects.append(makeUniqueWithoutFastMallocCheck<DerivedCheckedObject>(i + 1));
            else
                objects.append(makeUniqueWithoutFastMallocCheck<CheckedObject>(i + 1));
            set.add(*objects.last());
        }

        for (auto& object : objects) {
            EXPECT_EQ(object->ptrCount(), 1);
            EXPECT_TRUE(set.contains(*object));
        }

        Vector<bool> seen;
        seen.resize(objects.size());
        for (auto& item : set)
            seen[item->someFunction()] = true;
        for (auto value : seen)
            EXPECT_TRUE(value);

        auto setVector = WTF::copyToVector(set);

        for (auto& object : objects)
            EXPECT_EQ(object->ptrCount(), 2);
    }
}

}

} // namespace TestWebKitAPI

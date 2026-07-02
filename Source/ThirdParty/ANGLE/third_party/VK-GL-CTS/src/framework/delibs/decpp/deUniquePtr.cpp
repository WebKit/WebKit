/*-------------------------------------------------------------------------
 * drawElements C++ Base Library
 * -----------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Unique pointer.
 *//*--------------------------------------------------------------------*/

#include "deUniquePtr.hpp"

#include <exception>

namespace de
{

namespace
{

class Object
{
public:
    Object(bool &exists) : m_exists(exists)
    {
        m_exists = true;
    }

    ~Object(void)
    {
        m_exists = false;
    }

private:
    bool &m_exists;
};

struct CustomDeleter
{
    CustomDeleter(bool *called) : m_called(called)
    {
    }

    void operator()(Object *ptr)
    {
        DE_TEST_ASSERT(!*m_called);
        delete ptr;
        *m_called = true;
    }

    bool *m_called;
};

MovePtr<Object> createObject(bool &exists)
{
    UniquePtr<Object> objectPtr(new Object(exists));
    return objectPtr.move();
}

} // namespace

void UniquePtr_selfTest(void)
{
    // Basic test.
    {
        bool exists = false;
        {
            UniquePtr<Object> ptr(new Object(exists));
            DE_TEST_ASSERT(exists);
            DE_TEST_ASSERT(ptr.get() != nullptr);
        }
        DE_TEST_ASSERT(!exists);
    }

    // Exception test.
    {
        bool exists = false;
        try
        {
            UniquePtr<Object> ptr(new Object(exists));
            DE_TEST_ASSERT(exists);
            DE_TEST_ASSERT(ptr.get() != nullptr);
            throw std::exception();
        }
        catch (const std::exception &)
        {
            DE_TEST_ASSERT(!exists);
        }
        DE_TEST_ASSERT(!exists);
    }

    // Expression test.
    {
        bool exists = false;
        bool test   = (UniquePtr<Object>(new Object(exists))).get() != nullptr && exists;
        DE_TEST_ASSERT(!exists);
        DE_TEST_ASSERT(test);
    }

    // Custom deleter.
    {
        bool exists        = false;
        bool deleterCalled = false;
        {
            UniquePtr<Object, CustomDeleter> ptr(new Object(exists), CustomDeleter(&deleterCalled));
            DE_TEST_ASSERT(exists);
            DE_TEST_ASSERT(!deleterCalled);
            DE_TEST_ASSERT(ptr.get() != nullptr);
        }
        DE_TEST_ASSERT(!exists);
        DE_TEST_ASSERT(deleterCalled);
    }

    // MovePtr -> MovePtr moving
    {
        bool exists = false;
        MovePtr<Object> ptr(new Object(exists));
        DE_TEST_ASSERT(exists);
        {
            MovePtr<Object> ptr2 = ptr;
            DE_TEST_ASSERT(exists);
            // Ownership moved to ptr2, should be deleted when ptr2 goes out of scope.
        }
        DE_TEST_ASSERT(!exists);
    }

    // UniquePtr -> MovePtr moving
    {
        bool exists = false;
        UniquePtr<Object> ptr(new Object(exists));
        DE_TEST_ASSERT(exists);
        {
            MovePtr<Object> ptr2 = ptr.move();
            DE_TEST_ASSERT(exists);
            // Ownership moved to ptr2, should be deleted when ptr2 goes out of scope.
        }
        DE_TEST_ASSERT(!exists);
    }

    // MovePtr -> UniquePtr moving
    {
        bool exists = false;
        {
            UniquePtr<Object> ptr(createObject(exists));
            DE_TEST_ASSERT(exists);
        }
        DE_TEST_ASSERT(!exists);
    }

    // MovePtr assignment
    {
        bool exists1 = false;
        bool exists2 = false;
        MovePtr<Object> ptr1(new Object(exists1));
        MovePtr<Object> ptr2(new Object(exists2));
        ptr1 = ptr2;
        DE_TEST_ASSERT(!exists1);
        DE_TEST_ASSERT(exists2);
    }

    // MovePtr stealing
    {
        bool exists = false;
        Object *raw = nullptr;
        {
            MovePtr<Object> ptr1(new Object(exists));
            raw = ptr1.release();
            DE_TEST_ASSERT(raw != nullptr);
            DE_TEST_ASSERT(ptr1.get() == nullptr);
            DE_TEST_ASSERT(exists);
        }
        DE_TEST_ASSERT(exists);
        delete raw;
        DE_TEST_ASSERT(!exists);
    }

    // Null MovePtr and assigning to it.
    {
        bool exists = false;
        {
            MovePtr<Object> ptr1;
            DE_TEST_ASSERT(ptr1.get() == nullptr);
            MovePtr<Object> ptr2(new Object(exists));
            ptr1 = ptr2;
            DE_TEST_ASSERT(exists);
            DE_TEST_ASSERT(ptr1.get() != nullptr);
            DE_TEST_ASSERT(ptr2.get() == nullptr);
        }
        DE_TEST_ASSERT(!exists);
    }

#if 0
    // UniquePtr assignment or copy construction should not compile. This
    // piece of code is intentionally commented out. To manually test that
    // copying a UniquePtr is statically forbidden, uncomment and try to
    // compile.
    {
        bool exists = false;
        UniquePtr<Object> ptr(new Object(exists));
        {
            UniquePtr<Object> ptr2(ptr);
            DE_TEST_ASSERT(exists);
        }
        DE_TEST_ASSERT(!exists);
    }
#endif
}

} // namespace de

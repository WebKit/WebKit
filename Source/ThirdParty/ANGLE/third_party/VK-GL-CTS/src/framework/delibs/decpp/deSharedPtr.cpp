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
 * \brief Shared pointer.
 *//*--------------------------------------------------------------------*/

#include "deSharedPtr.hpp"
#include "deThread.hpp"
#include "deClock.h"

#include <exception>

namespace de
{

namespace
{

enum
{
    THREAD_TEST_TIME = 200 * 1000
};

class Object
{
public:
    Object(bool &exists) : m_exists(exists)
    {
        m_exists = true;
    }

    virtual ~Object(void)
    {
        m_exists = false;
    }

private:
    bool &m_exists;
};

class DerivedObject : public Object
{
public:
    DerivedObject(bool &exists) : Object(exists)
    {
    }
};

class SharedPtrTestThread : public Thread
{
public:
    SharedPtrTestThread(const SharedPtr<Object> &ptr, const bool &exists) : m_ptr(ptr), m_exists(exists)
    {
    }

    void run(void)
    {
        uint64_t startTime = deGetMicroseconds();
        uint64_t cnt       = 0;

        for (;; cnt++)
        {
            if (((cnt & (1 << 14)) != 0) && (deGetMicroseconds() - startTime >= THREAD_TEST_TIME))
                break;

            {
                SharedPtr<Object> ptrA(m_ptr);
                {
                    SharedPtr<Object> ptrB;
                    ptrB = ptrA;
                    ptrA = SharedPtr<Object>();
                }
            }
            DE_TEST_ASSERT(m_exists);
        }
    }

private:
    SharedPtr<Object> m_ptr;
    const bool &m_exists;
};

class WeakPtrTestThread : public Thread
{
public:
    WeakPtrTestThread(const SharedPtr<Object> &ptr, const bool &exists) : m_ptr(ptr), m_exists(exists)
    {
    }

    void run(void)
    {
        uint64_t startTime = deGetMicroseconds();
        uint64_t cnt       = 0;

        for (;; cnt++)
        {
            if (((cnt & (1 << 14)) != 0) && (deGetMicroseconds() - startTime >= THREAD_TEST_TIME))
                break;

            {
                WeakPtr<Object> ptrA(m_ptr);
                {
                    WeakPtr<Object> ptrB;
                    ptrB = ptrA;
                    ptrA = SharedPtr<Object>();
                }
            }
            DE_TEST_ASSERT(m_exists);
        }
    }

private:
    SharedPtr<Object> m_ptr;
    const bool &m_exists;
};

SharedPtr<Object> makeObject(bool &exists)
{
    return SharedPtr<Object>(new Object(exists));
}

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

} // namespace

void SharedPtr_selfTest(void)
{
    // Empty pointer test.
    {
        SharedPtr<Object> ptr;
        DE_TEST_ASSERT(ptr.get() == nullptr);
        DE_TEST_ASSERT(!ptr);
    }

    // Empty pointer copy.
    {
        SharedPtr<Object> ptrA;
        SharedPtr<Object> ptrB(ptrA);
        DE_TEST_ASSERT(ptrB.get() == nullptr);
    }

    // Empty pointer assignment.
    {
        SharedPtr<Object> ptrA;
        SharedPtr<Object> ptrB;
        ptrB = ptrA;
        ptrB = *&ptrB;
    }

    // Basic test.
    {
        bool exists = false;
        {
            SharedPtr<Object> ptr(new Object(exists));
            DE_TEST_ASSERT(exists);
            DE_TEST_ASSERT(ptr.get() != nullptr);
            DE_TEST_ASSERT(ptr);
        }
        DE_TEST_ASSERT(!exists);
    }

    // Exception test.
    {
        bool exists = false;
        try
        {
            SharedPtr<Object> ptr(new Object(exists));
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
        bool test   = (SharedPtr<Object>(new Object(exists))).get() != nullptr && exists;
        DE_TEST_ASSERT(!exists);
        DE_TEST_ASSERT(test);
    }

    // Assignment test.
    {
        bool exists = false;
        SharedPtr<Object> ptr(new Object(exists));
        DE_TEST_ASSERT(exists);
        ptr = SharedPtr<Object>();
        DE_TEST_ASSERT(!exists);
    }

    // Self-assignment test.
    {
        bool exists = false;
        {
            SharedPtr<Object> ptr(new Object(exists));
            DE_TEST_ASSERT(exists);
            DE_TEST_ASSERT(ptr.get() != nullptr);
            ptr = *&ptr;
        }
        DE_TEST_ASSERT(!exists);
    }

    // Basic multi-reference via copy ctor.
    {
        bool exists = false;
        {
            SharedPtr<Object> ptrA(new Object(exists));
            DE_TEST_ASSERT(exists);
            {
                SharedPtr<Object> ptrB(ptrA);
                DE_TEST_ASSERT(exists);
            }
            DE_TEST_ASSERT(exists);
        }
        DE_TEST_ASSERT(!exists);
    }

    // Basic multi-reference via assignment to empty.
    {
        bool exists = false;
        {
            SharedPtr<Object> ptrA(new Object(exists));
            DE_TEST_ASSERT(exists);
            {
                SharedPtr<Object> ptrB;
                ptrB = ptrA;
                DE_TEST_ASSERT(exists);
            }
            DE_TEST_ASSERT(exists);
        }
        DE_TEST_ASSERT(!exists);
    }

    // Multi-reference via assignment to non-empty.
    {
        bool existsA = false;
        bool existsB = false;
        {
            SharedPtr<Object> ptrA(new Object(existsA));
            DE_TEST_ASSERT(existsA);
            {
                SharedPtr<Object> ptrB(new Object(existsB));
                DE_TEST_ASSERT(existsB);
                ptrA = ptrB;
                DE_TEST_ASSERT(!existsA);
                DE_TEST_ASSERT(existsB);
            }
            DE_TEST_ASSERT(existsB);
        }
        DE_TEST_ASSERT(!existsB);
    }

    // Return from function.
    {
        bool exists = false;
        {
            SharedPtr<Object> ptr;
            ptr = makeObject(exists);
            DE_TEST_ASSERT(exists);
        }
        DE_TEST_ASSERT(!exists);
    }

    // Equality comparison.
    {
        bool existsA = false;
        bool existsB = false;
        SharedPtr<Object> ptrA(new Object(existsA));
        SharedPtr<Object> ptrB(new Object(existsB));
        SharedPtr<Object> ptrC(ptrA);

        DE_TEST_ASSERT(ptrA == ptrA);
        DE_TEST_ASSERT(ptrA != ptrB);
        DE_TEST_ASSERT(ptrA == ptrC);
        DE_TEST_ASSERT(ptrC != ptrB);
    }

    // Conversion via assignment.
    {
        bool exists = false;
        {
            SharedPtr<Object> basePtr;
            {
                SharedPtr<DerivedObject> derivedPtr(new DerivedObject(exists));
                DE_TEST_ASSERT(exists);
                basePtr = derivedPtr;
                DE_TEST_ASSERT(exists);
            }
            DE_TEST_ASSERT(exists);
        }
        DE_TEST_ASSERT(!exists);
    }

    // Conversion via copy ctor.
    {
        bool exists = false;
        {
            SharedPtr<DerivedObject> derivedPtr(new DerivedObject(exists));
            SharedPtr<Object> basePtr(derivedPtr);
            DE_TEST_ASSERT(exists);
            derivedPtr = SharedPtr<DerivedObject>();
            DE_TEST_ASSERT(exists);
        }
        DE_TEST_ASSERT(!exists);
    }

    // Explicit conversion operator.
    {
        bool exists = false;
        {
            SharedPtr<DerivedObject> derivedPtr(new DerivedObject(exists));
            DE_TEST_ASSERT(exists);

            SharedPtr<Object> basePtr = (SharedPtr<Object>)(derivedPtr);
            derivedPtr                = SharedPtr<DerivedObject>();
            DE_TEST_ASSERT(exists);
        }
        DE_TEST_ASSERT(!exists);
    }

    // Basic weak reference.
    {
        bool exists = false;
        SharedPtr<Object> ptr(new Object(exists));
        DE_TEST_ASSERT(exists);

        WeakPtr<Object> weakPtr(ptr);
        try
        {
            SharedPtr<Object> newRef(weakPtr);
            DE_TEST_ASSERT(exists);
        }
        catch (const DeadReferenceException &)
        {
            DE_TEST_ASSERT(false);
        }

        ptr = SharedPtr<Object>();
        DE_TEST_ASSERT(!exists);
        try
        {
            SharedPtr<Object> newRef(weakPtr);
            DE_TEST_ASSERT(false);
        }
        catch (const DeadReferenceException &)
        {
        }
    }

    // Basic SharedPtr threaded test.
    {
        bool exists = false;
        {
            SharedPtr<Object> ptr(new Object(exists));

            SharedPtrTestThread threadA(ptr, exists);
            SharedPtrTestThread threadB(ptr, exists);

            threadA.start();
            threadB.start();

            threadA.join();
            threadB.join();
            DE_TEST_ASSERT(exists);
        }
        DE_TEST_ASSERT(!exists);
    }

    // Basic WeakPtr threaded test.
    {
        bool exists = false;
        {
            SharedPtr<Object> ptr(new Object(exists));
            WeakPtrTestThread threadA(ptr, exists);
            WeakPtrTestThread threadB(ptr, exists);

            threadA.start();
            threadB.start();

            threadA.join();
            threadB.join();
            DE_TEST_ASSERT(exists);
        }
        DE_TEST_ASSERT(!exists);
    }

    // Basic custom deleter.
    {
        bool exists        = false;
        bool deleterCalled = false;
        {
            SharedPtr<Object> ptr(new Object(exists), CustomDeleter(&deleterCalled));
            DE_TEST_ASSERT(exists);
            DE_TEST_ASSERT(!deleterCalled);
            DE_TEST_ASSERT(ptr.get() != nullptr);
        }
        DE_TEST_ASSERT(!exists);
        DE_TEST_ASSERT(deleterCalled);
    }
}

} // namespace de

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
 * \brief Array buffer
 *//*--------------------------------------------------------------------*/

#include "deArrayBuffer.hpp"

#if defined(DE_VALGRIND_BUILD) && defined(HAVE_VALGRIND_MEMCHECK_H)
#include <valgrind/memcheck.h>
#endif

namespace de
{
namespace detail
{

void *ArrayBuffer_AlignedMalloc(size_t numBytes, size_t alignment)
{
    const int sizeAsInt = (int)numBytes;
    void *ptr;

    // int overflow
    if (sizeAsInt < 0 || numBytes != (size_t)sizeAsInt)
        throw std::bad_alloc();

    // alloc
    ptr = deAlignedMalloc(sizeAsInt, (int)alignment);
    if (!ptr)
        throw std::bad_alloc();

        // mark area as undefined for valgrind
#if defined(DE_VALGRIND_BUILD) && defined(HAVE_VALGRIND_MEMCHECK_H)
    if (RUNNING_ON_VALGRIND)
    {
        VALGRIND_MAKE_MEM_UNDEFINED(ptr, numBytes);
    }
#endif

    return ptr;
}

void ArrayBuffer_AlignedFree(void *ptr)
{
    deAlignedFree(ptr);
}

} // namespace detail

void ArrayBuffer_selfTest(void)
{
    // default constructor
    {
        de::ArrayBuffer<int> buf;
        DE_TEST_ASSERT(buf.size() == 0);
        DE_TEST_ASSERT(buf.getPtr() == nullptr);
    }

    // sized constructor
    {
        de::ArrayBuffer<int> buf(4);
        DE_TEST_ASSERT(buf.size() == 4);
        DE_TEST_ASSERT(buf.getPtr() != nullptr);
    }

    // copy constructor
    {
        de::ArrayBuffer<int> originalBuf(4);
        *originalBuf.getElementPtr(0) = 1;
        *originalBuf.getElementPtr(1) = 2;
        *originalBuf.getElementPtr(2) = 3;
        *originalBuf.getElementPtr(3) = 4;

        de::ArrayBuffer<int> targetBuf(originalBuf);

        DE_TEST_ASSERT(*originalBuf.getElementPtr(0) == 1);
        DE_TEST_ASSERT(*originalBuf.getElementPtr(1) == 2);
        DE_TEST_ASSERT(*originalBuf.getElementPtr(2) == 3);
        DE_TEST_ASSERT(*originalBuf.getElementPtr(3) == 4);

        DE_TEST_ASSERT(*targetBuf.getElementPtr(0) == 1);
        DE_TEST_ASSERT(*targetBuf.getElementPtr(1) == 2);
        DE_TEST_ASSERT(*targetBuf.getElementPtr(2) == 3);
        DE_TEST_ASSERT(*targetBuf.getElementPtr(3) == 4);
    }

    // assignment
    {
        de::ArrayBuffer<int> originalBuf(4);
        *originalBuf.getElementPtr(0) = 1;
        *originalBuf.getElementPtr(1) = 2;
        *originalBuf.getElementPtr(2) = 3;
        *originalBuf.getElementPtr(3) = 4;

        de::ArrayBuffer<int> targetBuf(1);

        targetBuf = originalBuf;

        DE_TEST_ASSERT(*originalBuf.getElementPtr(0) == 1);
        DE_TEST_ASSERT(*originalBuf.getElementPtr(1) == 2);
        DE_TEST_ASSERT(*originalBuf.getElementPtr(2) == 3);
        DE_TEST_ASSERT(*originalBuf.getElementPtr(3) == 4);

        DE_TEST_ASSERT(*targetBuf.getElementPtr(0) == 1);
        DE_TEST_ASSERT(*targetBuf.getElementPtr(1) == 2);
        DE_TEST_ASSERT(*targetBuf.getElementPtr(2) == 3);
        DE_TEST_ASSERT(*targetBuf.getElementPtr(3) == 4);
    }

    // clear
    {
        de::ArrayBuffer<int> buf(4);
        buf.clear();
        DE_TEST_ASSERT(buf.size() == 0);
        DE_TEST_ASSERT(buf.getPtr() == nullptr);
    }

    // setStorage
    {
        de::ArrayBuffer<int> buf(4);
        buf.setStorage(12);
        DE_TEST_ASSERT(buf.size() == 12);
        DE_TEST_ASSERT(buf.getPtr() != nullptr);
    }

    // setStorage, too large
    {
        de::ArrayBuffer<int> buf(4);
        *buf.getElementPtr(0) = 1;
        *buf.getElementPtr(1) = 2;
        *buf.getElementPtr(2) = 3;
        *buf.getElementPtr(3) = 4;

        try
        {
            buf.setStorage((size_t)-1);

            // setStorage succeeded, all ok
        }
        catch (std::bad_alloc &)
        {
            // alloc failed, check storage not changed

            DE_TEST_ASSERT(buf.size() == 4);
            DE_TEST_ASSERT(*buf.getElementPtr(0) == 1);
            DE_TEST_ASSERT(*buf.getElementPtr(1) == 2);
            DE_TEST_ASSERT(*buf.getElementPtr(2) == 3);
            DE_TEST_ASSERT(*buf.getElementPtr(3) == 4);
        }
    }

    // swap
    {
        de::ArrayBuffer<int> buf;
        de::ArrayBuffer<int> source(4);
        *source.getElementPtr(0) = 1;
        *source.getElementPtr(1) = 2;
        *source.getElementPtr(2) = 3;
        *source.getElementPtr(3) = 4;

        buf.swap(source);

        DE_TEST_ASSERT(source.size() == 0);
        DE_TEST_ASSERT(buf.size() == 4);
        DE_TEST_ASSERT(*buf.getElementPtr(0) == 1);
        DE_TEST_ASSERT(*buf.getElementPtr(1) == 2);
        DE_TEST_ASSERT(*buf.getElementPtr(2) == 3);
        DE_TEST_ASSERT(*buf.getElementPtr(3) == 4);
    }

    // default
    {
        de::ArrayBuffer<int> source(4);
        int dst;
        *source.getElementPtr(1) = 2;

        deMemcpy(&dst, (int *)source.getPtr() + 1, sizeof(int));

        DE_TEST_ASSERT(dst == 2);
    }

    // Aligned
    {
        de::ArrayBuffer<int, 64, sizeof(int)> source(4);
        int dst;
        *source.getElementPtr(1) = 2;

        deMemcpy(&dst, (int *)source.getPtr() + 1, sizeof(int));

        DE_TEST_ASSERT(dst == 2);
    }

    // Strided
    {
        de::ArrayBuffer<int, 4, 64> source(4);
        int dst;
        *source.getElementPtr(1) = 2;

        deMemcpy(&dst, (uint8_t *)source.getPtr() + 64, sizeof(int));

        DE_TEST_ASSERT(dst == 2);
    }

    // Aligned, Strided
    {
        de::ArrayBuffer<int, 32, 64> source(4);
        int dst;
        *source.getElementPtr(1) = 2;

        deMemcpy(&dst, (uint8_t *)source.getPtr() + 64, sizeof(int));

        DE_TEST_ASSERT(dst == 2);
    }
}

} // namespace de

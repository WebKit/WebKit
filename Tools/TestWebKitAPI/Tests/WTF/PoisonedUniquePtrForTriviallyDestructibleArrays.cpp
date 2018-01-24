/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <mutex>
#include <wtf/FastMalloc.h>
#include <wtf/PoisonedUniquePtr.h>

namespace TestWebKitAPI {

namespace {

uintptr_t g_poisonA;
uintptr_t g_poisonB;

using PoisonA = Poison<g_poisonA>;
using PoisonB = Poison<g_poisonB>;

static void initializePoisons()
{
    static std::once_flag initializeOnceFlag;
    std::call_once(initializeOnceFlag, [] {
        // Make sure we get 2 different poison values.
        g_poisonA = makePoison();
        while (!g_poisonB || g_poisonB == g_poisonA)
            g_poisonB = makePoison();
    });
}

template<typename T>
static void fillArray(T& array, int size)
{
    for (int i = 0; i < size; ++i)
        array[i] = i + 100;
}

static const int arraySize = 5;

} // anonymous namespace

TEST(WTF_PoisonedUniquePtrForTriviallyDestructibleArrays, Basic)
{
    initializePoisons();

    {
        PoisonedUniquePtr<PoisonA, int[]> empty;
        ASSERT_EQ(nullptr, empty.unpoisoned());
        ASSERT_EQ(0u, empty.bits());
    }
    {
        PoisonedUniquePtr<PoisonA, int[]> empty(nullptr);
        ASSERT_EQ(nullptr, empty.unpoisoned());
        ASSERT_EQ(0u, empty.bits());
    }

    {
        auto* a = new int[arraySize];
        fillArray(a, arraySize);
        {
            PoisonedUniquePtr<PoisonA, int[]> ptr(a);
            ASSERT_EQ(a, ptr.unpoisoned());
            ASSERT_EQ(a, &*ptr);
            for (auto i = 0; i < arraySize; ++i)
                ASSERT_EQ(a[i], ptr[i]);

#if ENABLE(POISON)
            uintptr_t ptrBits;
            std::memcpy(&ptrBits, &ptr, sizeof(ptrBits));
            ASSERT_TRUE(ptrBits != bitwise_cast<uintptr_t>(a));
#if ENABLE(POISON_ASSERTS)
            ASSERT_TRUE((PoisonedUniquePtr<PoisonA, int[]>::isPoisoned(ptrBits)));
#endif
#endif // ENABLE(POISON)
        }
    }

    {
        auto* a = new int[arraySize];
        fillArray(a, arraySize);

        PoisonedUniquePtr<PoisonA, int[]> ptr = a;
        ASSERT_EQ(a, ptr.unpoisoned());
        ASSERT_EQ(a, &*ptr);
        for (auto i = 0; i < arraySize; ++i)
            ASSERT_EQ(a[i], ptr[i]);
    }

    {
        PoisonedUniquePtr<PoisonA, int[]> ptr = PoisonedUniquePtr<PoisonA, int[]>::create(arraySize);
        ASSERT_TRUE(nullptr != ptr.unpoisoned());
        fillArray(ptr, arraySize);
        for (auto i = 0; i < arraySize; ++i)
            ASSERT_EQ((100 + i), ptr[i]);
    }

    {
        auto* a = new int[arraySize];
        fillArray(a, arraySize);
        auto* b = new int[arraySize];
        fillArray(b, arraySize);

        PoisonedUniquePtr<PoisonA, int[]> p1 = a;
        PoisonedUniquePtr<PoisonA, int[]> p2 = WTFMove(p1);
        ASSERT_EQ(nullptr, p1.unpoisoned());
        ASSERT_EQ(0u, p1.bits());
        ASSERT_EQ(a, p2.unpoisoned());
        for (auto i = 0; i < arraySize; ++i)
            ASSERT_EQ(a[i], p2[i]);

        PoisonedUniquePtr<PoisonA, int[]> p3 = b;
        PoisonedUniquePtr<PoisonB, int[]> p4 = WTFMove(p3);
        ASSERT_EQ(nullptr, p3.unpoisoned());
        ASSERT_EQ(0u, p3.bits());
        ASSERT_EQ(b, p4.unpoisoned());
        for (auto i = 0; i < arraySize; ++i)
            ASSERT_EQ(b[i], p4[i]);
    }

    {
        auto* a = new int[arraySize];
        fillArray(a, arraySize);
        auto* b = new int[arraySize];
        fillArray(b, arraySize);

        PoisonedUniquePtr<PoisonA, int[]> p1 = a;
        PoisonedUniquePtr<PoisonA, int[]> p2(WTFMove(p1));
        ASSERT_EQ(nullptr, p1.unpoisoned());
        ASSERT_EQ(0u, p1.bits());
        ASSERT_EQ(a, p2.unpoisoned());
        for (auto i = 0; i < arraySize; ++i)
            ASSERT_EQ(a[i], p2[i]);

        PoisonedUniquePtr<PoisonA, int[]> p3 = b;
        PoisonedUniquePtr<PoisonB, int[]> p4(WTFMove(p3));
        ASSERT_EQ(nullptr, p3.unpoisoned());
        ASSERT_EQ(0u, p3.bits());
        ASSERT_EQ(b, p4.unpoisoned());
        for (auto i = 0; i < arraySize; ++i)
            ASSERT_EQ(b[i], p4[i]);
    }

    {
        auto* a = new int[arraySize];
        fillArray(a, arraySize);

        PoisonedUniquePtr<PoisonA, int[]> ptr(a);
        ASSERT_EQ(a, ptr.unpoisoned());
        ptr.clear();
        ASSERT_TRUE(!ptr.unpoisoned());
        ASSERT_TRUE(!ptr.bits());
    }
}

TEST(WTF_PoisonedUniquePtrForTriviallyDestructibleArrays, Assignment)
{
    initializePoisons();

    {
        auto* a = new int[arraySize];
        auto* b = new int[arraySize];
        fillArray(a, arraySize);
        fillArray(b, arraySize);

        PoisonedUniquePtr<PoisonA, int[]> ptr(a);
        ASSERT_EQ(a, ptr.unpoisoned());
        ptr = b;
        ASSERT_EQ(b, ptr.unpoisoned());
    }

    {
        auto* a = new int[arraySize];
        fillArray(a, arraySize);

        PoisonedUniquePtr<PoisonA, int[]> ptr(a);
        ASSERT_EQ(a, ptr.unpoisoned());
        ptr = nullptr;
        ASSERT_EQ(nullptr, ptr.unpoisoned());
    }

    {
        auto* a = new int[arraySize];
        auto* b = new int[arraySize];
        auto* c = new int[arraySize];
        auto* d = new int[arraySize];
        fillArray(a, arraySize);
        fillArray(b, arraySize);
        fillArray(c, arraySize);
        fillArray(d, arraySize);

        PoisonedUniquePtr<PoisonA, int[]> p1(a);
        PoisonedUniquePtr<PoisonA, int[]> p2(b);
        ASSERT_EQ(a, p1.unpoisoned());
        ASSERT_EQ(b, p2.unpoisoned());
        p1 = WTFMove(p2);
        ASSERT_EQ(b, p1.unpoisoned());
        ASSERT_EQ(nullptr, p2.unpoisoned());

        PoisonedUniquePtr<PoisonA, int[]> p3(c);
        PoisonedUniquePtr<PoisonB, int[]> p4(d);
        ASSERT_EQ(c, p3.unpoisoned());
        ASSERT_EQ(d, p4.unpoisoned());
        p3 = WTFMove(p4);
        ASSERT_EQ(d, p3.unpoisoned());
        ASSERT_EQ(nullptr, p4.unpoisoned());
    }

    {
        auto* a = new int[arraySize];
        fillArray(a, arraySize);

        PoisonedUniquePtr<PoisonA, int[]> ptr(a);
        ASSERT_EQ(a, ptr.unpoisoned());
        ptr = a;
        ASSERT_EQ(a, ptr.unpoisoned());
    }

    {
        auto* a = new int[arraySize];
        fillArray(a, arraySize);

        PoisonedUniquePtr<PoisonA, int[]> ptr(a);
        ASSERT_EQ(a, ptr.unpoisoned());
#if COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wself-move"
#endif
        ptr = WTFMove(ptr);
#if COMPILER(CLANG)
#pragma clang diagnostic pop
#endif
        ASSERT_EQ(a, ptr.unpoisoned());
    }
}

TEST(WTF_PoisonedUniquePtrForTriviallyDestructibleArrays, Swap)
{
    initializePoisons();

    {
        auto* a = new int[arraySize];
        auto* b = new int[arraySize];
        fillArray(a, arraySize);
        fillArray(b, arraySize);

        PoisonedUniquePtr<PoisonA, int[]> p1 = a;
        PoisonedUniquePtr<PoisonA, int[]> p2;
        ASSERT_EQ(p1.unpoisoned(), a);
        ASSERT_TRUE(!p2.bits());
        ASSERT_TRUE(!p2.unpoisoned());
        p2.swap(p1);
        ASSERT_TRUE(!p1.bits());
        ASSERT_TRUE(!p1.unpoisoned());
        ASSERT_EQ(p2.unpoisoned(), a);

        PoisonedUniquePtr<PoisonA, int[]> p3 = b;
        PoisonedUniquePtr<PoisonB, int[]> p4;
        ASSERT_EQ(p3.unpoisoned(), b);
        ASSERT_TRUE(!p4.bits());
        ASSERT_TRUE(!p4.unpoisoned());
        p4.swap(p3);
        ASSERT_TRUE(!p3.bits());
        ASSERT_TRUE(!p3.unpoisoned());
        ASSERT_EQ(p4.unpoisoned(), b);
    }

    {
        auto* a = new int[arraySize];
        auto* b = new int[arraySize];
        fillArray(a, arraySize);
        fillArray(b, arraySize);

        PoisonedUniquePtr<PoisonA, int[]> p1 = a;
        PoisonedUniquePtr<PoisonA, int[]> p2;
        ASSERT_EQ(p1.unpoisoned(), a);
        ASSERT_TRUE(!p2.bits());
        ASSERT_TRUE(!p2.unpoisoned());
        swap(p1, p2);
        ASSERT_TRUE(!p1.bits());
        ASSERT_TRUE(!p1.unpoisoned());
        ASSERT_EQ(p2.unpoisoned(), a);

        PoisonedUniquePtr<PoisonA, int[]> p3 = b;
        PoisonedUniquePtr<PoisonB, int[]> p4;
        ASSERT_EQ(p3.unpoisoned(), b);
        ASSERT_TRUE(!p4.bits());
        ASSERT_TRUE(!p4.unpoisoned());
        swap(p3, p4);
        ASSERT_TRUE(!p3.bits());
        ASSERT_TRUE(!p3.unpoisoned());
        ASSERT_EQ(p4.unpoisoned(), b);
    }
}

static PoisonedUniquePtr<PoisonA, int[]> poisonedPtrFoo(int* ptr)
{
    return PoisonedUniquePtr<PoisonA, int[]>(ptr);
}

TEST(WTF_PoisonedUniquePtrForTriviallyDestructibleArrays, ReturnValue)
{
    initializePoisons();

    {
        auto* a = new int[arraySize];
        fillArray(a, arraySize);

        auto ptr = poisonedPtrFoo(a);
        ASSERT_EQ(a, ptr.unpoisoned());
        ASSERT_EQ(a, &*ptr);
        for (auto i = 0; i < arraySize; ++i)
            ASSERT_EQ(a[i], ptr[i]);
    }
}

} // namespace TestWebKitAPI


/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "RefLogger.h"
#include <mutex>
#include <wtf/Poisoned.h>

namespace TestWebKitAPI {

uintptr_t g_testPoisonA;
uintptr_t g_testPoisonB;

static void initializeTestPoison()
{
    static std::once_flag initializeOnceFlag;
    std::call_once(initializeOnceFlag, [] {
        // Make sure we get 2 different poison values.
        g_testPoisonA = makePoison();
        while (!g_testPoisonB || g_testPoisonB == g_testPoisonA)
            g_testPoisonB = makePoison();
    });
}

// For these tests, we need a base class and a derived class. For this purpose,
// we reuse the RefLogger and DerivedRefLogger classes.

TEST(WTF_Poisoned, Basic)
{
    initializeTestPoison();
    DerivedRefLogger a("a");

    Poisoned<g_testPoisonA, RefLogger*> empty;
    ASSERT_EQ(nullptr, empty.unpoisoned());

    {
        Poisoned<g_testPoisonA, RefLogger*> ptr(&a);
        ASSERT_EQ(&a, ptr.unpoisoned());
        ASSERT_EQ(&a, &*ptr);
        ASSERT_EQ(&a.name, &ptr->name);

#if ENABLE(POISON)
        uintptr_t ptrBits;
        std::memcpy(&ptrBits, &ptr, sizeof(ptrBits));
        ASSERT_TRUE(ptrBits != bitwise_cast<uintptr_t>(&a));
#if ENABLE(POISON_ASSERTS)
        ASSERT_TRUE((Poisoned<g_testPoisonA, RefLogger*>::isPoisoned(ptrBits)));
#endif
#endif // ENABLE(POISON)
    }

    {
        Poisoned<g_testPoisonA, RefLogger*> ptr = &a;
        ASSERT_EQ(&a, ptr.unpoisoned());
    }

    {
        Poisoned<g_testPoisonA, RefLogger*> p1 = &a;
        Poisoned<g_testPoisonA, RefLogger*> p2(p1);

        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&a, p2.unpoisoned());
        ASSERT_TRUE(p1.bits() == p2.bits());

#if ENABLE(MIXED_POISON)
        Poisoned<g_testPoisonB, RefLogger*> p3(p1);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_TRUE(p1.bits() != p3.bits());
#endif
    }

    {
        Poisoned<g_testPoisonA, RefLogger*> p1 = &a;
        Poisoned<g_testPoisonA, RefLogger*> p2 = p1;
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&a, p2.unpoisoned());
        ASSERT_TRUE(p1.bits() == p2.bits());

#if ENABLE(MIXED_POISON)
        Poisoned<g_testPoisonB, RefLogger*> p3 = p1;
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_TRUE(p1.bits() != p3.bits());
#endif
    }

    {
        Poisoned<g_testPoisonA, RefLogger*> p1 = &a;
        Poisoned<g_testPoisonA, RefLogger*> p2 = WTFMove(p1);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&a, p2.unpoisoned());
        ASSERT_TRUE(p1.bits() == p2.bits());

#if ENABLE(MIXED_POISON)
        Poisoned<g_testPoisonA, RefLogger*> p3 = &a;
        Poisoned<g_testPoisonB, RefLogger*> p4 = WTFMove(p3);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&a, p4.unpoisoned());
        ASSERT_TRUE(p3.bits() != p4.bits());
#endif
    }

    {
        Poisoned<g_testPoisonA, RefLogger*> p1 = &a;
        Poisoned<g_testPoisonA, RefLogger*> p2(WTFMove(p1));
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&a, p2.unpoisoned());
        ASSERT_TRUE(p1.bits() == p2.bits());

#if ENABLE(MIXED_POISON)
        Poisoned<g_testPoisonA, RefLogger*> p3 = &a;
        Poisoned<g_testPoisonB, RefLogger*> p4(WTFMove(p3));
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&a, p4.unpoisoned());
        ASSERT_TRUE(p3.bits() != p4.bits());
#endif
    }

#if ENABLE(MIXED_POISON)
    {
        Poisoned<g_testPoisonA, DerivedRefLogger*> p1 = &a;
        Poisoned<g_testPoisonA, RefLogger*> p2 = p1;
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&a, p2.unpoisoned());
        ASSERT_TRUE(p1.bits() == p2.bits());

        Poisoned<g_testPoisonB, RefLogger*> p3 = p1;
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_TRUE(p1.bits() != p3.bits());
    }
#endif

#if ENABLE(MIXED_POISON)
    {
        Poisoned<g_testPoisonA, DerivedRefLogger*> p1 = &a;
        Poisoned<g_testPoisonA, RefLogger*> p2 = WTFMove(p1);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&a, p2.unpoisoned());
        ASSERT_TRUE(p1.bits() == p2.bits());

        Poisoned<g_testPoisonA, DerivedRefLogger*> p3 = &a;
        Poisoned<g_testPoisonB, RefLogger*> p4 = WTFMove(p3);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&a, p4.unpoisoned());
        ASSERT_TRUE(p3.bits() != p4.bits());
    }
#endif

    {
        Poisoned<g_testPoisonA, RefLogger*> ptr(&a);
        ASSERT_EQ(&a, ptr.unpoisoned());
        ptr.clear();
        ASSERT_EQ(nullptr, ptr.unpoisoned());
    }
}

TEST(WTF_Poisoned, Assignment)
{
    initializeTestPoison();
    DerivedRefLogger a("a");
    RefLogger b("b");
    DerivedRefLogger c("c");

    {
        Poisoned<g_testPoisonA, RefLogger*> p1(&a);
        Poisoned<g_testPoisonA, RefLogger*> p2(&b);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&b, p2.unpoisoned());
        p1 = p2;
        ASSERT_EQ(&b, p1.unpoisoned());
        ASSERT_EQ(&b, p2.unpoisoned());
        ASSERT_TRUE(p1.bits() == p2.bits());

#if ENABLE(MIXED_POISON)
        Poisoned<g_testPoisonA, RefLogger*> p3(&a);
        Poisoned<g_testPoisonB, RefLogger*> p4(&b);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&b, p4.unpoisoned());
        p3 = p4;
        ASSERT_EQ(&b, p3.unpoisoned());
        ASSERT_EQ(&b, p4.unpoisoned());
        ASSERT_TRUE(p3.bits() != p4.bits());
#endif
    }

    {
        Poisoned<g_testPoisonA, RefLogger*> ptr(&a);
        ASSERT_EQ(&a, ptr.unpoisoned());
        ptr = &b;
        ASSERT_EQ(&b, ptr.unpoisoned());
    }

    {
        Poisoned<g_testPoisonA, RefLogger*> ptr(&a);
        ASSERT_EQ(&a, ptr.unpoisoned());
        ptr = nullptr;
        ASSERT_EQ(nullptr, ptr.unpoisoned());
    }

    {
        Poisoned<g_testPoisonA, RefLogger*> p1(&a);
        Poisoned<g_testPoisonA, RefLogger*> p2(&b);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&b, p2.unpoisoned());
        p1 = WTFMove(p2);
        ASSERT_EQ(&b, p1.unpoisoned());
        ASSERT_EQ(&b, p2.unpoisoned());
        ASSERT_TRUE(p1.bits() == p2.bits());

#if ENABLE(MIXED_POISON)
        Poisoned<g_testPoisonA, RefLogger*> p3(&a);
        Poisoned<g_testPoisonB, RefLogger*> p4(&b);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&b, p4.unpoisoned());
        p3 = WTFMove(p4);
        ASSERT_EQ(&b, p3.unpoisoned());
        ASSERT_EQ(&b, p4.unpoisoned());
        ASSERT_TRUE(p3.bits() != p4.bits());
#endif
    }

#if ENABLE(MIXED_POISON)
    {
        Poisoned<g_testPoisonA, RefLogger*> p1(&a);
        Poisoned<g_testPoisonA, DerivedRefLogger*> p2(&c);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&c, p2.unpoisoned());
        p1 = p2;
        ASSERT_EQ(&c, p1.unpoisoned());
        ASSERT_EQ(&c, p2.unpoisoned());
        ASSERT_TRUE(p1.bits() == p2.bits());

        Poisoned<g_testPoisonA, RefLogger*> p3(&a);
        Poisoned<g_testPoisonB, DerivedRefLogger*> p4(&c);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&c, p4.unpoisoned());
        p3 = p4;
        ASSERT_EQ(&c, p3.unpoisoned());
        ASSERT_EQ(&c, p4.unpoisoned());
        ASSERT_TRUE(p3.bits() != p4.bits());
    }
#endif

    {
        Poisoned<g_testPoisonA, RefLogger*> ptr(&a);
        ASSERT_EQ(&a, ptr.unpoisoned());
        ptr = &c;
        ASSERT_EQ(&c, ptr.unpoisoned());
    }

#if ENABLE(MIXED_POISON)
    {
        Poisoned<g_testPoisonA, RefLogger*> p1(&a);
        Poisoned<g_testPoisonA, DerivedRefLogger*> p2(&c);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&c, p2.unpoisoned());
        p1 = WTFMove(p2);
        ASSERT_EQ(&c, p1.unpoisoned());
        ASSERT_EQ(&c, p2.unpoisoned());
        ASSERT_TRUE(p1.bits() == p2.bits());

        Poisoned<g_testPoisonA, RefLogger*> p3(&a);
        Poisoned<g_testPoisonB, DerivedRefLogger*> p4(&c);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&c, p4.unpoisoned());
        p3 = WTFMove(p4);
        ASSERT_EQ(&c, p3.unpoisoned());
        ASSERT_EQ(&c, p4.unpoisoned());
        ASSERT_TRUE(p3.bits() != p4.bits());
    }
#endif

    {
        Poisoned<g_testPoisonA, RefLogger*> ptr(&a);
        ASSERT_EQ(&a, ptr.unpoisoned());
        ptr = ptr;
        ASSERT_EQ(&a, ptr.unpoisoned());
    }

    {
        Poisoned<g_testPoisonA, RefLogger*> ptr(&a);
        ASSERT_EQ(&a, ptr.unpoisoned());
#if COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wself-move"
#endif
        ptr = WTFMove(ptr);
#if COMPILER(CLANG)
#pragma clang diagnostic pop
#endif
        ASSERT_EQ(&a, ptr.unpoisoned());
    }
}

TEST(WTF_Poisoned, Swap)
{
    initializeTestPoison();
    RefLogger a("a");
    RefLogger b("b");

    {
        Poisoned<g_testPoisonA, RefLogger*> p1(&a);
        Poisoned<g_testPoisonA, RefLogger*> p2(&b);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&b, p2.unpoisoned());
        p1.swap(p2);
        ASSERT_EQ(&b, p1.unpoisoned());
        ASSERT_EQ(&a, p2.unpoisoned());

        ASSERT_TRUE(p1.bits() != p2.bits());

#if ENABLE(MIXED_POISON)
        Poisoned<g_testPoisonA, RefLogger*> p3(&a);
        Poisoned<g_testPoisonB, RefLogger*> p4(&b);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&b, p4.unpoisoned());
        p3.swap(p4);
        ASSERT_EQ(&b, p3.unpoisoned());
        ASSERT_EQ(&a, p4.unpoisoned());

        ASSERT_TRUE(p3.bits() != p4.bits());
        ASSERT_TRUE(p1.bits() == p3.bits());
        ASSERT_TRUE(p2.bits() != p4.bits());
#endif
    }

    {
        Poisoned<g_testPoisonA, RefLogger*> p1(&a);
        Poisoned<g_testPoisonA, RefLogger*> p2(&b);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&b, p2.unpoisoned());
#if ENABLE(MIXED_POISON)
        swap(p1, p2);
#else
        std::swap(p1, p2);
#endif
        ASSERT_EQ(&b, p1.unpoisoned());
        ASSERT_EQ(&a, p2.unpoisoned());

        ASSERT_TRUE(p1.bits() != p2.bits());

#if ENABLE(MIXED_POISON)
        Poisoned<g_testPoisonA, RefLogger*> p3(&a);
        Poisoned<g_testPoisonB, RefLogger*> p4(&b);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&b, p4.unpoisoned());
        swap(p3, p4);
        ASSERT_EQ(&b, p3.unpoisoned());
        ASSERT_EQ(&a, p4.unpoisoned());

        ASSERT_TRUE(p3.bits() != p4.bits());
        ASSERT_TRUE(p1.bits() == p3.bits());
        ASSERT_TRUE(p2.bits() != p4.bits());
#endif
    }
}

static Poisoned<g_testPoisonA, RefLogger*> poisonedPtrFoo(RefLogger& logger)
{
    return Poisoned<g_testPoisonA, RefLogger*>(&logger);
}

TEST(WTF_Poisoned, ReturnValue)
{
    initializeTestPoison();
    DerivedRefLogger a("a");

    {
        auto ptr = poisonedPtrFoo(a);
        ASSERT_EQ(&a, ptr.unpoisoned());
        ASSERT_EQ(&a, &*ptr);
        ASSERT_EQ(&a.name, &ptr->name);
    }
}

} // namespace TestWebKitAPI


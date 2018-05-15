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

#include "RefLogger.h"
#include <mutex>
#include <wtf/Poisoned.h>

namespace TestWebKitAPI {

namespace {

uintptr_t g_testPoisonA;
uintptr_t g_testPoisonB;

using TestPoisonA = Poison<g_testPoisonA>;
using TestPoisonB = Poison<g_testPoisonB>;

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

} // namespace anonymous

// For these tests, we need a base class and a derived class. For this purpose,
// we reuse the RefLogger and DerivedRefLogger classes.

TEST(WTF_Poisoned, DISABLED_Basic)
{
    initializeTestPoison();
    DerivedRefLogger a("a");

    {
        Poisoned<TestPoisonA, RefLogger*> empty;
        ASSERT_EQ(nullptr, empty.unpoisoned());
    }

    {
        Poisoned<TestPoisonA, RefLogger*> empty(nullptr);
        ASSERT_EQ(nullptr, empty.unpoisoned());
    }

    {
        Poisoned<TestPoisonA, RefLogger*> ptr(&a);
        ASSERT_EQ(&a, ptr.unpoisoned());
        ASSERT_EQ(&a, &*ptr);
        ASSERT_EQ(&a.name, &ptr->name);

#if ENABLE(POISON)
        uintptr_t ptrBits;
        std::memcpy(&ptrBits, &ptr, sizeof(ptrBits));
        ASSERT_TRUE(ptrBits != bitwise_cast<uintptr_t>(&a));
#if ENABLE(POISON_ASSERTS)
        ASSERT_TRUE((Poisoned<TestPoisonA, RefLogger*>::isPoisoned(ptrBits)));
#endif
#endif // ENABLE(POISON)
    }

    {
        Poisoned<TestPoisonA, RefLogger*> ptr = &a;
        ASSERT_EQ(&a, ptr.unpoisoned());
    }

    {
        Poisoned<TestPoisonA, RefLogger*> p1 = &a;
        Poisoned<TestPoisonA, RefLogger*> p2(p1);

        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&a, p2.unpoisoned());
        ASSERT_TRUE(p1 == p2);
        ASSERT_TRUE(p1.bits() == p2.bits());

        Poisoned<TestPoisonB, RefLogger*> p3(p1);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_TRUE(p1 == p3);
        ASSERT_TRUE(p1.bits() != p3.bits());
    }

    {
        Poisoned<TestPoisonA, RefLogger*> p1 = &a;
        Poisoned<TestPoisonA, RefLogger*> p2 = p1;
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&a, p2.unpoisoned());
        ASSERT_TRUE(p1 == p2);
        ASSERT_TRUE(p1.bits() == p2.bits());

        Poisoned<TestPoisonB, RefLogger*> p3 = p1;
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_TRUE(p1 == p3);
        ASSERT_TRUE(p1.bits() != p3.bits());
    }

    {
        Poisoned<TestPoisonA, RefLogger*> p1 = &a;
        Poisoned<TestPoisonA, RefLogger*> p2 = WTFMove(p1);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&a, p2.unpoisoned());
        ASSERT_TRUE(p1 == p2);
        ASSERT_TRUE(p1.bits() == p2.bits());

        Poisoned<TestPoisonA, RefLogger*> p3 = &a;
        Poisoned<TestPoisonB, RefLogger*> p4 = WTFMove(p3);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&a, p4.unpoisoned());
        ASSERT_TRUE(p3 == p4);
        ASSERT_TRUE(p3.bits() != p4.bits());
    }

    {
        Poisoned<TestPoisonA, RefLogger*> p1 = &a;
        Poisoned<TestPoisonA, RefLogger*> p2(WTFMove(p1));
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&a, p2.unpoisoned());
        ASSERT_TRUE(p1 == p2);
        ASSERT_TRUE(p1.bits() == p2.bits());

        Poisoned<TestPoisonA, RefLogger*> p3 = &a;
        Poisoned<TestPoisonB, RefLogger*> p4(WTFMove(p3));
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&a, p4.unpoisoned());
        ASSERT_TRUE(p3 == p4);
        ASSERT_TRUE(p3.bits() != p4.bits());
    }

    {
        Poisoned<TestPoisonA, DerivedRefLogger*> p1 = &a;
        Poisoned<TestPoisonA, RefLogger*> p2 = p1;
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&a, p2.unpoisoned());
        ASSERT_TRUE(p1 == p2);
        ASSERT_TRUE(p2 == p1);
        ASSERT_TRUE(p1.bits() == p2.bits());

        Poisoned<TestPoisonB, RefLogger*> p3 = p1;
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_TRUE(p1 == p3);
        ASSERT_TRUE(p3 == p1);
        ASSERT_TRUE(p1.bits() != p3.bits());
    }

    {
        Poisoned<TestPoisonA, DerivedRefLogger*> p1 = &a;
        Poisoned<TestPoisonA, RefLogger*> p2 = WTFMove(p1);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&a, p2.unpoisoned());
        ASSERT_TRUE(p1 == p2);
        ASSERT_TRUE(p2 == p1);
        ASSERT_TRUE(p1.bits() == p2.bits());

        Poisoned<TestPoisonA, DerivedRefLogger*> p3 = &a;
        Poisoned<TestPoisonB, RefLogger*> p4 = WTFMove(p3);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&a, p4.unpoisoned());
        ASSERT_TRUE(p3 == p4);
        ASSERT_TRUE(p4 == p3);
        ASSERT_TRUE(p3.bits() != p4.bits());
    }

    {
        Poisoned<TestPoisonA, RefLogger*> ptr(&a);
        ASSERT_EQ(&a, ptr.unpoisoned());
        ptr.clear();
        ASSERT_EQ(nullptr, ptr.unpoisoned());
    }

    {
        Poisoned<TestPoisonA, RefLogger*> pA1000a = reinterpret_cast<RefLogger*>(0x1000);
        Poisoned<TestPoisonB, RefLogger*> pB2000a = reinterpret_cast<RefLogger*>(0x2000);

        Poisoned<TestPoisonA, RefLogger*> pA1000b = reinterpret_cast<RefLogger*>(0x1000);
        Poisoned<TestPoisonA, RefLogger*> pA2000b = reinterpret_cast<RefLogger*>(0x2000);

        Poisoned<TestPoisonB, RefLogger*> pB1000c = reinterpret_cast<RefLogger*>(0x1000);
        Poisoned<TestPoisonB, RefLogger*> pB2000c = reinterpret_cast<RefLogger*>(0x2000);


        ASSERT_EQ(pA1000a == pA1000a, true);
        ASSERT_EQ(pA1000a == pA1000b, true);
        ASSERT_EQ(pA1000a == pA2000b, false);
        ASSERT_EQ(pA1000a == pB1000c, true);
        ASSERT_EQ(pA1000a == pB2000c, false);

        ASSERT_EQ(pA1000a != pA1000a, false);
        ASSERT_EQ(pA1000a != pA1000b, false);
        ASSERT_EQ(pA1000a != pA2000b, true);
        ASSERT_EQ(pA1000a != pB1000c, false);
        ASSERT_EQ(pA1000a != pB2000c, true);

        ASSERT_EQ(pA1000a < pA1000a, false);
        ASSERT_EQ(pA1000a < pA1000b, false);
        ASSERT_EQ(pA1000a < pA2000b, true);
        ASSERT_EQ(pA1000a < pB1000c, false);
        ASSERT_EQ(pA1000a < pB2000c, true);

        ASSERT_EQ(pB2000a < pB2000a, false);
        ASSERT_EQ(pB2000a < pA1000b, false);
        ASSERT_EQ(pB2000a < pA2000b, false);
        ASSERT_EQ(pB2000a < pB1000c, false);
        ASSERT_EQ(pB2000a < pB2000c, false);

        ASSERT_EQ(pA1000a <= pA1000a, true);
        ASSERT_EQ(pA1000a <= pA1000b, true);
        ASSERT_EQ(pA1000a <= pA2000b, true);
        ASSERT_EQ(pA1000a <= pB1000c, true);
        ASSERT_EQ(pA1000a <= pB2000c, true);

        ASSERT_EQ(pB2000a <= pB2000a, true);
        ASSERT_EQ(pB2000a <= pA1000b, false);
        ASSERT_EQ(pB2000a <= pA2000b, true);
        ASSERT_EQ(pB2000a <= pB1000c, false);
        ASSERT_EQ(pB2000a <= pB2000c, true);

        ASSERT_EQ(pA1000a > pA1000a, false);
        ASSERT_EQ(pA1000a > pA1000b, false);
        ASSERT_EQ(pA1000a > pA2000b, false);
        ASSERT_EQ(pA1000a > pB1000c, false);
        ASSERT_EQ(pA1000a > pB2000c, false);

        ASSERT_EQ(pB2000a > pB2000a, false);
        ASSERT_EQ(pB2000a > pA1000b, true);
        ASSERT_EQ(pB2000a > pA2000b, false);
        ASSERT_EQ(pB2000a > pB1000c, true);
        ASSERT_EQ(pB2000a > pB2000c, false);

        ASSERT_EQ(pA1000a >= pA1000a, true);
        ASSERT_EQ(pA1000a >= pA1000b, true);
        ASSERT_EQ(pA1000a >= pA2000b, false);
        ASSERT_EQ(pA1000a >= pB1000c, true);
        ASSERT_EQ(pA1000a >= pB2000c, false);

        ASSERT_EQ(pB2000a >= pB2000a, true);
        ASSERT_EQ(pB2000a >= pA1000b, true);
        ASSERT_EQ(pB2000a >= pA2000b, true);
        ASSERT_EQ(pB2000a >= pB1000c, true);
        ASSERT_EQ(pB2000a >= pB2000c, true);
    }

    {
        Poisoned<TestPoisonA, RefLogger*> prA1000 = reinterpret_cast<DerivedRefLogger*>(0x1000);
        Poisoned<TestPoisonA, DerivedRefLogger*> pdA1000 = reinterpret_cast<DerivedRefLogger*>(0x1000);
        Poisoned<TestPoisonB, RefLogger*> prB1000 = reinterpret_cast<DerivedRefLogger*>(0x1000);
        Poisoned<TestPoisonB, DerivedRefLogger*> pdB1000 = reinterpret_cast<DerivedRefLogger*>(0x1000);

        Poisoned<TestPoisonA, RefLogger*> prA2000 = reinterpret_cast<DerivedRefLogger*>(0x2000);
        Poisoned<TestPoisonA, DerivedRefLogger*> pdA2000 = reinterpret_cast<DerivedRefLogger*>(0x2000);
        Poisoned<TestPoisonB, RefLogger*> prB2000 = reinterpret_cast<DerivedRefLogger*>(0x2000);
        Poisoned<TestPoisonB, DerivedRefLogger*> pdB2000 = reinterpret_cast<DerivedRefLogger*>(0x2000);

        ASSERT_EQ(prA1000 == pdA1000, true);
        ASSERT_EQ(prA1000 == pdB1000, true);
        ASSERT_EQ(prA1000 == pdA2000, false);
        ASSERT_EQ(prA1000 == pdB2000, false);
        ASSERT_EQ(pdA1000 == prA1000, true);
        ASSERT_EQ(pdA1000 == prB1000, true);
        ASSERT_EQ(pdA1000 == prA2000, false);
        ASSERT_EQ(pdA1000 == prB2000, false);

        ASSERT_EQ(prA1000 != pdA1000, false);
        ASSERT_EQ(prA1000 != pdB1000, false);
        ASSERT_EQ(prA1000 != pdA2000, true);
        ASSERT_EQ(prA1000 != pdB2000, true);
        ASSERT_EQ(pdA1000 != prA1000, false);
        ASSERT_EQ(pdA1000 != prB1000, false);
        ASSERT_EQ(pdA1000 != prA2000, true);
        ASSERT_EQ(pdA1000 != prB2000, true);

        ASSERT_EQ(prA1000 < pdA1000, false);
        ASSERT_EQ(prA1000 < pdB1000, false);
        ASSERT_EQ(prA1000 < pdA2000, true);
        ASSERT_EQ(prA1000 < pdB2000, true);
        ASSERT_EQ(pdA1000 < prA1000, false);
        ASSERT_EQ(pdA1000 < prB1000, false);
        ASSERT_EQ(pdA1000 < prA2000, true);
        ASSERT_EQ(pdA1000 < prB2000, true);

        ASSERT_EQ(prA2000 < pdA1000, false);
        ASSERT_EQ(prA2000 < pdB1000, false);
        ASSERT_EQ(prA2000 < pdA2000, false);
        ASSERT_EQ(prA2000 < pdB2000, false);
        ASSERT_EQ(pdA2000 < prA1000, false);
        ASSERT_EQ(pdA2000 < prB1000, false);
        ASSERT_EQ(pdA2000 < prA2000, false);
        ASSERT_EQ(pdA2000 < prB2000, false);

        ASSERT_EQ(prA1000 <= pdA1000, true);
        ASSERT_EQ(prA1000 <= pdB1000, true);
        ASSERT_EQ(prA1000 <= pdA2000, true);
        ASSERT_EQ(prA1000 <= pdB2000, true);
        ASSERT_EQ(pdA1000 <= prA1000, true);
        ASSERT_EQ(pdA1000 <= prB1000, true);
        ASSERT_EQ(pdA1000 <= prA2000, true);
        ASSERT_EQ(pdA1000 <= prB2000, true);

        ASSERT_EQ(prA2000 <= pdA1000, false);
        ASSERT_EQ(prA2000 <= pdB1000, false);
        ASSERT_EQ(prA2000 <= pdA2000, true);
        ASSERT_EQ(prA2000 <= pdB2000, true);
        ASSERT_EQ(pdA2000 <= prA1000, false);
        ASSERT_EQ(pdA2000 <= prB1000, false);
        ASSERT_EQ(pdA2000 <= prA2000, true);
        ASSERT_EQ(pdA2000 <= prB2000, true);

        ASSERT_EQ(prA1000 > pdA1000, false);
        ASSERT_EQ(prA1000 > pdB1000, false);
        ASSERT_EQ(prA1000 > pdA2000, false);
        ASSERT_EQ(prA1000 > pdB2000, false);
        ASSERT_EQ(pdA1000 > prA1000, false);
        ASSERT_EQ(pdA1000 > prB1000, false);
        ASSERT_EQ(pdA1000 > prA2000, false);
        ASSERT_EQ(pdA1000 > prB2000, false);

        ASSERT_EQ(prA2000 > pdA1000, true);
        ASSERT_EQ(prA2000 > pdB1000, true);
        ASSERT_EQ(prA2000 > pdA2000, false);
        ASSERT_EQ(prA2000 > pdB2000, false);
        ASSERT_EQ(pdA2000 > prA1000, true);
        ASSERT_EQ(pdA2000 > prB1000, true);
        ASSERT_EQ(pdA2000 > prA2000, false);
        ASSERT_EQ(pdA2000 > prB2000, false);

        ASSERT_EQ(prA1000 >= pdA1000, true);
        ASSERT_EQ(prA1000 >= pdB1000, true);
        ASSERT_EQ(prA1000 >= pdA2000, false);
        ASSERT_EQ(prA1000 >= pdB2000, false);
        ASSERT_EQ(pdA1000 >= prA1000, true);
        ASSERT_EQ(pdA1000 >= prB1000, true);
        ASSERT_EQ(pdA1000 >= prA2000, false);
        ASSERT_EQ(pdA1000 >= prB2000, false);

        ASSERT_EQ(prA2000 >= pdA1000, true);
        ASSERT_EQ(prA2000 >= pdB1000, true);
        ASSERT_EQ(prA2000 >= pdA2000, true);
        ASSERT_EQ(prA2000 >= pdB2000, true);
        ASSERT_EQ(pdA2000 >= prA1000, true);
        ASSERT_EQ(pdA2000 >= prB1000, true);
        ASSERT_EQ(pdA2000 >= prA2000, true);
        ASSERT_EQ(pdA2000 >= prB2000, true);
    }
}

TEST(WTF_Poisoned, DISABLED_Assignment)
{
    initializeTestPoison();
    DerivedRefLogger a("a");
    RefLogger b("b");
    DerivedRefLogger c("c");

    {
        Poisoned<TestPoisonA, RefLogger*> p1(&a);
        Poisoned<TestPoisonA, RefLogger*> p2(&b);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&b, p2.unpoisoned());
        p1 = p2;
        ASSERT_EQ(&b, p1.unpoisoned());
        ASSERT_EQ(&b, p2.unpoisoned());
        ASSERT_TRUE(p1.bits() == p2.bits());

        Poisoned<TestPoisonA, RefLogger*> p3(&a);
        Poisoned<TestPoisonB, RefLogger*> p4(&b);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&b, p4.unpoisoned());
        p3 = p4;
        ASSERT_EQ(&b, p3.unpoisoned());
        ASSERT_EQ(&b, p4.unpoisoned());
        ASSERT_TRUE(p3.bits() != p4.bits());
    }

    {
        Poisoned<TestPoisonA, RefLogger*> ptr(&a);
        ASSERT_EQ(&a, ptr.unpoisoned());
        ptr = &b;
        ASSERT_EQ(&b, ptr.unpoisoned());
    }

    {
        Poisoned<TestPoisonA, RefLogger*> ptr(&a);
        ASSERT_EQ(&a, ptr.unpoisoned());
        ptr = nullptr;
        ASSERT_EQ(nullptr, ptr.unpoisoned());
    }

    {
        Poisoned<TestPoisonA, RefLogger*> p1(&a);
        Poisoned<TestPoisonA, RefLogger*> p2(&b);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&b, p2.unpoisoned());
        p1 = WTFMove(p2);
        ASSERT_EQ(&b, p1.unpoisoned());
        ASSERT_EQ(&b, p2.unpoisoned());
        ASSERT_TRUE(p1.bits() == p2.bits());

        Poisoned<TestPoisonA, RefLogger*> p3(&a);
        Poisoned<TestPoisonB, RefLogger*> p4(&b);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&b, p4.unpoisoned());
        p3 = WTFMove(p4);
        ASSERT_EQ(&b, p3.unpoisoned());
        ASSERT_EQ(&b, p4.unpoisoned());
        ASSERT_TRUE(p3.bits() != p4.bits());
    }

    {
        Poisoned<TestPoisonA, RefLogger*> p1(&a);
        Poisoned<TestPoisonA, DerivedRefLogger*> p2(&c);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&c, p2.unpoisoned());
        p1 = p2;
        ASSERT_EQ(&c, p1.unpoisoned());
        ASSERT_EQ(&c, p2.unpoisoned());
        ASSERT_TRUE(p1.bits() == p2.bits());

        Poisoned<TestPoisonA, RefLogger*> p3(&a);
        Poisoned<TestPoisonB, DerivedRefLogger*> p4(&c);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&c, p4.unpoisoned());
        p3 = p4;
        ASSERT_EQ(&c, p3.unpoisoned());
        ASSERT_EQ(&c, p4.unpoisoned());
        ASSERT_TRUE(p3.bits() != p4.bits());
    }

    {
        Poisoned<TestPoisonA, RefLogger*> ptr(&a);
        ASSERT_EQ(&a, ptr.unpoisoned());
        ptr = &c;
        ASSERT_EQ(&c, ptr.unpoisoned());
    }

    {
        Poisoned<TestPoisonA, RefLogger*> p1(&a);
        Poisoned<TestPoisonA, DerivedRefLogger*> p2(&c);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&c, p2.unpoisoned());
        p1 = WTFMove(p2);
        ASSERT_EQ(&c, p1.unpoisoned());
        ASSERT_EQ(&c, p2.unpoisoned());
        ASSERT_TRUE(p1.bits() == p2.bits());

        Poisoned<TestPoisonA, RefLogger*> p3(&a);
        Poisoned<TestPoisonB, DerivedRefLogger*> p4(&c);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&c, p4.unpoisoned());
        p3 = WTFMove(p4);
        ASSERT_EQ(&c, p3.unpoisoned());
        ASSERT_EQ(&c, p4.unpoisoned());
        ASSERT_TRUE(p3.bits() != p4.bits());
    }

    {
        Poisoned<TestPoisonA, RefLogger*> ptr(&a);
        ASSERT_EQ(&a, ptr.unpoisoned());
#if COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
#endif
        ptr = ptr;
#if COMPILER(CLANG)
#pragma clang diagnostic pop
#endif
        ASSERT_EQ(&a, ptr.unpoisoned());
    }

    {
        Poisoned<TestPoisonA, RefLogger*> ptr(&a);
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

TEST(WTF_Poisoned, DISABLED_Swap)
{
    initializeTestPoison();
    RefLogger a("a");
    RefLogger b("b");

    {
        Poisoned<TestPoisonA, RefLogger*> p1(&a);
        Poisoned<TestPoisonA, RefLogger*> p2(&b);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&b, p2.unpoisoned());
        p1.swap(p2);
        ASSERT_EQ(&b, p1.unpoisoned());
        ASSERT_EQ(&a, p2.unpoisoned());

        ASSERT_TRUE(p1.bits() != p2.bits());

        Poisoned<TestPoisonA, RefLogger*> p3(&a);
        Poisoned<TestPoisonB, RefLogger*> p4(&b);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&b, p4.unpoisoned());
        p3.swap(p4);
        ASSERT_EQ(&b, p3.unpoisoned());
        ASSERT_EQ(&a, p4.unpoisoned());

        ASSERT_TRUE(p3.bits() != p4.bits());
        ASSERT_TRUE(p1.bits() == p3.bits());
        ASSERT_TRUE(p2.bits() != p4.bits());
    }

    {
        Poisoned<TestPoisonA, RefLogger*> p1(&a);
        Poisoned<TestPoisonA, RefLogger*> p2(&b);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&b, p2.unpoisoned());
        swap(p1, p2);
        ASSERT_EQ(&b, p1.unpoisoned());
        ASSERT_EQ(&a, p2.unpoisoned());

        ASSERT_TRUE(p1.bits() != p2.bits());

        Poisoned<TestPoisonA, RefLogger*> p3(&a);
        Poisoned<TestPoisonB, RefLogger*> p4(&b);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&b, p4.unpoisoned());
        swap(p3, p4);
        ASSERT_EQ(&b, p3.unpoisoned());
        ASSERT_EQ(&a, p4.unpoisoned());

        ASSERT_TRUE(p3.bits() != p4.bits());
        ASSERT_TRUE(p1.bits() == p3.bits());
        ASSERT_TRUE(p2.bits() != p4.bits());
    }

    {
        Poisoned<TestPoisonA, RefLogger*> p1(&a);
        RefLogger* p2(&b);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&b, p2);
        swap(p1, p2);
        ASSERT_EQ(&b, p1.unpoisoned());
        ASSERT_EQ(&a, p2);

        ASSERT_TRUE(p1.bits() != bitwise_cast<uintptr_t>(p2));
    }

    {
        Poisoned<TestPoisonA, RefLogger*> p1(&a);
        RefLogger* p2(&b);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&b, p2);
        p1.swap(p2);
        ASSERT_EQ(&b, p1.unpoisoned());
        ASSERT_EQ(&a, p2);

        ASSERT_TRUE(p1.bits() != bitwise_cast<uintptr_t>(p2));
    }
}

static Poisoned<TestPoisonA, RefLogger*> poisonedPtrFoo(RefLogger& logger)
{
    return Poisoned<TestPoisonA, RefLogger*>(&logger);
}

TEST(WTF_Poisoned, DISABLED_ReturnValue)
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


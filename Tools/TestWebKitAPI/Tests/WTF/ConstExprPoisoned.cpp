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
#include <wtf/Poisoned.h>

namespace TestWebKitAPI {

static const uint32_t PoisonA = 0xaaaa;
static const uint32_t PoisonB = 0xbbbb;

// For these tests, we need a base class and a derived class. For this purpose,
// we reuse the RefLogger and DerivedRefLogger classes.

TEST(WTF_ConstExprPoisoned, Basic)
{
    DerivedRefLogger a("a");

    ConstExprPoisoned<PoisonA, RefLogger*> empty;
    ASSERT_EQ(nullptr, empty.unpoisoned());

    {
        ConstExprPoisoned<PoisonA, RefLogger*> ptr(&a);
        ASSERT_EQ(&a, ptr.unpoisoned());
        ASSERT_EQ(&a, &*ptr);
        ASSERT_EQ(&a.name, &ptr->name);

#if ENABLE(POISON)
        uintptr_t ptrBits;
        std::memcpy(&ptrBits, &ptr, sizeof(ptrBits));
        ASSERT_TRUE(ptrBits != bitwise_cast<uintptr_t>(&a));
#if ENABLE(POISON_ASSERTS)
        ASSERT_TRUE((ConstExprPoisoned<PoisonA, RefLogger*>::isPoisoned(ptrBits)));
#endif
#endif // ENABLE(POISON)
    }

    {
        ConstExprPoisoned<PoisonA, RefLogger*> ptr = &a;
        ASSERT_EQ(&a, ptr.unpoisoned());
    }

    {
        ConstExprPoisoned<PoisonA, RefLogger*> p1 = &a;
        ConstExprPoisoned<PoisonA, RefLogger*> p2(p1);
        ConstExprPoisoned<PoisonB, RefLogger*> p3(p1);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&a, p2.unpoisoned());
        ASSERT_EQ(&a, p3.unpoisoned());

        ASSERT_TRUE(p1.bits() == p2.bits());
        ASSERT_TRUE(p1.bits() != p3.bits());
    }

    {
        ConstExprPoisoned<PoisonA, RefLogger*> p1 = &a;
        ConstExprPoisoned<PoisonA, RefLogger*> p2 = p1;
        ConstExprPoisoned<PoisonB, RefLogger*> p3 = p1;
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&a, p2.unpoisoned());
        ASSERT_EQ(&a, p3.unpoisoned());

        ASSERT_TRUE(p1.bits() == p2.bits());
        ASSERT_TRUE(p1.bits() != p3.bits());
    }

    {
        ConstExprPoisoned<PoisonA, RefLogger*> p1 = &a;
        ConstExprPoisoned<PoisonA, RefLogger*> p2 = WTFMove(p1);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&a, p2.unpoisoned());

        ConstExprPoisoned<PoisonA, RefLogger*> p3 = &a;
        ConstExprPoisoned<PoisonB, RefLogger*> p4 = WTFMove(p3);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&a, p4.unpoisoned());

        ASSERT_TRUE(p1.bits() == p2.bits());
        ASSERT_TRUE(p3.bits() != p4.bits());
    }

    {
        ConstExprPoisoned<PoisonA, RefLogger*> p1 = &a;
        ConstExprPoisoned<PoisonA, RefLogger*> p2(WTFMove(p1));
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&a, p2.unpoisoned());

        ConstExprPoisoned<PoisonA, RefLogger*> p3 = &a;
        ConstExprPoisoned<PoisonB, RefLogger*> p4(WTFMove(p3));
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&a, p4.unpoisoned());

        ASSERT_TRUE(p1.bits() == p2.bits());
        ASSERT_TRUE(p3.bits() != p4.bits());
    }

    {
        ConstExprPoisoned<PoisonA, DerivedRefLogger*> p1 = &a;
        ConstExprPoisoned<PoisonA, RefLogger*> p2 = p1;
        ConstExprPoisoned<PoisonB, RefLogger*> p3 = p1;
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&a, p2.unpoisoned());
        ASSERT_EQ(&a, p3.unpoisoned());

        ASSERT_TRUE(p1.bits() == p2.bits());
        ASSERT_TRUE(p1.bits() != p3.bits());
    }

    {
        ConstExprPoisoned<PoisonA, DerivedRefLogger*> p1 = &a;
        ConstExprPoisoned<PoisonA, RefLogger*> p2 = WTFMove(p1);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&a, p2.unpoisoned());

        ConstExprPoisoned<PoisonA, DerivedRefLogger*> p3 = &a;
        ConstExprPoisoned<PoisonB, RefLogger*> p4 = WTFMove(p3);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&a, p4.unpoisoned());

        ASSERT_TRUE(p1.bits() == p2.bits());
        ASSERT_TRUE(p3.bits() != p4.bits());
    }

    {
        ConstExprPoisoned<PoisonA, RefLogger*> ptr(&a);
        ASSERT_EQ(&a, ptr.unpoisoned());
        ptr.clear();
        ASSERT_EQ(nullptr, ptr.unpoisoned());
    }
}

TEST(WTF_ConstExprPoisoned, Assignment)
{
    DerivedRefLogger a("a");
    RefLogger b("b");
    DerivedRefLogger c("c");

    {
        ConstExprPoisoned<PoisonA, RefLogger*> p1(&a);
        ConstExprPoisoned<PoisonA, RefLogger*> p2(&b);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&b, p2.unpoisoned());
        p1 = p2;
        ASSERT_EQ(&b, p1.unpoisoned());
        ASSERT_EQ(&b, p2.unpoisoned());

        ConstExprPoisoned<PoisonA, RefLogger*> p3(&a);
        ConstExprPoisoned<PoisonB, RefLogger*> p4(&b);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&b, p4.unpoisoned());
        p3 = p4;
        ASSERT_EQ(&b, p3.unpoisoned());
        ASSERT_EQ(&b, p4.unpoisoned());

        ASSERT_TRUE(p1.bits() == p2.bits());
        ASSERT_TRUE(p3.bits() != p4.bits());
    }

    {
        ConstExprPoisoned<PoisonA, RefLogger*> ptr(&a);
        ASSERT_EQ(&a, ptr.unpoisoned());
        ptr = &b;
        ASSERT_EQ(&b, ptr.unpoisoned());
    }

    {
        ConstExprPoisoned<PoisonA, RefLogger*> ptr(&a);
        ASSERT_EQ(&a, ptr.unpoisoned());
        ptr = nullptr;
        ASSERT_EQ(nullptr, ptr.unpoisoned());
    }

    {
        ConstExprPoisoned<PoisonA, RefLogger*> p1(&a);
        ConstExprPoisoned<PoisonA, RefLogger*> p2(&b);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&b, p2.unpoisoned());
        p1 = WTFMove(p2);
        ASSERT_EQ(&b, p1.unpoisoned());
        ASSERT_EQ(&b, p2.unpoisoned());

        ConstExprPoisoned<PoisonA, RefLogger*> p3(&a);
        ConstExprPoisoned<PoisonB, RefLogger*> p4(&b);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&b, p4.unpoisoned());
        p3 = WTFMove(p4);
        ASSERT_EQ(&b, p3.unpoisoned());
        ASSERT_EQ(&b, p4.unpoisoned());

        ASSERT_TRUE(p1.bits() == p2.bits());
        ASSERT_TRUE(p3.bits() != p4.bits());
    }

    {
        ConstExprPoisoned<PoisonA, RefLogger*> p1(&a);
        ConstExprPoisoned<PoisonA, DerivedRefLogger*> p2(&c);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&c, p2.unpoisoned());
        p1 = p2;
        ASSERT_EQ(&c, p1.unpoisoned());
        ASSERT_EQ(&c, p2.unpoisoned());

        ConstExprPoisoned<PoisonA, RefLogger*> p3(&a);
        ConstExprPoisoned<PoisonB, DerivedRefLogger*> p4(&c);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&c, p4.unpoisoned());
        p3 = p4;
        ASSERT_EQ(&c, p3.unpoisoned());
        ASSERT_EQ(&c, p4.unpoisoned());

        ASSERT_TRUE(p1.bits() == p2.bits());
        ASSERT_TRUE(p3.bits() != p4.bits());
    }

    {
        ConstExprPoisoned<PoisonA, RefLogger*> ptr(&a);
        ASSERT_EQ(&a, ptr.unpoisoned());
        ptr = &c;
        ASSERT_EQ(&c, ptr.unpoisoned());
    }

    {
        ConstExprPoisoned<PoisonA, RefLogger*> p1(&a);
        ConstExprPoisoned<PoisonA, DerivedRefLogger*> p2(&c);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&c, p2.unpoisoned());
        p1 = WTFMove(p2);
        ASSERT_EQ(&c, p1.unpoisoned());
        ASSERT_EQ(&c, p2.unpoisoned());

        ConstExprPoisoned<PoisonA, RefLogger*> p3(&a);
        ConstExprPoisoned<PoisonB, DerivedRefLogger*> p4(&c);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&c, p4.unpoisoned());
        p3 = WTFMove(p4);
        ASSERT_EQ(&c, p3.unpoisoned());
        ASSERT_EQ(&c, p4.unpoisoned());

        ASSERT_TRUE(p1.bits() == p2.bits());
        ASSERT_TRUE(p3.bits() != p4.bits());
    }

    {
        ConstExprPoisoned<PoisonA, RefLogger*> ptr(&a);
        ASSERT_EQ(&a, ptr.unpoisoned());
        ptr = ptr;
        ASSERT_EQ(&a, ptr.unpoisoned());
    }

    {
        ConstExprPoisoned<PoisonA, RefLogger*> ptr(&a);
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

TEST(WTF_ConstExprPoisoned, Swap)
{
    RefLogger a("a");
    RefLogger b("b");

    {
        ConstExprPoisoned<PoisonA, RefLogger*> p1(&a);
        ConstExprPoisoned<PoisonA, RefLogger*> p2(&b);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&b, p2.unpoisoned());
        p1.swap(p2);
        ASSERT_EQ(&b, p1.unpoisoned());
        ASSERT_EQ(&a, p2.unpoisoned());

        ConstExprPoisoned<PoisonA, RefLogger*> p3(&a);
        ConstExprPoisoned<PoisonB, RefLogger*> p4(&b);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&b, p4.unpoisoned());
        p3.swap(p4);
        ASSERT_EQ(&b, p3.unpoisoned());
        ASSERT_EQ(&a, p4.unpoisoned());

        ASSERT_TRUE(p1.bits() != p2.bits());
        ASSERT_TRUE(p3.bits() != p4.bits());
        ASSERT_TRUE(p1.bits() == p3.bits());
        ASSERT_TRUE(p2.bits() != p4.bits());
    }

    {
        ConstExprPoisoned<PoisonA, RefLogger*> p1(&a);
        ConstExprPoisoned<PoisonA, RefLogger*> p2(&b);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&b, p2.unpoisoned());
        swap(p1, p2);
        ASSERT_EQ(&b, p1.unpoisoned());
        ASSERT_EQ(&a, p2.unpoisoned());

        ConstExprPoisoned<PoisonA, RefLogger*> p3(&a);
        ConstExprPoisoned<PoisonB, RefLogger*> p4(&b);
        ASSERT_EQ(&a, p3.unpoisoned());
        ASSERT_EQ(&b, p4.unpoisoned());
        swap(p3, p4);
        ASSERT_EQ(&b, p3.unpoisoned());
        ASSERT_EQ(&a, p4.unpoisoned());

        ASSERT_TRUE(p1.bits() != p2.bits());
        ASSERT_TRUE(p3.bits() != p4.bits());
        ASSERT_TRUE(p1.bits() == p3.bits());
        ASSERT_TRUE(p2.bits() != p4.bits());
    }

    {
        ConstExprPoisoned<PoisonA, RefLogger*> p1(&a);
        RefLogger* p2(&b);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&b, p2);
        swap(p1, p2);
        ASSERT_EQ(&b, p1.unpoisoned());
        ASSERT_EQ(&a, p2);

        ASSERT_TRUE(p1.bits() != bitwise_cast<uintptr_t>(p2));
    }

    {
        ConstExprPoisoned<PoisonA, RefLogger*> p1(&a);
        RefLogger* p2(&b);
        ASSERT_EQ(&a, p1.unpoisoned());
        ASSERT_EQ(&b, p2);
        p1.swap(p2);
        ASSERT_EQ(&b, p1.unpoisoned());
        ASSERT_EQ(&a, p2);

        ASSERT_TRUE(p1.bits() != bitwise_cast<uintptr_t>(p2));
    }
}

static ConstExprPoisoned<PoisonA, RefLogger*> poisonedPtrFoo(RefLogger& logger)
{
    return ConstExprPoisoned<PoisonA, RefLogger*>(&logger);
}

TEST(WTF_ConstExprPoisoned, ReturnValue)
{
    DerivedRefLogger a("a");

    {
        auto ptr = poisonedPtrFoo(a);
        ASSERT_EQ(&a, ptr.unpoisoned());
        ASSERT_EQ(&a, &*ptr);
        ASSERT_EQ(&a.name, &ptr->name);
    }
}

} // namespace TestWebKitAPI


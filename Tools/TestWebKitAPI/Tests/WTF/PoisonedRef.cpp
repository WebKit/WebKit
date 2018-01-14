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
#include <wtf/RefPtr.h>

namespace TestWebKitAPI {

namespace {

uintptr_t g_poisonA;
uintptr_t g_poisonB;
uintptr_t g_poisonC;
uintptr_t g_poisonD;
uintptr_t g_poisonE;
uintptr_t g_poisonF;

static void initializePoisons()
{
    static std::once_flag initializeOnceFlag;
    std::call_once(initializeOnceFlag, [] {
        g_poisonA = makePoison();
        g_poisonB = makePoison();
        g_poisonC = makePoison();
        g_poisonD = makePoison();
        g_poisonF = makePoison();
        g_poisonF = makePoison();
    });
}

} // namespace anonymous

TEST(WTF_PoisonedRef, Basic)
{
    initializePoisons();

    DerivedRefLogger a("a");

    {
        PoisonedRef<g_poisonA, RefLogger> ref(a);
        EXPECT_EQ(&a, ref.ptr());
        EXPECT_EQ(&a.name, &ref->name);

#if ENABLE(POISON)
        uintptr_t ptrBits;
        std::memcpy(&ptrBits, &ref, sizeof(ptrBits));
        ASSERT_TRUE(ptrBits != bitwise_cast<uintptr_t>(&a));
        ASSERT_EQ(ptrBits, (Poisoned<g_poisonA, RefLogger*>(&a).bits()));
#if ENABLE(POISON_ASSERTS)
        ASSERT_TRUE((Poisoned<g_poisonA, RefLogger*>::isPoisoned(ptrBits)));
#endif
#endif // ENABLE(POISON)
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PoisonedRef<g_poisonB, RefLogger> ref(adoptRef(a));
        EXPECT_EQ(&a, ref.ptr());
        EXPECT_EQ(&a.name, &ref->name);
    }
    EXPECT_STREQ("deref(a) ", takeLogStr().c_str());
}

TEST(WTF_PoisonedRef, Assignment)
{
    initializePoisons();

    DerivedRefLogger a("a");
    RefLogger b("b");
    DerivedRefLogger c("c");

    {
        PoisonedRef<g_poisonC, RefLogger> ref(a);
        EXPECT_EQ(&a, ref.ptr());
        log() << "| ";
        ref = b;
        EXPECT_EQ(&b, ref.ptr());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | ref(b) deref(a) | deref(b) ", takeLogStr().c_str());

    {
        PoisonedRef<g_poisonD, RefLogger> ref(a);
        EXPECT_EQ(&a, ref.ptr());
        log() << "| ";
        ref = c;
        EXPECT_EQ(&c, ref.ptr());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | ref(c) deref(a) | deref(c) ", takeLogStr().c_str());

    {
        PoisonedRef<g_poisonE, RefLogger> ref(a);
        EXPECT_EQ(&a, ref.ptr());
        log() << "| ";
        ref = adoptRef(b);
        EXPECT_EQ(&b, ref.ptr());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | deref(a) | deref(b) ", takeLogStr().c_str());

    {
        PoisonedRef<g_poisonF, RefLogger> ref(a);
        EXPECT_EQ(&a, ref.ptr());
        log() << "| ";
        ref = adoptRef(c);
        EXPECT_EQ(&c, ref.ptr());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | deref(a) | deref(c) ", takeLogStr().c_str());
}

static PoisonedRef<g_poisonB, RefLogger> passWithRef(PoisonedRef<g_poisonC, RefLogger>&& reference)
{
    return WTFMove(reference);
}

TEST(WTF_PoisonedRef, ReturnValue)
{
    initializePoisons();

    DerivedRefLogger a("a");
    RefLogger b("b");
    DerivedRefLogger c("c");

    {
        PoisonedRef<g_poisonB, RefLogger> ref(passWithRef(PoisonedRef<g_poisonC, RefLogger>(a)));
        EXPECT_EQ(&a, ref.ptr());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PoisonedRef<g_poisonD, RefLogger> ref(a);
        EXPECT_EQ(&a, ref.ptr());
        log() << "| ";
        ref = passWithRef(b);
        EXPECT_EQ(&b, ref.ptr());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | ref(b) deref(a) | deref(b) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<g_poisonE, RefLogger> ptr(passWithRef(a));
        EXPECT_EQ(&a, ptr.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<g_poisonF, DerivedRefLogger> ptr(&a);
        PoisonedRefPtr<g_poisonA, RefLogger> ptr2(WTFMove(ptr));
        EXPECT_EQ(nullptr, ptr.get());
        EXPECT_EQ(&a, ptr2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PoisonedRef<g_poisonB, DerivedRefLogger> derivedReference(a);
        PoisonedRef<g_poisonC, RefLogger> baseReference(passWithRef(derivedReference.copyRef()));
        EXPECT_EQ(&a, derivedReference.ptr());
        EXPECT_EQ(&a, baseReference.ptr());
    }
    EXPECT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_PoisonedRef, Swap)
{
    initializePoisons();

    RefLogger a("a");
    RefLogger b("b");

    {
        PoisonedRef<g_poisonD, RefLogger> p1(a);
        PoisonedRef<g_poisonE, RefLogger> p2(b);
        log() << "| ";
        EXPECT_EQ(&a, p1.ptr());
        EXPECT_EQ(&b, p2.ptr());
        p1.swap(p2);
        EXPECT_EQ(&b, p1.ptr());
        EXPECT_EQ(&a, p2.ptr());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) ref(b) | | deref(a) deref(b) ", takeLogStr().c_str());

    {
        PoisonedRef<g_poisonF, RefLogger> p1(a);
        PoisonedRef<g_poisonA, RefLogger> p2(b);
        log() << "| ";
        EXPECT_EQ(&a, p1.ptr());
        EXPECT_EQ(&b, p2.ptr());
        swap(p1, p2);
        EXPECT_EQ(&b, p1.ptr());
        EXPECT_EQ(&a, p2.ptr());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) ref(b) | | deref(a) deref(b) ", takeLogStr().c_str());

    {
        PoisonedRef<g_poisonF, RefLogger> p1(a);
        Ref<RefLogger> p2(b);
        log() << "| ";
        EXPECT_EQ(&a, p1.ptr());
        EXPECT_EQ(&b, p2.ptr());
        swap(p1, p2);
        EXPECT_EQ(&b, p1.ptr());
        EXPECT_EQ(&a, p2.ptr());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) ref(b) | | deref(a) deref(b) ", takeLogStr().c_str());

    {
        PoisonedRef<g_poisonF, RefLogger> p1(a);
        Ref<RefLogger> p2(b);
        log() << "| ";
        EXPECT_EQ(&a, p1.ptr());
        EXPECT_EQ(&b, p2.ptr());
        p1.swap(p2);
        EXPECT_EQ(&b, p1.ptr());
        EXPECT_EQ(&a, p2.ptr());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) ref(b) | | deref(a) deref(b) ", takeLogStr().c_str());
}

struct PoisonedRefCheckingRefLogger : RefLogger {
    using Ref = PoisonedRef<g_poisonB, PoisonedRefCheckingRefLogger>;

    PoisonedRefCheckingRefLogger(const char* name);
    void ref();
    void deref();
    const Ref* slotToCheck { nullptr };
};

struct DerivedPoisonedRefCheckingRefLogger : PoisonedRefCheckingRefLogger {
    DerivedPoisonedRefCheckingRefLogger(const char* name);
};

PoisonedRefCheckingRefLogger::PoisonedRefCheckingRefLogger(const char* name)
    : RefLogger { name }
{
}

void PoisonedRefCheckingRefLogger::ref()
{
    if (slotToCheck)
        log() << "slot=" << slotToCheck->get().name << " ";
    RefLogger::ref();
}

void PoisonedRefCheckingRefLogger::deref()
{
    if (slotToCheck)
        log() << "slot=" << slotToCheck->get().name << " ";
    RefLogger::deref();
}

DerivedPoisonedRefCheckingRefLogger::DerivedPoisonedRefCheckingRefLogger(const char* name)
    : PoisonedRefCheckingRefLogger { name }
{
}

TEST(WTF_PoisonedRef, AssignBeforeDeref)
{
    initializePoisons();

    DerivedPoisonedRefCheckingRefLogger a("a");
    PoisonedRefCheckingRefLogger b("b");
    DerivedPoisonedRefCheckingRefLogger c("c");

    {
        PoisonedRefCheckingRefLogger::Ref ref(a);
        EXPECT_EQ(&a, ref.ptr());
        log() << "| ";
        a.slotToCheck = &ref;
        b.slotToCheck = &ref;
        ref = b;
        a.slotToCheck = nullptr;
        b.slotToCheck = nullptr;
        EXPECT_EQ(&b, ref.ptr());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | slot=a ref(b) slot=b deref(a) | deref(b) ", takeLogStr().c_str());

    {
        PoisonedRefCheckingRefLogger::Ref ref(a);
        EXPECT_EQ(&a, ref.ptr());
        log() << "| ";
        a.slotToCheck = &ref;
        c.slotToCheck = &ref;
        ref = c;
        a.slotToCheck = nullptr;
        c.slotToCheck = nullptr;
        EXPECT_EQ(&c, ref.ptr());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | slot=a ref(c) slot=c deref(a) | deref(c) ", takeLogStr().c_str());

    {
        PoisonedRefCheckingRefLogger::Ref ref(a);
        EXPECT_EQ(&a, ref.ptr());
        log() << "| ";
        a.slotToCheck = &ref;
        ref = adoptRef(b);
        a.slotToCheck = nullptr;
        EXPECT_EQ(&b, ref.ptr());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | slot=b deref(a) | deref(b) ", takeLogStr().c_str());

    {
        PoisonedRefCheckingRefLogger::Ref ref(a);
        EXPECT_EQ(&a, ref.ptr());
        log() << "| ";
        a.slotToCheck = &ref;
        ref = adoptRef(c);
        a.slotToCheck = nullptr;
        EXPECT_EQ(&c, ref.ptr());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | slot=c deref(a) | deref(c) ", takeLogStr().c_str());
}

} // namespace TestWebKitAPI

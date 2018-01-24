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
#include <wtf/NeverDestroyed.h>
#include <wtf/Poisoned.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace TestWebKitAPI {

namespace {

uintptr_t g_poisonA;
uintptr_t g_poisonB;
uintptr_t g_poisonC;
uintptr_t g_poisonD;
uintptr_t g_poisonE;
uintptr_t g_poisonF;

using PoisonA = Poison<g_poisonA>;
using PoisonB = Poison<g_poisonB>;
using PoisonC = Poison<g_poisonC>;
using PoisonD = Poison<g_poisonD>;
using PoisonE = Poison<g_poisonE>;
using PoisonF = Poison<g_poisonF>;

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

TEST(WTF_PoisonedRefPtr, Basic)
{
    initializePoisons();

    DerivedRefLogger a("a");

    PoisonedRefPtr<PoisonA, RefLogger> empty;
    EXPECT_EQ(nullptr, empty.get());

    {
        PoisonedRefPtr<PoisonB, RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        EXPECT_EQ(&a, &*ptr);
        EXPECT_EQ(&a.name, &ptr->name);

#if ENABLE(POISON)
        uintptr_t ptrBits;
        std::memcpy(&ptrBits, &ptr, sizeof(ptrBits));
        ASSERT_TRUE(ptrBits != bitwise_cast<uintptr_t>(&a));
        ASSERT_EQ(ptrBits, (Poisoned<PoisonB, RefLogger*>(&a).bits()));
#if ENABLE(POISON_ASSERTS)
        ASSERT_TRUE((Poisoned<PoisonB, RefLogger*>::isPoisoned(ptrBits)));
#endif
#endif // ENABLE(POISON)
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<PoisonC, RefLogger> ptr = &a;
        EXPECT_EQ(&a, ptr.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<PoisonD, RefLogger> p1 = &a;
        PoisonedRefPtr<PoisonE, RefLogger> p2(p1);
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<PoisonF, RefLogger> p1 = &a;
        PoisonedRefPtr<PoisonB, RefLogger> p2 = p1;
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<PoisonC, RefLogger> p1 = &a;
        PoisonedRefPtr<PoisonD, RefLogger> p2 = WTFMove(p1);
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<PoisonE, RefLogger> p1 = &a;
        PoisonedRefPtr<PoisonF, RefLogger> p2(WTFMove(p1));
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<PoisonB, DerivedRefLogger> p1 = &a;
        PoisonedRefPtr<PoisonC, RefLogger> p2 = p1;
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<PoisonD, DerivedRefLogger> p1 = &a;
        PoisonedRefPtr<PoisonE, RefLogger> p2 = WTFMove(p1);
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<PoisonF, RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        ptr = nullptr;
        EXPECT_EQ(nullptr, ptr.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_PoisonedRefPtr, AssignPassRefToPoisonedRefPtr)
{
    initializePoisons();

    DerivedRefLogger a("a");
    {
        Ref<RefLogger> passRef(a);
        PoisonedRefPtr<PoisonB, RefLogger> ptr = WTFMove(passRef);
        EXPECT_EQ(&a, ptr.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_PoisonedRefPtr, Adopt)
{
    initializePoisons();

    DerivedRefLogger a("a");

    PoisonedRefPtr<PoisonC, RefLogger> empty;
    EXPECT_EQ(nullptr, empty.get());

    {
        PoisonedRefPtr<PoisonD, RefLogger> ptr(adoptRef(&a));
        EXPECT_EQ(&a, ptr.get());
        EXPECT_EQ(&a, &*ptr);
        EXPECT_EQ(&a.name, &ptr->name);
    }
    EXPECT_STREQ("deref(a) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<PoisonE, RefLogger> ptr = adoptRef(&a);
        EXPECT_EQ(&a, ptr.get());
    }
    EXPECT_STREQ("deref(a) ", takeLogStr().c_str());
}

TEST(WTF_PoisonedRefPtr, Assignment)
{
    initializePoisons();

    DerivedRefLogger a("a");
    RefLogger b("b");
    DerivedRefLogger c("c");

    {
        PoisonedRefPtr<PoisonF, RefLogger> p1(&a);
        PoisonedRefPtr<PoisonB, RefLogger> p2(&b);
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&b, p2.get());
        log() << "| ";
        p1 = p2;
        EXPECT_EQ(&b, p1.get());
        EXPECT_EQ(&b, p2.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) ref(b) | ref(b) deref(a) | deref(b) deref(b) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<PoisonC, RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = &b;
        EXPECT_EQ(&b, ptr.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | ref(b) deref(a) | deref(b) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<PoisonD, RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = adoptRef(&b);
        EXPECT_EQ(&b, ptr.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | deref(a) | deref(b) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<PoisonE, RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        ptr = nullptr;
        EXPECT_EQ(nullptr, ptr.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<PoisonB, RefLogger> p1(&a);
        PoisonedRefPtr<PoisonC, RefLogger> p2(&b);
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&b, p2.get());
        log() << "| ";
        p1 = WTFMove(p2);
        EXPECT_EQ(&b, p1.get());
        EXPECT_EQ(nullptr, p2.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) ref(b) | deref(a) | deref(b) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<PoisonD, RefLogger> p1(&a);
        PoisonedRefPtr<PoisonE, DerivedRefLogger> p2(&c);
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&c, p2.get());
        log() << "| ";
        p1 = p2;
        EXPECT_EQ(&c, p1.get());
        EXPECT_EQ(&c, p2.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) ref(c) | ref(c) deref(a) | deref(c) deref(c) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<PoisonF, RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = &c;
        EXPECT_EQ(&c, ptr.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | ref(c) deref(a) | deref(c) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<PoisonB, RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = adoptRef(&c);
        EXPECT_EQ(&c, ptr.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | deref(a) | deref(c) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<PoisonC, RefLogger> p1(&a);
        PoisonedRefPtr<PoisonD, DerivedRefLogger> p2(&c);
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&c, p2.get());
        log() << "| ";
        p1 = WTFMove(p2);
        EXPECT_EQ(&c, p1.get());
        EXPECT_EQ(nullptr, p2.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) ref(c) | deref(a) | deref(c) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<PoisonE, RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = ptr;
        EXPECT_EQ(&a, ptr.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | ref(a) deref(a) | deref(a) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<PoisonF, RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
#if COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wself-move"
#endif
        ptr = WTFMove(ptr);
#if COMPILER(CLANG)
#pragma clang diagnostic pop
#endif
        EXPECT_EQ(&a, ptr.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_PoisonedRefPtr, Swap)
{
    initializePoisons();

    RefLogger a("a");
    RefLogger b("b");

    {
        PoisonedRefPtr<PoisonB, RefLogger> p1(&a);
        PoisonedRefPtr<PoisonC, RefLogger> p2(&b);
        log() << "| ";
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&b, p2.get());
        p1.swap(p2);
        EXPECT_EQ(&b, p1.get());
        EXPECT_EQ(&a, p2.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) ref(b) | | deref(a) deref(b) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<PoisonD, RefLogger> p1(&a);
        PoisonedRefPtr<PoisonE, RefLogger> p2(&b);
        log() << "| ";
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&b, p2.get());
        swap(p1, p2);
        EXPECT_EQ(&b, p1.get());
        EXPECT_EQ(&a, p2.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) ref(b) | | deref(a) deref(b) ", takeLogStr().c_str());
}

TEST(WTF_PoisonedRefPtr, ReleaseNonNull)
{
    initializePoisons();

    RefLogger a("a");

    {
        PoisonedRefPtr<PoisonF, RefLogger> refPtr = &a;
        PoisonedRefPtr<PoisonB, RefLogger> ref = refPtr.releaseNonNull();
    }

    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_PoisonedRefPtr, Release)
{
    initializePoisons();

    DerivedRefLogger a("a");
    RefLogger b("b");
    DerivedRefLogger c("c");

    {
        PoisonedRefPtr<PoisonC, RefLogger> p1 = &a;
        PoisonedRefPtr<PoisonD, RefLogger> p2 = WTFMove(p1);
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<PoisonE, RefLogger> p1 = &a;
        PoisonedRefPtr<PoisonF, RefLogger> p2(WTFMove(p1));
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<PoisonB, DerivedRefLogger> p1 = &a;
        PoisonedRefPtr<PoisonC, RefLogger> p2 = WTFMove(p1);
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<PoisonD, RefLogger> p1(&a);
        PoisonedRefPtr<PoisonE, RefLogger> p2(&b);
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&b, p2.get());
        log() << "| ";
        p1 = WTFMove(p2);
        EXPECT_EQ(&b, p1.get());
        EXPECT_EQ(nullptr, p2.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) ref(b) | deref(a) | deref(b) ", takeLogStr().c_str());

    {
        PoisonedRefPtr<PoisonF, RefLogger> p1(&a);
        PoisonedRefPtr<PoisonB, DerivedRefLogger> p2(&c);
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&c, p2.get());
        log() << "| ";
        p1 = WTFMove(p2);
        EXPECT_EQ(&c, p1.get());
        EXPECT_EQ(nullptr, p2.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) ref(c) | deref(a) | deref(c) ", takeLogStr().c_str());
}

static PoisonedRefPtr<PoisonC, RefLogger> f1(RefLogger& logger)
{
    return PoisonedRefPtr<PoisonC, RefLogger>(&logger);
}

TEST(WTF_PoisonedRefPtr, ReturnValue)
{
    initializePoisons();

    DerivedRefLogger a("a");

    {
        f1(a);
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        auto ptr = f1(a);
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

struct ConstRefCounted : RefCounted<ConstRefCounted> {
    static Ref<ConstRefCounted> create() { return adoptRef(*new ConstRefCounted); }
};

static const ConstRefCounted& returnConstRefCountedRef()
{
    static NeverDestroyed<ConstRefCounted> instance;
    return instance.get();
}
static ConstRefCounted& returnRefCountedRef()
{
    static NeverDestroyed<ConstRefCounted> instance;
    return instance.get();
}

TEST(WTF_PoisonedRefPtr, Const)
{
    initializePoisons();

    // This test passes if it compiles without an error.
    auto a = ConstRefCounted::create();
    Ref<const ConstRefCounted> b = WTFMove(a);
    PoisonedRefPtr<PoisonD, const ConstRefCounted> c = b.ptr();
    Ref<const ConstRefCounted> d = returnConstRefCountedRef();
    PoisonedRefPtr<PoisonE, const ConstRefCounted> e = &returnConstRefCountedRef();
    PoisonedRefPtr<PoisonF, ConstRefCounted> f = ConstRefCounted::create();
    PoisonedRefPtr<PoisonB, const ConstRefCounted> g = f;
    PoisonedRefPtr<PoisonC, const ConstRefCounted> h(f);
    Ref<const ConstRefCounted> i(returnRefCountedRef());
}

struct PoisonedRefPtrCheckingRefLogger : RefLogger {
    using Ref = PoisonedRefPtr<PoisonD, PoisonedRefPtrCheckingRefLogger>;

    PoisonedRefPtrCheckingRefLogger(const char* name);
    void ref();
    void deref();
    const Ref* slotToCheck { nullptr };
};

PoisonedRefPtrCheckingRefLogger::PoisonedRefPtrCheckingRefLogger(const char* name)
    : RefLogger { name }
{
}

static const char* loggerName(const PoisonedRefPtrCheckingRefLogger::Ref& pointer)
{
    return pointer ? &pointer->name : "null";
}

void PoisonedRefPtrCheckingRefLogger::ref()
{
    if (slotToCheck)
        log() << "slot=" << loggerName(*slotToCheck) << " ";
    RefLogger::ref();
}

void PoisonedRefPtrCheckingRefLogger::deref()
{
    if (slotToCheck)
        log() << "slot=" << loggerName(*slotToCheck) << " ";
    RefLogger::deref();
}

TEST(WTF_PoisonedRefPtr, AssignBeforeDeref)
{
    initializePoisons();

    PoisonedRefPtrCheckingRefLogger a("a");
    PoisonedRefPtrCheckingRefLogger b("b");

    {
        PoisonedRefPtrCheckingRefLogger::Ref p1(&a);
        PoisonedRefPtrCheckingRefLogger::Ref p2(&b);
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&b, p2.get());
        log() << "| ";
        a.slotToCheck = &p1;
        b.slotToCheck = &p1;
        p1 = p2;
        a.slotToCheck = nullptr;
        b.slotToCheck = nullptr;
        EXPECT_EQ(&b, p1.get());
        EXPECT_EQ(&b, p2.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) ref(b) | slot=a ref(b) slot=b deref(a) | deref(b) deref(b) ", takeLogStr().c_str());

    {
        PoisonedRefPtrCheckingRefLogger::Ref ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        log() << "| ";
        a.slotToCheck = &ptr;
        b.slotToCheck = &ptr;
        ptr = &b;
        a.slotToCheck = nullptr;
        b.slotToCheck = nullptr;
        EXPECT_EQ(&b, ptr.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | slot=a ref(b) slot=b deref(a) | deref(b) ", takeLogStr().c_str());

    {
        PoisonedRefPtrCheckingRefLogger::Ref ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        a.slotToCheck = &ptr;
        ptr = nullptr;
        a.slotToCheck = nullptr;
        EXPECT_EQ(nullptr, ptr.get());
    }
    EXPECT_STREQ("ref(a) slot=null deref(a) ", takeLogStr().c_str());

    {
        PoisonedRefPtrCheckingRefLogger::Ref p1(&a);
        PoisonedRefPtrCheckingRefLogger::Ref p2(&b);
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&b, p2.get());
        log() << "| ";
        a.slotToCheck = &p1;
        b.slotToCheck = &p1;
        p1 = WTFMove(p2);
        a.slotToCheck = nullptr;
        b.slotToCheck = nullptr;
        EXPECT_EQ(&b, p1.get());
        EXPECT_EQ(nullptr, p2.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) ref(b) | slot=b deref(a) | deref(b) ", takeLogStr().c_str());
}

TEST(WTF_PoisonedRefPtr, ReleaseNonNullBeforeDeref)
{
    initializePoisons();

    PoisonedRefPtrCheckingRefLogger a("a");

    {
        PoisonedRefPtrCheckingRefLogger::Ref refPtr = &a;
        a.slotToCheck = &refPtr;
        refPtr.releaseNonNull();
        a.slotToCheck = nullptr;
    }

    EXPECT_STREQ("ref(a) slot=null deref(a) ", takeLogStr().c_str());
}

} // namespace TestWebKitAPI

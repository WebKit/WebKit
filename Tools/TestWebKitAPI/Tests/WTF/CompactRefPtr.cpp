/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include <wtf/CompactRefPtr.h>

#include "AlignedRefLogger.h"
#include "Utilities.h"
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RefCounted.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>

namespace TestWebKitAPI {

TEST(WTF_CompactRefPtr, Basic)
{
    DerivedAlignedRefLogger a("a");

    CompactRefPtr<AlignedRefLogger> empty;
    EXPECT_EQ(nullptr, empty.get());

    {
        CompactRefPtr<AlignedRefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        EXPECT_EQ(&a, &*ptr);
        EXPECT_EQ(&a.name, &ptr->name);
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<AlignedRefLogger> ptr = &a;
        EXPECT_EQ(&a, ptr.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<AlignedRefLogger> p1 = &a;
        CompactRefPtr<AlignedRefLogger> p2(p1);
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<AlignedRefLogger> p1 = &a;
        CompactRefPtr<AlignedRefLogger> p2 = p1;
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<AlignedRefLogger> p1 = &a;
        CompactRefPtr<AlignedRefLogger> p2 = WTFMove(p1);
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<AlignedRefLogger> p1 = &a;
        CompactRefPtr<AlignedRefLogger> p2(WTFMove(p1));
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<DerivedAlignedRefLogger> p1 = &a;
        CompactRefPtr<AlignedRefLogger> p2 = p1;
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<DerivedAlignedRefLogger> p1 = &a;
        CompactRefPtr<AlignedRefLogger> p2 = WTFMove(p1);
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<AlignedRefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        ptr = nullptr;
        EXPECT_EQ(nullptr, ptr.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<AlignedRefLogger> ptr(nullptr);
        EXPECT_EQ(nullptr, ptr.get());
        EXPECT_EQ(false, static_cast<bool>(ptr));
    }
    EXPECT_STREQ("", takeLogStr().c_str());

    {
        CompactRefPtr<AlignedRefLogger> ptr = nullptr;
        EXPECT_EQ(nullptr, ptr.get());
        EXPECT_EQ(true, !ptr);
    }
    EXPECT_STREQ("", takeLogStr().c_str());
}

TEST(WTF_CompactRefPtr, AssignPassRefToCompactRefPtr)
{
    DerivedAlignedRefLogger a("a");
    {
        Ref<AlignedRefLogger> passRef(a);
        CompactRefPtr<AlignedRefLogger> ptr = WTFMove(passRef);
        EXPECT_EQ(&a, ptr.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_CompactRefPtr, Adopt)
{
    DerivedAlignedRefLogger a("a");

    CompactRefPtr<AlignedRefLogger> empty;
    EXPECT_EQ(nullptr, empty.get());

    {
        CompactRefPtr<AlignedRefLogger> ptr(adoptRef(&a));
        EXPECT_EQ(&a, ptr.get());
        EXPECT_EQ(&a, &*ptr);
        EXPECT_EQ(&a.name, &ptr->name);
    }
    EXPECT_STREQ("deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<AlignedRefLogger> ptr = adoptRef(&a);
        EXPECT_EQ(&a, ptr.get());
    }
    EXPECT_STREQ("deref(a) ", takeLogStr().c_str());
}

TEST(WTF_CompactRefPtr, Assignment)
{
    DerivedAlignedRefLogger a("a");
    AlignedRefLogger b("b");
    DerivedAlignedRefLogger c("c");

    {
        CompactRefPtr<AlignedRefLogger> p1(&a);
        CompactRefPtr<AlignedRefLogger> p2(&b);
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
        CompactRefPtr<AlignedRefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = &b;
        EXPECT_EQ(&b, ptr.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | ref(b) deref(a) | deref(b) ", takeLogStr().c_str());

    {
        CompactRefPtr<AlignedRefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = adoptRef(&b);
        EXPECT_EQ(&b, ptr.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | deref(a) | deref(b) ", takeLogStr().c_str());

    {
        CompactRefPtr<AlignedRefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        ptr = nullptr;
        EXPECT_EQ(nullptr, ptr.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<AlignedRefLogger> p1(&a);
        CompactRefPtr<AlignedRefLogger> p2(&b);
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
        CompactRefPtr<AlignedRefLogger> p1(&a);
        CompactRefPtr<DerivedAlignedRefLogger> p2(&c);
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
        CompactRefPtr<AlignedRefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = &c;
        EXPECT_EQ(&c, ptr.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | ref(c) deref(a) | deref(c) ", takeLogStr().c_str());

    {
        CompactRefPtr<AlignedRefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = adoptRef(&c);
        EXPECT_EQ(&c, ptr.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | deref(a) | deref(c) ", takeLogStr().c_str());

    {
        CompactRefPtr<AlignedRefLogger> p1(&a);
        CompactRefPtr<DerivedAlignedRefLogger> p2(&c);
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
        CompactRefPtr<AlignedRefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        log() << "| ";
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
        EXPECT_EQ(&a, ptr.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | ref(a) deref(a) | deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<AlignedRefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        IGNORE_WARNINGS_BEGIN("self-move")
        ptr = WTFMove(ptr);
        IGNORE_WARNINGS_END
        EXPECT_EQ(&a, ptr.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_CompactRefPtr, Swap)
{
    AlignedRefLogger a("a");
    AlignedRefLogger b("b");

    {
        CompactRefPtr<AlignedRefLogger> p1(&a);
        CompactRefPtr<AlignedRefLogger> p2(&b);
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
        CompactRefPtr<AlignedRefLogger> p1(&a);
        CompactRefPtr<AlignedRefLogger> p2(&b);
        log() << "| ";
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&b, p2.get());
        std::swap(p1, p2);
        EXPECT_EQ(&b, p1.get());
        EXPECT_EQ(&a, p2.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) ref(b) | | deref(a) deref(b) ", takeLogStr().c_str());
}

TEST(WTF_CompactRefPtr, ReleaseNonNull)
{
    AlignedRefLogger a("a");

    {
        CompactRefPtr<AlignedRefLogger> refPtr = &a;
        CompactRefPtr<AlignedRefLogger> ref = refPtr.releaseNonNull();
    }

    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_CompactRefPtr, Release)
{
    DerivedAlignedRefLogger a("a");
    AlignedRefLogger b("b");
    DerivedAlignedRefLogger c("c");

    {
        CompactRefPtr<AlignedRefLogger> p1 = &a;
        CompactRefPtr<AlignedRefLogger> p2 = WTFMove(p1);
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<AlignedRefLogger> p1 = &a;
        CompactRefPtr<AlignedRefLogger> p2(WTFMove(p1));
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<DerivedAlignedRefLogger> p1 = &a;
        CompactRefPtr<AlignedRefLogger> p2 = WTFMove(p1);
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<AlignedRefLogger> p1(&a);
        CompactRefPtr<AlignedRefLogger> p2(&b);
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
        CompactRefPtr<AlignedRefLogger> p1(&a);
        CompactRefPtr<DerivedAlignedRefLogger> p2(&c);
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

static CompactRefPtr<AlignedRefLogger> f1(AlignedRefLogger& logger)
{
    return CompactRefPtr<AlignedRefLogger>(&logger);
}

TEST(WTF_CompactRefPtr, ReturnValue)
{
    DerivedAlignedRefLogger a("a");

    {
        f1(a);
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        auto ptr = f1(a);
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

struct alignas(16) ConstRefCounted
    : RefCounted<ConstRefCounted> {
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

TEST(WTF_CompactRefPtr, Const)
{
    // This test passes if it compiles without an error.
    auto a = ConstRefCounted::create();
    Ref<const ConstRefCounted> b = WTFMove(a);
    CompactRefPtr<const ConstRefCounted> c = b.ptr();
    Ref<const ConstRefCounted> d = returnConstRefCountedRef();
    CompactRefPtr<const ConstRefCounted> e = &returnConstRefCountedRef();
    CompactRefPtr<ConstRefCounted> f = ConstRefCounted::create();
    CompactRefPtr<const ConstRefCounted> g = f;
    CompactRefPtr<const ConstRefCounted> h(f);
    Ref<const ConstRefCounted> i(returnRefCountedRef());
}

struct CompactRefPtrCheckingAlignedRefLogger : AlignedRefLogger {
    CompactRefPtrCheckingAlignedRefLogger(const char* name);
    void ref();
    void deref();
    const CompactRefPtr<CompactRefPtrCheckingAlignedRefLogger>* slotToCheck { nullptr };
};

CompactRefPtrCheckingAlignedRefLogger::CompactRefPtrCheckingAlignedRefLogger(const char* name)
    : AlignedRefLogger { name }
{
}

static const char* loggerName(const CompactRefPtr<CompactRefPtrCheckingAlignedRefLogger>& pointer)
{
    return pointer ? &pointer->name : "null";
}

void CompactRefPtrCheckingAlignedRefLogger::ref()
{
    if (slotToCheck)
        log() << "slot=" << loggerName(*slotToCheck) << " ";
    AlignedRefLogger::ref();
}

void CompactRefPtrCheckingAlignedRefLogger::deref()
{
    if (slotToCheck)
        log() << "slot=" << loggerName(*slotToCheck) << " ";
    AlignedRefLogger::deref();
}

TEST(WTF_CompactRefPtr, AssignBeforeDeref)
{
    CompactRefPtrCheckingAlignedRefLogger a("a");
    CompactRefPtrCheckingAlignedRefLogger b("b");

    {
        CompactRefPtr<CompactRefPtrCheckingAlignedRefLogger> p1(&a);
        CompactRefPtr<CompactRefPtrCheckingAlignedRefLogger> p2(&b);
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
        CompactRefPtr<CompactRefPtrCheckingAlignedRefLogger> ptr(&a);
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
        CompactRefPtr<CompactRefPtrCheckingAlignedRefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        a.slotToCheck = &ptr;
        ptr = nullptr;
        a.slotToCheck = nullptr;
        EXPECT_EQ(nullptr, ptr.get());
    }
    EXPECT_STREQ("ref(a) slot=null deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<CompactRefPtrCheckingAlignedRefLogger> p1(&a);
        CompactRefPtr<CompactRefPtrCheckingAlignedRefLogger> p2(&b);
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

TEST(WTF_CompactRefPtr, ReleaseNonNullBeforeDeref)
{
    CompactRefPtrCheckingAlignedRefLogger a("a");

    {
        CompactRefPtr<CompactRefPtrCheckingAlignedRefLogger> refPtr = &a;
        a.slotToCheck = &refPtr;
        refPtr.releaseNonNull();
        a.slotToCheck = nullptr;
    }

    EXPECT_STREQ("ref(a) slot=null deref(a) ", takeLogStr().c_str());
}

} // namespace TestWebKitAPI

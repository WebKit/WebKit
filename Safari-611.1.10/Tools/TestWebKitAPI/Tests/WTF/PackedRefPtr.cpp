/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "RefLogger.h"
#include "Utilities.h"
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/PackedRef.h>
#include <wtf/PackedRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>

namespace TestWebKitAPI {

TEST(WTF_PackedRefPtr, Basic)
{
    DerivedRefLogger a("a");

    PackedRefPtr<RefLogger> empty;
    EXPECT_EQ(nullptr, empty.get());

    {
        PackedRefPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        EXPECT_EQ(&a, &*ptr);
        EXPECT_EQ(&a.name, &ptr->name);
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PackedRefPtr<RefLogger> ptr = &a;
        EXPECT_EQ(&a, ptr.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PackedRefPtr<RefLogger> p1 = &a;
        PackedRefPtr<RefLogger> p2(p1);
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());

    {
        PackedRefPtr<RefLogger> p1 = &a;
        PackedRefPtr<RefLogger> p2 = p1;
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());

    {
        PackedRefPtr<RefLogger> p1 = &a;
        PackedRefPtr<RefLogger> p2 = WTFMove(p1);
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PackedRefPtr<RefLogger> p1 = &a;
        PackedRefPtr<RefLogger> p2(WTFMove(p1));
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PackedRefPtr<DerivedRefLogger> p1 = &a;
        PackedRefPtr<RefLogger> p2 = p1;
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());

    {
        PackedRefPtr<DerivedRefLogger> p1 = &a;
        PackedRefPtr<RefLogger> p2 = WTFMove(p1);
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PackedRefPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        ptr = nullptr;
        EXPECT_EQ(nullptr, ptr.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_PackedRefPtr, AssignPassRefToRefPtr)
{
    DerivedRefLogger a("a");
    {
        PackedRef<RefLogger> passRef(a);
        PackedRefPtr<RefLogger> ptr = WTFMove(passRef);
        EXPECT_EQ(&a, ptr.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_PackedRefPtr, Adopt)
{
    DerivedRefLogger a("a");

    PackedRefPtr<RefLogger> empty;
    EXPECT_EQ(nullptr, empty.get());

    {
        PackedRefPtr<RefLogger> ptr(adoptRef(&a));
        EXPECT_EQ(&a, ptr.get());
        EXPECT_EQ(&a, &*ptr);
        EXPECT_EQ(&a.name, &ptr->name);
    }
    EXPECT_STREQ("deref(a) ", takeLogStr().c_str());

    {
        PackedRefPtr<RefLogger> ptr = adoptRef(&a);
        EXPECT_EQ(&a, ptr.get());
    }
    EXPECT_STREQ("deref(a) ", takeLogStr().c_str());
}

TEST(WTF_PackedRefPtr, Assignment)
{
    DerivedRefLogger a("a");
    RefLogger b("b");
    DerivedRefLogger c("c");

    {
        PackedRefPtr<RefLogger> p1(&a);
        PackedRefPtr<RefLogger> p2(&b);
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
        PackedRefPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = &b;
        EXPECT_EQ(&b, ptr.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | ref(b) deref(a) | deref(b) ", takeLogStr().c_str());

    {
        PackedRefPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = adoptRef(&b);
        EXPECT_EQ(&b, ptr.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | deref(a) | deref(b) ", takeLogStr().c_str());

    {
        PackedRefPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        ptr = nullptr;
        EXPECT_EQ(nullptr, ptr.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PackedRefPtr<RefLogger> p1(&a);
        PackedRefPtr<RefLogger> p2(&b);
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
        PackedRefPtr<RefLogger> p1(&a);
        PackedRefPtr<DerivedRefLogger> p2(&c);
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
        PackedRefPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = &c;
        EXPECT_EQ(&c, ptr.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | ref(c) deref(a) | deref(c) ", takeLogStr().c_str());

    {
        PackedRefPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = adoptRef(&c);
        EXPECT_EQ(&c, ptr.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | deref(a) | deref(c) ", takeLogStr().c_str());

    {
        PackedRefPtr<RefLogger> p1(&a);
        PackedRefPtr<DerivedRefLogger> p2(&c);
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
        PackedRefPtr<RefLogger> ptr(&a);
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
        PackedRefPtr<RefLogger> ptr(&a);
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

TEST(WTF_PackedRefPtr, Swap)
{
    RefLogger a("a");
    RefLogger b("b");

    {
        PackedRefPtr<RefLogger> p1(&a);
        PackedRefPtr<RefLogger> p2(&b);
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
        PackedRefPtr<RefLogger> p1(&a);
        PackedRefPtr<RefLogger> p2(&b);
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

TEST(WTF_PackedRefPtr, ReleaseNonNull)
{
    RefLogger a("a");

    {
        PackedRefPtr<RefLogger> refPtr = &a;
        PackedRefPtr<RefLogger> ref = refPtr.releaseNonNull();
    }

    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_PackedRefPtr, Release)
{
    DerivedRefLogger a("a");
    RefLogger b("b");
    DerivedRefLogger c("c");

    {
        PackedRefPtr<RefLogger> p1 = &a;
        PackedRefPtr<RefLogger> p2 = WTFMove(p1);
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PackedRefPtr<RefLogger> p1 = &a;
        PackedRefPtr<RefLogger> p2(WTFMove(p1));
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PackedRefPtr<DerivedRefLogger> p1 = &a;
        PackedRefPtr<RefLogger> p2 = WTFMove(p1);
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PackedRefPtr<RefLogger> p1(&a);
        PackedRefPtr<RefLogger> p2(&b);
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
        PackedRefPtr<RefLogger> p1(&a);
        PackedRefPtr<DerivedRefLogger> p2(&c);
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

static RefPtr<RefLogger> f1(RefLogger& logger)
{
    return PackedRefPtr<RefLogger>(&logger);
}

TEST(WTF_PackedRefPtr, ReturnValue)
{
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

TEST(WTF_PackedRefPtr, Const)
{
    // This test passes if it compiles without an error.
    auto a = ConstRefCounted::create();
    PackedRef<const ConstRefCounted> b = WTFMove(a);
    PackedRefPtr<const ConstRefCounted> c = b.ptr();
    PackedRef<const ConstRefCounted> d = returnConstRefCountedRef();
    PackedRefPtr<const ConstRefCounted> e = &returnConstRefCountedRef();
    PackedRefPtr<ConstRefCounted> f = ConstRefCounted::create();
    PackedRefPtr<const ConstRefCounted> g = f;
    PackedRefPtr<const ConstRefCounted> h(f);
    PackedRef<const ConstRefCounted> i(returnRefCountedRef());
}

struct PackedRefPtrCheckingRefLogger : RefLogger {
    PackedRefPtrCheckingRefLogger(const char* name);
    void ref();
    void deref();
    const PackedRefPtr<PackedRefPtrCheckingRefLogger>* slotToCheck { nullptr };
};

PackedRefPtrCheckingRefLogger::PackedRefPtrCheckingRefLogger(const char* name)
    : RefLogger { name }
{
}

static const char* loggerName(const PackedRefPtr<PackedRefPtrCheckingRefLogger>& pointer)
{
    return pointer ? &pointer->name : "null";
}

void PackedRefPtrCheckingRefLogger::ref()
{
    if (slotToCheck)
        log() << "slot=" << loggerName(*slotToCheck) << " ";
    RefLogger::ref();
}

void PackedRefPtrCheckingRefLogger::deref()
{
    if (slotToCheck)
        log() << "slot=" << loggerName(*slotToCheck) << " ";
    RefLogger::deref();
}

TEST(WTF_PackedRefPtr, AssignBeforeDeref)
{
    PackedRefPtrCheckingRefLogger a("a");
    PackedRefPtrCheckingRefLogger b("b");

    {
        PackedRefPtr<PackedRefPtrCheckingRefLogger> p1(&a);
        PackedRefPtr<PackedRefPtrCheckingRefLogger> p2(&b);
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
        PackedRefPtr<PackedRefPtrCheckingRefLogger> ptr(&a);
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
        PackedRefPtr<PackedRefPtrCheckingRefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        a.slotToCheck = &ptr;
        ptr = nullptr;
        a.slotToCheck = nullptr;
        EXPECT_EQ(nullptr, ptr.get());
    }
    EXPECT_STREQ("ref(a) slot=null deref(a) ", takeLogStr().c_str());

    {
        PackedRefPtr<PackedRefPtrCheckingRefLogger> p1(&a);
        PackedRefPtr<PackedRefPtrCheckingRefLogger> p2(&b);
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

TEST(WTF_PackedRefPtr, ReleaseNonNullBeforeDeref)
{
    PackedRefPtrCheckingRefLogger a("a");

    {
        PackedRefPtr<PackedRefPtrCheckingRefLogger> refPtr = &a;
        a.slotToCheck = &refPtr;
        refPtr.releaseNonNull();
        a.slotToCheck = nullptr;
    }

    EXPECT_STREQ("ref(a) slot=null deref(a) ", takeLogStr().c_str());
}

} // namespace TestWebKitAPI

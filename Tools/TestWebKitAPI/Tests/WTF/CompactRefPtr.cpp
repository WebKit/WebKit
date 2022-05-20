/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
#include <wtf/CompactRefPtr.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RefCounted.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>

namespace TestWebKitAPI {

TEST(WTF_CompactRefPtr, Basic)
{
    DerivedRefLogger a("a");

    CompactRefPtr<RefLogger> empty;
    EXPECT_EQ(nullptr, empty.get());

    {
        CompactRefPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        EXPECT_EQ(&a, &*ptr);
        EXPECT_EQ(&a.name, &ptr->name);
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<RefLogger> ptr = &a;
        EXPECT_EQ(&a, ptr.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<RefLogger> p1 = &a;
        CompactRefPtr<RefLogger> p2(p1);
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<RefLogger> p1 = &a;
        CompactRefPtr<RefLogger> p2 = p1;
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<RefLogger> p1 = &a;
        CompactRefPtr<RefLogger> p2 = WTFMove(p1);
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<RefLogger> p1 = &a;
        CompactRefPtr<RefLogger> p2(WTFMove(p1));
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<DerivedRefLogger> p1 = &a;
        CompactRefPtr<RefLogger> p2 = p1;
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<DerivedRefLogger> p1 = &a;
        CompactRefPtr<RefLogger> p2 = WTFMove(p1);
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        ptr = nullptr;
        EXPECT_EQ(nullptr, ptr.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_CompactRefPtr, AssignPassRefToCompactRefPtr)
{
    DerivedRefLogger a("a");
    {
        Ref<RefLogger> passRef(a);
        CompactRefPtr<RefLogger> ptr = WTFMove(passRef);
        EXPECT_EQ(&a, ptr.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_CompactRefPtr, Adopt)
{
    DerivedRefLogger a("a");

    CompactRefPtr<RefLogger> empty;
    EXPECT_EQ(nullptr, empty.get());

    {
        CompactRefPtr<RefLogger> ptr(adoptRef(&a));
        EXPECT_EQ(&a, ptr.get());
        EXPECT_EQ(&a, &*ptr);
        EXPECT_EQ(&a.name, &ptr->name);
    }
    EXPECT_STREQ("deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<RefLogger> ptr = adoptRef(&a);
        EXPECT_EQ(&a, ptr.get());
    }
    EXPECT_STREQ("deref(a) ", takeLogStr().c_str());
}

TEST(WTF_CompactRefPtr, Assignment)
{
    DerivedRefLogger a("a");
    RefLogger b("b");
    DerivedRefLogger c("c");

    {
        CompactRefPtr<RefLogger> p1(&a);
        CompactRefPtr<RefLogger> p2(&b);
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
        CompactRefPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = &b;
        EXPECT_EQ(&b, ptr.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | ref(b) deref(a) | deref(b) ", takeLogStr().c_str());

    {
        CompactRefPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = adoptRef(&b);
        EXPECT_EQ(&b, ptr.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | deref(a) | deref(b) ", takeLogStr().c_str());

    {
        CompactRefPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        ptr = nullptr;
        EXPECT_EQ(nullptr, ptr.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<RefLogger> p1(&a);
        CompactRefPtr<RefLogger> p2(&b);
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
        CompactRefPtr<RefLogger> p1(&a);
        CompactRefPtr<DerivedRefLogger> p2(&c);
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
        CompactRefPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = &c;
        EXPECT_EQ(&c, ptr.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | ref(c) deref(a) | deref(c) ", takeLogStr().c_str());

    {
        CompactRefPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = adoptRef(&c);
        EXPECT_EQ(&c, ptr.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | deref(a) | deref(c) ", takeLogStr().c_str());

    {
        CompactRefPtr<RefLogger> p1(&a);
        CompactRefPtr<DerivedRefLogger> p2(&c);
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
        CompactRefPtr<RefLogger> ptr(&a);
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
        CompactRefPtr<RefLogger> ptr(&a);
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

TEST(WTF_CompactRefPtr, Swap)
{
    RefLogger a("a");
    RefLogger b("b");

    {
        CompactRefPtr<RefLogger> p1(&a);
        CompactRefPtr<RefLogger> p2(&b);
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
        CompactRefPtr<RefLogger> p1(&a);
        CompactRefPtr<RefLogger> p2(&b);
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
    RefLogger a("a");

    {
        CompactRefPtr<RefLogger> refPtr = &a;
        CompactRefPtr<RefLogger> ref = refPtr.releaseNonNull();
    }

    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_CompactRefPtr, Release)
{
    DerivedRefLogger a("a");
    RefLogger b("b");
    DerivedRefLogger c("c");

    {
        CompactRefPtr<RefLogger> p1 = &a;
        CompactRefPtr<RefLogger> p2 = WTFMove(p1);
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<RefLogger> p1 = &a;
        CompactRefPtr<RefLogger> p2(WTFMove(p1));
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<DerivedRefLogger> p1 = &a;
        CompactRefPtr<RefLogger> p2 = WTFMove(p1);
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<RefLogger> p1(&a);
        CompactRefPtr<RefLogger> p2(&b);
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
        CompactRefPtr<RefLogger> p1(&a);
        CompactRefPtr<DerivedRefLogger> p2(&c);
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

static CompactRefPtr<RefLogger> f1(RefLogger& logger)
{
    return CompactRefPtr<RefLogger>(&logger);
}

TEST(WTF_CompactRefPtr, ReturnValue)
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

struct CompactRefPtrCheckingRefLogger : RefLogger {
    CompactRefPtrCheckingRefLogger(const char* name);
    void ref();
    void deref();
    const CompactRefPtr<CompactRefPtrCheckingRefLogger>* slotToCheck { nullptr };
};

CompactRefPtrCheckingRefLogger::CompactRefPtrCheckingRefLogger(const char* name)
    : RefLogger { name }
{
}

static const char* loggerName(const CompactRefPtr<CompactRefPtrCheckingRefLogger>& pointer)
{
    return pointer ? &pointer->name : "null";
}

void CompactRefPtrCheckingRefLogger::ref()
{
    if (slotToCheck)
        log() << "slot=" << loggerName(*slotToCheck) << " ";
    RefLogger::ref();
}

void CompactRefPtrCheckingRefLogger::deref()
{
    if (slotToCheck)
        log() << "slot=" << loggerName(*slotToCheck) << " ";
    RefLogger::deref();
}

TEST(WTF_CompactRefPtr, AssignBeforeDeref)
{
    CompactRefPtrCheckingRefLogger a("a");
    CompactRefPtrCheckingRefLogger b("b");

    {
        CompactRefPtr<CompactRefPtrCheckingRefLogger> p1(&a);
        CompactRefPtr<CompactRefPtrCheckingRefLogger> p2(&b);
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
        CompactRefPtr<CompactRefPtrCheckingRefLogger> ptr(&a);
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
        CompactRefPtr<CompactRefPtrCheckingRefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        a.slotToCheck = &ptr;
        ptr = nullptr;
        a.slotToCheck = nullptr;
        EXPECT_EQ(nullptr, ptr.get());
    }
    EXPECT_STREQ("ref(a) slot=null deref(a) ", takeLogStr().c_str());

    {
        CompactRefPtr<CompactRefPtrCheckingRefLogger> p1(&a);
        CompactRefPtr<CompactRefPtrCheckingRefLogger> p2(&b);
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
    CompactRefPtrCheckingRefLogger a("a");

    {
        CompactRefPtr<CompactRefPtrCheckingRefLogger> refPtr = &a;
        a.slotToCheck = &refPtr;
        refPtr.releaseNonNull();
        a.slotToCheck = nullptr;
    }

    EXPECT_STREQ("ref(a) slot=null deref(a) ", takeLogStr().c_str());
}

// FIXME: Enable these tests once Win platform supports TestWebKitAPI::Util::run
#if !PLATFORM(WIN)

static bool done;
static bool isDestroyedInMainThread;
struct ThreadSafeRefCountedObject : ThreadSafeRefCounted<ThreadSafeRefCountedObject> {
    static Ref<ThreadSafeRefCountedObject> create() { return adoptRef(*new ThreadSafeRefCountedObject); }

    ~ThreadSafeRefCountedObject()
    {
        isDestroyedInMainThread = isMainThread();
        done = true;
    }
};

struct MainThreadSafeRefCountedObject : ThreadSafeRefCounted<MainThreadSafeRefCountedObject, WTF::DestructionThread::Main> {
    static Ref<MainThreadSafeRefCountedObject> create() { return adoptRef(*new MainThreadSafeRefCountedObject); }

    ~MainThreadSafeRefCountedObject()
    {
        isDestroyedInMainThread = isMainThread();
        done = true;
    }
};

TEST(WTF_CompactRefPtr, ReleaseInNonMainThread)
{
    done = false;
    Thread::create("", [object = ThreadSafeRefCountedObject::create()] { });
    TestWebKitAPI::Util::run(&done);

    EXPECT_FALSE(isDestroyedInMainThread);
}

TEST(WTF_CompactRefPtr, ReleaseInNonMainThreadDestroyInMainThread)
{
    WTF::initializeMainThread();
    done = false;
    Thread::create("", [object = MainThreadSafeRefCountedObject::create()] { });
    TestWebKitAPI::Util::run(&done);

    EXPECT_TRUE(isDestroyedInMainThread);
}

#endif

} // namespace TestWebKitAPI

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
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>

namespace TestWebKitAPI {

TEST(WTF_RefPtr, Basic)
{
    DerivedRefLogger a("a");

    RefPtr<RefLogger> empty;
    EXPECT_EQ(nullptr, empty.get());

    {
        RefPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        EXPECT_EQ(&a, &*ptr);
        EXPECT_EQ(&a.name, &ptr->name);
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> ptr = &a;
        EXPECT_EQ(&a, ptr.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> p1 = &a;
        RefPtr<RefLogger> p2(p1);
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> p1 = &a;
        RefPtr<RefLogger> p2 = p1;
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> p1 = &a;
        RefPtr<RefLogger> p2 = WTFMove(p1);
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> p1 = &a;
        RefPtr<RefLogger> p2(WTFMove(p1));
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<DerivedRefLogger> p1 = &a;
        RefPtr<RefLogger> p2 = p1;
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<DerivedRefLogger> p1 = &a;
        RefPtr<RefLogger> p2 = WTFMove(p1);
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        ptr = nullptr;
        EXPECT_EQ(nullptr, ptr.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_RefPtr, AssignPassRefToRefPtr)
{
    DerivedRefLogger a("a");
    {
        Ref<RefLogger> passRef(a);
        RefPtr<RefLogger> ptr = WTFMove(passRef);
        EXPECT_EQ(&a, ptr.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_RefPtr, Adopt)
{
    DerivedRefLogger a("a");

    RefPtr<RefLogger> empty;
    EXPECT_EQ(nullptr, empty.get());

    {
        RefPtr<RefLogger> ptr(adoptRef(&a));
        EXPECT_EQ(&a, ptr.get());
        EXPECT_EQ(&a, &*ptr);
        EXPECT_EQ(&a.name, &ptr->name);
    }
    EXPECT_STREQ("deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> ptr = adoptRef(&a);
        EXPECT_EQ(&a, ptr.get());
    }
    EXPECT_STREQ("deref(a) ", takeLogStr().c_str());
}

TEST(WTF_RefPtr, Assignment)
{
    DerivedRefLogger a("a");
    RefLogger b("b");
    DerivedRefLogger c("c");

    {
        RefPtr<RefLogger> p1(&a);
        RefPtr<RefLogger> p2(&b);
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
        RefPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = &b;
        EXPECT_EQ(&b, ptr.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | ref(b) deref(a) | deref(b) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = adoptRef(&b);
        EXPECT_EQ(&b, ptr.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | deref(a) | deref(b) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        ptr = nullptr;
        EXPECT_EQ(nullptr, ptr.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> p1(&a);
        RefPtr<RefLogger> p2(&b);
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
        RefPtr<RefLogger> p1(&a);
        RefPtr<DerivedRefLogger> p2(&c);
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
        RefPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = &c;
        EXPECT_EQ(&c, ptr.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | ref(c) deref(a) | deref(c) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = adoptRef(&c);
        EXPECT_EQ(&c, ptr.get());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | deref(a) | deref(c) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> p1(&a);
        RefPtr<DerivedRefLogger> p2(&c);
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
        RefPtr<RefLogger> ptr(&a);
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
        RefPtr<RefLogger> ptr(&a);
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

TEST(WTF_RefPtr, Swap)
{
    RefLogger a("a");
    RefLogger b("b");

    {
        RefPtr<RefLogger> p1(&a);
        RefPtr<RefLogger> p2(&b);
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
        RefPtr<RefLogger> p1(&a);
        RefPtr<RefLogger> p2(&b);
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

TEST(WTF_RefPtr, ReleaseNonNull)
{
    RefLogger a("a");

    {
        RefPtr<RefLogger> refPtr = &a;
        RefPtr<RefLogger> ref = refPtr.releaseNonNull();
    }

    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_RefPtr, Release)
{
    DerivedRefLogger a("a");
    RefLogger b("b");
    DerivedRefLogger c("c");

    {
        RefPtr<RefLogger> p1 = &a;
        RefPtr<RefLogger> p2 = WTFMove(p1);
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> p1 = &a;
        RefPtr<RefLogger> p2(WTFMove(p1));
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<DerivedRefLogger> p1 = &a;
        RefPtr<RefLogger> p2 = WTFMove(p1);
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> p1(&a);
        RefPtr<RefLogger> p2(&b);
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
        RefPtr<RefLogger> p1(&a);
        RefPtr<DerivedRefLogger> p2(&c);
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
    return RefPtr<RefLogger>(&logger);
}

TEST(WTF_RefPtr, ReturnValue)
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

TEST(WTF_RefPtr, Const)
{
    // This test passes if it compiles without an error.
    auto a = ConstRefCounted::create();
    Ref<const ConstRefCounted> b = WTFMove(a);
    RefPtr<const ConstRefCounted> c = b.ptr();
    Ref<const ConstRefCounted> d = returnConstRefCountedRef();
    RefPtr<const ConstRefCounted> e = &returnConstRefCountedRef();
    RefPtr<ConstRefCounted> f = ConstRefCounted::create();
    RefPtr<const ConstRefCounted> g = f;
    RefPtr<const ConstRefCounted> h(f);
    Ref<const ConstRefCounted> i(returnRefCountedRef());
}

struct RefPtrCheckingRefLogger : RefLogger {
    RefPtrCheckingRefLogger(const char* name);
    void ref();
    void deref();
    const RefPtr<RefPtrCheckingRefLogger>* slotToCheck { nullptr };
};

RefPtrCheckingRefLogger::RefPtrCheckingRefLogger(const char* name)
    : RefLogger { name }
{
}

static const char* loggerName(const RefPtr<RefPtrCheckingRefLogger>& pointer)
{
    return pointer ? &pointer->name : "null";
}

void RefPtrCheckingRefLogger::ref()
{
    if (slotToCheck)
        log() << "slot=" << loggerName(*slotToCheck) << " ";
    RefLogger::ref();
}

void RefPtrCheckingRefLogger::deref()
{
    if (slotToCheck)
        log() << "slot=" << loggerName(*slotToCheck) << " ";
    RefLogger::deref();
}

TEST(WTF_RefPtr, AssignBeforeDeref)
{
    RefPtrCheckingRefLogger a("a");
    RefPtrCheckingRefLogger b("b");

    {
        RefPtr<RefPtrCheckingRefLogger> p1(&a);
        RefPtr<RefPtrCheckingRefLogger> p2(&b);
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
        RefPtr<RefPtrCheckingRefLogger> ptr(&a);
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
        RefPtr<RefPtrCheckingRefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        a.slotToCheck = &ptr;
        ptr = nullptr;
        a.slotToCheck = nullptr;
        EXPECT_EQ(nullptr, ptr.get());
    }
    EXPECT_STREQ("ref(a) slot=null deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefPtrCheckingRefLogger> p1(&a);
        RefPtr<RefPtrCheckingRefLogger> p2(&b);
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

TEST(WTF_RefPtr, ReleaseNonNullBeforeDeref)
{
    RefPtrCheckingRefLogger a("a");

    {
        RefPtr<RefPtrCheckingRefLogger> refPtr = &a;
        a.slotToCheck = &refPtr;
        refPtr.releaseNonNull();
        a.slotToCheck = nullptr;
    }

    EXPECT_STREQ("ref(a) slot=null deref(a) ", takeLogStr().c_str());
}

// FIXME: Enable these tests once Win platform supports TestWebKitAPI::Util::run
#if! PLATFORM(WIN)

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

TEST(WTF_RefPtr, ReleaseInNonMainThread)
{
    done = false;
    Thread::create("", [object = ThreadSafeRefCountedObject::create()] { });
    TestWebKitAPI::Util::run(&done);

    EXPECT_FALSE(isDestroyedInMainThread);
}

TEST(WTF_RefPtr, ReleaseInNonMainThreadDestroyInMainThread)
{
    RunLoop::initializeMain();
    done = false;
    Thread::create("", [object = MainThreadSafeRefCountedObject::create()] { });
    TestWebKitAPI::Util::run(&done);

    EXPECT_TRUE(isDestroyedInMainThread);
}

#endif

} // namespace TestWebKitAPI

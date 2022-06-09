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
#include <wtf/RefPtr.h>

namespace TestWebKitAPI {

TEST(WTF_Ref, Basic)
{
    DerivedRefLogger a("a");

    {
        Ref<RefLogger> ref(a);
        EXPECT_EQ(&a, ref.ptr());
        EXPECT_EQ(&a.name, &ref->name);
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        Ref<RefLogger> ref(adoptRef(a));
        EXPECT_EQ(&a, ref.ptr());
        EXPECT_EQ(&a.name, &ref->name);
    }
    EXPECT_STREQ("deref(a) ", takeLogStr().c_str());
}

TEST(WTF_Ref, Assignment)
{
    DerivedRefLogger a("a");
    RefLogger b("b");
    DerivedRefLogger c("c");

    {
        Ref<RefLogger> ref(a);
        EXPECT_EQ(&a, ref.ptr());
        log() << "| ";
        ref = b;
        EXPECT_EQ(&b, ref.ptr());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | ref(b) deref(a) | deref(b) ", takeLogStr().c_str());

    {
        Ref<RefLogger> ref(a);
        EXPECT_EQ(&a, ref.ptr());
        log() << "| ";
        ref = c;
        EXPECT_EQ(&c, ref.ptr());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | ref(c) deref(a) | deref(c) ", takeLogStr().c_str());

    {
        Ref<RefLogger> ref(a);
        EXPECT_EQ(&a, ref.ptr());
        log() << "| ";
        ref = adoptRef(b);
        EXPECT_EQ(&b, ref.ptr());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | deref(a) | deref(b) ", takeLogStr().c_str());

    {
        Ref<RefLogger> ref(a);
        EXPECT_EQ(&a, ref.ptr());
        log() << "| ";
        ref = adoptRef(c);
        EXPECT_EQ(&c, ref.ptr());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | deref(a) | deref(c) ", takeLogStr().c_str());
}

static Ref<RefLogger> passWithRef(Ref<RefLogger>&& reference)
{
    return WTFMove(reference);
}

TEST(WTF_Ref, ReturnValue)
{
    DerivedRefLogger a("a");
    RefLogger b("b");
    DerivedRefLogger c("c");

    {
        Ref<RefLogger> ref(passWithRef(Ref<RefLogger>(a)));
        EXPECT_EQ(&a, ref.ptr());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        Ref<RefLogger> ref(a);
        EXPECT_EQ(&a, ref.ptr());
        log() << "| ";
        ref = passWithRef(b);
        EXPECT_EQ(&b, ref.ptr());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | ref(b) deref(a) | deref(b) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> ptr(passWithRef(a));
        EXPECT_EQ(&a, ptr.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<DerivedRefLogger> ptr(&a);
        RefPtr<RefLogger> ptr2(WTFMove(ptr));
        EXPECT_EQ(nullptr, ptr.get());
        EXPECT_EQ(&a, ptr2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        Ref<DerivedRefLogger> derivedReference(a);
        Ref<RefLogger> baseReference(passWithRef(derivedReference.copyRef()));
        EXPECT_EQ(&a, derivedReference.ptr());
        EXPECT_EQ(&a, baseReference.ptr());
    }
    EXPECT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_Ref, Swap)
{
    RefLogger a("a");
    RefLogger b("b");

    {
        Ref<RefLogger> p1(a);
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

    {
        Ref<RefLogger> p1(a);
        Ref<RefLogger> p2(b);
        log() << "| ";
        EXPECT_EQ(&a, p1.ptr());
        EXPECT_EQ(&b, p2.ptr());
        std::swap(p1, p2);
        EXPECT_EQ(&b, p1.ptr());
        EXPECT_EQ(&a, p2.ptr());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) ref(b) | | deref(a) deref(b) ", takeLogStr().c_str());
}

struct RefCheckingRefLogger : RefLogger {
    RefCheckingRefLogger(const char* name);
    void ref();
    void deref();
    const Ref<RefCheckingRefLogger>* slotToCheck { nullptr };
};

struct DerivedRefCheckingRefLogger : RefCheckingRefLogger {
    DerivedRefCheckingRefLogger(const char* name);
};

RefCheckingRefLogger::RefCheckingRefLogger(const char* name)
    : RefLogger { name }
{
}

void RefCheckingRefLogger::ref()
{
    if (slotToCheck)
        log() << "slot=" << slotToCheck->get().name << " ";
    RefLogger::ref();
}

void RefCheckingRefLogger::deref()
{
    if (slotToCheck)
        log() << "slot=" << slotToCheck->get().name << " ";
    RefLogger::deref();
}

DerivedRefCheckingRefLogger::DerivedRefCheckingRefLogger(const char* name)
    : RefCheckingRefLogger { name }
{
}

TEST(WTF_Ref, AssignBeforeDeref)
{
    DerivedRefCheckingRefLogger a("a");
    RefCheckingRefLogger b("b");
    DerivedRefCheckingRefLogger c("c");

    {
        Ref<RefCheckingRefLogger> ref(a);
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
        Ref<RefCheckingRefLogger> ref(a);
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
        Ref<RefCheckingRefLogger> ref(a);
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
        Ref<RefCheckingRefLogger> ref(a);
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

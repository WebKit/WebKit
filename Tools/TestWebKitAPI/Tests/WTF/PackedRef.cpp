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
#include <wtf/PackedRef.h>
#include <wtf/PackedRefPtr.h>

namespace TestWebKitAPI {

TEST(WTF_PackedRef, Basic)
{
    DerivedRefLogger a("a");

    {
        PackedRef<RefLogger> ref(a);
        EXPECT_EQ(&a, ref.ptr());
        EXPECT_EQ(&a.name, &ref->name);
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PackedRef<RefLogger> ref(adoptRef(a));
        EXPECT_EQ(&a, ref.ptr());
        EXPECT_EQ(&a.name, &ref->name);
    }
    EXPECT_STREQ("deref(a) ", takeLogStr().c_str());
}

TEST(WTF_PackedRef, Assignment)
{
    DerivedRefLogger a("a");
    RefLogger b("b");
    DerivedRefLogger c("c");

    {
        PackedRef<RefLogger> ref(a);
        EXPECT_EQ(&a, ref.ptr());
        log() << "| ";
        ref = b;
        EXPECT_EQ(&b, ref.ptr());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | ref(b) deref(a) | deref(b) ", takeLogStr().c_str());

    {
        PackedRef<RefLogger> ref(a);
        EXPECT_EQ(&a, ref.ptr());
        log() << "| ";
        ref = c;
        EXPECT_EQ(&c, ref.ptr());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | ref(c) deref(a) | deref(c) ", takeLogStr().c_str());

    {
        PackedRef<RefLogger> ref(a);
        EXPECT_EQ(&a, ref.ptr());
        log() << "| ";
        ref = adoptRef(b);
        EXPECT_EQ(&b, ref.ptr());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | deref(a) | deref(b) ", takeLogStr().c_str());

    {
        PackedRef<RefLogger> ref(a);
        EXPECT_EQ(&a, ref.ptr());
        log() << "| ";
        ref = adoptRef(c);
        EXPECT_EQ(&c, ref.ptr());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | deref(a) | deref(c) ", takeLogStr().c_str());
}

static PackedRef<RefLogger> passWithRef(PackedRef<RefLogger>&& reference)
{
    return WTFMove(reference);
}

TEST(WTF_PackedRef, ReturnValue)
{
    DerivedRefLogger a("a");
    RefLogger b("b");
    DerivedRefLogger c("c");

    {
        PackedRef<RefLogger> ref(passWithRef(PackedRef<RefLogger>(a)));
        EXPECT_EQ(&a, ref.ptr());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PackedRef<RefLogger> ref(a);
        EXPECT_EQ(&a, ref.ptr());
        log() << "| ";
        ref = passWithRef(b);
        EXPECT_EQ(&b, ref.ptr());
        log() << "| ";
    }
    EXPECT_STREQ("ref(a) | ref(b) deref(a) | deref(b) ", takeLogStr().c_str());

    {
        PackedRefPtr<RefLogger> ptr(passWithRef(a));
        EXPECT_EQ(&a, ptr.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PackedRefPtr<DerivedRefLogger> ptr(&a);
        PackedRefPtr<RefLogger> ptr2(WTFMove(ptr));
        EXPECT_EQ(nullptr, ptr.get());
        EXPECT_EQ(&a, ptr2.get());
    }
    EXPECT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        PackedRef<DerivedRefLogger> derivedReference(a);
        PackedRef<RefLogger> baseReference(passWithRef(derivedReference.copyRef()));
        EXPECT_EQ(&a, derivedReference.ptr());
        EXPECT_EQ(&a, baseReference.ptr());
    }
    EXPECT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_PackedRef, Swap)
{
    RefLogger a("a");
    RefLogger b("b");

    {
        PackedRef<RefLogger> p1(a);
        PackedRef<RefLogger> p2(b);
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
        PackedRef<RefLogger> p1(a);
        PackedRef<RefLogger> p2(b);
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

struct PackedRefCheckingRefLogger : RefLogger {
    PackedRefCheckingRefLogger(const char* name);
    void ref();
    void deref();
    const PackedRef<PackedRefCheckingRefLogger>* slotToCheck { nullptr };
};

struct DerivedPackedRefCheckingRefLogger : PackedRefCheckingRefLogger {
    DerivedPackedRefCheckingRefLogger(const char* name);
};

PackedRefCheckingRefLogger::PackedRefCheckingRefLogger(const char* name)
    : RefLogger { name }
{
}

void PackedRefCheckingRefLogger::ref()
{
    if (slotToCheck)
        log() << "slot=" << slotToCheck->get().name << " ";
    RefLogger::ref();
}

void PackedRefCheckingRefLogger::deref()
{
    if (slotToCheck)
        log() << "slot=" << slotToCheck->get().name << " ";
    RefLogger::deref();
}

DerivedPackedRefCheckingRefLogger::DerivedPackedRefCheckingRefLogger(const char* name)
    : PackedRefCheckingRefLogger { name }
{
}

TEST(WTF_PackedRef, AssignBeforeDeref)
{
    DerivedPackedRefCheckingRefLogger a("a");
    PackedRefCheckingRefLogger b("b");
    DerivedPackedRefCheckingRefLogger c("c");

    {
        PackedRef<PackedRefCheckingRefLogger> ref(a);
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
        PackedRef<PackedRefCheckingRefLogger> ref(a);
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
        PackedRef<PackedRefCheckingRefLogger> ref(a);
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
        PackedRef<PackedRefCheckingRefLogger> ref(a);
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

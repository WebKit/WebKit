/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

TEST(WTF_RefPtr, Basic)
{
    DerivedRefLogger a("a");

    RefPtr<RefLogger> empty;
    ASSERT_EQ(nullptr, empty.get());

    {
        RefPtr<RefLogger> ptr(&a);
        ASSERT_EQ(&a, ptr.get());
        ASSERT_EQ(&a, &*ptr);
        ASSERT_EQ(&a.name, &ptr->name);
    }
    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> ptr = &a;
        ASSERT_EQ(&a, ptr.get());
    }
    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> p1 = &a;
        RefPtr<RefLogger> p2(p1);
        ASSERT_EQ(&a, p1.get());
        ASSERT_EQ(&a, p2.get());
    }
    ASSERT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> p1 = &a;
        RefPtr<RefLogger> p2 = p1;
        ASSERT_EQ(&a, p1.get());
        ASSERT_EQ(&a, p2.get());
    }
    ASSERT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> p1 = &a;
        RefPtr<RefLogger> p2 = std::move(p1);
        ASSERT_EQ(nullptr, p1.get());
        ASSERT_EQ(&a, p2.get());
    }
    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> p1 = &a;
        RefPtr<RefLogger> p2(std::move(p1));
        ASSERT_EQ(nullptr, p1.get());
        ASSERT_EQ(&a, p2.get());
    }
    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<DerivedRefLogger> p1 = &a;
        RefPtr<RefLogger> p2 = p1;
        ASSERT_EQ(&a, p1.get());
        ASSERT_EQ(&a, p2.get());
    }
    ASSERT_STREQ("ref(a) ref(a) deref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<DerivedRefLogger> p1 = &a;
        RefPtr<RefLogger> p2 = std::move(p1);
        ASSERT_EQ(nullptr, p1.get());
        ASSERT_EQ(&a, p2.get());
    }
    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> ptr(&a);
        ASSERT_EQ(&a, ptr.get());
        ptr.clear();
        ASSERT_EQ(nullptr, ptr.get());
    }
    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> ptr(&a);
        ASSERT_EQ(&a, ptr.get());
        ptr.release();
        ASSERT_EQ(nullptr, ptr.get());
    }
    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_RefPtr, AssignPassRefToRefPtr)
{
    DerivedRefLogger a("a");
    {
        PassRef<RefLogger> passRef(a);
        RefPtr<RefLogger> ptr = std::move(passRef);
        ASSERT_EQ(&a, ptr.get());
        ptr.release();
        ASSERT_EQ(nullptr, ptr.get());
    }
    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_RefPtr, Adopt)
{
    DerivedRefLogger a("a");

    RefPtr<RefLogger> empty;
    ASSERT_EQ(nullptr, empty.get());

    {
        RefPtr<RefLogger> ptr(adoptRef(&a));
        ASSERT_EQ(&a, ptr.get());
        ASSERT_EQ(&a, &*ptr);
        ASSERT_EQ(&a.name, &ptr->name);
    }
    ASSERT_STREQ("deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> ptr = adoptRef(&a);
        ASSERT_EQ(&a, ptr.get());
    }
    ASSERT_STREQ("deref(a) ", takeLogStr().c_str());
}

TEST(WTF_RefPtr, Assignment)
{
    DerivedRefLogger a("a");
    RefLogger b("b");
    DerivedRefLogger c("c");

    {
        RefPtr<RefLogger> p1(&a);
        RefPtr<RefLogger> p2(&b);
        ASSERT_EQ(&a, p1.get());
        ASSERT_EQ(&b, p2.get());
        log() << "| ";
        p1 = p2;
        ASSERT_EQ(&b, p1.get());
        ASSERT_EQ(&b, p2.get());
        log() << "| ";
    }
    ASSERT_STREQ("ref(a) ref(b) | ref(b) deref(a) | deref(b) deref(b) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> ptr(&a);
        ASSERT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = &b;
        ASSERT_EQ(&b, ptr.get());
        log() << "| ";
    }
    ASSERT_STREQ("ref(a) | ref(b) deref(a) | deref(b) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> ptr(&a);
        ASSERT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = adoptRef(&b);
        ASSERT_EQ(&b, ptr.get());
        log() << "| ";
    }
    ASSERT_STREQ("ref(a) | deref(a) | deref(b) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> ptr(&a);
        ASSERT_EQ(&a, ptr.get());
        ptr = nullptr;
        ASSERT_EQ(nullptr, ptr.get());
    }
    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> p1(&a);
        RefPtr<RefLogger> p2(&b);
        ASSERT_EQ(&a, p1.get());
        ASSERT_EQ(&b, p2.get());
        log() << "| ";
        p1 = std::move(p2);
        ASSERT_EQ(&b, p1.get());
        ASSERT_EQ(nullptr, p2.get());
        log() << "| ";
    }
    ASSERT_STREQ("ref(a) ref(b) | deref(a) | deref(b) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> p1(&a);
        RefPtr<DerivedRefLogger> p2(&c);
        ASSERT_EQ(&a, p1.get());
        ASSERT_EQ(&c, p2.get());
        log() << "| ";
        p1 = p2;
        ASSERT_EQ(&c, p1.get());
        ASSERT_EQ(&c, p2.get());
        log() << "| ";
    }
    ASSERT_STREQ("ref(a) ref(c) | ref(c) deref(a) | deref(c) deref(c) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> ptr(&a);
        ASSERT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = &c;
        ASSERT_EQ(&c, ptr.get());
        log() << "| ";
    }
    ASSERT_STREQ("ref(a) | ref(c) deref(a) | deref(c) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> ptr(&a);
        ASSERT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = adoptRef(&c);
        ASSERT_EQ(&c, ptr.get());
        log() << "| ";
    }
    ASSERT_STREQ("ref(a) | deref(a) | deref(c) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> p1(&a);
        RefPtr<DerivedRefLogger> p2(&c);
        ASSERT_EQ(&a, p1.get());
        ASSERT_EQ(&c, p2.get());
        log() << "| ";
        p1 = std::move(p2);
        ASSERT_EQ(&c, p1.get());
        ASSERT_EQ(nullptr, p2.get());
        log() << "| ";
    }
    ASSERT_STREQ("ref(a) ref(c) | deref(a) | deref(c) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> ptr(&a);
        ASSERT_EQ(&a, ptr.get());
        log() << "| ";
        ptr = ptr;
        ASSERT_EQ(&a, ptr.get());
        log() << "| ";
    }
    ASSERT_STREQ("ref(a) | ref(a) deref(a) | deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> ptr(&a);
        ASSERT_EQ(&a, ptr.get());
        ptr = std::move(ptr);
        ASSERT_EQ(&a, ptr.get());
    }
    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_RefPtr, Swap)
{
    RefLogger a("a");
    RefLogger b("b");

    {
        RefPtr<RefLogger> p1(&a);
        RefPtr<RefLogger> p2(&b);
        ASSERT_EQ(&a, p1.get());
        ASSERT_EQ(&b, p2.get());
        p1.swap(p2);
        ASSERT_EQ(&b, p1.get());
        ASSERT_EQ(&a, p2.get());
        log() << "| ";
    }
    ASSERT_STREQ("ref(a) ref(b) | deref(a) deref(b) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> p1(&a);
        RefPtr<RefLogger> p2(&b);
        ASSERT_EQ(&a, p1.get());
        ASSERT_EQ(&b, p2.get());
        std::swap(p1, p2);
        ASSERT_EQ(&b, p1.get());
        ASSERT_EQ(&a, p2.get());
        log() << "| ";
    }
    ASSERT_STREQ("ref(a) ref(b) | deref(a) deref(b) ", takeLogStr().c_str());
}

TEST(WTF_RefPtr, ReleaseNonNull)
{
    RefLogger a("a");

    {
        RefPtr<RefLogger> refPtr = &a;
        RefPtr<RefLogger> ref = refPtr.releaseNonNull();
    }

    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

TEST(WTF_RefPtr, Release)
{
    DerivedRefLogger a("a");
    RefLogger b("b");
    DerivedRefLogger c("c");

    {
        RefPtr<RefLogger> p1 = &a;
        RefPtr<RefLogger> p2 = p1.release();
        ASSERT_EQ(nullptr, p1.get());
        ASSERT_EQ(&a, p2.get());
    }
    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> p1 = &a;
        RefPtr<RefLogger> p2(p1.release());
        ASSERT_EQ(nullptr, p1.get());
        ASSERT_EQ(&a, p2.get());
    }
    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<DerivedRefLogger> p1 = &a;
        RefPtr<RefLogger> p2 = p1.release();
        ASSERT_EQ(nullptr, p1.get());
        ASSERT_EQ(&a, p2.get());
    }
    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> p1(&a);
        RefPtr<RefLogger> p2(&b);
        ASSERT_EQ(&a, p1.get());
        ASSERT_EQ(&b, p2.get());
        log() << "| ";
        p1 = p2.release();
        ASSERT_EQ(&b, p1.get());
        ASSERT_EQ(nullptr, p2.get());
        log() << "| ";
    }
    ASSERT_STREQ("ref(a) ref(b) | deref(a) | deref(b) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> p1(&a);
        RefPtr<DerivedRefLogger> p2(&c);
        ASSERT_EQ(&a, p1.get());
        ASSERT_EQ(&c, p2.get());
        log() << "| ";
        p1 = p2.release();
        ASSERT_EQ(&c, p1.get());
        ASSERT_EQ(nullptr, p2.get());
        log() << "| ";
    }
    ASSERT_STREQ("ref(a) ref(c) | deref(a) | deref(c) ", takeLogStr().c_str());
}

RefPtr<RefLogger> f1(RefLogger& logger)
{
    return RefPtr<RefLogger>(&logger);
}

TEST(WTF_RefPtr, ReturnValue)
{
    DerivedRefLogger a("a");

    {
        f1(a);
    }
    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        auto ptr = f1(a);
    }
    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

} // namespace TestWebKitAPI

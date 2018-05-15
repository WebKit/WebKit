/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include <wtf/NakedPtr.h>

namespace TestWebKitAPI {

// For these tests, we need a base class and a derived class. For this purpose,
// we reuse the RefLogger and DerivedRefLogger classes.

TEST(WTF_NakedPtr, Basic)
{
    DerivedRefLogger a("a");

    NakedPtr<RefLogger> empty;
    ASSERT_EQ(nullptr, empty.get());

    {
        NakedPtr<RefLogger> ptr(&a);
        ASSERT_EQ(&a, ptr.get());
        ASSERT_EQ(&a, &*ptr);
        ASSERT_EQ(&a.name, &ptr->name);
    }

    {
        NakedPtr<RefLogger> ptr = &a;
        ASSERT_EQ(&a, ptr.get());
    }

    {
        NakedPtr<RefLogger> p1 = &a;
        NakedPtr<RefLogger> p2(p1);
        ASSERT_EQ(&a, p1.get());
        ASSERT_EQ(&a, p2.get());
    }

    {
        NakedPtr<RefLogger> p1 = &a;
        NakedPtr<RefLogger> p2 = p1;
        ASSERT_EQ(&a, p1.get());
        ASSERT_EQ(&a, p2.get());
    }

    {
        NakedPtr<RefLogger> p1 = &a;
        NakedPtr<RefLogger> p2 = WTFMove(p1);
        ASSERT_EQ(&a, p1.get());
        ASSERT_EQ(&a, p2.get());
    }

    {
        NakedPtr<RefLogger> p1 = &a;
        NakedPtr<RefLogger> p2(WTFMove(p1));
        ASSERT_EQ(&a, p1.get());
        ASSERT_EQ(&a, p2.get());
    }

    {
        NakedPtr<DerivedRefLogger> p1 = &a;
        NakedPtr<RefLogger> p2 = p1;
        ASSERT_EQ(&a, p1.get());
        ASSERT_EQ(&a, p2.get());
    }

    {
        NakedPtr<DerivedRefLogger> p1 = &a;
        NakedPtr<RefLogger> p2 = WTFMove(p1);
        ASSERT_EQ(&a, p1.get());
        ASSERT_EQ(&a, p2.get());
    }

    {
        NakedPtr<RefLogger> ptr(&a);
        ASSERT_EQ(&a, ptr.get());
        ptr.clear();
        ASSERT_EQ(nullptr, ptr.get());
    }
}

TEST(WTF_NakedPtr, Assignment)
{
    DerivedRefLogger a("a");
    RefLogger b("b");
    DerivedRefLogger c("c");

    {
        NakedPtr<RefLogger> p1(&a);
        NakedPtr<RefLogger> p2(&b);
        ASSERT_EQ(&a, p1.get());
        ASSERT_EQ(&b, p2.get());
        p1 = p2;
        ASSERT_EQ(&b, p1.get());
        ASSERT_EQ(&b, p2.get());
    }

    {
        NakedPtr<RefLogger> ptr(&a);
        ASSERT_EQ(&a, ptr.get());
        ptr = &b;
        ASSERT_EQ(&b, ptr.get());
    }

    {
        NakedPtr<RefLogger> ptr(&a);
        ASSERT_EQ(&a, ptr.get());
        ptr = nullptr;
        ASSERT_EQ(nullptr, ptr.get());
    }

    {
        NakedPtr<RefLogger> p1(&a);
        NakedPtr<RefLogger> p2(&b);
        ASSERT_EQ(&a, p1.get());
        ASSERT_EQ(&b, p2.get());
        p1 = WTFMove(p2);
        ASSERT_EQ(&b, p1.get());
        ASSERT_EQ(&b, p2.get());
    }

    {
        NakedPtr<RefLogger> p1(&a);
        NakedPtr<DerivedRefLogger> p2(&c);
        ASSERT_EQ(&a, p1.get());
        ASSERT_EQ(&c, p2.get());
        p1 = p2;
        ASSERT_EQ(&c, p1.get());
        ASSERT_EQ(&c, p2.get());
    }

    {
        NakedPtr<RefLogger> ptr(&a);
        ASSERT_EQ(&a, ptr.get());
        ptr = &c;
        ASSERT_EQ(&c, ptr.get());
    }

    {
        NakedPtr<RefLogger> p1(&a);
        NakedPtr<DerivedRefLogger> p2(&c);
        ASSERT_EQ(&a, p1.get());
        ASSERT_EQ(&c, p2.get());
        p1 = WTFMove(p2);
        ASSERT_EQ(&c, p1.get());
        ASSERT_EQ(&c, p2.get());
    }

    {
        NakedPtr<RefLogger> ptr(&a);
        ASSERT_EQ(&a, ptr.get());
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
        ASSERT_EQ(&a, ptr.get());
    }

    {
        NakedPtr<RefLogger> ptr(&a);
        ASSERT_EQ(&a, ptr.get());
#if COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wself-move"
#endif
        ptr = WTFMove(ptr);
#if COMPILER(CLANG)
#pragma clang diagnostic pop
#endif
        ASSERT_EQ(&a, ptr.get());
    }
}

TEST(WTF_NakedPtr, Swap)
{
    RefLogger a("a");
    RefLogger b("b");

    {
        NakedPtr<RefLogger> p1(&a);
        NakedPtr<RefLogger> p2(&b);
        ASSERT_EQ(&a, p1.get());
        ASSERT_EQ(&b, p2.get());
        p1.swap(p2);
        ASSERT_EQ(&b, p1.get());
        ASSERT_EQ(&a, p2.get());
    }

    {
        NakedPtr<RefLogger> p1(&a);
        NakedPtr<RefLogger> p2(&b);
        ASSERT_EQ(&a, p1.get());
        ASSERT_EQ(&b, p2.get());
        std::swap(p1, p2);
        ASSERT_EQ(&b, p1.get());
        ASSERT_EQ(&a, p2.get());
    }
}

NakedPtr<RefLogger> nakedPtrFoo(RefLogger& logger)
{
    return NakedPtr<RefLogger>(&logger);
}

TEST(WTF_NakedPtr, ReturnValue)
{
    DerivedRefLogger a("a");

    {
        auto ptr = nakedPtrFoo(a);
        ASSERT_EQ(&a, ptr.get());
        ASSERT_EQ(&a, &*ptr);
        ASSERT_EQ(&a.name, &ptr->name);
    }
}

} // namespace TestWebKitAPI

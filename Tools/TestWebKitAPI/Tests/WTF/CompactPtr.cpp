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

#include <wtf/CompactPtr.h>
#include "RefLogger.h"
#include "Utilities.h"
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RefCounted.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>

namespace TestWebKitAPI {

TEST(WTF_CompactPtr, Basic)
{
    DerivedRefLogger a("a");

    CompactPtr<RefLogger> empty;
    EXPECT_EQ(nullptr, empty.get());

    {
        CompactPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        EXPECT_EQ(&a, &*ptr);
        EXPECT_EQ(&a.name, &ptr->name);
    }

    {
        CompactPtr<RefLogger> ptr = &a;
        EXPECT_EQ(&a, ptr.get());
    }

    {
        CompactPtr<RefLogger> p1 = &a;
        CompactPtr<RefLogger> p2(p1);
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&a, p2.get());
    }

    {
        CompactPtr<RefLogger> p1 = &a;
        CompactPtr<RefLogger> p2 = p1;
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&a, p2.get());
    }

    {
        CompactPtr<RefLogger> p1 = &a;
        CompactPtr<RefLogger> p2 = WTFMove(p1);
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&a, p2.get());
    }

    {
        CompactPtr<RefLogger> p1 = &a;
        CompactPtr<RefLogger> p2(WTFMove(p1));
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&a, p2.get());
    }

    {
        CompactPtr<DerivedRefLogger> p1 = &a;
        CompactPtr<RefLogger> p2 = p1;
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&a, p2.get());
    }

    {
        CompactPtr<DerivedRefLogger> p1 = &a;
        CompactPtr<RefLogger> p2 = WTFMove(p1);
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&a, p2.get());
    }

    {
        CompactPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        ptr = nullptr;
        EXPECT_EQ(nullptr, ptr.get());
    }

    {
        CompactPtr<RefLogger> ptr(nullptr);
        EXPECT_EQ(nullptr, ptr.get());
    }

    {
        CompactPtr<RefLogger> ptr = nullptr;
        EXPECT_EQ(nullptr, ptr.get());
    }
}

TEST(WTF_CompactPtr, Assignment)
{
    DerivedRefLogger a("a");
    RefLogger b("b");
    DerivedRefLogger c("c");

    {
        CompactPtr<RefLogger> p1 = &a;
        CompactPtr<RefLogger> p2 = &b;
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&b, p2.get());
        p1 = p2;
        EXPECT_EQ(&b, p1.get());
        EXPECT_EQ(&b, p2.get());
    }

    {
        CompactPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        ptr = &b;
        EXPECT_EQ(&b, ptr.get());
    }

    {
        CompactPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        ptr = nullptr;
        EXPECT_EQ(nullptr, ptr.get());
    }

    {
        CompactPtr<RefLogger> p1 = &a;
        CompactPtr<RefLogger> p2 = &b;
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&b, p2.get());
        p1 = WTFMove(p2);
        EXPECT_EQ(&b, p1.get());
        EXPECT_EQ(&b, p2.get());
    }

    {
        CompactPtr<RefLogger> p1 = &a;
        CompactPtr<DerivedRefLogger> p2 = &c;
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&c, p2.get());
        p1 = p2;
        EXPECT_EQ(&c, p1.get());
        EXPECT_EQ(&c, p2.get());
    }

    {
        CompactPtr<RefLogger> p1 = &a;
        CompactPtr<DerivedRefLogger> p2 = &c;
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&c, p2.get());
        p1 = WTFMove(p2);
        EXPECT_EQ(&c, p1.get());
        EXPECT_EQ(&c, p2.get());
    }

    {
        CompactPtr<RefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        ptr = &c;
        EXPECT_EQ(&c, ptr.get());
    }
}

} // namespace TestWebKitAPI

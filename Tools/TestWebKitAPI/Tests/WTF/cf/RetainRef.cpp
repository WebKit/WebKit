/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RetainRef.h>

namespace TestWebKitAPI {

TEST(RetainRef, AdoptCFRef)
{
    RetainRef<CFArrayRef> array = adoptCFRef(CFArrayCreate(kCFAllocatorDefault, nullptr, 0, nullptr));
    EXPECT_EQ(1, CFGetRetainCount(array.get()));
    EXPECT_EQ(0, CFArrayGetCount(array.get()));
}

TEST(RetainRef, RetainRefFromValue)
{
    RetainPtr<CFArrayRef> source = adoptCF(CFArrayCreate(kCFAllocatorDefault, nullptr, 0, nullptr));
    EXPECT_EQ(1, CFGetRetainCount(source.get()));

    {
        RetainRef<CFArrayRef> ref = retainRef(source.get());
        EXPECT_EQ(2, CFGetRetainCount(source.get()));
        EXPECT_EQ(source.get(), ref.get());
    }
    EXPECT_EQ(1, CFGetRetainCount(source.get()));
}

TEST(RetainRef, CopyConstructor)
{
    RetainRef<CFArrayRef> a = adoptCFRef(CFArrayCreate(kCFAllocatorDefault, nullptr, 0, nullptr));
    EXPECT_EQ(1, CFGetRetainCount(a.get()));

    {
        RetainRef<CFArrayRef> b = a;
        EXPECT_EQ(2, CFGetRetainCount(a.get()));
        EXPECT_EQ(a.get(), b.get());
    }
    EXPECT_EQ(1, CFGetRetainCount(a.get()));
}

TEST(RetainRef, MoveConstructor)
{
    RetainRef<CFArrayRef> a = adoptCFRef(CFArrayCreate(kCFAllocatorDefault, nullptr, 0, nullptr));
    CFArrayRef raw = a.get();
    EXPECT_EQ(1, CFGetRetainCount(raw));

    RetainRef<CFArrayRef> b = WTF::move(a);
    EXPECT_EQ(1, CFGetRetainCount(raw));
    EXPECT_EQ(raw, b.get());
}

TEST(RetainRef, FromRetainPtrReleaseNonNull)
{
    RetainPtr<CFArrayRef> ptr = adoptCF(CFArrayCreate(kCFAllocatorDefault, nullptr, 0, nullptr));
    CFArrayRef raw = ptr.get();
    EXPECT_EQ(1, CFGetRetainCount(raw));

    RetainRef<CFArrayRef> ref = ptr.releaseNonNull();
    EXPECT_EQ(1, CFGetRetainCount(raw));
    EXPECT_EQ(raw, ref.get());
    EXPECT_EQ(nullptr, ptr.get());
}

TEST(RetainRef, MoveToRetainPtr)
{
    RetainRef<CFArrayRef> ref = adoptCFRef(CFArrayCreate(kCFAllocatorDefault, nullptr, 0, nullptr));
    CFArrayRef raw = ref.get();
    EXPECT_EQ(1, CFGetRetainCount(raw));

    {
        RetainPtr<CFArrayRef> ptr { WTF::move(ref) };
        EXPECT_EQ(1, CFGetRetainCount(raw));
        EXPECT_EQ(raw, ptr.get());
    }
}

TEST(RetainRef, AssignToRetainPtr)
{
    RetainPtr<CFArrayRef> ptr = adoptCF(CFArrayCreate(kCFAllocatorDefault, nullptr, 0, nullptr));
    CFArrayRef rawOriginal = ptr.get();
    EXPECT_EQ(1, CFGetRetainCount(rawOriginal));

    RetainRef<CFArrayRef> ref = adoptCFRef(CFArrayCreate(kCFAllocatorDefault, nullptr, 0, nullptr));
    CFArrayRef rawNew = ref.get();
    EXPECT_EQ(1, CFGetRetainCount(rawNew));

    ptr = WTF::move(ref);
    EXPECT_EQ(rawNew, ptr.get());
    EXPECT_EQ(1, CFGetRetainCount(rawNew));
}

TEST(RetainRef, CtadFromRetainRef)
{
    RetainRef<CFArrayRef> ref = adoptCFRef(CFArrayCreate(kCFAllocatorDefault, nullptr, 0, nullptr));
    CFArrayRef raw = ref.get();
    EXPECT_EQ(1, CFGetRetainCount(raw));

    RetainPtr ptr { WTF::move(ref) };
    static_assert(std::is_same_v<decltype(ptr), RetainPtr<CFArrayRef>>);
    EXPECT_EQ(raw, ptr.get());
    EXPECT_EQ(1, CFGetRetainCount(raw));
}

TEST(RetainRef, ExplicitFromRetainPtr)
{
    RetainPtr<CFArrayRef> ptr = adoptCF(CFArrayCreate(kCFAllocatorDefault, nullptr, 0, nullptr));
    EXPECT_EQ(1, CFGetRetainCount(ptr.get()));

    {
        RetainRef<CFArrayRef> ref { ptr.get() };
        EXPECT_EQ(2, CFGetRetainCount(ptr.get()));
        EXPECT_EQ(ptr.get(), ref.get());
    }
    EXPECT_EQ(1, CFGetRetainCount(ptr.get()));
}

TEST(RetainRef, Assignment)
{
    RetainRef<CFArrayRef> a = adoptCFRef(CFArrayCreate(kCFAllocatorDefault, nullptr, 0, nullptr));
    RetainRef<CFArrayRef> b = adoptCFRef(CFArrayCreate(kCFAllocatorDefault, nullptr, 0, nullptr));
    CFArrayRef rawB = b.get();

    a = b;
    EXPECT_EQ(rawB, a.get());
    EXPECT_EQ(2, CFGetRetainCount(rawB));
}

TEST(RetainRef, HashMap)
{
    HashMap<RetainRef<CFArrayRef>, int> map;
    RetainRef<CFArrayRef> key = adoptCFRef(CFArrayCreate(kCFAllocatorDefault, nullptr, 0, nullptr));
    map.add(key, 42);
    EXPECT_EQ(1u, map.size());
    EXPECT_EQ(42, map.get(key));
}

} // namespace TestWebKitAPI

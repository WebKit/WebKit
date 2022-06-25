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

#include "AlignedRefLogger.h"
#include "Utilities.h"
#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RefCounted.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>

namespace TestWebKitAPI {

TEST(WTF_CompactPtr, Basic)
{
    DerivedAlignedRefLogger a("a");

    CompactPtr<AlignedRefLogger> empty;
    EXPECT_EQ(nullptr, empty.get());

    {
        CompactPtr<AlignedRefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        EXPECT_EQ(&a, &*ptr);
        EXPECT_EQ(&a.name, &ptr->name);
    }

    {
        CompactPtr<AlignedRefLogger> ptr = &a;
        EXPECT_EQ(&a, ptr.get());
    }

    {
        CompactPtr<AlignedRefLogger> p1 = &a;
        CompactPtr<AlignedRefLogger> p2(p1);
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&a, p2.get());
    }

    {
        CompactPtr<AlignedRefLogger> p1 = &a;
        CompactPtr<AlignedRefLogger> p2 = p1;
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&a, p2.get());
    }

    {
        CompactPtr<AlignedRefLogger> p1 = &a;
        CompactPtr<AlignedRefLogger> p2 = WTFMove(p1);
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }

    {
        CompactPtr<AlignedRefLogger> p1 = &a;
        CompactPtr<AlignedRefLogger> p2(WTFMove(p1));
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }

    {
        CompactPtr<DerivedAlignedRefLogger> p1 = &a;
        CompactPtr<AlignedRefLogger> p2 = p1;
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&a, p2.get());
    }

    {
        CompactPtr<DerivedAlignedRefLogger> p1 = &a;
        CompactPtr<AlignedRefLogger> p2 = WTFMove(p1);
        EXPECT_EQ(nullptr, p1.get());
        EXPECT_EQ(&a, p2.get());
    }

    {
        CompactPtr<AlignedRefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        ptr = nullptr;
        EXPECT_EQ(nullptr, ptr.get());
    }

    {
        CompactPtr<AlignedRefLogger> ptr(nullptr);
        EXPECT_EQ(nullptr, ptr.get());
        EXPECT_EQ(false, static_cast<bool>(ptr));
    }

    {
        CompactPtr<AlignedRefLogger> ptr = nullptr;
        EXPECT_EQ(nullptr, ptr.get());
        EXPECT_EQ(true, !ptr);
    }

    {
        DerivedAlignedRefLogger b("b");
        DerivedAlignedRefLogger* p1 = &a;
        CompactPtr<DerivedAlignedRefLogger> p2(&b);
        p2.swap(p1);
        EXPECT_EQ(p1, &b);
        EXPECT_EQ(p2.get(), &a);
    }
}

TEST(WTF_CompactPtr, Assignment)
{
    DerivedAlignedRefLogger a("a");
    AlignedRefLogger b("b");
    DerivedAlignedRefLogger c("c");

    {
        CompactPtr<AlignedRefLogger> p1 = &a;
        CompactPtr<AlignedRefLogger> p2 = &b;
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&b, p2.get());
        p1 = p2;
        EXPECT_EQ(&b, p1.get());
        EXPECT_EQ(&b, p2.get());
    }

    {
        CompactPtr<AlignedRefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        ptr = &b;
        EXPECT_EQ(&b, ptr.get());
    }

    {
        CompactPtr<AlignedRefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        ptr = nullptr;
        EXPECT_EQ(nullptr, ptr.get());
    }

    {
        CompactPtr<AlignedRefLogger> p1 = &a;
        CompactPtr<AlignedRefLogger> p2 = &b;
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&b, p2.get());
        p1 = WTFMove(p2);
        EXPECT_EQ(&b, p1.get());
        EXPECT_EQ(nullptr, p2.get());
    }

    {
        CompactPtr<AlignedRefLogger> p1 = &a;
        CompactPtr<DerivedAlignedRefLogger> p2 = &c;
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&c, p2.get());
        p1 = p2;
        EXPECT_EQ(&c, p1.get());
        EXPECT_EQ(&c, p2.get());
    }

    {
        CompactPtr<AlignedRefLogger> p1 = &a;
        CompactPtr<DerivedAlignedRefLogger> p2 = &c;
        EXPECT_EQ(&a, p1.get());
        EXPECT_EQ(&c, p2.get());
        p1 = WTFMove(p2);
        EXPECT_EQ(&c, p1.get());
        EXPECT_EQ(nullptr, p2.get());
    }

    {
        CompactPtr<AlignedRefLogger> ptr(&a);
        EXPECT_EQ(&a, ptr.get());
        ptr = &c;
        EXPECT_EQ(&c, ptr.get());
    }
}

struct alignas(16) AlignedPackingTarget {
    unsigned m_value { 0 };
};
TEST(WTF_CompactPtr, HashMap)
{
    Vector<AlignedPackingTarget> vector;
    HashMap<PackedPtr<AlignedPackingTarget>, unsigned> map;
    vector.reserveCapacity(10000);
    for (unsigned i = 0; i < 10000; ++i)
        vector.uncheckedAppend(AlignedPackingTarget { i });

    for (auto& target : vector)
        map.add(&target, target.m_value);

    for (auto& target : vector) {
        EXPECT_TRUE(map.contains(&target));
        EXPECT_EQ(map.get(&target), target.m_value);
    }
}

TEST(WTF_CompactPtr, HashMapRemoveAndAdd)
{
    Vector<AlignedPackingTarget> vector;
    HashMap<PackedPtr<AlignedPackingTarget>, unsigned> map;
    vector.reserveCapacity(10000);
    for (unsigned i = 0; i < 10000; ++i)
        vector.uncheckedAppend(AlignedPackingTarget { i });

    for (auto& target : vector)
        map.add(&target, target.m_value);

    for (unsigned i = 0; i < 4000; ++i) {
        auto& target = vector[i];
        map.remove(&target);
    }

    for (unsigned i = 0; i < 4000; ++i) {
        auto& target = vector[i];
        map.add(&target, target.m_value);
    }

    for (auto& target : vector) {
        EXPECT_TRUE(map.contains(&target));
        EXPECT_EQ(map.get(&target), target.m_value);
    }
}

TEST(WTF_CompactPtr, StringHashSet)
{
    Vector<String> vector;
    HashSet<CompactPtr<StringImpl>> set;
    vector.reserveCapacity(10000);
    for (unsigned i = 0; i < 10000; ++i)
        vector.uncheckedAppend(String::number(i));

    for (auto& target : vector)
        set.add(target.impl());

    for (unsigned i = 0; i < 4000; ++i) {
        auto& target = vector[i];
        set.remove(target.impl());
    }

    for (unsigned i = 0; i < 4000; ++i) {
        auto& target = vector[i];
        set.add(target.impl());
    }

    for (auto& target : vector)
        set.add(target.impl());

    EXPECT_EQ(set.size(), vector.size());

    for (auto& target : vector)
        EXPECT_TRUE(set.contains(target.impl()));
}

} // namespace TestWebKitAPI

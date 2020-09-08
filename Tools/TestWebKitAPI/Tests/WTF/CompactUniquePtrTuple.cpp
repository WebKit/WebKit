/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include <wtf/CompactUniquePtrTuple.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace TestWebKitAPI {

class A {
    WTF_MAKE_FAST_ALLOCATED;

public:
    A() { ++s_constructorCallCount; }
    virtual ~A() { ++s_destructorCallCount; }

    static unsigned s_constructorCallCount;
    static unsigned s_destructorCallCount;
};

unsigned A::s_constructorCallCount = 0;
unsigned A::s_destructorCallCount = 0;

TEST(WTF_CompactUniquePtrTuple, Basic)
{
    A::s_constructorCallCount = 0;
    A::s_destructorCallCount = 0;

    CompactUniquePtrTuple<A, uint16_t> empty;
    EXPECT_EQ(0U, A::s_constructorCallCount);
    EXPECT_EQ(0U, A::s_destructorCallCount);
    EXPECT_EQ(nullptr, empty.pointer());
    EXPECT_EQ(0U, empty.type());

    CompactUniquePtrTuple<A, uint16_t> a = makeCompactUniquePtr<A, uint16_t>();
    EXPECT_EQ(1U, A::s_constructorCallCount);
    EXPECT_EQ(0U, A::s_destructorCallCount);
    EXPECT_NE(nullptr, a.pointer());
    EXPECT_EQ(0U, a.type());

    a.setPointer(nullptr);
    EXPECT_EQ(1U, A::s_constructorCallCount);
    EXPECT_EQ(1U, A::s_destructorCallCount);
    EXPECT_EQ(nullptr, a.pointer());
    EXPECT_EQ(0U, a.type());

    a = makeCompactUniquePtr<A, uint16_t>();
    EXPECT_EQ(2U, A::s_constructorCallCount);
    EXPECT_EQ(1U, A::s_destructorCallCount);
    EXPECT_NE(nullptr, a.pointer());
    EXPECT_EQ(0U, a.type());

    a.setType(0xffffU);
    EXPECT_EQ(2U, A::s_constructorCallCount);
    EXPECT_EQ(1U, A::s_destructorCallCount);
    EXPECT_NE(nullptr, a.pointer());
    EXPECT_EQ(0xffffU, a.type());

    a.setPointer(makeUnique<A>());
    A* oldPointer = a.pointer();
    EXPECT_EQ(3U, A::s_constructorCallCount);
    EXPECT_EQ(2U, A::s_destructorCallCount);
    EXPECT_NE(nullptr, a.pointer());
    EXPECT_EQ(0xffffU, a.type());

    a.setType(0x1010U);
    EXPECT_EQ(3U, A::s_constructorCallCount);
    EXPECT_EQ(2U, A::s_destructorCallCount);
    EXPECT_EQ(oldPointer, a.pointer());
    EXPECT_EQ(0x1010U, a.type());

    a = makeCompactUniquePtr<A, uint16_t>();
    EXPECT_EQ(4U, A::s_constructorCallCount);
    EXPECT_EQ(3U, A::s_destructorCallCount);
    EXPECT_NE(nullptr, a.pointer());
    EXPECT_EQ(0x0U, a.type());

    a.setType(0xf2f2U);
    EXPECT_EQ(4U, A::s_constructorCallCount);
    EXPECT_EQ(3U, A::s_destructorCallCount);
    EXPECT_NE(nullptr, a.pointer());
    EXPECT_EQ(0xf2f2U, a.type());

    oldPointer = a.pointer();
    auto movedA = a.moveToUniquePtr();
    EXPECT_EQ(4U, A::s_constructorCallCount);
    EXPECT_EQ(3U, A::s_destructorCallCount);
    EXPECT_EQ(nullptr, a.pointer());
    EXPECT_EQ(movedA.get(), oldPointer);
    EXPECT_EQ(0xf2f2U, a.type());

    a.setPointer(WTFMove(movedA));
    EXPECT_EQ(4U, A::s_constructorCallCount);
    EXPECT_EQ(3U, A::s_destructorCallCount);
    EXPECT_EQ(oldPointer, a.pointer());
    EXPECT_EQ(movedA.get(), nullptr);
    EXPECT_EQ(0xf2f2U, a.type());

    CompactUniquePtrTuple<A, uint16_t> b = WTFMove(a);
    EXPECT_EQ(4U, A::s_constructorCallCount);
    EXPECT_EQ(3U, A::s_destructorCallCount);
    EXPECT_EQ(nullptr, a.pointer());
    EXPECT_EQ(0, a.type());
    EXPECT_EQ(oldPointer, b.pointer());
    EXPECT_EQ(0xf2f2U, b.type());

    CompactUniquePtrTuple<A, uint16_t>* bPtr = &b;

    b = WTFMove(*bPtr);
    EXPECT_EQ(4U, A::s_constructorCallCount);
    EXPECT_EQ(3U, A::s_destructorCallCount);
    EXPECT_EQ(nullptr, a.pointer());
    EXPECT_EQ(0, a.type());
    EXPECT_EQ(oldPointer, b.pointer());
    EXPECT_EQ(0xf2f2U, b.type());

    b.setPointer(nullptr);
    EXPECT_EQ(4U, A::s_constructorCallCount);
    EXPECT_EQ(4U, A::s_destructorCallCount);
    EXPECT_EQ(nullptr, a.pointer());
    EXPECT_EQ(0, a.type());
    EXPECT_EQ(nullptr, b.pointer());
    EXPECT_EQ(0xf2f2U, b.type());
}

class B : public A {
    WTF_MAKE_FAST_ALLOCATED;

public:
    B() { ++s_constructorCallCount; }
    virtual ~B() { ++s_destructorCallCount; }

    static unsigned s_constructorCallCount;
    static unsigned s_destructorCallCount;
};

unsigned B::s_constructorCallCount = 0;
unsigned B::s_destructorCallCount = 0;

TEST(WTF_CompactUniquePtrTuple, Subclassing)
{
    A::s_constructorCallCount = 0;
    A::s_destructorCallCount = 0;
    B::s_constructorCallCount = 0;
    B::s_destructorCallCount = 0;

    CompactUniquePtrTuple<B, uint16_t> empty;
    EXPECT_EQ(0U, A::s_constructorCallCount);
    EXPECT_EQ(0U, A::s_destructorCallCount);
    EXPECT_EQ(0U, B::s_constructorCallCount);
    EXPECT_EQ(0U, B::s_destructorCallCount);
    EXPECT_EQ(nullptr, empty.pointer());
    EXPECT_EQ(0U, empty.type());

    CompactUniquePtrTuple<B, uint16_t> b = makeCompactUniquePtr<B, uint16_t>();
    EXPECT_EQ(1U, A::s_constructorCallCount);
    EXPECT_EQ(0U, A::s_destructorCallCount);
    EXPECT_EQ(1U, B::s_constructorCallCount);
    EXPECT_EQ(0U, B::s_destructorCallCount);
    EXPECT_NE(nullptr, b.pointer());
    EXPECT_EQ(0U, b.type());

    b.setType(0xff);
    EXPECT_EQ(1U, A::s_constructorCallCount);
    EXPECT_EQ(0U, A::s_destructorCallCount);
    EXPECT_EQ(1U, B::s_constructorCallCount);
    EXPECT_EQ(0U, B::s_destructorCallCount);
    EXPECT_NE(nullptr, b.pointer());
    EXPECT_EQ(0xffU, b.type());

    b.setPointer(nullptr);
    EXPECT_EQ(1U, A::s_constructorCallCount);
    EXPECT_EQ(1U, A::s_destructorCallCount);
    EXPECT_EQ(1U, B::s_constructorCallCount);
    EXPECT_EQ(1U, B::s_destructorCallCount);
    EXPECT_EQ(nullptr, b.pointer());
    EXPECT_EQ(0xffU, b.type());

    CompactUniquePtrTuple<A, uint16_t> a = makeCompactUniquePtr<B, uint16_t>();
    EXPECT_EQ(2U, A::s_constructorCallCount);
    EXPECT_EQ(1U, A::s_destructorCallCount);
    EXPECT_EQ(2U, B::s_constructorCallCount);
    EXPECT_EQ(1U, B::s_destructorCallCount);
    EXPECT_NE(nullptr, a.pointer());
    EXPECT_EQ(0U, a.type());

    a.setType(0x1c);
    EXPECT_EQ(2U, A::s_constructorCallCount);
    EXPECT_EQ(1U, A::s_destructorCallCount);
    EXPECT_EQ(2U, B::s_constructorCallCount);
    EXPECT_EQ(1U, B::s_destructorCallCount);
    EXPECT_NE(nullptr, a.pointer());
    EXPECT_EQ(0x1cU, a.type());

    auto* oldPointer = a.pointer();
    a.setPointer(makeUnique<B>());
    EXPECT_EQ(3U, A::s_constructorCallCount);
    EXPECT_EQ(2U, A::s_destructorCallCount);
    EXPECT_EQ(3U, B::s_constructorCallCount);
    EXPECT_EQ(2U, B::s_destructorCallCount);
    EXPECT_NE(oldPointer, a.pointer());
    EXPECT_EQ(0x1cU, a.type());

    a = makeCompactUniquePtr<A, uint16_t>();
    EXPECT_EQ(4U, A::s_constructorCallCount);
    EXPECT_EQ(3U, A::s_destructorCallCount);
    EXPECT_EQ(3U, B::s_constructorCallCount);
    EXPECT_EQ(3U, B::s_destructorCallCount);
    EXPECT_NE(oldPointer, a.pointer());
    EXPECT_EQ(0U, a.type());

    a.setType(0xfed);
    EXPECT_EQ(4U, A::s_constructorCallCount);
    EXPECT_EQ(3U, A::s_destructorCallCount);
    EXPECT_EQ(3U, B::s_constructorCallCount);
    EXPECT_EQ(3U, B::s_destructorCallCount);
    EXPECT_NE(nullptr, a.pointer());
    EXPECT_EQ(0xfedU, a.type());

    oldPointer = a.pointer();
    auto uniqueA = a.moveToUniquePtr();
    EXPECT_EQ(4U, A::s_constructorCallCount);
    EXPECT_EQ(3U, A::s_destructorCallCount);
    EXPECT_EQ(3U, B::s_constructorCallCount);
    EXPECT_EQ(3U, B::s_destructorCallCount);
    EXPECT_EQ(nullptr, a.pointer());
    EXPECT_EQ(oldPointer, uniqueA.get());
    EXPECT_EQ(0xfedU, a.type());
    
    uniqueA = nullptr;
    EXPECT_EQ(4U, A::s_constructorCallCount);
    EXPECT_EQ(4U, A::s_destructorCallCount);
    EXPECT_EQ(3U, B::s_constructorCallCount);
    EXPECT_EQ(3U, B::s_destructorCallCount);
    EXPECT_EQ(nullptr, a.pointer());
    EXPECT_EQ(nullptr, uniqueA.get());
    EXPECT_EQ(0xfedU, a.type());
}

struct ADeleter {
    static unsigned s_callCount;

    void operator()(A* a) const
    {
        ++s_callCount;
        delete a;
    }
};

unsigned ADeleter::s_callCount = 0;

TEST(WTF_CompactUniquePtrTuple, Deleter)
{
    A::s_constructorCallCount = 0;
    A::s_destructorCallCount = 0;
    B::s_constructorCallCount = 0;
    B::s_destructorCallCount = 0;
    ADeleter::s_callCount = 0;

    CompactUniquePtrTuple<B, uint16_t, ADeleter> empty;
    EXPECT_EQ(0U, A::s_constructorCallCount);
    EXPECT_EQ(0U, A::s_destructorCallCount);
    EXPECT_EQ(0U, B::s_constructorCallCount);
    EXPECT_EQ(0U, B::s_destructorCallCount);
    EXPECT_EQ(0U, ADeleter::s_callCount);
    EXPECT_EQ(nullptr, empty.pointer());
    EXPECT_EQ(0U, empty.type());

    CompactUniquePtrTuple<A, uint16_t, ADeleter> a = makeCompactUniquePtr<A, uint16_t, ADeleter>();
    EXPECT_EQ(1U, A::s_constructorCallCount);
    EXPECT_EQ(0U, A::s_destructorCallCount);
    EXPECT_EQ(0U, B::s_constructorCallCount);
    EXPECT_EQ(0U, B::s_destructorCallCount);
    EXPECT_EQ(0U, ADeleter::s_callCount);
    EXPECT_NE(nullptr, a.pointer());
    EXPECT_EQ(0U, a.type());

    a.setType(0x7e89U);
    EXPECT_EQ(1U, A::s_constructorCallCount);
    EXPECT_EQ(0U, A::s_destructorCallCount);
    EXPECT_EQ(0U, B::s_constructorCallCount);
    EXPECT_EQ(0U, B::s_destructorCallCount);
    EXPECT_EQ(0U, ADeleter::s_callCount);
    EXPECT_NE(nullptr, a.pointer());
    EXPECT_EQ(0x7e89U, a.type());

    auto* oldPointer = a.pointer();
    a.setPointer(std::unique_ptr<B, ADeleter>(new B));
    EXPECT_EQ(2U, A::s_constructorCallCount);
    EXPECT_EQ(1U, A::s_destructorCallCount);
    EXPECT_EQ(1U, B::s_constructorCallCount);
    EXPECT_EQ(0U, B::s_destructorCallCount);
    EXPECT_EQ(1U, ADeleter::s_callCount);
    EXPECT_NE(oldPointer, a.pointer());
    EXPECT_EQ(0x7e89U, a.type());

}

} // namespace TestWebKitAPI

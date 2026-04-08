/*
 * Copyright (C) 2026 Igalia, S.L. All rights reserved.
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "Helpers/Test.h"
#include <bmalloc/StackPointer.h>
#include <wtf/Compiler.h>
#include <wtf/StackBounds.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace TestWebKitAPI {

NEVER_INLINE static void* getStackPointerFromNeverInline()
{
    volatile int buffer[1];
    buffer[0] = 1;
    buffer[0] = buffer[0] + 1;
    return currentStackPointer();
}

// Uses __attribute__ directly because the ALWAYS_INLINE macro falls back
// to plain "inline" in debug builds.
static inline __attribute__((__always_inline__)) void* getStackPointerFromAlwaysInline()
{
    volatile int buffer[1];
    buffer[0] = 1;
    buffer[0] = buffer[0] + 1;
    return currentStackPointer();
}

NEVER_INLINE static void* getStackPointerFromSmallFrame()
{
    volatile int buffer[1];
    buffer[0] = 1;
    buffer[0] = buffer[0] + 1;
    return currentStackPointer();
}

NEVER_INLINE
#if COMPILER(CLANG)
__attribute__((optnone))
#else
__attribute__((optimize("O0")))
#endif
static void* getStackPointerFromLargeFrame()
{
    volatile int buffer[8192];
    buffer[0] = 1;
    buffer[0] = buffer[0] + 1;
    buffer[8191] = buffer[0];
    return currentStackPointer();
}

NEVER_INLINE
#if COMPILER(CLANG)
__attribute__((optnone))
#else
__attribute__((optimize("O0")))
#endif
static void* getStackPointerAtDepth(int depth)
{
    volatile int dummy = depth;
    if (dummy <= 0)
        return currentStackPointer();
    return getStackPointerAtDepth(dummy - 1);
}

TEST(CurrentStackPointer, WithinBounds)
{
    auto bounds = StackBounds::currentThreadStackBounds();
    void* sp = currentStackPointer();
    EXPECT_TRUE(bounds.contains(sp));
}

TEST(CurrentStackPointer, NearLocalVariable)
{
    void* sp = getStackPointerFromSmallFrame();
    auto bounds = StackBounds::currentThreadStackBounds();
    EXPECT_TRUE(bounds.contains(sp));

    void* sp2 = getStackPointerFromSmallFrame();
    ptrdiff_t delta = std::abs(static_cast<char*>(sp) - static_cast<char*>(sp2));
    EXPECT_LT(static_cast<size_t>(delta), 512u);
}

TEST(CurrentStackPointer, FromNeverInlineFunction)
{
    auto bounds = StackBounds::currentThreadStackBounds();
    void* spHere = currentStackPointer();
    void* spCallee = getStackPointerFromNeverInline();

    EXPECT_TRUE(bounds.contains(spHere));
    EXPECT_TRUE(bounds.contains(spCallee));

    EXPECT_LT(reinterpret_cast<uintptr_t>(spCallee), reinterpret_cast<uintptr_t>(spHere));
}

TEST(CurrentStackPointer, FromAlwaysInlineFunction)
{
    auto bounds = StackBounds::currentThreadStackBounds();
    void* spHere = currentStackPointer();
    void* spInlined = getStackPointerFromAlwaysInline();

    EXPECT_TRUE(bounds.contains(spHere));
    EXPECT_TRUE(bounds.contains(spInlined));

    ptrdiff_t delta = std::abs(static_cast<char*>(spHere) - static_cast<char*>(spInlined));
    EXPECT_LT(static_cast<size_t>(delta), 512u);
}

TEST(CurrentStackPointer, FromLambda)
{
    auto bounds = StackBounds::currentThreadStackBounds();
    auto lambda = [&]() -> void* {
        return currentStackPointer();
    };
    void* spLambda = lambda();
    EXPECT_TRUE(bounds.contains(spLambda));
}

TEST(CurrentStackPointer, FromFunctionPointer)
{
    auto bounds = StackBounds::currentThreadStackBounds();
    void* (* volatile funcPtr)() = &getStackPointerFromNeverInline;
    void* sp = funcPtr();
    EXPECT_TRUE(bounds.contains(sp));
}

TEST(CurrentStackPointer, DeeperFrameHasLowerAddress)
{
    auto bounds = StackBounds::currentThreadStackBounds();
    void* sp0 = currentStackPointer();
    void* sp5 = getStackPointerAtDepth(5);
    void* sp10 = getStackPointerAtDepth(10);
    void* sp20 = getStackPointerAtDepth(20);

    EXPECT_TRUE(bounds.contains(sp0));
    EXPECT_TRUE(bounds.contains(sp5));
    EXPECT_TRUE(bounds.contains(sp10));
    EXPECT_TRUE(bounds.contains(sp20));

    EXPECT_GT(reinterpret_cast<uintptr_t>(sp0), reinterpret_cast<uintptr_t>(sp5));
    EXPECT_GT(reinterpret_cast<uintptr_t>(sp5), reinterpret_cast<uintptr_t>(sp10));
    EXPECT_GT(reinterpret_cast<uintptr_t>(sp10), reinterpret_cast<uintptr_t>(sp20));
}

TEST(CurrentStackPointer, LargeFrameHasLowerAddress)
{
    void* spHere = currentStackPointer();
    void* spLarge = getStackPointerFromLargeFrame();

    EXPECT_GE(reinterpret_cast<uintptr_t>(spHere) - reinterpret_cast<uintptr_t>(spLarge), 8192u);
}

TEST(CurrentStackPointer, ConsistentAcrossConsecutiveCalls)
{
    void* sp1 = getStackPointerFromSmallFrame();
    void* sp2 = getStackPointerFromSmallFrame();

    ptrdiff_t delta = std::abs(static_cast<char*>(sp1) - static_cast<char*>(sp2));
    EXPECT_LT(static_cast<size_t>(delta), 8u);
}
static constexpr uintptr_t stackMagic = 0xDEADBEEFCAFEBABEull;

SUPPRESS_ASAN NEVER_INLINE static uintptr_t* findMagicFromSP()
{
    void* sp = currentStackPointer();
    uintptr_t* search = reinterpret_cast<uintptr_t*>(sp);
    uintptr_t* limit = search + 256;
    for (; search < limit; ++search) {
        if (*search == stackMagic)
            return search;
    }
    return nullptr;
}

TEST(CurrentStackPointer, FindAndOverwriteMagicOnStack)
{
    volatile uintptr_t magic = stackMagic;

    uintptr_t* found = findMagicFromSP();

    ASSERT_TRUE(found != nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(found), reinterpret_cast<uintptr_t>(&magic));
    EXPECT_EQ(*found, stackMagic);

    constexpr uintptr_t newValue = 0x12345678;
    *found = newValue;
    EXPECT_EQ(magic, newValue);
}

} // namespace TestWebKitAPI

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

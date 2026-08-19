/*
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
#include <wtf/EscapableByteSpan.h>

#include "Helpers/Test.h"
#include <optional>
#include <wtf/Vector.h>

// These tests exercise the C++ side of the byte views: the ~Escapable Borrowed* pair,
// and the Escapable* types with the Swift reference count that outlives-the-borrow
// bugs trip over. The Swift-side guarantees (that a ~Escapable view cannot be stored
// or outlive its call) are enforced by the compiler and so are not testable here.

namespace TestWebKitAPI {

#if ASSERT_ENABLED
#define EXPECTED_BORROW_CRASH "ASSERTION FAILED"
#else
#define EXPECTED_BORROW_CRASH ""
#endif

// MARK: - EscapableByteSpan

TEST(WTF_EscapableByteSpan, ExposesBytes)
{
    Vector<uint8_t> source { 10, 20, 30, 40, 50 };
    auto span = source.span();
    EscapableByteSpan bytes(span);

    EXPECT_EQ(bytes.size(), span.size());
    EXPECT_EQ(bytes.span().data(), span.data());
    EXPECT_EQ(bytes.span().size(), span.size());
    for (size_t i = 0; i < span.size(); ++i)
        EXPECT_EQ(bytes.span()[i], span[i]);
}

TEST(WTF_EscapableByteSpan, OverSubspan)
{
    Vector<uint8_t> source { 0, 1, 2, 3, 4, 5 };
    auto subspan = source.span().subspan(2, 3);
    EscapableByteSpan bytes(subspan);

    EXPECT_EQ(bytes.size(), 3u);
    EXPECT_EQ(bytes.span().data(), subspan.data());
    EXPECT_EQ(bytes.span()[0], 2u);
    EXPECT_EQ(bytes.span()[2], 4u);
}

TEST(WTF_EscapableByteSpan, VectorBytesExposesBytes)
{
    Vector<uint8_t> vector { 1, 2, 3, 4 };
    auto bytesBorrow = borrow(vector);
    EscapableByteSpan bytes = escapableSpan(bytesBorrow->span());

    EXPECT_EQ(bytes.size(), vector.size());
    EXPECT_EQ(bytes.span().data(), vector.span().data());
    for (size_t i = 0; i < vector.size(); ++i)
        EXPECT_EQ(bytes.span()[i], vector[i]);
}

TEST(WTF_EscapableByteSpan, EmptySpan)
{
    EscapableByteSpan bytes(std::span<const uint8_t> { });
    EXPECT_EQ(bytes.size(), 0u);
}

TEST(WTF_EscapableByteSpan, EmptyVector)
{
    Vector<uint8_t> vector;
    auto bytesBorrow = borrow(vector);
    EscapableByteSpan bytes = escapableSpan(bytesBorrow->span());
    EXPECT_EQ(bytes.size(), 0u);
}

// A transient copy, as Swift makes for the duration of a synchronous call, is fine as
// long as it is destroyed before the borrow ends. Copy construction and destruction are
// what Swift's value copies land on.
TEST(WTF_EscapableByteSpan, TransientCopyIsFine)
{
    Vector<uint8_t> vector { 1, 2, 3 };
    auto bytesBorrow = borrow(vector);
    EscapableByteSpan bytes = escapableSpan(bytesBorrow->span());

    {
        EscapableByteSpan copy(bytes);
        EXPECT_EQ(copy.size(), vector.size());
        EXPECT_EQ(copy.span().data(), vector.span().data());

        // A copy of a copy still counts against the same root.
        EscapableByteSpan copyOfCopy(copy);
        EXPECT_EQ(copyOfCopy.span().data(), vector.span().data());
    }

    EXPECT_EQ(bytes.size(), vector.size());
}

// Nested borrows of the same Vector are permitted; the inner borrow restores
// the outer borrow's state on destruction.
TEST(WTF_EscapableByteSpan, NestedVectorBorrows)
{
    Vector<uint8_t> vector { 9, 8, 7 };
    auto outerBorrow = borrow(vector);
    EscapableByteSpan outer = escapableSpan(outerBorrow->span());
    {
        auto innerBorrow = borrow(vector);
        EscapableByteSpan inner = escapableSpan(innerBorrow->span());
        EXPECT_EQ(inner.span().data(), vector.span().data());
    }
    EXPECT_EQ(outer.size(), vector.size());
}

TEST(WTF_EscapableByteSpan, ViewIdentityIsPerBorrow)
{
    Vector<uint8_t> vector { 1, 2, 3 };
    auto outerBorrow = borrow(vector);
    EscapableByteSpan outer = escapableSpan(outerBorrow->span());
    EXPECT_EQ(&outer, &outer);

    {
        auto innerBorrow = borrow(vector);
        EscapableByteSpan inner = escapableSpan(innerBorrow->span());
        EXPECT_NE(&inner, &outer);
        EscapableByteSpan copy(inner);
        EXPECT_EQ(copy.span().data(), vector.span().data());
    }

    // The inner borrow ended, but the outer view is untouched and still readable.
    EXPECT_EQ(outer.span().data(), vector.span().data());
    EXPECT_EQ(outer.size(), vector.size());
}

// MARK: - The stash check
//
// A copy stashed beyond the root's lifetime is caught when the root is destroyed, so
// the crash stack points at the too-early end of the borrow rather than later at an
// innocent reader. These are RELEASE_ASSERTs, so unlike most of the Borrow machinery
// they fire in release builds too.

TEST(WTF_EscapableByteSpanDeathTest, StashedCopyCrashesWhenTheBorrowEnds)
{
    auto shouldCrash = [] {
        // Outlives the borrow, as a Swift stored property would.
        std::optional<EscapableByteSpan> stash;
        {
            Vector<uint8_t> vector { 1, 2, 3 };
            auto bytesBorrow = borrow(vector);
            EscapableByteSpan bytes = escapableSpan(bytesBorrow->span());
            stash.emplace(bytes);
        }
    };
    ASSERT_DEATH_IF_SUPPORTED(shouldCrash(), EXPECTED_BORROW_CRASH);
}

TEST(WTF_EscapableByteSpanDeathTest, StashedCopyOfACopyCrashesWhenTheBorrowEnds)
{
    auto shouldCrash = [] {
        std::optional<EscapableByteSpan> stash;
        {
            Vector<uint8_t> vector { 1, 2, 3 };
            auto bytesBorrow = borrow(vector);
            EscapableByteSpan bytes = escapableSpan(bytesBorrow->span());
            // The intermediate copy dies first, so this only crashes if the copy of a
            // copy counted itself against the root rather than against its parent.
            {
                EscapableByteSpan intermediate(bytes);
                stash.emplace(intermediate);
            }
        }
    };
    ASSERT_DEATH_IF_SUPPORTED(shouldCrash(), EXPECTED_BORROW_CRASH);
}

TEST(WTF_EscapableByteSpanDeathTest, StashedCopyOverASpanCrashesWhenTheBorrowEnds)
{
    auto shouldCrash = [] {
        std::optional<EscapableByteSpan> stash;
        {
            Vector<uint8_t> vector { 1, 2, 3 };
            auto span = vector.span();
            EscapableByteSpan bytes(span);
            stash.emplace(bytes);
        }
    };
    ASSERT_DEATH_IF_SUPPORTED(shouldCrash(), EXPECTED_BORROW_CRASH);
}

TEST(WTF_EscapableByteSpanDeathTest, CopyEscapedToTheHeapCrashesWhenTheBorrowEnds)
{
    auto shouldCrash = [] {
        struct Escapee {
        std::optional<EscapableByteSpan> bytes;
        };
        auto escapee = makeUniqueWithoutFastMallocCheck<Escapee>();
        {
            Vector<uint8_t> vector { 1, 2, 3 };
            auto bytesBorrow = borrow(vector);
            EscapableByteSpan bytes = escapableSpan(bytesBorrow->span());
            escapee->bytes.emplace(bytes);
        }
    };
    ASSERT_DEATH_IF_SUPPORTED(shouldCrash(), EXPECTED_BORROW_CRASH);
}

TEST(WTF_EscapableByteSpan, CopyEscapedToTheHeapAndReleasedInTimeIsFine)
{
    struct Escapee {
        std::optional<EscapableByteSpan> bytes;
    };
    auto escapee = makeUniqueWithoutFastMallocCheck<Escapee>();
    Vector<uint8_t> vector { 1, 2, 3 };
    {
        auto bytesBorrow = borrow(vector);
        EscapableByteSpan bytes = escapableSpan(bytesBorrow->span());
        escapee->bytes.emplace(bytes);
        EXPECT_EQ(escapee->bytes->size(), vector.size());
        escapee->bytes.reset();
    }
    EXPECT_FALSE(escapee->bytes);
}

TEST(WTF_EscapableByteSpanDeathTest, MAYBE_ASSERT_ENABLED_DEATH_TEST(VectorBytesReallocationWhileBorrowedCrashes))
{
    auto shouldCrash = [] {
        Vector<uint8_t> vector { 1, 2, 3 };
        auto bytesBorrow = borrow(vector);
        EscapableByteSpan bytes = escapableSpan(bytesBorrow->span());
        vector.reserveCapacity(vector.capacity() + 1024);
    };
    ASSERT_DEATH_IF_SUPPORTED(shouldCrash(), "");
}

TEST(WTF_EscapableByteSpanDeathTest, MAYBE_ASSERT_ENABLED_DEATH_TEST(VectorBytesDestroyedVectorWhileBorrowedCrashes))
{
    auto shouldCrash = [] {
        auto* vector = new Vector<uint8_t> { 1, 2, 3 };
        auto bytesBorrow = borrow(*vector);
        EscapableByteSpan bytes = escapableSpan(bytesBorrow->span());
        delete vector;
    };
    ASSERT_DEATH_IF_SUPPORTED(shouldCrash(), "");
}

// A mutation that does not reallocate is permitted: the borrow guards the interior
// buffer, not the contents.
// Swift receives this by value, so this is what a call across the boundary does: bind a
// reference and copy it.
static size_t takesByReferenceAsSwiftWould(const EscapableByteSpan& bytes)
{
    EscapableByteSpan copy(bytes);
    return copy.size();
}

// The scope holds a Borrow on the Vector. If ending it failed to relinquish that Borrow,
// this reallocating mutation would crash.
TEST(WTF_EscapableByteSpan, VectorBytesReleasesItsBorrowWhenTheScopeEnds)
{
    Vector<uint8_t> vector { 1, 2, 3 };
    {
        auto bytesBorrow = borrow(vector);
        EscapableByteSpan bytes = escapableSpan(bytesBorrow->span());
        EXPECT_EQ(takesByReferenceAsSwiftWould(bytes), vector.size());
    }

    vector.reserveCapacity(vector.capacity() + 1024);
    vector.append(4);
    EXPECT_EQ(vector.size(), 4u);
}

// Same, for a borrow that is copied repeatedly before the scope ends.
TEST(WTF_EscapableByteSpan, VectorBytesReleasesItsBorrowAfterRepeatedCopies)
{
    Vector<uint8_t> vector { 9, 8, 7 };
    {
        auto bytesBorrow = borrow(vector);
        EscapableByteSpan bytes = escapableSpan(bytesBorrow->span());
        for (unsigned i = 0; i < 4; ++i)
            EXPECT_EQ(takesByReferenceAsSwiftWould(bytes), vector.size());
    }

    Vector<uint8_t> other { 1 };
    vector.swap(other);
    EXPECT_EQ(vector.size(), 1u);
}

TEST(WTF_EscapableByteSpan, VectorBytesNonReallocatingMutationIsFine)
{
    Vector<uint8_t> vector { 1, 2, 3 };
    vector.reserveCapacity(16);
    {
        auto bytesBorrow = borrow(vector);
        EscapableByteSpan bytes = escapableSpan(bytesBorrow->span());
        vector[0] = 9;
        EXPECT_EQ(bytes.span()[0], 9u);
    }
    vector.append(4);
    EXPECT_EQ(vector.size(), 4u);
}

#if OS(DARWIN)
TEST(WTF_EscapableByteSpanDeathTest, MAYBE_ASSERT_ENABLED_DEATH_TEST(HeapAllocatedBytesCrashes))
{
    auto shouldCrash = [] {
        struct Holder {
            explicit Holder(std::span<const uint8_t> bytes)
                : bytes(bytes)
            {
            }
            EscapableByteSpan bytes;
        };
        Vector<uint8_t> vector { 1, 2, 3 };
        // Deliberately leaked: the constructor is expected to crash, and if it
        // somehow does not, destroying the Holder would crash for another reason.
        SUPPRESS_UNCOUNTED_LOCAL auto* holder = new Holder(vector.span());
        EXPECT_EQ(holder->bytes.size(), vector.size());
    };
    ASSERT_DEATH_IF_SUPPORTED(shouldCrash(), "");
}
#endif

} // namespace TestWebKitAPI

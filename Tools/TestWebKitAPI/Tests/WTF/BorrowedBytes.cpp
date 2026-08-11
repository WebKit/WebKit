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
#include <wtf/BorrowedBytes.h>

#include "Helpers/Test.h"
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

// These tests exercise the C++ side of BorrowedBytes: the two scope types and
// the revocable view they hand out.

namespace TestWebKitAPI {

TEST(WTF_BorrowedBytes, SpanScopeExposesBytes)
{
    Vector<uint8_t> source { 10, 20, 30, 40, 50 };
    auto span = source.span();
    BorrowedSpanScope scope(span);

    auto& bytes = scope.bytes();
    EXPECT_EQ(bytes.size(), span.size());
    EXPECT_EQ(bytes.data(), span.data());
    for (size_t i = 0; i < span.size(); ++i)
        EXPECT_EQ(bytes.data()[i], span[i]);
}

// The real reason BorrowedSpanScope exists: borrowing a partial view into a
// buffer, which has no whole-container to register a Borrow with. Mirrors
// PlatformECKey::importCompressedPub.
TEST(WTF_BorrowedBytes, SpanScopeOverSubspan)
{
    Vector<uint8_t> source { 0, 1, 2, 3, 4, 5 };
    auto subspan = source.span().subspan(2, 3);
    BorrowedSpanScope scope(subspan);

    EXPECT_EQ(scope.bytes().size(), 3u);
    EXPECT_EQ(scope.bytes().data(), subspan.data());
    EXPECT_EQ(scope.bytes().data()[0], 2u);
    EXPECT_EQ(scope.bytes().data()[2], 4u);
}

TEST(WTF_BorrowedBytes, VectorScopeExposesBytes)
{
    Vector<uint8_t> vector { 1, 2, 3, 4 };
    BorrowedVectorScope scope(vector);

    auto& bytes = scope.bytes();
    EXPECT_EQ(bytes.size(), vector.size());
    EXPECT_EQ(bytes.data(), vector.span().data());
    for (size_t i = 0; i < vector.size(); ++i)
        EXPECT_EQ(bytes.data()[i], vector[i]);
}

TEST(WTF_BorrowedBytes, EmptySpan)
{
    BorrowedSpanScope scope(std::span<const uint8_t> { });
    EXPECT_EQ(scope.bytes().size(), 0u);
}

TEST(WTF_BorrowedBytes, EmptyVector)
{
    Vector<uint8_t> vector;
    BorrowedVectorScope scope(vector);
    EXPECT_EQ(scope.bytes().size(), 0u);
}

// While the borrow is live and un-stashed, the scope holds the only reference.
// A transient reference (as Swift takes for the duration of a synchronous call)
// is fine as long as it is released before the scope ends.
TEST(WTF_BorrowedBytes, ScopeHoldsSoleReference)
{
    Vector<uint8_t> vector { 1, 2, 3 };
    BorrowedVectorScope scope(vector);
    EXPECT_TRUE(scope.bytes().hasOneRef());
    {
        RefPtr<BorrowedBytes> transient = &scope.bytes();
        EXPECT_FALSE(scope.bytes().hasOneRef());
        EXPECT_EQ(transient->size(), vector.size());
    }
    EXPECT_TRUE(scope.bytes().hasOneRef());
}

// Nested borrows of the same Vector are permitted; the inner borrow restores
// the outer borrow's state on destruction.
TEST(WTF_BorrowedBytes, NestedVectorScopes)
{
    Vector<uint8_t> vector { 9, 8, 7 };
    BorrowedVectorScope outer(vector);
    {
        BorrowedVectorScope inner(vector);
        EXPECT_EQ(inner.bytes().data(), vector.span().data());
    }
    EXPECT_EQ(outer.bytes().size(), vector.size());
}

TEST(WTF_BorrowedBytes, ViewIdentityIsPerScope)
{
    Vector<uint8_t> vector { 1, 2, 3 };
    BorrowedVectorScope outer(vector);
    EXPECT_EQ(&outer.bytes(), &outer.bytes());

    RefPtr<BorrowedBytes> innerView;
    {
        BorrowedVectorScope inner(vector);
        EXPECT_NE(&inner.bytes(), &outer.bytes());
        innerView = &inner.bytes();
        innerView = nullptr;
    }

    // The inner borrow ended, but the outer view is untouched and still readable.
    EXPECT_EQ(outer.bytes().data(), vector.span().data());
    EXPECT_EQ(outer.bytes().size(), vector.size());
}

// MARK: - The stash check
//
// A view stashed beyond the scope's lifetime is caught at scope destruction, where
// the crash stack points at the too-early end of the borrow rather than later at an
// innocent reader. These are RELEASE_ASSERTs, so unlike most of the Borrow machinery
// they fire in release builds too — which is what makes them, rather than data()'s
// revoked-flag check, the mitigation for a reference that escaped.

TEST(WTF_BorrowedBytesDeathTest, StashedViewCrashesAtScopeEnd)
{
    auto shouldCrash = [] {
        RefPtr<BorrowedBytes> stashed;
        {
            Vector<uint8_t> vector { 1, 2, 3 };
            BorrowedVectorScope scope(vector);
            stashed = &scope.bytes();
            // ~BorrowedVectorScope asserts here: `stashed` still holds a
            // reference, so the borrow is ending too early.
        }
    };
    ASSERT_DEATH_IF_SUPPORTED(shouldCrash(), "");
}

TEST(WTF_BorrowedBytesDeathTest, StashedViewOverASpanCrashesAtScopeEnd)
{
    auto shouldCrash = [] {
        RefPtr<BorrowedBytes> stashed;
        {
            Vector<uint8_t> vector { 1, 2, 3 };
            auto span = vector.span();
            BorrowedSpanScope scope(span);
            stashed = &scope.bytes();
        }
    };
    ASSERT_DEATH_IF_SUPPORTED(shouldCrash(), "");
}

TEST(WTF_BorrowedBytesDeathTest, ViewEscapedToTheHeapCrashesAtScopeEnd)
{
    auto shouldCrash = [] {
        struct Escapee {
            RefPtr<BorrowedBytes> bytes;
        };
        auto* escapee = new Escapee;
        {
            Vector<uint8_t> vector { 1, 2, 3 };
            BorrowedVectorScope scope(vector);
            escapee->bytes = &scope.bytes();
        }
        delete escapee;
    };
    ASSERT_DEATH_IF_SUPPORTED(shouldCrash(), "");
}

TEST(WTF_BorrowedBytes, ViewEscapedToTheHeapAndReleasedInTimeIsFine)
{
    struct Escapee {
        RefPtr<BorrowedBytes> bytes;
    };
    auto escapee = makeUniqueWithoutFastMallocCheck<Escapee>();
    Vector<uint8_t> vector { 1, 2, 3 };
    {
        BorrowedVectorScope scope(vector);
        escapee->bytes = &scope.bytes();
        EXPECT_EQ(escapee->bytes->size(), vector.size());
        escapee->bytes = nullptr;
    }
    EXPECT_FALSE(escapee->bytes);
}

#if OS(DARWIN)
TEST(WTF_BorrowedBytesDeathTest, MAYBE_ASSERT_ENABLED_DEATH_TEST(HeapAllocatedScopeCrashes))
{
    auto shouldCrash = [] {
        struct Holder {
            explicit Holder(std::span<const uint8_t> bytes)
                : scope(bytes)
            {
            }
            BorrowedSpanScope scope;
        };
        Vector<uint8_t> vector { 1, 2, 3 };
        // Deliberately leaked: the constructor is expected to crash, and if it
        // somehow does not, destroying the Holder would crash for another reason.
        SUPPRESS_UNCOUNTED_LOCAL auto* holder = new Holder(vector.span());
        EXPECT_EQ(holder->scope.bytes().size(), vector.size());
    };
    ASSERT_DEATH_IF_SUPPORTED(shouldCrash(), "");
}
#endif

} // namespace TestWebKitAPI

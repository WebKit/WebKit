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

#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <wtf/Assertions.h>
#include <wtf/Borrow.h>
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/SwiftBridging.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>

namespace WTF {

// WTF's byte views for Swift interop.
//
// Where possible, use SpanUInt8 or MutableSpanUInt8 -- in C++ these are simply
// std::span<const uint8_t> and std::span<uint8_t>.
// On the Swift side these materialise as Span<UInt8> and MutableSpan<UInt8>, which are
// non-escapable. That means Swift will ensure at compile-time that the data is not
// stashed anywhere, but is used or copied before control returns to the C++ caller.
//
// However, some Swift APIs are not compatible with non-escapable types. In these cases,
// use EscapableByteSpan which adds a small runtime overhead to ensure Swift
// relinquishes its view of the data before it falls out of C++ scope.

// Common byte-buffer specializations, named so they can be referenced from Swift.
using SpanUInt8 = std::span<const uint8_t>;
using MutableSpanUInt8 = std::span<uint8_t>;
using VectorUInt8 = Vector<uint8_t>;

// EscapableByteSpan is a stack-only control block over a span of bytes owned elsewhere
// (typically a Vector on the C++ stack). It lets Swift borrow C++ bytes with no copy
// while remaining memory safe.
//
// It is always a stack local of the call that lends the bytes, and its destructor
// crashes if any copy of it is still alive when that call returns.
//
// On the Swift side, this type confirms to various useful protocols such as
// ContiguousBytes, DataProtocol - so this type can be passed directly to
// various Swift APIs as a valid set of bytes.
//
// You can compose this with the additional protections of WTF::Borrow - see
// the comment on escapableSpan, below.
//
// This type aims to address lifetime safety only. It does not address the
// differences in aliasing norms between Swift and C++. In future, we may aim to
// reflect the Swift side of this into AliasedSpans as proposed in
// https://github.com/DougGregor/swift-evolution/blob/aliased-spans/proposals/nnnn-aliased-spans.md
// (although that would run into the same limitations that it might not work with
// the majority of Swift APIs.)
// Meanwhile, users need to ensure that the actual data in the span or vector
// does not change during a period when Swift has access.
class SWIFT_ESCAPABLE EscapableByteSpan {
    WTF_FORBID_HEAP_ALLOCATION;
public:
    explicit EscapableByteSpan(SpanUInt8 bytes LIFETIME_BOUND)
        : m_span(bytes)
        , m_state(InPlaceType<Count>)
    {
        assertIsOnStack(this);
    }

    // Swift will make copies of this. Each copy sets m_state.RootRef to
    // the root copy which is maintaining the count.
    EscapableByteSpan(NOESCAPE const EscapableByteSpan& other LIFETIME_BOUND)
        : m_span(other.m_span)
        , m_state(InPlaceType<RootRef>, rootOf(other))
    {
        ++count();
    }

    EscapableByteSpan(EscapableByteSpan&&) = delete;
    EscapableByteSpan& operator=(const EscapableByteSpan&) = delete;
    EscapableByteSpan& operator=(EscapableByteSpan&&) = delete;

    ~EscapableByteSpan()
    {
        if (std::holds_alternative<RootRef>(m_state)) {
            // This is an extra copy
            --count();
        } else {
            // This is the root.
            // The borrow ends here. If anything stashed a copy beyond the synchronous call,
            // it is still counted, so crash at the site that ends the borrow rather than at
            // some later, innocent reader.
            RELEASE_ASSERT(!count());
        }
    }

    size_t size() const { return m_span.size(); }

    // LIFETIME_BOUND ties the Swift Span built from this to the lifetime of this value.
    SpanUInt8 span() const LIFETIME_BOUND { return m_span; }

private:
    using Count = unsigned;
    using RootRef = std::reference_wrapper<const EscapableByteSpan>;

    static const EscapableByteSpan& rootOf(const EscapableByteSpan& bytes LIFETIME_BOUND)
    {
        if (auto* root = std::get_if<RootRef>(&bytes.m_state))
            return root->get();
        return bytes;
    }

    // The root's count, whether this is the root or a copy.
    Count& count() const LIFETIME_BOUND
    {
        const EscapableByteSpan& root = rootOf(*this);
        return std::get<Count>(root.m_state);
    }

    SpanUInt8 m_span;
    mutable Variant<Count, RootRef> m_state;
};

// Builds an EscapableByteSpan for the duration of a call, so the borrow is scoped to
// exactly the expression that lends the bytes:
//
//     pal::Digest::update(escapableSpan(bytes));
//     pal::AesGcm::encrypt(escapableSpan(borrow(key)->span()), ...);
//
// The second form composes with Borrow to get the CanBorrow protection for a Vector.
// Binding the result to a local instead is caught by -Wdangling, because the Borrow
// temporary would not outlive the statement.
inline EscapableByteSpan escapableSpan(SpanUInt8 bytes LIFETIME_BOUND)
{
    return EscapableByteSpan(bytes);
}

} // namespace WTF

using WTF::EscapableByteSpan;
using WTF::escapableSpan;

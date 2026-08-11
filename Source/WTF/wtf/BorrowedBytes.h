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

#include <atomic>
#include <cstdint>
#include <span>
#include <wtf/Assertions.h>
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/Ref.h>
#include <wtf/SwiftBridging.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>

#if !defined(__swift__)
#include <wtf/Borrow.h>
#endif

namespace WTF {

class BorrowedBytesScopeBase;

// Common byte-buffer specialization, also named so it can be referenced from Swift.
using VectorUInt8 = Vector<uint8_t>;

// BorrowedBytes is a reference-counted, revocable view over a span of bytes
// owned elsewhere (typically a Vector on the C++ stack). It lets Swift borrow
// C++ bytes with no copy while remaining memory safe: the bytes are only
// reachable through data(), which crashes cleanly if the borrow has been
// revoked, rather than reading freed memory.
//
// A BorrowedBytes is always created and revoked by a stack-scoped scope object
// (BorrowedSpanScope / BorrowedVectorScope, below), which revokes it when the
// synchronous call that created it returns. Because the control block is
// reference counted, a view that outlives its scope (e.g. accidentally stashed
// on the Swift side) keeps the block alive — so the validity check itself never
// faults — but every subsequent access observes the revoked flag and crashes
// cleanly.
//
// This is, in effect, the Swift-crossable form of Borrow. It composes two
// protections against two distinct failure modes:
//
//   - "vector interior buffer destroyed *during* the borrow": this is exactly what
//     Borrow guards, and BorrowedVectorScope gets it by holding a Borrow on the
//     underlying Vector (engaging its CanBorrow protocol). BorrowedSpanScope
//     has no owning container, so it cannot offer this.
//   - "view stashed and used *after* the borrow ends": Borrow does not guard
//     this because C++ cannot easily stash a stack-only, noncopyable Borrow.
//     Swift can stash a shared reference, so the revocable control block adds a
//     runtime backstop for it. This half is the only part that is novel
//     relative to Borrow, and its entire value is at the Swift boundary.
//
// A C++ caller with a Vector should just use Borrow + span() directly and pay none
// of the refcount/revocation overhead. BorrowedBytes exists to carry a borrow across
// the C++/Swift boundary.
//
// On the Swift side, this type confirms to various useful protocols such as
// ContiguousBytes, DataProtocol - so this type can be passed directly to
// various Swift APIs as a valid set of bytes.
//
// A ~Copyable/~Escapable C++ type (a compile-time borrow-checked alternative to this
// reference-counted design) was tried and rejected: a non-escapable type can't be
// usefully used in some Swift contexts (specifically CryptoKit which is the first
// intended use of this type)
//
// This type aims to address lifetime safety only (including lifetime safety
// of interior storage buffers e.g. within a vector). It does not address the
// differences in aliasing norms between Swift and C++. In future, we may aim to
// reflect the Swift side of this into AliasedSpans as proposed in
// https://github.com/DougGregor/swift-evolution/blob/aliased-spans/proposals/nnnn-aliased-spans.md
// (although that would run into the same limitations that it might not work with
// the majority of Swift APIs.)
// Meanwhile, users need to ensure that the actual data in the span or vector
// does not change during a period when Swift has access.
class BorrowedBytes : public ThreadSafeRefCounted<BorrowedBytes> {
public:
    // Returns the borrowed data pointer, crashing cleanly if the borrow has
    // already been revoked. SWIFT_RETURNS_INDEPENDENT_VALUE keeps this callable
    // from Swift; it is only ever called inside an audited withUnsafeBytes
    // conformance, where the returned pointer is used synchronously.
    const uint8_t* data() const SWIFT_RETURNS_INDEPENDENT_VALUE {
        RELEASE_ASSERT(m_valid.load(std::memory_order_acquire));
        return m_span.data();
    }

    size_t size() const { return m_span.size(); }

#ifdef __swift__
    // FIXME: rdar://165684636 means we have to define these at this level of the
    // type hierarchy (see WTF::RefCountable).
    void ref() const { ThreadSafeRefCounted<BorrowedBytes>::ref(); }
    void deref() const { ThreadSafeRefCounted<BorrowedBytes>::deref(); }
#endif

private:
    friend class BorrowedBytesScopeBase;

    static Ref<BorrowedBytes> create(std::span<const uint8_t> bytes)
    {
        return adoptRef(*new BorrowedBytes(bytes));
    }

    explicit BorrowedBytes(std::span<const uint8_t> bytes)
        : m_span(bytes)
    {
    }

    void revoke() { m_valid.store(false, std::memory_order_release); }

    std::span<const uint8_t> m_span;
    std::atomic<bool> m_valid { true };
} SWIFT_SHARED_REFERENCE(.ref, .deref);

#if !defined(__swift__)

class BorrowedBytesScopeBase {
    WTF_MAKE_NONCOPYABLE(BorrowedBytesScopeBase);
    WTF_FORBID_HEAP_ALLOCATION;
public:
    BorrowedBytes& bytes() LIFETIME_BOUND { return m_bytes.get(); }

protected:
    explicit BorrowedBytesScopeBase(std::span<const uint8_t> bytes LIFETIME_BOUND)
        : m_bytes(BorrowedBytes::create(bytes))
    {
        assertIsOnStack(this);
    }

    ~BorrowedBytesScopeBase()
    {
        // The borrow ends here. If anything on the Swift side stashed the view
        // beyond the synchronous call, the control block still carries an
        // external reference at this point. Crash here rather than relying on
        // data()'s assertion, both for debuggability and thread safety.
        RELEASE_ASSERT(m_bytes->hasOneRef());
        m_bytes->revoke();
    }

private:
    Ref<BorrowedBytes> m_bytes;
};

// Borrows a bare span. Safe only because it is stack-scoped inside a synchronous
// call; it has no owning container to enforce the borrow at runtime, so it must
// never be heap-allocated or outlive its buffer.
class BorrowedSpanScope final : public BorrowedBytesScopeBase {
public:
    explicit BorrowedSpanScope(std::span<const uint8_t> bytes LIFETIME_BOUND)
        : BorrowedBytesScopeBase(bytes)
    {
    }
};

// Borrows a whole Vector and engages its CanBorrow protocol, so a reallocating
// mutation or destruction of the Vector while the borrow is live crashes rather
// than leaving the view dangling. Prefer this whenever a Vector is available.
class BorrowedVectorScope final : public BorrowedBytesScopeBase {
public:
    explicit BorrowedVectorScope(const VectorUInt8& vector LIFETIME_BOUND)
        : BorrowedBytesScopeBase(vector.span())
        , m_borrow(vector)
    {
    }

private:
    Borrow<const VectorUInt8> m_borrow;
};
#endif

} // namespace WTF

using WTF::BorrowedBytes;
#if !defined(__swift__)
using WTF::BorrowedSpanScope;
using WTF::BorrowedVectorScope;
#endif

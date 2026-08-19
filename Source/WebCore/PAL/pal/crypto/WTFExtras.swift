// Copyright (C) 2026 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

private import CxxStdlib
public import Foundation
import wtf

// EscapableByteSpan exposes borrowed C++ bytes to Foundation/CryptoKit consumers with
// no copy. The remaining `unsafe` uses are the UnsafeRawBufferPointer that
// ContiguousBytes' own signature demands, the non-null substitution for an empty buffer,
// and the output copy; a copy that outlives the borrow crashes when the borrow ends.

extension WTF.EscapableByteSpan: ContiguousBytes {
    /// Calls `body` with a raw buffer pointer over the borrowed bytes, valid only for the call.
    public func withUnsafeBytes<R, E>(_ body: (UnsafeRawBufferPointer) throws(E) -> R) throws(E) -> R where E: Error {
        // Safe: the C++ side of EscapableByteSpan has release assertions and
        // LIFETIME_BOUND guarantees that span lives longer than any Swift
        // accesses.
        let elements = unsafe Span<UInt8>(_unsafeCxxSpan: span())
        // Unsafe: We rely on body and its callees honoring the buffer's count and not escaping
        // pointers to it, but nothing ensures those properties.
        return try unsafe elements.bytes.withUnsafeBytes(body)
    }
}

extension WTF.EscapableByteSpan: RandomAccessCollection {
    /// The position of the first byte.
    public var startIndex: Int { 0 }

    /// The position one past the last byte.
    public var endIndex: Int { size() }

    /// The byte at `position`.
    public subscript(position: Int) -> UInt8 {
        // Safe: same justification as in ContiguousBytes above.
        let elements = unsafe Span<UInt8>(_unsafeCxxSpan: span())
        return elements[position]
    }
}

extension WTF.EscapableByteSpan: DataProtocol {
    /// The borrowed bytes as a single contiguous region.
    public var regions: CollectionOfOne<WTF.EscapableByteSpan> { CollectionOfOne(self) }
}

/// Bytes that are safe to hand to CryptoKit even when the source is empty.
///
/// Borrowing an empty WTF::Vector/span yields a view whose base pointer is
/// null, and CryptoKit does not tolerate a null-pointer-backed
/// zero-length buffer. This wrapper substitutes a non-null zero-length buffer
/// in that case.
public struct NonNullBytes: ContiguousBytes, DataProtocol, RandomAccessCollection {
    private let bytes: WTF.EscapableByteSpan

    fileprivate init(_ bytes: WTF.EscapableByteSpan) {
        self.bytes = bytes
    }

    /// Calls `body` with a raw buffer pointer over the bytes, valid only for the call.
    public func withUnsafeBytes<R, E>(_ body: (UnsafeRawBufferPointer) throws(E) -> R) throws(E) -> R where E: Error {
        if bytes.isEmpty {
            // Safe: `placeholder` is a live local, so its address is non-null; we expose it
            // with a zero count, so `body` can never read through it. This gives CryptoKit the
            // non-null base pointer it requires even for an empty buffer.
            var placeholder: UInt8 = 0
            return try unsafe Swift.withUnsafeBytes(of: &placeholder) { (raw: UnsafeRawBufferPointer) throws(E) in
                try unsafe body(UnsafeRawBufferPointer(start: raw.baseAddress, count: 0))
            }
        }
        return try unsafe bytes.withUnsafeBytes(body)
    }

    /// This value as a single contiguous region.
    public var regions: CollectionOfOne<NonNullBytes> { CollectionOfOne(self) }

    /// The position of the first byte.
    public var startIndex: Int { 0 }

    /// The position one past the last byte.
    public var endIndex: Int { bytes.endIndex }

    /// The byte at `position`.
    public subscript(position: Int) -> UInt8 { bytes[position] }
}

extension WTF.EscapableByteSpan {
    /// This value wrapped so it is always safe to hand to CryptoKit APIs, even when
    /// the borrow is empty (see the note on NonNullBytes).
    public var asNonNullBytes: NonNullBytes {
        NonNullBytes(self)
    }
}

/// Copies the bytes of any `ContiguousBytes` into a new `Vector<uint8_t>`.
///
/// This is the crypto *output* counterpart to `WTF.EscapableByteSpan` (the input
/// borrow): the destination Vector is allocated here at exactly the source's
/// byte count and owned by the result, so there is no borrow, lifetime
/// dependency, or aliasing, and copyMemory traps if the counts ever disagree.
///
/// This is a free function rather than a `WTF.VectorUInt8` initializer/extension
/// on purpose: a public Swift *extension* on the C++ class-template specialization
/// Vector<uint8_t> emits unparseable synthesized names (`_CUnsignedLong_0`) into
/// the verified module interface (rdar://181593806). A function that merely
/// *returns* the type prints the `VectorUInt8` typealias and verifies cleanly.
public func makeVectorUInt8(copying bytes: some ContiguousBytes) -> WTF.VectorUInt8 {
    // Safe: bytes borrows its own storage for the duration of this call, and source never escapes the closure.
    unsafe bytes.withUnsafeBytes { source in
        let result = WTF.VectorUInt8(source.count)
        if source.count > 0 {
            // Safe: result was just allocated at source.count bytes, so destination exactly covers its live storage.
            let destination = unsafe UnsafeMutableRawBufferPointer(
                start: UnsafeMutableRawPointer(mutating: result.span().__dataUnsafe()),
                count: result.size()
            )
            // Safe: copyMemory traps on a size mismatch, and source/destination come from separate, non-overlapping allocations.
            unsafe destination.copyMemory(from: source)
        }
        return result
    }
}

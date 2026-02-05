// Copyright (C) 2025-2026 Apple Inc. All rights reserved.
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

#if compiler(>=6.2)

// FIXME (rdar://164119356): Move parts of StdLibExtras.swift into WTF
// (those parts which are not specific to WebKit-level types) - and enable
// irrespective of BACK_FORWARD_LIST_SWIFT
#if ENABLE_BACK_FORWARD_LIST_SWIFT

internal import WebKit_Internal
internal import wtf

/// Conform any WTF::Ref<T> to this protocol to get useful extensions.
protocol CxxRef {
    associatedtype Pointee // you only need to specify this in your conformance
    init(_ object: Pointee)
    func copyRef() -> Self
}

/// Conform any WTF::Vector<WTF::Ref<T>> to this protocol to get useful extensions and iterators.
protocol CxxRefVector {
    associatedtype Element: CxxRef // you only need to specify this in your conformance
    init()
    mutating func append(consuming: Element)
    mutating func reserveCapacity(_ newCapacity: Int)
    func size() -> Int
    // swift-format-ignore: AlwaysUseLowerCamelCase, NoLeadingUnderscores
    func __atUnsafe(_ index: Int) -> UnsafePointer<Element>
}

extension WTF.String: LosslessStringConvertible {
    /// Construct a `WTF.String` from a `Swift.String`.
    public init(_ string: Swift.String) {
        // rdar://162517354 prevents us from simply writing
        // self = WTF.String.fromUTF8(swiftString.utf8CString.span);
        // Safety - we are guaranteed to get a valid buffer from the Swift
        // string for the duration that we're using it to construct the WTF::String.
        // The WTF::String will take a copy.
        let span = string.utf8CString.span
        self = unsafe span.withUnsafeBufferPointer { ptr in
            // Warning here is rdar://163018821
            // swift-format-ignore: NeverForceUnwrap
            let cppspan = unsafe SpanConstChar(ptr.baseAddress!, span.count)
            return unsafe WTF.String.fromUTF8(cppspan)
        }
    }

    /// Return a `Swift.String` from this `WTF.String`.
    public var description: Swift.String {
        // We could possibly make this quicker by treating a C++ span as
        // a Sequence. For now, we want to avoid unsafe as much as possible.
        String(utf8(WTF.LenientConversion).toStdString())
    }
}

extension WTF.String: ExpressibleByStringLiteral {
    /// Construct a `WTF.String` from a string literal.
    public init(stringLiteral: Swift.String) {
        self.init(stringLiteral)
    }
}

extension CxxRefVector {
    init(array: [Element.Pointee]) {
        var vec = Self()
        vec.reserveCapacity(array.count)
        for item in array {
            vec.append(consuming: Element(item))
        }
        self = vec
    }
}

// Iterator for WTF::Vectors of Ref types.
// rdar://169297366 when fixed will conform WTF::Vector directly to Sequence.
// We can't do that manually since this would require C++ interop types to be public
struct CxxRefVectorIterator<Vec: CxxRefVector>: Sequence, IteratorProtocol {
    typealias Element = Vec.Element
    var vec: Vec
    var pos: Int

    init(vec: Vec) {
        self.vec = vec
        self.pos = 0
    }

    mutating func next() -> Vec.Element? {
        if pos >= vec.size() {
            return nil
        }
        // Safety: we'll make a copy of the referent
        // before the vector goes out of scope. It's guaranteed
        // to have a valid lifetime, be initialized, and be
        // within the vector bounds.
        let item = unsafe vec.__atUnsafe(pos)
        pos += 1
        return unsafe item.pointee.copyRef()
    }
}

#endif // ENABLE_BACK_FORWARD_LIST_SWIFT

#endif // compiler(>=6.2)

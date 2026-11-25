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

import WebKit_Internal
import wtf

// MARK: Swift.String <-> WTF::StringImpl

// WTF::StringImpl is a Swift shared reference type (retained with ref(), released with deref()), so Swift passes
// StringImpl* parameters as `WTF.StringImpl?` and gets one from a RefPtr<StringImpl> with `.get()`.
// These only need wtf, so they are defined outside the ENABLE_BACK_FORWARD_LIST_SWIFT guard below.
// FIXME (rdar://164119356): Move these into WTF once WTF has Swift sources.

extension WTF.StringImpl {
    /// Creates a `StringImpl` with the contents of `string`.
    ///
    /// Only `nil` if allocation fails; the optional mirrors C++ `RefPtr<StringImpl>`, and passes straight to
    /// `StringImpl*` parameters.
    static func create(from string: Swift.String) -> WTF.StringImpl? {
        // FIXME: Like the WTF.String initializer this replaces, the Swift >= 6.4 branch includes the UTF-8 null
        // terminator, so the result ends in U+0000 (and an empty Swift string becomes "\0"). Upstream fixed only the
        // older-compiler branch (326293@main); the other is kept as is to preserve behavior.
        // Safety: the buffer is valid for the duration of the closure, and fromUTF8() copies it.
        #if compiler(>=6.4)
        string.utf8CString.withUnsafeBufferPointer { characters in
            unsafe characters.withMemoryRebound(to: UInt8.self) { bytes in
                // swift-format-ignore: NeverForceUnwrap
                unsafe WTF.StringImpl.fromUTF8(WTF.SpanUInt8(bytes.baseAddress!, bytes.count)).get()
            }
        }
        #else
        unsafe string.utf8CString.withUnsafeBufferPointer { characters in
            unsafe characters.withMemoryRebound(to: UInt8.self) { bytes in
                // swift-format-ignore: NeverForceUnwrap
                unsafe WTF.StringImpl.fromUTF8(WTF.SpanUInt8(bytes.baseAddress!, bytes.count - 1)).get()
            }
        }
        #endif
    }
}

extension Swift.String {
    /// Creates a `Swift.String` from a `StringImpl`; `nil` (a null WTF string) becomes the empty string.
    init(_ string: WTF.StringImpl?) {
        self.init(WTF.utf8StdString(string))
    }
}

// FIXME (rdar://164119356): Move parts of StdLibExtras.swift into WTF
// (those parts which are not specific to WebKit-level types) - and enable
// irrespective of BACK_FORWARD_LIST_SWIFT
#if ENABLE_BACK_FORWARD_LIST_SWIFT

/// Conform any WTF::Ref<T> to this protocol to get useful extensions.
protocol CxxRef {
    associatedtype Pointee // you only need to specify this in your conformance
    init(_ object: Pointee)
    func copyRef() -> Self
}

/// Conform any WTF::Vector<T> to this protocol to get useful extensions and iterators.
protocol CxxVector {
    associatedtype Element // you only need to specify this in your conformance
    init()
    mutating func append(consuming: Element)
    mutating func reserveCapacity(_ newCapacity: Int)
    func size() -> Int
    // swift-format-ignore: AlwaysUseLowerCamelCase, NoLeadingUnderscores
    func __atUnsafe(_ index: Int) -> UnsafePointer<Element>
}

/// Conform any WTF::Vector<WTF::Ref<T>> to this protocol to get useful extensions and iterators.
protocol CxxRefVector: CxxVector where Element: CxxRef {
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

#endif // ENABLE_BACK_FORWARD_LIST_SWIFT

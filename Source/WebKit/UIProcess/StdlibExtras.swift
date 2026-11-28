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

// FIXME (rdar://164119356): Move these protocols into WTF. Conforming WebKit's C++ types to public WTF protocols
// would make those types part of WebKit's public interface, so WTF::Ref and WTF::Vector would have to conform in C++
// with SWIFT_CONFORMS_TO_PROTOCOL instead. WTF's CxxVector would also need another name, since Swift's Cxx module
// has a CxxVector protocol of its own.
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

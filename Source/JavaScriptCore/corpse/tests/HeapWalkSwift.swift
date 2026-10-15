// Copyright (C) 2026 Igalia S.L.
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

#if JSC_SUPPORTS_SWIFT
public import Foundation
public import HeapWalkModule

/// A Swift class whose objects C++ holds through Swift's C++ interoperability.
///
/// Its Clang types come from modules, so this image's debug info describes each in its module's compile unit.
@_expose(Cxx)
public final class HeapWalkSwiftObject {
    /// A scalar field.
    public var value = 0
    /// A field of a type the CoreFoundation module defines.
    public var range = CFRange(location: 0, length: 1)
    /// A field of a type the HeapWalkModule module defines.
    public var moduleStruct = HeapWalkModuleStruct(range: CFRange(location: 0, length: 2), value: 3)
    /// A field of a C++ class the HeapWalkModule module defines.
    public var moduleClass = HeapWalkModuleClass()

    /// The length of the range the C++ class holds.
    public var moduleClassLength: Int { moduleClass.rangeLength() }

    /// Creates an object with every field set.
    public init() {}
}

/// A Swift value of fixed layout, which C++ holds inline.
@_expose(Cxx)
public struct HeapWalkSwiftValue {
    /// An array, which the value holds by reference.
    public var values = [1, 2, 3]
    /// A string, which the value holds inline.
    public var name = "heap walk"
}

/// Creates an object for C++ to hold.
@_expose(Cxx)
public func makeHeapWalkSwiftObject() -> HeapWalkSwiftObject {
    HeapWalkSwiftObject()
}

/// Creates a value for C++ to hold.
@_expose(Cxx)
public func makeHeapWalkSwiftValue() -> HeapWalkSwiftValue {
    HeapWalkSwiftValue()
}
#endif

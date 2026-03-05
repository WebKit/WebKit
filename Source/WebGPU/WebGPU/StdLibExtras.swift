// Copyright (C) 2025 Apple Inc. All rights reserved.
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

// FIXME (rdar://164119356): Move StdLibExtras.swift from WebGPU to WTF

private import CxxStdlib
internal import WebGPU_Private.WebGPU

// FIXME (rdar://162375123): This should be in the standard library.
extension MutableSpan where Element: BitwiseCopyable {
    @_lifetime(self: copy self)
    mutating func copyMemory(from source: Span<Element>) {
        // Safety: This is lifetime safe because we have exclusive access to 'self' and we don't escape 'selfBuffer'
        unsafe withUnsafeMutableBufferPointer { selfBuffer in
            // Safety: This is lifetime safe because we have exclusive access to 'source' and we don't escape 'sourceBuffer'
            unsafe source.withUnsafeBufferPointer { sourceBuffer in
                // Safety: This is bounds safe because we do a manual bounds check
                // Safety: This is type safe because we statically declare that our element types match and are BitwiseCopyable
                precondition(sourceBuffer.count <= selfBuffer.count)
                _ = unsafe memcpy(selfBuffer.baseAddress, sourceBuffer.baseAddress, sourceBuffer.count)
            }
        }
    }
}

// FIXME(rdar://130765784): We should be able use the built-in ===, but AnyObject currently excludes foreign reference types
func === (_ lhs: WGPUTexture, _ rhs: WGPUTexture) -> Bool {
    // Safety: Swift represents all reference types, including foreign reference types, as raw pointers
    unsafe unsafeBitCast(lhs, to: UnsafeRawPointer.self) == unsafeBitCast(rhs, to: UnsafeRawPointer.self)
}

extension Comparable {
    /// Returns this comparable value clamped to the given limiting range.
    ///
    /// - Parameter limits: The range to clamp the bounds of this value.
    /// - Returns: A value guaranteed to be in the range `[limits.lowerBound, limits.upperBound]`
    func clamped(to limits: ClosedRange<Self>) -> Self {
        min(max(self, limits.lowerBound), limits.upperBound)
    }
}

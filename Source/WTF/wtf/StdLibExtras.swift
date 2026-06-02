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

extension Comparable {
    /// Returns this comparable value clamped to the given limiting range.
    ///
    /// - Parameter limits: The range to clamp the bounds of this value.
    /// - Returns: A value guaranteed to be in the range `[limits.lowerBound, limits.upperBound]`
    public func clamped(to limits: ClosedRange<Self>) -> Self {
        min(max(self, limits.lowerBound), limits.upperBound)
    }
}

extension MutableSpan where Element: BitwiseCopyable {
    /// Copies the memory from `source` into this span.
    ///
    /// - Parameter source: The span to copy memory from. The `count` of `source` must not be greater than the `count` of `self`.
    @_lifetime(self: copy self)
    public mutating func copyMemory(from source: Span<Element>) {
        // Safety: This is lifetime safe because we have exclusive access to 'self' and we don't escape 'selfBuffer'
        unsafe withUnsafeMutableBytes { selfBuffer in
            // Safety: This is lifetime safe because we have exclusive access to 'source' and we don't escape 'sourceBuffer'
            unsafe source.withUnsafeBytes { sourceBuffer in
                // Safety: This is bounds safe because we do a manual bounds check
                // Safety: This is type safe because we statically declare that our element types match and are BitwiseCopyable
                precondition(sourceBuffer.count <= selfBuffer.count)
                unsafe selfBuffer.copyMemory(from: sourceBuffer)
            }
        }
    }
}

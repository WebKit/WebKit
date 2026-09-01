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

#if os(Windows)
// FIXME: (rdar://185504483) conflict with Windows Swift Foundation's ICU
import FoundationEssentials

typealias URL = FoundationEssentials.URL
#else
import Foundation

typealias URL = Foundation.URL
#endif

typealias String = Swift.String

struct UncheckedSendableKeyPathBox<Root, Value>: @unchecked Sendable {
    let keyPath: KeyPath<Root, Value>
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

/// A type that can be used to uniquely own an instance of `Value` while being copyable.
final class CopyableBox<Value: ~Copyable> {
    /// The value contained in this copyable box.
    var value: Value?

    /// Initializes a value of this copyable box with the given value.
    ///
    /// - Parameter value: The value to initialize the copyable box with.
    init(value: consuming Value) {
        self.value = consume value
    }

    /// Consumes the box's value and returns the instance of Value that was within the box.
    ///
    /// - Returns: The value in this box.
    func take() -> Value? {
        value.take()
    }
}

// Copyright (C) 2024 Apple Inc. All rights reserved.
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

import Foundation
import struct Foundation.URL
import struct Swift.String

extension RangeReplaceableCollection {
    /// Asynchronously converts an AsyncSequence to a non-async Sequence.
    ///
    /// - Parameters:
    ///   - sequence: The async sequence to convert.
    ///   - isolation: The current isolation context.
    /// - Throws: An error of type `Failure` if the async sequence throws.
    public init<Failure>(
        _ sequence: some AsyncSequence<Element, Failure>,
        isolation: isolated (any Actor)? = #isolation
    ) async throws(Failure) where Failure: Error {
        self.init()

        for try await element in sequence {
            append(element)
        }
    }
}

extension AsyncSequence {
    /// Waits for the current sequence to terminate or throw a failure.
    ///
    /// If the sequence is indefinite, this function will never return.
    public func wait(isolation: isolated (any Actor)? = #isolation) async throws(Failure) {
        for try await _ in self {
        }
    }
}

extension StringProtocol {
    /// Finds and returns the range of the first occurrence of a given string within a given range of the String, subject to given options, using the specified locale, if any.
    ///
    /// - Parameters:
    ///   - aString: The string to search for.
    ///   - mask: A mask specifying search options.
    ///   - searchRange: The range within the receiver for which to search for `aString`.
    ///   - locale: The locale to use when comparing the receiver with `aString`.
    /// - Returns: The range that `aString` is located in, represented as a quantity of UTF-16 code points.
    public func utf16Range(
        of aString: some StringProtocol,
        options mask: String.CompareOptions = [],
        range searchRange: Range<Self.Index>? = nil,
        locale: Locale? = nil
    ) -> Range<Int>? {
        guard let nominalRange = range(of: aString, options: mask, range: searchRange, locale: locale) else {
            return nil
        }

        let start = nominalRange.lowerBound.utf16Offset(in: self)
        let end = nominalRange.upperBound.utf16Offset(in: self)

        return start..<end
    }
}

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

#if compiler(>=6.2)

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

        // Safety: this is actually safe; false positive is rdar://154775389
        for try await unsafe element in sequence {
            append(element)
        }
    }
}

extension AsyncSequence {
    /// Waits for the current sequence to terminate or throw a failure.
    ///
    /// If the sequence is indefinite, this function will never return.
    public func wait(isolation: isolated (any Actor)? = #isolation) async throws(Failure) {
        // Safety: this is actually safe; false positive is rdar://154775389
        for try await unsafe _ in self {
        }
    }
}

/// Repeatedly invokes a condition until it evaluates to true or until a timeout has been reached.
///
/// - Parameters:
///   - timeout: The timeout to wait until before exiting this function and throwing an Error.
///   - interval: The duration to wait between consecutive evaluations of the condition
///   - function: The function of the source location where this is called.
///   - line: The line number of the source location where this is called.
///   - condition: The predicate condition to evaluate.
/// - Throws: An Error if the condition throws an Error, or if the timeout duration is reached.
public nonisolated(nonsending) func waitUntil(
    timeout: Duration = .seconds(10),
    interval: Duration = .milliseconds(100),
    function: StaticString = #function,
    line: Int = #line,
    condition: () async throws -> Bool,
) async throws {
    let deadline = ContinuousClock.now + timeout
    while ContinuousClock.now < deadline {
        if try await condition() {
            return
        }
        try await Task.sleep(for: interval)
    }
    throw CancellationError()
}

#endif // compiler(>=6.2)

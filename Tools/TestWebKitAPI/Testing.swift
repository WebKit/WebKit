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

import Foundation

#if compiler(>=6.2)

/// A set of facilities to mimic basic Swift Testing support, until Swift Testing can be directly used.
public enum Testing {
    /// The cause of a test failure.
    public struct Error: Swift.Error {
        /// A descriptive message of why the error occurred.
        public let message: Swift.String

        /// The name of the function where the error occurred.
        public let function: StaticString

        /// The line number in the file where the error occurred.
        public let line: Int

        init(_ message: Swift.String, function: StaticString = #function, line: Int = #line) {
            self.message = message
            self.function = function
            self.line = line
        }
    }

    /// Unwrap an optional value or, if it is `nil`, fail and throw an error.
    ///
    /// - Parameters:
    ///   - optionalValue: The optional value to be unwrapped.
    ///   - comment: A comment describing the expectation.
    ///   - function: The function of the source location where this is called.
    ///   - line: The line number of the source location where this is called.
    /// - Returns: The unwrapped value of `optionalValue`.
    /// - Throws: An Error if the value is `nil`
    public static func require<T>(
        _ optionalValue: T?,
        _ comment: @autoclosure () -> Swift.String? = nil,
        function: StaticString = #function,
        line: Int = #line,
    ) throws(Error) -> T {
        guard let optionalValue else {
            let resolvedComment = comment()
            throw Error("Found nil value of type \(T.self) : \(resolvedComment ?? "")", function: function, line: line)
        }

        return optionalValue
    }

    /// Check that an expectation has passed after a condition has been evaluated.
    ///
    /// If the equality condition evaluates to false, an Error is thrown.
    ///
    /// - Parameters:
    ///   - actualValue: The actual value to compare.
    ///   - expectedValue: The expected value to compare.
    ///   - comment: A comment describing the expectation.
    ///   - function: The function of the source location where this is called.
    ///   - line: The line number of the source location where this is called.
    /// - Throws: An error if the values are not equal.
    public static func expect<T>(
        _ actualValue: T?,
        toEqual expectedValue: T?,
        _ comment: @autoclosure () -> Swift.String? = nil,
        function: StaticString = #function,
        line: Int = #line,
    ) throws(Error) where T: Equatable {
        guard actualValue == expectedValue else {
            let resolvedComment = comment()
            throw Error(
                "Expected \(Swift.String(describing: actualValue)) to equal \(Swift.String(describing: expectedValue)) : \(resolvedComment ?? "")",
                function: function,
                line: line
            )
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
    public nonisolated(nonsending) static func waitUntil(
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
        throw Error("Timed out.", function: function, line: line)
    }
}

#endif // compiler(>=6.2)

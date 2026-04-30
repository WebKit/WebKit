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

#if ENABLE_SWIFTUI

public import WebKit

extension WebPage {
    /// A type that can encode itself to a JavaScript JSON value representation.
    public protocol JavaScriptEncodable {
        /// Encodes this value as a JSON value.
        ///
        /// - Returns: A representation of this value as encoded in JSON.
        func encoded() -> [String: Any?]
    }
}

extension WebPage {
    /// A type that can decode itself from a JavaScript JSON value representation.
    public protocol JavaScriptDecodable {
        /// Decodes a value from a JSON value.
        ///
        /// - Parameter decodedRepresentation: A representation of this value as encoded in JSON.
        init?(decodedRepresentation: [String: Any?])
    }
}

extension WebPage {
    /// A type that can be used by a `WebPage` to be evaluated by JavaScript.
    public protocol JavaScriptExpression: JavaScriptEncodable {
        /// The return type of this expression. If the expression does not return anything, this is `Void`.
        associatedtype Output: Sendable

        /// The JavaScript literal function body for this expression.
        ///
        /// The function body may access the properties of this type as if they were arguments to the JavaScript function.
        static var expression: String { get }
    }
}

extension WebPage {
    /// An error representing a failure when evaluating JavaScript.
    public enum JavaScriptEvaluationError: Error {
        /// An unexpected error occurred.
        case unexpectedResult(String)

        /// The JavaScript expression returned a result when none was expected.
        case noResult

        /// The expression returned a value of an unexpected type.
        case mismatchedType(String)

        /// The JavaScript output type failed to decode.
        case decodingFailure(String)
    }
}

extension WebPage {
    /// Evaluates the provided JavaScript expression.
    ///
    /// - Parameter expression: The expression to evaluate.
    /// - Throws: An error if the JavaScript evaluation or decoding fails.
    public func callJavaScript<Expression>(
        _ expression: Expression
    ) async throws where Expression: JavaScriptExpression, Expression.Output == Void {
        let arguments = expression.encoded() as [String: Any]
        let result = try await self.callJavaScript(Expression.expression, arguments: arguments)

        if let result {
            throw JavaScriptEvaluationError.unexpectedResult("expected no result, got \(result)")
        }
    }

    /// Evaluates the provided JavaScript expression.
    ///
    /// - Parameter expression: The expression to evaluate.
    /// - Returns: The result of evaluating the expression.
    /// - Throws: An error if the JavaScript evaluation or decoding fails.
    public func callJavaScript<Expression>(
        _ expression: Expression
    ) async throws -> Expression.Output where Expression: JavaScriptExpression, Expression.Output: JavaScriptDecodable {
        let arguments = expression.encoded() as [String: Any]
        let result = try await self.callJavaScript(Expression.expression, arguments: arguments)

        guard let result else {
            throw JavaScriptEvaluationError.noResult
        }

        guard let dictionaryResult = result as? [String: Any?] else {
            throw JavaScriptEvaluationError.mismatchedType("expected dictionary JS result")
        }

        guard let decodedResult = Expression.Output.init(decodedRepresentation: dictionaryResult) else {
            throw JavaScriptEvaluationError.decodingFailure("failed to decode result")
        }

        return decodedResult
    }
}

#endif // ENABLE_SWIFTUI

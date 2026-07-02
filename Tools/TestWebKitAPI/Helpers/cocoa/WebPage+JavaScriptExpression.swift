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
public import struct Swift.String

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
        case scriptError(underlyingError: any Error)

        /// The JavaScript expression returned a result when none was expected.
        case noResult

        /// The expression returned a value of an unexpected type.
        case mismatchedType(expected: Any.Type, actual: Any.Type)

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
    ) async throws(JavaScriptEvaluationError) where Expression: JavaScriptExpression, Expression.Output == Void {
        let arguments = expression.encoded() as [String: Any]
        let result: Any?
        do {
            result = try await self.callJavaScript(Expression.expression, arguments: arguments)
        } catch {
            throw .scriptError(underlyingError: error)
        }

        if let result {
            throw .mismatchedType(expected: Expression.Output.self, actual: type(of: result))
        }
    }

    /// Evaluates the provided JavaScript expression.
    ///
    /// - Parameter expression: The expression to evaluate.
    /// - Returns: The result of evaluating the expression.
    /// - Throws: An error if the JavaScript evaluation or decoding fails.
    public func callJavaScript<Expression>(
        _ expression: Expression
    ) async throws(JavaScriptEvaluationError) -> Expression.Output
    where Expression: JavaScriptExpression, Expression.Output: JavaScriptDecodable {
        let arguments = expression.encoded() as [String: Any]
        let result: Any?
        do {
            result = try await self.callJavaScript(Expression.expression, arguments: arguments)
        } catch {
            throw .scriptError(underlyingError: error)
        }

        guard let result else {
            throw .noResult
        }

        guard let dictionaryResult = result as? [String: Any?] else {
            throw .mismatchedType(expected: [String: Any?].self, actual: type(of: result))
        }

        guard let decodedResult = Expression.Output(decodedRepresentation: dictionaryResult) else {
            throw .decodingFailure("failed to decode result")
        }

        return decodedResult
    }

    /// Executes the specified string as an async JavaScript function.
    ///
    /// - Parameters:
    ///   - returnType: The type the expression returns.
    ///   - arguments: A dictionary of the arguments to pass to the function call.
    ///   - script: The JavaScript string to use as the function body.
    /// - Returns: The result of the script evaluation. If the type of the result is not the type of `returnType`, an error is thrown.
    /// - Throws: A `JavaScriptEvaluationError` error if there was a problem evaluating the script, or if a serialization failure occurred.
    public func callJavaScript<Result>(
        returning returnType: Result.Type,
        arguments: [String: Any] = [:],
        script: () -> String
    ) async throws(JavaScriptEvaluationError) -> Result {
        let result: Any?
        do {
            result = try await callJavaScript(script(), arguments: arguments)
        } catch {
            throw .scriptError(underlyingError: error)
        }

        guard let result else {
            throw .noResult
        }

        guard let result = result as? Result else {
            throw .mismatchedType(expected: Result.self, actual: type(of: result))
        }

        return result
    }

    /// Executes the specified string as an async JavaScript function.
    ///
    /// - Parameters:
    ///   - returnType: The type the expression returns.
    ///   - arguments: A dictionary of the arguments to pass to the function call.
    ///   - script: The JavaScript string to use as the function body.
    /// - Returns: The result of the script evaluation. If the type of the result is not the type of `returnType`, an error is thrown.
    /// - Throws: A `JavaScriptEvaluationError` error if there was a problem evaluating the script, or if a serialization failure occurred.
    public func callJavaScript<First, each Rest, Last>(
        returning returnType: (First, repeat each Rest, Last).Type,
        arguments: [String: Any] = [:],
        script: () -> String
    ) async throws(JavaScriptEvaluationError) -> (First, repeat each Rest, Last) {
        let result: Any?
        do {
            result = try await callJavaScript(script(), arguments: arguments)
        } catch {
            throw .scriptError(underlyingError: error)
        }

        guard let result else {
            throw .noResult
        }

        guard let array = result as? [Any] else {
            throw .mismatchedType(expected: [Any].self, actual: type(of: result))
        }

        return createTuple(of: returnType, from: array)
    }

    /// Executes the specified string as an async JavaScript function.
    ///
    /// - Parameters:
    ///   - returnType: The type the expression returns.
    ///   - arguments: A dictionary of the arguments to pass to the function call.
    ///   - script: The JavaScript string to use as the function body.
    /// - Returns: The result of the script evaluation. If the type of the result is not the type of `returnType`, an error is thrown.
    /// - Throws: A `JavaScriptEvaluationError` error if there was a problem evaluating the script, or if a serialization failure occurred.
    public func callJavaScript<Result>(
        returning returnType: Result?.Type,
        arguments: [String: Any] = [:],
        script: () -> String
    ) async throws(JavaScriptEvaluationError) -> Result? {
        let result: Any?
        do {
            result = try await callJavaScript(script(), arguments: arguments)
        } catch {
            throw .scriptError(underlyingError: error)
        }

        guard let result else {
            return nil
        }

        guard let result = result as? Result else {
            throw .mismatchedType(expected: Result.self, actual: type(of: result))
        }

        return result
    }

    /// Executes the specified string as an async JavaScript function.
    ///
    /// - Parameters:
    ///   - returnType: The type the expression returns.
    ///   - arguments: A dictionary of the arguments to pass to the function call.
    ///   - script: The JavaScript string to use as the function body.
    /// - Throws: A `JavaScriptEvaluationError` error if there was a problem evaluating the script, or if a serialization failure occurred.
    public func callJavaScript(
        returning returnType: Void.Type = Void.self,
        arguments: [String: Any] = [:],
        script: () -> String
    ) async throws(JavaScriptEvaluationError) {
        let result: Any?
        do {
            result = try await callJavaScript(script(), arguments: arguments)
        } catch {
            throw .scriptError(underlyingError: error)
        }

        guard result == nil else {
            throw .mismatchedType(expected: returnType, actual: type(of: result))
        }
    }
}

#endif // ENABLE_SWIFTUI

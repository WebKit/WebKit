/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/// A family of conversions for translating between Swift blocks expecting a `Result<V, Error>` and
/// Objective-C callbacks of the form `(T?, Error?)`.
///
/// Depending on the semantics of the Objective-C API, only one of these conversions is appropriate.
/// - If the Objective-C block expects to be called with exactly one non-null argument, use the
/// `exclusive(_:)` conversion.
/// - If the Objective-C block can be called with one or zero non-null arguments, use the
/// `treatNilAsSuccess` conversion.
/// - If the Objective-C block can be called with two non-null values, it is ineligible for
/// conversion to a `Result`.
/// - `boxingNilAsAnyForCompatibility` exists as a workaround for http://webkit.org/b/216198, and
/// should not be used by new code.
enum ObjCBlockConversion {
    /// Converts a block from `(Result<Value, Error>) -> Void` to `(Value?, Error?) -> Void`.
    ///
    /// The result block must be called with exactly one non-null argument. If both arguments are
    /// non-null then `handler` will be called with `.success(T)`. If both arguments are `nil`
    /// the conversion will trap.
    static func exclusive<Value>(_ handler: @escaping (Result<Value, Error>) -> Void) -> (Value?, Error?) -> Void {
        return { value, error in
            if let value = value {
                handler(.success(value))
            } else if let error = error {
                handler(.failure(error))
            } else {
                preconditionFailure("Bug in WebKit: Received neither result or failure.")
            }
        }
    }

    /// Converts a block from `(Result<Value?, Error>) -> Void` to `(Value?, Error) -> Void`.
    ///
    /// This performs the same conversion as `Self.exclusive(_:)`, but if the result block is called
    /// with `(nil, nil)` then `handler` is called with `.success(nil)`.
    static func treatNilAsSuccess<Value>(_ handler: @escaping (Result<Value?, Error>) -> Void) -> (Value?, Error?) -> Void {
        return { value, error in
            if let error = error {
                handler(.failure(error))
            } else {
                handler(.success(value))
            }
        }
    }

    /// Converts a block from `(Result<Value, Error>) -> Void` to `(Value?, Error) -> Void`.
    ///
    /// This performs the same conversion as `Self.exclusive(_:)`, but if the result block is called
    /// with `(nil, nil)` then `handler` is called with `.success(Optional<Any>.none as Any)`. This
    /// is a compatibility behavior for http://webkit.org/b/216198, and should not be adopted by
    /// new code.
    static func boxingNilAsAnyForCompatibility(_ handler: @escaping (Result<Any, Error>) -> Void) -> (Any?, Error?) -> Void {
        return { value, error in
            if let error = error {
                handler(.failure(error))
            } else if let success = value {
                handler(.success(success))
            } else {
                handler(.success(Optional<Any>.none as Any))
            }
        }
    }
}

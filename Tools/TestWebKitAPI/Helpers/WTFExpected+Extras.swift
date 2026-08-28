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

// FIXME: (rdar://164119356) Move this file into WTF.

#if compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN

/// Represents an `unexpected` value of an `std::expected<T, E>`.
public struct CxxUnexpected<Failure: Sendable>: Error {
    /// The unexpected error.
    public let error: Failure
}

/// The machinery shared by ``CxxExpected`` and ``CxxConsumingExpected``.
///
/// - Important: Do not conform to this directly. Conform to one of those two instead.
public protocol CxxExpectedBase: ~Copyable {
    /// The type of the expected value.
    associatedtype Value: ~Copyable

    /// The type of the unexpected value.
    associatedtype Failure: Sendable

    /// Checks whether the object contains an expected value.
    /// - Returns: `true` if the object contains an expected value; `false` otherwise.
    // swift-format-ignore: AlwaysUseLowerCamelCase
    func has_value() -> Bool

    /// Returns the expected value.
    ///
    /// - Note: Do not call or implement this yourself. It is unsafe to do so.
    // swift-format-ignore: AlwaysUseLowerCamelCase,NoLeadingUnderscores
    func __valueUnsafe() -> UnsafePointer<Value>

    /// Returns the unexpected value.
    ///
    /// - Note: Do not call or implement this yourself. It is unsafe to do so.
    // swift-format-ignore: AlwaysUseLowerCamelCase,NoLeadingUnderscores
    func __errorUnsafe() -> UnsafePointer<Failure>
}

/// A protocol for concrete specializations of `std::expected<T, E>` to conform to when `T` is `Copyable`.
///
/// Conforming a specialization to this protocol allows Swift to safely access properties of the `expected`.
/// For example, this makes it possible to use a type like
///
/// ```cpp
/// using ExpectedResult = std::expected<int, ErrorCode>;
/// ```
///
/// by creating this conformance:
///
/// ```swift
/// extension ExpectedResult: CxxExpected {}
/// ```
///
/// Swift can then use the `expected` naturally:
///
/// ```swift
/// let expectedResult: ExpectedResult = ...
/// let value = try expectedResult.value
/// ```
///
public protocol CxxExpected: CxxExpectedBase where Value: Copyable {
}

extension CxxExpected {
    /// Returns the expected value, or throws the unexpected error if one exists.
    public var value: Value {
        get throws(CxxUnexpected<Failure>) {
            // Safety properties:
            //
            // Non-null, initialized, aligned: both point into `self`, which is a fully constructed
            //     `std::expected`, so each is the address of a live object of the type it points to.
            // Lifetime: `self` is borrowed for the whole of this getter, so neither pointer can dangle
            //     here. Neither is stored; only the copy taken below escapes.
            // Aliasing: `self` still owns what both point at, so this may only read them. `.pointee`
            //     copies, running `Value`'s or `Failure`'s copy constructor, and leaves the `Expected`
            //     intact and readable again.

            guard has_value() else {
                throw unsafe CxxUnexpected(error: __errorUnsafe().pointee)
            }

            return unsafe __valueUnsafe().pointee
        }
    }
}

/// A protocol for concrete specializations of `std::expected<T, E>` for noncopyable `T`s.
///
/// This is the same as ``CxxExpected`` except that it matches a C++ signature like:
///
/// ```cpp
/// using ExpectedResult = std::expected<MoveOnlyThing, ErrorCode>;
/// ```
///
/// Conformers to this protocol must explicitly implement the `__take(_:)` requirement like so:
///
/// ```swift
/// extension ExpectedResult: CxxConsumingExpected {
///     static func __take(_ expected: consuming Self) -> Value {
///         takeValue(consuming: expected)
///     }
/// }
/// ```
///
/// where `takeValue` is a C++ free function implemented exactly as
///
/// ```cpp
/// inline ExpectedResult::value_type takeValue(ExpectedResult&& expected)
/// {
///     // Must be non-trivially destructible so that Swift runs the move constructor rather than
///     // relocating the value byte-wise, which would leave a value holding a pointer into itself
///     // dangling.
///     static_assert(!std::is_trivially_destructible_v<ExpectedResult::value_type>);
///     // Swift copies rather than consumes a copyable `Expected`, so the value would be moved out of
///     // that copy and the caller's `Expected` left untouched.
///     static_assert(!std::is_copy_constructible_v<ExpectedResult>);
///     return WTF::move(*expected);
/// }
/// ```
///
/// Swift imports that rvalue reference parameter as `consuming`, and destroys the moved-from `expected` once the call
/// returns.
///
/// - Important: Do not implement the protocol requirements yourself. It is unsafe if you do so.
public protocol CxxConsumingExpected: CxxExpectedBase, ~Copyable {
    /// Consumes the `expected`, moving its value out.
    ///
    /// - Parameter expected: The expected to take. It is guaranteed to have a value.
    /// - Returns: The moved value of the expected.
    /// - Note: Do not call this yourself.
    // swift-format-ignore: AlwaysUseLowerCamelCase,NoLeadingUnderscores
    static func __take(_ expected: consuming Self) -> Value
}

extension CxxConsumingExpected where Self: ~Copyable, Value: ~Copyable {
    /// Returns the expected value, or throws the unexpected error if one exists.
    ///
    /// - Returns: The consumed value of the expected, if one exists.
    /// - Throws: The unexpected error, if one exists.
    public consuming func consume() throws(CxxUnexpected<Failure>) -> Value {
        // Safety properties:
        //
        // As in `CxxExpected.value`, with one difference: `self` is consumed here rather than borrowed.
        // `__errorUnsafe()` only borrows it, and `__take(_:)` is what finally takes it, so `self` is
        // still live when the error is read out below.

        guard has_value() else {
            throw unsafe CxxUnexpected(error: __errorUnsafe().pointee)
        }

        return Self.__take(self)
    }
}

#endif // compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN

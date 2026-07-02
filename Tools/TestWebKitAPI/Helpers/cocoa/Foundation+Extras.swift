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

public import CoreGraphics
import Foundation
private import Synchronization

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

extension CGRect {
    /// The center point of this rect.
    public var center: CGPoint {
        .init(x: midX, y: midY)
    }
}

/// A type used to model an asynchronous promise, via a Semaphore-like interface.
public struct Future: Sendable, ~Copyable {
    private enum State {
        case initial
        case waiting(CheckedContinuation<Void, Never>)
        case signaled
    }

    private let state = Mutex<State>(.initial)

    /// Create a new Future.
    public init() {
    }

    /// Resolves the promise of this Future.
    public func signal() {
        let continuation = state.withLock { state -> CheckedContinuation<Void, Never>? in
            defer {
                state = .signaled
            }

            if case .waiting(let continuation) = state {
                return continuation
            }

            return nil
        }
        continuation?.resume()
    }

    /// Waits for the promise of this Future to be resolved.
    public func wait() async {
        await withCheckedContinuation { continuation in
            let resumeImmediately = state.withLock { state in
                switch state {
                case .signaled:
                    return true

                case .initial:
                    state = .waiting(continuation)
                    return false

                case .waiting:
                    preconditionFailure("Future only supports a single waiter")
                }
            }

            if resumeImmediately {
                continuation.resume()
            }
        }
    }
}

/// Temporarily installs a block-based implementation for an Objective-C instance method.
///
/// Runs `body` while the swap is in effect, then restores the original implementation
/// before returning. The original is restored even if `body` throws.
///
/// The block's first parameter must be the receiver, followed by the method's arguments.
/// Declare it with `@convention(block)` so the Objective-C runtime can bridge it to an IMP.
///
/// ```swift
/// let implementation: @convention(block) (NSPasteboard, Date) -> Bool = { _, _ in true }
/// try await withSwizzledObjectiveCInstanceMethod(
///     replacing: NSPasteboard.self,
///     name: #selector(NSPasteboard.canReadItem(withDataConformingToTypes:)),
///     with: implementation
/// ) {
///     // Code under test runs here with the mock in place.
/// }
/// ```
///
/// - Parameters:
///   - class: The class whose instance method will be temporarily replaced.
///   - name: The selector identifying the instance method to swap.
///   - implementation: An `@convention(block)` closure whose signature matches the method.
///   - body: The work to run while the swap is in effect.
/// - Returns: The value returned by `body`.
/// - Throws: Rethrows any error thrown by `body`.
@discardableResult
public nonisolated(nonsending) func withSwizzledObjectiveCInstanceMethod<Result, Failure>(
    replacing class: AnyClass,
    name: Selector,
    with implementation: Any,
    perform body: () async throws(Failure) -> sending Result
) async throws(Failure) -> sending Result where Result: ~Copyable, Failure: Error {
    guard let targetMethod = unsafe class_getInstanceMethod(`class`, name) else {
        fatalError("\(`class`) does not respond to \(name)")
    }

    let replacementImplementation = unsafe imp_implementationWithBlock(implementation)
    let originalImplementation = unsafe method_setImplementation(targetMethod, replacementImplementation)
    defer {
        unsafe method_setImplementation(targetMethod, originalImplementation)
        unsafe imp_removeBlock(replacementImplementation)
    }

    return try await body()
}

/// Temporarily replaces the implementation of an Objective-C class method with a custom block implementation for the lifetime of `body`.
///
/// For example, given a type
///
/// ```swift
/// @objc public class MyObjectiveCType: NSObject {
///     dynamic class func add(a: Int, b: Int) -> Int {
///         a + b
///     }
/// }
/// ```
///
/// then the implementation of `add` can be temporarily replaced:
///
/// ```swift
/// let newImplementation = @convention(block) (MyObjectiveCType.Type, Int, Int) -> Int = { _, a, b in
///     a - b
/// }
///
/// let result = withSwizzledObjectiveCClassMethod(
///     class: MyObjectiveCType.self,
///     replacing: #selector(MyObjectiveCType.add(a:b:)),
///     with: newImplementation
/// ) {
///     MyObjectiveCType.add(a: 5, b: 3)
/// }
///
/// // result is now `2`.
/// ```
///
/// - Parameters:
///   - class: The class whose selector should be replaced.
///   - selector: The selector to be replaced.
///   - implementationBlock: A block that will be used as the new implementation.
///   - body: The code to execute with the replaced implementation.
/// - Throws: Any error thrown by `body`.
/// - Returns: The return value of `body`.
/// - Note: The signature of `implementationBlock` must match exactly; if it does not, undefined behavior will occur.
@discardableResult
nonisolated(nonsending) public func withSwizzledObjectiveCClassMethod<Result, Failure>(
    class: AnyClass,
    replacing selector: Selector,
    with implementationBlock: Any,
    perform body: nonisolated(nonsending) () async throws(Failure) -> sending Result
) async throws(Failure) -> sending Result where Result: ~Copyable, Failure: Error {
    guard let method = unsafe class_getClassMethod(`class`, selector) else {
        preconditionFailure("failed to get class method")
    }

    let originalImplementation = unsafe method_setImplementation(method, imp_implementationWithBlock(implementationBlock))

    defer {
        unsafe method_setImplementation(method, originalImplementation)
    }

    return try await body()
}

/// Creates a tuple of a statically known type from some arbitrary sequence at runtime.
///
/// It is invalid to create a tuple from a sequence with a differing number of elements, or if the type of the elements do not match those of the tuple.
///
/// - Parameters:
///   - type: The type of the tuple.
///   - sequence: The sequence to create the tuple from.
/// - Returns: A tuple whose elements are equal to those of the sequence and whose type is equal to `type`.
public func createTuple<S, each T>(of type: (repeat each T).Type, from sequence: S) -> (repeat each T) where S: Sequence {
    var iterator = sequence.makeIterator()

    func extract<Element>(_: Element.Type, iterator: inout S.Iterator) -> Element {
        guard let element = iterator.next() else {
            preconditionFailure("The sequence has fewer elements than the type of the tuple.")
        }
        guard let typed = element as? Element else {
            preconditionFailure("Invalid type (expected: \(Element.self))")
        }
        return typed
    }

    return (repeat extract((each T).self, iterator: &iterator))
}

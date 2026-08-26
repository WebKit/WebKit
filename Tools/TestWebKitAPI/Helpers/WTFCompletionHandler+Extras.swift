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

import wtf.Core.CompletionHandler

/// The machinery shared by ``CxxCompletionHandler`` and ``CxxVoidCompletionHandler``.
///
/// - Important: Do not conform to this directly. Conform to one of those two instead.
public protocol CxxCompletionHandlerBase: ~Copyable {
    /// The type of the completion handler's only parameter, or `Void` if it has none.
    ///
    /// State this explicitly, matching the C++ parameter type:
    ///
    /// ```swift
    /// typealias Argument = CInt
    /// ```
    ///
    /// - Note: This cannot be inferred from `WTF::CompletionHandler`'s own type alias, because an imported C++
    ///   class-template member type alias is not accepted as an associated-type witness under
    ///   `MemberImportVisibility`. See rdar://185766398.
    associatedtype Argument: BitwiseCopyable & Sendable

    /// Creates a completion handler from the C ABI a Swift closure reduces to: a function pointer, an opaque context, and
    /// a destructor for that context.
    ///
    /// Every specialization of `WTF::CompletionHandler` already has this constructor, so conformances never implement it
    /// themselves.
    ///
    /// - Parameters:
    ///   - invoke: The function pointer.
    ///   - destroy: The function describing how to destroy the closure.
    ///   - context: An opaque context for the closure.
    /// - Important: Do not implement or call this yourself.
    init(invoke: WTF.SwiftClosureInvoke, destroy: WTF.SwiftClosureDestroy, context: UnsafeMutableRawPointer)
}

/// A protocol for concrete specializations of `WTF::CompletionHandler` that take one argument to conform to.
///
/// Conforming a specialization to this protocol allows Swift to easily call functions that accept `WTF::CompletionHandler` parameters.
/// For example, this makes it possible to call a function like:
///
/// ```cpp
/// using DoSomethingCompletionHandler = WTF::CompletionHandler<void(int)>;
/// void doSomething(DoSomethingCompletionHandler&&);
/// ```
///
/// by creating this conformance:
///
/// ```swift
/// extension DoSomethingCompletionHandler: @unsafe CxxCompletionHandler {
///     typealias Argument = CInt
/// }
/// ```
///
/// Swift can then call this via the `init(_ body: @escaping (Argument) -> Void)` initializer:
///
/// ```swift
/// doSomething(consuming: .init { number in ... })
/// ```
///
/// or by using the convenience continuation initializer:
///
/// ```swift
/// await withCheckedContinuation { continuation in
///     doSomething(consuming: .init(continuation))
/// }
/// ```
///
/// Common specializations like `WTF::VoidCompletionHandler` and `WTF::BoolCompletionHandler` already conform.
///
/// - Important: Do not implement the protocol requirements yourself. It is unsafe if you do so.
public protocol CxxCompletionHandler: CxxCompletionHandlerBase, ~Copyable {
    /// Invokes the completion handler.
    ///
    /// This is implemented by the imported C++ `operator()`, which is what makes the compiler verify that `Argument` matches
    /// the type C++ actually passes.
    ///
    /// - Important: Do not implement this yourself.
    mutating func callAsFunction(_ argument: Argument)
}

/// A protocol for concrete specializations of `WTF::CompletionHandler` that take no argument to conform to.
public protocol CxxVoidCompletionHandler: CxxCompletionHandlerBase, ~Copyable where Argument == Void {
    /// Invokes the completion handler.
    ///
    /// - Important: Do not implement this yourself.
    mutating func callAsFunction()
}

extension CxxCompletionHandlerBase where Self: ~Copyable {
    /// Creates a completion handler that owns `box` and dispatches to it.
    ///
    /// - Parameter box: The box holding the Swift closure to call.
    fileprivate init(box: SwiftClosureBoxBase) {
        // This is all safe because all uses of the raw pointer are encapsulated in the C++ implementation and its lifetime
        // is not modified elsewhere.
        unsafe self.init(
            invoke: { context, argument in
                // This uses `ErasedSwiftClosureBox` and not `SwiftClosureBox<Argument>` because a C function pointer
                // cannot be formed from a closure that captures generic parameters.
                unsafe Unmanaged<SwiftClosureBoxBase>.fromOpaque(context).takeUnretainedValue().call(with: argument)
            },
            destroy: { context in
                unsafe Unmanaged<AnyObject>.fromOpaque(context).release()
            },
            context: unsafe Unmanaged.passRetained(box).toOpaque()
        )
    }
}

extension CxxCompletionHandler where Self: ~Copyable {
    // These functions are safe because its implementation is safe.

    /// Creates a `WTF::CompletionHandler` type from a Swift closure.
    ///
    /// - Parameter body: The Swift closure to use.
    @safe
    public init(_ body: @escaping (Argument) -> Void) {
        self.init(box: SwiftClosureBox(body))
    }

    /// A convenience initializer to create a completion handler from a `CheckedContinuation` directly to make bridging to a Swift `async`
    /// context trivial.
    ///
    /// This should be preferred for most cases.
    ///
    /// - Parameter continuation: A continuation vended by `withCheckedContinuation`.
    @safe
    public init(_ continuation: CheckedContinuation<Argument, some Error>) {
        self.init { continuation.resume(returning: $0) }
    }
}

extension CxxVoidCompletionHandler where Self: ~Copyable {
    // These functions are safe because its implementation is safe.

    /// Creates a `WTF::CompletionHandler` type from a Swift closure.
    ///
    /// - Parameter body: The Swift closure to use.
    @safe
    public init(_ body: @escaping () -> Void) {
        self.init(box: SwiftVoidClosureBox(body))
    }

    /// A convenience initializer to create a completion handler from a `CheckedContinuation` directly to make bridging to a Swift `async`
    /// context trivial.
    ///
    /// This should be preferred for most cases.
    ///
    /// - Parameter continuation: A continuation vended by `withCheckedContinuation`.
    @safe
    public init(_ continuation: CheckedContinuation<Void, some Error>) {
        self.init { continuation.resume() }
    }
}

private class SwiftClosureBoxBase {
    func call(with argument: UnsafeRawPointer?) {
        preconditionFailure("Subclasses must override call(with:)")
    }
}

private final class SwiftVoidClosureBox: SwiftClosureBoxBase {
    private let body: () -> Void

    init(_ body: @escaping () -> Void) {
        self.body = body
    }

    override func call(with pointer: UnsafeRawPointer?) {
        body()
    }
}

private final class SwiftClosureBox<Argument: BitwiseCopyable & Sendable>: SwiftClosureBoxBase {
    private let body: (Argument) -> Void

    init(_ body: @escaping (Argument) -> Void) {
        self.body = body
    }

    override func call(with pointer: UnsafeRawPointer?) {
        guard let pointer = unsafe pointer else {
            preconditionFailure("A completion handler taking an argument must be passed a pointer to one")
        }

        // This is safe because `pointer` always comes from a value that was originally an `Argument` type.
        let argument = unsafe pointer.load(as: Argument.self)
        body(argument)
    }
}

extension WTF.VoidCompletionHandler: @unsafe CxxVoidCompletionHandler {}

extension WTF.BoolCompletionHandler: @unsafe CxxCompletionHandler {}

#endif // compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN

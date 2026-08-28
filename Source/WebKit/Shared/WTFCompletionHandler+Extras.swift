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

/// The machinery shared by ``CxxCompletionHandler``, ``CxxConsumingCompletionHandler``, and
/// ``CxxVoidCompletionHandler``.
///
/// - Important: Do not conform to this directly. Conform to one of those three instead.
protocol CxxCompletionHandlerBase: ~Copyable {
    /// The type of the completion handler's only parameter, or `Void` if it has none.
    ///
    /// State this explicitly, matching the C++ parameter type:
    ///
    /// ```swift
    /// typealias Argument = CInt
    /// ```
    associatedtype Argument: ~Copyable

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

/// A protocol for concrete specializations of `WTF::CompletionHandler` that take one argument by value to conform to.
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
/// - Note: If the C++ parameter is an rvalue reference (`T&&`) rather than a value, conform to
///   ``CxxConsumingCompletionHandler`` instead.
/// - Important: Do not implement the protocol requirements yourself. It is unsafe if you do so.
protocol CxxCompletionHandler: CxxCompletionHandlerBase, ~Copyable where Argument: Copyable {
    /// Invokes the completion handler.
    ///
    /// This is implemented by the imported C++ `operator()`, which is what makes the compiler verify that `Argument` matches
    /// the type C++ actually passes.
    ///
    /// - Important: Do not implement this yourself.
    mutating func callAsFunction(_ argument: Argument)
}

/// A protocol for concrete specializations of `WTF::CompletionHandler` that take one argument by rvalue reference to
/// conform to.
///
/// This is the same as ``CxxCompletionHandler`` except that it matches a C++ signature like:
///
/// ```cpp
/// using DoSomethingCompletionHandler = WTF::CompletionHandler<void(MoveOnlyThing&&)>;
/// ```
///
/// - Important: Do not implement the protocol requirements yourself. It is unsafe if you do so.
protocol CxxConsumingCompletionHandler: CxxCompletionHandlerBase, ~Copyable {
    /// Invokes the completion handler.
    ///
    /// This is implemented by the imported C++ `operator()`, which is what makes the compiler verify that `Argument` matches
    /// the type C++ actually passes.
    ///
    /// - Important: Do not implement this yourself.
    mutating func callAsFunction(consuming: consuming Argument)
}

/// A protocol for concrete specializations of `WTF::CompletionHandler` that take no argument to conform to.
protocol CxxVoidCompletionHandler: CxxCompletionHandlerBase, ~Copyable where Argument == Void {
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
                // This uses `SwiftClosureBoxBase` and not `SwiftClosureBox<Argument>` because a C function pointer
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
    /// - Parameters:
    ///   - isolation: The current isolation.
    ///   - body: The Swift closure to use.
    @safe
    init(isolation: isolated (any Actor)? = #isolation, _ body: @escaping (Argument) -> Void) {
        self.init(box: SwiftCopyingClosureBox(isolation: isolation, body))
    }
}

extension CxxConsumingCompletionHandler where Self: ~Copyable {
    // These functions are safe because its implementation is safe.

    /// Creates a `WTF::CompletionHandler` type from a Swift closure.
    ///
    /// - Parameters:
    ///   - isolation: The current isolation.
    ///   - body: The Swift closure to use.
    @safe
    init(isolation: isolated (any Actor)? = #isolation, _ body: @escaping (consuming Argument) -> Void) {
        self.init(box: SwiftTakingClosureBox(isolation: isolation, body))
    }
}

extension CxxCompletionHandler where Self: ~Copyable, Argument: Sendable {
    /// A convenience initializer to create a completion handler from a `CheckedContinuation` directly to make bridging to a Swift `async`
    /// context trivial.
    ///
    /// This should be preferred for most cases.
    ///
    /// - Parameter continuation: A continuation vended by `withCheckedContinuation`.
    @safe
    init(_ continuation: CheckedContinuation<Argument, some Error>) {
        self.init { continuation.resume(returning: $0) }
    }
}

extension CxxVoidCompletionHandler where Self: ~Copyable {
    // These functions are safe because its implementation is safe.

    /// Creates a `WTF::CompletionHandler` type from a Swift closure.
    ///
    /// - Parameters:
    ///   - isolation: The current isolation.
    ///   - body: The Swift closure to use.
    @safe
    init(isolation: isolated (any Actor)? = #isolation, _ body: @escaping () -> Void) {
        self.init(box: SwiftVoidClosureBox(isolation: isolation, body))
    }

    /// A convenience initializer to create a completion handler from a `CheckedContinuation` directly to make bridging to a Swift `async`
    /// context trivial.
    ///
    /// This should be preferred for most cases.
    ///
    /// - Parameter continuation: A continuation vended by `withCheckedContinuation`.
    @safe
    init(_ continuation: CheckedContinuation<Void, some Error>) {
        self.init { continuation.resume() }
    }
}

private class SwiftClosureBoxBase {
    private let isolation: (any Actor)?

    init(isolation: (any Actor)?) {
        self.isolation = isolation
    }

    func call(with argument: UnsafeMutableRawPointer?) {
        preconditionFailure("Subclasses must override call(with:)")
    }

    final func checkIsolation() {
        isolation?.preconditionIsolated("A completion handler must be invoked on the isolation that created it")
    }
}

private final class SwiftVoidClosureBox: SwiftClosureBoxBase {
    private let body: () -> Void

    init(isolation: (any Actor)?, _ body: @escaping () -> Void) {
        self.body = body
        super.init(isolation: isolation)
    }

    override func call(with pointer: UnsafeMutableRawPointer?) {
        unsafe precondition(pointer == nil)
        checkIsolation()
        body()
    }
}

/// Copies the argument out of the pointer C++ supplied, leaving C++ to destroy its own object.
private final class SwiftCopyingClosureBox<Argument>: SwiftClosureBoxBase {
    private let body: (Argument) -> Void

    init(isolation: (any Actor)?, _ body: @escaping (Argument) -> Void) {
        self.body = body
        super.init(isolation: isolation)
    }

    override func call(with pointer: UnsafeMutableRawPointer?) {
        guard let pointer = unsafe pointer else {
            preconditionFailure("A completion handler taking an argument must be passed a pointer to one")
        }

        checkIsolation()

        // Safety properties:
        //
        // Non-null, initialized, aligned: guarded above, and C++ passes `std::addressof` of the live
        //     parameter object it was invoked with, so it is the address of a fully constructed object.
        // Really an `Argument`: the `callAsFunction` requirement is witnessed by the imported C++
        //     `operator()`, so a conformance whose `Argument` disagrees with the C++ parameter type does
        //     not compile. The typealias is checked, not merely promised.
        // Lifetime: `invoke` is synchronous, so C++ is blocked inside the call for as long as this runs
        //     and cannot have released the object. This method never stores `pointer`; only the copied
        //     value may outlive the call.
        // Aliasing: C++ still owns that same object, so this may only read it. `.pointee` copies, and
        //     C++ is blocked and so cannot be writing concurrently. Taking the value instead would
        //     leave C++'s object deinitialized behind its back.
        // Isolation: `checkIsolation()` above.

        body(unsafe pointer.assumingMemoryBound(to: Argument.self).pointee)
    }
}

/// Takes the argument out of the pointer C++ supplied.
private final class SwiftTakingClosureBox<Argument: ~Copyable>: SwiftClosureBoxBase {
    private let body: (consuming Argument) -> Void

    init(isolation: (any Actor)?, _ body: @escaping (consuming Argument) -> Void) {
        self.body = body
        super.init(isolation: isolation)
    }

    override func call(with pointer: UnsafeMutableRawPointer?) {
        guard let pointer = unsafe pointer else {
            preconditionFailure("A completion handler taking an argument must be passed a pointer to one")
        }

        checkIsolation()

        // Safety properties:
        //
        // As in `SwiftCopyingClosureBox`, with three differences that all follow from C++ having built
        // this object solely to hand over:
        //
        // Aliasing: C++ placement-new'd the value into an `AlignedStorage` local and does not touch it
        //     again -- `AlignedStorage`'s destructor does not run `~ArgumentType` -- so taking it here
        //     races with nothing. Alignment comes from `alignas(alignment_of_v<T>)` on that storage.
        // Consumed exactly once: `.move()` deinitializes the storage, and nothing else destroys it. C++
        //     will not, and `CompletionHandler::operator()` clears its function before calling, so the
        //     handler cannot run twice.
        // Relocation: Swift moves a noncopyable imported C++ value byte-wise rather than running its
        //     move constructor. `WTF::CompletionHandler` static-asserts that such an argument has a
        //     non-trivial destructor, which is what makes Swift use the move constructor instead.

        body(unsafe pointer.assumingMemoryBound(to: Argument.self).move())
    }
}

extension WTF.VoidCompletionHandler: @unsafe CxxVoidCompletionHandler {}

extension WTF.BoolCompletionHandler: @unsafe CxxCompletionHandler {
    typealias Argument = Bool
}

#endif // compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN

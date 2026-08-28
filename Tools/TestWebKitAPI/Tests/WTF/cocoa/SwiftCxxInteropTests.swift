// Copyright (C) 2025-2026 Apple Inc. All rights reserved.
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

#if ENABLE_CXX_INTEROP && compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN

import Testing
import wtf
public import TestWTFLibrary
public import TestWTFLibrary.SwiftCxxInteropTestbed

private typealias Cxx = SwiftCxxInteropTestbed

// MARK: - Conformances

// This is safe because all conformances to the protocol are safe as long as they don't
// implement any of the requirements themselves.
extension Cxx.IntCompletionHandler: @unsafe TestWTFLibrary.CxxCompletionHandler {
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public typealias Argument = CInt
}

// `MoveOnlyProbe` is move-only in C++, so Swift imports it as `~Copyable`. This one takes it by rvalue
// reference, which the importer spells as `callAsFunction(consuming:)`, hence `CxxConsumingCompletionHandler`.
extension Cxx.MoveOnlyProbeCompletionHandler: @unsafe TestWTFLibrary.CxxConsumingCompletionHandler {
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public typealias Argument = SwiftCxxInteropTestbed.MoveOnlyProbe
}

// Copyable but not trivially copyable: this argument reaches Swift through its copy constructor.
extension Cxx.CopyCountingProbeCompletionHandler: @unsafe TestWTFLibrary.CxxCompletionHandler {
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public typealias Argument = SwiftCxxInteropTestbed.CopyCountingProbe
}

// Trivially copyable in C++, but Swift imports the `probe` field as a managed reference, so the bridge
// has to let Swift copy this argument rather than take it.
extension Cxx.SharedProbeHolderCompletionHandler: @unsafe TestWTFLibrary.CxxCompletionHandler {
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public typealias Argument = SwiftCxxInteropTestbed.SharedProbeHolder
}

// Noncopyable, so it is transferred to Swift, and it notices if Swift relocates it byte-wise.
extension Cxx.SelfReferentialProbeCompletionHandler: @unsafe TestWTFLibrary.CxxConsumingCompletionHandler {
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    public typealias Argument = SwiftCxxInteropTestbed.SelfReferentialProbe
}

// The simplest `Expected` shape, and the one ``CxxExpected``'s documentation uses as its example.
extension Cxx.IntExpected: TestWTFLibrary.CxxExpected {}

// Copyable but not trivially copyable: reading this value out has to run its copy constructor.
extension Cxx.CopyCountingProbeExpected: TestWTFLibrary.CxxExpected {}

// Trivially copyable in C++, but Swift imports the `probe` field as a managed reference, so reading the
// value has to leave the C++ side's ownership alone.
extension Cxx.SharedProbeHolderExpected: TestWTFLibrary.CxxExpected {}

// `MoveOnlyProbe` is move-only in C++, so the `Expected` holding one is noncopyable too and its value can
// only be moved out, which is what `CxxConsumingExpected` is for.
extension Cxx.MoveOnlyProbeExpected: TestWTFLibrary.CxxConsumingExpected {
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation, AlwaysUseLowerCamelCase, NoLeadingUnderscores
    public static func __take(_ expected: consuming Self) -> Value {
        Cxx.takeMoveOnlyProbeValue(consuming: expected)
    }
}

// Noncopyable, and notices if the value is relocated byte-wise on its way out.
extension Cxx.SelfReferentialProbeExpected: @unsafe TestWTFLibrary.CxxConsumingExpected {
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation, AlwaysUseLowerCamelCase, NoLeadingUnderscores
    public static func __take(_ expected: consuming Self) -> Value {
        unsafe Cxx.takeSelfReferentialProbeValue(consuming: expected)
    }
}

// An error that counts its own lifetime, so that the failure path -- which holds no value, and so moves
// none of the counters above -- can be checked at all.
extension Cxx.CountedErrorExpected: TestWTFLibrary.CxxConsumingExpected {
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation, AlwaysUseLowerCamelCase, NoLeadingUnderscores
    public static func __take(_ expected: consuming Self) -> Value {
        Cxx.takeCountedErrorValue(consuming: expected)
    }
}

// MARK: - Helpers

/// Observed via a weak reference to prove the closure context was released.
private final class Probe {}

private final class Recorder {
    var values: [CInt] = []
}

// MARK: - Tests

@Suite(.serialized)
@MainActor
struct SwiftCxxInteropTests {
    @Test
    func wtfFunctionCanBeInvokedFromSwift() async throws {
        let function = Cxx.IntBoolFunction { argument in
            argument ? 1 : 0
        }

        let result = Cxx.callIntBoolFunction(true, consuming: function)

        #expect(result == 1)
    }

    @Test
    func completionHandlerReceivesItsArgument() async throws {
        let result = await withCheckedContinuation { continuation in
            let completionHandler = Cxx.IntCompletionHandler { argument in
                continuation.resume(returning: argument)
            }

            Cxx.callIntCompletionHandler(3, consuming: completionHandler)
        }

        #expect(result == 3)
    }

    @Test
    func wtfCompletionHandlerCanBeInvokedFromSwift() async throws {
        let result = await withCheckedContinuation { continuation in
            let completionHandler = Cxx.IntCompletionHandler(continuation)

            Cxx.callIntCompletionHandler(3, consuming: completionHandler)
        }

        #expect(result == 3)
    }

    @Test
    func completionHandlerPreservesCapturedState() async throws {
        let captured: CInt = 40

        let result = await withCheckedContinuation { continuation in
            let completionHandler = Cxx.IntCompletionHandler { argument in
                continuation.resume(returning: argument + captured)
            }

            Cxx.callIntCompletionHandler(2, consuming: completionHandler)
        }

        #expect(result == 42)
    }

    @Test
    func voidCompletionHandlerCanBeInvokedFromSwift() async throws {
        var didCall = false

        let completionHandler = Cxx.VoidCompletionHandler {
            didCall = true
        }
        Cxx.callVoidCompletionHandler(consuming: completionHandler)

        #expect(didCall)
    }

    @Test
    func voidCompletionHandlerWorksWithAContinuation() async throws {
        await withCheckedContinuation { continuation in
            let completionHandler = Cxx.VoidCompletionHandler(continuation)

            Cxx.callVoidCompletionHandler(consuming: completionHandler)
        }
    }

    @Test
    func separateHandlersDoNotShareAContext() async throws {
        let recorder = Recorder()

        let first = Cxx.IntCompletionHandler { recorder.values.append($0 * 10) }
        let second = Cxx.IntCompletionHandler { recorder.values.append($0 * 100) }

        Cxx.callIntCompletionHandler(1, consuming: first)
        Cxx.callIntCompletionHandler(2, consuming: second)

        #expect(recorder.values == [10, 200])
    }

    // MARK: Context lifetime

    @Test
    func contextIsReleasedAfterTheHandlerIsCalled() async throws {
        weak var weakProbe: Probe?

        do {
            let probe = Probe()
            weakProbe = probe

            let completionHandler = Cxx.IntCompletionHandler { _ in
                // Capture the probe so it is kept alive only by the closure context.
                _ = probe
            }
            Cxx.callIntCompletionHandler(1, consuming: completionHandler)
        }

        #expect(weakProbe == nil, "the closure context should be released once the handler has run")
    }

    @Test
    func contextSurvivesUntilTheHandlerIsInvokedLater() async throws {
        // Leaving a stored handler behind would strand it for the next test, so clear the
        // slot even if this test returns early.
        defer { Cxx.resetStoredIntCompletionHandler() }

        // storeIntCompletionHandler() returns without calling the handler, so the closure has
        // to outlive that call.
        let result = await withCheckedContinuation { continuation in
            Cxx.storeIntCompletionHandler(consuming: .init(continuation))

            Cxx.invokeStoredIntCompletionHandler(9)
        }

        #expect(result == 9)
    }

    // MARK: Noncopyable arguments

    @Test
    func moveOnlyCompletionHandlerReceivesItsArgument() async throws {
        let recorder = Recorder()

        Cxx.callMoveOnlyProbeCompletionHandler(
            5,
            consuming: .init { (probe: consuming Cxx.MoveOnlyProbe) in
                recorder.values.append(probe.value())
            }
        )

        #expect(recorder.values == [5])
        #expect(Cxx.liveMoveOnlyProbeCount() == 0, "the argument should be destroyed exactly once")
    }

    @Test
    func moveOnlyArgumentIsDestroyedWhenTheClosureIgnoresIt() async throws {
        // C++ hands ownership of the argument to Swift, so nothing else will destroy it if the closure
        // does not look at it.
        Cxx.callMoveOnlyProbeCompletionHandler(8, consuming: .init { _ in })

        #expect(Cxx.liveMoveOnlyProbeCount() == 0, "an ignored argument should still be destroyed")
    }

    @Test
    func storedMoveOnlyCompletionHandlerSurvivesUntilInvoked() async throws {
        // See contextSurvivesUntilTheHandlerIsInvokedLater().
        defer { Cxx.resetStoredMoveOnlyProbeCompletionHandler() }

        let recorder = Recorder()

        Cxx.storeMoveOnlyProbeCompletionHandler(
            consuming: .init { (probe: consuming Cxx.MoveOnlyProbe) in
                recorder.values.append(probe.value())
            }
        )
        Cxx.invokeStoredMoveOnlyProbeCompletionHandler(9)

        #expect(recorder.values == [9])
        #expect(Cxx.liveMoveOnlyProbeCount() == 0, "the argument should be destroyed exactly once")
    }

    // MARK: Copyable, non-trivially-copyable arguments

    @Test
    func copyableArgumentIsCopiedRatherThanTakenFromTheCaller() async throws {
        Cxx.resetCopyCountingProbeCounts()

        let recorder = Recorder()

        // A copyable argument reaches Swift as a copy made by Swift, not by C++.
        Cxx.callCopyCountingProbeCompletionHandler(
            11,
            consuming: .init { (probe: Cxx.CopyCountingProbe) in
                recorder.values.append(probe.value())
            }
        )

        #expect(recorder.values == [11], "Swift should see the value C++ passed")
        #expect(Cxx.copyCountingProbeCopyCount() == 1, "exactly one copy should be made for Swift")
        #expect(Cxx.liveCopyCountingProbeCount() == 0, "every probe should be destroyed")
    }

    // MARK: Arguments holding a managed reference

    @Test
    func argumentHoldingAManagedReferenceLeavesTheCallersCountAlone() async throws {
        Cxx.resetSharedProbe()

        // The closure never looks at the argument, so any change to the reference count is the bridge's
        // doing rather than the closure's. C++ passed a borrowed reference and still holds it.
        let refCountAfter = Cxx.callSharedProbeHolderCompletionHandler(consuming: .init { _ in })

        #expect(
            Cxx.sharedProbeRefCalls() == Cxx.sharedProbeDerefCalls(),
            "the bridge must balance every retain and release it causes"
        )
        #expect(refCountAfter == 1, "the caller's own reference must survive the handler")
    }

    // MARK: Relocation

    @Test
    func noncopyableArgumentIsRelocatedWithItsMoveConstructor() async throws {
        let recorder = Recorder()

        // Pins the property that WTF::CompletionHandler's static_assert relies on: a noncopyable
        // argument with a non-trivial destructor is relocated by running its C++ move constructor, so
        // a pointer into the object stays valid. If Swift ever copies the bytes instead, the interior
        // pointer is left aimed at storage C++ has already abandoned.
        Cxx.callSelfReferentialProbeCompletionHandler(
            7,
            consuming: .init { (probe: consuming Cxx.SelfReferentialProbe) in
                unsafe recorder.values.append(probe.interiorPointerIsValid() ? 1 : 0)
                unsafe recorder.values.append(probe.valueThroughInteriorPointer())
            }
        )

        #expect(
            recorder.values == [1, 7],
            "a noncopyable argument must be relocated with its move constructor, not by copying its bytes"
        )
    }

    // MARK: Expected

    @Test
    func expectedExposesItsValue() async throws {
        let value = try Cxx.makeIntExpected(3).value

        #expect(value == 3)
    }

    @Test
    func unexpectedThrowsItsError() async throws {
        let unexpected = Cxx.makeIntUnexpected(.TooLarge)

        #expect(!unexpected.has_value())

        let thrown = try #require(throws: CxxUnexpected<Cxx.ProbeError>.self) {
            _ = try unexpected.value
        }
        #expect(thrown.error == .TooLarge)
    }

    @Test
    func readingTheValueLeavesTheExpectedAlone() async throws {
        let expected = Cxx.makeIntExpected(7)

        // `value` borrows rather than takes, so nothing here should be a one-shot read.
        let first = try expected.value
        let second = try expected.value

        #expect(first == 7)
        #expect(second == 7)
        #expect(expected.has_value())
    }

    @Test
    func readingAnUnexpectedLeavesTheErrorInPlace() async throws {
        let unexpected = Cxx.makeIntUnexpected(.TooSmall)

        // See readingTheValueLeavesTheExpectedAlone(): the failure path has to be repeatable too.
        for _ in 0..<2 {
            let thrown = try #require(throws: CxxUnexpected<Cxx.ProbeError>.self) {
                _ = try unexpected.value
            }
            #expect(thrown.error == .TooSmall)
        }
    }

    // MARK: Copyable, non-trivially-copyable values

    @Test
    func readingACopyableValueCopiesItExactlyOnce() async throws {
        Cxx.resetCopyCountingProbeCounts()

        // Sampled rather than assumed to be zero. There is no way to reset a count of live objects, so a
        // probe stranded by an earlier test would otherwise fail this one and point the reader here
        // rather than at the leak.
        let baseline = Cxx.liveCopyCountingProbeCount()

        do {
            let expected = Cxx.makeCopyCountingProbeExpected(11)
            let probe = try expected.value

            // Sampled before the last use of either probe below, so neither can have been released early
            // by the time these are read.
            let copies = Cxx.copyCountingProbeCopyCount()
            let live = Cxx.liveCopyCountingProbeCount() - baseline

            #expect(probe.value() == 11, "Swift should see the value C++ stored")
            #expect(expected.has_value(), "reading the value should not have emptied the expected")
            #expect(copies == 1, "exactly one copy should be made for Swift")
            #expect(live == 2, "the expected should still hold its own probe")
        }

        #expect(Cxx.liveCopyCountingProbeCount() == baseline, "every probe should be destroyed")
    }

    @Test
    func theValueOutlivesTheExpectedItWasReadFrom() async throws {
        Cxx.resetCopyCountingProbeCounts()

        // See readingACopyableValueCopiesItExactlyOnce().
        let baseline = Cxx.liveCopyCountingProbeCount()

        let probe: Cxx.CopyCountingProbe
        do {
            let expected = Cxx.makeCopyCountingProbeExpected(12)
            probe = try expected.value
        }

        let live = Cxx.liveCopyCountingProbeCount() - baseline

        // The expected -- and the probe it held -- are gone, so a `value` that handed Swift a pointer into
        // the expected rather than a copy would be reading freed storage here.
        #expect(probe.value() == 12)
        #expect(live == 1, "only Swift's copy should still be alive")
    }

    @Test
    func readingAnUnexpectedNeverTouchesTheValue() async throws {
        Cxx.resetCopyCountingProbeCounts()

        // See readingACopyableValueCopiesItExactlyOnce().
        let baseline = Cxx.liveCopyCountingProbeCount()

        do {
            let unexpected = Cxx.makeCopyCountingProbeUnexpected(.TooSmall)

            let thrown = try #require(throws: CxxUnexpected<Cxx.ProbeError>.self) {
                _ = try unexpected.value
            }

            #expect(thrown.error == .TooSmall)
            #expect(Cxx.copyCountingProbeCopyCount() == 0, "an expected holding an error has no value to copy")
        }

        #expect(Cxx.liveCopyCountingProbeCount() == baseline, "every probe should be destroyed")
    }

    // MARK: Values holding a managed reference

    @Test
    func valueHoldingAManagedReferenceLeavesTheCallersCountAlone() async throws {
        Cxx.resetSharedProbe()

        do {
            let holder = try Cxx.makeSharedProbeHolderExpected().value

            // See readingACopyableValueCopiesItExactlyOnce(): sampled before the last use of `holder`, so
            // its reference is certainly still held here.
            let countWhileHeld = Cxx.sharedProbeRefCount()
            withExtendedLifetime(holder) {}

            #expect(countWhileHeld == 2, "Swift's copy of the value should hold a reference of its own")
        }

        #expect(
            Cxx.sharedProbeRefCalls() == Cxx.sharedProbeDerefCalls(),
            "reading the value must balance every retain and release it causes"
        )
        #expect(Cxx.sharedProbeRefCount() == 1, "the C++ side's own reference must survive the read")
    }

    @Test
    func readingAnUnexpectedNeverRetainsTheManagedReference() async throws {
        Cxx.resetSharedProbe()

        do {
            let unexpected = Cxx.makeSharedProbeHolderUnexpected(.TooSmall)

            let thrown = try #require(throws: CxxUnexpected<Cxx.ProbeError>.self) {
                _ = try unexpected.value
            }

            // A bridge that loaded the value union before checking has_value() would have retained the
            // shared probe by now.
            #expect(thrown.error == .TooSmall)
            #expect(Cxx.sharedProbeRefCount() == 1, "an expected holding an error has no value to retain")
        }

        #expect(
            Cxx.sharedProbeRefCalls() == Cxx.sharedProbeDerefCalls(),
            "reading the error must balance every retain and release it causes"
        )
        #expect(Cxx.sharedProbeRefCount() == 1, "the C++ side's own reference must survive the read")
    }

    // MARK: Noncopyable values

    @Test
    func noncopyableValueCanBeConsumed() async throws {
        // See readingACopyableValueCopiesItExactlyOnce().
        let baseline = Cxx.liveMoveOnlyProbeCount()

        do {
            let probe = try Cxx.makeMoveOnlyProbeExpected(5).consume()

            let live = Cxx.liveMoveOnlyProbeCount() - baseline

            // A moved-from probe reports -1, so this also proves Swift was handed the destination of the
            // move rather than its source.
            #expect(probe.value() == 5)
            #expect(live == 1, "only the consumed value should still be alive")
        }

        #expect(
            Cxx.liveMoveOnlyProbeCount() == baseline,
            "the consumed value should be destroyed exactly once"
        )
    }

    @Test
    func consumingAnUnexpectedThrowsItsError() async throws {
        let thrown = try #require(throws: CxxUnexpected<Cxx.ProbeError>.self) {
            _ = try Cxx.makeMoveOnlyProbeUnexpected(.TooLarge).consume()
        }

        #expect(thrown.error == .TooLarge)
    }

    @Test
    func consumingAnUnsafeUnexpectedThrowsItsError() async throws {
        // consumingAnUnexpectedThrowsItsError() covers the failure path for a value Swift imports as
        // safe; this covers it for one Swift imports as `@unsafe`.
        let thrown = try #require(throws: CxxUnexpected<Cxx.ProbeError>.self) {
            _ = try unsafe Cxx.makeSelfReferentialProbeUnexpected(.TooSmall).consume()
        }

        #expect(thrown.error == .TooSmall)
    }

    @Test
    func consumingAnUnexpectedDestroysTheExpectedItConsumed() async throws {
        // An expected holding an error holds no value, so none of the value-side counters can see what
        // the failure path did with the expected it was handed. An error that counts its own lifetime
        // can: this is what proves `consume()` neither leaks nor double-destroys on the way out.
        let baseline = Cxx.liveCountingProbeErrorCount()

        do {
            let thrown = try #require(throws: CxxUnexpected<Cxx.CountingProbeError>.self) {
                _ = try Cxx.makeCountedErrorUnexpected(.TooLarge).consume()
            }

            #expect(thrown.error.value() == .TooLarge)
        }

        #expect(
            Cxx.liveCountingProbeErrorCount() == baseline,
            "the failure path should destroy the expected it consumed, exactly once"
        )
    }

    @Test
    func consumedNoncopyableValueIsRelocatedWithItsMoveConstructor() async throws {
        // The same property noncopyableArgumentIsRelocatedWithItsMoveConstructor() pins, on the way out of
        // an expected rather than into a completion handler: taking the value has to run the C++ move
        // constructor, or the probe's pointer into itself is left aimed at storage C++ has abandoned.
        // takeSelfReferentialProbeValue()'s static assertion is what makes Swift do that.
        let probe = try unsafe Cxx.makeSelfReferentialProbeExpected(7).consume()

        let interiorPointerIsValid = unsafe probe.interiorPointerIsValid()
        let valueThroughInteriorPointer = unsafe probe.valueThroughInteriorPointer()

        #expect(
            interiorPointerIsValid,
            "a noncopyable value must be relocated with its move constructor, not by copying its bytes"
        )
        #expect(valueThroughInteriorPointer == 7)
    }
}

#endif // ENABLE_CXX_INTEROP && compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN

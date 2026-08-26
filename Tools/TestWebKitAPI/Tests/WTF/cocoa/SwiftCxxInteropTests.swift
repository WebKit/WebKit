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
}

#endif // ENABLE_CXX_INTEROP && compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN

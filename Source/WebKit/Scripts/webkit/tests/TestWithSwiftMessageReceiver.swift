//
// Copyright (C) 2021-2023 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1.  Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// 2.  Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

import WebKit_Internal

final class TestWithSwiftWeakRef: @unchecked Sendable {
    private weak var target: TestWithSwift?
    init(target: TestWithSwift) {
        self.target = target
    }

    @used
    func getMessageTarget() -> TestWithSwift? {
        target
    }

    @used
    func dispatchTestAsyncMessage(
        connection: sending IPC.Connection,
        param: sending UInt32,
        completionHandler: sending CompletionHandlers.TestWithSwift.TestAsyncMessageCompletionHandler
    ) {
        MainActor.assumeIsolated {
            guard let target else {
                return
            }
            Task.immediate {
                await Self.runTestAsyncMessage(
                    target: target,
                    connection: connection,
                    param: param,
                    completionHandler: completionHandler
                )
            }
        }
    }

    @MainActor
    private static func runTestAsyncMessage(
        target: TestWithSwift,
        connection: IPC.Connection,
        param: UInt32,
        completionHandler: CompletionHandlers.TestWithSwift.TestAsyncMessageCompletionHandler
    ) async {
        do {
            let reply = try await mayThrowInvalidMessage(
                target.testAsyncMessage(
                    connection: connection,
                    param: param
                )
            )
            completionHandler.pointee(reply)
        } catch {
            markMessageInvalid(error, on: connection, message: .TestWithSwift_TestAsyncMessage)
            CompletionHandlers.TestWithSwift.completeWithDefaultReply(completionHandler)
        }
    }

    @used
    func dispatchTestSyncMessage(
        connection: sending IPC.Connection,
        param: sending UInt32,
        completionHandler: sending CompletionHandlers.TestWithSwift.TestSyncMessageCompletionHandler
    ) {
        MainActor.assumeIsolated {
            guard let target else {
                return
            }
            Task.immediate {
                await Self.runTestSyncMessage(
                    target: target,
                    connection: connection,
                    param: param,
                    completionHandler: completionHandler
                )
            }
        }
    }

    @MainActor
    private static func runTestSyncMessage(
        target: TestWithSwift,
        connection: IPC.Connection,
        param: UInt32,
        completionHandler: CompletionHandlers.TestWithSwift.TestSyncMessageCompletionHandler
    ) async {
        do {
            let reply = try await mayThrowInvalidMessage(
                target.testSyncMessage(
                    connection: connection,
                    param: param
                )
            )
            completionHandler.pointee(reply)
        } catch {
            markMessageInvalid(error, on: connection, message: .TestWithSwift_TestSyncMessage)
            CompletionHandlers.TestWithSwift.completeWithDefaultReply(completionHandler)
        }
    }

    @used
    func dispatchTestMessageWithAliasedParameter(
        connection: sending IPC.Connection,
        frameState: sending WebKit.RefFrameState
    ) {
        MainActor.assumeIsolated {
            guard let target else {
                return
            }
            Self.runTestMessageWithAliasedParameter(
                target: target,
                connection: connection,
                frameState: frameState
            )
        }
    }

    @MainActor
    private static func runTestMessageWithAliasedParameter(
        target: TestWithSwift,
        connection: IPC.Connection,
        frameState: WebKit.RefFrameState
    ) {
        do {
            try mayThrowInvalidMessage(
                target.testMessageWithAliasedParameter(
                    connection: connection,
                    frameState: frameState
                )
            )
        } catch {
            markMessageInvalid(error, on: connection, message: .TestWithSwift_TestMessageWithAliasedParameter)
        }
    }

    @used
    func dispatchTestThrowingMessageWithReply(
        connection: sending IPC.Connection,
        param: sending UInt32,
        completionHandler: sending CompletionHandlers.TestWithSwift.TestThrowingMessageWithReplyCompletionHandler
    ) {
        MainActor.assumeIsolated {
            guard let target else {
                return
            }
            Task.immediate {
                await Self.runTestThrowingMessageWithReply(
                    target: target,
                    connection: connection,
                    param: param,
                    completionHandler: completionHandler
                )
            }
        }
    }

    @MainActor
    private static func runTestThrowingMessageWithReply(
        target: TestWithSwift,
        connection: IPC.Connection,
        param: UInt32,
        completionHandler: CompletionHandlers.TestWithSwift.TestThrowingMessageWithReplyCompletionHandler
    ) async {
        do {
            let reply = try await mayThrowInvalidMessage(
                target.testThrowingMessageWithReply(
                    connection: connection,
                    param: param
                )
            )
            completionHandler.pointee(reply)
        } catch {
            markMessageInvalid(error, on: connection, message: .TestWithSwift_TestThrowingMessageWithReply)
            CompletionHandlers.TestWithSwift.completeWithDefaultReply(completionHandler)
        }
    }

    @used
    func dispatchTestThrowingMessageWithoutReply(
        connection: sending IPC.Connection,
        frameState: sending WebKit.RefFrameState,
        frameID: sending WebCore.FrameIdentifier
    ) {
        MainActor.assumeIsolated {
            guard let target else {
                return
            }
            Self.runTestThrowingMessageWithoutReply(
                target: target,
                connection: connection,
                frameState: frameState,
                frameID: frameID
            )
        }
    }

    @MainActor
    private static func runTestThrowingMessageWithoutReply(
        target: TestWithSwift,
        connection: IPC.Connection,
        frameState: WebKit.RefFrameState,
        frameID: WebCore.FrameIdentifier
    ) {
        do {
            try mayThrowInvalidMessage(
                target.testThrowingMessageWithoutReply(
                    connection: connection,
                    frameState: frameState,
                    frameID: frameID
                )
            )
        } catch {
            markMessageInvalid(error, on: connection, message: .TestWithSwift_TestThrowingMessageWithoutReply)
        }
    }
}

extension WebKit.TestWithSwiftMessageForwarder {
    static func create(target: TestWithSwift) -> RefTestWithSwiftMessageForwarder {
        let weakRefContainer = TestWithSwiftWeakRef(target: target)
        // Safety: we're creating a pointer which will immediately be stored in a
        // proper ref-counted reference on the C++ side before this call returns.
        // Workaround for rdar://163107752.
        return unsafe WebKit.TestWithSwiftMessageForwarder.createFromWeak(
            OpaquePointer(
                Unmanaged.passRetained(weakRefContainer).toOpaque()
            )
        )
    }
}

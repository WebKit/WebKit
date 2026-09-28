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

// Safety: target is only written in init, assumeIsolated asserts the main thread, and weak loads are atomic in the Swift runtime
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
            Task.immediateOnMainActor {
                await TestAsyncMessageInvocation(
                    target: target,
                    connection: connection,
                    param: param,
                    completionHandler: completionHandler
                )
                .run()
            }
        }
    }

    private final class TestAsyncMessageInvocation {
        private let target: TestWithSwift
        private let connection: IPC.Connection
        private let param: UInt32
        private let completionHandler: CompletionHandlers.TestWithSwift.TestAsyncMessageCompletionHandler

        init(
            target: TestWithSwift,
            connection: IPC.Connection,
            param: UInt32,
            completionHandler: CompletionHandlers.TestWithSwift.TestAsyncMessageCompletionHandler
        ) {
            self.target = target
            self.connection = connection
            self.param = param
            self.completionHandler = completionHandler
        }

        @MainActor
        func run() async {
            do {
                let reply = try await mayThrowInvalidMessage(
                    target.testAsyncMessage(
                        connection: connection,
                        param: param
                    )
                )
                completionHandler.pointee(reply)
            } catch {
                markMessageInvalid(error, on: connection)
                CompletionHandlers.TestWithSwift.completeWithDefaultReply(completionHandler)
            }
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
            Task.immediateOnMainActor {
                await TestSyncMessageInvocation(
                    target: target,
                    connection: connection,
                    param: param,
                    completionHandler: completionHandler
                )
                .run()
            }
        }
    }

    private final class TestSyncMessageInvocation {
        private let target: TestWithSwift
        private let connection: IPC.Connection
        private let param: UInt32
        private let completionHandler: CompletionHandlers.TestWithSwift.TestSyncMessageCompletionHandler

        init(
            target: TestWithSwift,
            connection: IPC.Connection,
            param: UInt32,
            completionHandler: CompletionHandlers.TestWithSwift.TestSyncMessageCompletionHandler
        ) {
            self.target = target
            self.connection = connection
            self.param = param
            self.completionHandler = completionHandler
        }

        @MainActor
        func run() async {
            do {
                let reply = try await mayThrowInvalidMessage(
                    target.testSyncMessage(
                        connection: connection,
                        param: param
                    )
                )
                completionHandler.pointee(reply)
            } catch {
                markMessageInvalid(error, on: connection)
                CompletionHandlers.TestWithSwift.completeWithDefaultReply(completionHandler)
            }
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
            Task.immediateOnMainActor {
                await TestMessageWithAliasedParameterInvocation(
                    target: target,
                    connection: connection,
                    frameState: frameState
                )
                .run()
            }
        }
    }

    private final class TestMessageWithAliasedParameterInvocation {
        private let target: TestWithSwift
        private let connection: IPC.Connection
        private let frameState: WebKit.RefFrameState

        init(
            target: TestWithSwift,
            connection: IPC.Connection,
            frameState: WebKit.RefFrameState
        ) {
            self.target = target
            self.connection = connection
            self.frameState = frameState
        }

        @MainActor
        func run() async {
            do {
                try await mayThrowInvalidMessage(
                    target.testMessageWithAliasedParameter(
                        connection: connection,
                        frameState: frameState
                    )
                )
            } catch {
                markMessageInvalid(error, on: connection)
            }
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
            Task.immediateOnMainActor {
                await TestThrowingMessageWithReplyInvocation(
                    target: target,
                    connection: connection,
                    param: param,
                    completionHandler: completionHandler
                )
                .run()
            }
        }
    }

    private final class TestThrowingMessageWithReplyInvocation {
        private let target: TestWithSwift
        private let connection: IPC.Connection
        private let param: UInt32
        private let completionHandler: CompletionHandlers.TestWithSwift.TestThrowingMessageWithReplyCompletionHandler

        init(
            target: TestWithSwift,
            connection: IPC.Connection,
            param: UInt32,
            completionHandler: CompletionHandlers.TestWithSwift.TestThrowingMessageWithReplyCompletionHandler
        ) {
            self.target = target
            self.connection = connection
            self.param = param
            self.completionHandler = completionHandler
        }

        @MainActor
        func run() async {
            do {
                let reply = try await mayThrowInvalidMessage(
                    target.testThrowingMessageWithReply(
                        connection: connection,
                        param: param
                    )
                )
                completionHandler.pointee(reply)
            } catch {
                markMessageInvalid(error, on: connection)
                CompletionHandlers.TestWithSwift.completeWithDefaultReply(completionHandler)
            }
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
            Task.immediateOnMainActor {
                await TestThrowingMessageWithoutReplyInvocation(
                    target: target,
                    connection: connection,
                    frameState: frameState,
                    frameID: frameID
                )
                .run()
            }
        }
    }

    private final class TestThrowingMessageWithoutReplyInvocation {
        private let target: TestWithSwift
        private let connection: IPC.Connection
        private let frameState: WebKit.RefFrameState
        private let frameID: WebCore.FrameIdentifier

        init(
            target: TestWithSwift,
            connection: IPC.Connection,
            frameState: WebKit.RefFrameState,
            frameID: WebCore.FrameIdentifier
        ) {
            self.target = target
            self.connection = connection
            self.frameState = frameState
            self.frameID = frameID
        }

        @MainActor
        func run() async {
            do {
                try await mayThrowInvalidMessage(
                    target.testThrowingMessageWithoutReply(
                        connection: connection,
                        frameState: frameState,
                        frameID: frameID
                    )
                )
            } catch {
                markMessageInvalid(error, on: connection)
            }
        }
    }

    @used
    func dispatchTestMessageWithEmptyReply(
        connection: sending IPC.Connection,
        param: sending UInt32,
        completionHandler: sending CompletionHandlers.TestWithSwift.TestMessageWithEmptyReplyCompletionHandler
    ) {
        MainActor.assumeIsolated {
            guard let target else {
                return
            }
            Task.immediateOnMainActor {
                await TestMessageWithEmptyReplyInvocation(
                    target: target,
                    connection: connection,
                    param: param,
                    completionHandler: completionHandler
                )
                .run()
            }
        }
    }

    private final class TestMessageWithEmptyReplyInvocation {
        private let target: TestWithSwift
        private let connection: IPC.Connection
        private let param: UInt32
        private let completionHandler: CompletionHandlers.TestWithSwift.TestMessageWithEmptyReplyCompletionHandler

        init(
            target: TestWithSwift,
            connection: IPC.Connection,
            param: UInt32,
            completionHandler: CompletionHandlers.TestWithSwift.TestMessageWithEmptyReplyCompletionHandler
        ) {
            self.target = target
            self.connection = connection
            self.param = param
            self.completionHandler = completionHandler
        }

        @MainActor
        func run() async {
            do {
                try await mayThrowInvalidMessage(
                    target.testMessageWithEmptyReply(
                        connection: connection,
                        param: param
                    )
                )
                completionHandler.pointee()
            } catch {
                markMessageInvalid(error, on: connection)
                CompletionHandlers.TestWithSwift.completeWithDefaultReply(completionHandler)
            }
        }
    }
}

extension WebKit.TestWithSwiftMessageForwarder {
    static func create(target: TestWithSwift) -> RefTestWithSwiftMessageForwarder {
        let weakRefContainer = TestWithSwiftWeakRef(target: target)
        // Safety: we're creating a pointer which will immediately be stored in a
        // proper ref-counted reference on the C++ side before this call returns.
        // Workaround for rdar://163107752.
        let forwarder = unsafe WebKit.TestWithSwiftMessageForwarder.createFromWeak(
            OpaquePointer(
                Unmanaged.passRetained(weakRefContainer).toOpaque()
            )
        )
        #if compiler(>=6.4)
        return RefTestWithSwiftMessageForwarder(forwarder)
        #else
        return forwarder
        #endif
    }
}

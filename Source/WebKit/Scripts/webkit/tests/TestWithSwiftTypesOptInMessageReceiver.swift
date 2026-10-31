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

#if ENABLE_SWIFT_TEST_CONDITION
import WebKit_Internal
#endif

#if ENABLE_SWIFT_TEST_CONDITION
// Safety: target is only written in init, assumeIsolated asserts the main thread, and weak loads are atomic in the Swift runtime
@_expose(Cxx)
final class TestWithSwiftTypesOptInWeakRef: @unchecked Sendable {
    private weak var target: TestWithSwiftTypesOptIn?
    init(target: TestWithSwiftTypesOptIn) {
        self.target = target
    }

    @used
    func getMessageTarget() -> TestWithSwiftTypesOptIn? {
        target
    }

    @used
    func dispatchCxxTypedMessage(
        connection: sending IPC.Connection,
        param: sending UInt32,
        completionHandler: sending CompletionHandlers.TestWithSwiftTypesOptIn.CxxTypedMessageCompletionHandler
    ) {
        MainActor.assumeIsolated {
            guard let target else {
                return
            }
            Task.immediateOnMainActor {
                await CxxTypedMessageInvocation(
                    target: target,
                    connection: connection,
                    param: param,
                    completionHandler: completionHandler
                )
                .run()
            }
        }
    }

    private final class CxxTypedMessageInvocation {
        private let target: TestWithSwiftTypesOptIn
        private let connection: IPC.Connection
        private let param: UInt32
        private let completionHandler: CompletionHandlers.TestWithSwiftTypesOptIn.CxxTypedMessageCompletionHandler

        init(
            target: TestWithSwiftTypesOptIn,
            connection: IPC.Connection,
            param: UInt32,
            completionHandler: CompletionHandlers.TestWithSwiftTypesOptIn.CxxTypedMessageCompletionHandler
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
                    target.cxxTypedMessage(
                        connection: connection,
                        param: param
                    )
                )
                CompletionHandlers.TestWithSwiftTypesOptIn.complete(completionHandler, reply)
            } catch {
                markMessageInvalid(error, on: connection)
                CompletionHandlers.TestWithSwiftTypesOptIn.completeWithDefaultReply(completionHandler)
            }
        }
    }

    @used
    func dispatchSwiftTypedMessage(
        connection: sending IPC.Connection,
        decoder: inout IPC.SwiftDecoder,
        replyEncoder: sending IPC.SwiftReplyEncoder
    ) -> Bool {
        let url: String?
        let values: [UInt32]
        do {
            url = try IPCCoding.WTFString.decode(from: &decoder)
            values = try IPCCoding.Vector<UInt32>.decode(from: &decoder)
        } catch {
            return false
        }
        MainActor.assumeIsolated {
            guard let target else {
                return
            }
            Task.immediateOnMainActor {
                await SwiftTypedMessageInvocation(
                    target: target,
                    connection: connection,
                    url: url,
                    values: values,
                    replyEncoder: replyEncoder
                )
                .run()
            }
        }
        return true
    }

    private final class SwiftTypedMessageInvocation {
        private let target: TestWithSwiftTypesOptIn
        private let connection: IPC.Connection
        private let url: String?
        private let values: [UInt32]
        private let replyEncoder: IPC.SwiftReplyEncoder

        init(
            target: TestWithSwiftTypesOptIn,
            connection: IPC.Connection,
            url: String?,
            values: [UInt32],
            replyEncoder: IPC.SwiftReplyEncoder
        ) {
            self.target = target
            self.connection = connection
            self.url = url
            self.values = values
            self.replyEncoder = replyEncoder
        }

        @MainActor
        func run() async {
            do {
                let reply = try await mayThrowInvalidMessage(
                    target.swiftTypedMessage(
                        connection: connection,
                        url: url,
                        values: values
                    )
                )
                UInt8.encode(reply, to: replyEncoder)
                replyEncoder.send()
            } catch {
                markMessageInvalid(error, on: connection)
                replyEncoder.cancel()
            }
        }
    }

    @used
    func dispatchSwiftTypedMessageWithoutReply(
        connection: sending IPC.Connection,
        decoder: inout IPC.SwiftDecoder
    ) -> Bool {
        let value: UInt64?
        do {
            value = try IPCCoding.StdOptional<UInt64>.decode(from: &decoder)
        } catch {
            return false
        }
        MainActor.assumeIsolated {
            guard let target else {
                return
            }
            Task.immediateOnMainActor {
                await SwiftTypedMessageWithoutReplyInvocation(
                    target: target,
                    connection: connection,
                    value: value
                )
                .run()
            }
        }
        return true
    }

    private final class SwiftTypedMessageWithoutReplyInvocation {
        private let target: TestWithSwiftTypesOptIn
        private let connection: IPC.Connection
        private let value: UInt64?

        init(
            target: TestWithSwiftTypesOptIn,
            connection: IPC.Connection,
            value: UInt64?
        ) {
            self.target = target
            self.connection = connection
            self.value = value
        }

        @MainActor
        func run() async {
            do {
                try await mayThrowInvalidMessage(
                    target.swiftTypedMessageWithoutReply(
                        connection: connection,
                        value: value
                    )
                )
            } catch {
                markMessageInvalid(error, on: connection)
            }
        }
    }

    @used
    func dispatchSwiftTypedSynchronousMessage(
        connection: sending IPC.Connection,
        decoder: inout IPC.SwiftDecoder,
        replyEncoder: sending IPC.SwiftReplyEncoder
    ) -> Bool {
        let payload: IPCTypes.TestSwiftStruct
        do {
            payload = try IPCTypes.CxxName.WebKit_TestSwiftStruct.decode(from: &decoder)
        } catch {
            return false
        }
        MainActor.assumeIsolated {
            guard let target else {
                return
            }
            Task.immediateOnMainActor {
                await SwiftTypedSynchronousMessageInvocation(
                    target: target,
                    connection: connection,
                    payload: payload,
                    replyEncoder: replyEncoder
                )
                .run()
            }
        }
        return true
    }

    private final class SwiftTypedSynchronousMessageInvocation {
        private let target: TestWithSwiftTypesOptIn
        private let connection: IPC.Connection
        private let payload: IPCTypes.TestSwiftStruct
        private let replyEncoder: IPC.SwiftReplyEncoder

        init(
            target: TestWithSwiftTypesOptIn,
            connection: IPC.Connection,
            payload: IPCTypes.TestSwiftStruct,
            replyEncoder: IPC.SwiftReplyEncoder
        ) {
            self.target = target
            self.connection = connection
            self.payload = payload
            self.replyEncoder = replyEncoder
        }

        @MainActor
        func run() async {
            do {
                let reply = try await mayThrowInvalidMessage(
                    target.swiftTypedSynchronousMessage(
                        connection: connection,
                        payload: payload
                    )
                )
                IPCCoding.WTFString.encode(reply, to: replyEncoder)
                replyEncoder.send()
            } catch {
                markMessageInvalid(error, on: connection)
                replyEncoder.cancel()
            }
        }
    }

    @used
    func dispatchCxxTypedSynchronousMessage(
        connection: sending IPC.Connection,
        param: sending UInt32,
        completionHandler: sending CompletionHandlers.TestWithSwiftTypesOptIn.CxxTypedSynchronousMessageCompletionHandler
    ) {
        MainActor.assumeIsolated {
            guard let target else {
                return
            }
            Task.immediateOnMainActor {
                await CxxTypedSynchronousMessageInvocation(
                    target: target,
                    connection: connection,
                    param: param,
                    completionHandler: completionHandler
                )
                .run()
            }
        }
    }

    private final class CxxTypedSynchronousMessageInvocation {
        private let target: TestWithSwiftTypesOptIn
        private let connection: IPC.Connection
        private let param: UInt32
        private let completionHandler: CompletionHandlers.TestWithSwiftTypesOptIn.CxxTypedSynchronousMessageCompletionHandler

        init(
            target: TestWithSwiftTypesOptIn,
            connection: IPC.Connection,
            param: UInt32,
            completionHandler: CompletionHandlers.TestWithSwiftTypesOptIn.CxxTypedSynchronousMessageCompletionHandler
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
                    target.cxxTypedSynchronousMessage(
                        connection: connection,
                        param: param
                    )
                )
                CompletionHandlers.TestWithSwiftTypesOptIn.complete(completionHandler, reply)
            } catch {
                markMessageInvalid(error, on: connection)
                CompletionHandlers.TestWithSwiftTypesOptIn.completeWithDefaultReply(completionHandler)
            }
        }
    }
}

extension WebKit.TestWithSwiftTypesOptInMessageForwarder {
    static func create(target: TestWithSwiftTypesOptIn) -> RefTestWithSwiftTypesOptInMessageForwarder {
        let weakRefContainer = TestWithSwiftTypesOptInWeakRef(target: target)
        // Safety: we're creating a pointer which will immediately be stored in a
        // proper ref-counted reference on the C++ side before this call returns.
        // Workaround for rdar://163107752.
        return unsafe WebKit.TestWithSwiftTypesOptInMessageForwarder.createFromWeak(
            OpaquePointer(
                Unmanaged.passRetained(weakRefContainer).toOpaque()
            )
        )
    }
}
#endif

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
@_expose(Cxx)
final class TestWithStreamSwiftWeakRef: @unchecked Sendable {
    private weak var target: TestWithStreamSwift?
    init(target: TestWithStreamSwift) {
        self.target = target
    }

    @used
    func getMessageTarget() -> TestWithStreamSwift? {
        target
    }

    @used
    func dispatchSendString(
        connection: sending IPC.StreamServerConnection,
        url: sending WTF.String
    ) {
        MainActor.assumeIsolated {
            guard let target else {
                return
            }
            Task.immediateOnMainActor {
                await SendStringInvocation(
                    target: target,
                    connection: connection,
                    url: url
                )
                .run()
            }
        }
    }

    private final class SendStringInvocation {
        private let target: TestWithStreamSwift
        private let connection: IPC.StreamServerConnection
        private let url: WTF.String

        init(
            target: TestWithStreamSwift,
            connection: IPC.StreamServerConnection,
            url: WTF.String
        ) {
            self.target = target
            self.connection = connection
            self.url = url
        }

        @MainActor
        func run() async {
            do {
                try await mayThrowInvalidMessage(
                    target.sendString(
                        connection: connection,
                        url: url
                    )
                )
            } catch {
                markMessageInvalid(error, on: connection)
            }
        }
    }

    @used
    func dispatchSendStringSync(
        connection: sending IPC.StreamServerConnection,
        url: sending WTF.String,
        completionHandler: sending CompletionHandlers.TestWithStreamSwift.SendStringSyncCompletionHandler
    ) {
        MainActor.assumeIsolated {
            guard let target else {
                return
            }
            Task.immediateOnMainActor {
                await SendStringSyncInvocation(
                    target: target,
                    connection: connection,
                    url: url,
                    completionHandler: completionHandler
                )
                .run()
            }
        }
    }

    private final class SendStringSyncInvocation {
        private let target: TestWithStreamSwift
        private let connection: IPC.StreamServerConnection
        private let url: WTF.String
        private let completionHandler: CompletionHandlers.TestWithStreamSwift.SendStringSyncCompletionHandler

        init(
            target: TestWithStreamSwift,
            connection: IPC.StreamServerConnection,
            url: WTF.String,
            completionHandler: CompletionHandlers.TestWithStreamSwift.SendStringSyncCompletionHandler
        ) {
            self.target = target
            self.connection = connection
            self.url = url
            self.completionHandler = completionHandler
        }

        @MainActor
        func run() async {
            do {
                let reply = try await mayThrowInvalidMessage(
                    target.sendStringSync(
                        connection: connection,
                        url: url
                    )
                )
                CompletionHandlers.TestWithStreamSwift.complete(completionHandler, reply)
            } catch {
                markMessageInvalid(error, on: connection)
                CompletionHandlers.TestWithStreamSwift.completeWithDefaultReply(completionHandler)
            }
        }
    }
}

extension WebKit.TestWithStreamSwiftMessageForwarder {
    static func create(target: TestWithStreamSwift) -> RefTestWithStreamSwiftMessageForwarder {
        let weakRefContainer = TestWithStreamSwiftWeakRef(target: target)
        // Safety: we're creating a pointer which will immediately be stored in a
        // proper ref-counted reference on the C++ side before this call returns.
        // Workaround for rdar://163107752.
        return unsafe WebKit.TestWithStreamSwiftMessageForwarder.createFromWeak(
            OpaquePointer(
                Unmanaged.passRetained(weakRefContainer).toOpaque()
            )
        )
    }
}

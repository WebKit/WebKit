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
final class TestWithSwiftTypesWeakRef: @unchecked Sendable {
    private weak var target: TestWithSwiftTypes?
    init(target: TestWithSwiftTypes) {
        self.target = target
    }

    @used
    func getMessageTarget() -> TestWithSwiftTypes? {
        target
    }

    @used
    func dispatchMessageWithoutReply(
        connection: sending IPC.Connection,
        decoder: inout IPC.SwiftDecoder
    ) -> Bool {
        let value: UInt32
        let name: String?
        let values: [UInt64]
        do {
            value = try UInt32.decode(from: &decoder)
            name = try IPCCoding.WTFString.decode(from: &decoder)
            values = try IPCCoding.Vector<UInt64>.decode(from: &decoder)
        } catch {
            return false
        }
        MainActor.assumeIsolated {
            guard let target else {
                return
            }
            Task.immediateOnMainActor {
                await MessageWithoutReplyInvocation(
                    target: target,
                    connection: connection,
                    value: value,
                    name: name,
                    values: values
                )
                .run()
            }
        }
        return true
    }

    private final class MessageWithoutReplyInvocation {
        private let target: TestWithSwiftTypes
        private let connection: IPC.Connection
        private let value: UInt32
        private let name: String?
        private let values: [UInt64]

        init(
            target: TestWithSwiftTypes,
            connection: IPC.Connection,
            value: UInt32,
            name: String?,
            values: [UInt64]
        ) {
            self.target = target
            self.connection = connection
            self.value = value
            self.name = name
            self.values = values
        }

        @MainActor
        func run() async {
            do {
                try await mayThrowInvalidMessage(
                    target.messageWithoutReply(
                        connection: connection,
                        value: value,
                        name: name,
                        values: values
                    )
                )
            } catch {
                markMessageInvalid(error, on: connection)
            }
        }
    }

    @used
    func dispatchMessageWithoutParameters(
        connection: sending IPC.Connection,
        decoder: inout IPC.SwiftDecoder
    ) -> Bool {
        MainActor.assumeIsolated {
            guard let target else {
                return
            }
            Task.immediateOnMainActor {
                await MessageWithoutParametersInvocation(
                    target: target,
                    connection: connection
                )
                .run()
            }
        }
        return true
    }

    private final class MessageWithoutParametersInvocation {
        private let target: TestWithSwiftTypes
        private let connection: IPC.Connection

        init(
            target: TestWithSwiftTypes,
            connection: IPC.Connection
        ) {
            self.target = target
            self.connection = connection
        }

        @MainActor
        func run() async {
            do {
                try await mayThrowInvalidMessage(
                    target.messageWithoutParameters(
                        connection: connection
                    )
                )
            } catch {
                markMessageInvalid(error, on: connection)
            }
        }
    }

    @used
    func dispatchMessageWithReply(
        connection: sending IPC.Connection,
        decoder: inout IPC.SwiftDecoder,
        replyEncoder: sending IPC.SwiftReplyEncoder
    ) -> Bool {
        let payload: IPCTypes.TestSwiftStruct
        let delta: Int32?
        do {
            payload = try IPCTypes.CxxName.WebKit_TestSwiftStruct.decode(from: &decoder)
            delta = try IPCCoding.StdOptional<Int32>.decode(from: &decoder)
        } catch {
            return false
        }
        MainActor.assumeIsolated {
            guard let target else {
                return
            }
            Task.immediateOnMainActor {
                await MessageWithReplyInvocation(
                    target: target,
                    connection: connection,
                    payload: payload,
                    delta: delta,
                    replyEncoder: replyEncoder
                )
                .run()
            }
        }
        return true
    }

    private final class MessageWithReplyInvocation {
        private let target: TestWithSwiftTypes
        private let connection: IPC.Connection
        private let payload: IPCTypes.TestSwiftStruct
        private let delta: Int32?
        private let replyEncoder: IPC.SwiftReplyEncoder

        init(
            target: TestWithSwiftTypes,
            connection: IPC.Connection,
            payload: IPCTypes.TestSwiftStruct,
            delta: Int32?,
            replyEncoder: IPC.SwiftReplyEncoder
        ) {
            self.target = target
            self.connection = connection
            self.payload = payload
            self.delta = delta
            self.replyEncoder = replyEncoder
        }

        @MainActor
        func run() async {
            do {
                let reply = try await mayThrowInvalidMessage(
                    target.messageWithReply(
                        connection: connection,
                        payload: payload,
                        delta: delta
                    )
                )
                IPCTypes.CxxName.WebKit_TestSwiftStruct.encode(reply, to: replyEncoder)
                replyEncoder.send()
            } catch {
                markMessageInvalid(error, on: connection)
                replyEncoder.cancel()
            }
        }
    }

    @used
    func dispatchMessageWithMultipleReplies(
        connection: sending IPC.Connection,
        decoder: inout IPC.SwiftDecoder,
        replyEncoder: sending IPC.SwiftReplyEncoder
    ) -> Bool {
        let names: [String?]
        let frameID: IPCTypes.FrameIdentifier?
        do {
            names = try IPCCoding.Vector<IPCCoding.WTFString>.decode(from: &decoder)
            frameID = try IPCCoding.Markable<IPCTypes.CxxName.WebCore_FrameIdentifier>.decode(from: &decoder)
        } catch {
            return false
        }
        MainActor.assumeIsolated {
            guard let target else {
                return
            }
            Task.immediateOnMainActor {
                await MessageWithMultipleRepliesInvocation(
                    target: target,
                    connection: connection,
                    names: names,
                    frameID: frameID,
                    replyEncoder: replyEncoder
                )
                .run()
            }
        }
        return true
    }

    private final class MessageWithMultipleRepliesInvocation {
        private let target: TestWithSwiftTypes
        private let connection: IPC.Connection
        private let names: [String?]
        private let frameID: IPCTypes.FrameIdentifier?
        private let replyEncoder: IPC.SwiftReplyEncoder

        init(
            target: TestWithSwiftTypes,
            connection: IPC.Connection,
            names: [String?],
            frameID: IPCTypes.FrameIdentifier?,
            replyEncoder: IPC.SwiftReplyEncoder
        ) {
            self.target = target
            self.connection = connection
            self.names = names
            self.frameID = frameID
            self.replyEncoder = replyEncoder
        }

        @MainActor
        func run() async {
            do {
                let reply = try await mayThrowInvalidMessage(
                    target.messageWithMultipleReplies(
                        connection: connection,
                        names: names,
                        frameID: frameID
                    )
                )
                Bool.encode(reply.0, to: replyEncoder)
                IPCCoding.Vector<IPCCoding.WTFString>.encode(reply.1, to: replyEncoder)
                IPCCoding.StdOptional<Double>.encode(reply.2, to: replyEncoder)
                replyEncoder.send()
            } catch {
                markMessageInvalid(error, on: connection)
                replyEncoder.cancel()
            }
        }
    }

    @used
    func dispatchMessageWithEmptyReply(
        connection: sending IPC.Connection,
        decoder: inout IPC.SwiftDecoder,
        replyEncoder: sending IPC.SwiftReplyEncoder
    ) -> Bool {
        let frameID: IPCTypes.FrameIdentifier
        do {
            frameID = try IPCTypes.CxxName.WebCore_FrameIdentifier.decode(from: &decoder)
        } catch {
            return false
        }
        MainActor.assumeIsolated {
            guard let target else {
                return
            }
            Task.immediateOnMainActor {
                await MessageWithEmptyReplyInvocation(
                    target: target,
                    connection: connection,
                    frameID: frameID,
                    replyEncoder: replyEncoder
                )
                .run()
            }
        }
        return true
    }

    private final class MessageWithEmptyReplyInvocation {
        private let target: TestWithSwiftTypes
        private let connection: IPC.Connection
        private let frameID: IPCTypes.FrameIdentifier
        private let replyEncoder: IPC.SwiftReplyEncoder

        init(
            target: TestWithSwiftTypes,
            connection: IPC.Connection,
            frameID: IPCTypes.FrameIdentifier,
            replyEncoder: IPC.SwiftReplyEncoder
        ) {
            self.target = target
            self.connection = connection
            self.frameID = frameID
            self.replyEncoder = replyEncoder
        }

        @MainActor
        func run() async {
            do {
                try await mayThrowInvalidMessage(
                    target.messageWithEmptyReply(
                        connection: connection,
                        frameID: frameID
                    )
                )
                replyEncoder.send()
            } catch {
                markMessageInvalid(error, on: connection)
                replyEncoder.cancel()
            }
        }
    }

    @used
    func dispatchMessageWithRefs(
        connection: sending IPC.Connection,
        decoder: inout IPC.SwiftDecoder,
        replyEncoder: sending IPC.SwiftReplyEncoder
    ) -> Bool {
        let object: IPCTypes.TestSwiftRefCountedClass
        let optionalObject: IPCTypes.TestSwiftRefCountedClass?
        let objects: [IPCTypes.TestSwiftRefCountedClass]
        do {
            object = try IPCTypes.CxxName.WebKit_TestSwiftRefCountedClass.decode(from: &decoder)
            optionalObject = try IPCCoding.StdOptional<IPCTypes.CxxName.WebKit_TestSwiftRefCountedClass>.decode(from: &decoder)
            objects = try IPCCoding.Vector<IPCTypes.CxxName.WebKit_TestSwiftRefCountedClass>.decode(from: &decoder)
        } catch {
            return false
        }
        MainActor.assumeIsolated {
            guard let target else {
                return
            }
            Task.immediateOnMainActor {
                await MessageWithRefsInvocation(
                    target: target,
                    connection: connection,
                    object: object,
                    optionalObject: optionalObject,
                    objects: objects,
                    replyEncoder: replyEncoder
                )
                .run()
            }
        }
        return true
    }

    private final class MessageWithRefsInvocation {
        private let target: TestWithSwiftTypes
        private let connection: IPC.Connection
        private let object: IPCTypes.TestSwiftRefCountedClass
        private let optionalObject: IPCTypes.TestSwiftRefCountedClass?
        private let objects: [IPCTypes.TestSwiftRefCountedClass]
        private let replyEncoder: IPC.SwiftReplyEncoder

        init(
            target: TestWithSwiftTypes,
            connection: IPC.Connection,
            object: IPCTypes.TestSwiftRefCountedClass,
            optionalObject: IPCTypes.TestSwiftRefCountedClass?,
            objects: [IPCTypes.TestSwiftRefCountedClass],
            replyEncoder: IPC.SwiftReplyEncoder
        ) {
            self.target = target
            self.connection = connection
            self.object = object
            self.optionalObject = optionalObject
            self.objects = objects
            self.replyEncoder = replyEncoder
        }

        @MainActor
        func run() async {
            do {
                let reply = try await mayThrowInvalidMessage(
                    target.messageWithRefs(
                        connection: connection,
                        object: object,
                        optionalObject: optionalObject,
                        objects: objects
                    )
                )
                IPCCoding.StdOptional<IPCTypes.CxxName.WebKit_TestSwiftRefCountedClass>.encode(reply, to: replyEncoder)
                replyEncoder.send()
            } catch {
                markMessageInvalid(error, on: connection)
                replyEncoder.cancel()
            }
        }
    }

    @used
    func dispatchSynchronousMessage(
        connection: sending IPC.Connection,
        decoder: inout IPC.SwiftDecoder,
        replyEncoder: sending IPC.SwiftReplyEncoder
    ) -> Bool {
        let kind: IPCTypes.TestSwiftEnum
        let options: IPCTypes.TestSwiftOptionSet
        do {
            kind = try IPCTypes.CxxName.WebKit_TestSwiftEnum.decode(from: &decoder)
            options = try IPCTypes.CxxName.WebKit_TestSwiftOptionSet.decode(from: &decoder)
        } catch {
            return false
        }
        MainActor.assumeIsolated {
            guard let target else {
                return
            }
            Task.immediateOnMainActor {
                await SynchronousMessageInvocation(
                    target: target,
                    connection: connection,
                    kind: kind,
                    options: options,
                    replyEncoder: replyEncoder
                )
                .run()
            }
        }
        return true
    }

    private final class SynchronousMessageInvocation {
        private let target: TestWithSwiftTypes
        private let connection: IPC.Connection
        private let kind: IPCTypes.TestSwiftEnum
        private let options: IPCTypes.TestSwiftOptionSet
        private let replyEncoder: IPC.SwiftReplyEncoder

        init(
            target: TestWithSwiftTypes,
            connection: IPC.Connection,
            kind: IPCTypes.TestSwiftEnum,
            options: IPCTypes.TestSwiftOptionSet,
            replyEncoder: IPC.SwiftReplyEncoder
        ) {
            self.target = target
            self.connection = connection
            self.kind = kind
            self.options = options
            self.replyEncoder = replyEncoder
        }

        @MainActor
        func run() async {
            do {
                let reply = try await mayThrowInvalidMessage(
                    target.synchronousMessage(
                        connection: connection,
                        kind: kind,
                        options: options
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
    func dispatchSynchronousMessageWithoutParameters(
        connection: sending IPC.Connection,
        decoder: inout IPC.SwiftDecoder,
        replyEncoder: sending IPC.SwiftReplyEncoder
    ) -> Bool {
        MainActor.assumeIsolated {
            guard let target else {
                return
            }
            Task.immediateOnMainActor {
                await SynchronousMessageWithoutParametersInvocation(
                    target: target,
                    connection: connection,
                    replyEncoder: replyEncoder
                )
                .run()
            }
        }
        return true
    }

    private final class SynchronousMessageWithoutParametersInvocation {
        private let target: TestWithSwiftTypes
        private let connection: IPC.Connection
        private let replyEncoder: IPC.SwiftReplyEncoder

        init(
            target: TestWithSwiftTypes,
            connection: IPC.Connection,
            replyEncoder: IPC.SwiftReplyEncoder
        ) {
            self.target = target
            self.connection = connection
            self.replyEncoder = replyEncoder
        }

        @MainActor
        func run() async {
            do {
                let reply = try await mayThrowInvalidMessage(
                    target.synchronousMessageWithoutParameters(
                        connection: connection
                    )
                )
                IPCCoding.StdOptional<IPCTypes.CxxName.WebKit_TestSwiftRefCountedClass>.encode(reply.0, to: replyEncoder)
                IPCCoding.WTFString.encode(reply.1, to: replyEncoder)
                replyEncoder.send()
            } catch {
                markMessageInvalid(error, on: connection)
                replyEncoder.cancel()
            }
        }
    }

    @used
    func dispatchMessageWithCxxTypes(
        connection: sending IPC.Connection,
        frameState: sending WebKit.RefFrameState,
        param: sending UInt32,
        completionHandler: sending CompletionHandlers.TestWithSwiftTypes.MessageWithCxxTypesCompletionHandler
    ) {
        MainActor.assumeIsolated {
            guard let target else {
                return
            }
            Task.immediateOnMainActor {
                await MessageWithCxxTypesInvocation(
                    target: target,
                    connection: connection,
                    frameState: frameState,
                    param: param,
                    completionHandler: completionHandler
                )
                .run()
            }
        }
    }

    private final class MessageWithCxxTypesInvocation {
        private let target: TestWithSwiftTypes
        private let connection: IPC.Connection
        private let frameState: WebKit.RefFrameState
        private let param: UInt32
        private let completionHandler: CompletionHandlers.TestWithSwiftTypes.MessageWithCxxTypesCompletionHandler

        init(
            target: TestWithSwiftTypes,
            connection: IPC.Connection,
            frameState: WebKit.RefFrameState,
            param: UInt32,
            completionHandler: CompletionHandlers.TestWithSwiftTypes.MessageWithCxxTypesCompletionHandler
        ) {
            self.target = target
            self.connection = connection
            self.frameState = frameState
            self.param = param
            self.completionHandler = completionHandler
        }

        @MainActor
        func run() async {
            do {
                let reply = try await mayThrowInvalidMessage(
                    target.messageWithCxxTypes(
                        connection: connection,
                        frameState: frameState,
                        param: param
                    )
                )
                CompletionHandlers.TestWithSwiftTypes.complete(completionHandler, reply)
            } catch {
                markMessageInvalid(error, on: connection)
                CompletionHandlers.TestWithSwiftTypes.completeWithDefaultReply(completionHandler)
            }
        }
    }
}

extension WebKit.TestWithSwiftTypesMessageForwarder {
    static func create(target: TestWithSwiftTypes) -> RefTestWithSwiftTypesMessageForwarder {
        let weakRefContainer = TestWithSwiftTypesWeakRef(target: target)
        // Safety: we're creating a pointer which will immediately be stored in a
        // proper ref-counted reference on the C++ side before this call returns.
        // Workaround for rdar://163107752.
        return unsafe WebKit.TestWithSwiftTypesMessageForwarder.createFromWeak(
            OpaquePointer(
                Unmanaged.passRetained(weakRefContainer).toOpaque()
            )
        )
    }
}

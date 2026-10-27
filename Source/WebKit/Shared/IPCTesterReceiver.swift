// Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE_IPC_TESTING_API && ENABLE_IPC_TESTING_SWIFT

import WebKit_Internal
import wtf

// Proxy interface to test IPC activities related to receiving messages in Swift.
@_expose(Cxx)
final class IPCTesterReceiver {
    // Optional just because of an initialization order issue. Always occupied after initialization finished.
    private var messageForwarder: RefIPCTesterReceiverMessageForwarder?

    @MainActor
    private var pendingDeferredReply: CheckedContinuation<UInt64, Never>?
    @MainActor
    private var deferredReplyArgument: UInt32 = 0
    @MainActor
    private var recordedTexts: [String?] = []

    // @used ensures these are retained even under -O -wmo: rdar://179098545
    @used
    init() {
        self.messageForwarder = WebKit.IPCTesterReceiverMessageForwarder.create(target: self)
    }

    @used
    func getMessageReceiver() -> RefIPCTesterReceiverMessageForwarder {
        guard let messageForwarder = self.messageForwarder else {
            fatalError("Unreachable - guaranteed to exist")
        }
        return messageForwarder
    }

    @MainActor
    func asyncMessage(
        connection: IPC.Connection,
        arg1: UInt32
    ) -> UInt32 {
        arg1 + 2
    }

    @MainActor
    func deferredReplyMessage(
        connection: IPC.Connection,
        arg1: UInt32
    ) async -> UInt64 {
        deferredReplyArgument = arg1
        return await withCheckedContinuation { continuation in
            pendingDeferredReply = continuation
        }
    }

    @MainActor
    func completeDeferredReply(connection: IPC.Connection, arg1: UInt32) {
        let continuation = pendingDeferredReply
        pendingDeferredReply = nil
        continuation?.resume(returning: UInt64(deferredReplyArgument) + UInt64(arg1) + 2)
    }

    // Takes C++ types, so negates the value without touching the WTF::String.
    @MainActor
    func transformInner(connection: IPC.Connection, inner: WebKit.IPCTesterInner) -> WebKit.IPCTesterInner {
        var reply = inner
        reply.value = 0 &- inner.value
        return reply
    }

    // The handlers below take Swift types, and must reply as ipcTesterTransformed() and the other
    // functions in IPCTesterPayload.h do.

    @MainActor
    func transformInnerSwift(connection: IPC.Connection, inner: IPCTypes.IPCTesterInner) -> IPCTypes.IPCTesterInner {
        Self.transformed(inner)
    }

    @MainActor
    func transformPayloadSwift(connection: IPC.Connection, payload: IPCTypes.IPCTesterPayload) -> IPCTypes.IPCTesterPayload {
        Self.transformed(payload)
    }

    @MainActor
    func transformPayloadSynchronouslySwift(
        connection: IPC.Connection,
        payload: IPCTypes.IPCTesterPayload
    ) -> IPCTypes.IPCTesterPayload {
        Self.transformed(payload)
    }

    @MainActor
    func transformNodeSwift(
        connection: IPC.Connection,
        node: IPCTypes.IPCTesterNode,
        link: IPCTypes.IPCTesterNode?
    ) -> (mirrored: IPCTypes.IPCTesterNode?, nodeCount: UInt32) {
        let nodeCount = Self.nodeCount(node) + (link.map(Self.nodeCount) ?? 0)
        return (link == nil ? nil : Self.mirrored(node), nodeCount)
    }

    @MainActor
    func describeSwift(
        connection: IPC.Connection,
        color: IPCTypes.IPCTesterColor,
        flags: IPCTypes.IPCTesterFlag,
        choice: IPCTypes.IPCTesterChoice,
        identifier: IPCTypes.IPCConnectionTesterIdentifier,
        variant: IPCTypes.IPCTesterVariant,
        flag: Bool
    ) -> String? {
        let colorName =
            switch color {
            case .red: "red"
            case .green: "green"
            case .blue: "blue"
            }
        let flagNames = [(IPCTypes.IPCTesterFlag.first, "first"), (.second, "second"), (.third, "third")]
            .filter { flags.contains($0.0) }
            .map(\.1)
        let variantDescription =
            switch variant {
            case .uint64(let value): "uint64:\(value)"
            case .string(let value): "string:\(value ?? "null")"
            case .ipcTesterInner(let value): "inner:\(value.value):\(value.label ?? "null")"
            }
        let flagsDescription = flagNames.isEmpty ? "none" : flagNames.joined(separator: "|")
        return "color=\(colorName) flags=\(flagsDescription) choice=\(choice == .yes ? "yes" : "no") "
            + "identifier=\(identifier.rawValue) variant=\(variantDescription) flag=\(flag)"
    }

    @MainActor
    func halveEvenNumberSwift(connection: IPC.Connection, value: UInt32) throws(InvalidMessage) -> UInt32 {
        try messageCheck("The number to halve must be even") {
            value.isMultiple(of: 2)
        }
        return value / 2
    }

    @MainActor
    func recordSwift(connection: IPC.Connection, text: String?) {
        recordedTexts.append(text)
    }

    @MainActor
    func takeRecordedSwift(connection: IPC.Connection) -> [String?] {
        let texts = recordedTexts
        recordedTexts = []
        return texts
    }

    @MainActor
    func clearRecordedSwift(connection: IPC.Connection) {
        recordedTexts = []
    }

    private static func exclaimed(_ string: String?) -> String? {
        string.map { $0 + "!" }
    }

    private static func transformed(_ inner: IPCTypes.IPCTesterInner) -> IPCTypes.IPCTesterInner {
        IPCTypes.IPCTesterInner(value: 0 &- inner.value, label: exclaimed(inner.label))
    }

    private static func transformed(_ payload: IPCTypes.IPCTesterPayload) -> IPCTypes.IPCTesterPayload {
        var result = payload
        result.flag.toggle()
        result.u8 &+= 1
        result.u16 &+= 1
        result.u32 &+= 1
        result.u64 &+= 1
        result.i8 &-= 1
        result.i16 &-= 1
        result.i32 &-= 1
        result.i64 &-= 1
        result.f *= 2
        result.d *= 2
        result.text = exclaimed(payload.text)
        result.numbers.reverse()
        result.strings.reverse()
        result.color = payload.color.map { color in
            switch color {
            case .red: .green
            case .green: .blue
            case .blue: .red
            }
        }
        result.flags = payload.flags.symmetricDifference([.first, .second, .third])
        result.choice = payload.choice == .yes ? .no : .yes
        result.inner = transformed(payload.inner)
        result.inners = payload.inners.map(transformed)
        result.variant =
            switch payload.variant {
            case .uint64(let value): .uint64(value &+ 1)
            case .string(let value): .string(exclaimed(value))
            case .ipcTesterInner(let value): .ipcTesterInner(transformed(value))
            }
        return result
    }

    private static func mirrored(_ node: IPCTypes.IPCTesterNode) -> IPCTypes.IPCTesterNode {
        IPCTypes.IPCTesterNode(
            name: exclaimed(node.name),
            children: node.children.reversed().map(mirrored),
            link: node.link.map(mirrored)
        )
    }

    private static func nodeCount(_ node: IPCTypes.IPCTesterNode) -> UInt32 {
        node.children.reduce(1) { $0 + nodeCount($1) } + (node.link.map(nodeCount) ?? 0)
    }
}

#endif

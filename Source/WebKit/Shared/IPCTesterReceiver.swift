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
final class IPCTesterReceiver {
    // Optional just because of an initialization order issue. Always occupied after initialization finished.
    private var messageForwarder: RefIPCTesterReceiverMessageForwarder?

    private var deferredReply: CompletionHandlers.IPCTesterReceiver.DeferredReplyMessageCompletionHandler?
    private var deferredReplyArgument: UInt32 = 0

    init() {
        self.messageForwarder = WebKit.IPCTesterReceiverMessageForwarder.create(target: self)
    }

    func getMessageReceiver() -> RefIPCTesterReceiverMessageForwarder {
        guard let messageForwarder = self.messageForwarder else {
            fatalError("Unreachable - guaranteed to exist")
        }
        return messageForwarder
    }

    func asyncMessage(
        connection: IPC.Connection,
        arg1: UInt32,
        completionHandler: CompletionHandlers.IPCTesterReceiver.AsyncMessageCompletionHandler
    ) {
        completionHandler.pointee(arg1 + 2)
    }

    func deferredReplyMessage(
        connection: IPC.Connection,
        arg1: UInt32,
        completionHandler: CompletionHandlers.IPCTesterReceiver.DeferredReplyMessageCompletionHandler
    ) {
        deferredReplyArgument = arg1
        deferredReply = completionHandler
    }

    func completeDeferredReply(connection: IPC.Connection, arg1: UInt32) {
        guard let reply = deferredReply else {
            return
        }
        deferredReply = nil
        reply.pointee(UInt64(deferredReplyArgument) + UInt64(arg1) + 2)
    }
}

#endif

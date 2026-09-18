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

// A Swift IPC receiver which cannot answer a message until a later one says so, for a C++ test to
// drive over a real connection. Shaped like the dispatch messages.py generates: a main actor
// isolated handler, reached from a nonisolated entry point because C++ calls it, entered with
// MainActor.assumeIsolated and started with Task.immediate so it runs up to its first suspension
// while IPC dispatch is still on the stack.
//
// The reply is a WTF::CompletionHandler, the same as the C++ receiver's, but C++ holds it and Swift
// holds a token for it - see SwiftDeferredReplySupport.h for why.
//
// cmake builds this into the TestIPCLibrary helper library rather than into TestIPC, because the C++
// which calls it includes the generated interop header: were both in one target, that C++ would
// depend on its own target's Swift compile. Xcode orders Swift before C++ within a target, so there
// it is simply part of TestIPC - which is why the test picks its header and namespace by build
// system.

import Foundation

@MainActor
private final class DeferredReplyState {
    static let shared = DeferredReplyState()

    var pendingReply: CheckedContinuation<Void, Never>?
    var replyToken: UInt64 = 0
    var handlerStartedOnMainThread = false
    var handlerResumedOnMainThread = false
    var handlerDidResume = false
}

private func onMainThread() -> Bool {
    pthread_main_np() == 1
}

@MainActor
private func handleMessageDeferringItsReply(replyToken: UInt64) async {
    let state = DeferredReplyState.shared
    state.handlerStartedOnMainThread = onMainThread()
    state.replyToken = replyToken
    await withCheckedContinuation { continuation in
        state.pendingReply = continuation
    }
    state.handlerResumedOnMainThread = onMainThread()
    state.handlerDidResume = true
    answerDeferredReplyForSwift(state.replyToken)
    state.replyToken = 0
}

/// Dispatches the message whose reply is deferred.
///
/// Mirrors a generated dispatch function: nonisolated because C++ calls it, entering the actor with
/// assumeIsolated and starting the handler with Task.immediate so it runs until its first suspension
/// while dispatch is still on the stack.
public func testIPCDispatchMessageDeferringItsReply(replyToken: UInt64) {
    MainActor.assumeIsolated {
        // Discarded deliberately: the task is not awaited, and a single-expression closure would
        // otherwise return it and leave assumeIsolated's own result unused.
        _ = Task.immediate {
            await handleMessageDeferringItsReply(replyToken: replyToken)
        }
    }
}

/// Dispatches the later message which lets the first one answer.
public func testIPCDispatchMessageReleasingTheDeferredReply() {
    MainActor.assumeIsolated {
        let continuation = DeferredReplyState.shared.pendingReply
        DeferredReplyState.shared.pendingReply = nil
        continuation?.resume()
    }
}

/// Whether the handler began on the main thread.
public func testIPCHandlerStartedOnMainThread() -> Bool {
    MainActor.assumeIsolated { DeferredReplyState.shared.handlerStartedOnMainThread }
}

/// Whether the handler resumed on the main thread.
public func testIPCHandlerResumedOnMainThread() -> Bool {
    MainActor.assumeIsolated { DeferredReplyState.shared.handlerResumedOnMainThread }
}

/// Whether the handler got past its suspension.
public func testIPCHandlerDidResume() -> Bool {
    MainActor.assumeIsolated { DeferredReplyState.shared.handlerDidResume }
}

/// Forgets any state from a previous test.
public func testIPCResetDeferredReplyState() {
    MainActor.assumeIsolated {
        let state = DeferredReplyState.shared
        state.pendingReply = nil
        state.replyToken = 0
        state.handlerStartedOnMainThread = false
        state.handlerResumedOnMainThread = false
        state.handlerDidResume = false
    }
}

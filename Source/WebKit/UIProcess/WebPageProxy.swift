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

#if compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN

import Foundation
import WebKit_Internal
import WebCore_Private

// This is safe because all conformances to the protocol are safe as long as they don't
// implement any of the requirements themselves.
extension WebKit.WebPageProxy.SelectWithGestureCompletionHandler: @unsafe CxxCompletionHandler {
    typealias Argument = WebKit.SelectWithGestureResult
}

// This is safe because all conformances to the protocol are safe as long as they don't
// implement any of the requirements themselves.
extension WebKit.WebPageProxy.RunJavaScriptInFrameCompletionHandler: @unsafe CxxConsumingCompletionHandler {
    typealias Argument = WebKit.RunJavaScriptResult
}

extension WebKit.WebPageProxy {
    #if canImport(Swift, _version: 6.4)
    @unsafe
    struct JavaScriptArgument: ~Copyable {
        let key: String
        let value: WebKit.JavaScriptEvaluationResult
    }

    @available(anyAppleOSAndDownlevels 27.0, *)
    @MainActor
    func runJavaScriptInMainFrame(
        source: IPC.TransferString,
        taintedness: JSC.SourceTaintedOrigin,
        url: WTF.URL,
        runAsAsyncFunction: Bool,
        arguments: consuming UniqueArray<JavaScriptArgument>?,
        forceUserGesture: Bool,
        removeTransientActivation: Bool,
        wantsResult: Bool
    ) async throws -> WebKit.JavaScriptEvaluationResult {
        var argumentsBridge = unsafe WebKit.JavaScriptArgumentsBridge()
        if var arguments = unsafe arguments {
            unsafe argumentsBridge = unsafe WebKit.JavaScriptArgumentsBridge(arguments.count)

            while unsafe !arguments.isEmpty {
                let argument = unsafe arguments.remove(at: 0)
                unsafe argumentsBridge.append(consuming: WTF.String(argument.key), consuming: argument.value)
            }
        }

        let box = unsafe try await withCheckedThrowingContinuation { continuation in
            let parameters = unsafe WebKit.RunJavaScriptParameters(
                source: source,
                taintedness: taintedness,
                sourceURL: url,
                runAsAsyncFunction: runAsAsyncFunction ? .Yes : .No,
                arguments: argumentsBridge.consume(),
                forceUserGesture: forceUserGesture ? .Yes : .No,
                removeTransientActivation: removeTransientActivation ? .Yes : .No
            )

            unsafe runJavaScriptInMainFrame(
                consuming: parameters,
                wantsResult,
                consuming: .init { result in
                    do {
                        let evaluationResult = try unsafe result.consume()
                        let box = unsafe CopyableBox(value: evaluationResult)
                        unsafe continuation.resume(returning: box)
                    } catch {
                        unsafe continuation.resume(throwing: error)
                    }
                }
            )
        }

        // Guaranteed to be non-nil since `take` is only called once, here.
        // swift-format-ignore: NeverForceUnwrap
        return unsafe box.take()!
    }
    #endif // canImport(Swift, _version: 6.4)

    private borrowing func editorStateCopy() -> WebKit.EditorState {
        unsafe __editorStateUnsafe().pointee
    }

    var editorState: WebKit.EditorState {
        editorStateCopy()
    }
}

#endif // compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN

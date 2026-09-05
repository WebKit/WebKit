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

#if HAVE_NEW_CODABLE
// This is safe because all conformances to the protocol are safe as long as they don't
// implement any of the requirements themselves.
extension WebKit.WebPageProxy.RunJavaScriptInFrameCompletionHandler: CxxConsumingCompletionHandler {
    typealias Argument = WebKit.RunJavaScriptResult
}
#endif // HAVE_NEW_CODABLE

extension WebKit.WebPageProxy {
    #if HAVE_NEW_CODABLE
    @MainActor
    func runJavaScriptInMainFrame(
        parameters: consuming WebKit.RunJavaScriptParameters,
        wantsResult: Bool
    ) async throws -> WebKit.JavaScriptEvaluationResult {
        let parametersBox = unsafe CopyableBox(value: parameters)

        let box = try await withCheckedThrowingContinuation { continuation in
            // Guaranteed to be non-nil since `take` is only called once, here.
            // swift-format-ignore: NeverForceUnwrap
            unsafe runJavaScriptInMainFrame(
                consuming: parametersBox.take()!,
                wantsResult,
                consuming: .init { result in
                    do {
                        let evaluationResult = try unsafe result.consume()
                        let box = CopyableBox(value: evaluationResult)
                        continuation.resume(returning: box)
                    } catch {
                        continuation.resume(throwing: error)
                    }
                }
            )
        }

        // Guaranteed to be non-nil since `take` is only called once, here.
        // swift-format-ignore: NeverForceUnwrap
        return box.take()!
    }
    #endif // HAVE_NEW_CODABLE

    private borrowing func editorStateCopy() -> WebKit.EditorState {
        unsafe __editorStateUnsafe().pointee
    }

    var editorState: WebKit.EditorState {
        editorStateCopy()
    }
}

#endif // compiler(>=6.4) && !SWIFT_WEBKIT_TOOLCHAIN

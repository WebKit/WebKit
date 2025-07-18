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

#if ENABLE_SWIFTUI && compiler(>=6.0)

import Foundation
internal import WebKit_Internal

final class WKURLSchemeHandlerAdapter: NSObject, WKURLSchemeHandler {
    init(_ wrapped: any URLSchemeHandler) {
        self.wrapped = wrapped
    }

    private let wrapped: any URLSchemeHandler

    private var tasks: [ObjectIdentifier: Task<Void, Never>] = [:]

    func webView(_ webView: WKWebView, start urlSchemeTask: any WKURLSchemeTask) {
        let task = Task {
            do {
                for try await result in wrapped.reply(for: urlSchemeTask.request) {
                    try Task.checkCancellation()

                    switch result {
                    case .response(let response):
                        urlSchemeTask.didReceive(response)

                    case .data(let data):
                        urlSchemeTask.didReceive(data)
                    }
                }

                try Task.checkCancellation()

                urlSchemeTask.didFinish()
            } catch is CancellationError {
                // If a CancellationError is thrown, that implies the Task has been cancelled, which itself implies that
                // the task has failed or finished. Consequently, the for-try-await loop needs to check for cancellation
                // and switch into this branch to avoid calling any of the `WKURLSchemeTask` methods, which throw Obj-C
                // exceptions, rightfully, if they are called after the task has failed or finished.
            } catch {
                urlSchemeTask.didFailWithError(error)
            }

            tasks[ObjectIdentifier(urlSchemeTask)] = nil
        }

        tasks[ObjectIdentifier(urlSchemeTask)] = task
    }

    func webView(_ webView: WKWebView, stop urlSchemeTask: any WKURLSchemeTask) {
        tasks.removeValue(forKey: ObjectIdentifier(urlSchemeTask))?.cancel()
    }
}

#endif

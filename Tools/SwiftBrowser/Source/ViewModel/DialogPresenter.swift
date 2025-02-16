// Copyright (C) 2024 Apple Inc. All rights reserved.
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

import Foundation
@_spi(Private) import WebKit

@MainActor
final class DialogPresenter: DialogPresenting {
    struct Dialog: Hashable, Identifiable, Sendable {
        enum Configuration: Sendable {
            case alert(String, @Sendable () -> Void)
            case confirm(String, @Sendable (sending WebPage.JavaScriptConfirmResult) -> Void)
            case prompt(String, defaultText: String?, @Sendable (sending WebPage.JavaScriptPromptResult) -> Void)
        }

        let id = UUID()
        let configuration: Configuration

        func hash(into hasher: inout Hasher) {
            hasher.combine(id)
        }

        static func == (lhs: Dialog, rhs: Dialog) -> Bool {
            lhs.id == rhs.id
        }
    }

    struct FilePicker {
        let allowsMultipleSelection: Bool
        let allowsDirectories: Bool
        let completion: @Sendable (sending WebPage.FileInputPromptResult) -> Void
    }

    weak var owner: BrowserViewModel? = nil

    func handleJavaScriptAlert(message: String, initiatedBy frame: WebPage.FrameInfo) async {
        await withCheckedContinuation { continuation in
            owner?.currentDialog = Dialog(configuration: .alert(message, continuation.resume))
        }
    }

    func handleJavaScriptConfirm(message: String, initiatedBy frame: WebPage.FrameInfo) async -> WebPage.JavaScriptConfirmResult {
        await withCheckedContinuation { continuation in
            owner?.currentDialog = Dialog(configuration: .confirm(message, continuation.resume(returning:)))
        }
    }

    func handleJavaScriptPrompt(message: String, defaultText: String?, initiatedBy frame: WebPage.FrameInfo) async -> WebPage.JavaScriptPromptResult {
        await withCheckedContinuation { continuation in
            owner?.currentDialog = Dialog(configuration: .prompt(message, defaultText: defaultText, continuation.resume(returning:)))
        }
    }

    func handleFileInputPrompt(parameters: WKOpenPanelParameters, initiatedBy frame: WebPage.FrameInfo) async -> WebPage.FileInputPromptResult {
        await withCheckedContinuation { continuation in
            owner?.currentFilePicker = FilePicker(allowsMultipleSelection: parameters.allowsMultipleSelection, allowsDirectories: parameters.allowsDirectories, completion: continuation.resume(returning:))
        }
    }
}

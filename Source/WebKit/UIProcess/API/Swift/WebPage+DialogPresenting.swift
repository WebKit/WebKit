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

#if ENABLE_SWIFTUI && compiler(>=6.0)

import Foundation

// MARK: Supporting types

extension WebPage_v0 {
    public enum JavaScriptConfirmResult: Hashable, Sendable {
        case ok
        case cancel
    }

    public enum JavaScriptPromptResult: Hashable, Sendable {
        case ok(String)
        case cancel
    }

    public enum FileInputPromptResult: Hashable, Sendable {
        case ok([URL])
        case cancel
    }
}

// MARK: DialogPresenting protocol

@_spi(Private)
public protocol DialogPresenting {
    @MainActor
    func handleJavaScriptAlert(message: String, initiatedBy frame: WebPage_v0.FrameInfo) async

    @MainActor
    func handleJavaScriptConfirm(message: String, initiatedBy frame: WebPage_v0.FrameInfo) async -> WebPage_v0.JavaScriptConfirmResult

    @MainActor
    func handleJavaScriptPrompt(message: String, defaultText: String?, initiatedBy frame: WebPage_v0.FrameInfo) async -> WebPage_v0.JavaScriptPromptResult

    @MainActor
    func handleFileInputPrompt(parameters: WKOpenPanelParameters, initiatedBy frame: WebPage_v0.FrameInfo) async -> WebPage_v0.FileInputPromptResult
}

// MARK: Default implementation

public extension DialogPresenting {
    @MainActor
    func handleJavaScriptAlert(message: String, initiatedBy frame: WebPage_v0.FrameInfo) async {
    }

    @MainActor
    func handleJavaScriptConfirm(message: String, initiatedBy frame: WebPage_v0.FrameInfo) async -> WebPage_v0.JavaScriptConfirmResult {
        .cancel
    }

    @MainActor
    func handleJavaScriptPrompt(message: String, defaultText: String?, initiatedBy frame: WebPage_v0.FrameInfo) async -> WebPage_v0.JavaScriptPromptResult {
        .cancel
    }

    @MainActor
    func handleFileInputPrompt(parameters: WKOpenPanelParameters, initiatedBy frame: WebPage_v0.FrameInfo) async -> WebPage_v0.FileInputPromptResult {
        .cancel
    }
}

 #endif

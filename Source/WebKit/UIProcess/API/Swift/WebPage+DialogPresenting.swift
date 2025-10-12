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

extension WebPage {
    /// The result of handling a JavaScript confirm invocation.
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public enum JavaScriptConfirmResult: Hashable, Sendable {
        /// Signals an affirmative action was produced by the invocation.
        case ok

        /// Signals a negative action was produced by the invocation.
        case cancel
    }

    /// The result of handling a JavaScript confirm invocation.
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public enum JavaScriptPromptResult: Hashable, Sendable {
        /// Signals an affirmative action was produced by the invocation with the specified text.
        case ok(Swift.String)

        /// Signals a negative action was produced by the invocation.
        case cancel
    }

    /// The result of handling a JavaScript open invocation.
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public enum FileInputPromptResult: Hashable, Sendable {
        /// Signals an affirmative action was produced by the invocation with the specified files.
        case selected([Foundation.URL])

        /// Signals a negative action was produced by the invocation.
        case cancel
    }
}

// MARK: DialogPresenting protocol

extension WebPage {
    /// Allows providing custom behavior to handle JavaScript actions and provide a response.
    ///
    /// Typically when handling these, some UI should be presented to the user for them to provide a response,
    /// which will then be communicated back to JavaScript.
    ///
    /// When these methods are invoked, JavaScript is blocked until the async method returns.
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public protocol DialogPresenting {
        /// A JavaScript `alert()` function has been invoked.
        ///
        /// - Parameters:
        ///   - message: The message provided by JavaScript.
        ///   - frame: Information about the frame whose JavaScript process initiated this call.
        @MainActor
        func handleJavaScriptAlert(message: Swift.String, initiatedBy frame: WebPage.FrameInfo) async

        /// A JavaScript `confirm()` function has been invoked.
        ///
        /// - Parameters:
        ///   - message: The message provided by JavaScript.
        ///   - frame: Information about the frame whose JavaScript process initiated this call.
        /// - Returns: The result of handling the invocation.
        @MainActor
        func handleJavaScriptConfirm(message: Swift.String, initiatedBy frame: WebPage.FrameInfo) async -> WebPage.JavaScriptConfirmResult

        /// A JavaScript `prompt()` function has been invoked.
        ///
        /// - Parameters:
        ///   - message: The message provided by JavaScript.
        ///   - defaultText: The initial text provided by JavaScript, intended to be displayed in some text entry field.
        ///   - frame: Information about the frame whose JavaScript process initiated this call.
        /// - Returns: The result of handling the invocation; if the result is affirmative, the response will include some text returned to JavaScript.
        @MainActor
        func handleJavaScriptPrompt(
            message: Swift.String,
            defaultText: Swift.String?,
            initiatedBy frame: WebPage.FrameInfo
        ) async -> WebPage.JavaScriptPromptResult

        /// Returns the result of handling a JavaScript request to open files.
        ///
        /// - Parameters:
        ///   - parameters: The options to use for the file dialog.
        ///   - frame: Information about the frame whose JavaScript process initiated this call.
        /// - Returns: The result of handling the invocation; if the result is affirmative, the response will include a set of files returned to JavaScript.
        @MainActor
        func handleFileInputPrompt(
            parameters: WKOpenPanelParameters,
            initiatedBy frame: WebPage.FrameInfo
        ) async -> WebPage.FileInputPromptResult
    }
}

// MARK: Default implementation

@available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
@available(watchOS, unavailable)
@available(tvOS, unavailable)
extension WebPage.DialogPresenting {
    /// By default, this method immediately returns.
    @MainActor
    public func handleJavaScriptAlert(message: Swift.String, initiatedBy frame: WebPage.FrameInfo) async {
    }

    /// By default, this method immediately returns with a result of `.cancel`.
    @MainActor
    public func handleJavaScriptConfirm(
        message: Swift.String,
        initiatedBy frame: WebPage.FrameInfo
    ) async -> WebPage.JavaScriptConfirmResult {
        .cancel
    }

    /// By default, this method immediately returns with a result of `.cancel`.
    @MainActor
    public func handleJavaScriptPrompt(
        message: Swift.String,
        defaultText: Swift.String?,
        initiatedBy frame: WebPage.FrameInfo
    ) async -> WebPage.JavaScriptPromptResult {
        .cancel
    }

    /// By default, this method immediately returns with a result of `.cancel`.
    @MainActor
    public func handleFileInputPrompt(
        parameters: WKOpenPanelParameters,
        initiatedBy frame: WebPage.FrameInfo
    ) async -> WebPage.FileInputPromptResult {
        .cancel
    }
}

#endif

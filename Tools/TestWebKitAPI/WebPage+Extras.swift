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

#if ENABLE_SWIFTUI && compiler(>=6.2)

import Foundation
@_spi(Testing) @_spi(CrossImportOverlay) import WebKit
import WebKit_Private.WKPreferencesPrivate
import WebKit_Private.WKWebViewPrivateForTesting
import WebKit_Private.WKWebViewPrivate

#if os(macOS)
private import Carbon
#endif

extension WebPage {
    enum EditCommand: String {
        case deleteBackward = "DeleteBackward"
    }

    func waitForNextPresentationUpdate() async {
        await withCheckedContinuation { continuation in
            backingWebView._do(afterNextPresentationUpdate: {
                continuation.resume()
            })
        }
    }

    func setWebFeature(_ key: String, enabled: Bool) {
        guard let feature = WKPreferences._features().first(where: { $0.key == key }) else {
            fatalError()
        }

        backingWebView.configuration.preferences._setEnabled(enabled, for: feature)
    }

    func renderTree() async throws -> String {
        try await backingWebView._getRenderTreeAsString()
    }

    func insertText(_ text: String) async {
        #if os(macOS)
        backingWebView.insertText(text)
        #else
        backingWebView.textInputContentView.insertText(text)
        #endif
        await waitForNextPresentationUpdate()
    }

    func executeEditCommand(_ command: EditCommand, with argument: String? = nil) async {
        let success = await backingWebView._executeEditCommand(command.rawValue, argument: argument)
        assert(success)
    }

    #if os(macOS)
    func click(at location: NSPoint) {
        guard let window = unsafe backingWebView.window else {
            preconditionFailure("Could not create NSEvent because there is no NSWindow.")
        }

        let timestamp = GetCurrentEventTime()

        let mouseDown = NSEvent.mouseEvent(
            with: .leftMouseDown,
            location: location,
            modifierFlags: [],
            timestamp: timestamp,
            windowNumber: window.windowNumber,
            context: nil,
            eventNumber: 0,
            clickCount: 1,
            pressure: 1
        )

        let mouseUp = NSEvent.mouseEvent(
            with: .leftMouseUp,
            location: location,
            modifierFlags: [],
            timestamp: timestamp,
            windowNumber: window.windowNumber,
            context: nil,
            eventNumber: 0,
            clickCount: 1,
            pressure: 0
        )

        guard let mouseDown, let mouseUp else {
            preconditionFailure("Could not create NSEvent.")
        }

        backingWebView.mouseDown(with: mouseDown)
        backingWebView.mouseUp(with: mouseUp)
    }
    #endif // os(macOS)
}

#endif // ENABLE_SWIFTUI && compiler(>=6.2)

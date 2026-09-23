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

#if ENABLE_SWIFTUI

import Foundation
@_spi(Testing) @_spi(CrossImportOverlay) public import WebKit
import WebKit_Private.WKPreferencesPrivate
import WebKit_Private.WKWebViewPrivateForTesting
public import struct Swift.String
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView

extension WebPage {
    /// Suspends execution of the current context until the next presentation update has occurred for this page.
    public func waitForNextPresentationUpdate() async {
        await withCheckedContinuation { continuation in
            backingWebView._do(afterNextPresentationUpdate: {
                continuation.resume()
            })
        }
    }

    /// Configures a specific preference for the engine to use.
    ///
    /// - Parameters:
    ///   - key: The preference key value.
    ///   - enabled: If the preference should be enabled or disabled.
    public func setWebFeature(_ key: String, enabled: Bool) {
        guard let feature = WKPreferences._features().first(where: { $0.key == key }) else {
            fatalError()
        }

        backingWebView.configuration.preferences._setEnabled(enabled, for: feature)
    }

    /// Produces a String representing the  current visual state of the web content as a tree of rendering elements.
    ///
    /// - Returns: The representative render tree for the current web content.
    public func renderTree() async throws -> String {
        try await backingWebView._getRenderTreeAsString()
    }

    /// Inserts text into the web page.
    ///
    /// - Parameter text: The text to insert.
    public func insertText(_ text: String) async {
        #if os(macOS)
        backingWebView.insertText(text)
        #else
        backingWebView.textInputContentView.insertText(text)
        #endif
        await waitForNextPresentationUpdate()
    }

}

#endif // ENABLE_SWIFTUI

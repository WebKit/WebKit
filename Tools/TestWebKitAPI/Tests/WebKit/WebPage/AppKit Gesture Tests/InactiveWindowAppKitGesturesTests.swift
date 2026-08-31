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

#if HAVE_APPKIT_GESTURES_SUPPORT

import Foundation
import struct Foundation.URL
@_spi(WebKitAdditions_Testing) @_spi(Testing) import WebKit
import SwiftUI
import struct Swift.String
private import struct TestWebKitAPILibrary.DOMRect
import Testing
private import TestWebKitAPILibrary
private import Recap
private import AppKit_Private.NSMenu_Private

extension AppKitGesturesTests {
    @MainActor
    @Suite(.serialized, .timeLimit(.minutes(1)))
    final class InactiveWindow: AppKitGestureTestSuite {
        static let text = "Here's to the crazy ones."

        let recap = Recap.shared

        let page: WebPage = {
            var configuration = WebPage.Configuration()
            configuration.requiresUserActionForEditingControlsManager = true
            return WebPage(configuration: configuration)
        }()

        let windowHost: TestWindowHost

        init() async throws {
            let contentSize = NSSize(width: 800, height: 600)

            // The web view is hosted in a window that never becomes key, so it always sees itself in an
            // inactive window even after being clicked.
            self.windowHost = TestWindowHost(size: contentSize, shouldBecomeKey: false) { [page] in
                WebView(page)
                    .webViewBackForwardNavigationGestures(.enabled)
            }

            await NSApp.waitForActivation()
        }
    }
}

extension AppKitGesturesTests.InactiveWindow {
    @Test
    func singleClickIntoInactiveWindowIsSuppressed() async throws {
        let html = """
            <body style="margin: 0">
            <div id="target"
                 onclick="window.clickCount = (window.clickCount || 0) + 1;"
                 style="width: 100vw; height: 100vh; font-size: 30px;">click target</div>
            </body>
            """
        try await page.load(html: html).wait()
        await page.waitForNextPresentationUpdate()

        #expect(!window.isKeyWindow)

        let targetBounds = try await screenBounds(ofElementWithID: "target")

        await recap.play { composer in
            composer._wk_click(at: targetBounds.center, for: .seconds(0.1))
        }

        await page.waitForPendingMouseEvents()
        await page.waitForNextPresentationUpdate()

        let clickCount = try await page.callJavaScript(returning: Int.self) {
            "return window.clickCount || 0;"
        }
        #expect(clickCount == 0)
    }
}

#endif

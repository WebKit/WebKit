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

#if os(macOS) && ENABLE_SWIFTUI

import AppKit
import SwiftUI
import Testing
@_spi(Testing) import WebKit
private import TestWebKitAPILibrary

@MainActor
struct WebPageMouseEventsTests {
    private let page = WebPage()
    private let window: NSWindow

    init() async throws {
        self.window = NSWindow(size: NSSize(width: 400, height: 400)) { [page] in
            WebView(page)
        }
        self.window.setFrameOrigin(.zero)
        self.window.makeKeyAndOrderFront(nil)
    }

    @Test
    func mouseDownUpFiresClickHandler() async throws {
        let html = """
            <body style="margin:0;width:100%;height:100vh"
                  onclick="window.clicked=true">x</body>
            """
        try await page.load(html: html).wait()

        let center = CGPoint(x: 200, y: 200)
        page.mouseDown(at: center)
        page.mouseUp(at: center)
        await page.waitForPendingMouseEvents()

        let fired = try await page.callJavaScript("return window.clicked === true;") as? Bool
        #expect(fired == true)
    }
}

#endif // os(macOS) && ENABLE_SWIFTUI

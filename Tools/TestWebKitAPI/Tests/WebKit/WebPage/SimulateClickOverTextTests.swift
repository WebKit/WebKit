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

#if ENABLE_SWIFTUI

@_spi(CrossImportOverlay) import WebKit
import Testing
private import WebKit_Private.WKWebViewPrivate
private import TestWebKitAPILibrary
import struct Swift.String

extension WebPage {
    fileprivate func simulateClick(over text: String) async -> Bool {
        await backingWebView._simulateClickOverFirstMatchingTextInViewport(withUserInteraction: text)
    }
}

@MainActor
struct SimulateClickOverTextTests {
    @Test(arguments: [false, true])
    func clickTargets(afterScrolling: Bool) async throws {
        let page = WebPage()
        let testPage = try #require(Bundle.testResources.url(forResource: "click-targets", withExtension: "html"))
        try await page.load(testPage).wait()

        if afterScrolling {
            try await page.callJavaScript("scrollBy(0, 5000)")
            await page.waitForNextPresentationUpdate()
        }

        #expect(await page.simulateClick(over: "Bugzilla"))
        #expect(await page.simulateClick(over: "Sign up"))
        #expect(await page.simulateClick(over: "First name"))
        #expect(await page.simulateClick(over: "Log in"))
        #expect(await page.simulateClick(over: "More info"))
        #expect(await page.simulateClick(over: "Close"))

        let expectedEvents = ["mousedown", "mouseup", "click"]

        let topSelectors = [
            "a.top",
            "button.top",
            "input[type=text].top",
            "input[type=submit].top",
            "input[type=button].top",
            "div.close.top",
        ]

        let bottomSelectors = [
            "a.bottom",
            "button.bottom",
            "input.bottom",
            "input[type=submit].bottom",
            "input[type=button].bottom",
            "div.close.bottom",
        ]

        for selector in topSelectors {
            let result = try await page.callJavaScript(returning: [String]?.self, arguments: ["selector": selector]) {
                """
                return document.querySelector(selector).events;
                """
            }
            #expect(result == (afterScrolling ? nil : expectedEvents))
        }

        for selector in bottomSelectors {
            let result = try await page.callJavaScript(returning: [String]?.self, arguments: ["selector": selector]) {
                """
                return document.querySelector(selector).events;
                """
            }
            #expect(result == (afterScrolling ? expectedEvents : nil))
        }
    }
}

#endif // ENABLE_SWIFTUI

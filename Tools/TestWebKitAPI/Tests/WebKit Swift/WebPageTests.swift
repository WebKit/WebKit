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

#if ENABLE_SWIFTUI && canImport(Testing) && compiler(>=6.0)

import SwiftUI
import Observation
import Testing
@_spi(Private) @_spi(Testing) import WebKit
@_spi(Private) import _WebKit_SwiftUI

extension WebPage.NavigationEvent.Kind: @retroactive Equatable {
    public static func == (lhs: Self, rhs: Self) -> Bool {
        switch (lhs, rhs) {
        case (.startedProvisionalNavigation, .startedProvisionalNavigation):
            true
        case (.receivedServerRedirect, .receivedServerRedirect):
            true
        case (.committed, .committed):
            true
        case (.finished, .finished):
            true
        case (.failedProvisionalNavigation(_), .failedProvisionalNavigation(_)):
            true
        case (.failed(_), .failed(_)):
            true
        default:
            false
        }
    }
}

extension WebPage.NavigationEvent: @retroactive Equatable {
    public static func == (lhs: Self, rhs: Self) -> Bool {
        lhs.kind == rhs.kind && lhs.navigationID == rhs.navigationID
    }
}

// MARK: Supporting test types

@MainActor
fileprivate class TestNavigationDecider: WebPage.NavigationDeciding {
    init() {
        (self.navigationActionStream, self.navigationActionContinuation) = AsyncStream.makeStream(of: WebPage.NavigationAction.self)
        (self.navigationResponseStream, self.navigationResponseContinuation) = AsyncStream.makeStream(of: WebPage.NavigationResponse.self)
    }

    let navigationActionStream: AsyncStream<WebPage.NavigationAction>
    private let navigationActionContinuation: AsyncStream<WebPage.NavigationAction>.Continuation

    let navigationResponseStream: AsyncStream<WebPage.NavigationResponse>
    private let navigationResponseContinuation: AsyncStream<WebPage.NavigationResponse>.Continuation

    var preferencesMutation: (inout WebPage.NavigationPreferences) -> Void = { _ in }

    func decidePolicy(for action: WebPage.NavigationAction, preferences: inout WebPage.NavigationPreferences) async -> WKNavigationActionPolicy {
        preferencesMutation(&preferences)

        navigationActionContinuation.yield(action)
        return .allow
    }

    func decidePolicy(for response: WebPage.NavigationResponse) async -> WKNavigationResponsePolicy {
        navigationResponseContinuation.yield(response)
        return .allow
    }
}

// MARK: Tests

@MainActor
struct WebPageTests {
    @Test
    func observableProperties() async throws {
        let page = WebPage()

        let html = """
        <html>
        <head>
            <title>Title</title>
        </head>
        <body></body>
        </html>
        """

        #expect(page.url == nil)
        #expect(page.title == "")
        #expect(!page.isLoading)
        #expect(page.estimatedProgress == 0.0)
        #expect(page.serverTrust == nil)
        #expect(!page.hasOnlySecureContent)
        #expect(page.themeColor == nil)

        // FIXME: (283456) Make this test more comprehensive once Observation supports observing a stream of changes to properties.
    }

    @Test
    func decidePolicyForNavigationActionFragment() async throws {
        let decider = TestNavigationDecider()
        let page = WebPage(navigationDecider: decider)

        let html = "<script>window.location.href='#fragment';</script>"

        let baseURL = URL(string: "http://webkit.org")!
        page.load(html: html, baseURL: baseURL)

        let actions = await Array(decider.navigationActionStream.prefix(2))

        #expect(actions[0].request.url!.absoluteString == "http://webkit.org/")
        #expect(actions[1].request.url!.absoluteString == "http://webkit.org/#fragment")
    }

    @Test
    func javaScriptEvaluation() async throws {
        let page = WebPage()

        let arguments = [
            "a": 1,
            "b": 2,
        ]

        let result = try await page.callJavaScript("return a + b;", arguments: arguments) as! Int
        #expect(result == 3)

        let nilResult = try await page.callJavaScript("console.log('hi')")
        #expect(nilResult == nil)
    }

    @Test
    func decidePolicyForNavigationResponse() async throws {
        let decider = TestNavigationDecider()
        let page = WebPage(navigationDecider: decider)

        let simpleURL = Bundle.testResources.url(forResource: "simple", withExtension: "html")!
        let request = URLRequest(url: simpleURL)

        page.load(request)

        let responses = await Array(decider.navigationResponseStream.prefix(1))

        #expect(responses[0].response.url!.absoluteString == simpleURL.absoluteString)
    }
}

#endif

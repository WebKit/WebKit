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

#if ENABLE_APP_PRIVACY_REPORT

private import TestWebKitAPILibrary
import Testing
import WebKit
private import WebKit_Private.WKWebViewPrivateForTesting

import struct Foundation.URL
import struct Swift.String

@MainActor
struct SiteIsolationAppPrivacyReportTests {
    @Test(.timeLimit(.minutes(1)), arguments: [URLRequest.Attribution.developer, .user])
    func crossSiteIframeLoadsAndNavigationsHaveMainFrameAttribution(attribution: URLRequest.Attribution) async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                "<iframe id='iframe' src='https://webkit.org/iframe'></iframe>"
            }

            Route("/iframe") {
                """
                <div id='box' style='width: 10px; height: 10px'></div>
                <script src='/iframe.js'></script>
                """
            }

            Route("/iframe.js", headerFields: ["Content-Type": "text/javascript"]) {
                "alert('iframe')"
            }

            Route("/image", headerFields: ["Content-Type": "image/png"]) {
                ""
            }

            Route("/navigated-iframe") {
                "<script>alert('navigated iframe')</script>"
            }

            Route("/self-navigated-iframe") {
                "<script>alert('self-navigated iframe')</script>"
            }
        }

        try await server.run { serverConfiguration in
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: .init(httpsProxyFor: serverConfiguration))
            let alerts = AlertRecorder()
            webView.uiDelegate = alerts

            var request = URLRequest(url: try #require(URL(string: "https://example.com/mainframe")))
            request.attribution = attribution

            try await webView.expectAttribution(attribution) {
                webView.load(request)
                try await navigationDelegate.waitForDidFinishNavigation()
                #expect(try await alerts.nextAlert() == "iframe")
            }

            try await webView.expectAttribution(attribution) {
                let iframe = try #require(await webView.firstChildFrame)
                try await webView.callJavaScript(in: iframe) { "box.style.backgroundImage = 'url(/image)'; box.offsetWidth" }
            }

            try await webView.expectAttribution(attribution) {
                try await webView.callJavaScript { "document.getElementById('iframe').src = 'https://webkit.org/navigated-iframe'" }
                #expect(try await alerts.nextAlert() == "navigated iframe")
            }

            try await webView.expectAttribution(attribution) {
                let iframe = try #require(await webView.firstChildFrame)
                try await webView.callJavaScript(in: iframe) { "location = '/self-navigated-iframe'" }
                #expect(try await alerts.nextAlert() == "self-navigated iframe")
            }
        }
    }
}

extension WKWebView {
    fileprivate func expectAttribution(
        _ attribution: URLRequest.Attribution,
        ofRequestsMadeBy action: () async throws -> Void,
        sourceLocation: SourceLocation = #_sourceLocation
    ) async throws {
        await _clearAppPrivacyReportTestingData()
        try await action()
        try await waitForCondition("a request to be recorded") {
            await !attributionsOfLoadedRequests().isEmpty
        }
        #expect(await attributionsOfLoadedRequests() == [attribution], sourceLocation: sourceLocation)
    }

    private func attributionsOfLoadedRequests() async -> Set<URLRequest.Attribution> {
        let testingData = await _appPrivacyReportTestingData()
        var attributions: Set<URLRequest.Attribution> = []
        if testingData.hasLoadedAppInitiatedRequestTesting.boolValue {
            attributions.insert(.developer)
        }
        if testingData.hasLoadedNonAppInitiatedRequestTesting.boolValue {
            attributions.insert(.user)
        }
        return attributions
    }
}

#endif // ENABLE_APP_PRIVACY_REPORT

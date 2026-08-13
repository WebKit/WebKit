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

import Foundation
private import TestWebKitAPILibrary
import Testing
import WebKit

import struct Foundation.URL
import struct Swift.String

@MainActor
private final class HostPageSchemeHandler: NSObject, WKURLSchemeHandler {
    private let pagesByLowercasedHost: [String: String]

    init(pagesByLowercasedHost: [String: String]) {
        self.pagesByLowercasedHost = pagesByLowercasedHost
    }

    func webView(_ webView: WKWebView, start urlSchemeTask: any WKURLSchemeTask) {
        guard let url = urlSchemeTask.request.url,
            let page = pagesByLowercasedHost[url.host(percentEncoded: false)?.lowercased() ?? ""]
        else {
            urlSchemeTask.didFailWithError(URLError(.fileDoesNotExist))
            return
        }

        urlSchemeTask.didReceive(URLResponse(url: url, mimeType: "text/html", expectedContentLength: -1, textEncodingName: "utf-8"))
        urlSchemeTask.didReceive(Data(page.utf8))
        urlSchemeTask.didFinish()
    }

    func webView(_ webView: WKWebView, stop urlSchemeTask: any WKURLSchemeTask) {}
}

@MainActor
@Suite(.timeLimit(.minutes(1)))
struct SiteIsolationPostMessageTests {
    @Test(arguments: ["MixedCase", "lowercase"])
    func messageFromCrossSiteIframeIsDelivered(iframeHost: String) async throws {
        let handler = HostPageSchemeHandler(pagesByLowercasedHost: [
            "parent": """
            <script>addEventListener('message', event => alert(event.data + ' ' + event.origin))</script>
            <iframe src='customscheme://\(iframeHost)/'></iframe>
            """,
            iframeHost.lowercased(): "<script>parent.postMessage('hello', '*')</script>",
        ])
        let configuration = WKWebViewConfiguration()
        configuration.setURLSchemeHandler(handler, forURLScheme: "customscheme")
        let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: configuration)
        let alerts = AlertRecorder()
        webView.uiDelegate = alerts

        webView.load(URLRequest(url: try #require(URL(string: "customscheme://parent/"))))
        try await navigationDelegate.waitForDidFinishNavigation()

        let iframeOrigin = "customscheme://\(iframeHost.lowercased())"
        #expect(try await alerts.nextAlert() == "hello \(iframeOrigin)")
        #expect(
            await webView.frameTreesInProcesses() == [
                .local("customscheme://parent", children: [.remote]),
                .remote(children: [.local(iframeOrigin)]),
            ]
        )
    }
}

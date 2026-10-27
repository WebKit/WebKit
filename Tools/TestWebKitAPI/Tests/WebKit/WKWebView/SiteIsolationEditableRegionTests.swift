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

#if ENABLE_EDITABLE_REGION

private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.WKWebViewConfigurationExtras
import Testing
import WebKit
private import WebKit_Private.WKWebViewPrivateForTestingIOS
private import WebKit_Private._WKTextInputContext

import struct Swift.String

@MainActor
struct SiteIsolationEditableRegionTests {
    @Test(.timeLimit(.minutes(1)))
    func requestingTextInputContextsBuildsEditableRegionInCrossOriginIframe() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                WebViewWithFocusedCrossOriginIframe.mainFrameHTML
            }

            // The touch-action makes the iframe's layers build an event region before the request, without an editable
            // region, so only invalidating them lets the request add one.
            Route("/iframe") {
                "<body style='touch-action: none'><input></body>"
            }
        }

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration._test_configurationWithTestPlugInClassName(
                "WebProcessPlugInWithInternals",
                configureJSCForTesting: true
            )
            configuration.websiteDataStore = WKWebViewConfiguration(httpsProxyFor: serverConfiguration).websiteDataStore

            let page = try await WebViewWithFocusedCrossOriginIframe(configuration: configuration)
            let webView = page.webView
            let childFrame = page.childFrame

            try await waitForCondition("the iframe to finish loading") {
                try await webView.callJavaScript(returning: String.self, in: childFrame) { "return document.readyState" } == "complete"
            }

            let eventRegionDump = {
                try await webView.callJavaScript(returning: String.self, in: childFrame) {
                    "return internals.layerTreeAsText(document, internals.LAYER_TREE_INCLUDES_EVENT_REGION | internals.LAYER_TREE_INCLUDES_ROOT_LAYER_PROPERTIES)"
                }
            }
            #expect(try await eventRegionDump().contains("(event region"))
            #expect(try await !eventRegionDump().contains("(editable region"))

            let contexts = try #require(await webView._requestTextInputContexts(in: webView.bounds))
            #expect(contexts.count == 1)

            #expect(try await eventRegionDump().contains("(editable region"))
        }
    }
}

#endif // ENABLE_EDITABLE_REGION

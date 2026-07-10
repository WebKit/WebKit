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

#if ENABLE_SWIFTUI && ENABLE_CXX_INTEROP

import Testing
@_spi(Testing) import WebKit
private import TestWebKitAPILibrary
private import WebKit_Private._WKWebsiteDataStoreConfiguration
private import WebKit_Private.WKWebsiteDataStorePrivate
import struct Swift.String
import struct Foundation.URL

@MainActor
private struct JSHandleNavigationDecider: WebPage.NavigationDeciding {
    mutating func decideAuthenticationChallengeDisposition(
        for challenge: URLAuthenticationChallenge
    ) async -> (URLSession.AuthChallengeDisposition, URLCredential?) {
        (.useCredential, challenge.protectionSpace.serverTrust.map(URLCredential.init(trust:)))
    }
}

@MainActor
struct JSHandleTests {
    @Test
    func basic() async throws {
        var server = HTTPServer(protocol: .httpsProxy) {
            Route("/example") {
                "<iframe id=onlyframe src='https://webkit.org/webkit'></iframe><div id=onlydiv></div>"
            }

            Route("/webkit") {
                "hi"
            }

            Route("/foobar") {
                "<p>after navigation</p>"
            }
        }

        try await server.run {
            let configuration = WebPage.Configuration($0)
            let page = WebPage(configuration: configuration, navigationDecider: JSHandleNavigationDecider())

            try await page.load(URL(string: "https://example.com/example")).wait()

            let worldConfiguration = WKContentWorld.Configuration()
            worldConfiguration.jsHandleCreationEnabled = true
            let world = WKContentWorld(configuration: worldConfiguration)

            var result = try await page.callJavaScript(returning: WKJSHandle.self, contentWorld: world) {
                """
                return window.webkit.createJSHandle(onlyframe.contentWindow);
                """
            }
            #expect(await result.windowProxyFrame?.request.url == URL(string: "https://webkit.org/webkit"))

            let iframeRef = try await page.callJavaScript(returning: WKJSHandle.self, contentWorld: world) {
                """
                return window.webkit.createJSHandle(onlyframe);
                """
            }

            result = try await page.callJavaScript(returning: WKJSHandle.self, contentWorld: world) {
                """
                return window.webkit.createJSHandle(onlydiv);
                """
            }
            #expect(await result.windowProxyFrame == nil)

            await #expect(throws: (any Error).self) {
                try await page.callJavaScript(contentWorld: world) {
                    """
                    window.webkit.createJSHandle(5);
                    """
                }
            }

            result = try await page.callJavaScript(returning: WKJSHandle.self, contentWorld: world) {
                """
                return window.webkit.createJSHandle(document.createTextNode('hi'));
                """
            }
            #expect(await result.windowProxyFrame == nil)
            #expect(result.contentWorld == world)

            let optionalHandle = try await page.callJavaScript(returning: WKJSHandle?.self, contentWorld: .page) {
                """
                return window.WebKitJSHandle;
                """
            }
            #expect(optionalHandle == nil)

            let argumentsDictionary = try await page.callJavaScript(returning: [String: WKJSHandle].self, contentWorld: world) {
                """
                return {'arg':window.webkit.createJSHandle(onlydiv)};
                """
            }

            let outerHTML = try await page.callJavaScript(returning: String.self, arguments: argumentsDictionary, contentWorld: world) {
                """
                return arg.outerHTML;
                """
            }
            #expect(outerHTML == "<div id=\"onlydiv\"></div>")

            let childFrame = try await #require(page.mainFrame?.childFrames.first?.info)
            let isIFrameRefUndefined = try await page.callJavaScript(
                returning: Bool.self,
                arguments: ["n": iframeRef],
                in: .init(childFrame),
                contentWorld: .page
            ) {
                """
                return n === undefined;
                """
            }
            #expect(isIFrameRefUndefined)

            let nodeID = try await page.callJavaScript(returning: String.self, arguments: ["n": iframeRef], contentWorld: world) {
                """
                return n.id;
                """
            }
            #expect(nodeID == "onlyframe")

            let funcRef = try await page.callJavaScript(returning: WKJSHandle.self, contentWorld: world) {
                """
                function returnThirty() { return '30'; }; 
                return window.webkit.createJSHandle(returnThirty);
                """
            }

            let functionResult = try await page.callJavaScript(returning: String.self, arguments: ["n": funcRef], contentWorld: world) {
                """
                return n();
                """
            }
            #expect(functionResult == "30")

            let plainObjectRef = try await page.callJavaScript(returning: WKJSHandle.self, contentWorld: world) {
                """
                return window.webkit.createJSHandle({greeting: 'hi'});
                """
            }

            let plainObjectResult = try await page.callJavaScript(
                returning: String.self,
                arguments: ["n": plainObjectRef],
                contentWorld: world
            ) {
                """
                return n.greeting;
                """
            }
            #expect(plainObjectResult == "hi")

            let firstFrame = try await #require(page.mainFrame?.childFrames.first?.info)
            result = try await page.callJavaScript(returning: WKJSHandle.self, in: .init(firstFrame), contentWorld: world) {
                """
                return window.webkit.createJSHandle(window.parent);
                """
            }
            #expect(await result.windowProxyFrame?.request.url == URL(string: "https://example.com/example"))

            // After top-level navigation, old JSHandles should be undefined.

            try await page.load(URL(string: "https://example.com/foobar")).wait()
            configuration.processPool?._garbageCollectJavaScriptObjectsForTesting()

            for ref in [iframeRef, funcRef, plainObjectRef] {
                let isUndefined = try await page.callJavaScript(returning: Bool.self, arguments: ["n": ref], contentWorld: world) {
                    """
                    return n === undefined;
                    """
                }
                #expect(isUndefined)
            }
        }
    }
}

extension WebPage.Configuration {
    fileprivate init(_ serverConfiguration: HTTPServer.Configuration) {
        self.init()

        let storeConfiguration = _WKWebsiteDataStoreConfiguration(nonPersistentConfiguration: ())
        storeConfiguration.httpsProxy = serverConfiguration.httpsProxy
        self.websiteDataStore = WKWebsiteDataStore._store(with: storeConfiguration)
    }
}

#endif // ENABLE_SWIFTUI && ENABLE_CXX_INTEROP

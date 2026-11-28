// Copyright (C) 2024-2026 Apple Inc. All rights reserved.
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
private import TestWebKitAPILibrary.Helpers.cocoa.TestNavigationDelegate
import Testing
import WebKit

import struct Foundation.URL
import struct Swift.String

@MainActor
struct RestoreSessionStorageTests {
    private func withDataStore(
        isPersistent: Bool,
        _ body: @MainActor (WKWebsiteDataStore) async throws -> Void
    ) async throws {
        if isPersistent {
            try await withTemporaryWebsiteDataStore { dataStore, _ in
                try await body(dataStore)
            }
        } else {
            try await body(.nonPersistent())
        }
    }

    @Test(arguments: [true, false])
    func restoreSessionStorage(isPersistent: Bool) async throws {
        try await withDataStore(isPersistent: isPersistent) { dataStore in
            await dataStore.removeData(ofTypes: WKWebsiteDataStore.allWebsiteDataTypes(), modifiedSince: .distantPast)

            let viewConfiguration = WKWebViewConfiguration()
            viewConfiguration.websiteDataStore = dataStore
            let webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 100, height: 100), configuration: viewConfiguration)
            defer { extendLifetime(webView) }

            let baseURL = URL(string: "https://webkit.org")
            try await webView.load(html: "<body>webkit.org</body>", baseURL: baseURL)

            try await webView.callJavaScript {
                "sessionStorage.setItem('key', 'value')"
            }

            let sessionStorageData = try await webView.fetchData(of: .sessionStorage)

            await dataStore.removeData(ofTypes: WKWebsiteDataStore.allWebsiteDataTypes(), modifiedSince: .distantPast)

            let newWebView = WKWebView(frame: CGRect(x: 0, y: 0, width: 100, height: 100), configuration: viewConfiguration)
            try await newWebView.restoreData(sessionStorageData)

            try await newWebView.load(html: "<body>webkit.org</body>", baseURL: baseURL)

            let restoredValue = try await newWebView.callJavaScript(returning: String.self) {
                "return sessionStorage.getItem('key')"
            }
            #expect(restoredValue == "value")
        }
    }

    @Test(arguments: [true, false])
    func restoreSessionStorageInThirdPartyIframe(isPersistent: Bool) async throws {
        var server = try HTTPServer(protocol: .https) {
            Route("/") {
                """
                <script>
                function postMessage(message)
                {
                    parent.postMessage(message, '*');
                }
                function item()
                {
                    result = sessionStorage.getItem('key');
                    if (!result) {
                        sessionStorage.setItem('key', 'value');
                        postMessage('Item is set');
                    } else
                        postMessage(result);
                }
                item();
                </script>
                """
            }
        }

        try await server.run { serverConfiguration in
            let mainFrameHTML = """
                <script>
                function postResult(event)
                {
                    window.webkit.messageHandlers.testHandler.postMessage(event.data);
                }
                addEventListener('message', postResult, false);
                </script>
                <iframe src='\(serverConfiguration.address.absoluteString)'>
                """

            try await withDataStore(isPersistent: isPersistent) { dataStore in
                await dataStore.removeData(ofTypes: WKWebsiteDataStore.allWebsiteDataTypes(), modifiedSince: .distantPast)

                let viewConfiguration = WKWebViewConfiguration()
                let messages = ScriptMessageRecorder(addingTo: viewConfiguration)
                viewConfiguration.websiteDataStore = dataStore

                let navigationDelegate = TestNavigationDelegate()
                navigationDelegate.allowAnyTLSCertificate()
                defer { extendLifetime(navigationDelegate) }

                let webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 100, height: 100), configuration: viewConfiguration)
                defer { extendLifetime(webView) }
                webView.navigationDelegate = navigationDelegate

                let baseURL = URL(string: "https://webkit.org")
                webView.loadHTMLString(mainFrameHTML, baseURL: baseURL)
                #expect(try await messages.nextStringMessage() == "Item is set")

                let sessionStorageData = try await webView.fetchData(of: .sessionStorage)

                await dataStore.removeData(ofTypes: WKWebsiteDataStore.allWebsiteDataTypes(), modifiedSince: .distantPast)

                let newWebView = WKWebView(frame: CGRect(x: 0, y: 0, width: 100, height: 100), configuration: viewConfiguration)
                defer { extendLifetime(newWebView) }
                newWebView.navigationDelegate = navigationDelegate

                try await newWebView.restoreData(sessionStorageData)

                newWebView.loadHTMLString(mainFrameHTML, baseURL: baseURL)
                #expect(try await messages.nextStringMessage() == "value")
            }
        }
    }
}

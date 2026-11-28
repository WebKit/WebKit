// Copyright (C) 2016-2026 Apple Inc. All rights reserved.
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
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
import Testing
import WebKit
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private.WKWebsiteDataStorePrivate

import struct Foundation.URL
import struct Swift.String

@MainActor
struct IndexedDBDatabaseProcessKillTests {
    @Test
    func databaseProcessKill() async throws {
        try await withTemporaryWebsiteDataStore { dataStore, _ in
            let configuration = WKWebViewConfiguration()
            configuration.websiteDataStore = dataStore
            let messages = ScriptMessageRecorder(addingTo: configuration)

            let webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
            defer { extendLifetime(webView) }
            webView.loadTestPageNamed("IndexedDBDatabaseProcessKill-1")

            var terminatedNetworkProcess = false
            var receivedOpenError = false
            var receivedDeleteError = false
            var receivedDatabaseError = false

            // The errors arrive in no fixed order, so wait for all of them rather than stopping at the database error.
            while !(receivedOpenError && receivedDeleteError && receivedDatabaseError) {
                switch try await messages.nextStringMessage() {
                case "OpenRequestUpgradeNeeded" where !terminatedNetworkProcess:
                    terminatedNetworkProcess = true
                    dataStore._terminateNetworkProcess()
                case "OpenRequestError":
                    receivedOpenError = true
                case "DeleteRequestError":
                    receivedDeleteError = true
                case "DatabaseErrorReceived":
                    receivedDatabaseError = true
                default:
                    break
                }
            }

            expectTrue(terminatedNetworkProcess)
        }
    }

    @Test
    func oneVMPerThread() async throws {
        let configuration = WKWebViewConfiguration()
        configuration.websiteDataStore = .nonPersistent()
        let messages = ScriptMessageRecorder(addingTo: configuration)

        let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
        let secondWebView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
        defer {
            extendLifetime(webView)
            extendLifetime(secondWebView)
        }

        let html = """
            <script>
            function openDatabase() {
                var request = indexedDB.open('testDB');
                request.onupgradeneeded = function(event) {
                    let db = event.target.result;
                    let os = db.createObjectStore('testOS');
                    for (let i = 0; i < 10000; i++)
                        os.put(i, i);
                    webkit.messageHandlers.testHandler.postMessage('Opened');
                };
            }
            </script>
            """

        let firstURL = try requireNonNil(URL(string: "https://webkit.org"))
        let secondURL = try requireNonNil(URL(string: "https://apple.com"))
        try await webView.load(html: html, baseURL: firstURL)
        try await secondWebView.load(html: html, baseURL: secondURL)

        webView.startEvaluatingJavaScript("openDatabase()", withUserGesture: false)
        expectEqual(try await messages.nextStringMessage(), "Opened")

        expectEqual(kill(webView._webProcessIdentifier, SIGKILL), 0)

        secondWebView.startEvaluatingJavaScript("openDatabase()", withUserGesture: false)
        expectEqual(try await messages.nextStringMessage(), "Opened")
    }
}

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

import struct Swift.String

@MainActor
struct IndexedDBMultiProcessTests {
    @Test
    func indexedDBMultiProcess() async throws {
        try await withTemporaryWebsiteDataStore { dataStore, _ in
            let configuration = WKWebViewConfiguration()
            configuration.websiteDataStore = dataStore
            let messages = ScriptMessageRecorder(addingTo: configuration)

            let webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
            defer { extendLifetime(webView) }
            webView.loadTestPageNamed("IndexedDBMultiProcess-1")

            expectEqual(try await messages.nextStringMessage(), "UpgradeNeeded")
            expectEqual(try await messages.nextStringMessage(), "Transaction complete")
            expectEqual(try await messages.nextStringMessage(), "Open success")

            let webView2 = WKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
            defer { extendLifetime(webView2) }
            webView2.loadTestPageNamed("IndexedDBMultiProcess-2")

            expectEqual(try await messages.nextStringMessage(), "Value of foo: bar")
            expectEqual(try await messages.nextStringMessage(), "Get loops started")

            let webView3 = WKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
            defer { extendLifetime(webView3) }
            webView3.loadTestPageNamed("IndexedDBMultiProcess-3")

            expectEqual(try await messages.nextStringMessage(), "Blocked!")

            // The get loops in webView2 keep the database open, so it can only be deleted once that process is gone.
            webView2._killWebContentProcessAndResetState()

            expectEqual(try await messages.nextStringMessage(), "Deleted!")
        }
    }
}

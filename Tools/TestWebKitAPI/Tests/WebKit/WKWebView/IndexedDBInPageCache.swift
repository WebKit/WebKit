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
struct IndexedDBInPageCacheTests {
    @Test
    func deletingDatabaseClosesConnectionOfPageInPageCache() async throws {
        try await withTemporaryWebsiteDataStore { dataStore, _ in
            let configuration = WKWebViewConfiguration()
            configuration.websiteDataStore = dataStore
            let messages = ScriptMessageRecorder(addingTo: configuration)

            let webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
            defer { extendLifetime(webView) }

            let holdsConnectionURL = try requireNonNil(Bundle.testResources.url(forResource: "IndexedDBInPageCache", withExtension: "html"))
            webView.load(URLRequest(url: holdsConnectionURL))
            expectEqual(try await messages.nextStringMessage(), "First Database Connection Opened")

            let deletesDatabaseURL = try requireNonNil(
                Bundle.testResources.url(forResource: "IndexedDBNotInPageCache", withExtension: "html")
            )
            webView.load(URLRequest(url: deletesDatabaseURL))
            expectEqual(try await messages.nextStringMessage(), "Database Deleted")

            webView.goBack()
            let expected =
                "First Database Connection Closed, Second Database Connection Not Failed, Third Database Connection Opened"
            expectEqual(try await messages.nextStringMessage(), expected)
        }
    }
}

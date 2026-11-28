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
private import WebKit_Private.WKNavigationDelegatePrivate
private import WebKit_Private._WKWebsiteDataStoreConfiguration

import struct Foundation.URL
import struct Swift.String

// The crypto keys in the database files were wrapped with this master key, so the page can only read them if the
// web view uses the same one.
@MainActor
private final class CryptoMasterKeyNavigationDelegate: NSObject, WKNavigationDelegatePrivate {
    // swift-format-ignore: NoLeadingUnderscores
    func _webCryptoMasterKey(for webView: WKWebView) -> Data {
        Data(0..<16)
    }
}

// The Networking process migrates files at the top of the directory when it starts, so they go there before the data
// store exists.
private func addBackwardCompatibleDatabase(to configuration: _WKWebsiteDataStoreConfiguration) throws {
    let databaseDirectory = configuration._indexedDBDatabaseDirectory.appending(
        path: "file__0/backward_compatibility",
        directoryHint: .isDirectory
    )
    try FileManager.default.createDirectory(at: databaseDirectory, withIntermediateDirectories: true)

    for suffix in ["", "-shm", "-wal"] {
        let source = try requireNonNil(
            Bundle.testResources.url(forResource: "IndexedDBStructuredCloneBackwardCompatibility", withExtension: "sqlite3" + suffix)
        )
        try FileManager.default.copyItem(at: source, to: databaseDirectory.appending(path: "IndexedDB.sqlite3" + suffix))
    }
}

@MainActor
struct IndexedDBStructuredCloneBackwardCompatibilityTests {
    @Test
    func structuredCloneBackwardCompatibility() async throws {
        try await withTemporaryWebsiteDataStore(configure: addBackwardCompatibleDatabase) { dataStore, _ in
            let configuration = WKWebViewConfiguration()
            configuration.websiteDataStore = dataStore
            let messages = ScriptMessageRecorder(addingTo: configuration)

            let webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
            let delegate = CryptoMasterKeyNavigationDelegate()
            webView.navigationDelegate = delegate
            defer {
                extendLifetime(webView)
                extendLifetime(delegate)
            }

            let readURL = try requireNonNil(
                Bundle.testResources.url(forResource: "IndexedDBStructuredCloneBackwardCompatibilityRead", withExtension: "html")
            )
            webView.load(URLRequest(url: readURL))
            expectEqual(try await messages.nextStringMessage(), "Pass")
        }
    }
}

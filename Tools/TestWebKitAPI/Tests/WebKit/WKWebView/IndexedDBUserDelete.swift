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
private import WebKit_Private._WKWebsiteDataStoreConfiguration

import struct Foundation.URL
import struct Swift.String

private func containsDatabaseFile(under directory: URL) -> Bool {
    let paths = (try? FileManager.default.subpathsOfDirectory(atPath: directory.path)) ?? []
    return paths.contains { $0 == "IndexedDB.sqlite3" || $0.hasSuffix("/IndexedDB.sqlite3") }
}

@MainActor
struct IndexedDBUserDeleteTests {
    @Test
    func userDelete() async throws {
        try await withTemporaryWebsiteDataStore { dataStore, _ in
            let configuration = WKWebViewConfiguration()
            configuration.websiteDataStore = dataStore
            let messages = ScriptMessageRecorder(addingTo: configuration)

            let webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
            defer { extendLifetime(webView) }

            let pageURL = try requireNonNil(Bundle.testResources.url(forResource: "IndexedDBUserDelete", withExtension: "html"))
            webView.load(URLRequest(url: pageURL))
            let message = try await messages.nextStringMessage()

            // The page starts many transactions after it posts its message, so the data is removed while they're pending.
            await dataStore.removeData(ofTypes: [WKWebsiteDataTypeIndexedDBDatabases], modifiedSince: .distantPast)

            expectEqual(message, "Continue")
        }
    }

    @Test
    func userDeleteBeforeLoading() async throws {
        try await withTemporaryWebsiteDataStore { dataStore, configuration in
            let databaseDirectory = configuration._indexedDBDatabaseDirectory.appending(
                path: "file__0/IndexedDBUserDeleteBeforeLoading",
                directoryHint: .isDirectory
            )
            try FileManager.default.createDirectory(at: databaseDirectory, withIntermediateDirectories: true)
            let source = try requireNonNil(Bundle.testResources.url(forResource: "IndexedDB", withExtension: "sqlite3"))
            try FileManager.default.copyItem(at: source, to: databaseDirectory.appending(path: "IndexedDB.sqlite3"))
            expectTrue(FileManager.default.fileExists(atPath: databaseDirectory.path))

            await dataStore.removeData(ofTypes: [WKWebsiteDataTypeIndexedDBDatabases], modifiedSince: .distantPast)

            // The Networking process moves the database into the directory of its origin when it starts, so the original
            // location is empty either way, and the database must not remain anywhere else.
            expectFalse(FileManager.default.fileExists(atPath: databaseDirectory.path))
            expectFalse(containsDatabaseFile(under: configuration._indexedDBDatabaseDirectory))
            let originDirectory = await dataStore.originDirectory(for: URL(filePath: "/"), type: WKWebsiteDataTypeIndexedDBDatabases)
            expectFalse(containsDatabaseFile(under: originDirectory))
        }
    }
}

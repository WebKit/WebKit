// Copyright (C) 2018-2026 Apple Inc. All rights reserved.
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

import CoreGraphics
import Foundation
private import TestWebKitAPILibrary
import Testing
import WebKit
private import WebKit_Private.WKWebsiteDataStorePrivate

import struct Foundation.URL
import struct Foundation.URLRequest
import struct Swift.String

@MainActor
struct IndexedDBTempFileSizeTests {
    @Test
    func walFileIsTruncatedWhenDatabaseIsReopened() async throws {
        try await withTemporaryWebsiteDataStore { dataStore, _ in
            let configuration = WKWebViewConfiguration()
            configuration.websiteDataStore = dataStore
            let messages = ScriptMessageRecorder(addingTo: configuration)

            let databaseRootDirectory = await dataStore.originDirectory(
                for: try #require(URL(string: "file://")),
                type: WKWebsiteDataTypeIndexedDBDatabases
            )
            let databaseDirectory = databaseRootDirectory.appending(
                path: sqliteFileNameHash("IndexedDBTempFileSize"),
                directoryHint: .isDirectory
            )
            let walFileURL = databaseDirectory.appending(path: "IndexedDB.sqlite3-wal", directoryHint: .notDirectory)

            do {
                let webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
                defer { extendLifetime(webView) }
                let firstPage = try #require(Bundle.testResources.url(forResource: "IndexedDBTempFileSize-1", withExtension: "html"))
                webView.load(URLRequest(url: firstPage))

                #expect(try await messages.nextStringMessage() == "UpgradeNeeded")
                #expect(try await messages.nextStringMessage() == "Success")
            }

            // Terminate the Networking process so closing the database doesn't checkpoint the WAL file.
            dataStore._terminateNetworkProcess()

            #expect(FileManager.default.fileExists(atPath: walFileURL.path))
            let fileSizeBefore = try #require(FileManager.default.attributesOfItem(atPath: walFileURL.path)[.size] as? Int64)

            let webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
            defer { extendLifetime(webView) }
            let secondPage = try #require(Bundle.testResources.url(forResource: "IndexedDBTempFileSize-2", withExtension: "html"))
            webView.load(URLRequest(url: secondPage))

            #expect(try await messages.nextStringMessage() == "Success")

            let fileSizeAfter = try #require(FileManager.default.attributesOfItem(atPath: walFileURL.path)[.size] as? Int64)
            #expect(fileSizeBefore > fileSizeAfter)
        }
    }
}

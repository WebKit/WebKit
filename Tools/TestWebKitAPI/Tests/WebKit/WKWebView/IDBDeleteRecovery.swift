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

import CoreGraphics
import Foundation
private import TestWebKitAPILibrary
import Testing
import WebKit
private import WebKit_Private.WKWebsiteDataStorePrivate

import struct Foundation.URL
import struct Swift.String

@MainActor
struct IDBDeleteRecoveryTests {
    @Test
    func deleteRecovery() async throws {
        try await withTemporaryWebsiteDataStore { dataStore, _ in
            let originURL = try requireNonNil(URL(string: "file://"))
            let indexedDBDirectory = await dataStore.originDirectory(for: originURL, type: WKWebsiteDataTypeIndexedDBDatabases)
            // Stop the Networking process started by originDirectory(for:type:) before populating the database files on disk.
            dataStore._terminateNetworkProcess()

            let databaseDirectory = indexedDBDirectory.appending(
                path: sqliteFileNameHash("IDBDeleteRecovery"),
                directoryHint: .isDirectory
            )
            try FileManager.default.createDirectory(at: databaseDirectory, withIntermediateDirectories: true)

            for suffix in ["sqlite3", "sqlite3-shm", "sqlite3-wal"] {
                let sourceURL = try requireNonNil(Bundle.testResources.url(forResource: "IDBDeleteRecovery", withExtension: suffix))
                try FileManager.default.copyItem(
                    at: sourceURL,
                    to: databaseDirectory.appending(path: "IndexedDB.\(suffix)", directoryHint: .notDirectory)
                )
            }

            let configuration = WKWebViewConfiguration()
            configuration.websiteDataStore = dataStore
            let messages = ScriptMessageRecorder(addingTo: configuration)

            let webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
            defer { extendLifetime(webView) }

            try await webView.load(testPageNamed: "IDBDeleteRecovery")
            let message = try await messages.nextStringMessage()
            expectEqual(message, "Deleted database")
            expectFalse(FileManager.default.fileExists(atPath: databaseDirectory.path(percentEncoded: false)))
        }
    }
}

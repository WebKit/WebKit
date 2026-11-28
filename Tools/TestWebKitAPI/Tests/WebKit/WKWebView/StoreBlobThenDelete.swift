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

import struct Foundation.URL
import struct Foundation.URLRequest
import struct Swift.String

@MainActor
private struct StoredBlob {
    let webView: WKWebView
    let messages: ScriptMessageRecorder
    let originDirectory: URL
    let databaseDirectory: URL

    var blobFileURL: URL {
        databaseDirectory.appending(path: "1.blob", directoryHint: .notDirectory)
    }

    var databaseFileURL: URL {
        databaseDirectory.appending(path: "IndexedDB.sqlite3", directoryHint: .notDirectory)
    }

    static func store(in dataStore: WKWebsiteDataStore) async throws -> StoredBlob {
        let configuration = WKWebViewConfiguration()
        configuration.websiteDataStore = dataStore
        let messages = ScriptMessageRecorder(addingTo: configuration)

        let webView = WKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
        let page = try #require(Bundle.testResources.url(forResource: "StoreBlobToBeDeleted", withExtension: "html"))
        webView.load(URLRequest(url: page))
        #expect(try await messages.nextStringMessage() == "Success")

        let originDirectory = await dataStore.originDirectory(
            for: try #require(URL(string: "file://")),
            type: WKWebsiteDataTypeIndexedDBDatabases
        )
        let databaseDirectory = originDirectory.appending(
            path: sqliteFileNameHash("StoreBlobToBeDeleted"),
            directoryHint: .isDirectory
        )
        let storedBlob = StoredBlob(
            webView: webView,
            messages: messages,
            originDirectory: originDirectory,
            databaseDirectory: databaseDirectory
        )

        #expect(FileManager.default.fileExists(atPath: storedBlob.blobFileURL.path))
        #expect(FileManager.default.fileExists(atPath: storedBlob.databaseFileURL.path))
        return storedBlob
    }
}

@MainActor
struct StoreBlobThenDeleteTests {
    @Test
    func storeBlobThenRemoveData() async throws {
        try await withTemporaryWebsiteDataStore { dataStore, _ in
            let storedBlob = try await StoredBlob.store(in: dataStore)

            // Orphaned -wal and -shm files without a .sqlite3 file should still be removed with their directory.
            let fakeDatabaseDirectory = storedBlob.originDirectory.appending(
                path: sqliteFileNameHash("FakeDatabasePath"),
                directoryHint: .isDirectory
            )
            let fakeWalURL = fakeDatabaseDirectory.appending(path: "IndexedDB.sqlite3-wal", directoryHint: .notDirectory)
            let fakeShmURL = fakeDatabaseDirectory.appending(path: "IndexedDB.sqlite3-shm", directoryHint: .notDirectory)
            try FileManager.default.createDirectory(at: fakeDatabaseDirectory, withIntermediateDirectories: true)
            try FileManager.default.copyItem(at: storedBlob.blobFileURL, to: fakeWalURL)
            try FileManager.default.copyItem(at: storedBlob.databaseFileURL, to: fakeShmURL)

            let otherBlobNames = ["7182.blob", "1a.blob", "a1.blob", ".blob"]
            let otherBlobURLs = otherBlobNames.map { name in
                storedBlob.databaseDirectory.appending(path: name, directoryHint: .notDirectory)
            }
            for otherBlobURL in otherBlobURLs {
                try FileManager.default.copyItem(at: storedBlob.blobFileURL, to: otherBlobURL)
            }

            await dataStore.removeData(ofTypes: WKWebsiteDataStore.allWebsiteDataTypes(), modifiedSince: .distantPast)

            #expect(!FileManager.default.fileExists(atPath: storedBlob.blobFileURL.path))
            #expect(!FileManager.default.fileExists(atPath: storedBlob.databaseFileURL.path))

            for otherBlobURL in otherBlobURLs {
                #expect(
                    !FileManager.default.fileExists(atPath: otherBlobURL.path),
                    "\(otherBlobURL.lastPathComponent) should have been removed"
                )
            }

            #expect(!FileManager.default.fileExists(atPath: fakeWalURL.path))
            #expect(!FileManager.default.fileExists(atPath: fakeShmURL.path))
            #expect(!FileManager.default.fileExists(atPath: fakeDatabaseDirectory.path))
        }
    }

    @Test
    func storeBlobThenDeleteDatabase() async throws {
        try await withTemporaryWebsiteDataStore { dataStore, _ in
            let storedBlob = try await StoredBlob.store(in: dataStore)

            let anotherBlobFileURL = storedBlob.databaseDirectory.appending(
                path: "7182.blob",
                directoryHint: .notDirectory
            )
            try FileManager.default.copyItem(at: storedBlob.blobFileURL, to: anotherBlobFileURL)

            try await storedBlob.webView.callJavaScript {
                "deleteDatabase(() => { sendMessage('Delete success'); })"
            }
            #expect(try await storedBlob.messages.nextStringMessage() == "Delete success")

            #expect(!FileManager.default.fileExists(atPath: storedBlob.blobFileURL.path))
            #expect(!FileManager.default.fileExists(atPath: storedBlob.databaseFileURL.path))
            #expect(FileManager.default.fileExists(atPath: anotherBlobFileURL.path))
        }
    }
}

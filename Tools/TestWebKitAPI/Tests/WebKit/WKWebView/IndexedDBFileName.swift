// Copyright (C) 2019-2026 Apple Inc. All rights reserved.
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
private import WebKit_Private.WKWebsiteDataStorePrivate
private import WebKit_Private._WKWebsiteDataStoreConfiguration

import struct Foundation.URL
import struct Foundation.URLRequest
import struct Swift.String

// Without unified origin storage, a data store keeps the IndexedDB databases of each origin in a directory under its
// IndexedDB directory. That layout has changed over time:
//
//     IndexedDB/
//         <origin>/<database name>/IndexedDB.sqlite3              Unversioned.
//         v0 -> IndexedDB/                                        A symbolic link to the IndexedDB directory.
//         v1/<origin>/<hash of database name>/IndexedDB.sqlite3   The current layout.
//
// When the Networking process first uses the directory, it replaces the v0 symbolic link with a directory and moves the
// unversioned origin directories into it. When an origin is accessed, it moves every database of the origin from v0 to
// v1, renaming each by the hash of its name.

private let existingDatabaseName = "IndexedDBTest"
private let createdDatabaseName = "IndexedDBOther"
private let unusedDatabaseName = "IndexedDBUnused"
private let fileOrigin = "file__0"

private func copyDatabase(_ resourceName: String = "IndexedDB", toDirectory directoryURL: URL) throws {
    let resourceURL = try #require(Bundle.testResources.url(forResource: resourceName, withExtension: "sqlite3"))
    try FileManager.default.createDirectory(at: directoryURL, withIntermediateDirectories: true)
    try FileManager.default.copyItem(at: resourceURL, to: directoryURL.appending(path: "IndexedDB.sqlite3"))
}

private func createDirectoriesWithOtherOrigins(in rootURL: URL) throws {
    try IndexedDBFileNameTests.DirectoryLayout.v1.createDirectories(in: rootURL)
    try copyDatabase(toDirectory: rootURL.appending(components: "https_apple.com_0", existingDatabaseName, directoryHint: .isDirectory))
    try copyDatabase(
        toDirectory: rootURL.appending(
            components: "v1",
            "https_webkit.org_0",
            sqliteFileNameHash(existingDatabaseName),
            directoryHint: .isDirectory
        )
    )
}

// Places a database holding "IndexedDBTest" in the directory for the hash of "IndexedDBOther", simulating a hash collision.
private func createDirectoriesWithHashCollision(in rootURL: URL) throws {
    try createDirectoriesWithOtherOrigins(in: rootURL)
    try copyDatabase(
        toDirectory: rootURL.appending(
            components: "v1",
            fileOrigin,
            sqliteFileNameHash(createdDatabaseName),
            directoryHint: .isDirectory
        )
    )
}

private func fileExists(_ url: URL) -> Bool {
    FileManager.default.fileExists(atPath: url.path(percentEncoded: false))
}

@MainActor
struct IndexedDBFileNameTests {
    enum DirectoryLayout: CaseIterable, Sendable {
        case unversioned
        case v0
        case v1

        func createDirectories(in rootURL: URL) throws {
            let unversionedOriginURL = rootURL.appending(path: fileOrigin, directoryHint: .isDirectory)
            try copyDatabase(toDirectory: unversionedOriginURL.appending(path: unusedDatabaseName, directoryHint: .isDirectory))

            if self == .v1 {
                try copyDatabase(
                    toDirectory: rootURL.appending(
                        components: "v1",
                        fileOrigin,
                        sqliteFileNameHash(existingDatabaseName),
                        directoryHint: .isDirectory
                    )
                )
            } else {
                try copyDatabase(toDirectory: unversionedOriginURL.appending(path: existingDatabaseName, directoryHint: .isDirectory))
            }

            if self != .unversioned {
                try FileManager.default.createSymbolicLink(at: rootURL.appending(path: "v0"), withDestinationURL: rootURL)
            }
        }
    }

    private func withLegacyIndexedDBDataStore(
        populate: (URL) throws -> Void,
        _ body: @MainActor (WKWebsiteDataStore, URL) async throws -> Void
    ) async throws {
        try await withTemporaryDirectory(prefix: "WebsiteData") { directoryURL in
            let configuration = _WKWebsiteDataStoreConfiguration(directory: directoryURL)
            configuration.unifiedOriginStorageLevel = .none

            // Create the files before the data store exists so its Networking process sees them on first access.
            let indexedDBDirectoryURL = configuration._indexedDBDatabaseDirectory
            try populate(indexedDBDirectoryURL)

            let dataStore = WKWebsiteDataStore._store(with: configuration)
            defer { dataStore._terminateNetworkProcess() }

            try await body(dataStore, indexedDBDirectoryURL)
        }
    }

    private func withBakedDatabase(named resourceName: String, _ body: @MainActor (WKWebsiteDataStore) async throws -> Void) async throws {
        try await withTemporaryWebsiteDataStore { dataStore, _ in
            let originURL = try #require(URL(string: "file://"))
            let originDirectoryURL = await dataStore.originDirectory(for: originURL, type: WKWebsiteDataTypeIndexedDBDatabases)
            try copyDatabase(
                resourceName,
                toDirectory: originDirectoryURL.appending(path: sqliteFileNameHash(existingDatabaseName), directoryHint: .isDirectory)
            )

            try await body(dataStore)
        }
    }

    private func makeWebView(dataStore: WKWebsiteDataStore) -> (WKWebView, ScriptMessageRecorder) {
        let configuration = WKWebViewConfiguration()
        configuration.websiteDataStore = dataStore
        let recorder = ScriptMessageRecorder(addingTo: configuration)
        return (WKWebView(frame: .zero, configuration: configuration), recorder)
    }

    private func openDatabase(inPageNamed pageName: String, webView: WKWebView, recorder: ScriptMessageRecorder) async throws -> String {
        defer { extendLifetime(webView) }
        let pageURL = try #require(Bundle.testResources.url(forResource: pageName, withExtension: "html"))
        try await webView.loadAndWait(URLRequest(url: pageURL))
        return try await recorder.nextStringMessage()
    }

    @Test(arguments: DirectoryLayout.allCases)
    func migratesDatabaseDirectories(from layout: DirectoryLayout) async throws {
        try await withLegacyIndexedDBDataStore(populate: layout.createDirectories(in:)) { dataStore, rootURL in
            let (webView, recorder) = makeWebView(dataStore: dataStore)

            let createResult = try await openDatabase(inPageNamed: "IndexedDBFileName-1", webView: webView, recorder: recorder)
            #expect(createResult == "Success")

            let openResult = try await openDatabase(inPageNamed: "IndexedDBFileName-2", webView: webView, recorder: recorder)
            #expect(openResult == "Success")

            let oldOriginURL = rootURL.appending(components: "v0", fileOrigin, directoryHint: .isDirectory)
            let newOriginURL = rootURL.appending(components: "v1", fileOrigin, directoryHint: .isDirectory)
            for databaseName in [existingDatabaseName, createdDatabaseName, unusedDatabaseName] {
                #expect(!fileExists(oldOriginURL.appending(path: databaseName)), "\(databaseName) is still in v0")
                #expect(fileExists(newOriginURL.appending(path: sqliteFileNameHash(databaseName))), "\(databaseName) is not in v1")
            }
        }
    }

    @Test
    func fetchAndRemoveDataRecordsAcrossVersions() async throws {
        try await withLegacyIndexedDBDataStore(populate: createDirectoriesWithOtherOrigins(in:)) { dataStore, rootURL in
            let types: Set = [WKWebsiteDataTypeIndexedDBDatabases]

            let records = await dataStore.dataRecords(ofTypes: types)
            #expect(records.count == 3, "\(records)")

            await dataStore.removeData(ofTypes: types, modifiedSince: .distantPast)

            let remainingNames = try FileManager.default.contentsOfDirectory(atPath: rootURL.path(percentEncoded: false))
            #expect(remainingNames.count == 2, "\(remainingNames)")
            let newVersionURL = rootURL.appending(path: "v1", directoryHint: .isDirectory)
            #expect(fileExists(newVersionURL))
            let newVersionNames = try FileManager.default.contentsOfDirectory(atPath: newVersionURL.path(percentEncoded: false))
            #expect(newVersionNames == [])
        }
    }

    @Test
    func hashCollisionFailsToOpen() async throws {
        try await withLegacyIndexedDBDataStore(populate: createDirectoriesWithHashCollision(in:)) { dataStore, _ in
            let (webView, recorder) = makeWebView(dataStore: dataStore)

            let result = try await openDatabase(inPageNamed: "IndexedDBFileName-1", webView: webView, recorder: recorder)
            #expect(result == "Error")
        }
    }

    @Test
    func openDatabaseAfterDatabaseNameUpgrade() async throws {
        try await withBakedDatabase(named: "IndexedDB") { dataStore in
            let (webView, recorder) = makeWebView(dataStore: dataStore)

            let upgradeResult = try await openDatabase(inPageNamed: "IndexedDBFileName-2", webView: webView, recorder: recorder)
            #expect(upgradeResult == "Success")

            dataStore._terminateNetworkProcess()
            webView.reload()
            defer { extendLifetime(webView) }
            let reopenResult = try await recorder.nextStringMessage()
            #expect(reopenResult == "Success")
        }
    }

    @Test
    func openDatabaseWithMismatchedMetadataVersionAndName() async throws {
        try await withBakedDatabase(named: "IndexedDBWrongMetadataVersion") { dataStore in
            let (webView, recorder) = makeWebView(dataStore: dataStore)

            let result = try await openDatabase(inPageNamed: "IndexedDBFileName-2", webView: webView, recorder: recorder)
            #expect(result == "Success")
        }
    }
}

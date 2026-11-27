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

// Helpers for tests that need an isolated persistent website data store, or that inspect the files a data store keeps
// on disk.

private import CryptoKit
public import Foundation
public import WebKit
private import WebKit_Private.WKWebsiteDataStorePrivate
public import WebKit_Private._WKWebsiteDataStoreConfiguration

public import struct Foundation.URL
public import struct Swift.String

/// Runs a body with a new, empty directory in the temporary directory, and removes the directory afterward.
///
/// - Parameters:
///   - prefix: The start of the directory's name. The rest is unique to each call.
///   - body: The code to run, which receives the URL of the directory.
/// - Returns: The value the body returns.
/// - Throws: The error from creating the directory, or any error the body throws.
@MainActor
public func withTemporaryDirectory<Result>(
    prefix: String = "TestWebKitAPI",
    _ body: @MainActor (URL) async throws -> Result
) async throws -> Result {
    let directoryURL = FileManager.default.temporaryDirectory.appending(
        path: "\(prefix)-\(UUID().uuidString)",
        directoryHint: .isDirectory
    )
    try FileManager.default.createDirectory(at: directoryURL, withIntermediateDirectories: true)
    defer { try? FileManager.default.removeItem(at: directoryURL) }

    return try await body(directoryURL)
}

/// Runs a body with a persistent website data store that keeps its files in a new temporary directory.
///
/// The data store is isolated from every other store, so a test can create data in it and then read it back from the
/// files on disk. The Networking process of the data store is terminated after the body, so that it releases the files
/// before the directory is removed.
///
/// - Parameters:
///   - configure: Changes the configuration of the data store before the store is created. A data store copies its
///     configuration, so a change made after creating the store has no effect on it.
///   - body: The code to run, which receives the data store and the configuration the store was created from.
/// - Returns: The value the body returns.
/// - Throws: Any error `configure` or the body throws, or the error from creating the directory.
@MainActor
public func withTemporaryWebsiteDataStore<Result>(
    configure: (_WKWebsiteDataStoreConfiguration) throws -> Void = { _ in },
    _ body: @MainActor (WKWebsiteDataStore, _WKWebsiteDataStoreConfiguration) async throws -> Result
) async throws -> Result {
    try await withTemporaryDirectory(prefix: "WebsiteData") { directoryURL in
        let configuration = _WKWebsiteDataStoreConfiguration(directory: directoryURL)
        try configure(configuration)

        let dataStore = WKWebsiteDataStore._store(with: configuration)
        defer { dataStore._terminateNetworkProcess() }

        return try await body(dataStore, configuration)
    }
}

extension WKWebsiteDataStore {
    /// Finds the directory where the data store keeps one type of data for an origin.
    ///
    /// - Parameters:
    ///   - origin: The origin that owns the data.
    ///   - topOrigin: The origin of the top-level page that embeds `origin`. Defaults to `origin`.
    ///   - type: The type of data, such as `WKWebsiteDataTypeIndexedDBDatabases`.
    /// - Returns: The URL of the directory, which might not exist yet.
    @MainActor
    public func originDirectory(for origin: URL, topOrigin: URL? = nil, type: String) async -> URL {
        let path = await _originDirectory(forTesting: origin, topOrigin: topOrigin ?? origin, type: type)
        return URL(filePath: path, directoryHint: .isDirectory)
    }
}

/// Computes the hash that WebKit uses in the name of a SQLite database file, for a name given by a page.
///
/// This matches `WebCore::SQLiteFileSystem::computeHashForFileName`, which hashes the UTF-8 bytes of the name, without a
/// null terminator, with SHA-256. Computing it here means that tests don't need to import WebCore.
///
/// - Parameter fileName: The name to hash.
/// - Returns: The 64 lowercase hexadecimal digits of the hash.
public func sqliteFileNameHash(_ fileName: String) -> String {
    SHA256.hash(data: Data(fileName.utf8))
        .map { ($0 < 16 ? "0" : "") + String($0, radix: 16) }
        .joined()
}

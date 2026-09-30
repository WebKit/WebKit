// Copyright (C) 2024-2026 Apple Inc. All rights reserved.
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

#if ENABLE_WK_WEB_EXTENSIONS

import Foundation
import Testing
import WebKit
private import WebKit_Private.WKWebExtensionControllerConfigurationPrivate

import struct Swift.String

@MainActor
struct WKWebExtensionControllerConfigurationTests {
    @Test
    func initialization() throws {
        var configuration = WKWebExtensionController.Configuration.default()

        #expect(configuration.isPersistent)
        #expect(configuration.identifier == nil)
        #expect(!configuration._isTemporary)
        #expect(configuration.webViewConfiguration != nil)
        #expect(configuration !== WKWebExtensionController.Configuration.default())
        #expect(configuration != WKWebExtensionController.Configuration.default())
        #expect(configuration.webViewConfiguration != WKWebExtensionController.Configuration.default().webViewConfiguration)
        #expect(configuration._storageDirectoryPath == WKWebExtensionController.Configuration.default()._storageDirectoryPath)
        #expect(configuration.defaultWebsiteDataStore == WKWebsiteDataStore.default())

        configuration = WKWebExtensionController.Configuration.nonPersistent()

        #expect(!configuration.isPersistent)
        #expect(configuration.identifier == nil)
        #expect(!configuration._isTemporary)
        #expect(configuration.webViewConfiguration != nil)
        #expect(configuration._storageDirectoryPath == nil)
        #expect(configuration !== WKWebExtensionController.Configuration.nonPersistent())
        #expect(configuration != WKWebExtensionController.Configuration.nonPersistent())
        #expect(configuration.webViewConfiguration != WKWebExtensionController.Configuration.nonPersistent().webViewConfiguration)
        #expect(!configuration.defaultWebsiteDataStore.isPersistent)
        #expect(!configuration.webViewConfiguration.websiteDataStore.isPersistent)
        #expect(configuration.defaultWebsiteDataStore == configuration.webViewConfiguration.websiteDataStore)

        let identifier = UUID()
        configuration = WKWebExtensionController.Configuration(identifier: identifier)

        #expect(configuration.isPersistent)
        #expect(configuration.identifier == identifier)
        #expect(!configuration._isTemporary)
        #expect(configuration.webViewConfiguration != nil)
        #expect(configuration !== WKWebExtensionController.Configuration(identifier: identifier))
        #expect(configuration != WKWebExtensionController.Configuration(identifier: identifier))
        #expect(configuration.webViewConfiguration != WKWebExtensionController.Configuration(identifier: identifier).webViewConfiguration)
        #expect(configuration._storageDirectoryPath == WKWebExtensionController.Configuration(identifier: identifier)._storageDirectoryPath)
        #expect(configuration.defaultWebsiteDataStore == WKWebsiteDataStore.default())

        configuration = WKWebExtensionController.Configuration._temporary()

        #expect(configuration != WKWebExtensionController.Configuration._temporary())

        #expect(configuration.isPersistent)
        #expect(configuration._isTemporary)
        #expect(configuration.identifier == nil)
        #expect(configuration.webViewConfiguration != nil)
        #expect(configuration !== WKWebExtensionController.Configuration._temporary())
        #expect(configuration != WKWebExtensionController.Configuration._temporary())
        #expect(configuration.webViewConfiguration != WKWebExtensionController.Configuration.nonPersistent().webViewConfiguration)
        #expect(configuration._storageDirectoryPath != WKWebExtensionController.Configuration._temporary()._storageDirectoryPath)
        #expect(configuration.defaultWebsiteDataStore == WKWebsiteDataStore.default())
    }

    @Test
    func secureCoding() throws {
        var configuration = WKWebExtensionController.Configuration.default()
        var data = try NSKeyedArchiver.archivedData(withRootObject: configuration, requiringSecureCoding: true)
        var result = try #require(
            try NSKeyedUnarchiver.unarchivedObject(ofClass: WKWebExtensionController.Configuration.self, from: data)
        )

        #expect(result.isPersistent)
        #expect(result.identifier == nil)
        #expect(!result._isTemporary)
        #expect(result._storageDirectoryPath == configuration._storageDirectoryPath)
        #expect(result.webViewConfiguration != nil)
        #expect(result.defaultWebsiteDataStore == WKWebsiteDataStore.default())
        #expect(configuration !== result)
        #expect(result != configuration)
        #expect(result.webViewConfiguration != configuration.webViewConfiguration)
        #expect(result.defaultWebsiteDataStore == configuration.defaultWebsiteDataStore)

        configuration = WKWebExtensionController.Configuration.nonPersistent()
        data = try NSKeyedArchiver.archivedData(withRootObject: configuration, requiringSecureCoding: true)
        result = try #require(
            try NSKeyedUnarchiver.unarchivedObject(ofClass: WKWebExtensionController.Configuration.self, from: data)
        )

        #expect(!result.isPersistent)
        #expect(result.identifier == nil)
        #expect(!result._isTemporary)
        #expect(result._storageDirectoryPath == configuration._storageDirectoryPath)
        #expect(result.webViewConfiguration != nil)
        #expect(!result.defaultWebsiteDataStore.isPersistent)
        #expect(!result.webViewConfiguration.websiteDataStore.isPersistent)
        #expect(configuration !== result)
        #expect(result != configuration)
        #expect(result.webViewConfiguration != configuration.webViewConfiguration)

        let identifier = UUID()
        configuration = WKWebExtensionController.Configuration(identifier: identifier)
        data = try NSKeyedArchiver.archivedData(withRootObject: configuration, requiringSecureCoding: true)
        result = try #require(
            try NSKeyedUnarchiver.unarchivedObject(ofClass: WKWebExtensionController.Configuration.self, from: data)
        )

        #expect(result.isPersistent)
        #expect(!result._isTemporary)
        #expect(result._storageDirectoryPath == configuration._storageDirectoryPath)
        #expect(result.webViewConfiguration != nil)
        #expect(result.identifier == identifier)
        #expect(result.defaultWebsiteDataStore == WKWebsiteDataStore.default())
        #expect(configuration !== result)
        #expect(result != configuration)
        #expect(result.webViewConfiguration != configuration.webViewConfiguration)
        #expect(result.defaultWebsiteDataStore == configuration.defaultWebsiteDataStore)

        configuration = WKWebExtensionController.Configuration._temporary()
        data = try NSKeyedArchiver.archivedData(withRootObject: configuration, requiringSecureCoding: true)
        result = try #require(
            try NSKeyedUnarchiver.unarchivedObject(ofClass: WKWebExtensionController.Configuration.self, from: data)
        )

        #expect(result.isPersistent)
        #expect(result.identifier == nil)
        #expect(result._isTemporary)
        #expect(result._storageDirectoryPath == configuration._storageDirectoryPath)
        #expect(result.webViewConfiguration != nil)
        #expect(result.defaultWebsiteDataStore == WKWebsiteDataStore.default())
        #expect(configuration !== result)
        #expect(result != configuration)
        #expect(result.webViewConfiguration != configuration.webViewConfiguration)
        #expect(result.defaultWebsiteDataStore == configuration.defaultWebsiteDataStore)
    }

    @Test
    func copying() throws {
        var configuration = WKWebExtensionController.Configuration.default()
        var copy = try #require(configuration.copy() as? WKWebExtensionController.Configuration)

        #expect(copy.isPersistent)
        #expect(copy.identifier == nil)
        #expect(!copy._isTemporary)
        #expect(copy._storageDirectoryPath == configuration._storageDirectoryPath)
        #expect(copy.webViewConfiguration != nil)
        #expect(copy.defaultWebsiteDataStore == WKWebsiteDataStore.default())
        #expect(configuration !== copy)
        #expect(copy != configuration)
        #expect(copy.webViewConfiguration != configuration.webViewConfiguration)
        #expect(copy.defaultWebsiteDataStore == configuration.defaultWebsiteDataStore)

        configuration = WKWebExtensionController.Configuration.nonPersistent()
        copy = try #require(configuration.copy() as? WKWebExtensionController.Configuration)

        #expect(!copy.isPersistent)
        #expect(copy.identifier == nil)
        #expect(!copy._isTemporary)
        #expect(copy._storageDirectoryPath == configuration._storageDirectoryPath)
        #expect(copy.webViewConfiguration != nil)
        #expect(!copy.defaultWebsiteDataStore.isPersistent)
        #expect(!copy.webViewConfiguration.websiteDataStore.isPersistent)
        #expect(configuration !== copy)
        #expect(copy != configuration)
        #expect(copy.webViewConfiguration != configuration.webViewConfiguration)
        #expect(copy.defaultWebsiteDataStore == configuration.defaultWebsiteDataStore)

        let identifier = UUID()
        configuration = WKWebExtensionController.Configuration(identifier: identifier)
        copy = try #require(configuration.copy() as? WKWebExtensionController.Configuration)

        #expect(copy.isPersistent)
        #expect(copy.identifier == identifier)
        #expect(!copy._isTemporary)
        #expect(copy._storageDirectoryPath == configuration._storageDirectoryPath)
        #expect(copy.webViewConfiguration != nil)
        #expect(copy.defaultWebsiteDataStore == WKWebsiteDataStore.default())
        #expect(configuration !== copy)
        #expect(copy != configuration)
        #expect(copy.webViewConfiguration != configuration.webViewConfiguration)
        #expect(copy.defaultWebsiteDataStore == configuration.defaultWebsiteDataStore)

        configuration = WKWebExtensionController.Configuration._temporary()
        copy = try #require(configuration.copy() as? WKWebExtensionController.Configuration)

        #expect(copy.isPersistent)
        #expect(copy.identifier == nil)
        #expect(copy._isTemporary)
        #expect(copy._storageDirectoryPath == configuration._storageDirectoryPath)
        #expect(copy.webViewConfiguration != nil)
        #expect(copy.defaultWebsiteDataStore == WKWebsiteDataStore.default())
        #expect(configuration !== copy)
        #expect(copy != configuration)
        #expect(copy.webViewConfiguration != configuration.webViewConfiguration)
        #expect(copy.defaultWebsiteDataStore == configuration.defaultWebsiteDataStore)
    }

    @Test
    func webViewConfigurationWithCopyAndCoding() throws {
        let originalConfiguration = WKWebExtensionController.Configuration.default()

        #expect(originalConfiguration.webViewConfiguration != nil)

        let newWebViewConfiguration = WKWebViewConfiguration()
        originalConfiguration.webViewConfiguration = newWebViewConfiguration
        #expect(originalConfiguration.webViewConfiguration == newWebViewConfiguration)

        let copiedConfiguration = try #require(originalConfiguration.copy() as? WKWebExtensionController.Configuration)
        #expect(copiedConfiguration != originalConfiguration)
        #expect(copiedConfiguration.webViewConfiguration != newWebViewConfiguration)

        let encodedData = try NSKeyedArchiver.archivedData(withRootObject: originalConfiguration, requiringSecureCoding: true)

        let decodedConfiguration = try #require(
            try NSKeyedUnarchiver.unarchivedObject(ofClass: WKWebExtensionController.Configuration.self, from: encodedData)
        )

        #expect(decodedConfiguration != originalConfiguration)
        #expect(decodedConfiguration.webViewConfiguration != newWebViewConfiguration)

        originalConfiguration.webViewConfiguration = nil
        #expect(originalConfiguration.webViewConfiguration != nil)
        #expect(originalConfiguration.webViewConfiguration != newWebViewConfiguration)
    }

    @Test
    func defaultWebsiteDataStoreWithCopyAndCoding() throws {
        let originalConfiguration = WKWebExtensionController.Configuration.default()

        #expect(originalConfiguration.defaultWebsiteDataStore == WKWebsiteDataStore.default())

        let newDataStore = WKWebsiteDataStore.nonPersistent()
        originalConfiguration.defaultWebsiteDataStore = newDataStore
        #expect(originalConfiguration.defaultWebsiteDataStore == newDataStore)

        let copiedConfiguration = try #require(originalConfiguration.copy() as? WKWebExtensionController.Configuration)
        #expect(copiedConfiguration == originalConfiguration)
        #expect(copiedConfiguration.defaultWebsiteDataStore == newDataStore)

        let encodedData = try NSKeyedArchiver.archivedData(withRootObject: originalConfiguration, requiringSecureCoding: true)

        let decodedConfiguration = try #require(
            try NSKeyedUnarchiver.unarchivedObject(ofClass: WKWebExtensionController.Configuration.self, from: encodedData)
        )

        #expect(decodedConfiguration != originalConfiguration)
        #expect(decodedConfiguration.defaultWebsiteDataStore != newDataStore)
        #expect(decodedConfiguration.defaultWebsiteDataStore != WKWebsiteDataStore.default())

        originalConfiguration.defaultWebsiteDataStore = nil
        #expect(originalConfiguration.defaultWebsiteDataStore == WKWebsiteDataStore.default())
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS

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
private import WebKit_Private.WKWebExtensionPrivate

import struct Swift.String

@MainActor
struct WKWebExtensionDataRecordTests {
    private let dataRecordTestManifest: [String: Any] = [
        "manifest_version": 3,
        "name": "DataRecord",
        "permissions": ["storage"],
        "background": ["scripts": ["background.js"], "type": "module", "persistent": false],
    ]

    private let dataRecordTestTwoManifest: [String: Any] = [
        "manifest_version": 3,
        "name": "DataRecordTwo",
        "permissions": ["storage"],
        "background": ["scripts": ["background.js"], "type": "module", "persistent": false],
    ]

    private let allDataTypes: Set<WKWebExtension.DataType> = [.local, .session, .synchronized]

    // FIXME rdar://167044676 for macOS
    #if WTF_PLATFORM_MAC
    @Test(.disabled("rdar://167044676"))
    #else
    @Test
    #endif
    func getDataRecords() async throws {
        let backgroundScript = """
            const data = { 'string': 'string', 'number': 1, 'boolean': true, 'dictionary': {'key': 'value'}, 'array': [1, true, 'string'] }
            await browser?.storage?.local?.set(data)
            await browser?.storage?.session?.set(data)
            await browser?.storage?.sync?.set(data)
            """

        let extensionToTest = try #require(
            WKWebExtension(manifestDictionary: dataRecordTestManifest, resources: ["background.js": backgroundScript])
        )
        let testController = WKWebExtensionController(configuration: ._temporary())

        let context = WKWebExtensionContext(for: extensionToTest)
        context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.storage)

        // Give the extension a unique identifier so it opts into saving data in the temporary configuration.
        context.uniqueIdentifier = "org.webkit.test.extension (76C788B8)"

        try testController.load(context)

        // Wait for the background page to load.
        try await Task.sleep(for: .seconds(4))

        let dataRecord = try #require(await testController.dataRecord(ofTypes: allDataTypes, for: context))

        #expect(dataRecord.errors.count == 0)

        #expect(dataRecord.displayName == "DataRecord")
        #expect(dataRecord.uniqueIdentifier == "org.webkit.test.extension (76C788B8)")

        #expect(dataRecord.containedDataTypes.count == 3)
        #expect(dataRecord.totalSizeInBytes == 237)

        #expect(dataRecord.sizeInBytes(ofTypes: [.local]) == 79)
        #expect(dataRecord.sizeInBytes(ofTypes: [.session]) == 79)
        #expect(dataRecord.sizeInBytes(ofTypes: [.synchronized]) == 79)

        let sizeOfDataTypes = dataRecord.sizeInBytes(ofTypes: allDataTypes)
        #expect(sizeOfDataTypes == 237)
    }

    // FIXME rdar://147858640 for iOS Debug and rdar://167044676 for macOS
    @Test(.disabled("rdar://147858640 for iOS Debug and rdar://167044676 for macOS"))
    func getDataRecordsForMultipleContexts() async throws {
        let backgroundScriptOne = """
            const data = { 'string': 'string', 'number': 1, 'boolean': true, 'dictionary': {'key': 'value'}, 'array': [1, true, 'string'] }
            await browser?.storage?.local?.set(data)
            """

        let backgroundScriptTwo = """
            const data = { 'string': 'string', 'number': 1, 'boolean': true, 'dictionary': {'key': 'value'}, 'array': [1, true, 'string'] }
            await browser?.storage?.session?.set(data)
            await browser?.storage?.sync?.set(data)
            """

        let testController = WKWebExtensionController(configuration: ._temporary())

        let testExtensionOne = try #require(
            WKWebExtension(manifestDictionary: dataRecordTestManifest, resources: ["background.js": backgroundScriptOne])
        )
        let testContextOne = WKWebExtensionContext(for: testExtensionOne)
        testContextOne.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.storage)
        testContextOne.uniqueIdentifier = "org.webkit.testOne.extension (76C788B8)"

        let testExtensionTwo = try #require(
            WKWebExtension(manifestDictionary: dataRecordTestTwoManifest, resources: ["background.js": backgroundScriptTwo])
        )
        let testContextTwo = WKWebExtensionContext(for: testExtensionTwo)
        testContextTwo.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.storage)
        testContextTwo.uniqueIdentifier = "org.webkit.testTwo.extension (76C788B8)"

        try testController.load(testContextOne)
        try testController.load(testContextTwo)

        // Wait for the background pages to load.
        try await Task.sleep(for: .seconds(4))

        let dataRecords = await testController.dataRecords(ofTypes: allDataTypes)

        #expect(dataRecords.count == 2)
        #expect(dataRecords[0].errors.count == 0)
        #expect(dataRecords[1].errors.count == 0)

        let dataRecordOne: WKWebExtension.DataRecord
        let dataRecordTwo: WKWebExtension.DataRecord

        if dataRecords[0].uniqueIdentifier == "org.webkit.testOne.extension (76C788B8)" {
            dataRecordOne = dataRecords[0]
            dataRecordTwo = dataRecords[1]

            #expect(dataRecordTwo.uniqueIdentifier == "org.webkit.testTwo.extension (76C788B8)")
        } else {
            dataRecordOne = dataRecords[1]
            dataRecordTwo = dataRecords[0]

            #expect(dataRecordOne.uniqueIdentifier == "org.webkit.testOne.extension (76C788B8)")
            #expect(dataRecordTwo.uniqueIdentifier == "org.webkit.testTwo.extension (76C788B8)")
        }

        #expect(dataRecordOne.displayName == "DataRecord")
        #expect(dataRecordTwo.displayName == "DataRecordTwo")

        #expect(dataRecordOne.totalSizeInBytes == 79)
        #expect(dataRecordTwo.totalSizeInBytes == 158)

        #expect(dataRecordOne.sizeInBytes(ofTypes: [.local]) == 79)
        #expect(dataRecordOne.sizeInBytes(ofTypes: [.session]) == 0)
        #expect(dataRecordOne.sizeInBytes(ofTypes: [.synchronized]) == 0)

        #expect(dataRecordTwo.sizeInBytes(ofTypes: [.local]) == 0)
        #expect(dataRecordTwo.sizeInBytes(ofTypes: [.session]) == 79)
        #expect(dataRecordTwo.sizeInBytes(ofTypes: [.synchronized]) == 79)
    }

    // FIXME: rdar://125926932 (Enable the WKWebExtensionDataRecord.RemoveDataRecords test (272236))
    @Test(.disabled("rdar://125926932"))
    func removeDataRecords() async throws {
        let backgroundScript = """
            const data = { 'string': 'string', 'number': 1, 'boolean': true, 'dictionary': {'key': 'value'}, 'array': [1, true, 'string'] }
            await browser?.storage?.local?.set(data)
            await browser?.storage?.session?.set(data)
            await browser?.storage?.sync?.set(data)
            """

        let extensionToTest = try #require(
            WKWebExtension(manifestDictionary: dataRecordTestManifest, resources: ["background.js": backgroundScript])
        )
        let testController = WKWebExtensionController(configuration: ._temporary())

        let context = WKWebExtensionContext(for: extensionToTest)
        context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.storage)

        // Give the extension a unique identifier so it opts into saving data in the temporary configuration.
        context.uniqueIdentifier = "org.webkit.test.extension (76C788B8)"

        try testController.load(context)

        // Wait for the background page to load.
        try await Task.sleep(for: .seconds(4))

        let dataRecords = await testController.dataRecords(ofTypes: allDataTypes)

        #expect(dataRecords.count == 1)
        #expect(dataRecords.first?.totalSizeInBytes == 237)

        await testController.removeData(ofTypes: [.local, .session], from: dataRecords)

        let updatedRecords = await testController.dataRecords(ofTypes: allDataTypes)

        #expect(updatedRecords.count == 1)

        #expect(updatedRecords[0].errors.count == 0)

        // Sync storage should still have data.
        #expect(updatedRecords.first?.totalSizeInBytes == 79)
    }

    // FIXME: rdar://125926932 (Enable the WKWebExtensionDataRecord.RemoveDataRecords test (272236))
    @Test(.disabled("rdar://125926932"))
    func removeDataRecordsForMultipleContexts() async throws {
        let backgroundScriptOne = """
            const data = { 'string': 'string', 'number': 1, 'boolean': true, 'dictionary': {'key': 'value'}, 'array': [1, true, 'string'] }
            await browser?.storage?.local?.set(data)
            """

        let backgroundScriptTwo = """
            const data = { 'string': 'string', 'number': 1, 'boolean': true, 'dictionary': {'key': 'value'}, 'array': [1, true, 'string'] }
            await browser?.storage?.sync?.set(data)
            """

        let testController = WKWebExtensionController(configuration: ._temporary())

        let testExtensionOne = try #require(
            WKWebExtension(manifestDictionary: dataRecordTestManifest, resources: ["background.js": backgroundScriptOne])
        )
        let testContextOne = WKWebExtensionContext(for: testExtensionOne)
        testContextOne.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.storage)
        testContextOne.uniqueIdentifier = "org.webkit.testOne.extension (76C788B8)"

        let testExtensionTwo = try #require(
            WKWebExtension(manifestDictionary: dataRecordTestTwoManifest, resources: ["background.js": backgroundScriptTwo])
        )
        let testContextTwo = WKWebExtensionContext(for: testExtensionTwo)
        testContextTwo.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.storage)
        testContextTwo.uniqueIdentifier = "org.webkit.testTwo.extension (76C788B8)"

        try testController.load(testContextOne)
        try testController.load(testContextTwo)

        // Wait for the background pages to load.
        try await Task.sleep(for: .seconds(4))

        let dataRecords = await testController.dataRecords(ofTypes: allDataTypes)

        #expect(dataRecords.count == 2)
        #expect(dataRecords[0].totalSizeInBytes + dataRecords[1].totalSizeInBytes == 237)

        await testController.removeData(ofTypes: allDataTypes, from: dataRecords)

        #expect(dataRecords[0].errors.count == 0)
        #expect(dataRecords[1].errors.count == 0)

        let updatedDataRecords = await testController.dataRecords(ofTypes: allDataTypes)

        #expect(updatedDataRecords[0].errors.count == 0)
        #expect(updatedDataRecords[1].errors.count == 0)

        #expect(updatedDataRecords.count == 2)
        #expect(updatedDataRecords[0].totalSizeInBytes == 0)
        #expect(updatedDataRecords[1].totalSizeInBytes == 0)
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS

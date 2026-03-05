/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"

#if ENABLE(WK_WEB_EXTENSIONS)
#import "TestCocoa.h"
#import "WebExtensionUtilities.h"
#import <WebKit/WKWebExtensionDataRecord.h>

namespace TestWebKitAPI {

static auto *dataRecordTestManifest = @{
    @"manifest_version": @3,
    @"name": @"DataRecord",
    @"permissions": @[ @"storage" ],
    @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
};

static auto *dataRecordTestTwoManifest = @{
    @"manifest_version": @3,
    @"name": @"DataRecordTwo",
    @"permissions": @[ @"storage" ],
    @"background": @{ @"scripts": @[ @"background.js" ], @"type": @"module", @"persistent": @NO },
};

static auto *allDataTypesSet = [NSSet setWithArray:@[ WKWebExtensionDataTypeLocal, WKWebExtensionDataTypeSession, WKWebExtensionDataTypeSynchronized ]];

// FIXME rdar://167044676 for macOS
#if PLATFORM(MAC)
TEST(WKWebExtensionDataRecord, DISABLED_GetDataRecords)
#else
TEST(WKWebExtensionDataRecord, GetDataRecords)
#endif
{
    auto *backgroundScript = Util::constructScript(@[
        @"const data = { 'string': 'string', 'number': 1, 'boolean': true, 'dictionary': {'key': 'value'}, 'array': [1, true, 'string'] }",
        @"await browser?.storage?.local?.set(data)",
        @"await browser?.storage?.session?.set(data)",
        @"await browser?.storage?.sync?.set(data)",
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:dataRecordTestManifest resources:@{ @"background.js": backgroundScript  }]);
    auto *testController = [[WKWebExtensionController alloc] initWithConfiguration:WKWebExtensionControllerConfiguration._temporaryConfiguration];

    auto *context = [[WKWebExtensionContext alloc] initForExtension:extension.get()];
    [context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionStorage];

    // Give the extension a unique identifier so it opts into saving data in the temporary configuration.
    context.uniqueIdentifier = @"org.webkit.test.extension (76C788B8)";

    [testController loadExtensionContext:context error:nil];

    // Wait for the background page to load.
    TestWebKitAPI::Util::runFor(4_s);

    __block bool fetchComplete = false;
    [testController fetchDataRecordOfTypes:allDataTypesSet forExtensionContext:context completionHandler:^(WKWebExtensionDataRecord *dataRecord) {
        EXPECT_EQ(dataRecord.errors.count, 0UL);

        EXPECT_NS_EQUAL(dataRecord.displayName, @"DataRecord");
        EXPECT_NS_EQUAL(dataRecord.uniqueIdentifier, @"org.webkit.test.extension (76C788B8)");

        EXPECT_EQ(dataRecord.containedDataTypes.count, 3UL);
        EXPECT_EQ(dataRecord.totalSizeInBytes, 237UL);

        EXPECT_EQ([dataRecord sizeInBytesOfTypes:[NSSet setWithObject:WKWebExtensionDataTypeLocal]], 79UL);
        EXPECT_EQ([dataRecord sizeInBytesOfTypes:[NSSet setWithObject:WKWebExtensionDataTypeSession]], 79UL);
        EXPECT_EQ([dataRecord sizeInBytesOfTypes:[NSSet setWithObject:WKWebExtensionDataTypeSynchronized]], 79UL);

        unsigned long long sizeOfDataTypes = [dataRecord sizeInBytesOfTypes:allDataTypesSet];
        EXPECT_EQ(sizeOfDataTypes, 237UL);

        fetchComplete = true;
    }];

    TestWebKitAPI::Util::run(&fetchComplete);
}

// FIXME rdar://147858640 for iOS Debug and rdar://167044676 for macOS
TEST(WKWebExtensionDataRecord, DISABLED_GetDataRecordsForMultipleContexts)
{
    auto *backgroundScriptOne = Util::constructScript(@[
        @"const data = { 'string': 'string', 'number': 1, 'boolean': true, 'dictionary': {'key': 'value'}, 'array': [1, true, 'string'] }",
        @"await browser?.storage?.local?.set(data)",
    ]);

    auto *backgroundScriptTwo = Util::constructScript(@[
        @"const data = { 'string': 'string', 'number': 1, 'boolean': true, 'dictionary': {'key': 'value'}, 'array': [1, true, 'string'] }",
        @"await browser?.storage?.session?.set(data)",
        @"await browser?.storage?.sync?.set(data)",
    ]);

    auto *testController = [[WKWebExtensionController alloc] initWithConfiguration:WKWebExtensionControllerConfiguration._temporaryConfiguration];

    auto *testExtensionOne = [[WKWebExtension alloc] _initWithManifestDictionary:dataRecordTestManifest resources:@{ @"background.js": backgroundScriptOne }];
    auto *testContextOne = [[WKWebExtensionContext alloc] initForExtension:testExtensionOne];
    [testContextOne setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionStorage];
    testContextOne.uniqueIdentifier = @"org.webkit.testOne.extension (76C788B8)";

    auto *testExtensionTwo = [[WKWebExtension alloc] _initWithManifestDictionary:dataRecordTestTwoManifest resources:@{ @"background.js": backgroundScriptTwo }];
    auto *testContextTwo = [[WKWebExtensionContext alloc] initForExtension:testExtensionTwo];
    [testContextTwo setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionStorage];
    testContextTwo.uniqueIdentifier = @"org.webkit.testTwo.extension (76C788B8)";

    [testController loadExtensionContext:testContextOne error:nil];
    [testController loadExtensionContext:testContextTwo error:nil];

    // Wait for the background pages to load.
    TestWebKitAPI::Util::runFor(4_s);

    __block bool fetchComplete = false;
    [testController fetchDataRecordsOfTypes:allDataTypesSet completionHandler:^(NSArray<WKWebExtensionDataRecord *> *dataRecords) {
        EXPECT_EQ(dataRecords.count, 2UL);
        EXPECT_EQ(dataRecords[0].errors.count, 0UL);
        EXPECT_EQ(dataRecords[1].errors.count, 0UL);

        WKWebExtensionDataRecord *dataRecordOne;
        WKWebExtensionDataRecord *dataRecordTwo;

        if ([dataRecords[0].uniqueIdentifier isEqualToString:@"org.webkit.testOne.extension (76C788B8)"]) {
            dataRecordOne = dataRecords[0];
            dataRecordTwo = dataRecords[1];

            EXPECT_NS_EQUAL(dataRecordTwo.uniqueIdentifier, @"org.webkit.testTwo.extension (76C788B8)");
        } else {
            dataRecordOne = dataRecords[1];
            dataRecordTwo = dataRecords[0];

            EXPECT_NS_EQUAL(dataRecordOne.uniqueIdentifier, @"org.webkit.testOne.extension (76C788B8)");
            EXPECT_NS_EQUAL(dataRecordTwo.uniqueIdentifier, @"org.webkit.testTwo.extension (76C788B8)");
        }

        EXPECT_NS_EQUAL(dataRecordOne.displayName, @"DataRecord");
        EXPECT_NS_EQUAL(dataRecordTwo.displayName, @"DataRecordTwo");

        EXPECT_EQ(dataRecordOne.totalSizeInBytes, 79UL);
        EXPECT_EQ(dataRecordTwo.totalSizeInBytes, 158UL);

        EXPECT_EQ([dataRecordOne sizeInBytesOfTypes:[NSSet setWithObject:WKWebExtensionDataTypeLocal]], 79UL);
        EXPECT_EQ([dataRecordOne sizeInBytesOfTypes:[NSSet setWithObject:WKWebExtensionDataTypeSession]], 0UL);
        EXPECT_EQ([dataRecordOne sizeInBytesOfTypes:[NSSet setWithObject:WKWebExtensionDataTypeSynchronized]], 0UL);

        EXPECT_EQ([dataRecordTwo sizeInBytesOfTypes:[NSSet setWithObject:WKWebExtensionDataTypeLocal]], 0UL);
        EXPECT_EQ([dataRecordTwo sizeInBytesOfTypes:[NSSet setWithObject:WKWebExtensionDataTypeSession]], 79UL);
        EXPECT_EQ([dataRecordTwo sizeInBytesOfTypes:[NSSet setWithObject:WKWebExtensionDataTypeSynchronized]], 79UL);

        fetchComplete = true;
    }];

    TestWebKitAPI::Util::run(&fetchComplete);
}

// FIXME: rdar://125926932 (Enable the WKWebExtensionDataRecord.RemoveDataRecords test (272236))
TEST(WKWebExtensionDataRecord, DISABLED_RemoveDataRecords)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const data = { 'string': 'string', 'number': 1, 'boolean': true, 'dictionary': {'key': 'value'}, 'array': [1, true, 'string'] }",
        @"await browser?.storage?.local?.set(data)",
        @"await browser?.storage?.session?.set(data)",
        @"await browser?.storage?.sync?.set(data)",
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:dataRecordTestManifest resources:@{ @"background.js": backgroundScript  }]);
    auto *testController = [[WKWebExtensionController alloc] initWithConfiguration:WKWebExtensionControllerConfiguration._temporaryConfiguration];

    auto *context = [[WKWebExtensionContext alloc] initForExtension:extension.get()];
    [context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionStorage];

    // Give the extension a unique identifier so it opts into saving data in the temporary configuration.
    context.uniqueIdentifier = @"org.webkit.test.extension (76C788B8)";

    [testController loadExtensionContext:context error:nil];

    // Wait for the background page to load.
    TestWebKitAPI::Util::runFor(4_s);

    __block bool removalComplete = false;
    [testController fetchDataRecordsOfTypes:allDataTypesSet completionHandler:^(NSArray<WKWebExtensionDataRecord *> *dataRecords) {
        EXPECT_EQ(dataRecords.count, 1UL);
        EXPECT_EQ(dataRecords.firstObject.totalSizeInBytes, 237UL);

        [testController removeDataOfTypes:[NSSet setWithArray:@[ WKWebExtensionDataTypeLocal, WKWebExtensionDataTypeSession ]] fromDataRecords:dataRecords completionHandler:^{
            [testController fetchDataRecordsOfTypes:allDataTypesSet completionHandler:^(NSArray<WKWebExtensionDataRecord *> *updatedRecords) {
                EXPECT_EQ(updatedRecords.count, 1UL);

                EXPECT_EQ(updatedRecords[0].errors.count, 0UL);

                // Sync storage should still have data.
                EXPECT_EQ(updatedRecords.firstObject.totalSizeInBytes, 79UL);
                removalComplete = true;
            }];
        }];
    }];

    TestWebKitAPI::Util::run(&removalComplete);
}

// FIXME: rdar://125926932 (Enable the WKWebExtensionDataRecord.RemoveDataRecords test (272236))
TEST(WKWebExtensionDataRecord, DISABLED_RemoveDataRecordsForMultipleContexts)
{
    auto *backgroundScriptOne = Util::constructScript(@[
        @"const data = { 'string': 'string', 'number': 1, 'boolean': true, 'dictionary': {'key': 'value'}, 'array': [1, true, 'string'] }",
        @"await browser?.storage?.local?.set(data)",
    ]);

    auto *backgroundScriptTwo = Util::constructScript(@[
        @"const data = { 'string': 'string', 'number': 1, 'boolean': true, 'dictionary': {'key': 'value'}, 'array': [1, true, 'string'] }",
        @"await browser?.storage?.sync?.set(data)",
    ]);

    auto *testController = [[WKWebExtensionController alloc] initWithConfiguration:WKWebExtensionControllerConfiguration._temporaryConfiguration];

    auto *testExtensionOne = [[WKWebExtension alloc] _initWithManifestDictionary:dataRecordTestManifest resources:@{ @"background.js": backgroundScriptOne }];
    auto *testContextOne = [[WKWebExtensionContext alloc] initForExtension:testExtensionOne];
    [testContextOne setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionStorage];
    testContextOne.uniqueIdentifier = @"org.webkit.testOne.extension (76C788B8)";

    auto *testExtensionTwo = [[WKWebExtension alloc] _initWithManifestDictionary:dataRecordTestTwoManifest resources:@{ @"background.js": backgroundScriptTwo }];
    auto *testContextTwo = [[WKWebExtensionContext alloc] initForExtension:testExtensionTwo];
    [testContextTwo setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionStorage];
    testContextTwo.uniqueIdentifier = @"org.webkit.testTwo.extension (76C788B8)";

    [testController loadExtensionContext:testContextOne error:nil];
    [testController loadExtensionContext:testContextTwo error:nil];

    // Wait for the background pages to load.
    TestWebKitAPI::Util::runFor(4_s);

    __block bool removalComplete = false;
    [testController fetchDataRecordsOfTypes:allDataTypesSet completionHandler:^(NSArray<WKWebExtensionDataRecord *> *dataRecords) {
        EXPECT_EQ(dataRecords.count, 2UL);
        EXPECT_EQ(dataRecords[0].totalSizeInBytes + dataRecords[1].totalSizeInBytes, 237UL);

        [testController removeDataOfTypes:allDataTypesSet fromDataRecords:dataRecords completionHandler:^{
            EXPECT_EQ(dataRecords[0].errors.count, 0UL);
            EXPECT_EQ(dataRecords[1].errors.count, 0UL);
            [testController fetchDataRecordsOfTypes:allDataTypesSet completionHandler:^(NSArray<WKWebExtensionDataRecord *> *updatedDataRecords) {
                EXPECT_EQ(updatedDataRecords[0].errors.count, 0UL);
                EXPECT_EQ(updatedDataRecords[1].errors.count, 0UL);

                EXPECT_EQ(updatedDataRecords.count, 2UL);
                EXPECT_EQ(updatedDataRecords[0].totalSizeInBytes, 0UL);
                EXPECT_EQ(updatedDataRecords[1].totalSizeInBytes, 0UL);

                removalComplete = true;
            }];
        }];
    }];

    TestWebKitAPI::Util::run(&removalComplete);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)

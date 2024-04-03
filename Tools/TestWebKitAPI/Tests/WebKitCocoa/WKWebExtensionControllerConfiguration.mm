/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import <WebKit/WKFoundation.h>
#import <WebKit/_WKWebExtensionControllerConfigurationPrivate.h>

namespace TestWebKitAPI {

TEST(WKWebExtensionControllerConfiguration, Initialization)
{
    auto *configuration = _WKWebExtensionControllerConfiguration.defaultConfiguration;

    EXPECT_TRUE(configuration.persistent);
    EXPECT_NULL(configuration.identifier);
    EXPECT_FALSE(configuration._temporary);
    EXPECT_NOT_NULL(configuration.webViewConfiguration);
    EXPECT_NE(configuration, _WKWebExtensionControllerConfiguration.defaultConfiguration);
    EXPECT_FALSE([configuration isEqual:_WKWebExtensionControllerConfiguration.defaultConfiguration]);
    EXPECT_FALSE([configuration.webViewConfiguration isEqual:_WKWebExtensionControllerConfiguration.defaultConfiguration.webViewConfiguration]);
    EXPECT_NS_EQUAL(configuration._storageDirectoryPath, _WKWebExtensionControllerConfiguration.defaultConfiguration._storageDirectoryPath);
    EXPECT_NS_EQUAL(configuration.defaultWebsiteDataStore, WKWebsiteDataStore.defaultDataStore);

    configuration = _WKWebExtensionControllerConfiguration.nonPersistentConfiguration;

    EXPECT_FALSE(configuration.persistent);
    EXPECT_NULL(configuration.identifier);
    EXPECT_FALSE(configuration._temporary);
    EXPECT_NOT_NULL(configuration.webViewConfiguration);
    EXPECT_NULL(configuration._storageDirectoryPath);
    EXPECT_NE(configuration, _WKWebExtensionControllerConfiguration.nonPersistentConfiguration);
    EXPECT_FALSE([configuration isEqual:_WKWebExtensionControllerConfiguration.nonPersistentConfiguration]);
    EXPECT_FALSE([configuration.webViewConfiguration isEqual:_WKWebExtensionControllerConfiguration.nonPersistentConfiguration.webViewConfiguration]);
    EXPECT_NS_EQUAL(configuration.defaultWebsiteDataStore, WKWebsiteDataStore.defaultDataStore);

    auto *identifier = [NSUUID UUID];
    configuration = [_WKWebExtensionControllerConfiguration configurationWithIdentifier:identifier];

    EXPECT_TRUE(configuration.persistent);
    EXPECT_NS_EQUAL(configuration.identifier, identifier);
    EXPECT_FALSE(configuration._temporary);
    EXPECT_NOT_NULL(configuration.webViewConfiguration);
    EXPECT_NE(configuration, [_WKWebExtensionControllerConfiguration configurationWithIdentifier:identifier]);
    EXPECT_FALSE([configuration isEqual:[_WKWebExtensionControllerConfiguration configurationWithIdentifier:identifier]]);
    EXPECT_FALSE([configuration.webViewConfiguration isEqual:[_WKWebExtensionControllerConfiguration configurationWithIdentifier:identifier].webViewConfiguration]);
    EXPECT_NS_EQUAL(configuration._storageDirectoryPath, [_WKWebExtensionControllerConfiguration configurationWithIdentifier:identifier]._storageDirectoryPath);
    EXPECT_NS_EQUAL(configuration.defaultWebsiteDataStore, WKWebsiteDataStore.defaultDataStore);

    configuration = _WKWebExtensionControllerConfiguration._temporaryConfiguration;

    EXPECT_FALSE([configuration isEqual:_WKWebExtensionControllerConfiguration._temporaryConfiguration]);

    EXPECT_TRUE(configuration.persistent);
    EXPECT_TRUE(configuration._temporary);
    EXPECT_NULL(configuration.identifier);
    EXPECT_NOT_NULL(configuration.webViewConfiguration);
    EXPECT_NE(configuration, _WKWebExtensionControllerConfiguration._temporaryConfiguration);
    EXPECT_FALSE([configuration isEqual:_WKWebExtensionControllerConfiguration._temporaryConfiguration]);
    EXPECT_FALSE([configuration.webViewConfiguration isEqual:_WKWebExtensionControllerConfiguration.nonPersistentConfiguration.webViewConfiguration]);
    EXPECT_FALSE([configuration._storageDirectoryPath isEqualToString:_WKWebExtensionControllerConfiguration._temporaryConfiguration._storageDirectoryPath]);
    EXPECT_NS_EQUAL(configuration.defaultWebsiteDataStore, WKWebsiteDataStore.defaultDataStore);
}

TEST(WKWebExtensionControllerConfiguration, SecureCoding)
{
    NSError *error = nil;
    auto *configuration = _WKWebExtensionControllerConfiguration.defaultConfiguration;
    auto *data = [NSKeyedArchiver archivedDataWithRootObject:configuration requiringSecureCoding:YES error:&error];
    _WKWebExtensionControllerConfiguration *result = [NSKeyedUnarchiver unarchivedObjectOfClass:_WKWebExtensionControllerConfiguration.class fromData:data error:&error];

    EXPECT_NULL(error);
    EXPECT_NOT_NULL(data);
    EXPECT_NOT_NULL(result);

    EXPECT_TRUE(result.persistent);
    EXPECT_NULL(result.identifier);
    EXPECT_FALSE(result._temporary);
    EXPECT_NS_EQUAL(result._storageDirectoryPath, configuration._storageDirectoryPath);
    EXPECT_NOT_NULL(result.webViewConfiguration);
    EXPECT_NS_EQUAL(result.defaultWebsiteDataStore, WKWebsiteDataStore.defaultDataStore);
    EXPECT_NE(configuration, result);
    EXPECT_FALSE([result isEqual:configuration]);
    EXPECT_FALSE([result.webViewConfiguration isEqual:configuration.webViewConfiguration]);
    EXPECT_NS_EQUAL(result.defaultWebsiteDataStore, configuration.defaultWebsiteDataStore);

    configuration = _WKWebExtensionControllerConfiguration.nonPersistentConfiguration;
    data = [NSKeyedArchiver archivedDataWithRootObject:configuration requiringSecureCoding:YES error:&error];
    result = [NSKeyedUnarchiver unarchivedObjectOfClass:_WKWebExtensionControllerConfiguration.class fromData:data error:&error];

    EXPECT_NULL(error);
    EXPECT_NOT_NULL(data);
    EXPECT_NOT_NULL(result);

    EXPECT_FALSE(result.persistent);
    EXPECT_NULL(result.identifier);
    EXPECT_FALSE(result._temporary);
    EXPECT_NS_EQUAL(result._storageDirectoryPath, configuration._storageDirectoryPath);
    EXPECT_NOT_NULL(result.webViewConfiguration);
    EXPECT_NS_EQUAL(result.defaultWebsiteDataStore, WKWebsiteDataStore.defaultDataStore);
    EXPECT_NE(configuration, result);
    EXPECT_FALSE([result isEqual:configuration]);
    EXPECT_FALSE([result.webViewConfiguration isEqual:configuration.webViewConfiguration]);
    EXPECT_NS_EQUAL(result.defaultWebsiteDataStore, configuration.defaultWebsiteDataStore);

    auto *identifier = [NSUUID UUID];
    configuration = [_WKWebExtensionControllerConfiguration configurationWithIdentifier:identifier];
    data = [NSKeyedArchiver archivedDataWithRootObject:configuration requiringSecureCoding:YES error:&error];
    result = [NSKeyedUnarchiver unarchivedObjectOfClass:_WKWebExtensionControllerConfiguration.class fromData:data error:&error];

    EXPECT_NULL(error);
    EXPECT_NOT_NULL(data);
    EXPECT_NOT_NULL(result);

    EXPECT_TRUE(result.persistent);
    EXPECT_FALSE(result._temporary);
    EXPECT_NS_EQUAL(result._storageDirectoryPath, configuration._storageDirectoryPath);
    EXPECT_NOT_NULL(result.webViewConfiguration);
    EXPECT_NS_EQUAL(result.identifier, identifier);
    EXPECT_NS_EQUAL(result.defaultWebsiteDataStore, WKWebsiteDataStore.defaultDataStore);
    EXPECT_NE(configuration, result);
    EXPECT_FALSE([result isEqual:configuration]);
    EXPECT_FALSE([result.webViewConfiguration isEqual:configuration.webViewConfiguration]);
    EXPECT_NS_EQUAL(result.defaultWebsiteDataStore, configuration.defaultWebsiteDataStore);

    configuration = _WKWebExtensionControllerConfiguration._temporaryConfiguration;
    data = [NSKeyedArchiver archivedDataWithRootObject:configuration requiringSecureCoding:YES error:&error];
    result = [NSKeyedUnarchiver unarchivedObjectOfClass:_WKWebExtensionControllerConfiguration.class fromData:data error:&error];

    EXPECT_NULL(error);
    EXPECT_NOT_NULL(data);
    EXPECT_NOT_NULL(result);

    EXPECT_TRUE(result.persistent);
    EXPECT_NULL(result.identifier);
    EXPECT_TRUE(result._temporary);
    EXPECT_NS_EQUAL(result._storageDirectoryPath, configuration._storageDirectoryPath);
    EXPECT_NOT_NULL(result.webViewConfiguration);
    EXPECT_NS_EQUAL(result.defaultWebsiteDataStore, WKWebsiteDataStore.defaultDataStore);
    EXPECT_NE(configuration, result);
    EXPECT_FALSE([result isEqual:configuration]);
    EXPECT_FALSE([result.webViewConfiguration isEqual:configuration.webViewConfiguration]);
    EXPECT_NS_EQUAL(result.defaultWebsiteDataStore, configuration.defaultWebsiteDataStore);
}

TEST(WKWebExtensionControllerConfiguration, Copying)
{
    auto *configuration = _WKWebExtensionControllerConfiguration.defaultConfiguration;
    _WKWebExtensionControllerConfiguration *copy = [configuration copy];

    EXPECT_TRUE(copy.persistent);
    EXPECT_NULL(copy.identifier);
    EXPECT_FALSE(copy._temporary);
    EXPECT_NS_EQUAL(copy._storageDirectoryPath, configuration._storageDirectoryPath);
    EXPECT_NOT_NULL(copy.webViewConfiguration);
    EXPECT_NS_EQUAL(copy.defaultWebsiteDataStore, WKWebsiteDataStore.defaultDataStore);
    EXPECT_NE(configuration, copy);
    EXPECT_FALSE([copy isEqual:configuration]);
    EXPECT_FALSE([copy.webViewConfiguration isEqual:configuration.webViewConfiguration]);
    EXPECT_NS_EQUAL(copy.defaultWebsiteDataStore, configuration.defaultWebsiteDataStore);

    configuration = _WKWebExtensionControllerConfiguration.nonPersistentConfiguration;
    copy = [configuration copy];

    EXPECT_FALSE(copy.persistent);
    EXPECT_NULL(copy.identifier);
    EXPECT_FALSE(copy._temporary);
    EXPECT_NS_EQUAL(copy._storageDirectoryPath, configuration._storageDirectoryPath);
    EXPECT_NOT_NULL(copy.webViewConfiguration);
    EXPECT_NS_EQUAL(copy.defaultWebsiteDataStore, WKWebsiteDataStore.defaultDataStore);
    EXPECT_NE(configuration, copy);
    EXPECT_FALSE([copy isEqual:configuration]);
    EXPECT_FALSE([copy.webViewConfiguration isEqual:configuration.webViewConfiguration]);
    EXPECT_NS_EQUAL(copy.defaultWebsiteDataStore, configuration.defaultWebsiteDataStore);

    auto *identifier = [NSUUID UUID];
    configuration = [_WKWebExtensionControllerConfiguration configurationWithIdentifier:identifier];
    copy = [configuration copy];

    EXPECT_TRUE(copy.persistent);
    EXPECT_NS_EQUAL(copy.identifier, identifier);
    EXPECT_FALSE(copy._temporary);
    EXPECT_NS_EQUAL(copy._storageDirectoryPath, configuration._storageDirectoryPath);
    EXPECT_NOT_NULL(copy.webViewConfiguration);
    EXPECT_NS_EQUAL(copy.defaultWebsiteDataStore, WKWebsiteDataStore.defaultDataStore);
    EXPECT_NE(configuration, copy);
    EXPECT_FALSE([copy isEqual:configuration]);
    EXPECT_FALSE([copy.webViewConfiguration isEqual:configuration.webViewConfiguration]);
    EXPECT_NS_EQUAL(copy.defaultWebsiteDataStore, configuration.defaultWebsiteDataStore);

    configuration = _WKWebExtensionControllerConfiguration._temporaryConfiguration;
    copy = [configuration copy];

    EXPECT_TRUE(copy.persistent);
    EXPECT_NULL(copy.identifier);
    EXPECT_TRUE(copy._temporary);
    EXPECT_NS_EQUAL(copy._storageDirectoryPath, configuration._storageDirectoryPath);
    EXPECT_NOT_NULL(copy.webViewConfiguration);
    EXPECT_NS_EQUAL(copy.defaultWebsiteDataStore, WKWebsiteDataStore.defaultDataStore);
    EXPECT_NE(configuration, copy);
    EXPECT_FALSE([copy isEqual:configuration]);
    EXPECT_FALSE([copy.webViewConfiguration isEqual:configuration.webViewConfiguration]);
    EXPECT_NS_EQUAL(copy.defaultWebsiteDataStore, configuration.defaultWebsiteDataStore);
}

TEST(WKWebExtensionControllerConfiguration, WebViewConfigurationWithCopyAndCoding)
{
    auto *originalConfiguration = _WKWebExtensionControllerConfiguration.defaultConfiguration;

    EXPECT_NOT_NULL(originalConfiguration.webViewConfiguration);

    auto *newWebViewConfiguration = [[WKWebViewConfiguration alloc] init];
    originalConfiguration.webViewConfiguration = newWebViewConfiguration;
    EXPECT_NS_EQUAL(originalConfiguration.webViewConfiguration, newWebViewConfiguration);

    _WKWebExtensionControllerConfiguration *copiedConfiguration = [originalConfiguration copy];
    EXPECT_NOT_NULL(copiedConfiguration);
    EXPECT_FALSE([copiedConfiguration isEqual:originalConfiguration]);
    EXPECT_FALSE([copiedConfiguration.webViewConfiguration isEqual:newWebViewConfiguration]);

    NSError *error = nil;
    auto *encodedData = [NSKeyedArchiver archivedDataWithRootObject:originalConfiguration requiringSecureCoding:YES error:&error];
    EXPECT_NULL(error);
    EXPECT_NOT_NULL(encodedData);

    _WKWebExtensionControllerConfiguration *decodedConfiguration = [NSKeyedUnarchiver unarchivedObjectOfClass:_WKWebExtensionControllerConfiguration.class fromData:encodedData error:&error];
    EXPECT_NULL(error);
    EXPECT_NOT_NULL(decodedConfiguration);

    EXPECT_FALSE([decodedConfiguration isEqual:originalConfiguration]);
    EXPECT_FALSE([decodedConfiguration.webViewConfiguration isEqual:newWebViewConfiguration]);

    originalConfiguration.webViewConfiguration = nil;
    EXPECT_NOT_NULL(originalConfiguration.webViewConfiguration);
    EXPECT_FALSE([originalConfiguration.webViewConfiguration isEqual:newWebViewConfiguration]);
}

TEST(WKWebExtensionControllerConfiguration, DefaultWebsiteDataStoreWithCopyAndCoding)
{
    auto *originalConfiguration = _WKWebExtensionControllerConfiguration.defaultConfiguration;

    EXPECT_NS_EQUAL(originalConfiguration.defaultWebsiteDataStore, WKWebsiteDataStore.defaultDataStore);

    auto *newDataStore = WKWebsiteDataStore.nonPersistentDataStore;
    originalConfiguration.defaultWebsiteDataStore = newDataStore;
    EXPECT_NS_EQUAL(originalConfiguration.defaultWebsiteDataStore, newDataStore);

    _WKWebExtensionControllerConfiguration *copiedConfiguration = [originalConfiguration copy];
    EXPECT_NOT_NULL(copiedConfiguration);
    EXPECT_TRUE([copiedConfiguration isEqual:originalConfiguration]);
    EXPECT_NS_EQUAL(copiedConfiguration.defaultWebsiteDataStore, newDataStore);

    NSError *error = nil;
    auto *encodedData = [NSKeyedArchiver archivedDataWithRootObject:originalConfiguration requiringSecureCoding:YES error:&error];
    EXPECT_NULL(error);
    EXPECT_NOT_NULL(encodedData);

    _WKWebExtensionControllerConfiguration *decodedConfiguration = [NSKeyedUnarchiver unarchivedObjectOfClass:_WKWebExtensionControllerConfiguration.class fromData:encodedData error:&error];
    EXPECT_NULL(error);
    EXPECT_NOT_NULL(decodedConfiguration);

    EXPECT_FALSE([decodedConfiguration isEqual:originalConfiguration]);
    EXPECT_FALSE([decodedConfiguration.defaultWebsiteDataStore isEqual:newDataStore]);
    EXPECT_FALSE([decodedConfiguration.defaultWebsiteDataStore isEqual:WKWebsiteDataStore.defaultDataStore]);

    originalConfiguration.defaultWebsiteDataStore = nil;
    EXPECT_NS_EQUAL(originalConfiguration.defaultWebsiteDataStore, WKWebsiteDataStore.defaultDataStore);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)

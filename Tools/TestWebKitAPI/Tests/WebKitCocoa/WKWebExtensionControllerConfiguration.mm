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
    EXPECT_NE(configuration, _WKWebExtensionControllerConfiguration.defaultConfiguration);
    EXPECT_NS_EQUAL(configuration, _WKWebExtensionControllerConfiguration.defaultConfiguration);

    configuration = _WKWebExtensionControllerConfiguration.nonPersistentConfiguration;

    EXPECT_FALSE(configuration.persistent);
    EXPECT_NULL(configuration.identifier);
    EXPECT_NE(configuration, _WKWebExtensionControllerConfiguration.nonPersistentConfiguration);
    EXPECT_NS_EQUAL(configuration, _WKWebExtensionControllerConfiguration.nonPersistentConfiguration);

    auto *identifier = [NSUUID UUID];
    configuration = [_WKWebExtensionControllerConfiguration configurationWithIdentifier:identifier];

    EXPECT_TRUE(configuration.persistent);
    EXPECT_NS_EQUAL(configuration.identifier, identifier);
    EXPECT_NE(configuration, [_WKWebExtensionControllerConfiguration configurationWithIdentifier:identifier]);
    EXPECT_NS_EQUAL(configuration, [_WKWebExtensionControllerConfiguration configurationWithIdentifier:identifier]);
}

TEST(WKWebExtensionControllerConfiguration, SecureCoding)
{
    NSError *error = nil;
    auto *configuration = _WKWebExtensionControllerConfiguration.defaultConfiguration;
    auto *data = [NSKeyedArchiver archivedDataWithRootObject:configuration requiringSecureCoding:YES error:&error];
    _WKWebExtensionControllerConfiguration *result = [NSKeyedUnarchiver unarchivedObjectOfClass:_WKWebExtensionControllerConfiguration.class fromData:data error:&error];

    EXPECT_NULL(error);
    EXPECT_TRUE(result.persistent);
    EXPECT_NULL(result.identifier);
    EXPECT_NE(configuration, result);
    EXPECT_NS_EQUAL(configuration, result);

    configuration = _WKWebExtensionControllerConfiguration.nonPersistentConfiguration;
    data = [NSKeyedArchiver archivedDataWithRootObject:configuration requiringSecureCoding:YES error:&error];
    result = [NSKeyedUnarchiver unarchivedObjectOfClass:_WKWebExtensionControllerConfiguration.class fromData:data error:&error];

    EXPECT_NULL(error);
    EXPECT_FALSE(result.persistent);
    EXPECT_NULL(result.identifier);
    EXPECT_NE(configuration, result);
    EXPECT_NS_EQUAL(configuration, result);

    auto *identifier = [NSUUID UUID];
    configuration = [_WKWebExtensionControllerConfiguration configurationWithIdentifier:identifier];
    data = [NSKeyedArchiver archivedDataWithRootObject:configuration requiringSecureCoding:YES error:&error];
    result = [NSKeyedUnarchiver unarchivedObjectOfClass:_WKWebExtensionControllerConfiguration.class fromData:data error:&error];

    EXPECT_NULL(error);
    EXPECT_TRUE(result.persistent);
    EXPECT_NS_EQUAL(result.identifier, identifier);
    EXPECT_NE(configuration, result);
    EXPECT_NS_EQUAL(configuration, result);
}

TEST(WKWebExtensionControllerConfiguration, Copying)
{
    auto *configuration = _WKWebExtensionControllerConfiguration.defaultConfiguration;
    _WKWebExtensionControllerConfiguration *copy = [configuration copy];

    EXPECT_TRUE(copy.persistent);
    EXPECT_NULL(copy.identifier);
    EXPECT_NE(configuration, copy);
    EXPECT_NS_EQUAL(configuration, copy);

    configuration = _WKWebExtensionControllerConfiguration.nonPersistentConfiguration;
    copy = [configuration copy];

    EXPECT_FALSE(copy.persistent);
    EXPECT_NULL(copy.identifier);
    EXPECT_NE(configuration, copy);
    EXPECT_NS_EQUAL(configuration, copy);

    auto *identifier = [NSUUID UUID];
    configuration = [_WKWebExtensionControllerConfiguration configurationWithIdentifier:identifier];
    copy = [configuration copy];

    EXPECT_TRUE(copy.persistent);
    EXPECT_NS_EQUAL(copy.identifier, identifier);
    EXPECT_NE(configuration, copy);
    EXPECT_NS_EQUAL(configuration, copy);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)

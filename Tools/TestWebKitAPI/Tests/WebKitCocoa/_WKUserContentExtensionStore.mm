/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#import <WebKit/WKFoundation.h>

#import "PlatformUtilities.h"
#import "Test.h"
#import <WebKit/_WKUserContentExtensionStorePrivate.h>
#import <wtf/RetainPtr.h>

class _WKUserContentExtensionStoreTest : public testing::Test {
public:
    virtual void SetUp()
    {
        [[_WKUserContentExtensionStore defaultStore] _removeAllContentExtensions];
    }
};

static NSString *basicFilter = @"[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*webkit.org\"}}]";

TEST_F(_WKUserContentExtensionStoreTest, Compile)
{
    __block bool doneCompiling = false;
    [[_WKUserContentExtensionStore defaultStore] compileContentExtensionForIdentifier:@"TestExtension" encodedContentExtension:basicFilter completionHandler:^(_WKUserContentFilter *filter, NSError *error) {
    
        EXPECT_NOT_NULL(filter);
        EXPECT_NULL(error);

        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);
}

static NSString *invalidFilter = @"[";

static void checkDomain(NSError *error)
{
    EXPECT_STREQ([[error domain] UTF8String], [_WKUserContentExtensionsDomain UTF8String]);
}

TEST_F(_WKUserContentExtensionStoreTest, InvalidExtension)
{
    __block bool doneCompiling = false;
    [[_WKUserContentExtensionStore defaultStore] compileContentExtensionForIdentifier:@"TestExtension" encodedContentExtension:invalidFilter completionHandler:^(_WKUserContentFilter *filter, NSError *error) {
    
        EXPECT_NULL(filter);
        EXPECT_NOT_NULL(error);
        checkDomain(error);
        EXPECT_EQ(error.code, _WKUserContentExtensionStoreErrorCompileFailed);
        EXPECT_STREQ("Rule list compilation failed: Failed to parse the JSON String.", [[error helpAnchor] UTF8String]);

        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);
}

TEST_F(_WKUserContentExtensionStoreTest, Lookup)
{
    __block bool doneCompiling = false;
    [[_WKUserContentExtensionStore defaultStore] compileContentExtensionForIdentifier:@"TestExtension" encodedContentExtension:basicFilter completionHandler:^(_WKUserContentFilter *filter, NSError *error) {
    
        EXPECT_NOT_NULL(filter);
        EXPECT_NULL(error);

        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);

    __block bool doneLookingUp = false;
    [[_WKUserContentExtensionStore defaultStore] lookupContentExtensionForIdentifier:@"TestExtension" completionHandler:^(_WKUserContentFilter *filter, NSError *error) {
    
        EXPECT_NOT_NULL(filter);
        EXPECT_NULL(error);

        doneLookingUp = true;
    }];
    TestWebKitAPI::Util::run(&doneLookingUp);
}

TEST_F(_WKUserContentExtensionStoreTest, NonExistingIdentifierLookup)
{
    __block bool doneLookingUp = false;
    [[_WKUserContentExtensionStore defaultStore] lookupContentExtensionForIdentifier:@"DoesNotExist" completionHandler:^(_WKUserContentFilter *filter, NSError *error) {
    
        EXPECT_NULL(filter);
        EXPECT_NOT_NULL(error);
        checkDomain(error);
        EXPECT_EQ(error.code, _WKUserContentExtensionStoreErrorLookupFailed);
        EXPECT_STREQ("Rule list lookup failed: Unspecified error during lookup.", [[error helpAnchor] UTF8String]);
        
        doneLookingUp = true;
    }];
    TestWebKitAPI::Util::run(&doneLookingUp);
}

TEST_F(_WKUserContentExtensionStoreTest, VersionMismatch)
{
    __block bool doneCompiling = false;
    [[_WKUserContentExtensionStore defaultStore] compileContentExtensionForIdentifier:@"TestExtension" encodedContentExtension:basicFilter completionHandler:^(_WKUserContentFilter *filter, NSError *error)
    {
        
        EXPECT_NOT_NULL(filter);
        EXPECT_NULL(error);
        
        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);

    [[_WKUserContentExtensionStore defaultStore] _invalidateContentExtensionVersionForIdentifier:@"TestExtension"];
    
    __block bool doneLookingUp = false;
    [[_WKUserContentExtensionStore defaultStore] lookupContentExtensionForIdentifier:@"TestExtension" completionHandler:^(_WKUserContentFilter *filter, NSError *error)
    {
        
        EXPECT_NULL(filter);
        EXPECT_NOT_NULL(error);
        checkDomain(error);
        EXPECT_EQ(error.code, _WKUserContentExtensionStoreErrorVersionMismatch);
        EXPECT_STREQ("Rule list lookup failed: Version of file does not match version of interpreter.", [[error helpAnchor] UTF8String]);
        
        doneLookingUp = true;
    }];
    TestWebKitAPI::Util::run(&doneLookingUp);
}

TEST_F(_WKUserContentExtensionStoreTest, Removal)
{
    __block bool doneCompiling = false;
    [[_WKUserContentExtensionStore defaultStore] compileContentExtensionForIdentifier:@"TestExtension" encodedContentExtension:basicFilter completionHandler:^(_WKUserContentFilter *filter, NSError *error) {
    
        EXPECT_NOT_NULL(filter);
        EXPECT_NULL(error);

        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);

    __block bool doneRemoving = false;
    [[_WKUserContentExtensionStore defaultStore] removeContentExtensionForIdentifier:@"TestExtension" completionHandler:^(NSError *error) {
        EXPECT_NULL(error);

        doneRemoving = true;
    }];
    TestWebKitAPI::Util::run(&doneRemoving);
}

TEST_F(_WKUserContentExtensionStoreTest, NonExistingIdentifierRemove)
{
    __block bool doneRemoving = false;
    [[_WKUserContentExtensionStore defaultStore] removeContentExtensionForIdentifier:@"DoesNotExist" completionHandler:^(NSError *error) {
        EXPECT_NOT_NULL(error);
        checkDomain(error);
        EXPECT_EQ(error.code, _WKUserContentExtensionStoreErrorRemoveFailed);
        EXPECT_STREQ("Rule list removal failed: Unspecified error during remove.", [[error helpAnchor] UTF8String]);

        doneRemoving = true;
    }];
    TestWebKitAPI::Util::run(&doneRemoving);
}

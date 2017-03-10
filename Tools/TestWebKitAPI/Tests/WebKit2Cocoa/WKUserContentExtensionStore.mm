/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if WK_API_ENABLED

#import "PlatformUtilities.h"
#import "Test.h"
#import <WebKit/WKContentExtensionStorePrivate.h>
#import <wtf/RetainPtr.h>

class WKContentExtensionStoreTest : public testing::Test {
public:
    virtual void SetUp()
    {
        [[WKContentExtensionStore defaultStore] _removeAllContentExtensions];
    }
};

static NSString *basicFilter = @"[{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\".*webkit.org\"}}]";

TEST_F(WKContentExtensionStoreTest, Compile)
{
    __block bool doneCompiling = false;
    [[WKContentExtensionStore defaultStore] compileContentExtensionForIdentifier:@"TestExtension" encodedContentExtension:basicFilter completionHandler:^(WKContentExtension *filter, NSError *error) {
    
        EXPECT_NOT_NULL(filter);
        EXPECT_NULL(error);

        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);
}

static NSString *invalidFilter = @"[";

static void checkDomain(NSError *error)
{
    EXPECT_STREQ([[error domain] UTF8String], [WKErrorDomain UTF8String]);
}

TEST_F(WKContentExtensionStoreTest, InvalidExtension)
{
    __block bool doneCompiling = false;
    [[WKContentExtensionStore defaultStore] compileContentExtensionForIdentifier:@"TestExtension" encodedContentExtension:invalidFilter completionHandler:^(WKContentExtension *filter, NSError *error) {
    
        EXPECT_NULL(filter);
        EXPECT_NOT_NULL(error);
        checkDomain(error);
        EXPECT_EQ(error.code, WKErrorContentExtensionStoreCompileFailed);
        EXPECT_STREQ("Extension compilation failed: Failed to parse the JSON String.", [[error helpAnchor] UTF8String]);

        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);
}

TEST_F(WKContentExtensionStoreTest, Lookup)
{
    __block bool doneCompiling = false;
    [[WKContentExtensionStore defaultStore] compileContentExtensionForIdentifier:@"TestExtension" encodedContentExtension:basicFilter completionHandler:^(WKContentExtension *filter, NSError *error) {
    
        EXPECT_NOT_NULL(filter);
        EXPECT_NULL(error);

        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);

    __block bool doneLookingUp = false;
    [[WKContentExtensionStore defaultStore] lookupContentExtensionForIdentifier:@"TestExtension" completionHandler:^(WKContentExtension *filter, NSError *error) {
    
        EXPECT_NOT_NULL(filter);
        EXPECT_NULL(error);

        doneLookingUp = true;
    }];
    TestWebKitAPI::Util::run(&doneLookingUp);
}

TEST_F(WKContentExtensionStoreTest, NonExistingIdentifierLookup)
{
    __block bool doneLookingUp = false;
    [[WKContentExtensionStore defaultStore] lookupContentExtensionForIdentifier:@"DoesNotExist" completionHandler:^(WKContentExtension *filter, NSError *error) {
    
        EXPECT_NULL(filter);
        EXPECT_NOT_NULL(error);
        checkDomain(error);
        EXPECT_EQ(error.code, WKErrorContentExtensionStoreLookupFailed);
        EXPECT_STREQ("Extension lookup failed: Unspecified error during lookup.", [[error helpAnchor] UTF8String]);
        
        doneLookingUp = true;
    }];
    TestWebKitAPI::Util::run(&doneLookingUp);
}

TEST_F(WKContentExtensionStoreTest, VersionMismatch)
{
    __block bool doneCompiling = false;
    [[WKContentExtensionStore defaultStore] compileContentExtensionForIdentifier:@"TestExtension" encodedContentExtension:basicFilter completionHandler:^(WKContentExtension *filter, NSError *error)
    {
        
        EXPECT_NOT_NULL(filter);
        EXPECT_NULL(error);
        
        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);

    [[WKContentExtensionStore defaultStore] _invalidateContentExtensionVersionForIdentifier:@"TestExtension"];
    
    __block bool doneLookingUp = false;
    [[WKContentExtensionStore defaultStore] lookupContentExtensionForIdentifier:@"TestExtension" completionHandler:^(WKContentExtension *filter, NSError *error)
    {
        
        EXPECT_NULL(filter);
        EXPECT_NOT_NULL(error);
        checkDomain(error);
        EXPECT_EQ(error.code, WKErrorContentExtensionStoreVersionMismatch);
        EXPECT_STREQ("Extension lookup failed: Version of file does not match version of interpreter.", [[error helpAnchor] UTF8String]);
        
        doneLookingUp = true;
    }];
    TestWebKitAPI::Util::run(&doneLookingUp);
}

TEST_F(WKContentExtensionStoreTest, Removal)
{
    __block bool doneCompiling = false;
    [[WKContentExtensionStore defaultStore] compileContentExtensionForIdentifier:@"TestExtension" encodedContentExtension:basicFilter completionHandler:^(WKContentExtension *filter, NSError *error) {
    
        EXPECT_NOT_NULL(filter);
        EXPECT_NULL(error);

        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);

    __block bool doneRemoving = false;
    [[WKContentExtensionStore defaultStore] removeContentExtensionForIdentifier:@"TestExtension" completionHandler:^(NSError *error) {
        EXPECT_NULL(error);

        doneRemoving = true;
    }];
    TestWebKitAPI::Util::run(&doneRemoving);
}

TEST_F(WKContentExtensionStoreTest, NonExistingIdentifierRemove)
{
    __block bool doneRemoving = false;
    [[WKContentExtensionStore defaultStore] removeContentExtensionForIdentifier:@"DoesNotExist" completionHandler:^(NSError *error) {
        EXPECT_NOT_NULL(error);
        checkDomain(error);
        EXPECT_EQ(error.code, WKErrorContentExtensionStoreRemoveFailed);
        EXPECT_STREQ("Extension removal failed: Unspecified error during remove.", [[error helpAnchor] UTF8String]);

        doneRemoving = true;
    }];
    TestWebKitAPI::Util::run(&doneRemoving);
}

#endif

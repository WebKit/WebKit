/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(RetainPtr, AdoptNS)
{
    RetainPtr<NSObject> object1 = adoptNS([[NSObject alloc] init]);
    EXPECT_EQ(1, CFGetRetainCount(object1.get()));

    RetainPtr<NSObject *> object2 = adoptNS([[NSObject alloc] init]);
    EXPECT_EQ(1, CFGetRetainCount(object2.get()));

    RetainPtr<id> object3 = adoptNS([[NSObject alloc] init]);
    EXPECT_EQ(1, CFGetRetainCount(object3.get()));
}

TEST(RetainPtr, ConstructionFromMutableNSType)
{
    NSMutableString *string = [NSMutableString stringWithUTF8String:"foo"];

    // This should invoke RetainPtr's move constructor.
    // FIXME: This doesn't actually test that we moved the value. We should use a mock
    // NSObject that logs -retain and -release calls.
    RetainPtr<NSString> ptr = RetainPtr<NSMutableString>(string);

    EXPECT_EQ(string, ptr);

    RetainPtr<NSMutableString> temp = string;

    // This should invoke RetainPtr's move constructor.
    RetainPtr<NSString> ptr2(WTFMove(temp));

    EXPECT_EQ(string, ptr2);
    EXPECT_EQ((NSString *)nil, temp);
}

TEST(RetainPtr, ConstructionFromSameNSType)
{
    NSString *string = @"foo";

    // This should invoke RetainPtr's move constructor.
    // FIXME: This doesn't actually test that we moved the value. We should use a mock
    // NSObject that logs -retain and -release calls.
    RetainPtr<NSString> ptr = RetainPtr<NSString>(string);

    EXPECT_EQ(string, ptr);

    RetainPtr<NSString> temp = string;

    // This should invoke RetainPtr's move constructor.
    RetainPtr<NSString> ptr2(WTFMove(temp));

    EXPECT_EQ(string, ptr2);
    EXPECT_EQ((NSString *)nil, temp);
}

TEST(RetainPtr, ConstructionFromSimilarNSType)
{
    NSString *string = @"foo";

    // This should invoke RetainPtr's move constructor.
    // FIXME: This doesn't actually test that we moved the value. We should use a mock
    // NSObject that logs -retain and -release calls.
    RetainPtr<NSString> ptr = RetainPtr<NSString *>(string);

    EXPECT_EQ(string, ptr);

    RetainPtr<NSString *> temp = string;

    // This should invoke RetainPtr's move constructor.
    RetainPtr<NSString> ptr2(WTFMove(temp));

    EXPECT_EQ(string, ptr2);
    EXPECT_EQ((NSString *)nil, temp);
}

TEST(RetainPtr, ConstructionFromSimilarNSTypeReversed)
{
    NSString *string = @"foo";

    // This should invoke RetainPtr's move constructor.
    // FIXME: This doesn't actually test that we moved the value. We should use a mock
    // NSObject that logs -retain and -release calls.
    RetainPtr<NSString *> ptr = RetainPtr<NSString>(string);

    EXPECT_EQ(string, ptr);

    RetainPtr<NSString> temp = string;

    // This should invoke RetainPtr's move constructor.
    RetainPtr<NSString *> ptr2(WTFMove(temp));

    EXPECT_EQ(string, ptr2);
    EXPECT_EQ((NSString *)nil, temp);
}

TEST(RetainPtr, MoveAssignmentFromMutableNSType)
{
    NSMutableString *string = [NSMutableString stringWithUTF8String:"foo"];
    RetainPtr<NSString> ptr;

    // This should invoke RetainPtr's move assignment operator.
    // FIXME: This doesn't actually test that we moved the value. We should use a mock
    // NSObject that logs -retain and -release calls.
    ptr = RetainPtr<NSMutableString>(string);

    EXPECT_EQ(string, ptr);

    ptr = nil;
    RetainPtr<NSMutableString> temp = string;

    // This should invoke RetainPtr's move assignment operator.
    ptr = WTFMove(temp);

    EXPECT_EQ(string, ptr);
    EXPECT_EQ((NSString *)nil, temp);
}

TEST(RetainPtr, MoveAssignmentFromSameNSType)
{
    NSString *string = @"foo";
    RetainPtr<NSString> ptr;

    // This should invoke RetainPtr's move assignment operator.
    // FIXME: This doesn't actually test that we moved the value. We should use a mock
    // NSObject that logs -retain and -release calls.
    ptr = RetainPtr<NSString>(string);

    EXPECT_EQ(string, ptr);

    ptr = nil;
    RetainPtr<NSString> temp = string;

    // This should invoke RetainPtr's move assignment operator.
    ptr = WTFMove(temp);

    EXPECT_EQ(string, ptr);
    EXPECT_EQ((NSString *)nil, temp);
}

TEST(RetainPtr, MoveAssignmentFromSimilarNSType)
{
    NSString *string = @"foo";
    RetainPtr<NSString> ptr;

    // This should invoke RetainPtr's move assignment operator.
    // FIXME: This doesn't actually test that we moved the value. We should use a mock
    // NSObject that logs -retain and -release calls.
    ptr = RetainPtr<NSString *>(string);

    EXPECT_EQ(string, ptr);

    ptr = nil;
    RetainPtr<NSString *> temp = string;

    // This should invoke RetainPtr's move assignment operator.
    ptr = WTFMove(temp);

    EXPECT_EQ(string, ptr);
    EXPECT_EQ((NSString *)nil, temp);
}

TEST(RetainPtr, MoveAssignmentFromSimilarNSTypeReversed)
{
    NSString *string = @"foo";
    RetainPtr<NSString *> ptr;

    // This should invoke RetainPtr's move assignment operator.
    // FIXME: This doesn't actually test that we moved the value. We should use a mock
    // NSObject that logs -retain and -release calls.
    ptr = RetainPtr<NSString>(string);

    EXPECT_EQ(string, ptr);

    ptr = nil;
    RetainPtr<NSString> temp = string;

    // This should invoke RetainPtr's move assignment operator.
    ptr = WTFMove(temp);

    EXPECT_EQ(string, ptr);
    EXPECT_EQ((NSString *)nil, temp);
}

TEST(RetainPtr, OptionalRetainPtrNS)
{
    // Test assignment from adoptNS().
    Optional<RetainPtr<NSObject>> optionalObject1 = adoptNS([NSObject new]);
    EXPECT_EQ(1, CFGetRetainCount(optionalObject1.value().get()));
    RetainPtr<NSObject> object1 = optionalObject1.value();
    EXPECT_EQ(optionalObject1.value(), object1);

    // Test assignment from retainPtr().
    Optional<RetainPtr<NSObject>> optionalObject2;
    @autoreleasepool {
        optionalObject2 = retainPtr([[NSObject new] autorelease]);
    }
    EXPECT_EQ(1, CFGetRetainCount(optionalObject2.value().get()));
    RetainPtr<NSObject> object2 = optionalObject2.value();
    EXPECT_EQ(optionalObject2.value(), object2);

    EXPECT_NE(object1, object2);

    // Test assignment from Optional<RetainPtr<NSObject>>.
    optionalObject1 = optionalObject2;
    EXPECT_TRUE(optionalObject1.value());
    EXPECT_TRUE(optionalObject1.value().get());
    EXPECT_EQ(optionalObject1.value(), object2);
    EXPECT_TRUE(optionalObject2.value());
    EXPECT_TRUE(optionalObject2.value().get());
    EXPECT_EQ(optionalObject2.value(), object2);

    // Reset after assignment test.
    optionalObject1 = object1;
    EXPECT_EQ(optionalObject1.value(), object1);
    EXPECT_EQ(optionalObject2.value(), object2);

    // Test move from Optional<RetainPtr<NSObject>>.
    optionalObject1 = WTFMove(optionalObject2);
    EXPECT_TRUE(optionalObject1.value());
    EXPECT_TRUE(optionalObject1.value().get());
    EXPECT_EQ(optionalObject1.value(), object2);
    EXPECT_FALSE(optionalObject2);
}

TEST(RetainPtr, RetainPtrNS)
{
    RetainPtr<NSObject> object1;
    @autoreleasepool {
        object1 = retainPtr([[NSObject new] autorelease]);
    }
    EXPECT_EQ(1, CFGetRetainCount(object1.get()));

    RetainPtr<NSObject *> object2;
    @autoreleasepool {
        object2 = retainPtr([[NSObject new] autorelease]);
    }
    EXPECT_EQ(1, CFGetRetainCount(object2.get()));

    RetainPtr<id> object3;
    @autoreleasepool {
        object3 = retainPtr([[NSObject new] autorelease]);
    }
    EXPECT_EQ(1, CFGetRetainCount(object3.get()));
}

} // namespace TestWebKitAPI

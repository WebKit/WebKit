/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "Helpers/Test.h"
#import "Helpers/cocoa/TestCocoa.h"
#import <WebKit/CocoaObjectForKeyHelpers.h>

namespace TestWebKitAPI {

TEST(CocoaHelpers, ObjectForKeyStringReturnsNilOnlyWhenAskedIfEmpty)
{
    NSDictionary *dictionary = @{ @"empty": @"", @"value": @"text" };

    EXPECT_NULL(WebKit::objectForKey<NSString>(dictionary, @"empty", true, Nil));
    EXPECT_NS_EQUAL(@"", WebKit::objectForKey<NSString>(dictionary, @"empty", false, Nil));
    EXPECT_NS_EQUAL(@"text", WebKit::objectForKey<NSString>(dictionary, @"value", true, Nil));
}

TEST(CocoaHelpers, ObjectForKeyArrayReturnsNilOnlyWhenAskedIfEmpty)
{
    NSDictionary *dictionary = @{ @"empty": @[ ] };

    EXPECT_NULL(WebKit::objectForKey<NSArray>(dictionary, @"empty", true, Nil));
    EXPECT_NS_EQUAL(@[ ], WebKit::objectForKey<NSArray>(dictionary, @"empty", false, Nil));
}

TEST(CocoaHelpers, ObjectForKeyArrayKeepsOnlyElementsOfRequiredClass)
{
    NSDictionary *dictionary = @{ @"mixed": @[ @1, @"two", @3 ], @"strings": @[ @"one" ] };

    EXPECT_NS_EQUAL((@[ @1, @3 ]), WebKit::objectForKey<NSArray>(dictionary, @"mixed", true, NSNumber.class));
    EXPECT_NS_EQUAL(@[ ], WebKit::objectForKey<NSArray>(dictionary, @"strings", false, NSNumber.class));
}

TEST(CocoaHelpers, ObjectForKeyDictionaryReturnsNilOnlyWhenAskedIfEmpty)
{
    NSDictionary *dictionary = @{ @"empty": @{ } };

    EXPECT_NULL(WebKit::objectForKey<NSDictionary>(dictionary, @"empty", true, Nil));
    EXPECT_NS_EQUAL(@{ }, WebKit::objectForKey<NSDictionary>(dictionary, @"empty", false, Nil));
}

TEST(CocoaHelpers, ObjectForKeyDictionaryKeepsOnlyValuesOfRequiredClass)
{
    NSDictionary *dictionary = @{ @"mixed": @{ @"a": @1, @"b": @"two" } };

    EXPECT_NS_EQUAL(@{ @"a": @1 }, WebKit::objectForKey<NSDictionary>(dictionary, @"mixed", true, NSNumber.class));
}

TEST(CocoaHelpers, ObjectForKeySetReturnsNilOnlyWhenAskedIfEmpty)
{
    NSDictionary *dictionary = @{ @"empty": [NSSet set] };

    EXPECT_NULL(WebKit::objectForKey<NSSet>(dictionary, @"empty", true, Nil));
    EXPECT_NS_EQUAL([NSSet set], WebKit::objectForKey<NSSet>(dictionary, @"empty", false, Nil));
}

TEST(CocoaHelpers, ObjectForKeySetKeepsOnlyElementsOfRequiredClass)
{
    NSDictionary *dictionary = @{ @"mixed": [NSSet setWithObjects:@1, @"two", @3, nil] };

    EXPECT_NS_EQUAL(([NSSet setWithObjects:@1, @3, nil]), WebKit::objectForKey<NSSet>(dictionary, @"mixed", true, NSNumber.class));
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)

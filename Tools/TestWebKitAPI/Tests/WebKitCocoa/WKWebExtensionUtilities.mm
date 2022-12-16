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
#import <WebKit/_WKWebExtensionUtilities.h>

namespace TestWebKitAPI {

TEST(WKWebExtensionUtilities, TestRequiredKeys)
{
    NSDictionary *dictionary = @{
        @"A": @"A",
        @"B": @"B",
    };

    NSString *error = nil;
    BOOL result = [_WKWebExtensionUtilities validateContentsOfDictionary:dictionary
        requiredKeys:@[ @"A", @"B" ]
        optionalKeys:@[ ]
        keyToExpectedValueType:@{ @"A": [NSObject class], @"B": [NSObject class] }
        outExceptionString:&error];
    EXPECT_TRUE(result);
    EXPECT_NULL(error);

    result = [_WKWebExtensionUtilities validateContentsOfDictionary:dictionary
        requiredKeys:@[ @"A", @"B", @"C" ]
        optionalKeys:@[ ]
        keyToExpectedValueType:@{ @"A": [NSObject class], @"B": [NSObject class], @"C": [NSObject class] }
        outExceptionString:&error];
    EXPECT_FALSE(result);
    EXPECT_NS_EQUAL(error, @"Missing required keys: C.");

    result = [_WKWebExtensionUtilities validateContentsOfDictionary:@{ }
        requiredKeys:@[ @"A", @"B", @"C" ]
        optionalKeys:@[ ]
        keyToExpectedValueType:@{ @"A": [NSObject class], @"B": [NSObject class], @"C": [NSObject class] }
        outExceptionString:&error];
    EXPECT_FALSE(result);
    EXPECT_NS_EQUAL(error, @"Missing required keys: A, B, C.");
}

TEST(WKWebExtensionUtilities, TestOptionalKeys)
{
    NSDictionary *dictionary = @{
        @"A": @"A",
    };

    NSString *error = nil;
    BOOL result = [_WKWebExtensionUtilities validateContentsOfDictionary:dictionary
        requiredKeys:@[ ]
        optionalKeys:@[ @"A" ]
        keyToExpectedValueType:@{ @"A": [NSObject class] }
        outExceptionString:&error];
    EXPECT_TRUE(result);
    EXPECT_NULL(error);

    result = [_WKWebExtensionUtilities validateContentsOfDictionary:dictionary
        requiredKeys:@[ ]
        optionalKeys:@[ @"A", @"B" ]
        keyToExpectedValueType:@{ @"A": [NSObject class], @"B": [NSObject class] }
        outExceptionString:&error];
    EXPECT_TRUE(result);
    EXPECT_NULL(error);
}

TEST(WKWebExtensionUtilities, TestUnexpectedKeys)
{
    NSDictionary *dictionary = @{
        @"A": @"A",
        @"B": @YES,
    };

    NSString *error = nil;
    BOOL result = [_WKWebExtensionUtilities validateContentsOfDictionary:dictionary
        requiredKeys:@[ ]
        optionalKeys:@[ ]
        keyToExpectedValueType:@{ }
        outExceptionString:&error];
    EXPECT_TRUE(result);
    EXPECT_NULL(error);

    error = nil;
    result = [_WKWebExtensionUtilities validateContentsOfDictionary:dictionary
        requiredKeys:@[ ]
        optionalKeys:@[ @"B" ]
        keyToExpectedValueType:@{ @"B": [@YES class] }
        outExceptionString:&error];
    EXPECT_TRUE(result);
    EXPECT_NULL(error);

    error = nil;
    result = [_WKWebExtensionUtilities validateContentsOfDictionary:dictionary
        requiredKeys:@[ @"B" ]
        optionalKeys:@[ ]
        keyToExpectedValueType:@{ @"B": [@YES class] }
        outExceptionString:&error];
    EXPECT_TRUE(result);
    EXPECT_NULL(error);
}

TEST(WKWebExtensionUtilities, TestExpectedType)
{
    NSDictionary *dictionary = @{
        @"A": @"A",
        @"B": @"B",
    };

    NSString *error = nil;
    BOOL result = [_WKWebExtensionUtilities validateContentsOfDictionary:dictionary
        requiredKeys:@[ ]
        optionalKeys:@[ @"A" ]
        keyToExpectedValueType:@{ @"A": [NSString class] }
        outExceptionString:&error];
    EXPECT_TRUE(result);
    EXPECT_NULL(error);

    error = nil;
    result = [_WKWebExtensionUtilities validateContentsOfDictionary:dictionary
        requiredKeys:@[ ]
        optionalKeys:@[ @"A" ]
        keyToExpectedValueType:@{ @"A": [NSNumber class] }
        outExceptionString:&error];
    EXPECT_FALSE(result);
    EXPECT_NS_EQUAL(error, @"Expected a number value for 'A', found a string value instead.");

    error = nil;
    result = [_WKWebExtensionUtilities validateContentsOfDictionary:dictionary
        requiredKeys:@[ ]
        optionalKeys:@[ @"B" ]
        keyToExpectedValueType:@{ @"B": [@YES class] }
        outExceptionString:&error];
    EXPECT_FALSE(result);
    EXPECT_NS_EQUAL(error, @"Expected a boolean value for 'B', found a string value instead.");
}

TEST(WKWebExtensionUtilities, TestExpectedTypeArrayOfTypes)
{
    NSDictionary *dictionary = @{
        @"A": @[ @"AString" ],
    };

    NSString *error = nil;
    BOOL result = [_WKWebExtensionUtilities validateContentsOfDictionary:dictionary
        requiredKeys:@[ ]
        optionalKeys:@[ @"A" ]
        keyToExpectedValueType:@{ @"A": @[ [NSString class] ] }
        outExceptionString:&error];
    EXPECT_TRUE(result);
    EXPECT_NULL(error);

    error = nil;
    result = [_WKWebExtensionUtilities validateContentsOfDictionary:dictionary
        requiredKeys:@[ ]
        optionalKeys:@[ @"A" ]
        keyToExpectedValueType:@{ @"A": @[ [NSNumber class] ] }
        outExceptionString:&error];
    EXPECT_FALSE(result);
    EXPECT_NS_EQUAL(error, @"Expected number values in the array for 'A', found a string value instead.");

    dictionary = @{
        @"A": @[ ],
    };

    error = nil;
    result = [_WKWebExtensionUtilities validateContentsOfDictionary:dictionary
        requiredKeys:@[ ]
        optionalKeys:@[ @"A" ]
        keyToExpectedValueType:@{ @"A": @[ [NSString class] ] }
        outExceptionString:&error];
    EXPECT_TRUE(result);
    EXPECT_NULL(error);

    error = nil;
    dictionary = @{
        @"A": @"Definitely not an array",
    };

    error = nil;
    result = [_WKWebExtensionUtilities validateContentsOfDictionary:dictionary
        requiredKeys:@[ ]
        optionalKeys:@[ @"A" ]
        keyToExpectedValueType:@{ @"A": @[ [NSString class] ] }
        outExceptionString:&error];
    EXPECT_FALSE(result);
    EXPECT_NS_EQUAL(error, @"Expected an array for 'A'.");
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)

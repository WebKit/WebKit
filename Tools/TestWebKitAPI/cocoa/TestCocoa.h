/*
 * Copyright (C) 2019-2025 Apple Inc. All rights reserved.
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

#pragma once

#import "PlatformUtilities.h"
#import "Test.h"
#import <wtf/text/TextStream.h>

namespace TestWebKitAPI {
namespace Util {

template<typename T, typename U>
static inline ::testing::AssertionResult assertNSObjectsAreEqual(const char* expectedExpression, const char* actualExpression, T *expected, U *actual)
{
    if ((!expected && !actual) || [expected isEqual:actual])
        return ::testing::AssertionSuccess();
    return ::testing::internal::EqFailure(expectedExpression, actualExpression, toSTD([expected description]), toSTD([actual description]), false /* ignoring_case */);
}

#if PLATFORM(IOS_FAMILY)
void instantiateUIApplicationIfNeeded(Class customApplicationClass = nil);
#endif

} // namespace Util
} // namespace TestWebKitAPI

#define EXPECT_NS_EQUAL(expected, actual) \
    EXPECT_PRED_FORMAT2(TestWebKitAPI::Util::assertNSObjectsAreEqual, expected, actual)

#if PLATFORM(IOS_FAMILY)
std::ostream& operator<<(std::ostream&, const UIEdgeInsets&);
bool operator==(const UIEdgeInsets&, const UIEdgeInsets&);
#endif

#if USE(CG)

std::ostream& operator<<(std::ostream&, const CGPoint&);
bool operator==(const CGPoint&, const CGPoint&);
std::ostream& operator<<(std::ostream&, const CGSize&);
bool operator==(const CGSize&, const CGSize&);
std::ostream& operator<<(std::ostream&, const CGRect&);
bool operator==(const CGRect&, const CGRect&);

constexpr CGFloat redColorComponents[4] = { 1, 0, 0, 1 };
constexpr CGFloat blueColorComponents[4] = { 0, 0, 1, 1 };

#endif

/**
 A testing hook to be able to easily write a full test in Swift and run it using the existing gtest infrastructure.

 To write a Swift test:

 1. Use this macro to define a new test, such as:

 ```
 SWIFT_TEST(AppKitGestures, ClickingChangesSelection);
 ```

 2. Create an Objective-C interface named "{Suite}Support" and declare class methods for each test of the form:

 ```
 + (void)test{Name}WithCompletionHandler:(NS_SWIFT_UI_ACTOR void(^)(NSError * _Nullable))completionHandler;
 ```

 3. Add an `@objc @implementation` Swift implementation for the interface type, and implement each method.

 4. Use the convenience testing functions in the `Testing` type to write expectations and throw errors.

 */
#define SWIFT_TEST(Suite, Name) \
TEST(Suite, Name) \
{ \
    __block bool done = false; \
    [Suite##Support test##Name##WithCompletionHandler:^(NSError *error) { \
        if (error) { \
            TextStream errorMessage; \
            errorMessage << error; \
            EXPECT_NULL(error) << errorMessage.release().utf8().data(); \
        } \
        done = true; \
    }]; \
    TestWebKitAPI::Util::run(&done); \
}

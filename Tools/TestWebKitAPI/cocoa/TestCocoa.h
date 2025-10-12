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

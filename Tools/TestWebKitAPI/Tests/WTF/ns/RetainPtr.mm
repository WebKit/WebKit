/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
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
    RetainPtr<NSObject> foo = adoptNS([[NSObject alloc] init]);

    EXPECT_EQ(1, CFGetRetainCount(foo.get()));
}

TEST(RetainPtr, MoveAssignmentFromSameType)
{
    NSString *string = @"foo";
    RetainPtr<NSString> ptr;

    // This should invoke RetainPtr's move assignment operator.
    ptr = RetainPtr<NSString>(string);

    EXPECT_EQ(string, ptr);

    ptr = 0;
    RetainPtr<NSString> temp = string;

    // This should invoke RetainPtr's move assignment operator.
    ptr = std::move(temp);

    EXPECT_EQ(string, ptr);
    EXPECT_EQ((NSString *)0, temp);
}

TEST(RetainPtr, MoveAssignmentFromSimilarType)
{
    NSMutableString *string = [NSMutableString stringWithUTF8String:"foo"];
    RetainPtr<NSString> ptr;

    // This should invoke RetainPtr's move assignment operator.
    ptr = RetainPtr<NSMutableString>(string);

    EXPECT_EQ(string, ptr);

    ptr = 0;
    RetainPtr<NSMutableString> temp = string;

    // This should invoke RetainPtr's move assignment operator.
    ptr = std::move(temp);

    EXPECT_EQ(string, ptr);
    EXPECT_EQ((NSString *)0, temp);
}

TEST(RetainPtr, ConstructionFromSameType)
{
    NSString *string = @"foo";

    // This should invoke RetainPtr's move constructor.
    RetainPtr<NSString> ptr = std::move(RetainPtr<NSString>(string));

    EXPECT_EQ(string, ptr);

    RetainPtr<NSString> temp = string;

    // This should invoke RetainPtr's move constructor.
    RetainPtr<NSString> ptr2(std::move(temp));

    EXPECT_EQ(string, ptr2);
    EXPECT_EQ((NSString *)0, temp);
}

TEST(RetainPtr, ConstructionFromSimilarType)
{
    NSMutableString *string = [NSMutableString stringWithUTF8String:"foo"];

    // This should invoke RetainPtr's move constructor.
    RetainPtr<NSString> ptr = RetainPtr<NSMutableString>(string);

    EXPECT_EQ(string, ptr);

    RetainPtr<NSMutableString> temp = string;

    // This should invoke RetainPtr's move constructor.
    RetainPtr<NSString> ptr2(std::move(temp));

    EXPECT_EQ(string, ptr2);
    EXPECT_EQ((NSString *)0, temp);
}

} // namespace TestWebKitAPI

/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import <wtf/text/TextStreamCocoa.h>

#import <Foundation/Foundation.h>

TEST(WTF_TextStream, NSString)
{
    TextStream ts;
    ts << @"Test";
    EXPECT_EQ(ts.release(), "Test"_s);
}

TEST(WTF_TextStream, Class)
{
    TextStream ts;
    ts << [NSString class];
    EXPECT_EQ(ts.release(), "NSString"_s);
}

TEST(WTF_TextStream, NSArray)
{
    {
        TextStream ts;
        ts << @[ @"Test1", @"Test2" ];
        EXPECT_EQ(ts.release(), "[Test1, Test2]"_s);
    }
    {
        TextStream ts;
        ts << @[ @"Test1", @[ @"Test2", @"Test3" ] ];
        EXPECT_EQ(ts.release(), "[Test1, [Test2, Test3]]"_s);
    }
}

TEST(WTF_TextStream, NSDictionary)
{
    {
        TextStream ts;
        ts << @{ @"Test1" : @(3), @"Test2" : @"Hello" };
        EXPECT_EQ(ts.release(), "{Test1: 3, Test2: Hello}"_s);
    }
    {
        TextStream ts;
        ts << @{ @"Test1" : @(3), @"Test2" : @[ @"Hello", @" ", @"there" ] };
        EXPECT_EQ(ts.release(), "{Test1: 3, Test2: [Hello,  , there]}"_s);
    }
}

TEST(WTF_TextStream, NSNumber)
{
    TextStream ts;
    ts << @(3);
    EXPECT_EQ(ts.release(), "3"_s);
}

TEST(WTF_TextStream, id)
{
    TextStream ts;
    id value = @(3);
    ts << value;
    EXPECT_EQ(ts.release(), "3"_s);
}

TEST(WTF_TextStream, CoreGraphics)
{
    {
        TextStream ts;
        ts << CGRectMake(1, 2, 3, 4);
        EXPECT_EQ(ts.release(), "{{1.00, 2.00}, {3.00, 4.00}}"_s);
    }
    {
        TextStream ts;
        ts << CGPointMake(1, 2);
        EXPECT_EQ(ts.release(), "{1.00, 2.00}"_s);
    }
    {
        TextStream ts;
        ts << CGSizeMake(3, 4);
        EXPECT_EQ(ts.release(), "{3.00, 4.00}"_s);
    }
}

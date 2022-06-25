/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "PlatformUtilities.h"

#import <WebKit/WebStringTruncator.h>

namespace TestWebKitAPI {

TEST(WebKitLegacy, StringTruncator)
{
    @autoreleasepool {
        EXPECT_EQ(nil, [WebStringTruncator centerTruncateString:@"abcdefghijklmnopqrstuvwxyz" toWidth:100 withFont:nil]);
        EXPECT_STREQ([[WebStringTruncator centerTruncateString:@"abcdefghijklmnopqrstuvwxyz" toWidth:100 withFont:[NSFont fontWithName:@"Helvetica" size:12]] UTF8String], "abcdefg…tuvwxyz");
        EXPECT_STREQ([[WebStringTruncator centerTruncateString:@"abcdefghijklmnopqrstuvwxyz" toWidth:100] UTF8String], "abcdef…uvwxyz");
        EXPECT_EQ(nil, [WebStringTruncator rightTruncateString:@"abcdefghijklmnopqrstuvwxyz" toWidth:100 withFont:nil]);
        EXPECT_STREQ([[WebStringTruncator rightTruncateString:@"abcdefghijklmnopqrstuvwxyz" toWidth:100 withFont:[NSFont fontWithName:@"Helvetica" size:12]] UTF8String], "abcdefghijklmno…");
        EXPECT_STREQ([[WebStringTruncator centerTruncateString:@"ābcdefghijklmnopqrstuvwxyz" toWidth:100 withFont:[NSFont fontWithName:@"Helvetica" size:12]] UTF8String], "ābcdefg…tuvwxyz");
        EXPECT_STREQ([[WebStringTruncator rightTruncateString:@"ābcdefghijklmnopqrstuvwxyz" toWidth:100 withFont:[NSFont fontWithName:@"Helvetica" size:12]] UTF8String], "ābcdefghijklmno…");
        EXPECT_EQ([WebStringTruncator widthOfString:@"abcdefghijklmnopqrstuvwxyz" font:nil], 0);
        EXPECT_EQ([WebStringTruncator widthOfString:@"abcdefghijklmnopqrstuvwxyz" font:[NSFont fontWithName:@"Helvetica" size:12]], 152.736328125);
        EXPECT_EQ([WebStringTruncator widthOfString:@"ābcdefghijklmnopqrstuvwxyz" font:[NSFont fontWithName:@"Helvetica" size:12]], 152.736328125);
    }
}

} // namespace TestWebKitAPI

/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import <wtf/text/cocoa/ContextualizedNSString.h>

#import <wtf/cocoa/TypeCastsCocoa.h>

TEST(WTF_ContextualizedNSString, Basic)
{
    auto context = "abc"_str;
    auto contents = "def"_str;
    auto contextualizedString = adoptNS([[WTFContextualizedNSString alloc] initWithContext:context contents:contents]);
    EXPECT_TRUE([contextualizedString isEqualToString:@"abcdef"]);
    EXPECT_TRUE([@"abcdef" isEqualToString:contextualizedString.get()]);
    EXPECT_EQ(contextualizedString.get().length, 6UL);
    EXPECT_EQ([contextualizedString characterAtIndex:0], 'a');
    EXPECT_EQ([contextualizedString characterAtIndex:1], 'b');
    EXPECT_EQ([contextualizedString characterAtIndex:2], 'c');
    EXPECT_EQ([contextualizedString characterAtIndex:3], 'd');
    EXPECT_EQ([contextualizedString characterAtIndex:4], 'e');
    EXPECT_EQ([contextualizedString characterAtIndex:5], 'f');
}

TEST(WTF_ContextualizedNSString, BasicNoContext)
{
    auto contents = "abc"_str;
    auto contextualizedString = adoptNS([[WTFContextualizedNSString alloc] initWithContext: { } contents:contents]);
    EXPECT_TRUE([contextualizedString isEqualToString:@"abc"]);
    EXPECT_TRUE([@"abc" isEqualToString:contextualizedString.get()]);
    EXPECT_EQ(contextualizedString.get().length, 3UL);
    EXPECT_EQ([contextualizedString characterAtIndex:0], 'a');
    EXPECT_EQ([contextualizedString characterAtIndex:1], 'b');
    EXPECT_EQ([contextualizedString characterAtIndex:2], 'c');
}

TEST(WTF_ContextualizedNSString, getCharacters)
{
    auto context = "abc"_str;
    auto contents = "def"_str;
    auto contextualizedString = adoptNS([[WTFContextualizedNSString alloc] initWithContext:context contents:contents]);

    unichar buffer[7];
    std::ranges::fill(buffer, '\0');

    [contextualizedString getCharacters:buffer range:NSMakeRange(0, 0)];
    EXPECT_EQ(buffer[0], '\0');
    std::ranges::fill(buffer, '\0');

    [contextualizedString getCharacters:buffer range:NSMakeRange(0, 1)];
    EXPECT_EQ(buffer[0], 'a');
    EXPECT_EQ(buffer[1], '\0');
    std::ranges::fill(buffer, '\0');

    [contextualizedString getCharacters:buffer range:NSMakeRange(0, 2)];
    EXPECT_EQ(buffer[0], 'a');
    EXPECT_EQ(buffer[1], 'b');
    EXPECT_EQ(buffer[2], '\0');
    std::ranges::fill(buffer, '\0');

    [contextualizedString getCharacters:buffer range:NSMakeRange(0, 3)];
    EXPECT_EQ(buffer[0], 'a');
    EXPECT_EQ(buffer[1], 'b');
    EXPECT_EQ(buffer[2], 'c');
    EXPECT_EQ(buffer[3], '\0');
    std::ranges::fill(buffer, '\0');

    [contextualizedString getCharacters:buffer range:NSMakeRange(0, 4)];
    EXPECT_EQ(buffer[0], 'a');
    EXPECT_EQ(buffer[1], 'b');
    EXPECT_EQ(buffer[2], 'c');
    EXPECT_EQ(buffer[3], 'd');
    EXPECT_EQ(buffer[4], '\0');
    std::ranges::fill(buffer, '\0');

    [contextualizedString getCharacters:buffer range:NSMakeRange(0, 5)];
    EXPECT_EQ(buffer[0], 'a');
    EXPECT_EQ(buffer[1], 'b');
    EXPECT_EQ(buffer[2], 'c');
    EXPECT_EQ(buffer[3], 'd');
    EXPECT_EQ(buffer[4], 'e');
    EXPECT_EQ(buffer[5], '\0');
    std::ranges::fill(buffer, '\0');

    [contextualizedString getCharacters:buffer range:NSMakeRange(0, 6)];
    EXPECT_EQ(buffer[0], 'a');
    EXPECT_EQ(buffer[1], 'b');
    EXPECT_EQ(buffer[2], 'c');
    EXPECT_EQ(buffer[3], 'd');
    EXPECT_EQ(buffer[4], 'e');
    EXPECT_EQ(buffer[5], 'f');
    EXPECT_EQ(buffer[6], '\0');
    std::ranges::fill(buffer, '\0');

    [contextualizedString getCharacters:buffer range:NSMakeRange(1, 0)];
    EXPECT_EQ(buffer[0], '\0');
    std::ranges::fill(buffer, '\0');

    [contextualizedString getCharacters:buffer range:NSMakeRange(1, 1)];
    EXPECT_EQ(buffer[0], 'b');
    EXPECT_EQ(buffer[1], '\0');
    std::ranges::fill(buffer, '\0');

    [contextualizedString getCharacters:buffer range:NSMakeRange(1, 2)];
    EXPECT_EQ(buffer[0], 'b');
    EXPECT_EQ(buffer[1], 'c');
    EXPECT_EQ(buffer[2], '\0');
    std::ranges::fill(buffer, '\0');

    [contextualizedString getCharacters:buffer range:NSMakeRange(1, 3)];
    EXPECT_EQ(buffer[0], 'b');
    EXPECT_EQ(buffer[1], 'c');
    EXPECT_EQ(buffer[2], 'd');
    EXPECT_EQ(buffer[3], '\0');
    std::ranges::fill(buffer, '\0');

    [contextualizedString getCharacters:buffer range:NSMakeRange(1, 4)];
    EXPECT_EQ(buffer[0], 'b');
    EXPECT_EQ(buffer[1], 'c');
    EXPECT_EQ(buffer[2], 'd');
    EXPECT_EQ(buffer[3], 'e');
    EXPECT_EQ(buffer[4], '\0');
    std::ranges::fill(buffer, '\0');

    [contextualizedString getCharacters:buffer range:NSMakeRange(1, 5)];
    EXPECT_EQ(buffer[0], 'b');
    EXPECT_EQ(buffer[1], 'c');
    EXPECT_EQ(buffer[2], 'd');
    EXPECT_EQ(buffer[3], 'e');
    EXPECT_EQ(buffer[4], 'f');
    EXPECT_EQ(buffer[5], '\0');
    std::ranges::fill(buffer, '\0');

    [contextualizedString getCharacters:buffer range:NSMakeRange(3, 0)];
    EXPECT_EQ(buffer[0], '\0');
    std::ranges::fill(buffer, '\0');

    [contextualizedString getCharacters:buffer range:NSMakeRange(3, 1)];
    EXPECT_EQ(buffer[0], 'd');
    EXPECT_EQ(buffer[1], '\0');
    std::ranges::fill(buffer, '\0');

    [contextualizedString getCharacters:buffer range:NSMakeRange(3, 2)];
    EXPECT_EQ(buffer[0], 'd');
    EXPECT_EQ(buffer[1], 'e');
    EXPECT_EQ(buffer[2], '\0');
    std::ranges::fill(buffer, '\0');

    [contextualizedString getCharacters:buffer range:NSMakeRange(3, 3)];
    EXPECT_EQ(buffer[0], 'd');
    EXPECT_EQ(buffer[1], 'e');
    EXPECT_EQ(buffer[2], 'f');
    EXPECT_EQ(buffer[3], '\0');
    std::ranges::fill(buffer, '\0');

    [contextualizedString getCharacters:buffer range:NSMakeRange(4, 0)];
    EXPECT_EQ(buffer[0], '\0');
    std::ranges::fill(buffer, '\0');

    [contextualizedString getCharacters:buffer range:NSMakeRange(4, 1)];
    EXPECT_EQ(buffer[0], 'e');
    EXPECT_EQ(buffer[1], '\0');
    std::ranges::fill(buffer, '\0');

    [contextualizedString getCharacters:buffer range:NSMakeRange(4, 2)];
    EXPECT_EQ(buffer[0], 'e');
    EXPECT_EQ(buffer[1], 'f');
    EXPECT_EQ(buffer[2], '\0');
    std::ranges::fill(buffer, '\0');

    [contextualizedString getCharacters:buffer range:NSMakeRange(6, 0)];
    EXPECT_EQ(buffer[0], '\0');
    std::ranges::fill(buffer, '\0');

    [contextualizedString getCharacters:buffer range:NSMakeRange(7, 0)];
    EXPECT_EQ(buffer[0], '\0');
    std::ranges::fill(buffer, '\0');
}

TEST(WTF_ContextualizedNSString, nonPrimitive)
{
    auto context = "abc"_str;
    auto contents = "def"_str;
    auto contextualizedString = adoptNS([[WTFContextualizedNSString alloc] initWithContext:context contents:contents]);

    EXPECT_TRUE([contextualizedString hasPrefix:@"ab"]);
    EXPECT_TRUE([contextualizedString hasPrefix:@"abcd"]);
    EXPECT_FALSE([contextualizedString hasPrefix:@"b"]);
    EXPECT_FALSE([contextualizedString hasPrefix:@"bcd"]);
    EXPECT_TRUE([contextualizedString hasSuffix:@"ef"]);
    EXPECT_TRUE([contextualizedString hasSuffix:@"cdef"]);
    EXPECT_FALSE([contextualizedString hasSuffix:@"c"]);
    EXPECT_FALSE([contextualizedString hasSuffix:@"bcd"]);
    EXPECT_TRUE([contextualizedString containsString:@"ab"]);
    EXPECT_TRUE([contextualizedString containsString:@"bcd"]);
    EXPECT_TRUE([contextualizedString containsString:@"e"]);
    EXPECT_FALSE([contextualizedString containsString:@"cb"]);

    auto copy = adoptNS([contextualizedString copy]);
    EXPECT_TRUE([copy isEqualToString:@"abcdef"]);

    auto mutableCopy = adoptNS([contextualizedString mutableCopy]);
    EXPECT_TRUE([mutableCopy isEqualToString:@"abcdef"]);
}

TEST(WTF_ContextualizedNSString, cfString)
{
    auto context = "abc"_str;
    auto contents = "def"_str;
    auto contextualizedString = adoptNS([[WTFContextualizedNSString alloc] initWithContext:context contents:contents]);
    auto contextualizedCFString = bridge_cast(static_cast<NSString *>(contextualizedString.get()));

    EXPECT_EQ(CFStringGetCharacterAtIndex(contextualizedCFString, 0), 'a');
    EXPECT_EQ(CFStringGetCharacterAtIndex(contextualizedCFString, 1), 'b');
    EXPECT_EQ(CFStringGetCharacterAtIndex(contextualizedCFString, 2), 'c');
    EXPECT_EQ(CFStringGetCharacterAtIndex(contextualizedCFString, 3), 'd');
    EXPECT_EQ(CFStringGetCharacterAtIndex(contextualizedCFString, 4), 'e');
    EXPECT_EQ(CFStringGetCharacterAtIndex(contextualizedCFString, 5), 'f');

    EXPECT_NE(CFStringFind(contextualizedCFString, CFSTR("ab"), 0).location, kCFNotFound);
    EXPECT_NE(CFStringFind(contextualizedCFString, CFSTR("bcd"), 0).location, kCFNotFound);
    EXPECT_NE(CFStringFind(contextualizedCFString, CFSTR("de"), 0).location, kCFNotFound);
    EXPECT_EQ(CFStringFind(contextualizedCFString, CFSTR("AB"), 0).location, kCFNotFound);
    EXPECT_NE(CFStringFind(contextualizedCFString, CFSTR("AB"), kCFCompareCaseInsensitive).location, kCFNotFound);

    EXPECT_EQ(CFStringGetLength(contextualizedCFString), 6);
}

TEST(WTF_ContextualizedNSString, tokenizer)
{
    auto context = "th"_str;
    auto contents = "is is some text"_str;
    auto contextualizedString = adoptNS([[WTFContextualizedNSString alloc] initWithContext:context contents:contents]);
    auto contextualizedCFString = bridge_cast(static_cast<NSString *>(contextualizedString.get()));

    auto locale = adoptCF(CFLocaleCreate(kCFAllocatorDefault, CFSTR("en_US")));
    auto tokenizer = adoptCF(CFStringTokenizerCreate(kCFAllocatorDefault, contextualizedCFString, CFRangeMake(0, CFStringGetLength(contextualizedCFString)), kCFStringTokenizerUnitWord, locale.get()));

    CFIndex indices[] = { 4, 7, 12, 17 };
    unsigned index = 0;
    for (CFIndex i = 0; i < CFStringGetLength(contextualizedCFString); ++index) {
        CFStringTokenizerGoToTokenAtIndex(tokenizer.get(), i);
        auto range = CFStringTokenizerGetCurrentTokenRange(tokenizer.get());
        ASSERT_NE(range.location, kCFNotFound);
        auto next = range.location + range.length;
        ASSERT_LT(index, std::size(indices));
        EXPECT_EQ(next, indices[index]);
        i = next + 1;
    }
    EXPECT_EQ(index, std::size(indices));
}

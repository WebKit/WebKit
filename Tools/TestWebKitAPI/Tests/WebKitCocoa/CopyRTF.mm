
/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS_FAMILY)
#import <MobileCoreServices/MobileCoreServices.h>
#endif

#if USE(APPKIT)
using PlatformColor = NSColor;
#else
using PlatformColor = UIColor;
#endif

#if USE(APPKIT)
static NSData *readRTFDataFromPasteboard()
{
    return [[NSPasteboard generalPasteboard] dataForType:NSPasteboardTypeRTF];
}
#else
static NSData *readRTFDataFromPasteboard()
{
    id value = [[UIPasteboard generalPasteboard] valueForPasteboardType:(__bridge NSString *)kUTTypeRTF];
    ASSERT([value isKindOfClass:[NSData class]]);
    return value;
}
#endif

static RetainPtr<NSAttributedString> copyAttributedStringFromHTML(NSString *htmlString, bool forceDarkMode)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);

    if (forceDarkMode)
        [webView forceDarkMode];

    [webView synchronouslyLoadHTMLString:htmlString];

    [webView selectAll:nil];
    [webView _synchronouslyExecuteEditCommand:@"Copy" argument:nil];

    NSData *rtfData = readRTFDataFromPasteboard();
    EXPECT_NOT_NULL(rtfData);

    auto attributedString = adoptNS([[NSAttributedString alloc] initWithData:rtfData options:@{ } documentAttributes:nil error:nullptr]);
    EXPECT_NOT_NULL(attributedString.get());

    return attributedString;
}

static void checkColor(PlatformColor *color, CGFloat red, CGFloat green, CGFloat blue, CGFloat alpha)
{
    EXPECT_NOT_NULL(color);

    CGFloat observedRed = 0;
    CGFloat observedGreen = 0;
    CGFloat observedBlue = 0;
    CGFloat observedAlpha = 0;
    [color getRed:&observedRed green:&observedGreen blue:&observedBlue alpha:&observedAlpha];

    EXPECT_EQ(red, observedRed);
    EXPECT_EQ(green, observedRed);
    EXPECT_EQ(blue, observedBlue);
    EXPECT_EQ(alpha, observedAlpha);
}

#if HAVE(OS_DARK_MODE_SUPPORT)
TEST(CopyRTF, StripsDefaultTextColorOfDarkContent)
{
    auto attributedString = copyAttributedStringFromHTML(@"<style>:root { color-scheme: dark }</style> Default <span style=\"color: black\">Black</span> <span style=\"color: white\">White</span>", true);

    __block size_t i = 0;
    [attributedString enumerateAttribute:NSForegroundColorAttributeName inRange:NSMakeRange(0, [attributedString length]) options:0 usingBlock:^(id value, NSRange attributeRange, BOOL *stop) {
        if (!i)
            EXPECT_NULL(value);
        else if (i == 1) {
            EXPECT_NOT_NULL(value);
            checkColor(value, 0, 0, 0, 1);
        } else if (i == 2)
            EXPECT_NULL(value);
        else
            ASSERT_NOT_REACHED();

        ++i;
    }];

    EXPECT_EQ(i, 3UL);
}
#endif

TEST(CopyRTF, StripsDefaultTextColorOfLightContent)
{
    auto attributedString = copyAttributedStringFromHTML(@"Default <span style=\"color: white\">White</span> <span style=\"color: black\">Black</span>", false);

    __block size_t i = 0;
    [attributedString enumerateAttribute:NSForegroundColorAttributeName inRange:NSMakeRange(0, [attributedString length]) options:0 usingBlock:^(id value, NSRange attributeRange, BOOL *stop) {
        if (!i)
            EXPECT_NULL(value);
        else if (i == 1) {
            EXPECT_NOT_NULL(value);
            checkColor(value, 1, 1, 1, 1);
        } else if (i == 2)
            EXPECT_NULL(value);
        else
            ASSERT_NOT_REACHED();

        ++i;
    }];

    EXPECT_EQ(i, 3UL);
}

#if ENABLE(DATA_DETECTION)

TEST(CopyRTF, StripsDataDetectorsLinks)
{
    auto attributedString = copyAttributedStringFromHTML(@"<a href=\"https://www.apple.com\">Apple</a> <a href=\"x-apple-data-detectors://0\" dir=\"ltr\" x-apple-data-detectors=\"true\" x-apple-data-detectors-type=\"calendar-event\" x-apple-data-detectors-result=\"0\" style=\"color: currentcolor; text-decoration-color: rgba(128, 128, 128, 0.38);\">on Friday 11/6 at 4pm</a>", false);

    __block size_t i = 0;
    [attributedString enumerateAttribute:NSLinkAttributeName inRange:NSMakeRange(0, [attributedString length]) options:0 usingBlock:^(id value, NSRange attributeRange, BOOL *stop) {
        if (!i)
            EXPECT_NOT_NULL(value);
        else if (i == 1)
            EXPECT_NULL(value);
        else
            FAIL();

        ++i;
    }];

    EXPECT_EQ(i, 2UL);
}

#endif // ENABLE(DATA_DETECTION)

TEST(CopyRTF, StripsUserSelectNone)
{
    auto attributedString = copyAttributedStringFromHTML(@"hello <span style='-webkit-user-select: none; user-select: none;'>world "
        "<span style='-webkit-user-select: initial; user-select: initial;'>WebKit </span></span>"
        "<div style='-webkit-user-select: none; user-select: none;'>some<br>user-select-none<br>content</div><span inert>foo </span>bar", false);

    EXPECT_WK_STREQ([attributedString string].UTF8String, "hello WebKit bar");
}

#endif // PLATFORM(COCOA)

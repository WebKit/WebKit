
/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#import "CocoaColor.h"
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#import <MobileCoreServices/MobileCoreServices.h>
#endif

@interface WKWebView ()
- (void)copy:(id)sender;
- (void)paste:(id)sender;
@end

#if PLATFORM(MAC)

@interface WKWebView () <NSServicesMenuRequestor>
@end

NSData *readHTMLDataFromPasteboard()
{
    return [[NSPasteboard generalPasteboard] dataForType:NSHTMLPboardType];
}

NSString *readHTMLStringFromPasteboard()
{
    return [[NSPasteboard generalPasteboard] stringForType:NSHTMLPboardType];
}

#else

NSData *readHTMLDataFromPasteboard()
{
    return [[UIPasteboard generalPasteboard] dataForPasteboardType:(__bridge NSString *)kUTTypeHTML];
}

NSString *readHTMLStringFromPasteboard()
{
    id value = [[UIPasteboard generalPasteboard] valueForPasteboardType:(__bridge NSString *)kUTTypeHTML];
    if ([value isKindOfClass:[NSData class]])
        value = [[[NSString alloc] initWithData:(NSData *)value encoding:NSUTF8StringEncoding] autorelease];
    ASSERT([value isKindOfClass:[NSString class]]);
    return (NSString *)value;
}

#endif

static RetainPtr<TestWKWebView> createWebViewWithCustomPasteboardDataEnabled()
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    auto preferences = (__bridge WKPreferencesRef)[[webView configuration] preferences];
    WKPreferencesSetDataTransferItemsEnabled(preferences, true);
    WKPreferencesSetCustomPasteboardDataEnabled(preferences, true);
    return webView;
}

TEST(CopyHTML, Sanitizes)
{
    auto webView = createWebViewWithCustomPasteboardDataEnabled();
    [webView synchronouslyLoadTestPageNamed:@"copy-html"];
    [webView stringByEvaluatingJavaScript:@"HTMLToCopy = '<meta content=\"secret\"><b onmouseover=\"dangerousCode()\">hello</b>"
        "<!-- secret-->, world<script>dangerousCode()</script>';"];
    [webView copy:nil];
    [webView paste:nil];
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"didCopy"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"didPaste"].boolValue);
    EXPECT_WK_STREQ("<meta content=\"secret\"><b onmouseover=\"dangerousCode()\">hello</b><!-- secret-->, world<script>dangerousCode()</script>",
        [webView stringByEvaluatingJavaScript:@"pastedHTML"]);
    String htmlInNativePasteboard = readHTMLStringFromPasteboard();
    EXPECT_TRUE(htmlInNativePasteboard.contains("hello"));
    EXPECT_TRUE(htmlInNativePasteboard.contains(", world"));
    EXPECT_FALSE(htmlInNativePasteboard.contains("secret"));
    EXPECT_FALSE(htmlInNativePasteboard.contains("dangerousCode"));
}

TEST(CopyHTML, SanitizationPreservesCharacterSetInSelectedText)
{
    auto webView = createWebViewWithCustomPasteboardDataEnabled();
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<body>"
        "<meta charset='utf-8'>"
        "<span id='copy'>我叫謝文昇</span>"
        "<script>getSelection().selectAllChildren(copy);</script>"
        "</body>"];
    [webView copy:nil];
    [webView waitForNextPresentationUpdate];

    NSString *copiedMarkup = readHTMLStringFromPasteboard();
    EXPECT_TRUE([copiedMarkup containsString:@"<span "]);
    EXPECT_TRUE([copiedMarkup containsString:@"我叫謝文昇"]);
    EXPECT_TRUE([copiedMarkup containsString:@"</span>"]);

    auto attributedString = adoptNS([[NSAttributedString alloc] initWithData:readHTMLDataFromPasteboard() options:@{ NSDocumentTypeDocumentOption: NSHTMLTextDocumentType } documentAttributes:nil error:nil]);
    EXPECT_WK_STREQ("我叫謝文昇", [attributedString string]);
}

TEST(CopyHTML, SanitizationPreservesCharacterSet)
{
    Vector<std::pair<RetainPtr<NSString>, RetainPtr<NSData>>, 3> markupStringsAndData;
    auto webView = createWebViewWithCustomPasteboardDataEnabled();
    for (NSString *encodingName in @[ @"utf-8", @"windows-1252", @"bogus-encoding" ]) {
        [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<!DOCTYPE html>"
            "<body>"
            "<meta charset='%@'>"
            "<p id='copy'>Copy me</p>"
            "<script>"
            "copy.addEventListener('copy', e => {"
            "    e.clipboardData.setData('text/html', `<span style='color: red;'>我叫謝文昇</span>`);"
            "    e.preventDefault();"
            "});"
            "getSelection().selectAllChildren(copy);"
            "</script>"
            "</body>", encodingName]];
        [webView copy:nil];
        [webView waitForNextPresentationUpdate];

        markupStringsAndData.append({ readHTMLStringFromPasteboard(), readHTMLDataFromPasteboard() });
    }

    for (auto& [copiedMarkup, copiedData] : markupStringsAndData) {
        EXPECT_TRUE([copiedMarkup containsString:@"<span "]);
        EXPECT_TRUE([copiedMarkup containsString:@"color: red;"]);
        EXPECT_TRUE([copiedMarkup containsString:@"我叫謝文昇"]);
        EXPECT_TRUE([copiedMarkup containsString:@"</span>"]);

        NSError *attributedStringConversionError = nil;

        auto attributedString = adoptNS([[NSAttributedString alloc] initWithData:copiedData.get() options:@{ NSDocumentTypeDocumentOption: NSHTMLTextDocumentType } documentAttributes:nil error:&attributedStringConversionError]);
        EXPECT_WK_STREQ("我叫謝文昇", [attributedString string]);

        __block BOOL foundColorAttribute = NO;
        [attributedString enumerateAttribute:NSForegroundColorAttributeName inRange:NSMakeRange(0, 5) options:0 usingBlock:^(CocoaColor *color, NSRange range, BOOL*) {
            CGFloat redComponent = 0;
            CGFloat greenComponent = 0;
            CGFloat blueComponent = 0;
            [color getRed:&redComponent green:&greenComponent blue:&blueComponent alpha:nil];

            EXPECT_EQ(1., redComponent);
            EXPECT_EQ(0., greenComponent);
            EXPECT_EQ(0., blueComponent);
            foundColorAttribute = YES;
        }];
        EXPECT_TRUE(foundColorAttribute);
    }
}

#if PLATFORM(MAC)

TEST(CopyHTML, ItemTypesWhenCopyingWebContent)
{
    auto webView = createWebViewWithCustomPasteboardDataEnabled();
    [webView synchronouslyLoadHTMLString:@"<strong style='color: rgb(255, 0, 0);'>This is some text to copy.</strong>"];
    [webView stringByEvaluatingJavaScript:@"getSelection().selectAllChildren(document.body)"];
    [webView copy:nil];
    [webView waitForNextPresentationUpdate];

    NSArray<NSPasteboardItem *> *items = NSPasteboard.generalPasteboard.pasteboardItems;
    EXPECT_EQ(1U, items.count);

    NSArray<NSPasteboardType> *types = items.firstObject.types;
    EXPECT_TRUE([types containsObject:(__bridge NSString *)kUTTypeWebArchive]);
    EXPECT_TRUE([types containsObject:(__bridge NSString *)NSPasteboardTypeRTF]);
    EXPECT_TRUE([types containsObject:(__bridge NSString *)NSPasteboardTypeString]);
    EXPECT_TRUE([types containsObject:(__bridge NSString *)NSPasteboardTypeHTML]);
}

TEST(CopyHTML, WriteRichTextSelectionToPasteboard)
{
    auto webView = createWebViewWithCustomPasteboardDataEnabled();
    [webView synchronouslyLoadHTMLString:@"<strong style='color: rgb(255, 0, 0);'>This is some text to copy.</strong>"];
    [webView stringByEvaluatingJavaScript:@"getSelection().selectAllChildren(document.body)"];

    auto pasteboard = [NSPasteboard pasteboardWithUniqueName];
    [webView writeSelectionToPasteboard:pasteboard types:@[ (__bridge NSString *)kUTTypeWebArchive ]];

    EXPECT_GT([pasteboard dataForType:(__bridge NSString *)kUTTypeWebArchive].length, 0U);
}

#endif // PLATFORM(MAC)

#endif // PLATFORM(COCOA)

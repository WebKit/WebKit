
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

#import "PasteboardUtilities.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <WebCore/ColorCocoa.h>
#import <WebKit/NSAttributedStringPrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <pal/spi/cocoa/NSAttributedStringSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
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
    RetainPtr<id> value = [[UIPasteboard generalPasteboard] valueForPasteboardType:(__bridge NSString *)kUTTypeHTML];
    if ([value isKindOfClass:[NSData class]])
        value = adoptNS([[NSString alloc] initWithData:(NSData *)value encoding:NSUTF8StringEncoding]);
    ASSERT([value isKindOfClass:[NSString class]]);
    return (NSString *)value.autorelease();
}

#endif

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
    EXPECT_TRUE(htmlInNativePasteboard.contains("hello"_s));
    EXPECT_TRUE(htmlInNativePasteboard.contains(", world"_s));
    EXPECT_FALSE(htmlInNativePasteboard.contains("secret"_s));
    EXPECT_FALSE(htmlInNativePasteboard.contains("dangerousCode"_s));
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
        [attributedString enumerateAttribute:NSForegroundColorAttributeName inRange:NSMakeRange(0, 5) options:0 usingBlock:^(WebCore::CocoaColor *color, NSRange range, BOOL*) {
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

TEST(CopyHTML, SanitizationPreservesRelativeURLInAttributedString)
{
    auto webView = createWebViewWithCustomPasteboardDataEnabled();
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html>"
        "<head><base href='https://webkit.org/' /></head>"
        "<body><a href='/downloads'>Click</a> for downloads</body>"
        "</html>"];
    [webView stringByEvaluatingJavaScript:@"getSelection().selectAllChildren(document.body)"];
    [webView copy:nil];
    [webView waitForNextPresentationUpdate];

#if PLATFORM(IOS_FAMILY)
    RetainPtr archiveData = [UIPasteboard.generalPasteboard dataForPasteboardType:UTTypeWebArchive.identifier];
#else
    RetainPtr archiveData = [NSPasteboard.generalPasteboard dataForType:UTTypeWebArchive.identifier];
#endif

    __block bool done = false;
    __block RetainPtr<NSAttributedString> resultString;
    __block RetainPtr<NSError> resultError;
    [NSAttributedString _loadFromHTMLWithOptions:@{ } contentLoader:^(WKWebView *loadingWebView) {
        return [loadingWebView loadData:archiveData.get() MIMEType:UTTypeWebArchive.preferredMIMEType characterEncodingName:@"" baseURL:[NSURL URLWithString:@"about:blank"]];
    } completionHandler:^(NSAttributedString *string, NSDictionary *, NSError *error) {
        resultString = string;
        resultError = error;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto links = adoptNS([NSMutableArray<NSURL *> new]);
    [resultString enumerateAttribute:NSLinkAttributeName inRange:NSMakeRange(0, 5) options:0 usingBlock:^(NSURL *url, NSRange, BOOL*) {
        [links addObject:url];
    }];

    EXPECT_NULL(resultError);
    EXPECT_WK_STREQ("Click for downloads", [resultString string]);
    EXPECT_EQ(1U, [links count]);
    EXPECT_WK_STREQ("https://webkit.org/downloads", [links firstObject].absoluteString);
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

TEST(CopyHTML, CopySelectedTextInTextDocument)
{
    RetainPtr webView = createWebViewWithCustomPasteboardDataEnabled();
    RetainPtr fileURL = [NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"json"];

    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:fileURL.get()]];
    [webView stringByEvaluatingJavaScript:@"getSelection().selectAllChildren(document.body)"];
    RetainPtr expectedString = [webView stringByEvaluatingJavaScript:@"getSelection().toString()"];
    [webView copy:nil];
    [webView waitForNextPresentationUpdate];

#if PLATFORM(MAC)
    RetainPtr pastedObjects = [[NSPasteboard generalPasteboard] readObjectsForClasses:@[ NSAttributedString.class ] options:nil];
    RetainPtr copiedText = dynamic_objc_cast<NSAttributedString>([pastedObjects firstObject]);
#else
    RetainPtr itemProvider = [[[UIPasteboard generalPasteboard] itemProviders] firstObject];
    __block bool doneLoading = false;
    __block RetainPtr<NSAttributedString> copiedText;
    [itemProvider loadObjectOfClass:NSAttributedString.class completionHandler:^(NSAttributedString *result, NSError *) {
        copiedText = result;
        doneLoading = true;
    }];
    TestWebKitAPI::Util::run(&doneLoading);
#endif
    EXPECT_WK_STREQ(expectedString.get(), [copiedText string]);
}

#endif // PLATFORM(COCOA)

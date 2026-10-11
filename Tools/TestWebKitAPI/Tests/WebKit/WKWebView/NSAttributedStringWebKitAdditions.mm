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

#import "Helpers/cocoa/TestCocoa.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebCore/FontCocoa.h>
#import <WebKit/NSAttributedString.h>
#import <wtf/RetainPtr.h>
#import <wtf/Scope.h>

TEST(NSAttributedStringWebKitAdditions, DefaultFontSize)
{
    __block bool done = false;
    [NSAttributedString loadFromHTMLWithString:@"Hello World" options:@{ } completionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *attributes, NSError *error) {
        EXPECT_EQ([[attributedString attribute:NSFontAttributeName atIndex:0 effectiveRange:nil] pointSize], 12.0);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(NSAttributedStringWebKitAdditions, MultipleParagraphs)
{
    __block bool done = false;
    [NSAttributedString loadFromHTMLWithString:@"<p style='margin-bottom:1px'>Hello</p><p style='margin-bottom:2px'>World</p>" options:@{ } completionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *attributes, NSError *error) {
        EXPECT_EQ([[attributedString attribute:NSParagraphStyleAttributeName atIndex:0 effectiveRange:nil] paragraphSpacing], 1.0);
        EXPECT_EQ([[attributedString attribute:NSParagraphStyleAttributeName atIndex:6 effectiveRange:nil] paragraphSpacing], 2.0);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(NSAttributedStringWebKitAdditions, BlockQuotes)
{
    __block bool done = false;
    [NSAttributedString loadFromHTMLWithString:@"<p>Hello</p><blockquote>Down<blockquote>under</blockquote></blockquote>" options:@{ } completionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *attributes, NSError *error) {
        EXPECT_EQ([[attributedString attribute:NSParagraphStyleAttributeName atIndex:0 effectiveRange:nil] headerLevel], 0);
        EXPECT_EQ([[attributedString attribute:NSParagraphStyleAttributeName atIndex:6 effectiveRange:nil] headerLevel], 0);
        NSPresentationIntent* quote1 = [attributedString attribute:NSPresentationIntentAttributeName atIndex:6 effectiveRange:nil];
        ASSERT_NOT_NULL(quote1);
        EXPECT_EQ([quote1 indentationLevel], 0);
        EXPECT_EQ([[attributedString attribute:NSParagraphStyleAttributeName atIndex:11 effectiveRange:nil] headerLevel], 0);
        NSPresentationIntent* quote2 = [attributedString attribute:NSPresentationIntentAttributeName atIndex:11 effectiveRange:nil];
        ASSERT_NOT_NULL(quote2);
        EXPECT_EQ([quote2 indentationLevel], 1);
        EXPECT_NE([quote1 identity], [quote2 identity]);
        EXPECT_EQ([quote1 identity], [[quote2 parentIntent] identity]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

// Like the global preference tests in PreferenceChanges.mm, this relies on global defaults reaching the
// web content process, which is only covered on macOS. The converter itself is tested in PresentationIntents.mm.
#if PLATFORM(MAC)

static RetainPtr<NSAttributedString> attributedStringFromHTML(NSString *html)
{
    __block RetainPtr<NSAttributedString> result;
    __block bool done = false;
    [NSAttributedString loadFromHTMLWithString:html options:@{ } completionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *attributes, NSError *error) {
        result = attributedString;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result;
}

static NSPresentationIntent *presentationIntentForSubstring(NSAttributedString *attributedString, NSString *substring)
{
    NSRange range = [attributedString.string rangeOfString:substring];
    if (range.location == NSNotFound)
        return nil;
    return [attributedString attribute:NSPresentationIntentAttributeName atIndex:range.location effectiveRange:nil];
}

static NSInlinePresentationIntent inlinePresentationIntentForSubstring(NSAttributedString *attributedString, NSString *substring)
{
    NSRange range = [attributedString.string rangeOfString:substring];
    if (range.location == NSNotFound)
        return 0;
    return [[attributedString attribute:NSInlinePresentationIntentAttributeName atIndex:range.location effectiveRange:nil] unsignedIntegerValue];
}

static const CFStringRef presentationIntentHTMLInterchangeKey = CFSTR("__NSPresentationIntentHTMLInterchange");

// Sets the global default that makes WebKit read every presentation intent kind, or removes it if the value is null.
// The web content process sees changes to global defaults asynchronously, so this waits until the change takes effect.
static bool setPresentationIntentHTMLInterchangeValue(CFPropertyListRef value)
{
    CFPreferencesSetValue(presentationIntentHTMLInterchangeKey, value, kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    CFPreferencesSynchronize(kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);

    id object = (__bridge id)value;
    bool enabled = [object respondsToSelector:@selector(boolValue)] && [object boolValue];
    auto readsParagraphIntents = [] {
        return !!presentationIntentForSubstring(attributedStringFromHTML(@"<p>probe</p>").get(), @"probe");
    };
    for (unsigned i = 0; i < 50 && readsParagraphIntents() != enabled; ++i)
        TestWebKitAPI::Util::runFor(100_ms);
    return readsParagraphIntents() == enabled;
}

TEST(NSAttributedStringWebKitAdditions, PresentationIntentHTMLInterchangeDefault)
{
    RetainPtr previousValue = adoptCF(CFPreferencesCopyValue(presentationIntentHTMLInterchangeKey, kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost));
    auto restoreDefault = makeScopeExit([previousValue] {
        setPresentationIntentHTMLInterchangeValue(previousValue.get());
    });
    ASSERT_TRUE(setPresentationIntentHTMLInterchangeValue(kCFBooleanTrue));

    RetainPtr attributedString = attributedStringFromHTML(@"<h1>Title</h1><ul><li>item <em>soft</em></li></ul>");

    NSPresentationIntent *title = presentationIntentForSubstring(attributedString.get(), @"Title");
    ASSERT_NOT_NULL(title);
    EXPECT_EQ(title.intentKind, NSPresentationIntentKindHeader);
    EXPECT_EQ(title.headerLevel, 1);

    NSPresentationIntent *item = presentationIntentForSubstring(attributedString.get(), @"item");
    ASSERT_NOT_NULL(item);
    EXPECT_EQ(item.intentKind, NSPresentationIntentKindListItem);
    EXPECT_EQ(item.parentIntent.intentKind, NSPresentationIntentKindUnorderedList);
    EXPECT_EQ(inlinePresentationIntentForSubstring(attributedString.get(), @"soft"), NSInlinePresentationIntentEmphasized);
}

#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)
TEST(NSAttributedStringWebKitAdditions, DirectoriesNotCreated)
{
    NSString *directory = [NSHomeDirectory() stringByAppendingString:@"/Library/Cookies"];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeItemAtPath:directory error:nil];

    __block bool done = false;
    [NSAttributedString loadFromHTMLWithData:[NSData data] options:@{ } completionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *attributes, NSError *error) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto cookieDirectoryExists = [&] {
        return [[NSFileManager defaultManager] fileExistsAtPath:directory];
    };
    EXPECT_FALSE(cookieDirectoryExists());

    RetainPtr webView = adoptNS([TestWKWebView new]);
    [webView synchronouslyLoadHTMLString:@"hi"];
    EXPECT_TRUE(cookieDirectoryExists());
}
#endif

TEST(NSAttributedStringWebKitAdditions, FontDataURL)
{
    NSURL *fontURL = [NSBundle.test_resourcesBundle URLForResource:@"Ahem" withExtension:@"ttf"];
    NSData *fontData = [NSData dataWithContentsOfURL:fontURL];
    NSString *fontBase64 = [fontData base64EncodedStringWithOptions:0];

    NSString *html = [NSString stringWithFormat:@""
        "<html>"
        "<head>"
        "<style>"
        "@font-face { font-family: exampleFont; src: url(data:font/opentype;base64,%@); }"
        "div { font-family: exampleFont; }"
        "</style>"
        "</head>"
        "<body><div>hello!</div></body>"
        "</html>", fontBase64];

    __block bool done = false;
    [NSAttributedString loadFromHTMLWithString:html options:@{ } completionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *attributes, NSError *error) {
        __block bool foundFont { false };
        [attributedString enumerateAttributesInRange:NSMakeRange(0, attributedString.length) options:0 usingBlock:^(NSDictionary *attributes, NSRange attributeRange, BOOL *stop) {
            if (attributes[NSFontAttributeName])
                foundFont = true;
        }];
        EXPECT_TRUE(foundFont);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(NSAttributedStringWebKitAdditions, InstalledFont)
{
    NSString *html = [NSString stringWithFormat:@""
        "<html>"
        "<head>"
        "<style>"
        "div { font-family: Helvetica; }"
        "</style>"
        "</head>"
        "<body><div>hello!</div></body>"
        "</html>"];

    __block bool done = false;
    [NSAttributedString loadFromHTMLWithString:html options:@{ } completionHandler:^(NSAttributedString *attributedString, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *attributes, NSError *error) {
        __block bool foundFont { false };
        [attributedString enumerateAttributesInRange:NSMakeRange(0, attributedString.length) options:0 usingBlock:^(NSDictionary *attributes, NSRange attributeRange, BOOL *stop) {
            if (attributes[NSFontAttributeName]) {
                foundFont = true;
                EXPECT_WK_STREQ(((WebCore::CocoaFont *)attributes[NSFontAttributeName]).fontName, "Helvetica"_s);
            }
        }];
        EXPECT_TRUE(foundFont);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

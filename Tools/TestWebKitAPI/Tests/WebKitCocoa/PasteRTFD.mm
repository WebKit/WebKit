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

#include "config.h"

#if WK_API_ENABLED && PLATFORM(COCOA)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#include <MobileCoreServices/MobileCoreServices.h>
#endif

@interface WKWebView ()
- (void)paste:(id)sender;
@end

#if PLATFORM(MAC)
void writeRTFToPasteboard(NSData *data)
{
    [[NSPasteboard generalPasteboard] declareTypes:@[NSPasteboardTypeRTF] owner:nil];
    [[NSPasteboard generalPasteboard] setData:data forType:NSPasteboardTypeRTF];
}

void writeRTFDToPasteboard(NSData *data)
{
    [[NSPasteboard generalPasteboard] declareTypes:@[NSRTFDPboardType] owner:nil];
    [[NSPasteboard generalPasteboard] setData:data forType:NSRTFDPboardType];
}
#else

@interface NSAttributedString ()
- (id)initWithRTF:(NSData *)data documentAttributes:(NSDictionary **)dict;
- (id)initWithRTFD:(NSData *)data documentAttributes:(NSDictionary **)dict;
- (NSData *)RTFFromRange:(NSRange)range documentAttributes:(NSDictionary *)dict;
- (NSData *)RTFDFromRange:(NSRange)range documentAttributes:(NSDictionary *)dict;
- (BOOL)containsAttachments;
@end

void writeRTFToPasteboard(NSData *data)
{
    [[UIPasteboard generalPasteboard] setItems:@[@{ (__bridge NSString *)kUTTypeRTF : data}]];
}

void writeRTFDToPasteboard(NSData *data)
{
    [[UIPasteboard generalPasteboard] setItems:@[@{ (__bridge NSString *)kUTTypeFlatRTFD : data}]];
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

static RetainPtr<NSAttributedString> createHelloWorldString()
{
    auto hello = adoptNS([[NSAttributedString alloc] initWithString:@"hello" attributes:@{ NSUnderlineStyleAttributeName : @(NSUnderlineStyleSingle) }]);
    auto world = adoptNS([[NSAttributedString alloc] initWithString:@", world" attributes:@{ }]);
    auto string = adoptNS([[NSMutableAttributedString alloc] init]);
    [string appendAttributedString:hello.get()];
    [string appendAttributedString:world.get()];
    return string;
}

TEST(PasteRTF, ExposesHTMLTypeInDataTransfer)
{
    auto webView = createWebViewWithCustomPasteboardDataEnabled();
    [webView synchronouslyLoadTestPageNamed:@"paste-rtfd"];

    auto string = createHelloWorldString();
    writeRTFToPasteboard([string RTFFromRange:NSMakeRange(0, [string length]) documentAttributes:@{ }]);
    [webView paste:nil];

    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"clipboardData.types.includes('text/html')"]);
    [webView stringByEvaluatingJavaScript:@"editor.innerHTML = clipboardData.values[0]; editor.focus()"];
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"document.queryCommandState('underline')"].boolValue);
    [webView stringByEvaluatingJavaScript:@"getSelection().modify('move', 'forward', 'lineboundary')"];
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"document.queryCommandState('underline')"].boolValue);
    EXPECT_WK_STREQ("hello, world", [webView stringByEvaluatingJavaScript:@"editor.textContent"]);
}

TEST(PasteRTFD, EmptyRTFD)
{
    auto webView = createWebViewWithCustomPasteboardDataEnabled();
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><body><div id='editor' contenteditable></div></body></html>"];

    writeRTFDToPasteboard([NSData data]);
    [webView stringByEvaluatingJavaScript:@"document.getElementById('editor').focus()"];
    [webView paste:nil];
}

TEST(PasteRTFD, ExposesHTMLTypeInDataTransfer)
{
    auto webView = createWebViewWithCustomPasteboardDataEnabled();
    [webView synchronouslyLoadTestPageNamed:@"paste-rtfd"];

    auto string = createHelloWorldString();
    writeRTFDToPasteboard([string RTFDFromRange:NSMakeRange(0, [string length]) documentAttributes:@{ }]);
    [webView paste:nil];

    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"clipboardData.types.includes('text/html')"]);
    [webView stringByEvaluatingJavaScript:@"editor.innerHTML = clipboardData.values[0]; editor.focus()"];
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"document.queryCommandState('underline')"].boolValue);
    [webView stringByEvaluatingJavaScript:@"getSelection().modify('move', 'forward', 'lineboundary')"];
    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"document.queryCommandState('underline')"].boolValue);
    EXPECT_WK_STREQ("hello, world", [webView stringByEvaluatingJavaScript:@"editor.textContent"]);
}

TEST(PasteRTFD, ImageElementUsesBlobURL)
{
    auto webView = createWebViewWithCustomPasteboardDataEnabled();
    [webView synchronouslyLoadTestPageNamed:@"paste-rtfd"];

    auto *pngData = [NSData dataWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"sunset-in-cupertino-200px" ofType:@"png" inDirectory:@"TestWebKitAPI.resources"]];
    auto attachment = adoptNS([[NSTextAttachment alloc] initWithData:pngData ofType:(__bridge NSString *)kUTTypePNG]);
    NSAttributedString *string = [NSAttributedString attributedStringWithAttachment:attachment.get()];
    NSData *RTFDData = [string RTFDFromRange:NSMakeRange(0, [string length]) documentAttributes:@{ }];

    writeRTFDToPasteboard(RTFDData);
    [webView paste:nil];

    [webView waitForMessage:@"loaded"];
    EXPECT_WK_STREQ("200", [webView stringByEvaluatingJavaScript:@"imageElement = document.querySelector('img'); imageElement.width"]);
    EXPECT_WK_STREQ("blob:", [webView stringByEvaluatingJavaScript:@"url = new URL(imageElement.src).protocol"]);
}

TEST(PasteRTFD, ImageElementUsesBlobURLInHTML)
{
    auto webView = createWebViewWithCustomPasteboardDataEnabled();
    [webView synchronouslyLoadTestPageNamed:@"paste-rtfd"];

    auto *pngData = [NSData dataWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"sunset-in-cupertino-200px" ofType:@"png" inDirectory:@"TestWebKitAPI.resources"]];
    auto attachment = adoptNS([[NSTextAttachment alloc] initWithData:pngData ofType:(__bridge NSString *)kUTTypePNG]);
    NSAttributedString *string = [NSAttributedString attributedStringWithAttachment:attachment.get()];
    NSData *RTFDData = [string RTFDFromRange:NSMakeRange(0, [string length]) documentAttributes:@{ }];

    writeRTFDToPasteboard(RTFDData);
    [webView paste:nil];

    [webView waitForMessage:@"loaded"];
    EXPECT_WK_STREQ("[\"text/html\"]", [webView stringByEvaluatingJavaScript:@"JSON.stringify(clipboardData.types)"]);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"imageElement = (new DOMParser).parseFromString(clipboardData.values[0], 'text/html').querySelector('img'); !!imageElement"].boolValue);
    EXPECT_WK_STREQ("blob:", [webView stringByEvaluatingJavaScript:@"new URL(imageElement.src).protocol"]);
}

#endif // WK_API_ENABLED && PLATFORM(MAC)



